# 阶段 2：tooltip / popup / contextmenu 分批移植清单（供审阅，尚未开工）

目标：把 CS:GO 的这三套 UI 基础设施真正跑起来（目前布局里的
`CSGOTooltipManager` / `CSGOPopupManager` / `ContextMenuManager` / `TooltipPanel`
四个类型都被降级成普通 `Panel`，`UiToolkitAPI` 是 JS 空壳，所以悬停提示、弹窗、
右键菜单全都不存在）。

已确认的有利条件（本仓已就位，不需要动）：

| 依赖 | 位置 |
|---|---|
| `CPanel2D` / `IUIPanel` / `IUIWindow` | `public/panorama/**` + `panorama/controls/panel2d.cpp` 等 |
| `CTooltip`（框架 tooltip 基类） | `public/panorama/controls/tooltip.h` + `panorama/controls/tooltip.cpp` |
| `CContextMenu` / `CSimpleContextMenu` | `public/panorama/controls/contextmenu.h` + `.cpp` |
| `panorama/uisettings.h` | `public/panorama/uisettings.h` |
| `panorama/uiinputcapture.h` | `public/panorama/uiinputcapture.h` |
| `IGameUIFuncs.h` | `public/IGameUIFuncs.h` |
| `uijsregistration.h`（`REGISTER_PANEL2D_FACTORY` 等） | `public/panorama/uijsregistration.h` |
| `CRenderPanel` / `CRenderThreadCallback` | 阶段 1 已启用（`controls/source2/renderpanel.cpp` 入编译） |
| `CGameEventListener` | `game/shared/GameEventListener.h` |

## 批次划分

### 批次 A —— 宿主层（其余三批都挂在这上面）
| 文件 | 行数 |
|---|---|
| `ui_symbols.h/.cpp` | 17 + 18 |
| `ui_custom_layout.h/.cpp` | 35 + 124 |
| `ui_js_panel.h/.cpp` | 40 + 38 |
| `ui_root.h/.cpp`（`CUI_Root`，三个管理器的宿主 + `GetRootForWindow`） | 56 + 265 |

- 依赖：全部已就位；`ui_root.cpp` 里的 Dota 代码整段在 `#if DOTA_DLL` 内（本移植不定义该宏即可），
  另有 `#if defined( CSTRIKE15 )` / `#if CSTRIKE_TRUNK_BUILD` 分支需要选边（见“需要你拍板”）。
- 验收：布局里出现 `UIRoot` 类型；`CUI_Root::GetRootForWindow()` 可用（届时阶段 1 的
  `CCSGO_BlurTarget` 里那个“爬父节点找 root”的替身可以换回原写法）。

### 批次 B —— tooltip（悬停提示，直接修掉“鼠标放上去没反应”）
| 文件 | 行数 |
|---|---|
| `ui_tooltip_manager.h/.cpp`（`CUI_TooltipManager` + 全部 `UIShow*Tooltip` 事件） | 81 + 209 |
| `tooltips/ui_tooltip_base.h/.cpp`（`CUI_Tooltip_Base : CTooltip`） | 74 + 88 |
| `tooltips/ui_tooltip_text.h/.cpp` | 21 + 31 |
| `tooltips/ui_tooltip_title_text.h/.cpp` | 24 + 42 |
| `tooltips/ui_tooltip_title_image_text.h/.cpp` | 26 + 47 |
| `tooltips/ui_tooltip_custom_layout.h/.cpp` | 21 + 34 |
| cstrike15：`csgo_tooltippanel.h/.cpp` + `csgo_ui_tooltip_manager.h/.cpp` | 25+138 / 16+30 |

- 验收：`mainmenu.xml` 导航图标的 `onmouseover="UiToolkitAPI.ShowTextTooltip(...)"` 能弹出文字
  （前提是批次 E 也做完，否则改由 `se_api_shim.js` 里临时挂一个 C++ 桥来验证）。
- 风险：低。tooltip 内容面板是纯 UI，不依赖 Steam/饰品数据。

### 批次 C —— popup（弹窗）
| 文件 | 行数 |
|---|---|
| `ui_popup_manager.h/.cpp` | 94 + 514 |
| `ui_popup.h/.cpp` | 81 + 171 |
| `popups/ui_popup_generic.h/.cpp` | 94 + 357 |
| `popups/ui_popup_generic_text_entry.h/.cpp` | 48 + 145 |
| `popups/ui_popup_custom_layout.h/.cpp` | 16 + 19 |
| `popups/ui_popup_generic_enums.h` | 13 |
| cstrike15：`csgo_popup_manager.h/.cpp` + `csgo_globalpopups.h/.cpp` | 17+32 / 19+56 |

- 依赖：`ui_popup.cpp` 用 `IGameUIFuncs.h`（已就位）；`CUI_PopupManager` 构造时会
  `BLoadLayout("file://{resources}/layout/popups/popup_manager.xml")` ⇒ **需确认内容侧部署了
  `layout/popups/*.xml`**（`popup_generic.xml` / `popup_manager.xml`），否则要一并补内容。
- 验收：`UiToolkitAPI.ShowGenericPopupOk(...)` 能弹出并有关闭回调；CS:S 的 VGUI 不参与。
- 风险：中。`ui_popup_manager.cpp` 是这三批里最大的单文件，且 CS:GO 用它做了"跨窗口只允许
  一个模态"的仲裁，可能与本移植的 VGUI 层有交互，需要实机确认。

### 批次 D —— context menu（右键菜单）
| 文件 | 行数 |
|---|---|
| `ui_context_menu_manager.h/.cpp` | 23 + 82 |
| `context_menus/ui_context_menu_base.h/.cpp` | 45 + 43 |
| `context_menus/ui_context_menu_custom_layout.h/.cpp` | 25 + 25 |

- 验收：右键导航图标/物品格弹出菜单；`DismissAllContextMenus` 行为与 CS:GO 一致。
- 风险：低。CS:GO 里 `ui_context_menu_manager.cpp` 的 Dota 专属事件整段在 `#if DOTA_DLL`。

### 批次 E —— JS 绑定（你已选“照搬 cstrike15 组件框架”）
| 文件 | 行数 |
|---|---|
| `cstrike15/uicomponents/uicomponent_common.h/.cpp`（`PANORAMA_COMPONENT_API_DEF_*` 宏体） | 777 + 280 |
| `cstrike15/uicomponents/uicomponent_uitoolkit.h/.cpp/.functions.inc`（`UiToolkitAPI` 全部绑定） | 120 + 763 + 122 |
| `cstrike15/panorama/csgo_panorama_script_bindings.h/.cpp`（组件注册入口） | 102 + 207 |
| 视需要：`cstrike15/panorama/csgo_dialog_variable_handlers.h/.cpp` | 66 + 238 |

- 依赖：`uicomponent_common.h` 里有 `#error "only in Client DLL"` 之类的守卫，需要按本移植的
  模块划分调整；组件宏体系（`UI_COMPONENT_API_DEF_COMMON_DOC` 等）需要接到 panorama 的
  JS 工厂上。
- 验收：`UiToolkitAPI.ShowTextTooltip / HideTextTooltip / ShowGenericPopupOk /
  ShowCustomLayoutPopupParameters / ShowContextMenu*` 全部由 C++ 实现，
  `se_api_shim.js` 里对应的空壳可以删掉。
- 风险：**这是整批里最不确定的一段**（宏体系 + 组件注册），建议单独排期、单独验证。

## 目录与 include 方案（需要你拍板）

CS:GO 源码里的写法是 `#include "panorama/ui_root.h"`、`#include "popups/ui_popup_generic.h"`
——它在 CS:GO 里靠 include 路径解析到 `game/client/panorama/`。本移植的
`panorama/` 前缀已经指向别处（框架私有头 + `public/panorama`），所以两种放法：

- **方案 1（推荐，源码可逐字照搬）**：文件按原目录结构放进
  `panorama/seport/gameclient/panorama/...`（即 `.../gameclient/panorama/ui_root.h`、
  `.../gameclient/panorama/popups/ui_popup_generic.h`），并把 `panorama/seport/gameclient`
  加进 include 路径 ⇒ 所有 `#include "panorama/xxx"` **一字不改**；
  cstrike15 侧同样放 `.../gameclient/cstrike15/panorama/...` 与 `.../cstrike15/uicomponents/...`。
  代价：目录名里出现嵌套的 `panorama/`，看着略怪。
- **方案 2**：平铺在 `panorama/seport/gameclient/`，把每处 include 改成 `gameclient/...`
  （约 40 处文本替换）。目录更整齐，但与 CS:GO 原文件不再逐字对应，后续对差别时更费劲。

## 需要你拍板的点（都是你列的“重大改动”类别）

1. **目录/include 方案**：方案 1 还是方案 2。
2. **是否定义 `CSTRIKE15` / `CSTRIKE_TRUNK_BUILD`**：`ui_root.cpp`、`ui_popup_manager.cpp` 等
   有按游戏分叉的分支，选边会影响行为（CS:GO 的 cstrike15 分支才是我们要的）。
3. **编译落点**：批次 A（宿主层）建议进 `panorama_client.lib`；批次 B/C/D/E 的 cstrike15
   专属部分按阶段 1 的先例进 `panoramauiclient.dll`。是否同意。
4. **是否顺便补 `public/panorama/**` 的少量头**（例如 `uipage.h` 一类目前缺失的头；需要先确认
   到底哪些被真正引用）。
5. **内容侧检查**：`layout/popups/*.xml`、`layout/tooltips/*.xml`、`layout/context_menus/*.xml`
   是否已部署到 `D:\cstrike\cstrike\panorama\layout`；缺的部分要不要一起补。
6. **排期**：批次 A→B→D（低风险，可立刻开工）与批次 C、E（中/高风险）是否分开做、分开验证。

## 预估规模

四个批次合计约 **5 800 行**移植代码（不含 `ui_page*`、`ui_canvas`、`ui_itempreview*` 这些
与业务数据强相关的部分）。其中 A+B+D ≈ 1 750 行，属于可以直接开工的低风险部分。
