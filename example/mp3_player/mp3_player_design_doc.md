# MP3 播放器重构设计文档 (基于 SquareLine Studio 与 LVGL)

## 1. 架构概览

本项目将摒弃原有的手工页面管理器 (`page_manager`)，全面转向 **MVC (Model-View-Controller)** 架构，以实现 UI 设计与核心业务逻辑的彻底解耦。

*   **View (视图层)**：由 SquareLine Studio (SLS) 自动生成。负责屏幕布局、样式、动画以及基本的用户交互捕获。
*   **Model/Controller (逻辑层)**：由 C 语言手写实现 (`player_logic`)。作为播放器的“大脑”，负责状态维护（播放/暂停、进度）、播放列表管理以及对接底层音频硬件或系统接口。

---

## 2. 目录结构设计

重构后的 `@example/mp3_player/` 目录结构建议如下：

```text
example/mp3_player/
├── CMakeLists.txt        # 构建脚本
├── main.c                # 主入口（负责初始化 LVGL、外设和逻辑）
├── backend/              # 业务逻辑层 (Model)
│   ├── player_logic.h    # 播放器核心逻辑接口
│   └── player_logic.c    # 播放状态机与核心实现
└── ui/                   # 视图层 (View, 由 SquareLine 生成)
    ├── ui.h              # UI 全局变量与接口
    ├── ui.c              # UI 初始化
    ├── ui_events.h       # UI 事件回调声明
    ├── ui_events.c       # UI 事件回调实现 (在此调用 backend 的接口)
    ├── ui_helpers.h/c    # SLS 辅助函数
    ├── screens/          # SLS 自动生成的各页面文件
    └── assets/           # 图片、字体资源
```

---

## 3. UI 与业务逻辑的交互机制

交互是双向的，核心思想是**“互不干涉内部实现，仅通过接口/变量通信”**。

### 3.1 UI 触发业务逻辑 (View -> Model)
1.  **UI 端配置**：在 SquareLine Studio 中，选中交互控件（如“播放”按钮），为其添加 `Event`，Action 选择 `Call function`，命名为 `on_play_btn_clicked`。
2.  **代码端桥接**：SLS 会在 `ui_events.c` 中生成空函数。开发者在其中填入业务代码：
    ```c
    // ui_events.c (手动实现)
    #include "backend/player_logic.h"
    
    void on_play_btn_clicked(lv_event_t * e) {
        // 仅仅通知逻辑层，不关心 UI 的变化
        player_logic_toggle_play();
    }
    ```

### 3.2 业务逻辑更新 UI (Model -> View)
当底层状态发生变化（例如歌曲自然播放完毕，切换到了下一首），需要更新 UI。
1.  **获取 UI 句柄**：SLS 会在 `ui.h` 中将关键控件暴露为全局变量，如 `extern lv_obj_t * ui_Label_SongTitle;`。
2.  **直接操作或解耦操作**：
    *   **基础做法**：在 `player_logic.c` 的状态刷新定时器中，包含 `ui.h`，直接调用 `lv_label_set_text(ui_Label_SongTitle, "新歌曲名");`。
    *   **高级做法 (推荐)**：使用 LVGL 原生的**消息总线 (`lv_msg`)**。
        *   逻辑层抛出消息：`lv_msg_send(MSG_TRACK_CHANGED, new_track_info);`
        *   UI 初始化时订阅消息，收到消息后自动刷新自己。这样逻辑层可以完全不包含 `ui.h`。

---

## 4. 页面管理 (屏幕路由)

摒弃旧版使用容器嵌套来模拟页面的做法。
所有页面的跳转、路由、过场动画（如淡入淡出、侧滑）**全部在 SquareLine Studio 内部通过连线设计完成**。SLS 生成的代码会使用 LVGL 原生的 `lv_scr_load_anim()` 来管理屏幕的生命周期。C 后端无需编写任何页面管理的业务代码。

---

## 5. 跨平台移植规划 (模拟器 -> i.MX6ULL)

得益于当前项目 `lv_port_linux` 已有的 `driver_backends` 抽象机制，移植过程被大大简化。

### 阶段一：PC 模拟器阶段 (当前)
*   **图形后端**：通过 SDL2 运行 LVGL，方便实时预览 SquareLine 导出的 UI。
*   **音频后端**：在 `player_logic.c` 中使用简单的 `printf` 打印模拟播放状态，或者引入跨平台的简单音频库播放 WAV/MP3，跑通 MVC 数据流。

### 阶段二：i.MX6ULL 开发板阶段
*   **图形后端**：通过 CMake 切换交叉编译工具链。驱动后端无缝切换至 Linux Framebuffer (`/dev/fb0`) 与 EVDEV 触摸输入 (`/dev/input/event0`)。
*   **音频后端**：根据 i.MX6ULL 搭载的系统环境，在 `player_logic.c` 的实现中接入真实的音频控制：
    *   可以通过 `system("aplay xxx.mp3")` 调用系统命令。
    *   或者链接 `alsa-lib` 进行更细致的音频流控制。
    *   得益于 MVC 架构，以上修改**完全不需要触碰 UI 层的任何代码**。
