# CS:GO Panorama（csgoui）移植 —— 任务清单（从开始到现在）

> 配套文档：
> - `csgo_panorama_port_plan.md` —— 可行性分析 + 分阶段计划（Phase 0~5）
> - `CSGO_PANORAMA_PORT_NOTES.md` —— 实操手法 / 正确命令 / 速查
> - `csgo_panorama_port_pitfalls.md` —— **踩坑表**
> - `csgo_panorama_port_breakthroughs.md` —— **重大突破**
>
> 图例：✅ 已完成　🔶 进行中　⬜ 未开始　🅿️ 已打包/已归档
> 分支：`CSGO-Panorama`（仓库共 621 提交，移植线从 `391effac` 起）
> 更新：2026-09-16

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
| 模糊结果偏亮 + 左侧一块平灰 | 🅿️ **挂起（09-17）** | 已排除：算法 / shader / `mix-blend-mode` / `mipmapgaussian` / scratch RT 尺寸 / sampler 绑定 / blurrects 查找。已**关掉模糊**（`SE_PortSupportsBlurPasses()` → `false`）并归档为“暂时查不出的 bug”，见 pitfalls §I（P63–P65） |
| 关掉模糊后的实测（09-17） | ✅ | 静态背景：布局完整但**仍泛白**（mean 203.6 / >200 像素 65.6%）；开背景视频：背景全黑（视频只在 blur pass 那条路径上被画出来）。脚本 `build/_bluroff_verify3.ps1`，截图 `build/_bluroff_nomovie.png` / `build/_bluroff.png` |
| **移植玩家头像面板 `CSGOAvatarImage`（脱离 Steam，改读本地文件）** | ✅（09-17） | `panorama/seport/gameclient/cstrike15/panorama/csgo_avatarimage.{h,cpp}`（新文件），删掉原 `seport_panel_stubs.cpp` 里的同名桩；图像按 `<steamid64>` → `<accountid>` → `local` 找 `materials/panorama/images/avatars/*.png|jpg|svg`。实机：文件被正确读取并加载（`SE_AVPROBE: image loaded ...`）。见 P67/P68 |
| **本地玩家卡片可见（右上角头像因此能显示）** | ✅（09-17） | 真因不是渲染而是桩数据：`LobbyAPI.IsSessionActive()` truthy ⇒ `party.js` 以为在队伍里 ⇒ `HideLocalPlayer(true)` 把 `JsLocalPlayercard` 整张塌掉（P66）。shim 改答 `false` 等 7 项后卡片回归 |
| 清掉 avatar 探针（`SE_AVPROBE` 三处，含用户加的 per-candidate 行） | ⬜ | `panorama/seport/gameclient/cstrike15/panorama/csgo_avatarimage.cpp`；头像确认正常后删 |
| 清掉部署内容里的死 hack（`mainmenu.css:1232/1243` 两处） | ⬜ | 见 P50/P61；留着会误导 |
| 补齐 8 个缺失面板类型（`CSGOSettingsSlider`/`CSGOAudioSettings`/`CSGOVideoSettings`/`CSGOStatsProgressGraph`/`MapSpiderGraph`/`WeaponSpiderGraph`/`HeatMap` 等） | ⬜ | 设置页/统计页因此为空 |
| 补 JS API 缺口（`RemoveAllOptions` 等） | ⬜ | 日志报 `settingsmenu_gamesettings.js:8` |
| 补完本地化 token（`#CSGO_Tournament_Event_Location_[]` 等） | ⬜ | 日志 `Unable to localize` |
| 移植 batch E 剩余 gameclient 文件 | ⬜ | —— |
| 主菜单渲染整体验收（泛白单独处理完） | ✅（泛白部分，09-19） | 根因=shader 丢 SRGBREAD，修复后 mean 203.6→96.4，灰阶逐条精确；见 `csgo_panorama_port_breakthroughs.md` T1。视频假色也已修（YUV 平面 I8→A8 + fxc 读 .a，09-19 第二阶段，P104/T1） |
| 方案 B：`panoramauiclient` 实现 `IGameUI`，panorama 正式接管 GameUI | ⬜ | 可回退任务 A 的三处守卫 |
| 字体 D（可选）：`.uifont` 包（protobuf + OpenSSL AES） | ⬜ | 需拍板；CS:GO 自身内容不用它 |

## 6. 收尾 / 交付（Phase 5）⬜

| 任务 | 状态 | 说明 |
|---|---|---|
| 清除全部临时探针（`SE_PORT_BLUR*` / `SE_PORT_BLURSTR` / `SE_PORT_BLURDATA` / `SE_PORT_BLURPUSH` 等） | ⬜ | 分别在 `source2surface.cpp` / `uianimationengine.cpp` / `styles.cpp` |
| 回滚临时诊断（`@panorama_disable_blur` 默认值改回 `0`） | ⬜ | 09-16 A/B 用 |
| 还原 `.vscode/tasks.json`（任务运行器会改它） | ⬜ | 提交前 |
| 把仓库里的 mod 内容铺到游戏目录 | ⬜ | 见 §7 |
| 更新测试包 `D:\se-panorama-port.zip`（260 MB / 122.6 MB zip） | ⬜ | 用 `build\_make_package.ps1` |

## 7. 内容与素材从哪来（交付相关）

| 素材 | 来源 | 去处 | 是否入 git |
|---|---|---|---|
| panorama layout/scripts/styles | CS:GO 内容 | `<mod>\panorama\` | ✅（`mods/panorama_test`） |
| 字体包 33 个 `.vfont`（74.6 MB） | `csgo legacy\csgo\panorama\fonts`（= `D:\se\panorama\fonts`） | `<mod>\panorama\fonts\` | ❌ 用 `build\_stage_fonts.ps1` 铺 |
| 背景视频 `panorama\videos`（181 MB） | CS:GO 内容 | 同上 | ❌ |
| 图片素材 `materials\panorama\images`（2076 文件 / 557 MB） | CS:GO 内容 | `<mod>\materials\panorama\images` | ❌ |
| 本地化 `resource\csgo_*.txt` | CS:GO 内容 | `<mod>\resource\` | ❌ |
| 测试包 | `build\_make_package.ps1` | `D:\se-panorama-port.zip` | ❌ |

## 8. 下一步建议（按优先级）

1. ⬜ **把结论记牢：模糊已“关掉并挂起”**（`SE_PortSupportsBlurPasses()` 返回 `false`，2026-09-17）——
   “背景模糊为什么不生效” 归入 **暂时查不出的 bug**，细节/已排除项/遗留疑点在 `docs/csgo_panorama_port_pitfalls.md` §I（P63–P65）。
   关掉后实测：静态背景的主菜单**仍然泛白**（mean 203.6），开背景视频则背景全黑 ⇒ 泛白另有根因（背景亮 → 没有压暗），不是模糊。
   下次开工：把那个函数改回 `true`，先抓 “进 sampler 的究竟是不是 error texture / 尺寸多少”，别再重读算法。
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
| **背景模糊（blurrects / CSGOBlurTarget）不生效** | 🅿️ **挂起（2026-09-17 用户拍板）** | 已关掉（`SE_PortSupportsBlurPasses()` → `false`，标注 PARKED）；**根因未找到**。完整记录 + 已排除清单 + 下次接手点见 `csgo_panorama_port_pitfalls.md` §I（P63–P65）。重新开工：先把开关改回 `true`，然后抓 sampler 里的纹理身份/尺寸 | —— |
| **主菜单泛白** | ✅ 已修复（09-19） | 根因/修复/验收见 `csgo_panorama_port_breakthroughs.md` T1（SRGBREAD 丢失；mean 203.6→96.4）。视频假色也已修（A8 方案，P104） | —— |
| **Panorama 模型查看器**（查看游戏自带 `.mdl`，不要 CS:GO 皮肤） | ⬜ 未开工 | **`docs/panorama_model_viewer_plan.md`**（含关键事实：`IVModelRender` 在 `engine/l_studio.cpp:812`、`CStaticProp` 是现成模板、`csgo_backbufferimage.cpp` 是面板先例、光照风险与对策、两个待拍板点） | ≈ 1 周 |
| **库存（Inventory）** | ⬜ 未开工 | 结论：**界面可行、真数据不可行**。物品定义 `items_game.txt` 在本机 `E:\SteamLibrary\steamapps\common\csgo legacy\csgo\scripts\items\`（6.79 MB）；econ 源码在 CS:GO `game/shared/econ/`（43 文件，本仓只有 `ihasowner.h`）；`public/gcsdk/gcclient/` 不存在 + 无 Steam 登录 ⇒ 只能"真 schema + 假库存"；3D 检视 `ui_itempreview_panel.cpp` 6861 行属独立大工程 | ≈ 1.5–2 周 |
| 补 JS 数据 API 后端（`InventoryAPI`/`LoadoutAPI`/…31 个） | ⬜ 未开工 | 这 31 个在 CS:GO 源码树 **0 命中**（Valve 闭源 game client），**不可照抄**，只能按内容实际调用面自建 | —— |
| 清探针（`SE_PORT_BLUR*` / `BLURSTR` / `BLURDATA` / `BLURPUSH`、`source2surface.cpp` / `uipanel.cpp` / `mf_video_player.cpp` 里的临时输出） | ⬜ | 见 §6 | —— |
