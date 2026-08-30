## 结论

`example/mp3_player` 是一个基于 LVGL 的单线程 MP3 播放器：

- `main.c` 负责初始化 LVGL、SDL、UI 和播放器。
- `ui/` 是 View，仅处理界面事件。
- `backend/ui_bridge.c` 负责 UI 与播放逻辑之间的桥接。
- `backend/player_logic.c` 负责扫描曲库、状态机和解码泵。
- `audio_out_sdl.c` 使用 SDL 队列音频输出，SDL 内部线程负责实际播放。

### 1. 启动与交互流程

```mermaid
flowchart TD
    A["main()"] --> B["lv_init()"]
    B --> C["设置窗口 1024x600"]
    C --> D["注册并初始化 SDL/LVGL backend"]

    D -->|失败| E["打印错误并退出"]
    D -->|成功| F["ui_init()"]

    F --> G["创建 Home 和 Audio 页面"]
    G --> H["读取 LV_MUSIC_DIR"]
    H --> I["ui_bridge_init(music_dir)"]

    I --> J["player_logic_init()"]
    J --> K["audio_out_init()"]
    J --> L["scan_library()"]

    L --> M["readdir 扫描目录"]
    M --> N["过滤 .mp3 文件"]
    N --> O["读取 ID3 标题/歌手"]
    O --> P["无标签时使用文件名"]

    P --> Q{"找到 MP3?"}
    Q -->|否| R["创建 50ms 解码定时器\n保持 STOPPED"]
    Q -->|是| S["创建 50ms 解码定时器\n立即上报初始状态"]

    R --> T["构建播放列表并显示停止图标"]
    S --> T
    T --> U["driver_backends_run_loop()"]

    U --> V{"LVGL 事件"}

    V -->|点击 Home 音频图标| W["切换到 Audio 页面"]
    V -->|点击播放列表按钮| X["显示/隐藏播放列表"]
    V -->|点击曲目| Y["player_logic_select(index)"]
    V -->|点击播放/暂停| Z["player_logic_toggle_play()"]
    V -->|点击上一首/下一首| AA["player_logic_prev/next_track()"]

    Y --> AB["打开 MP3 流并更新状态"]
    Z --> AB
    AA --> AB

    AB --> AC["ui_bridge_on_status()"]
    AC --> AD["刷新歌名、歌手、进度条、时间和图标"]
```

### 2. 解码与播放流程

```mermaid
flowchart TD
    A["lv_timer: pump_cb()\n每 50ms 执行"] --> B{"PLAYING 且解码器已打开?"}

    B -->|否| Z["本轮结束"]
    B -->|是| C{"音频队列 < 160KB?"}

    C -->|否| H["检查状态上报周期"]
    C -->|是| D["mp3dec_ex_read_frame()"]

    D --> E{"got == 0?"}
    E -->|是| F["EOF 或解码错误"]
    F --> G["auto_next()"]
    G --> G1["切换到下一首并重新打开流"]
    G1 --> Z

    E -->|否| I["计算 PCM 帧数"]
    I --> J["audio_out_write()"]
    J --> K["SDL_QueueAudio()"]
    K --> L["SDL 音频线程从队列取样播放"]

    J --> M["decoded_samples += 实际写入帧数"]
    M --> N{"写入帧数少于解码帧数?"}

    N -->|是，队列满| H
    N -->|否| C

    H --> O{"累计达到 500ms?"}
    O -->|否| Z
    O -->|是| P["report_status()"]
    P --> Q["ui_bridge_on_status()"]
    Q --> R["更新 UI"]
    R --> Z
```

## 关键状态行为

| 当前状态 | 操作 | 结果 |
|---|---|---|
| `STOPPED` 且未选曲 | 播放 | 打开第 1 首并播放 |
| `PLAYING` | 播放/暂停 | 进入 `PAUSED` |
| `PAUSED` | 播放/暂停 | 恢复 `PLAYING` |
| `STOPPED` | 选曲、上一首、下一首 | 打开曲目但保持暂停 |
| `PLAYING/PAUSED` | 选曲、上一首、下一首 | 打开目标曲目并播放 |
| `PLAYING` 到 EOF | 自动切换 | 循环播放下一首 |

## 静态分析发现的几个问题

1. 播放列表索引读取方式不匹配：

   `make_row()` 将索引作为事件回调参数传入，但 `row_click_cb()` 使用的是 `lv_obj_get_user_data(row)`。两者不是同一个数据源，因此点击动态列表项时索引很可能始终为 `0`。

   - [ui_bridge.c:38](/home/tanxzh/tanxzh/lvgl/lv_port_linux/example/mp3_player/backend/ui_bridge.c:38)
   - [ui_bridge.c:75](/home/tanxzh/tanxzh/lvgl/lv_port_linux/example/mp3_player/backend/ui_bridge.c:75)
   - LVGL API 说明：[lv_event.h:199](/home/tanxzh/tanxzh/lvgl/lv_port_linux/lvgl/include/lvgl/core/lv_event.h:199)

   最小修复是让 `row_click_cb()` 使用 `lv_event_get_user_data(e)`。

2. 设计文档写的是“点击曲目出声”，但当前代码从 `STOPPED` 选曲时传入 `play=false`，实际会进入暂停状态：

   - [player_logic.c:375](/home/tanxzh/tanxzh/lvgl/lv_port_linux/example/mp3_player/backend/player_logic.c:375)
   - [player_logic.c:230](/home/tanxzh/tanxzh/lvgl/lv_port_linux/example/mp3_player/backend/player_logic.c:230)

3. 成功打开解码器后，外部的 `lv_fs_file_t` 没有对应关闭，切歌多次可能造成文件句柄泄漏：

   - [player_logic.c:185](/home/tanxzh/tanxzh/lvgl/lv_port_linux/example/mp3_player/backend/player_logic.c:185)
   - [player_logic.c:205](/home/tanxzh/tanxzh/lvgl/lv_port_linux/example/mp3_player/backend/player_logic.c:205)
   - [minimp3_ex.h:1368](/home/tanxzh/tanxzh/lvgl/lv_port_linux/example/mp3_player/backend/decoder/minimp3_ex.h:1368)

仓库设计文档记录逻辑层测试已通过，但 GUI 手工验收项目仍标记为待验证。未修改代码。
