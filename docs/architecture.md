# 架构与工程分解 (bl_music_player)

> 本文件是 README「工程分解」章节的展开明细；方案变更时两处同步更新。

## 1. 分层模型

| 层 | 内容 | 代码位置 |
|---|---|---|
| L5 表现层 | 画面(封面/歌词/进度) + 逻辑接线 + 状态快照 | `device/app/src/{view,presenter,model}` |
| L4 媒体抽象 | 会话模型 + 双后端合并 + 事件/命令路由 | `device/libs/bl_media` |
| L3 能力库 | 音频输出(mixer/sink) + 蓝牙接入(a2dp/avrcp/spp) | `device/libs/bl_audio`、`device/libs/bl_bt` |
| L2 协议接入 | 设备: BlueZ D-Bus(AVRCP/A2DP) + SPP 服务；主机: forwarder 传输 | `device/libs/bl_bt`、`host/windows/media_forwarder` |
| L1 基础设施 | buildroot(bluez/sbc/dbus/alsa) + kernel/dts + 固件 + 构建部署 | `configure/`、`linux_driver/`、`tools/` |

## 2. 模块清单与接口契约

| 模块 | 职责 | 对外接口 | 状态 |
|---|---|---|---|
| `bl_audio` | mixer 音量/路由、sink PCM 输出、wav 测试 | `bl_audio.h` | ✅ 冻结 v0.2 |
| `bl_media` | 媒体会话抽象: 模型/事件/命令 + 双后端合并 | `bl_media.h` | ★ 待冻结(M1 前) |
| `bl_bt` | A2DP sink 收流解码、AVRCP 控制器、SPP 服务 | 内部接口(不对外) | ★ 新增 |
| app MVP | model(状态快照) / presenter(调库+事件桥) / view(画面) | 使用 bl_media/bl_audio | ★ 填 scaffold |
| forwarder | Windows 侧 SMTC→传输 | `docs/protocol-forwarder.md` | ★ 协议待冻结(M3 前) |

### 依赖方向

```
app/presenter ──只用──► bl_media.h (+ bl_audio.h 的 mixer)
                            ▲
      bt_avrcp(后端) ───────┤  上报 report_* / 实现 backend.command
      bt_spp(后端) ─────────┘
      bt_a2dp(音频) ──► bl_sink (不接 bl_media)
依赖链: bl_bt → bl_media → bl_audio (单向无环)
```

### bl_media 契约(M1 前冻结) — 拆两个头文件

**bl_media.h(UI 面, app 只看这个)**

```c
/* 数据模型: UI 只读快照 */
typedef struct {
	char		title[128];
	char		artist[128];
	char		album[128];
	uint32_t	duration_ms;
	uint32_t	position_ms;
	int		state;		/* 0 stop / 1 play / 2 pause */
	const uint8_t	*cover;		/* JPEG 字节, 可为 NULL */
	uint32_t	cover_len;
	const char	*lyrics_lrc;	/* LRC 文本, 可为 NULL */
} bl_media_info_t;

/* 事件(库线程回调! UI 侧 lv_async_call 转投) */
typedef enum {
	BL_MEDIA_EVT_INFO,	/* 曲目/封面/歌词变化 */
	BL_MEDIA_EVT_STATE,	/* 播放态变化 */
	BL_MEDIA_EVT_POSITION,	/* 位置推进(低频) */
} bl_media_evt_t;
typedef void (*bl_media_cb_t)(bl_media_evt_t, const bl_media_info_t *, void *);

int	bl_media_open(bl_media_cb_t cb, void *user);
void	bl_media_close(void);

/* 命令(UI 线程直接调, 路由到当前活跃后端) */
typedef enum {
	BL_MEDIA_CMD_PLAY, BL_MEDIA_CMD_PAUSE, BL_MEDIA_CMD_NEXT,
	BL_MEDIA_CMD_PREV, BL_MEDIA_CMD_SEEK,	/* arg = 目标毫秒 */
} bl_media_cmd_t;
int	bl_media_command(bl_media_cmd_t cmd, long arg);

/* 快照(深拷贝, 调用者 release) */
int	bl_media_get_info(bl_media_info_t *out);
void	bl_media_info_release(bl_media_info_t *info);
```

**bl_media_backend.h(后端面, 只有 bl_bt 看)**

```c
typedef struct {
	const char	*name;		/* "avrcp" | "forwarder" */
	int		priority;	/* 大者优先: forwarder > avrcp */
	int	(*start)(void);
	void	(*stop)(void);
	int	(*command)(bl_media_cmd_t cmd, long arg);
} bl_media_backend_t;

int	bl_media_register_backend(const bl_media_backend_t *be);

/* 后端 -> 媒体层 上报通道(bl_bt 调用) */
void	bl_media_report_info(const char *src, const bl_media_info_t *info);
void	bl_media_report_state(const char *src, int state);
void	bl_media_report_position(const char *src, uint32_t pos_ms);
void	bl_media_report_gone(const char *src);	/* 断连 -> 自动降级 */
```

### 接口冻结时的 5 条契约细节

| # | 契约 | 决定 |
|---|---|---|
| 1 | cover/lyrics 内存所有权 | 上报时后端分配 -> bl_media 深拷贝 -> 回调返回后前端管; get_info 返深拷贝由调用者 release |
| 2 | 合并优先级 | priority 大者活跃; forwarder > avrcp; report_gone 后自动回落 |
| 3 | 命令路由 | 发给最高优先活跃后端; 无后端返回 -ENODEV |
| 4 | 事件线程 | 回调在库线程 -> UI 必须 lv_async_call 转投 |
| 5 | 位置来源 | forwarder 带时间戳(精确可插值); avrcp 低频快照; 取高优先权威源 |

### bl_bt 接口

**对外 bl_bt.h(仅两个函数)**

```c
int	bl_bt_start(void);	/* 注册 avrcp/spp 后端 + A2DP endpoint + SPP 监听 + 配对 Agent */
void	bl_bt_stop(void);
```

**内部三模块(bl_bt.c 调用)及要完成的 BlueZ D-Bus 调用**

| 文件 | 内部接口 | 要调用的 BlueZ D-Bus 接口 |
|---|---|---|
| `bt_avrcp.c` | `bt_avrcp_init/deinit()`, `bt_avrcp_command(cmd,arg)` | ObjectManager.GetManagedObjects(发现); `MediaPlayer1`{属性 Title/Artist/Album/Status/Position/Track; 方法 Play/Pause/Stop/Next/Previous/Seek}; PropertiesChanged(信号); Agent1+AgentManager1(配对) |
| `bt_a2dp.c` | `bt_a2dp_init/deinit()` | `Media1.RegisterEndpoint`(A2DP SINK UUID 0000110B); MediaEndpoint1.SelectConfiguration/SetConfiguration/ClearConfiguration; `MediaTransport1.Acquire` -> fd; read -> libsbc 解码 -> bl_sink_write |
| `bt_spp.c` | `bt_spp_init/deinit()`, `bt_spp_command(cmd,arg)` | RFCOMM socket(AF_BLUETOOTH/SOCK_STREAM/BTPROTO_RFCOMM, SPP UUID 00001101) bind/listen/accept; 收字节流 -> 协议解析 -> report_* |

### 接口清单总表

| 头文件 | 面向 | 内容 | 冻结时机 |
|---|---|---|---|
| `bl_media.h` | app/UI | 模型、3 类事件、5 个命令、快照查询 | M1 前 |
| `bl_media_backend.h` | bl_bt | backend vtable + register + 4 个 report | M1 前 |
| `bl_bt.h` | app | start/stop | M1 前 |
| `bt_avrcp/a2dp/spp.h` | bl_bt 内部 | init/deinit(/command) | M1-M3 演进 |
| `docs/protocol-forwarder.md` | 主机 C# + bt_spp | 报文字段/分帧/命令 | M3 前 |

## 3. 数据流

```
手机(A2DP) ──SBC流──► bt_a2dp ──解码──► bl_sink ──► WM8960 ──► 喇叭
手机(AVRCP)──元数据──► bt_avrcp ─┐
Windows ─SPP─► bt_spp ──────────┴──► bl_media(合并) ──事件──► presenter ──► view
UI 命令 ──► bl_media ──► 当前后端 ──► 手机/Windows
```

## 4. 工作分解(WBS)

| 包 | 内容 | 里程碑 |
|---|---|---|
| A 基础设施 | buildroot 开 experimental 重编；蓝牙配对/Agent；部署脚本 | M0 |
| B 设备蓝牙 | A2DP endpoint/transport/SBC；AVRCP 客户端；SPP 服务 | M0-M3 |
| C 媒体抽象 | bl_media 模型/事件/命令 + 双后端合并 | M1 |
| D 音频输出 | bl_audio 增强(volume_pct/delay)；A2DP 时钟策略 | M2/M5 |
| E 界面 | MVP model/presenter/view；now-playing + 控制 | M4 |
| F 主机侧 | forwarder 传输(SPP client) + 协议编解码 | M3 |
| G 协议 | forwarder 私有协议设计/冻结 | M3 前 |
| H 验证 | 板端验收清单；主机端无板调试 | 全程 |

## 5. 迁移方案(已完成 v0.5, 2026-10-12)

实际执行(与计划的差异: port 作为整体收编为 `device/app`, 而非放 third_party):

```
lv_port_linux                       -> device/app          (收编为应用; 内部 lvgl/ 移除)
lv_port_linux/lvgl (坏 gitfile)     -> 删除; 内容转 third_party/lvgl submodule
src/platform/linux/audio            -> device/libs/bl_audio
src/platform/windows                -> host/windows
```

关键操作: lvgl 从仓库历史抢救(`git archive HEAD:...`) → 切官方 tag **v9.6.0**
→ 离线收编为 submodule(gitlink = 80ca777e3, 真实上游 commit) → `absorbgitdirs` 归一化。
引用更新: `device/app/CMakeLists.txt`(add_subdirectory → ../../third_party/lvgl)、
`device/app/Kconfig`(rsource → ../../third_party/lvgl/Kconfig)。

验证: `device/app` host 构建 `lvglsim` 通过; `device/libs/bl_audio` 主机 smoke 通过。

## 6. 风险与对策

| 风险 | 对策 |
|---|---|
| BlueZ experimental 编译链(dbus/glib 依赖) | M0 单独验证, 失败则退回自实现 AVRCP(工作量大, 尽量不) |
| 不同手机/Windows 的 AVRCP 字段缺失差异 | bl_media 合并策略: forwarder 优先, AVRCP 兜底 |
| A2DP 与 SPP 共存带宽/时延 | 经典蓝牙可共存; M3 实测延迟, 加大 buffer |
| A2DP 时钟漂移(长时间播放) | M5: `snd_pcm_delay` 查询 + 偶发丢/插帧 |
| 歌词同步精度(forwarder 位置外推 vs 网络延迟) | 位置带时间戳, UI 侧本地插值 |
| SBC 解码实时性(800MHz A7) | 48k stereo SBC 解码 CPU 占用 <5%, 无风险 |
