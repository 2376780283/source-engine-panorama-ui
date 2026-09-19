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
