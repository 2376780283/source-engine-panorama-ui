# CS:GO Panorama 移植 — 未解决的 Bug（原因未找到）

> 记录**已充分调查、但根因仍未确定**的问题。每条包含：现象、反向证据、已排除项、当前嫌疑、下一步。
> 已有的"已解决坑"见 `csgo_panorama_port_pitfalls.md`。

---

## U1. 主菜单悬停无视觉反馈 —— 工具提示不显示 / 图标悬停不变色（2026-09-18，未解决）

### 现象（用户实测）
- 主菜单下悬停左侧导航图标（主面板 / 开始游戏 / 物品栏 / 观战 / 统计 / 监管 / 设置…）：
  - **图标不变色**（既不变深也不变亮）；
  - **不出现工具提示黑框**；
- 多次**干净单实例**测试（含 02:01 的 44 秒运行）均如此；用户判定"没有任何变化"。

### 反向证据（开发端自动化，`-windowed` 窗口模式，1920x1080）
- **工具提示实际渲染过**：
  - 截图 `build/_ttip12_y90.png`（悬停"主面板"）中黑框清晰可见（放大图 `_crop_y90_topleft.png`）；
  - 像素证据（`_profilediff.txt`）：y90 的 tooltip 区域 (108-205, 58-135) 平均亮度 **139**（对照 208+）；y340 的物品栏 tooltip 区域 **149**（对照 183+）。
- **图标悬停/选中变色可见**：物品栏（条带 y360-376 亮度 116 vs 186）、观战（y464-480，110 vs 182）——对应 `mainmenu.css` 的 `wash-color: rgb(24,24,24)`（白→深灰）。
- **悬停探测全通**：`SE_PORT_HITTEST` 命中正确图标（如 `(41,172)->MainMenuNavBarPlay`）；tooltip 链路完整（`ShowTextTooltip → EnsureTooltip → SetTooltipVisible 1 → desired 99x51/111x51/141x51/201x51/276x51 → OnLayoutTraverse → Hide`），数量级正常。

### 已排除
| 假设 | 排除依据 |
|---|---|
| 输入未到达主菜单 | 命中测试命中图标、hover 事件、JS `onmouseover` 全部触发 |
| 输入坐标错位 | 命中面板与鼠标悬停位置一致（用户坐标与 UI 坐标自洽） |
| 样式/内容缺失 | `code.pbin`（真正生效的内容包）内 `mainmenu.css` 存在 hover 规则；tooltip 样式 `tooltips/tooltip_text.css`、`tooltip_base.css` 完整（`.TooltipContainer{opacity:0}` + `.TooltipVisible{opacity:1}`） |
| JS 异常 | JSEXC = 0 |
| 渲染停摆 | Frame 探针：fps 130+；视频解码/呈现正常（MFFRAME 30fps 全速） |
| 双实例/窗口抢输入 | 已定位并净化过（详见下一节"环境事故"）；用户 02:01 干净单实例复测仍无变化 |
| 分辨率 / DPI | 同机同参数（1920x1080 @125%）复现不出差异 |

### 当前最大嫌疑（区分度最高的差异）
1. **开发端能看到效果的观察全部在窗口模式（`-windowed`）；用户运行是默认模式（无 `-windowed`，即全屏）**（D:\cstrike 无 video.txt；实测同参数启动为全屏 1920x1080 @ 0,0）。
2. **全屏下程序化 `SetCursorPos` 不进入游戏输入流**（全屏 + RAW INPUT 模式不接收合成 WM_MOUSEMOVE）：
   - 全屏实验（`_fs_probe.ps1`）中悬停主面板/开始游戏，**tooltip 探针零输出**（`se_tooltip_probe.txt` 未生成）——与用户全屏下"探针有输出"（真实鼠标）不同；
   - 因此**尚无法在全屏下用自动化验证"逻辑触发但视觉不可见"的状态**。
3. 嫌疑方向（待验证）：
   - 全屏渲染/上屏路径中，hover 样式变更与 tooltip 的**重绘未上屏**；
   - 或全屏下 tooltip 的 `opacity` 过渡（175ms）不推进，面板停在 `opacity: 0`。

### 下一步调查
1. 用 `SendInput`（**相对**鼠标移动，可进入 RAW INPUT 流）在全屏下注入悬停，复现"逻辑触发但视觉不可见"；
2. 在 tooltip 面板加**绘制探针**（记录 `CTooltip` 的 Paint 调用、最终 opacity），全屏/窗口两模式对比；
3. 对比全屏/窗口两种模式下 panorama surface 的更新统计（重绘次数/上屏帧）。

### 环境事故（已处理的干扰源，供后续测试时防范）
- 2026-09-18 01:44：开发端自动化实例与用户实例**同时运行**，窗口重叠、输入被前台实例抢走：出现"用户悬停无反应、而探针记录来自另一实例"的混乱（engine.log 中混入人类鼠标轨迹坐标；tooltip 探针出现脚本从未悬停的图标）。
- 规矩：**任何自动化游戏运行前先 `Get-Process hl2_launcher | Stop-Process -Force` 并确认；绝不在用户实测期间跑自动化。**

### 证据文件索引
- 截图：`build/_ttip12_00content.png`、`_ttip12_y90/y190/y340/y440/y540.png`、`_crop_y90_topleft.png`、`_fs_home.png`、`_fs_play.png`
- 数值：`build/_profilediff.txt`、`build/_profiles.txt`、`build/_pbin_mm.txt`
- probe：`D:\cstrike\se_tooltip_probe.txt`、`D:\cstrike\engine.log`（SE_PORT_HITTEST）
- 脚本：`build/_fs_probe.ps1`（全屏复现实验）、`build/_profiles.ps1`、`build/_profilediff.ps1`、`build/_crop90.ps1`、`build/_pbin_mm.ps1`

---

## U2. [低优先级] Legacy 弹窗偶发不弹 —— 去重标记"抢得太早"+ 包装链被挡（2026-09-18，已定位未修）

### 现象（用户实测）
- 开关都是开（engine.log：`SE port: popup switches se_popup_news='1' se_popup_legacy='1'`），但某一轮启动里 **"Legacy version of CS:GO" 弹窗从不出现**（用户报"Legacy 弹窗不弹"）；早先一轮（10:25）同一份代码则正常弹一次。
- 伴随日志：`弹窗: 重复的 Legacy 弹窗已拦截` 每 ~0.25 秒一次刷到会话结束；`弹窗栈[MainMenu]: 可见=1/3` 里那 1 个是同时刻的**新闻**弹窗；全程**没有** `弹窗: Legacy 弹窗已弹出*` 成功日志。

### 关键证据（`D:\cstrike\engine.log`，2026-09-18 10:54 那轮）
```
[1.3317] SE port: popup switches se_popup_news='1' se_popup_legacy='1'
[2.3255] 弹窗: 重复的 Legacy 弹窗已拦截
[2.3257] 弹窗: Legacy 弹窗创建返回空(管理器未就绪), 开始重试
[2.3265] panorama: loaded file://{resources}/layout/base_mainmenu.xml    ← 弹窗管理器在这之后才存在
... 之后每 0.25 秒一条 "已拦截" 直到底 ...（无任何 "已弹出" 成功日志）
[5.3519] 弹窗: 关闭按钮点击                                          ← 用户关掉的是新闻弹窗
```
（`config.cfg` 里另有 `se_legacy_popup_shown "1"` —— 去重标记是 FCVAR_ARCHIVE 偏好，被一并存盘；但它在启动时不会被载回，见下面第 1 点。）

### 根因（已定位）
`se_session_sim.js::dedupeLegacyPopup` 的问题有二：
1. **标记写得比"创建成功"早**：owner 在调用真正的 `ShowGenericPopupTwoOptions` **之前**就写 `se_legacy_popup_shown="1"`。而最早的那次调用发生在 `base_mainmenu.xml` 加载**之前**（多上下文注册的处理器之一在 2.32s 就跑了 `_ShowLegacyVersionWarning`）——那时 CUI_Root 还没把弹窗管理器注册到窗口上，调用**静默返回空、弹窗根本没创建**。标记已写死 ⇒ 之后再也没人能"当 owner"。
2. **包装链**：`dedupeLegacyPopup` 在每个上下文里各包一层（`UiToolkitAPI` 若是跨上下文共享对象，后装的 `orig` 捕获到的其实是别人包的 hook）。空返回后的 0.25s 重试虽然直接调 `orig`，但调到的可能是链上别的 hook —— 被 `seen==="1"` 挡下并打"已拦截"（这正好解释了那串每 0.25s 一次的日志，以及为何重试永远拿不到弹窗）。

### 修法（下次做，几行改动，未实测）
1. **标记只在创建成功后写**（或"先写、空返回就回滚成 ''再重试"——后者还能顺带防同帧双弹）：
   `var r = orig(...); if (!r) { SetSettingString(marker,""); 重试 } else { SetSettingString(marker,"1"); log("已弹出") }`。
2. **包装层幂等**：`if (g.UiToolkitAPI.__seLegacyHooked) return; g.UiToolkitAPI.__seLegacyHooked = true;` —— 避免多层包装链。
3. （可选）只在根面板 id 为 `'MainMenu'` 的上下文里接管，排除布局加载前的过早触发。

### 验收判据
- engine.log 里 `弹窗: Legacy 弹窗已弹出` 恰好一次；`弹窗栈[MainMenu]: 可见` 出现对应的一次 1；点"确定"后回 0。
- 关掉开关（`se_popup_legacy 0`）时只见一次 `弹窗: Legacy 弹窗已被 se_popup_legacy=0 关闭`、无"已拦截"刷屏。

### 优先级
- 用户 2026-09-18 判定：**不重要**（开关能关、弹不弹不影响功能），已存档等后续顺手修。

---

## U3. 控制台面板不显示 —— 只画出一条 ~55px 的窄条（2026-09-19，未解决）

### 现象（用户实测）
- 按 `` ` ``（反引号）打开控制台：**功能全对** —— 能看到版本号、Tab 命令补全能用、命令能执行；**就是看不到控制台面板**。
- 主菜单里和进地图后都是这样。

### 反向证据（2026-09-19 自动化：`build/_ingame_console_test.ps1` = `+map de_dust2 +toggleconsole` + 定时截图 + 读探针）

1. **控制台确实被创建、被激活、且可见**（`D:\cstrike\se_console_probe.txt`）：
   ```
   Activate: init=1 visible=0 bounds=(544,24,712,528) parent=0 embedded=...
   CConsoleDialog::Activate visible=1
   CConsoleDialog::PerformLayout client=(8,27,696,495) visible=1 parent=0
   ```
2. **绘制时刻几何完全正确，而且每帧都在画**（2026-09-19 新加的探针，`CGameConsoleDialog::Paint()`）：
   ```
   CGameConsoleDialog::Paint #22 abs=(544,24) size=(712,528) clip=(544,24,1256,552) screen=1280x720 prop=0 vis=1 parent=0
   ...（连续 #22..#40，每帧一条）
   ```
   ⇒ 面板自己认为的位置 / 尺寸 / 裁剪都对，`Paint()` 也真的被调用了。
3. **但标称矩形里没有它的像素**（`engine/view.cpp` 的 A/B/C 回读探针，控制台可见时在地图分支采 (900,120) 与 (5,5)）：
   ```
   SE console probe A (after View_Render):  rt=NULL in=123,132,132,255 out=132,132,132,255
   SE console probe B (after panorama):    in=123,132,132,255
   SE console probe C (after vgui repaint): in=123,132,132,255
   ```
   A/B/C 三点同值（≈世界画面）⇒ 控制台渲染前后，(900,120) 没有任何变化。
4. **却在客户端左侧出现一条窄条**（截图 `build/_ingame_t30.png`，放大 `build/_svg_zoom.png`）：
   x ≈ 0..55、y ≈ 330..460 一条暗色带，内容是控制台的东西（`+` / `∨` 一行 + 高亮的 `SE cons…` +
   两行带小图标的 `powers…`），文字被右侧**硬裁**。

### 已排除
| 假设 | 排除依据 |
|---|---|
| 控制台没被创建 / 用的是 gameui.dll 那个 | `engine.log`: `SE port: IGameConsole provided by panoramauiclient.dll`；`se_console_probe.txt` 有 `Factory`/`Initialize` |
| 没被激活 / `visible=0` | `CConsoleDialog::Activate visible=1`、`Paint()` 探针 `vis=1` |
| 父面板被隐藏 ⇒ 链上 `IsFullyVisible()=0` | 父为 0（**顶层 popup，引擎故意不设父**，CS:GO 式；这条已经修掉了，见 `vgui_baseui_interface.cpp::Init()` 的 `#ifdef PANORAMA_ENABLE` 分支） |
| 它不是 popup / 不在 popup 列表里 | `Paint()` 每帧被调 ⇒ popup 循环走得到它 |
| 几何 / 裁剪算错 | `Paint()` 探针：`abs` / `size` / `clip` 全对 |
| 输入没到控制台 | 用户实测：补全、执行都正常 |
| 泛白 / sRGB 同族问题 | 无关：sRGB 修复前后都是这个现象 |
| 主菜单 vs 地图差异 | 两种场景现象一致 |

### 当前最大嫌疑（按可能性排序）
1. **popup 绘制被 stencil 掩掉**：`CMatSystemSurface::PaintTraverseEx` 的 popup 循环对每个 popup 做
   `pRenderContext->SetStencilReferenceValue( bIsTopmostPopup ? 255 : nStencilRef )` ⇒ popup 是**带 stencil 测试**画的。
   若 panorama 的合成/裁剪把 stencil 留在某个小矩形上，popup（控制台）就只在那块矩形里出像素；
   而普通面板（HUD / 雷达 / 聊天）不走 stencil ⇒ 正好解释"其它 VGUI 正常、只有这个 popup 被裁成一条"。
2. **scissor 残留**：本 port 的 scissor 是降级实现（单矩形 `SetScissorRect(l,t,r,b,bool)`，**没有栈**），
   panorama 画过的矩形可能留下一个窄 scissor。
3. 画到了非当前 RT / 视口不同（可能性较低：clip 正确、且每帧都在画）。

### 下一步（接手就从这里开始）
1. **红块定位法**：在 `CGameConsoleDialog::Paint()` 开头（`BaseClass::Paint()` 之前）画一个满面板的红矩形：
   `surface()->DrawSetColor(255,0,0,255); surface()->DrawFilledRect(0,0,712,528);`
   - 红色出现在 (544,24) ⇒ 绘制链没问题，是**被谁盖住 / 掩掉**；
   - 红色也只出现在左侧窄条 ⇒ **掩码类**问题（stencil / scissor），直接做第 2 步。
2. **stencil A/B**：在 `PaintTraverseEx` 的 popup 循环里，`ipanel()->PaintTraverse(popupPanel, true)` 之前加
   `pRenderContext->SetStencilEnable( false );`（或 `SetStencilReferenceValue(0)`），看控制台是否立刻出现。
3. 打印 popup 画完那一刻的 **scissor / viewport**（`IMatRenderContext`），并与 panorama 最后一次 draw 的状态比对；
   顺便确认 panorama 收尾有没有把 scissor / stencil 恢复。

### 本轮为调查加的临时设施（收尾要清）
- `panorama/seport/gameclient/cstrike15/gameui/gameconsoledialog.{h,cpp}`：`CGameConsoleDialog::Paint()` 几何探针（上限 40 条）
- `panorama/seport/gameclient/cstrike15/gameui/gameconsole.cpp`：**红色背景 hack**（`SetBgColor(200,30,30,255)`，用来区分
  "画了但没颜色" vs "根本没画"）+ `Initialize`/`Activate`/`IsConsoleVisible` 探针
- `engine/view.cpp`：A/B/C 像素回读探针（只在 `Con_IsVisible()` 时打）
- `vgui2/vgui_controls/consoledialog.cpp`：`Activate` / `PerformLayout` / `OnThink` 探针
- `vguimatsurface/MatSystemSurface.cpp`：`SE popup pass` 探针（popup 列表 + 受限面板）
- 脚本：`build/_ingame_console_test.ps1`（本次复现）、`build/_shotbox.ps1`（截图布局测量）、`build/_svg_zoom.ps1`（放大裁剪）

### 测量口径提醒（本轮踩到）
- 截图会被 **DWM 缩放**：`_ingame_t30.png` 实际是 **1029x604**（对应 1280x720 客户区，缩放 ≈0.8）。
  量像素 / 换算坐标前先看图的真实尺寸，别按 1280x720 直接算。
- 同一张图里，客户区之外还有未绘制的区域（左侧黑带 / 右侧发白）——那是窗口比渲染区大或缩放的产物，
  不一定是 bug。

### 优先级
- 待用户定。功能可用（能输入、能执行、有补全），只是面板看不到 ⇒ 观感问题，不挡其它工作。

### 证据文件索引
- 截图：`build/_ingame_t30.png`（整窗）、`build/_svg_zoom.png`（左侧窄条放大 ×6 / ×10）
- 数值：`build/_ingame_console_test.txt`（运行报告）、`build/_logcheck.txt`（`SE console probe` 扫描 = 978 条）
- probe：`D:\cstrike\se_console_probe.txt`、`D:\cstrike\se_ui_probe.txt`（`SE console probe A/B/C`）、
  `D:\cstrike\se_popup_probe.txt`（`SE popup pass`）
