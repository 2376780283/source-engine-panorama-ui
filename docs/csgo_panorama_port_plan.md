# CS:GO Panorama (csgoui) 移植 —— 可行性分析与分步计划

> 目标：把 `D:\CSGO2019` 中的 CS:GO Panorama UI 系统（含 csgoui 页面）移植进 `d:\source-engine`。
> 本文基于对两个代码库的**实际文件盘点**编写（2026-09-08），作为动工前的基线文档。

---

## 0. TL;DR（结论先行）

- **总体判断**：可行，但必须**分阶段、框架先行**。一次性"把整套 csgoui 跑起来"等价于移植大半个 CS:GO 客户端 + 引擎增量，风险极高。
- **三个硬阻塞**（决定可行性的天花板）：
  1. **V8 JS 库**：Panorama 脚本层依赖 V8；`d:\source-engine` **自带真 V8 头**（`external/v8/include`，2020 大导入遗留，路径正是 panorama `#else` 分支期望的 `../external/v8/...`），但**没有 V8 库** → 头可编译、链接缺库；CSGO2019 源码树里同样只有对预编译库的引用。**年代匹配需在 Phase 2 验证。**
  2. **UI 内容缺失**：csgoui 真正的页面是 XML/CSS/JS/图片/字体，编译/打包进 CS:GO 的 `pak`/游戏内容中，**源码树里没有这些内容**。没有内容，框架只能显示自建测试页。
  3. **客户端不匹配**：SE 的客户端是 CS:S 风格 `game/client/cstrike`（纯 VGUI），而 csgoui 页面绑定 CS:GO 的 `cstrike15` 客户端与经济/库存/比赛状态系统。
- **构建路线**：SE 用 waf + 一个**极简 VPC 解析器**（`scripts/waifulib/vpc_parser.py`，仅 148 行，只解析文件列表与少量宏），无法解析 CSGO2019 的 `$Conditional/$Configuration/protobuf_builder` 等复杂 VPC。因此新模块**直接手写 wscript** 更现实，VPC 仅作文件清单参考。
- **平台建议**：先只做 **Windows (msvc)**；linux/android 因预编译依赖（V8 等）暂缓。
- **合规提醒**：CSGO2019 属 Valve 泄露源码、目标仓库为其 fork。本计划仅用于本地技术研究/个人移植，注意不扩散、不商用。

---

## 1. 两个代码库的基线（事实盘点）

| 维度 | `d:\source-engine`（目标） | `D:\CSGO2019`（源） |
|---|---|---|
| 血缘 | Source 2013 系多游戏引擎（`cstrike`=CS:S 风格 + `hl2/portal/dod/hl1…`） | CS:GO 2019 完整源码（客户端 `cstrike15`） |
| 构建 | waf（`wscript` + 极简 `vpc_parser`） | VPC + `CreateSolution.bat` |
| Panorama | `public/panorama` 仅**79 个头文件**，且与 CSGO2019 **内容分叉**（如 `iuipanel.h` 差异 263 行） | 组件齐全（见 §2） |
| 引擎 panorama 引用 | 无（engine/client 中 grep 不到 panorama） | `PANORAMA_ENABLE` 编译期宏控制 |
| 第三方 | `thirdparty/`：SDL、curl、zlib、freetype、fontconfig、protobuf-2.6.1 等（从源码构建） | `external/` 仅 crypto++/openssl；其余依赖 Valve 预编译 `lib/public`（本目录不存在） |
| 客户端 | `game/client/cstrike` 纯 VGUI | `game/client/cstrike15`（465 文件）+ `client_panorama` 变体 |

**同名文件重合度**（按相对路径统计，粗略反映血缘接近度）：

| 目录 | SE 文件数 | CSGO2019 文件数 | 同名重合 |
|---|---|---|---|
| `engine` | 554 | 739 | 417 |
| `public` | 1154 | 1404 | 991 |
| `materialsystem` | 1668 | 1738 | 998 |
| `game/client` | 1071 | 1209 | 549 |
| `game/server` | 1229 | 855 | 623 |

> 结论：两者血缘近、底层 API（tier0/1/2、materialsystem、filesystem 等）大体对齐，**适合做"增量移植 + 适配"**；但 engine 与 public 仍存在数百文件的差异，CSGO2019 的 `PANORAMA_ENABLE` 片段不能直接复制，必须逐点适配。

---

## 2. Panorama（csgoui）组件清单与依赖地图

### 2.1 组件清单（CSGO2019 → 目标建议路径）

| 组件 | 位置（CSGO2019） | 文件数 | 作用 | 目标建议路径 | 移植难度 |
|---|---|---|---|---|---|
| Panorama 框架 | `panorama/` | 189 | UI 框架本体：`layout/ renderer/ input/ text/ textinput/ controls/ data/ source2/`、`uiengine.cpp`、`uitoplevelwindow*` 等 | `d:\source-engine\panorama\` | ★★★★★ |
| Source1 封装层 | `panorama_s1wrapper/` | 279 | 把 Source2 风格渲染/声音/输入翻译到 Source1（`rendersystem/ wrap_render.cpp wrap_sound.cpp s1wrapper.h` 等） | `d:\source-engine\panorama_s1wrapper\` | ★★★★★ |
| 客户端模块 | `panoramauiclient/` | 3 | 独立 DLL，导出 `IPanoramaUIEngine / IPanoramaUIClient / IUITextServices`，由 launcher 加载 | `d:\source-engine\panoramauiclient\` | ★★★ |
| 客户端 UI 基类 | `game/client/panorama/` | 60 | `ui_root/ui_page/ui_popup/ui_tooltip/context_menu/ui_js_panel/ui_itempreview` 等（依赖 CS:GO 客户端） | `d:\source-engine\game\client\panorama\` | ★★★★ |
| **csgoui（CS:GO 页面 C++ 侧）** | `game/client/cstrike15/panorama/` | 145 | `csgo_mainmenu/scoreboard/buymenu/endofmatch/hud/loadout/teamselect/store…` | 决策点，见 §5 Phase 4 | ★★★★★ |
| CS:GO 客户端 | `game/client/cstrike15/` | 465 | econ/库存/玩法状态，`client_panorama` 变体 | 决策点，见 §5 Phase 4 | ★★★★★ |
| 对外头文件 | `public/panorama/` | 98 | 框架对外接口 | `d:\source-engine\public\panorama\`（覆盖 79 个 + 补缺） | ★★ |

### 2.2 引擎/系统侧集成面（CSGO2019 中带 `PANORAMA_ENABLE` 或引用 panorama 的位置）

| 文件 | 做了什么 | SE 现状 |
|---|---|---|
| `engine/panoramaenginehandler.cpp/.h` | 创建顶层 window、输入/渲染回调、头像图片等 | 无，需整体移植+适配 |
| `engine/sys_dll2.cpp` | 获取 `IPanoramaUIEngine/UIClient` 接口、`AddPanoramaView` | 需移植片段 |
| `engine/baseclientstate.cpp` | 引用 `panorama/iavatarimagemgr.h` | 需移植片段 |
| `inputsystem/inputsystem.cpp` | 输入路由到 `IPanoramaUIEngine` | 需移植片段 |
| `interfaces/interfaces.cpp` | 注册 `PANORAMAUI_ENGINE/CLIENT/TEXT_SERVICES_INTERFACE_VERSION` | 需移植片段 |
| `launcher/launcher.cpp` | 按 panorama 模式加载 `panoramauiclient` 模块 | 需移植片段 |
| `materialsystem/stdshaders/` | `panorama_cshader.cpp`、`panorama_*_ps30/vs30.fxc`、`panoramafancy_*`、`panorama_ssaa_resolve.cpp` | 无，需移植 |

### 2.3 外部依赖在 SE 的现状核对

| 依赖 | 用途 | CSGO2019 | SE 现状 |
|---|---|---|---|
| **V8** | Panorama JS 引擎 | 引用 `v8.h`（预编译） | ⚠️ 头在 `external/v8/include`（真头、旧 API）但**无库可链**（**B1 降级为“缺库+年代匹配”**） |
| **protobuf** | `chromemessages/steammessages` 消息 | 较新版本（预编译 libprotobuf） | ⚠️ 仅 `protobuf-2.6.1`，版本偏旧（**B7**） |
| **resourcefile** | UI 资源解析 | `$LIBPUBLIC\resourcefile` | ❌ 无（**B2/B5 相关**） |
| **socketlib** | 网络（Windows） | `$LIBPUBLIC\socketlib` | ❌ 待查（可能由 tier/其他替代） |
| SDL | 顶层 window/输入 | 使用 | ✅ `thirdparty/SDL` |
| curl | web api | 使用 | ✅ `thirdparty/curl` |
| zlib/libpng/libjpeg | 图片 | 使用 | ✅ |
| freetype/fontconfig/pango | 文本（pango 仅 macOS） | 使用 | ✅ freetype/fontconfig；pango 无（Windows 走 `panorama_text_base`） |
| crypto++ | 加解密 | `external/` | ⚠️ 待查 |

---

## 3. CSGO2019 中 Panorama 是怎么跑起来的（集成架构）

```
launcher.cpp ──(bPanorama 判断)──▶ 加载 "panoramauiclient" AppSystem
                                        │  导出 IPanoramaUIEngine / IPanoramaUIClient / IUITextServices
                                        ▼
engine (PANORAMA_ENABLE)  ◀── interfaces.cpp 注册接口
  ├─ sys_dll2.cpp : 查询接口 + AddPanoramaView
  ├─ panoramaenginehandler.cpp : 创建顶层 UI window / 输入 / 渲染回调 / 头像
  ├─ baseclientstate.cpp : 用 avatar 图片
  └─ inputsystem.cpp : 输入转发给 IPanoramaUIEngine
        ▲
client_panorama.dll  (client_cstrike15_panorama.vpc → OUTBINNAME=client_panorama)
  ├─ 链接静态库: panorama + panorama_s1wrapper + panorama_client + bitmap/vtf/mathlib/tier2/tier3
  ├─ 内含 cstrike15 客户端 465 文件 + game/client/panorama 60 文件 + cstrike15/panorama 145 文件
  └─ 由 panorama_s1wrapper 把 Source2 风格渲染请求翻译为 Source1 的 mesh/材质
```

> 要点：**Panorama 不是引擎外挂一个 UI DLL 就完事**——它被编译进 `client_panorama.dll`，且引擎、输入系统、材质着色器都要在 `PANORAMA_ENABLE` 下协同。SE 目前完全没有这条路径。

---

## 4. 可行性评估与阻塞点

### 4.1 阻塞/风险表（按影响排序）

| 编号 | 阻塞 | 影响 | 对策（候选） | 状态 |
|---|---|---|---|---|
| **B1** | V8 **库**缺失（头已具备） | `panoramatypes.h`/`iuiengine.h` 无条件 include v8 → 头可编译但链接缺库 | ① 获取/构建与 SE `external/v8` 头同年代的 V8 库（Windows x86）② 换 JS 引擎：不现实 ③ 砍脚本层：仅静态面板 | 🔴 Phase 2 起始决策 |
| **B2** | csgoui 页面内容（XML/CSS/JS/图/字）不在源码树 | 无内容可显示 | ① 先自建测试页验证框架 ② 正式 csgoui 内容需合法获取 CS:GO 游戏文件 | 🔴 |
| **B3** | SE 客户端是 CS:S（cstrike），非 CS:GO（cstrike15） | csgoui 页面依赖 CS:GO 特有状态接口 | 决策：是否连 cstrike15 一起移植（工作量+许可）；或只移植与游戏状态弱耦合的页面 | 🟠 决策点 |
| **B4** | `public/panorama` 头文件分叉（79 vs 98，内容差异大） | 框架源码按 CSGO2019 头文件编写，无法用 SE 现头编译 | 以 CSGO2019 的 98 个头文件为基线合入（Phase 1） | 🟡 低风险 |
| **B5** | SE 的 `vpc_parser`（148 行）不支持复杂 VPC | 不能直接喂 CSGO2019 的 `.vpc` | 新模块手写 `wscript`；VPC 仅作文件清单来源 | 🟡 中风险 |
| **B6** | 引擎版本漂移（engine 同名 417/554，内容有差异） | `PANORAMA_ENABLE` 片段不能照搬 | 逐文件手工移植+编译驱动 | 🟠 中风险 |
| **B7** | protobuf 版本（SE 2.6.1 vs CSGO 需要新） | `chromemessages/steammessages` 生成/链接 | 视 Phase 2 需要升级 SE protobuf 或裁剪消息依赖 | 🟠 中风险 |
| **B8** | resourcefile/socketlib 等库缺失 | 链接期缺库 | 在 SE 内补建或裁剪对应功能 | 🟠 中风险 |
| **B9** | 合规/来源 | 全流程 | 仅本地研究；CSGO2019 不扩散不商用 | 🟡 提醒 |

### 4.2 各阶段可行性速评

| 阶段 | 可行性 | 理由 |
|---|---|---|
| Phase 0 基线构建 | ✅ 高 | 纯环境与流程 |
| Phase 1 头文件/接口对齐 | ✅ 高 | 纯文件合入 + 编译冒烟 |
| Phase 2 框架库编译 | 🟠 中 | 受 B1/B7/B8 影响；**建议先做"砍 V8/脚本"冒烟再接入 V8** |
| Phase 3 引擎接入 | 🟠 中 | 受 B6 影响，工作量可控但琐碎 |
| Phase 4 csgoui 页面 | 🔴 低→高 | 受 B2/B3 决定，属"要么小演示、要么大工程"的岔路 |

---

## 5. 分阶段移植计划

> 每阶段含：目标 / 主要动作 / 验收标准 / 预计工作量。**每阶段结束设 go/no-go 决策点。**

### Phase 0 —— 环境与基线固化（约 0.5~1 天）
- [ ] 建分支 `csgo-panorama-port`（fork 基线 `391effac`）。
- [ ] 用现有任务跑通基线构建（`./waf.bat install`），记录产物路径与报错基线。
- [ ] 确认 Windows 工具链（msvc / VS 版本）、Python 版本、waf 版本。
- [ ] 给 CSGO2019 源码树做只读快照索引（文件清单/哈希），便于追溯。
- **验收**：基线可复现构建；无回归。
- **决策**：环境就绪 → Phase 1。

### Phase 1 —— Panorama 公共头文件与接口对齐（约 1~2 天）
- [ ] 以 CSGO2019 `public/panorama/`（98 文件）覆盖 SE（79）并补齐缺文件（`iuirenderdevice.h`、`renderer/rendercommands.h`、`text/`、`controls/`、`s1wrapperRenderAttributes.h`、`parseuieventparam.h`、`uiinputcapture.h`、`iavatarimagemgr.h` 等）。
- [ ] 比对 SE/CSGO2019 `public/` 其余目录，把 panorama 头文件依赖的类型/接口缺口列出并补齐（如 `interfaces/`、`common/` 内相关声明）。
- [ ] 建一个"头文件冒烟"waf 目标（仅 include + 空 TU），验证全部 panorama 头可编译。
- **验收**：`public/panorama` 与 CSGO2019 同源；头文件编译通过。
- **决策**：头文件口径定稿 → Phase 2。

### Phase 2 —— 框架库移植：panorama / panorama_s1wrapper / panoramauiclient（数周，务必拆子步骤）
- 子步骤 2.1 **建立 waf 目标骨架**（参照 `vstdlib/wscript`、`vpklib/wscript` 模式）：
  - `libpanorama`（`panorama/`）、`libpanorama_s1wrapper`（`panorama_s1wrapper/`，静态）、`panoramauiclient`（动态，导出三个接口）。
  - 编译宏：`PANORAMA_EXPORTS`/`PANORAMA_CLIENT_EXPORTS`/`SOURCE2_PANORAMA_FIXME`/`SOURCE2_PANORAMA`/`PANORAMA_USE_S1WRAPPER`/`PANORAMA_ENABLE`。
- 子步骤 2.2 **脚本层决策（B1）**：先以"禁用 JS/最小脚本"配置让主体编译跑通，再接 V8（见 §4.1 B1）。这是影响最大的子步骤。
- 子步骤 2.3 **Source1 封装裁剪**：确认走 `PANORAMA_USE_S1WRAPPER` 路径；渲染→Source1 mesh/材质、声音包装、输入包装、文本（Windows 用 `panorama_text_base`，避免 pango）。
- 子步骤 2.4 逐文件修复 include/接口漂移；把依赖裁剪到可构建集（B7/B8 决策：protobuf 升级 or 裁剪消息）。
- **验收**：`panoramauiclient.dll` 可独立构建；用接口 factory 能查到 `IPanoramaUIEngine/IPanoramaUIClient/IUITextServices`。
- **决策**：模块可加载 → Phase 3。

### Phase 3 —— 引擎接入（PANORAMA_ENABLE）（约 1~2 周）
- [ ] 移植 `engine/panoramaenginehandler.cpp/.h` 到 SE engine 并适配（宿主循环、sys 差异、窗口/SDL）。
- [ ] 按 CSGO2019 语义给 SE 打 `PANORAMA_ENABLE` 补丁：`sys_dll2.cpp`、`interfaces/interfaces.cpp`、`launcher/launcher.cpp`、`inputsystem/inputsystem.cpp`、`baseclientstate.cpp`。
- [ ] 移植 materialsystem panorama shaders（`stdshaders` + `fxctmp9`）并在 `materialsystem` 注册。
- [ ] 引擎创建顶层 UI window，输入路由闭环。
- **验收**：以"测试面板"（自建 XML/CSS/JS 或空面板）能在游戏画面内显示并响应输入。
- **决策**：引擎能承载 Panorama → Phase 4。

### Phase 4 —— 客户端 UI 与 csgoui（岔路口，见 B2/B3）
- 路线 A（**推荐作为后续第一步**）：先移植 `game/client/panorama/`（60 文件）中与游戏状态弱耦合的基类，自建若干页面验证页面系统/弹窗/提示。
- 路线 B（真 csgoui，需 go/no-go）：
  - 决策 1：是否整体移植 `game/client/cstrike15/`（465 文件）+ econ/库存基础设施（几乎是把 CS:GO 客户端搬进 SE）。
  - 决策 2：csgoui 页面内容（XML/CSS/JS）的合法来源（B2/B9）。
  - 若 A 不行/不做 B：为 SE 的 CS:S 客户端**定制一套 csgoui 风格页面**（中等工作量，无合规风险）。
- **验收**：主菜单/计分板等目标页面显示并与目标客户端数据源绑定。

### Phase 5 —— 收敛与收尾
- [ ] 构建系统定稿（wscript 直写 vs 增强 vpc_parser，两者择一并文档化）。
- [ ] 性能/内存/日志通道检查；清理调试代码。
- [ ] 更新本计划 + 仓库 memory 记录实际差异与踩坑。

---

## 6. 推荐下一步

1. **立即执行 Phase 0 + Phase 1**：成本低、信息增量大，且能立刻暴露"头文件口径"问题。
2. **Phase 2 开始前做 B1（V8）决策**：它是整个计划中影响面最大、最不确定的一环；建议先做一个"砍脚本层能否编译"的 3 天 spike。
3. **B2/B3（内容与客户端）作为独立决策项**放在 Phase 4 前，避免前期为错误目标投入。

---

## 附录 A：组件文件清单（源 → 目标建议）

| 源（CSGO2019） | 文件数 | 目标（source-engine） | 备注 |
|---|---|---|---|
| `public/panorama/` | 98 | `public/panorama/` | 覆盖式合入（Phase 1） |
| `panorama/` | 189 | `panorama/` | 新增（Phase 2） |
| `panorama_s1wrapper/` | 279 | `panorama_s1wrapper/` | 新增（Phase 2） |
| `panoramauiclient/` | 3 | `panoramauiclient/` | 新增 DLL（Phase 2） |
| `engine/panoramaenginehandler.*` | 2 | `engine/` | 移植+适配（Phase 3） |
| `engine/sys_dll2.cpp` 等 4 处引擎片段 | – | `engine/` 等 | 打补丁（Phase 3） |
| `materialsystem/stdshaders/panorama*` | 8 | `materialsystem/stdshaders/` | 移植（Phase 3） |
| `game/client/panorama/` | 60 | `game/client/panorama/` | 决策（Phase 4） |
| `game/client/cstrike15/panorama/` | 145 | 决策 | csgoui C++ 侧（Phase 4） |
| `game/client/cstrike15/` | 465 | 决策 | CS:GO 客户端（Phase 4） |

## 附录 B：关键文件速查（CSGO2019）

- 客户端全景变体：`game/client/client_cstrike15_panorama.vpc`（→ `client_panorama.dll`）
- 框架库依赖：`panorama/panorama.vpc`、`panorama/panorama_client.vpc`（依赖 mathlib/bitmap/vtf/tier2/tier3/resourcefile/socketlib/libprotobuf/steamclient_client/panorama_s1wrapper）
- 集成开关：宏 `PANORAMA_ENABLE`；launcher 逻辑见 `launcher/launcher.cpp:781-803`
- 引擎处理：`engine/panoramaenginehandler.h/.cpp`；`engine/sys_dll2.cpp`（~569、1255、3261 行）

## 附录 C：常用命令（目标工程）

```text
./waf.bat configure -T debug --prefix=build/out/   # 配置
./waf.bat install                                   # 构建
git -C d:\source-engine <...>                       # git 需用 -C（多终端目录易漂移）
```
---

## 执行记录（Phase 0/1, 2026-09-08）

### Phase 0 —— 完成
- 分支 `csgo-panorama-port`（基线 `391effac`）。
- 工具链：MSVC 14.44.35207（VS2022 Community），HostX86→32 位，`GAMES=cstrike`，release；前置库 `lib/win32/x86`、`dx9sdk`。
- `waf configure` 首跑未带 `--build-games`，**误成 hl2/64 位**（产到 `build/out/hl2/bin`，且成功跑通、佐证工具链 OK）；已纠正为 **cstrike/32 位**：`waf configure -T release --32bits --build-games=cstrike --prefix=build/out/ --disable-warns --enable-opus`。cstrike 基线构建后台进行中。

### Phase 1 —— 完成（冒烟编译待基线构建后执行）
- **引用面盘点**：SE 全源码（engine/game/common/materialsystem/vgui2/launcher/inputsystem/tools…）**对 `panorama` 头零引用** → `public/panorama` 是“休眠”目录，覆盖不破坏现有构建。
- **头文件合入**：以 CSGO2019 `public/panorama`(98) 为基线覆盖 SE(79)：`ADD=21 OVERWRITE=77 SAME=0`，保留 SE 独有 `uipbmsgbase.h`、`controls/listsegmentview.h`（均无人引用）。现 100 文件，报告 `docs/panorama_header_overlay_report.txt`。
- **关键发现（B1 降级）**：`panoramatypes.h`/`iuiengine.h` 无条件 include v8；SE 已带真 V8 头（`external/v8/include`，2020 提交 `3bf9df6b` 与 panorama 头一同导入）→ **纯头编译可行**；但无 V8 库 → 链接仍是阻塞。另 2 头（`data/imageloader.h`、`uitoplevelwindowoverlay.h`）escape 到根 `panorama/` 源模块，留待 Phase 2。
- 冒烟脚手架：`scratch/panorama_hdrcheck/`（98 自洽头 TU + 仅编译目标的 wscript），已临时挂入根 `wscript` `game` 列表（`# TEMP Phase-1...` 标记），Phase 1 通过后移除。

### 待办
- [ ] cstrike 基线构建完成后：重新挂 scratch → configure → `waf build --targets=panorama_hdrcheck` 跑纯头冒烟。
- [ ] 冒烟通过后：移除 scratch 挂载、清理 `scripts/dev/tmp_*.ps1`，提交 Phase 1。
- [ ] Phase 2 起始做 B1 决策：V8 库来源/年代。

### 纯头冒烟诊断结论（2026-09-08，最终）
采用 **SOURCE2_PANORAMA 模式**（与 CSGO2019 框架 `panorama.vpc` 一致）进行纯头冒烟，逐轮修复情况：
1. ✅ `tier1/utlptrarray.h` 缺失 → 已从 CSGO2019 拷入 SE。
2. ✅ `panoramatypes.h` 遗留 `CUtlVectorFixedGrowable<T,int>` vs SE tier1 `size_t` 版冲突 → 加守卫 `SE_TIER1_FIXEDGROWABLE`（SOURCE2 模式下该块本就被跳过，守卫保留无害）。
3. ✅ `audio/iaudiointerface.h`（source1 分支，连 CSGO2019 都没有）→ 证实必须走 SOURCE2 模式。
4. ✅ v8：SOURCE2 路径硬编码 `../thirdparty/v8/include/v8.h` → 已把 `external/v8` 镜像到 `thirdparty/v8`。
5. ✅ `tier0/platwindow.h`、`mathlib/beziercurve.h` 缺失 → 已从 CSGO2019 拷入。
6. 🔴 剩余为 **CSGO Source2 时代公共层**，无法“补几个头”解决：
   - `DECLARE_LOGGING_CHANNEL`（`tier0/logging.h` 子系统，SE 无；其依赖 `icommandline.h`/`xbox/xbox_console.h` 连 CSGO2019 public 都没有）。
   - `CRefCount`：两仓库 `tier1/refcount.h` 都只含 `CRefCountService*`，无普通 `CRefCount`（Source2 系，仅框架构建路径具备）。
   - 结论：**纯头全绿已跨入 Phase 2（“CSGO 公共层移植 + 不破坏 SE 自身构建”的 tier 对齐工作包）**。
- 已把临时冒烟挂载从根 `wscript` 移除并重新 configure，**仓库恢复干净可构建**（cstrike/32 基线）。
- 新增/镜像文件（均未被 SE 现有代码引用，无害）：`public/tier1/utlptrarray.h`、`public/tier0/platwindow.h`、`public/mathlib/beziercurve.h`、`thirdparty/v8/`（external 镜像）。临时脚本 `scripts/dev/tmp_*.ps1`、`scratch/panorama_hdrcheck/` 保留备查，提交前清理。

---

## Phase 2 依赖分层（2026-09-08，实测 include 扫描）

> 已把 CSGO2019 三个源码树拷入本仓库（未跟踪）：`panorama/`(189)、`panoramauiclient/`(3)、`panorama_s1wrapper/`(279)。
> 扫描这 456 个文件的所有 include，与 SE 比对：1027 次解析成功，413 个“未解析”（含大量噪音）。报告 `docs/panorama_phase2_missing_includes.txt`。
> 分层结论：

- **L0 噪音/目录类**（无需“移植”，加 include 目录或放对位置即可）：
  系统/SDK 头（windows.h、DWrite.h、D3Dcommon.h、SDL.h、WinSock2.h、std*.h…）；文本后端 pango/glib/ft2build/fontconfig（多为 macOS 分支；SE 已有 freetype，需 include 到 `thirdparty/freetype` 等）；树内自引用（s1wrapper.h、wrap_texture.h、uitoplevelwindowoverlay.h 等）。
- **L1 真·缺失 Source2 接口子系统**（从 CSGO2019 移植公共头 + 判定 shim/stub，wrapper 阶段实现）：
  `resourcesystem/iresourcesystem.h`、`rendersystem/irenderdevice.h|irendercontext.h`、`materialsystem2/imaterialsystem2.h`、`assetsystem/iassetsystem.h`、`iimemanager.h`、`common/enumutils*.h`。
- **L2 生成依赖**：protobuf 生成 `.pb.h`（rendermessages 等）→ 需在 SE 建 protobuf 生成流水线（SE 有 protobuf-2.6.1，CSGO 用更新版，需对齐）。
- **L3 链接卡点**：V8 库（仅影响最终链接；静态库编译阶段只需 v8 头，可先行）。

**建议执行序**：先做 L1（把缺失接口公共头/最小实现搬入并配好 include 目录）→ 尝试编译 `panorama/` 的最小自洽 TU 集 → 逐模块展开；L3 与 L2 在需要链接/生成时处理。

### 首次框架编译实测（2026-09-08，panoramasymbol.cpp 经 stdafx 全量拉入）
- 已建 `panorama/wscript`（stlib 目标 `panorama`，SOURCE2_PANORAMA），临时注册后**首次真编译**：错误收敛为**有限 6 类**（= tier 对齐包）：
  1. logging 通道缺失（`LOG_PANORAMA`/`DECLARE_LOGGING_CHANNEL`，需 `tier0/logging.h` 或其 shim）
  2. `CRefCount` 无定义（rendercommands/iuirenderengine/iuiengine；两库 refcount.h 都只有 `CRefCountService*` → 需自供 source2 风格 CRefCount shim）
  3. mathlib 漂移：`Quaternion::ToQAngle`、`VMatrix::GetIdentityMatrix`（CSGO 有、SE 无 → 增补方法）
  4. `ConstructOneArg/TwoArg` 与 SE `tier0/platform.h` 重定义（panoramatypes 需加守卫/适配）
  5. `CMemoryStack::WillAllocSucceed` 缺失（SE tier1 增补）
  6. 缺少量头：`currencyamount.h` 等
- ⚠️ panorama 目标**暂不注册**进根 wscript（否则默认 `waf install` 会失败）；迭代时临时注册 + `waf build --targets=panorama`，编完撤下。

### 两条策略（待确认，推荐 A）
- **A（推荐）补丁/shim 路线**：在 SE 现有 tier 上**增量加守卫/方法/迷你实现**（platform.h 冲突加守卫、mathlib 补方法、tier1 补 WillAllocSucceed/CRefCount、写最小 logging shim、补 currencyamount.h）。风险低、不动 SE 现有语义，panorama 就着 SE-era 基础编译。
- **B tier 整体升级路线**：把 SE 共享 tier0/tier1/mathlib 升级到 CSGO 时代（与 CSGO 引擎统一）。改动面大、可能波及整个 SE 引擎构建，风险高。
- 影响：panorama_s1wrapper 已含 rendersystem/resourcesystem/materialsystem2/assetsystem 接口（加 include 目录即解析）；v8 已镜像 thirdparty/v8（子模块工作区）；`currencyamount.h` 待补。

### 路线 A 补丁规格（已收集素材，逐项机械照做即可）
1. ✅ `ConstructOneArg/TwoArg`：`panoramatypes.h` 已加守卫 `SE_PLATFORM_HAS_CONSTRUCT_HELPERS`（在 `panorama/wscript` defines 中定义；SE `tier0/platform.h` 已有同语义实现）。
2. ✅ 缺头 `currencyamount.h`：已从 `D:\CSGO2019\common\currencyamount.h` 拷入 `common/`。
3. `CRefCount`：CSGO 定义于 `public/gcsdk/refcount.h:75`（`namespace GCSDK`，AddRef/Release/DestroyThis/m_cRef=1）。需确认 `renderer/rendercommands.h` 使用处的命名空间后，把该类提供到 panorama 可见处（或补到 `tier1/refcount.h` 末尾、或以 GCSDK 头方式引入）。
4. `CMemoryStack::WillAllocSucceed`：SE `public/tier1/memstack.h` 无；CSGO 版：类内声明 `bool WillAllocSucceed( unsigned bytes ) const;` + inline 实现（用 GetMaxSize/GetUsed/CommitTo 语义）→ 增补进 SE memstack（需按 SE 类现有 API 适配）。
5. mathlib 增量：SE 缺 `Quaternion::ToQAngle`（CSGO `vector.h` 系，见 RadianEuler/Quaternion 转换）与 `VMatrix::GetIdentityMatrix()`（CSGO `vmatrix.h:199` 内联 `static const VMatrix& GetIdentityMatrix(){ static const VMatrix identityMatrix(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1); return identityMatrix; }`）→ 在 SE `mathlib/vector.h`、`vmatrix.h` 增量补方法（不改语义）。
6. logging 通道：SE 无 `tier0/logging.h`；CSGO 有但依赖 `icommandline.h/xbox/win32consoleio`（部分不在其 public）。最小路线：自写迷你 `logging.h`（`DECLARE_LOGGING_CHANNEL` 空实现或 stub + 记录用到的 `Log_*`），或在 SE tier0 补最小通道。需先统计 panorama 用到的 logging API。
7. 其余小头（若有）随编译逐个补。

> 每次改共享头后建议：先 `waf install` 验基线（会触发相关模块重编），再临时注册 panorama 跑 `--targets=panorama`。

---

## 变更日志
- 2026-09-08：成稿（可行性分析+分阶段计划）；执行 Phase 0/1（见上执行记录）。