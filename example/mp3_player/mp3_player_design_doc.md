# MP3 播放器设计与移植方案

| | |
|---|---|
| **文档版本** | v2.0（2026-08-23，取代 2025-08-02 的重构设计文档，见 git 历史 `d9667bd`）|
| **目标平台** | 阶段一：PC/Ubuntu 22.04 模拟器；阶段二/三：i.MX6ULL 开发板 |
| **当前状态** | 阶段一代码完成并提交（`e17115b` 重构、`6b9357b` 播放引擎），待人工 GUI 验收 |

---

## 1. 项目概述

基于 `lv_port_linux` 开发 MP3 播放器。核心策略：**同一份应用代码**先在 PC 模拟器
（SDL 窗口）上开发验证，随后仅通过切换构建配置与音频后端移植到 i.MX6ULL 板端。

目标硬件（i.MX6ULL）：

- LCD：1024x600
- 触摸：电容屏（内核驱动出 `/dev/input/eventX`，经 EVDEV 接入）
- 音频：ALSA（libasound）直写
- 工具链：BSP SDK 自带 `arm-linux-gnueabihf`

## 2. 总体架构

### 2.1 分层视图

```
┌─────────────────────────────────────────────────────┐
│  View 层  example/mp3_player/ui/                    │
│  SquareLine Studio 导出，禁止手写业务逻辑            │
│  Home 屏(入口) + Audio 屏(播放/列表/歌词)             │
└──────────────┬──────────────────────▲───────────────┘
               │ ui_events.c           │ 状态回调刷新控件
               │ (一行式转发)           │
┌──────────────▼──────────────────────┴───────────────┐
│  Bridge 层  backend/ui_bridge.c                     │
│  唯一同时认识 ui.h 与 player_logic.h 的文件          │
│  播放列表动态构建 / 标签·进度条·图标回写 / CJK 字体   │
└──────────────┬──────────────────────▲───────────────┘
               │ player_logic_*()      │ pl_status_t (500ms)
┌──────────────▼──────────────────────┴───────────────┐
│  Model 层  backend/player_logic.c                   │
│  库扫描(dirent+ID3) / 状态机 / lv_timer 解码泵       │
└───────┬─────────────────────────────────────────────┘
        │ PCM(S16LE)
┌───────▼─────────────┐   ┌──────────────────────────┐
│ audio_out HAL       │   │ decoder/minimp3(_ex).h   │
│ ├ audio_out_sdl.c   │   │ 纯C单头，两端共用         │
│ └ audio_out_alsa.c  │   └──────────────────────────┘
│   (板端, 阶段二)     │
└─────────────────────┘
```

### 2.2 目录结构（实际落地）

```text
example/mp3_player/
├── mp3_player_design_doc.md   # 本文档
├── main.c                     # 入口：1024x600、init 链、MUSIC_DIR 宏
├── backend/
│   ├── player_logic.h/.c      # Model：状态机 + 解码泵
│   ├── mp3_tags.h/.c          # ID3v2 文本帧解析（含中文编码转换）
│   ├── audio_out.h            # 音频 HAL 抽象接口
│   ├── audio_out_sdl.c        # PC 实现（SDL 队列音频）
│   ├── ui_bridge.h/.c         # View↔Model 桥接
│   └── decoder/
│       ├── minimp3.h          # vendor 自 lieff/minimp3（公有领域）
│       └── minimp3_ex.h       # 流式打开 / 帧级读取
└── ui/                        # SquareLine Studio 1.6.1 + LVGL 9.3 导出
    ├── ui.c/h                 # 屏幕初始化（勿手改，重导出会覆盖）
    ├── screens/ui_Home.*      # 主屏：audio 入口图标可点击跳转
    ├── screens/ui_Audio.*     # 播放屏：封面/信息/进度条/歌词面板/播放列表/四控制键
    ├── ui_events.c            # 事件回调——仅一行式转发（见 §4.5）
    └── images/ components/ fonts/
```

### 2.3 数据流

**View → Model（用户操作）**

```
点击播放列表行 ──► on_track_item_clicked(e) ──► player_logic_select(idx)
点击 Play/Pause ─► AudioPlayToPause/PauseToPlay ─► player_logic_toggle_play()
点击 Prev/Next ──► AudioPreMusic/AudioNextMusic ─► player_logic_prev/next_track()
点击 Home 的 audio 图标 ─► _ui_screen_change(ui_Audio)   （纯 View 内部）
```

**Model → View（状态上报）**

```
lv_timer(50ms 解码泵)
  ├─ PLAYING 且队列水位 < 160KB ──► minimp3 解码一帧 ──► audio_out_write()
  ├─ EOF ──► 自动 start_track(next)
  └─ 每 10 tick(500ms) ──► pl_status_t{title,artist,duration,position,state}
                            ──► status_cb == ui_bridge_on_status
                                 ├─ 歌名/歌手标签（source_han_sans_sc_16_cjk 字体）
                                 ├─ ui_AudioPlayBar range/value
                                 ├─ mm:ss 时间标签 ×2
                                 └─ Play/Pause 图标可见性互换
```

### 2.4 线程模型

- **LVGL 主循环**承载全部业务：扫描、解码、UI 刷新。无跨线程 UI 访问。
- **SDL 音频线程**（库内部）只做一件事：从设备队列取样本送声卡。
- 生产者(`audio_out_write`)与消费者的同步由 SDL 队列 API 内建，应用层零锁。

## 3. 关键设计决策记录

| # | 决策点 | 选择 | 理由 | 被否方案及原因 |
|---|--------|------|------|----------------|
| D1 | MP3 解码器 | minimp3/minimp3_ex 单头 vendor 入库 | 零外部依赖，PC/ARM 两端同一份源码，无交叉编译负担 | libmpg123/mad：需两端分别装库；ffmpeg：过重 |
| D2 | PC 音频输出 | SDL 队列音频（`SDL_QueueAudio`, callback=NULL） | SDL2 已因显示后端链接，**零新增依赖**；自动经 PulseAudio/PipeWire 与桌面混音共存；免手写环形缓冲与加锁 | PC 直写 ALSA：仅 Linux、绕过声音服务器易冲突、无额外收益 |
| D3 | 板端音频输出 | ALSA(libasound) 直写（阶段二实现） | 板上通常无 PulseAudio，直连是嵌入式标准做法；可精确获取播放进度回填 UI | `system("madplay ...")`：拿不到实时进度，音量不可控 |
| D4 | 音频后端切换 | CMake 按 `CONFIG_LV_USE_SDL` 自动选择 `.c` | SDL 开=模拟器=SDL 音频；FBDEV-only=板子=ALSA。**零新增配置项** | 新增 Kconfig 选项：多一处维护 |
| D5 | 状态回写机制 | 回调函数直连（`pl_status_cb_t`） | 主线程内触发天然安全；实现直观 | `lv_msg` 消息总线：解耦更彻底但需开 `LV_USE_MSG`，列为后续演进项 |
| D6 | SLS 重导出保护 | 业务全放 `backend/`；`ui_events.c` 只保留单行转发 | SLS 重导出会清空 `ui_events.c` 函数体，单行转发秒恢复 | 在 screens/*.c 里写业务：必被覆盖丢失 |
| D7 | 播放控制按钮来源 | SquareLine 中添加并导出 | 遵守"UI 归 SLS"原则，视觉风格统一 | 代码动态创建：重导出易冲突（曾作为过渡方案验证可行） |
| D8 | 曲目时长获取 | 首帧 bitrate 估算 `filesize*8/kbps`(毫秒) | 打开即得，CBR 精确 | `MP3D_DO_NOT_SCAN`=0 全文件扫索引：大文件开启慢；VBR 场景估算误差可接受 |
| D9 | 中文显示 | 启用内置 `SOURCE_HAN_SANS_SC_16_CJK` + ID3 编码转 UTF-8 | .config 已启用该字体，无需字体生成工具链 | SLS 自定义 TTF：需重新生成资源，CJK 全量字库体积大 |
| D10 | 页面管理 | SLS 连线 + `lv_scr_load_anim`（沿用 v1 文档决策） | 已验证工作正常 | 手写 page_manager：已于 `e17115b` 移除 |

## 4. 模块实现细节

### 4.1 player_logic（Model）

公开接口：

```c
int  player_logic_init(const char *music_dir, pl_status_cb_t cb, void *user);
int  player_logic_get_track(int index, pl_track_info_t *out);
void player_logic_toggle_play(void);
void player_logic_next_track(void);      /* 含播完自动切歌 */
void player_logic_prev_track(void);
void player_logic_select(int index);
```

要点：

- **库扫描**：POSIX `readdir` 过滤 `*.mp3`（上限 64 首），逐文件读 ID3；
  无标题时回退用文件名去扩展名。
- **lv_fs 路径规则**：native 路径前拼盘符构造 `"A:<path>"`（盘符 =
  `LV_FS_STDIO_LETTER`，Kconfig 当前为 65/'A'）。minimp3 通过
  `mp3dec_io_t{read,seek}` 回调对接 `lv_fs_read/lv_fs_seek`，
  因此解码层同样具备板端可移植性。
- **流打开**：`mp3dec_ex_open_cb(..., MP3D_DO_NOT_SCAN)` 快速打开，
  首帧即填充 hz/channels/bitrate_kbps。
- **时长估算**（注意单位）：`duration_ms = filesize*8 / bitrate_kbps`
  —— bits 除以"千比特"恰好直接得到毫秒，**不要**再除 1000（曾因此出 bug，见 §6）。
- **解码泵伪代码**：

```c
pump_cb():  /* lv_timer, 50ms */
    if state != PLAYING or !dec_open: return
    while audio_out_queued() < 160KB:
        got = mp3dec_ex_read_frame(...)        /* ≤1152 samples */
        if got == 0: auto_next(); return       /* EOF */
        written = audio_out_write(samples, got/ch)
        decoded_samples += written*ch          /* 进度按实际入队计 */
        if written < got/ch: break              /* 队列满，下轮再续 */
    every 500ms -> report_status()
```

### 4.2 audio_out HAL

接口（`audio_out.h`）：

```c
int    audio_out_init(void);
int    audio_out_open(uint32_t sample_rate, uint8_t channels); /* S16_LE 交错 */
size_t audio_out_write(const int16_t *pcm, size_t frames);     /* 尽力写,不阻塞 */
size_t audio_out_queued(void);
void   audio_out_pause(bool on);
void   audio_out_set_volume(uint8_t pct);                      /* 软音量 0..100 */
void   audio_out_close(void);
```

SDL 实现（`audio_out_sdl.c`）：

- `SDL_OpenAudioDevice(..., callback=NULL)` 启用**队列音频模式**：
  设备 FIFO 由 SDL 内部线程消费，生产侧无需任何锁。
- `write()` 上限 192KB 队列（约 0.5s@44.1k/stereo/S16）；满则返回 0，
  由解码泵轮询补位——保证永不阻塞 LVGL 主循环。
- 音量在入队前以整数定点缩放（permile→255 定标），避免回调内逐样本浮点。
- `pause()` 用 `SDL_PauseAudioDevice`：静音输出且暂停排空，恢复无缝续播。

### 4.3 mp3_tags（ID3v2 元数据）

- 支持 ID3v2.2(3字符帧ID+24bit尺寸)、v2.3(plain32)、v2.4(syncsafe32)。
- 提取 `TIT2/TT2`(标题)、`TPE1/TP1`(歌手)；其余帧 seek 跳过。
- 四种文本编码统一转 UTF-8：0=Latin-1、1=UTF-16(BOM 自适应)、2=UTF-16BE
  （含代理对拼接）、3=UTF-8 透传。
- 边界处理：帧长越界/截断保护、padding NUL 终止、截断时裁掉半个多字节序列。

### 4.4 ui_bridge（桥接）

- `ui_bridge_init(dir)`：调用 `player_logic_init` 注册状态回调，
  按曲目数构建播放列表行（隐藏 SLS 样板行）。
- 行规格复制自样板 `ui_AudioPlaylistSubPanel`：668x40、居中；
  三列标签偏移 x=-307/-22/+211；行 user_data 存索引，CLICKED→`select(idx)`。
- 状态映射：歌名/歌手设 `lv_font_source_han_sans_sc_16_cjk`；
  进度条 range=[0,duration_ms]；时间标签 `%02u:%02u`；
  `PLAYING`⇒隐 Play 显 Pause，否则反之。

### 4.5 ui_events 接线表（SLS 重导出后按此恢复）

```c
#include "backend/player_logic.h"

on_track_item_clicked(e) { select((int)(intptr_t)user_data_of(current_target)); }
AudioPlayToPause(e)      { player_logic_toggle_play(); }
AudioPauseToPlay(e)      { player_logic_toggle_play(); }
AudioPreMusic(e)         { player_logic_prev_track(); }
AudioNextMusic(e)        { player_logic_next_track(); }
```

> Play/Pause 都只调 toggle_play：图标可见性由状态回调统一驱动，
> 避免 UI 状态与真实播放状态漂移。

### 4.6 main 启动序列

```
lv_init → settings(1024x600) → driver_backends_register/init(SDL)
→ ui_init() → ui_bridge_init(MUSIC_DIR)   /* 内含 player_logic_init */
→ driver_backends_run_loop()
```

音乐目录：宏 `MUSIC_DIR_DEFAULT "/home/tanxzh/Music"`，
环境变量 `LV_MUSIC_DIR` 可覆盖。

## 5. 构建系统说明

- 根 `CMakeLists.txt` 以 `file(GLOB_RECURSE example/mp3_player/*.c)` 收源，
  新增 `.c` 免登记；include 目录已含 `example/mp3_player` 与 `ui/` 各子目录，
  因此业务代码可用 `"ui/ui.h"`、`"backend/player_logic.h"` 项目相对引用。
- **SDL2 链接陷阱**：lvgl 目标将 SDL2 以 PRIVATE 链接，lvglsim 必须显式
  `find_package(SDL2)` 并 `target_link_libraries(lvglsim PUBLIC SDL2::SDL2)`
  （已在根 CMakeLists 处理，仅当 `CONFIG_LV_USE_SDL` 时启用）。
- 双 build 目录并行（阶段二启用）：

```bash
cmake -B build-pc                                        # .config (SDL)
cmake -B build-imx6ull -DCMAKE_TOOLCHAIN_FILE=cmake/user_cross_compile_setup.cmake
```

## 6. 阶段一验证记录（2026-08-23）

### 6.1 逻辑层独立测试（脱离 GUI，直接驱动 player_logic）

测试素材：`~/Music/` 三首 ffmpeg 生成的正弦波 MP3（128kbps CBR，
test1 带 UTF-8 ID3v2.4 中文标题，test3 无标签作文件名回退用例）。

结果：

| 用例 | 结果 |
|---|---|
| 目录扫描 | ✅ 3 首全部发现 |
| 中文 ID3 解析 | ✅ 「晴天测试曲一 / 测试歌手甲」UTF-8 正确还原 |
| 无标签回退 | ✅ 显示文件名 test3 |
| 播放推进 | ✅ position 0→2→3→4s 连续递增 |
| 时长估算 | ✅ 45s/60s/90s 与实际一致 |
| 手动切歌/选曲 | ✅ 状态与标题即时刷新 |

### 6.2 开发过程中修复的问题（备忘）

1. **时长单位换算错误**：初版 `bits/(kbps*1000)` 得到的是秒，显示恒为 0s；
   改为 `bits/kbps` 直接得毫秒。
2. **SDL 音频方案简化**：原计划手写 SPSC 环形缓冲 + 回调；发现 callback=NULL
   即启用 SDL 内建队列 FIFO，删除约 60 行手写并发代码。
3. **编辑事故**：修正单位时一次 edit 误删 `open_current_stream` 函数签名并残留
   旧版重复定义，编译前复查发现，已恢复。
4. **SLS 导出属性坑**：ContainerAudio 曾被意外取消 Clickable 导致无法进入
   Audio 屏；最终以「图标本身可点击 + 独立跳转事件」方案解决（更精准）。
5. **SDL2 PRIVATE 链接**：lvglsim 直接使用 SDL API 需自行 find_package 链接。

### 6.3 待人工验收清单（GUI 交互项）

- [ ] ① 1024x600 窗口正常渲染
- [ ] ② Home → Audio 跳转
- [ ] ③ 播放列表列出 ≥3 首
- [ ] ④ 点击曲目出声、进度条/时间走动
- [ ] ⑤ 暂停/恢复生效且图标互换
- [ ] ⑥ 上一首/下一首切换正常
- [ ] ⑦ 中文歌名显示正常
- [ ] ⑧ 播完自动切下一首

运行方式：`./build/bin/lvglsim`（素材默认读 `~/Music/`）。

## 7. 阶段二：i.MX6ULL 板端移植（待实施）

原则：**不改应用代码**，只动构建配置与新增一个音频后端文件。

### 7.1 上板前置检查

```bash
fbset                          # ① 确认默认色深与分辨率
cat /proc/bus/input/devices    # ② 找触摸 eventX 节点
aplay -l                       # ③ 确认声卡与 ALSA 驱动就绪
```

### 7.2 色深决策树

```
fbset 显示 32bpp(XRGB8888)?
├─ 是 → 保持 CONFIG_LV_COLOR_DEPTH_32=y，零改动 ✅
└─ 否(16bpp RGB565 常见) →
    ① defconfig 改 CONFIG_LV_COLOR_DEPTH_16=y
    ② 注释 ui/ui.c 中 "#if LV_COLOR_DEPTH != 32 #error"
    ③ SquareLine 按 RGB565 重新导出图片资源
```

### 7.3 板端 defconfig（新建 configs/imx6ull_fbdev.defconfig）

```
CONFIG_LV_USE_CLIB_MALLOC=y
CONFIG_LV_USE_CLIB_STRING=y
CONFIG_LV_USE_CLIB_SPRINTF=y
CONFIG_LV_OS_PTHREAD=y
CONFIG_LV_COLOR_DEPTH_32=y          # 按 fbset 结果调整
CONFIG_LV_DEF_REFR_PERIOD=33        # 单核 A7 先 30fps，流畅后再提
CONFIG_LV_DRAW_SW_DRAW_UNIT_CNT=1   # i.MX6ULL 单核
CONFIG_LV_USE_LINUX_FBDEV=y
CONFIG_LV_USE_EVDEV=y
CONFIG_LV_FS_STDIO=y                # 盘符 'A' 保持与 PC 一致
# 裁剪：关闭 demo/benchmark/lottie/svg/qrcode/gif 等
# 字体仅保留 montserrat 14/30 + source_han_sans_sc_16_cjk
```

### 7.4 工具链与 ALSA 后端

- BSP SDK 环境 source 后，`cmake/user_cross_compile_setup.cmake` 指向
  `$OECORE_TARGET_SYSROOT`；sysroot 缺 libasound 则从板端 rootfs 拷贝预编译库。
- 新增 `backend/audio_out_alsa.c`：`snd_pcm_open("default")` →
  `snd_pcm_set_params(SND_PCM_FORMAT_S16_LE, interleaved, latency≈100ms)` →
  循环 `snd_pcm_writei()`；接口与 SDL 版逐一对应（§4.2），
  音量先用软件缩放（规避不同 codec mixer 名差异）。
- CMake 选择逻辑（D4 决策）：

```cmake
if(CONFIG_LV_USE_SDL)
    ... audio_out_sdl.c + SDL2::SDL2
else()
    ... audio_out_alsa.c + 链接 asound
endif()
```

### 7.5 部署与调试（阶段三）

```bash
scp build-imx6ull/bin/lvglsim root@<board>:/usr/bin/   # 或 NFS rootfs
# 板上：
export LV_LINUX_FBDEV_DEVICE=/dev/fb0
export LV_LINUX_EVDEV_POINTER_DEVICE=/dev/input/event1
export LV_MUSIC_DIR=/mnt/sdcard/music                  # U盘/NFS 曲目目录
./lvglsim
```

权限：加入 `video`、`input` 组，避免 sudo 运行。
卡顿调优顺序：确认 fbdev 双缓冲 → 保持 REFR_PERIOD=33 → 关 SYSMON →
最后考虑降色深/降分辨率。

## 8. 风险登记表

| 风险 | 影响 | 缓解措施 | 状态 |
|---|---|---|---|
| 板端 framebuffer 仅支持 16bpp | 图片资源需重导出 | §7.2 决策树预案 | 待上板确认 |
| VBR 文件时长估算偏差 | 进度条比例失真 | 显示层面可接受；后续可引入 `MP3D_DO_NOT_SCAN=0` 扫描或 Xing 头解析 | 已知限制 |
| ID3 非 UTF-8 老编码(GBK)乱码 | 歌名显示异常 | 测试素材固定 `-id3v2_version 4`；转换器已支持 UTF-16/Latin-1；GBK 属罕见遗留格式，出现再评估 | 低概率 |
| SLS 重导出覆盖手写代码 | 功能回退 | D6 约定：业务只在 backend/；ui_events.c 单行转发秒恢复 | 制度性防范 |
| 板端性能不足（单核 A7） | UI 卡顿 | 30fps 起步、裁剪特性清单已列入 defconfig | 阶段二实测 |
| minimp3 seek 精度（未实现拖动进度条） | 功能缺失（非缺陷） | 本期进度条仅展示；PlayBar 为 bar 非 slider，后续如需拖动改 slider + `mp3dec_ex_seek(MP3D_SEEK_TO_SAMPLE)` | 规划外延 |

## 9. 附录

### 9.1 测试素材生成

```bash
mkdir -p ~/Music
ffmpeg -f lavfi -i sine=frequency=440:duration=90 -codec:a libmp3lame -b:a 128k \
  -metadata title="晴天测试曲一" -metadata artist="测试歌手甲" \
  -id3v2_version 4 ~/Music/test1.mp3
# test2: 英文标题; test3: 无标签（验证文件名回退）
```

### 9.2 命令速查

```bash
cmake --build build -j$(nproc) && ./build/bin/lvglsim   # 构建并运行
LV_MUSIC_DIR=/path ./build/bin/lvglsim                   # 指定曲目目录
defconfig configs/get_started.defconfig                  # 重置 Kconfig（会覆盖 .config）
menuconfig                                               # 交互调配置
```

### 9.3 相关提交

| 提交 | 内容 |
|---|---|
| `d9667bd` | v1 设计文档（MVC 重构构想，历史参考） |
| `e17115b` | MVC 重构落地 + SLS UI 首次导入（800x480→1024x600 演进） |
| `6b9357b` | 播放引擎全量接入（本文 §4 所述实现） |
