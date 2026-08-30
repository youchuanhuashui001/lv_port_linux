# MP3 Player 项目规则

## UI 由 SquareLine Studio 维护

- 将 `ui/` 视为 SquareLine Studio 的导出目录。界面布局、控件、样式、图片、
  事件绑定和导出的事件函数都不能手工修改。
- 所有 UI 改动必须先在 SquareLine Studio 源工程中完成，再导出到 `ui/`，
  并将导出的文件一起提交。
- 运行时逻辑、播放逻辑、数据绑定和调试打印放在 `backend/` 或其他非导出文件中；
  导出的回调函数只负责转发到运行时代码。
- 导出后如果改动丢失，应修改 SquareLine 源工程或调整后端代码，不能直接修补导出文件。
- 每次导出后都要检查 `git diff`，并完成 MP3 Player 编译验证。

SquareLine 源工程必须和本目录一起纳入版本管理。当前 `ui/project.info` 标识的源工程名为
`SquareLine_Project.spj`。
