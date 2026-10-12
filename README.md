# bl_music_player

**多平台蓝牙媒体终端**（i.MX6ULL）：从 Windows / Android / macOS / Linux 获取媒体信息与音频流，LVGL 可视化 + 播放控制。
目标硬件: 正点原子 ALPHA 2.0 (i.MX6ULL + WM8960 codec, 声卡注册为 `wm8960-audio` card 1)。

## 工程分解

> 详细模块/接口/WBS/风险见 `docs/architecture.md`（本文件为总纲与索引）。

### 分层

```
L5 表现层   device/app/src/{view,presenter,model}  画面 + 接线 + 状态快照
L4 媒体抽象 device/libs/bl_media                   会话模型 + 双后端合并 + 事件/命令(待实现)
L3 能力库   device/libs/bl_audio + bl_bt           音频输出(mixer/sink) + 蓝牙(a2dp/avrcp/spp)
L2 协议接入 BlueZ D-Bus(AVRCP/A2DP) + SPP 服务     主机: Windows forwarder 传输
L1 基础设施 buildroot/kernel/dts/固件/构建部署
```

### 目录结构(v0.5 落定)

```
bl_music_player/
├── README.md                      总纲
├── docs/
│   ├── architecture.md            工程分解明细(接口契约/WBS/风险)
│   └── third-party.md             第三方依赖管理方案
├── third_party/
│   └── lvgl/                      ★submodule 官方 v9.6.0 (唯一真依赖)
├── device/                        设备端(i.MX6ULL)
│   ├── app/                       ★收编自 lv_port_linux 的应用骨架
│   │   ├── build.sh                 ARCH=host(桌面调试)/arm(板子)
│   │   ├── CMakeLists.txt / Kconfig / lv_conf.h
│   │   ├── src/lib/                 显示/输入后端(sdl/fbdev/evdev/drm...)
│   │   └── src/{main.c,lyrics_demo.c,ui_theme.c,model,presenter,view}
│   └── libs/
│       └── bl_audio/               mixer + sink + wav(已实现, 冻结 v0.2)
│                                   (bl_media / bl_bt 待 M1-M3 实现)
├── host/windows/                  主机端: media_forwarder(C# SMTC→传输)
├── linux_driver/                  驱动/内核学习验证区
├── configure/                     buildroot/kernel/uboot 配置库
├── resources/                     音频资源
└── bin/                           (预留)
```

### 第三方依赖

| 依赖 | 形式 | 升级方式 |
|---|---|---|
| lvgl | **git submodule** `third_party/lvgl` (v9.6.0) | `cd third_party/lvgl && git fetch --tags && git checkout <tag>` 后 `git add third_party/lvgl` |
| lv_port_linux | **已收编**为 `device/app`(自有代码, 无升级义务) | 需要时手动参考上游 |
| rtl8761b-firmware | git submodule(`linux_driver/bluetooth/`) | 同 lvgl |

克隆本项目后需初始化子模块:

```bash
git submodule update --init --recursive
```

### GitHub 加速(WSL 环境)

本机用 **Watt Toolkit** 加速 GitHub 代理监听 `127.0.0.1:26561`; WSL 为 `Mirrored` 网络模式 → 可直连该代理。已配置:

```bash
git config --global http.proxy  http://127.0.0.1:26561
git config --global https.proxy http://127.0.0.1:26561
# 取消: git config --global --unset http.proxy && git config --global --unset https.proxy
```

> 依赖 Watt Toolkit 运行中; 若代理端口变化(设置里可查), 同步改这两项。

## 技术方案

### 总体架构(v0.5)

```
┌ Windows PC ──────────────┐   ┌ Android / macOS / Linux ─┐
│ forwarder: SMTC +封面     │   │ 系统媒体会话(Spotify等)   │
│  +LRCLIB歌词 +位置外推     │   └──────────┬──────────────┘
└─────────┬────────────────┘              │
          │ SPP 私有通道(待做)             │ 标准 A2DP + AVRCP
          ▼                               ▼
┌──────────────────────────────────────────────────────┐
│ i.MX6ULL (BlueZ + LVGL)                              │
│  bl_media  会话抽象: 元数据/状态/进度/命令 + 双后端合并 │
│  bl_bt     A2DP sink(SBC解码→bl_sink) / AVRCP / SPP   │
│  bl_sink   PCM → plughw:1,1(ASRC) → WM8960           │
│  bl_mixer  音量(AVRCP 绝对音量将来映射到此)             │
│  LVGL MVP  now-playing UI + 控制 → bl_media           │
└──────────────────────────────────────────────────────┘
```

接口契约(冻结点)见 `docs/architecture.md` 第 2 节: `bl_media.h`/`bl_media_backend.h`/`bl_bt.h`
+ forwarder 协议。

### 目标平台已验证的硬件约束(wm8960)

| 约束 | 结论 |
|---|---|
| 卡号 | `wm8960-audio` = **card 1**; `/etc/asound.conf` 默认已指向 card 1 |
| PCM 设备 | device 0 = HiFi 直连(**仅 48000/stereo**); device 1 = HiFi-ASRC-FE(任意采样率, **A2DP 输出固定用它**) |
| 直连链路 EIO 墙 | 44.1k 或单声道 → `snd_pcm_writei` 返回 -EIO(WM8960 时钟主模式 BCLK 整数分频无解) |
| Speaker 音量刻度 | `127 = 0dB`, **100 只有 -21dB**; 'Playback Volume' 是 DAC 数字音量 |
| 路由开关 | 必须开 `Left/Right Output Mixer PCM`, 否则流起不来; `bl_mixer_route_default()` 一键拉起 |
| 耳机检测 | 插耳机自动禁用 Ext Spk; 排查看 `/sys/kernel/debug/asoc` 的 Ext Spk 状态 |

### 蓝牙基础设施状态(buildroot)

| 包 | 状态 | 说明 |
|---|---|---|
| `BR2_PACKAGE_BLUEZ5_UTILS` | ✅ | 含 PLUGINS_AUDIO |
| `BR2_PACKAGE_SBC` | ✅ | A2DP SBC 解码库 |
| `BR2_PACKAGE_BLUEZ5_UTILS_EXPERIMENTAL` | ⛔ **未开(M0 必改)** | 不开则无远端 `org.bluez.MediaPlayer1`(AVRCP) |

## 构建方式

### 应用(lvglsim, 桌面/板子)

```bash
cd device/app
bash build.sh ARCH=host     # 桌面(SDL)调试 → build_host/bin/lvglsim
bash build.sh ARCH=arm      # 交叉编译(板子) → build_arm/bin/lvglsim
```

### bl_audio 库(主机自测)

```bash
cd device/libs/bl_audio
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release && make
./bl_audio_smoke            # 无声卡环境下验证库链接与错误路径
```

### linux_driver/sound 测试程序

```bash
cd linux_driver/sound
make                        # test/ 下所有 .c 交叉编译为同名可执行
```

## 开发计划(里程碑 M0-M5)

| 里程碑 | 内容 | 验收标准 | 依赖 |
|---|---|---|---|
| **M0 基建** | buildroot 开 experimental; BlueZ 冒烟; 部署脚本 | `busctl tree org.bluez` 见 adapter; 手机可配对 | — |
| **M1 元数据/控制** | bl_media 契约冻结 + AVRCP 后端; CLI 调试程序 | Android 播歌, 板端 CLI 打印曲目/状态, 命令生效 | M0 |
| **M2 蓝牙音频** | bl_a2dp(endpoint→fd→SBC→bl_sink) | 手机放歌, WM8960 出声, 无 EIO | M0 |
| **M3 forwarder 通道** | 协议冻结; C# SPP 发送 + 板端 spp 后端 | Windows 播歌, 板端 CLI 收到封面/歌词/位置 | M1、主机侧 |
| **M4 LVGL 界面** | MVP 落地: now-playing + 控制 | 人手全流程操作 | M1-M3 |
| **M5 打磨** | 漂移/绝对音量/断连重连/多设备 | 长播稳定、异常自恢复 | M4 |

并行: 主机端(C# 无板可调) ∥ 设备端 M0-M2。

## 技术方案变更记录

> 工作流约定: 技术方案确定或变更时, 在此追加记录并同步更新上面各章节。

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-10-12 | v0.5 | **工程结构落定**: lv_port_linux 收编为 `device/app`; lvgl 转 submodule(`third_party/lvgl` @ v9.6.0, 从仓库历史抢救并切官方 tag, gitlink 可 fetch); `src/platform/linux/audio`→`device/libs/bl_audio`; `src/platform/windows`→`host/windows`。验证: host 构建 `lvglsim` 通过 + `bl_audio_smoke` 通过。新增 GitHub 代理配置(Watt Toolkit) |
| 2026-10-10 | v0.4 | 工程分解定稿(分层/结构/接口契约/迁移方案/WBS); 开发计划改为里程碑 M0-M5; 新增 `docs/architecture.md` |
| 2026-10-10 | v0.3 | 目标重定位: 本地播放器 → 多平台蓝牙媒体终端; 新增 bl_media/bl_a2dp 规划; BlueZ experimental 为硬阻塞 |
| 2026-10-09 | v0.2 | bl_audio 库方案: mixer/sink/wav 静态库; 主机自测 + ARM 交叉编译通过 |
| 2026-10-06 | v0.1 | linux_driver/sound 测试程序 |

---

# Coding Format

## comment | annotation

1. The header comment should be applied with the doxygen-comment style, in other cases the comment should be included with '//'.
2. The curly braces of a function is a new line seperated from the function name line. like this:
        ```
        int print_hello(void)
        {
                // function body...
                return 0;
        }
        ```
3. All the indent should be 8-spaces size and substituted to tab.
4. The code block "switch... case" should be like this:
        ```
        switch(char){
        	case 'a':
                	// code
        	case 'b':
                // ...
                default:
                // code
        }
        ```
5. The variables' declaration should be the header of the function.
6. parentheses in code block should be like this:
        ```
        // for
        for(int i=0;i<\x;i++){
                //...
        }

        tyepdef struct {
		int	a;
		int	b;
        } test_t;
        ```
7. Column's limition: 80
8.
