# CS:GO Panorama（csgoui）移植 —— 任务清单（从开始到现在）

> 配套文档：
> - `csgo_panorama_port_plan.md` —— 可行性分析 + 分阶段计划（Phase 0~5）
> - `CSGO_PANORAMA_PORT_NOTES.md` —— 实操手法 / 正确命令 / 速查
> - `csgo_panorama_port_pitfalls.md` —— **踩坑表**
> - `csgo_panorama_port_breakthroughs.md` —— **重大突破**
> - `csgo_panorama_port_commands.md` —— **新增命令 / 开关一览**（ConVar + ConCommand + 启动开关 + 注册机制）
>
> 图例：✅ 已完成　🔶 进行中　⬜ 未开始　🅿️ 已打包/已归档
> 分支：`CSGO-Panorama`（仓库共 621 提交，移植线从 `391effac` 起）
> 更新：2026-09-19

---

## 0. 目标与总路线

把 CS:GO（2019 版）的 **Panorama UI 框架（csgoui）** 移植到 Source Engine 2013，
用 `panorama_s1wrapper` 把 Source 2 的渲染/资源/材质接口架在 Source 1 上，
让 CS:GO 的**真实内容**（layout / scripts / styles / 字体 / 视频）能在本引擎里跑起来。

分五段推进：

| 段 | 内容 | 状态 |
|---|---|---|
| Phase 0 | 环境与基线固化 | ✅ |
| Phase 1 | Panorama 公共头文件与接口对齐 | ✅ |
| Phase 2 | 框架库移植：panorama / panorama_s1wrapper / panoramauiclient | ✅ |
| Phase 3 | 引擎接入（`PANORAMA_ENABLE`） | ✅ |
| Phase 4 | 客户端 UI 与 csgoui（内容层） | 🔶 主体完成，模糊/面板/本地化收尾中 |
| Phase 5 | 收敛与收尾（清探针、包、验收） | ⬜ |

---

## 1. Phase 0 —— 环境与基线（2026-09-08）✅

| 任务 | 状态 | 证据 |
|---|---|---|
| 确认工具链（VS2022 MSVC 14.44 + waf + Python） | ✅ | `docs/CSGO_PANORAMA_PORT_NOTES.md` |
| 固化正确配置：**cstrike / 32 位 / release** | ✅ | `waf.bat configure -T release --32bits --build-games=cstrike ...` |
| cstrike 基线构建通过 | ✅ | notes §1 |

> 关键事实：本仓库 waf **默认 hl2 + 64 位**，不带参数会跑偏（详见踩坑表 P1）。

## 2. Phase 1 —— 公共头对齐（09-08）✅

| 任务 | 状态 | 提交 |
|---|---|---|
| CSGO2019 `public/panorama` 整组覆盖合入本树 | ✅ | `bcf4b76d` |
| 补 3 个缺失公共头（`utlptrarray.h` / `platwindow.h` / `beziercurve.h`） | ✅ | `bcf4b76d` |
| `panoramatypes.h` 加 `SE_TIER1_FIXEDGROWABLE` 守卫 | ✅ | `bcf4b76d` |
| v8 头镜像到 `thirdparty/v8` | ✅ | `bcf4b76d` |
| 产出报告：`docs/panorama_header_overlay_report.txt` | ✅ | 🅿️ |

## 3. Phase 2 —— 三个框架库移植（09-09 → 09-12）✅

### 3.1 `panorama.lib`（框架本体）

| 任务 | 状态 | 提交 |
|---|---|---|
| 源码树移入 + L1 接口头 + 首轮编译修复 | ✅ | `922bda3d` |
| 编译 shim / 补丁批（SE tier 增量 + CSGO 容器遮蔽） | ✅ | `5fa1e0bc` `cc7f7cd4` `a58e99f5` |
| 核心 13 TU 入列 | ✅ | `2aef4240` |
| **V8 攻坚**：头升到 6.8 → 再统一到 **7.3.492** + 6.x 兼容垫片 | ✅ | `c1517498` `868b6ccd` `876e00eb` `cc0cf539` |
| localize / renderer / data / layoutfile 入列（38 TU 全绿） | ✅ | `e81ff2cb` `5b95b05d` `1a885725` `ec8b6006` |
| protobuf 工具链自建（`scripts/dev/build_protobuf.ps1`） | ✅ | `1a885725` |

### 3.2 `panorama_s1wrapper`（Source2 → Source1 桥）

| 任务 | 状态 | 提交 |
|---|---|---|
| 桥接层编译通过（M1） | ✅ | `ca194c87` |

### 3.3 `panorama_client` + `panoramauiclient`

| 任务 | 状态 | 提交 |
|---|---|---|
| 新建客户端控件静态库（M2）：panel2d + debugger | ✅ | `ef555fc4` |
| 控件批 2/3（34 TU）→ 批 4 调试叠层（38 TU）→ 批 5 textinput（43 TU） | ✅ | `26f261f9` `b6380cc0` `3722050e` |
| M3：`panoramauiclient.dll` **未解析符号 18 → 9 → 0**，链接通过 | ✅ | `586970d9` `3da10b5b` |

## 4. Phase 3 —— 引擎接入（09-12 → 09-13）✅

| 任务 | 状态 | 提交 |
|---|---|---|
| 引擎目标编译+链接（M4 part 1） | ✅ | `de74afc6` |
| 引擎帧循环驱动 panorama（part 2） | ✅ | `8dc1a800` |
| 启动器装载 `panoramauiclient.dll`（part 3） | ✅ | `4ccec702` |
| panorama 渲染器拿到 D3D shader（part 4） | ✅ | `c93d251f` `751b87c1` |
| panorama layout 加载 + 着色器移植 | ✅ | `4b4ac1e2` |
| 每帧向背缓冲发出绘制（M4 收口） | ✅ | `e960df79` |
| 诊断"draw 发出但屏幕无像素" | ✅ | `41dc2cad` |
| **关掉剔除 → UI 真正画出像素** | ✅ | `361ff380` |
| 「没有编译版资源」不再当错误 | ✅ | `a41ffba5` |
| `panorama_status` / `panorama_test` 控制台命令 | ✅ | `18f13779` |

## 5. Phase 4 —— 客户端 UI 与内容层（09-13 → 09-16）🔶

| 任务 | 状态 | 提交 / 证据 |
|---|---|---|
| 让 CS:GO 脚本不再因未知事件名/未实现 API 中断 | ✅ | `39a766f8` |
| shader 组合索引崩溃修复 | ✅ | `acee9c4a` |
| **M5：主菜单加载/布局/绘制/脚本全跑通** | ✅ | `715a424f` |
| 样式值容错 + 数值强制转换 + 视频播放器空指针 | ✅ | `4d6df3ae` |
| 贴图绑定 + 属性静态存储 + 模糊层 RT 接线 + 高斯模糊 shader | ✅ | `6f775ca8` |
| **移植 CS:GO 的 SVG 渲染器（图标能解码）** | ✅ | `d3ec3c95` `23003cec` |
| 输入链路（鼠标事件按 CS:GO 源码补全 + VGUI 隐藏开关） | ✅ | `0da1003c` |
| 按钮有响应（JS 兜底桩）+ 修 `$.HTMLEscape` 崩溃 | ✅ | `d192b115` |
| **webm 动态背景跑通（Media Foundation 播放器）** | ✅ | `3f2de032` + 09-16 实机验证 |
| 游戏侧面板类 `CSGOBlurTarget` / `CSGOBackbufferImagePanel` | ✅ | `af6840ba` |
| 批次 A：`game/client/panorama` 宿主层（`CUI_Root` 等） | ✅ | `9bbad148` |
| 批次 B：tooltip 系统（含 `CSGOTooltipManager`） | ✅ | `a99e34d4` |
| 批次 C+D：popup + context menu + 三个引擎全局 | ✅ | `100afcf1` |
| 主菜单恢复可点击（默认隐藏 VGUI gameui） | ✅ | `e5390e21` |
| 批次 E（上）：cstrike15 UI 组件框架 + `UiToolkitAPI` | ✅ | `e7f3553b` |
| 批次 E（中）：`CCSGO_MainMenu` | ✅ | `6b8fd98e` |
| 布局名归一化 + VGUI 默认隐藏 + 本地化加载 | ✅ | `fb934e63` |
| 7 个未实现面板补桩，JS 异常清零 | ✅ | `59761a61` |
| **任务 A：Esc/反引号按 CS:GO 路由，控制台与 VGUI GameUI 解耦** | ✅ | `b7f0720e` |
| **文本渲染修复（字形被压成横条、颜色发暗）** | ✅ | `b9e0a823` |
| **`.vfont` 字体包装载 → 字体族真正生效** | ✅ | `5ffcd3d4` |
| 主菜单鼠标指针（输入系统光标 + 锁 VGUI 光标） | ✅ | `b7421761` |
| 合成层按 CS:GO 对齐（blur 层 per-layer RT + `se_port_backdrop_blit` 回退开关） | ✅ | `44c35c1e` |
| 移植 `GameInterfaceAPI`（含 `ui_mainmenu_bkgnd_movie` 真 ConVar） | ✅ | `11e71333` |
| 修「点错误窗就退出」（shim 万能真值 stub 的副作用） | ✅ | `cb42a110` |
| 实机确认：真 `code.pbin` 资源包被接受、页面可导航、中文生效、背景视频在播 | ✅ | 09-16 本次会话日志/截图 |
| **定位紫黑缺材质格根因（= 模糊路径）** | ✅ | 真因 = `_rt_FullFrameFB2` 是 **32x32 占位 RT**（P52）；修 `S1Wrapper_FindFullFrameBuffer` 自建同尺寸 scratch；实机紫格消失。**待提交** |
| 模糊取错源 ⇒ 背景是均匀灰 | ✅ | `csgo_blurtarget.cpp` 的 blurrects 查找改回 CS:GO 的 `CUI_Root::GetRootForWindow()`（P57）；实机背景变回**模糊的视频**。**待提交** |
| 模糊路径与 CS:GO 逐函数对拍 | ✅ | `build/_rendercmp.ps1`：9/15 函数 0 差异，其余只差两个默认关闭的 `SE_PortBackdropBlit()` 块 + 探针残行 ⇒ 算法层无罪（P56/P62） |
| 模糊结果偏亮 + 左侧一块平灰 | ✅ **已解挂（09-19）** | 09-17 挂起（P63–P65）；09-19 复查**不再复现**——T1 的 sRGB 修复 + YUV 平面改 A8 之后，开启 blur pass 不再出紫黑格，背景是 CS:GO 那种**模糊+压暗**。已恢复开启并做成 cfg 开关 `se_blur`（默认 `1`），见 `csgo_panorama_port_breakthroughs.md` **T2** |
| **背景模糊改成 cfg 开关 `se_blur`**（默认 `1`） | ✅（09-19） | `source2surface.cpp` 定义 + `se_ui_settings.cpp` 显式 `RegisterConCommand`（`FCVAR_ARCHIVE`，无 DEVELOPMENTONLY）；`config.cfg`/`autoexec.cfg`/控制台/启动行 `+se_blur 0` 都能切，**下一帧生效**。实测 `+se_blur 0` ⇒ 探针 `seBlur=0 … gate=0`，背景恢复锐利（`build/_blursw_off.png`）。见 T2 |
| 关掉模糊后的实测（09-17，历史） | 🅿️ 已作废 | 那轮的结论（静态背景泛白 mean 203.6 / 开视频全黑）是在 **T1 的 sRGB 修复之前**测的；09-19 解挂后两条都不复现。脚本 `build/_bluroff_verify3.ps1`，截图 `build/_bluroff_nomovie.png` / `build/_bluroff.png`。只作历史保留 |
| **移植玩家头像面板 `CSGOAvatarImage`（脱离 Steam，改读本地文件）** | ✅（09-17） | `panorama/seport/gameclient/cstrike15/panorama/csgo_avatarimage.{h,cpp}`（新文件），删掉原 `seport_panel_stubs.cpp` 里的同名桩；图像按 `<steamid64>` → `<accountid>` → `local` 找 `materials/panorama/images/avatars/*.png|jpg|svg`。实机：文件被正确读取并加载（`SE_AVPROBE: image loaded ...`）。见 P67/P68 |
| **本地玩家卡片可见（右上角头像因此能显示）** | ✅（09-17） | 真因不是渲染而是桩数据：`LobbyAPI.IsSessionActive()` truthy ⇒ `party.js` 以为在队伍里 ⇒ `HideLocalPlayer(true)` 把 `JsLocalPlayercard` 整张塌掉（P66）。shim 改答 `false` 等 7 项后卡片回归 |
| 清掉 avatar 探针（`SE_AVPROBE` 三处，含用户加的 per-candidate 行） | ⬜ | `panorama/seport/gameclient/cstrike15/panorama/csgo_avatarimage.cpp`；头像确认正常后删 |
| 清掉部署内容里的死 hack（`mainmenu.css:1232/1243` 两处） | ⬜ | 见 P50/P61；留着会误导 |
| 补齐 8 个缺失面板类型（`CSGOSettingsSlider`/`CSGOAudioSettings`/`CSGOVideoSettings`/`CSGOStatsProgressGraph`/`MapSpiderGraph`/`WeaponSpiderGraph`/`HeatMap` 等） | ⬜ | 设置页/统计页因此为空 |
| 补 JS API 缺口（`RemoveAllOptions` 等） | ⬜ | 日志报 `settingsmenu_gamesettings.js:8` |
| 补完本地化 token（`#CSGO_Tournament_Event_Location_[]` 等） | ⬜ | 日志 `Unable to localize` |
| 移植 batch E 剩余 gameclient 文件 | ⬜ | —— |
| 主菜单渲染整体验收（泛白单独处理完） | ✅（泛白部分，09-19） | 根因=shader 丢 SRGBREAD，修复后 mean 203.6→96.4，灰阶逐条精确；见 `csgo_panorama_port_breakthroughs.md` T1。视频假色也已修（YUV 平面 I8→A8 + fxc 读 .a，09-19 第二阶段，P104/T1） |
| **新闻 / 商店文案走移植自己的本地化**（前缀 `se_port`） | ✅（09-19） | 新增 `<mod>/panorama/localization/se_port_{english,schinese}.txt`（随仓库走：`mods/panorama_test/panorama/localization/`），由 `panoramauiclient/se_uicomponents.cpp` 调 `BLoadLocalizationFile( "se_port" )` 加载（紧随 csgo/cstrike），JS 侧 `se_session_sim.js` 的 `seLocalize()` 把 `STORE_NAMES` / 新闻条目 / 商店按钮都换成 token。实测：默认中文 = `商店` / `巴黎 2023 观众通行证`；`-language english` = `Store` / `Paris 2023 Sticker` / `Operation Payback` / `View On Market` ⇒ **跟语言切换**。原版做法见下条 |
| 原版 CS:GO 新闻/商店的本地化机制（调查结论，写代码前先看） | ✅（09-19） | ① **新闻不用本地 token**：`mainmenu_news.js` 走 `BlogAPI.RequestRSSFeed()` + 事件 `PanoramaComponent_Blog_RSSFeedReceived`，`item.date/title/description` **原样**进 `SetDialogVariable` ⇒ 文案是 **Valve blog 后端按语言下发**的，本地只负责排版。② **商店是“数据 + token”**：版面来自 `generated/items_event_current_generated_store.js`（`g_ActiveTournamentInfo` 全是 id/代号，无文案）+ `StoreAPI.*`；文案用 `$.Localize("#...")`，token 名由数据拼出（`#CSGO_Tournament_Event_Location_<eventid>`、`#<opname>_name`）。③ **物品名**：`itemtile.js` → `ItemInfo.GetFormattedName()`（`common/iteminfo.js`），把名字套进 `#CSGO_ItemName_Base` / `_Painted`（`武器|涂装` 用 `|` 分隔）/ `_Custom` 模板 ⇒ 名字本体也是 token（来自 econ schema `items_game.txt` 的 `item_name`）。**我们的 faux item 对不上真 schema**，所以只能用自己的 token（这是“照原版机制、换 token 表”） |
| 方案 B：`panoramauiclient` 实现 `IGameUI`，panorama 正式接管 GameUI | ⬜ | 可回退任务 A 的三处守卫 |
| 字体 D（可选）：`.uifont` 包（protobuf + OpenSSL AES） | ⬜ | 需拍板；CS:GO 自身内容不用它 |
| **库存页移植（第一批）**：`InventoryItemList` 列表类 + faux econ 目录 + JS 数据层 | ✅（09-19 实机通过） | ① C++：`panorama/seport/gameclient/cstrike15/panorama/se_faux_econ.{h,cpp}`（23 件 faux 物品、4 个分类、排序、图标表）+ `csgo_inventory_item_list.{h,cpp}`（照 CS:GO 结构：`CDelayLoadList` + `SetInventoryFilter` 事件 + `count` JS 访问器 + `itemtile.xml` 每格；econ 调用全部改到 faux 层）。② 布局自引用（`inventory_item_list.xml` 的根元素就是 `<InventoryItemList>`）是**安全的**：`CUIPanel::BApplyLayoutFile` 会把根元素描述应用到**面板自身**而非再造一个实例（源码注释：*doesn't ensure that root of layout file matches this panel type*）。③ JS：`se_session_sim.js` 追加"库存数据层"块（`GetCategories`/`GetSubCategories`/`GetInventoryStructureJSON`/`GetItemRarityColor`(CS:GO 真配色)/`IsEquipped`/`GetSlotSubPosition`/`IsItemInfoValid`/`GetInventoryItemIDByIndex` 等 ~40 个**类型正确**的回答）。④ 分类直接复用 CS:GO 自己的 `Inv_Category_*` token（`csgo_english.txt` 里有 80 个），不需要新增 loc。 |
| 库存页移植（第二批）：`CSGOLoadout` 配装面板 | ✅ 面板可打开（09-19 实机） | `csgo_loadout.{h,cpp}` 注册类型 `CSGOLoadout`，加载 `loadout.xml`，保留 `TeamLogo`/`ItemWheel`/`LoadoutItemList` 三个子面板，回答内容侧的两个事件（`ShowLoadout` / `Loadout_FilterForPosition`）——两者都把物品列表指向 faux 目录。**没有移植**：真 loadout 槽位状态（`CStrikeLoadout`/`CUiComponent_Loadout`）与 `CSGORadialSelector`（仍是降级 Panel）。 |
| 库存物品图标接真素材 | ✅（09-19） | 用户从 CS:GO 安装解出 `resource/flash/econ/**`（19 011 个文件，含 `default_generated` 1 899 / `stickers` 14 221 / `status_icons` 1 100 …），`{images_econ}` = `<mod>/resource/flash`（panorama.cfg），所以格子的 `src` 直接用 `file://{images_econ}/econ/...`。库里按内容实有文件挑：大行动 6..11 用 `status_icons/operation_<n>_gold_large.png`、任务用 `tools/mission.png`、奖励用 `default_generated/weapon_ak47_*_large.png`。 |
| 库存页实机验收 + 两个修复（09-19） | ✅ | 用户实机确认：物品格与图标正常、商店格可点、右键菜单中文条目正常。修复 ① **配装按钮“配置不可用”**：`LoadoutAPI.IsLoadoutAllowed` 曾是 `false`（当年为类型安全乱填的），改 `true` 后按钮可点（`se_api_shim.js` + `se_session_sim.js`）。② **搜索/排序无效**：`GetInventoryCount`/`GetInventoryItemIDByIndex` 返回全目录，改成在 `SetInventorySortAndFilters` 里按**本地化名字过滤 + 排序**的真实结果集。教训：当年为“类型正确”乱填的假值（`IsLoadoutAllowed=false` 等）会变成真 UI 的阻断，需逐个复查（`IsInventoryValid`/`IsConnectedToGC`/`IsCraftReady` …）。 |
| **输入框打不进字（未解决）** | 🅿️ 09-19 试修一次、实机仍无效 | 根因 = Windows 上**没人把 `WM_CHAR` 变成 `IE_KeyTyped`**：SDL 那条路（`inputsystem.cpp` 的 SDL_TEXTINPUT 分支）会发 `IE_KeyTyped`，Win32 虽然因 `TranslateMessage()`（`PollInputState_Windows`）确实生成 `WM_CHAR`，但没人处理它（全引擎唯一的 `WM_CHAR` 处理器在 panorama **调试器**窗口里）⇒ 文本框的 `OnKeyTyped` 永远不响。修 `engine/sys_mainwind.cpp`：`CGame::WindowProc` 加 `case WM_CHAR`（只转发可打印字符：`CTextEntry::OnKeyTyped` 本就忽略控制字符、退格/回车/方向键走按键事件、且 `IsESC()` 把 `m_nData==27` 当 Esc 会重复触发）→ 构造 `IE_KeyTyped`（`m_nData`=字符、`m_nData2`=修饰键）→ `PanoramaHandleInputEvent()`，UI 不吃就 `CallWindowProc` 交回 VGUI/控制台。链路其余部分都已在位：`ProcessUserInput` 有开发期的 `bHandleEvent = true` 兜底、`CTopLevelWindowSource2::HandleInputEvent` 有 `IE_KeyTyped` 分支、`CUIInputEngine` 把 `k_eKeyChar` 交给焦点面板的 `OnKeyTyped`。**实机结论（09-19）：照这条加了 `WM_CHAR → IE_KeyTyped` 桥后仍然打不进字 ⇒ 别只盯这一处，继续查 ① 焦点是否真的落到 `CTextEntry`（`m_ActionFocus.m_pFocus`）② `CTopLevelWindowSource2::HandleKeyboardInputMessage` 是否被调用 ③ `CTextEntry::OnKeyTyped` 是否被调。该改动留在工作区、**未提交**。** |
| 库存页剩余缺口 | ⬜ | ① `CSGOQuickInventory`（`quickinventory.xml`）未移植；② `ItemImage` 已换成移植的 `CItemImagePanel`（`EconItemImage` 只在 `#if !defined( CSTRIKE15 )` 分支里被引用，本仓编译不到）；③ 搜索结果面板走 JS 侧扁平表（`SetInventorySortAndFilters`/`GetInventoryCount`），C++ 的 `szFilterString`/`szSubStringFilter` 在 faux 层是 no-op（C++ 拿不到本地化名字）；④ 装备状态/洗牌（shuffle）全是空答案（无 GC、无 loadout 状态）。 |

## 6. 收尾 / 交付（Phase 5）⬜

| 任务 | 状态 | 说明 |
|---|---|---|
| 清除全部临时探针（`SE_PORT_BLUR*` / `SE_PORT_BLURSTR` / `SE_PORT_BLURDATA` / `SE_PORT_BLURPUSH` 等） | ⬜ | 分别在 `source2surface.cpp` / `uianimationengine.cpp` / `styles.cpp`。⚠️ `SE_PORT_BLUR` 那行现在还带 09-19 为验证开关加的 `seBlur=` / `gate=` 字段，一并清；**`se_blur` 这个 ConVar 本身保留**（功能开关，不是探针） |
| 回滚临时诊断（`@panorama_disable_blur` 默认值改回 `0`） | ⬜ | 09-16 A/B 用 |
| 还原 `.vscode/tasks.json`（任务运行器会改它） | ⬜ | 提交前 |
| 把仓库里的 mod 内容铺到游戏目录 | ⬜ | 见 §7 |
| 更新测试包 `D:\se-panorama-port.zip`（260 MB / 122.6 MB zip） | ⬜ | 用 `build\_make_package.ps1` |

## 7. 内容与素材从哪来（交付相关）

| 素材 | 来源 | 去处 | 是否入 git |
|---|---|---|---|
| panorama layout/scripts/styles | CS:GO 内容 | `<mod>\panorama\` | ✅（`mods/panorama_test`） |
| 移植自有本地化 `se_port_{english,schinese}.txt` | **本仓库自己写**（不是 CS:GO 内容） | `<mod>\panorama\localization\` | ✅（`mods/panorama_test/panorama/localization/`，用 `build\_deploy_se_js.ps1` 铺） |
| 字体包 33 个 `.vfont`（74.6 MB） | `csgo legacy\csgo\panorama\fonts`（= `D:\se\panorama\fonts`） | `<mod>\panorama\fonts\` | ❌ 用 `build\_stage_fonts.ps1` 铺 |
| 背景视频 `panorama\videos`（181 MB） | CS:GO 内容 | 同上 | ❌ |
| 图片素材 `materials\panorama\images`（2076 文件 / 557 MB） | CS:GO 内容 | `<mod>\materials\panorama\images` | ❌ |
| 本地化 `resource\csgo_*.txt` | CS:GO 内容 | `<mod>\resource\` | ❌ |
| 测试包 | `build\_make_package.ps1` | `D:\se-panorama-port.zip` | ❌ |

## 8. 下一步建议（按优先级）

1. ✅ **模糊已解挂（2026-09-19）**：`SE_PortSupportsBlurPasses()` 现在读 cfg 开关 **`se_blur`**（默认 `1` = 开），
   09-17 那组“挂起”结论（`pitfalls §I` 的 P63–P65）**已作废**，完整记录见 `csgo_panorama_port_breakthroughs.md` **T2**。
   重开模糊后**泛白不再出现**——T1 的 sRGB 修复才是真根因（P63 当年把泛白归给“缺少压暗层”只对了一半）。
   遗留：`SE_PORT_BLUR*` 探针（含 09-19 加的 `seBlur=` / `gate=` 字段）要清；`se_blur` ConVar 保留。
2. ✅ **泛白已破案并修复**（2026-09-19，与压暗层无关）：真因 = panorama shader 绑纹理丢了 `TEXTURE_BINDFLAGS_SRGBREAD`，
   纹理按 sRGB 原值进"线性混合+输出编码"管线被多提亮一次 gamma。修复 = 两个 shader SHADOW_STATE 补 `EnableSRGBRead`（stdshader_dx9）。
   实测：灰阶 17→73 修到逐条精确，整窗 mean 203.6→96.4。压暗层其实一直在画且数学正确。详见 T1/P101-P104。
3. ⬜ 清探针 + 回滚临时诊断（`se_port_opaque_blend`、`FANCYQUAD`、DRAW 探针、`SE audio`/`MF sync`）+ 还原 `tasks.json`，然后提交一次「收尾」。
4. ⬜ 补齐面板类型与 JS API → 设置页/统计页能看。
5. ⬜ 主菜单渲染整体验收 → 更新测试包 → 上第二台机器测。
6. ⬜ （可选）方案 B：panorama 接管 GameUI；字体 D `.uifont`。

## 9. 待办 backlog（思路已定，未开工）

| 项 | 状态 | 思路/结论存档 | 估算 |
|---|---|---|---|
| **背景模糊（blurrects / CSGOBlurTarget）不生效** | ✅ **已解挂（2026-09-19）** | 不再复现（T1 的 sRGB 修复 + YUV 平面 A8 之后），已恢复开启并做成 cfg 开关 `se_blur`（见 T2）。09-17 的挂起记录（P63–P65）保留作历史，**别再按它去查** | —— |
| **主菜单泛白** | ✅ 已修复（09-19） | 根因/修复/验收见 `csgo_panorama_port_breakthroughs.md` T1（SRGBREAD 丢失；mean 203.6→96.4）。视频假色也已修（A8 方案，P104） | —— |
| **Panorama 模型查看器**（查看游戏自带 `.mdl`，不要 CS:GO 皮肤） | ⬜ 未开工 | **`docs/panorama_model_viewer_plan.md`**（含关键事实：`IVModelRender` 在 `engine/l_studio.cpp:812`、`CStaticProp` 是现成模板、`csgo_backbufferimage.cpp` 是面板先例、光照风险与对策、两个待拍板点） | ≈ 1 周 |
| **库存（Inventory）** | ⬜ 未开工 | 结论：**界面可行、真数据不可行**。物品定义 `items_game.txt` 在本机 `E:\SteamLibrary\steamapps\common\csgo legacy\csgo\scripts\items\`（6.79 MB）；econ 源码在 CS:GO `game/shared/econ/`（43 文件，本仓只有 `ihasowner.h`）；`public/gcsdk/gcclient/` 不存在 + 无 Steam 登录 ⇒ 只能"真 schema + 假库存"；3D 检视 `ui_itempreview_panel.cpp` 6861 行属独立大工程 | ≈ 1.5–2 周 |
| 补 JS 数据 API 后端（`InventoryAPI`/`LoadoutAPI`/…31 个） | ⬜ 未开工 | 这 31 个在 CS:GO 源码树 **0 命中**（Valve 闭源 game client），**不可照抄**，只能按内容实际调用面自建 | —— |
| 清探针（`SE_PORT_BLUR*` / `BLURSTR` / `BLURDATA` / `BLURPUSH`、`source2surface.cpp` / `uipanel.cpp` / `mf_video_player.cpp` 里的临时输出） | ⬜ | 见 §6 | —— |
