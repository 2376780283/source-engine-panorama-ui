# CS:GO Panorama 移植 —— 新增命令一览

用法：控制台直接敲；`config.cfg` / `autoexec.cfg` 里写一行；启动行加 `+`（例 `+se_blur 0`）。带 `*` 的会存盘。

## ConVar

- `se_blur` (默认 1) * —— 背景模糊总开关。1 = 开（背景模糊+压暗，CS:GO 观感），0 = 关（背景变锐利）。
- `se_port_backdrop_blit` (默认 0) * —— 旧的后景 blit hack，只做 A/B；0 = CS:GO 行为，别动。
- `se_probe_force_opaque` (默认 0) —— 诊断：所有合成层强制不透明混合。
- `se_port_opaque_blend` (默认 0) —— 诊断：所有 panorama quad 强制不透明混合。
- `se_hittest_probe` (默认 0) —— 打印 hover 命中的面板链到 `engine.log`（前缀 `SE_PORT_HITTEST`）。
- `se_popup_news` (默认 1) * —— 启动时的新闻弹窗。0 = 不弹。
- `se_popup_legacy` (默认 1) * —— 启动时的 "Legacy version of CS:GO" 弹窗。0 = 不弹。
- `se_background_video` (默认 `panorama/videos/anubis720.webm`) * —— VGUI 兜底背景视频路径。
- `se_background_video_enable` (默认 1) * —— VGUI 兜底背景视频开关，0 = 关。
- `ui_mainmenu_bkgnd_movie` (默认 `anubis720`) * —— 主菜单背景影片名（内容 JS 读的那个）。
- `cl_scoreboard_mouse_enable_binding` (默认 `+attack2`) * —— 记分板鼠标选择用的绑定。
- `panorama_menu_layout` (默认 `base_mainmenu.xml`) —— `panorama_menu` 默认加载的布局。

## 控制台命令

- `panorama_menu [0|1|布局路径]` —— 建/删托管菜单视图，或换布局；无参 = 切换。
- `panorama_test [0|1]` —— 建/删 bring-up 测试视图；无参 = 切换。
- `panorama_status` —— 打印 panorama 状态（指针 / 接口版本 / views 数 / 尺寸）。
- `condump` —— 把控制台文本转存 `condumpNN.log`（从 CS:S `gameui.dll` 抢过来的）。
- `panorama_dump_deny_input` —— 列出"拒绝输入给游戏"的面板；release 下不可见（上游原文）。

## 启动开关

- `+panorama_menu base_mainmenu.xml` —— 启动即建菜单视图。**注意是 `base_mainmenu.xml`**，不是 `mainmenu.xml`。
- `-se_keep_vgui_menu` —— 不隐藏 CS:S 的 VGUI 菜单（排错用；默认是隐藏）。
- `-se_panorama_menu_only` —— 老的显式开关，现在与默认行为相同。
- `-se_background_movie anubis720` —— 背景影片名（引擎侧那条，等效 `ui_mainmenu_bkgnd_movie`）。
- `-se_autotest` —— 只有 `build/*.ps1` 测试脚本用它认自己的进程，**引擎不读**。

## 探针文件（追加写，跑前先清）

- `D:\cstrike\engine.log` —— `Msg/Warning` 类探针（`SE_PORT_*`）。测试脚本会先删它。
- `D:\cstrike\se_ui_probe.txt` —— panorama 侧 `fopen` 写的探针（面板树 / 图片 / 合成层 / 样式）。
- `D:\cstrike\se_blurprobe.txt` —— 模糊路径（`BLURFAST` / `BLURCOPY` / `BLURAPPLY`）。
- `D:\cstrike\se_tooltip_probe.txt` —— tooltip 链路。
- `D:\cstrike\se_sound_probe.txt` —— 声音（**验证完应当删除**）。

## 三条别踩

- 想让 release 也能用的开关，**不要加 `FCVAR_DEVELOPMENTONLY`**（会被隐藏，报 Unknown command）。
- panorama 模块里**运行时**新建的 ConVar 必须自己 `RegisterConCommand()`，而且名字必须常驻内存，否则 cfg 读不到。
- `@panorama_disable_blur` **不是开关**（DEVELOPMENTONLY，且只能"关"不能"开"）——切模糊请用 `se_blur`。

## 相关

- 模糊 + `se_blur` 的来龙去脉：`csgo_panorama_port_breakthroughs.md` **T2**
- 探针清理清单：`csgo_panorama_port_checklist.md` §6
