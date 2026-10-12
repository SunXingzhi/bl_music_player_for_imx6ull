# 第三方依赖管理方案 (bl_music_player)

> 现状诊断 + 集成模型 + 迁移步骤。
>
> **状态: 已完成 (v0.5, 2026-10-12)**
> 结果: `lvgl` = submodule `third_party/lvgl` @ **v9.6.0**(gitlink 80ca777e3, 可 fetch);
> `lv_port_linux` 收编为 `device/app`; 应用/库/主机端全部归位。
> 下文的"现状诊断"保留为迁移前记录。

## 1. 核心模型: 依赖 vs 模板

| | `lvgl` | `lv_port_linux` |
|---|---|---|
| 角色 | **依赖(库)** | **模板(应用骨架)** |
| 本质 | 图形库, 被链接 | 可执行 app 骨架(main.c + 后端适配 + 构建系统) |
| 集成 | submodule, **永不改** | clone 后**收编为自己的应用**, 随意改 |
| 升级 | git pull / 换 tag | 无升级义务; 需要时手动 diff 上游 |
| 归属 | `third_party/lvgl`(别人的) | `device/app/`(我们的) |

"第三方不可改"铁律**只适用于依赖(lvgl)**; 模板就是拿来改成自己的。

### 为什么不能把 port 当"不改动的依赖"

1. 它不是库(无可链接 target), `add_subdirectory` 只产它自己的 lvglsim;
2. CMake 与 main.c 强耦合(入口写死在官方脚本);
3. 官方定位即 "starting point/template"。

=> 不能"集成一个未修改的 port"; 只能收编。

## 2. 现状诊断(2026-10-10)

| # | 问题 | 证据 |
|---|---|---|
| 1 | port 被"去 git 化"并入仓库, 升级通道已断 | `.git` 不存在; 6449 文件以普通文件被追踪 |
| 2 | **坏 gitfile**: lvgl 是孤儿副本 | `lv_port_linux/lvgl/.git` = `gitdir: ../.git/modules/lvgl`(失效) |
| 3 | 应用代码混在 port 内 | `src/{main.c,lyrics_demo.c,ui_theme.c,model,presenter,view}` |
| 4 | 官方构建脚本被改 | `CMakeLists.txt:80` 硬编码应用源文件(收编后此改动合法) |

量化: port 本体(去 lvgl) = 80 文件; lvgl = 6369 文件。
版本凭证缺失: lvgl 版本待从源码读(`lvgl.h` 的 `LVGL_VERSION_*`); port 来源 commit 待记录。

## 3. 目标结构(收编模型)

```
bl_music_player/
├── third_party/
│   └── lvgl/                submodule 官方(pin tag), 唯一真依赖
├── device/
│   ├── app/                 lv_port_linux 整体收编 = "我们的应用"
│   │   ├── build.sh            保留: ARCH=host(桌面调试)/arm(板子)
│   │   ├── CMakeLists.txt      保留且归我们所有
│   │   ├── Kconfig / lv_conf.h 保留: 后端选择/功能裁剪
│   │   ├── src/lib/            保留: 显示/输入后端(sdl/fbdev/evdev/drm...)
│   │   └── src/{main.c,model,presenter,view}  我们的代码
│   └── libs/{bl_audio,bl_media,bl_bt}
└── docs/third-party.md      本文件
```

## 4. 迁移步骤(3 步)

```bash
# 1. 收编: port 整体成为我们的应用
git mv lv_port_linux device/app
# 2. 拔雷: 删孤儿 lvgl(先记录版本! 见第5节)
rm -rf device/app/lvgl
git rm -r --cached device/app/lvgl
# 3. 依赖化: lvgl 转 submodule
git submodule add https://github.com/lvgl/lvgl.git third_party/lvgl
git -C third_party/lvgl checkout <匹配tag>
# 4. 改 device/app/CMakeLists.txt: lvgl 路径指向 third_party/lvgl(见第6节)
# 5. 更新 README 目录结构 + 变更记录
```

注意: `device/app/src/{main.c,model,presenter,view}` **不用搬走**——device/app 整个都是我们的。

## 5. 前置: 确认 lvgl 版本(删除前必做)

```bash
grep -E 'LVGL_VERSION_(MAJOR|MINOR|PATCH)' lv_port_linux/lvgl/lvgl.h
# 或比对 lv_version.h; 得到版本后 checkout 对应 tag, 保证 API 兼容
```

## 6. device/app/CMakeLists.txt 改动要点

```cmake
# 原来: add_subdirectory(lvgl)            # 官方脚本内的相对路径
# 改为: 指向 submodule
add_subdirectory(${CMAKE_SOURCE_DIR}/../third_party/lvgl lvgl_build)
```

其余(后端源码、Kconfig、build.sh)不动。

## 7. 收编后保留的 port 价值点

| 价值 | 说明 |
|---|---|
| `src/lib/display_backends/*` | 板子用 fbdev, 桌面调试用 sdl; port 真正有用的代码 |
| `build.sh ARCH=host\|arm` | 一份代码双架构: host 调 UI, arm 部署 |
| `Kconfig` + `lv_conf.h` | 按需裁剪 lvgl 省内存 |

## 8. 可选: 历史瘦身

老 commit 仍含 6369 lvgl 文件, clone 体积大; 彻底瘦身需重写历史
(`git filter-repo --path lv_port_linux/lvgl --invert-paths`, 会改 commit hash, 谨慎)。

## 附录: submodule 操作要点(实验验证)

"先下载到指定路径, 再登记为 submodule" —— **可以, 但两个前提**:

| 前提 | 说明 |
|---|---|
| 预下载的必须是**真正的 git 仓库**(clone 来的) | 解压的 tar/zip 不行; 报 `already exists and is not a valid git repo` |
| 目标路径不能在 index 里 | 否则报 `already exists in the index`, 先 `git rm -r --cached <path>` |

```bash
# 方式 A(网络不稳: 先下载再登记)
git clone --depth 1 <url或镜像> third_party/lvgl
git -C third_party/lvgl fetch --tags && git -C third_party/lvgl checkout <tag>
git submodule add <url> third_party/lvgl      # 打印 "Adding existing repo..."
git submodule absorbgitdirs                   # 可选: .git 收进 .git/modules

# 方式 B(最简)
git submodule add --depth 1 <url> third_party/lvgl
git -C third_party/lvgl checkout <tag>
```

细节: `.gitmodules` 记录的是"他人 clone 用的 url"; 父仓库只记 commit hash(故依赖不可改);
坏 gitfile(本项目 lvgl 现状)不是有效仓库, 必须先删/重建才能收编。

