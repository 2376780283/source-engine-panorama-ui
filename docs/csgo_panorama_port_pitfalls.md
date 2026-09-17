# CS:GO Panorama 移植 —— 踩坑表

> 配套：`csgo_panorama_port_checklist.md`（任务清单）、`csgo_panorama_port_breakthroughs.md`（重大突破）、
> `csgo_panorama_port_plan.md`（计划）、`CSGO_PANORAMA_PORT_NOTES.md`（实操速查）。
>
> 每条尽量给出：**症状 → 根因 → 解法 → 关联**。P 编号可被其他文档引用。
> 更新：2026-09-16

---

## A. 构建与配置

| # | 坑 | 症状 | 根因 | 解法 / 关联 |
|---|---|---|---|---|
| P1 | waf 默认跑 hl2 + **64 位** | 产物跑到 `build/out/hl2/...`，cstrike 内容对不上 | `wscript` 里 `MSVC_TARGETS = ['x64']` 是默认（注释还写反了） | 配置必须显式 `-T release --32bits --build-games=cstrike` |
| P2 | VPC 解析器太弱 | `scripts/waifulib/vpc_parser.py` 遇 `$Conditional/$Configuration/protobuf_builder` 就废 | 只实现了 148 行的子集 | **新模块手写 wscript**，VPC 仅当文件清单参考 |
| P3 | 改了 wscript 不生效 | 新增源文件"没编进去" | waf 需要重新 configure 才反映 wscript 变更 | 改完重跑 configure |
| P4 | 改完源码，游戏里行为没变 | 反复验证同一个"没变化"的现象 | 只 build 没 install / 没部署；或游戏进程还锁着 DLL | 部署脚本先杀进程；`D:\cstrike\bin` **和** `D:\cstrike\cstrike\bin` 都要覆盖 |
| P5 | 64 位下 Panorama 三个目标被跳过 | 日志 `panorama: skipped in 64-bit builds (no x64 prebuilt V8)` | `external/v8/lib/win/x64/` 是**空目录**；NuGet 的 7.3.492 x64 包是 0.2 MB 空壳 | 32 位是唯一可用配置；要 64 位需自建 V8 x64 |
| P6 | protobuf 只有 x86 | 64 位链接缺 `libprotobuf` | `scripts/dev/build_protobuf.ps1` 用 `vcvars32` | 要用 `vcvars64` 重跑该脚本 |
| P7 | 注释不可信 | 以为"缺 protobuf 生成源所以字体包编不进来" | `panorama/wscript` 的注释过期了 | 已核实：`uifontfile_format.pb.cc` 早就在树里 → 注释已更正 |

## B. 源码 / 接口对齐

| # | 坑 | 症状 | 根因 | 解法 / 关联 |
|---|---|---|---|---|
| P8 | `public/panorama` 是**不同支线** | 只补缺头，编不过/行为诡异 | SE 与 CSGO2019 的 panorama 公共头整体分叉（`iuipanel.h` 差 263 行） | 整组以 CSGO2019 覆盖，**勿只补缺** |
| P9 | V8 include 双路径 | 头找不到 / 符号对不上 | `SOURCE2_PANORAMA → ../thirdparty/v8`，否则 `../external/v8`；SE 自带 external/v8 头但**无库** | 统一 7.3.492 + 加一层 6.x 兼容垫片（`cc0cf539`） |
| P10 | `iuiengine.h` 的 source1 分支引用不存在的头 | 编译失败 | 该分支 include `audio/iaudiointerface.h`，CSGO2019 里都没有 | 只能按 `SOURCE2_PANORAMA` 模式编译 |
| P11 | `CUtlVectorFixedGrowable` 冲突 | 编译报类型不匹配 | `panoramatypes.h` 遗留 `int` 版 vs tier1 `size_t` 版 | 守卫宏 `SE_TIER1_FIXEDGROWABLE` |
| P12 | CS:GO 缺 `lib/` | 无法自洽构建 CSGO2019 | Valve 预编译库没随源码发布 | 本树提供；`panorama_text_base.vpc` 里的 `libeay32.lib` 等要另找替代 |
| P13 | 头里 `#include "memdbgon.h"`（无前缀） | 找不到头 | CS:GO 的 include 目录约定 | 把 `public/tier0` 直接加进 include 目录 |
| P14 | MSVC `/showIncludes` 刷屏 + C4819 | 错误行号难定位 | 代码页 1252 vs UTF-8 中文 | 靠行号定位；别读乱码文件名 |
| P15 | 编译器"一次只报前几个缺失头" | 一个 TU 要修很多轮 | MSVC 行为 | 预期多轮修 |

## C. 渲染与着色器

| # | 坑 | 症状 | 根因 | 解法 / 关联 |
|---|---|---|---|---|
| P16 | **背脸剔除** | draw 都发出了，屏幕**一个像素都没有** | panorama 顶点已在 clip space 且 Y 翻转，与 D3D9 `D3DCULL_CCW` 冲突 | 关掉剔除（`361ff380`，bring-up 最关键修复） |
| P17 | shader 组合索引 | 主菜单一画就崩溃 | 动态 combo 索引越界/组合表对不上 | 修组合索引（`acee9c4a`）；手写 `fxctmp9/panorama_{vs30,ps30}.inc` 的步长必须与 C++ 公式一致：`fastblur*1 + blur*9 + particle*18 + downsample*36` |
| P18 | 未绑定的采样器 | 纹理位置是纯白 | D3D9 对未绑定 sampler 返回白色 | `pAttr->GetValue(&pTexture, ATTR_Texture0)` + `BindTexture(SHADER_SAMPLER0, ...)`（`6f775ca8`） |
| P19 | `$renderattr` 未设/为 NULL | 崩溃（release 下断言被编掉） | 材质是共享的，某些绘制路径不走 `UpdateMaterial()` | 在 shader 里显式判空并跳过（`panorama_dx9.cpp` / `panoramafancy_dx9.cpp`） |
| P20 | 纹理指针不可信 | `CShaderSystem::BindTexture` 里崩 | 绘制引用了已释放/未加载的纹理 | 加指针范围校验；缺纹理时按"无纹理"画并告警 |
| P21 | 「没有编译版资源」被当错误 | 日志刷红，误判成故障 | 有的资源本来就只有散文件版 | 降级为提示（`a41ffba5`） |
| P22 | SVG 扫描线写错 | 图标**只填了第一行像素** | shim 的 scanline 循环写错 | 修循环 + 加离线解码测试（`23003cec`） |
| P23 | **紫黑缺材质格子**（当前问题） | 主菜单左侧/右侧面板整片紫黑格；日志**没有**缺图报错 | 模糊那几趟绘制走不通，Source 兜底是错误材质（紫黑格子贴图） | 09-16 A/B 实证：把 `@panorama_disable_blur` 默认改 1 → 格子全消失、页面完整渲染；根因（PS combo / 纹理绑定）待钉死 |
| P24 | 手写 shader 组合文件 | PS 组合存在但代码没实现 | `fxctmp9/*.inc` 是手写替代 Valve 的生成器 | 补齐模糊/降采样变体时要同步改 `.inc` 与 `panorama_*_dx9.cpp` |

## D. 文本与字体

| # | 坑 | 症状 | 根因 | 解法 / 关联 |
|---|---|---|---|---|
| P25 | 字体 alpha 图集**部分上传** | 字形"每 4 行才有一条横杠"般破碎 | 用了 `ITexture::Download(&rect)` 只推了一条 6 行片段进显存 | 改成整图 `TexImage2D`（`b9e0a823`，`SEUploadPanoramaAlphaAtlas`） |
| P26 | `m_bIsAlphaTexture` 被强置 `false` | 文字颜色发暗 | 端口早期诊断把它写死了 | 恢复 CS:GO 的 `true`（`b9e0a823`） |
| P27 | 字号看起来小 2/3 | 以为缩放算错 | **不是 bug**：`CStylePropertyFont::ApplyUIScaleFactor()` 是 CS:GO 自身设计（CSS 按 1080 空间书写） | 不要"修"它；调 `ui_scale` 才是正道 |
| P28 | **字体格式认错** | 按 `.uifont`（protobuf + OpenSSL AES）规划了半天 | CS:GO **游戏内容**用的是 `.vfont`（Valve Font 容器 = SimpleCodec 异或过的 TTF + `VFONT1` 尾标） | 用 `common/valvefont.h` + `common/simplecodec.h`，**不需要 OpenSSL**（`5ffcd3d4`） |
| P29 | 字体族名对不上 | 设了 `font-family: Stratum2` 还是回退 | DirectWrite 用 nameID 16（typographic family） | `stratum2*` 的 typographic family 才是 `Stratum2`；`notosans` → `Noto Sans`、`notomono` → `Noto Mono` |
| P30 | 字体包必须散文件 | 打包装完字体不生效 | 装载器走绝对路径扫目录 | 33 个 `.vfont` 放 `<mod>\panorama\fonts\`（74.6 MB，**别进 git**，用 `build\_stage_fonts.ps1`） |
| P31 | `CDirIterator` 在本树不存在 | 编不过 | CS:GO 把它声明在 `public/tier1/fileio.h`（6272 B），本树是 SE 版（2632 B，没有它） | 写了 `panorama/seport/se_diriterator.{h,cpp}`（Win32 FindFirstFileA），**不动 `public/`** |
| P32 | `LoadFileIntoBuffer()` 是个 Steam 客户端全局 | 链接/编译缺符号 | `uifontfileloaderwin32.cpp` 依赖 Steam 客户端版才有的自由函数 | 换成 `g_pFullFileSystem->ReadFile()`（路径此时已是绝对路径） |

## E. 输入与交互

| # | 坑 | 症状 | 根因 | 解法 / 关联 |
|---|---|---|---|---|
| P33 | **启动参数带控制台** | "菜单点不动 / Esc 没反应"，看起来像 UI 坏了 | `-console`/`-toconsole`/`-dev`/`-rpt` 会让 VGUI 控制台在启动时可见，而 panorama 的输入分发在 `Con_IsVisible()` 时**直接 return** | 干净启动（见 `run_css.bat`）；这条坑最贵，排查过一轮 |
| P34 | VGUI GameUI 吃掉点击 | 主菜单看不见点击响应 | 默认 VGUI gameui 可见并接收输入 | 默认隐藏 VGUI gameui（`e5390e21`、`fb934e63`） |
| P35 | Esc 弹出 VGUI2 菜单 | 按 Esc 出现 CS:S 菜单 | `keys.cpp` 无条件先喂 VGUI，`CEngineVGui::Key_Event()` 把 Esc 变成 `gameui_activate` | 照 CS:GO 加 `IsESC()` 特判 + panorama 优先（`b7f0720e`，任务 A） |
| P36 | `` ` `` 打开控制台却挂在 GameUI 上 | 按 `` ` `` 连带弹出 VGUI 菜单 | `CEngineVGui::ShowConsole()` 开头会 `ActivateGameUI()` | panorama 激活时跳过它，把控制台重新挂到 `staticPanel`（`b7f0720e`） |
| P37 | 键盘注入不可靠 | 用 `keybd_event` 验证按键得到乱结果 | 扫描码 0 → `data=0`；且本机把 `VK_OEM_3` 映射成 `KEY_ESCAPE` | 按键行为验证**请人按**，或用探针记录 `INPUT type/data/consumed` |
| P38 | 命令行 `+convar` 设不进去 | 想用 `+@panorama_disable_blur 1` 做 A/B，探针显示仍是 0 | `+xxx` 在启动早期执行，那时 panorama 还没注册这个 convar | 只能改默认值重编，或进游戏后在控制台设 |
| P39 | 残留游戏进程 | 弹 "Only one instance of the game can be running at one time."，且截图为 0x0/199x34 | 上次进程没退干净 | 启动前杀 `hl2|cstrike|source|launcher`；截图前等窗口宽度 ≥1000 再拍 |

## F. 工具链 / 会话（会拖慢一切的那些）

| # | 坑 | 症状 | 根因 | 解法 / 关联 |
|---|---|---|---|---|
| P40 | 交互式 PowerShell 反复退化 | `^U` 乱码注入、管道吞输出、`Get-Content/Select-String` 找不到、命令"无输出" | 会话/PSReadLine 状态坏了 | 一律 `.ps1` 脚本 + `powershell -NoProfile -ExecutionPolicy Bypass -File`；必要时 `create_and_run_task` 开新终端 |
| P41 | **PS 5.1 把无 BOM 的 `.ps1` 按 ANSI(GBK) 读** | 脚本里的中文路径/字符串全烂（踩两次：打包目录名 `打包`、提交信息） | PS 5.1 默认编码 | 非 ASCII 内容**别写在 .ps1 里**：用编辑器写 UTF-8 文件，脚本 `[IO.File]::ReadAllText(..., UTF8)`；或按名字形状找对象 |
| P42 | git 输出的"字节校验"是假象 | 以为提交信息编码坏了，其实没坏 | PS 用控制台码页(GBK)解码 git 的 UTF-8 输出 | `cmd /c "git ... > file"` 让 cmd 直接落盘，再读**原始字节**：`[IO.File]::ReadAllBytes` |
| P43 | 提交信息中文 | 提交进仓库变成乱码 | 同上，`git commit -m "中文"` 也过 PS | 编辑器写 UTF-8 文件 + `git commit --amend -F file`，再按 P42 验证 |
| P44 | git 必须 `-C` | 在别的 cwd 下 git 操作错仓库 | 终端 cwd 会漂移 | 一律 `git -C d:\source-engine` |
| P45 | 探针文件跨 run 累积 | 误把上一轮的探针输出当成这一轮的 | `se_*_probe.txt` 是**追加**写；`engine.log` 每轮重建 | 每轮先删日志；认准当次 run 的文件与时间戳 |
| P46 | 探针混进提交 | 仓库里有 `SE_PORT_BLUR*` 等 bring-up 探针 | 排查完忘了清 | 收尾统一清理（见 checklist Phase 5） |
| P47 | `create_and_run_task` 改 `.vscode/tasks.json` | 提交时多出一个不想要的改动 | 任务运行器行为 | 提交前 `git checkout -- .vscode/tasks.json` |
| P48 | 用了"下载来的 OpenSSL"当唯一路子 | 差点引入一整个加密库 | 本机没有可链接的 OpenSSL（`D:\CSGO2019\external\openssl-1.0.1e` 只有头）；网络当时不通 | 先证伪需求（P28）→ 结论是根本不需要 |

## G. 2026-09-16 追加（泛白 / 紫黑格 / 弹窗那一轮）

| # | 坑 | 症状 | 根因 | 解法 / 关联 |
|---|---|---|---|---|
| P49 | **CSS 命名颜色在本端口全透明** | 通用弹窗（`CUI_Popup_Generic` 运行时 new 的 CButton+CLabel）按钮只有深色胶囊、**没有文字**；凡内容里用 `White`/`black`/`disabledColor` 等命名颜色的地方都不对 | `panorama/layout/csshelpers.cpp::BParseNamedColor()` 的颜色表是照 CS:GO 的 `Color` 写的：CS:GO 的 `Color(r,g,b)` alpha 默认 **255**（`D:\CSGO2019\public\color.h`），本树编译的是 Source 1 的 `public/Color.h`（alpha 默认 **0**）⇒ 表里 148 个命名颜色全透明。弹窗按钮文字唯一的颜色来源就是 csgostyles.css:1437 的 `color: White` | 命中后除 `transparent` 外补 `SetColor(r(),g(),b(),255)`（`74001266`）。判据：`CLabel::Paint` 探针打出 `color=255,255,255,0`（排版正常、字形在，只是 alpha=0） |
| P50 | **改 `D:\cstrike` 里的内容 CSS 不生效** | 往 `styles/popups/popup_generic.css` 加"红底 + 大字号"规则做验证，重启后毫无变化（差点误判成"样式没应用"） | 布局/样式是从打包的 `panorama/code.pbin` 读的，**松散文件不参与** | 别用"改内容 CSS"验证；要验证就改代码/加探针（或看 `CLabel::Paint` 的输出） |
| P51 | **`CSGOMainMenu` 面板被构造两次** | 主菜单叠出两个一模一样的弹窗：点一下只关掉被压在下面那个 ⇒ 看起来像"弹窗点不动"，Esc 也要按两次 | `base_mainmenu.xml` 有一层 `<CSGOMainMenu id="MainMenu">`，`mainmenu.xml:57` 又嵌了一层 `<CSGOMainMenu class="MainMenuRootPanel">`；panorama 面板事件沿父链冒泡 ⇒ 第二份实例重复注册同一批事件处理 ⇒ `mainmenu.js::_OnShowMainMenu()` 跑两次 | 只让外层实例（`s_pMainMenu == NULL` 那次）注册事件/跑状态机，内层设完初值立即返回（`5e46e183`）。验证：弹窗在屏时面板像素 15075 → 一次 Enter 后 0 |
| P52 | **`_rt_FullFrameFB2` 是 32x32 占位 RT**（**紫黑格真因**） | 打开模糊那几趟后，主菜单背景出现紫黑缺材质格；**只覆盖模糊矩形那一块**（新闻/内容面板位置），其余是泛白的视频 | Source 1 只把 `_rt_FullFrameFB` 建到屏幕尺寸；`_rt_FullFrameFB2` 被 `materials->FindTexture(name, TEXTURE_GROUP_RENDER_TARGET)` 顺手造了一张 **32x32** 的占位 RT（探针实测 `size=32x32 format=16`）。而 `ApplyGaussianBlur`/`ApplyFastGaussianBlur` 把 index 0/1 **都当 framebuffer 尺寸**用（texel 偏移、`ATTR_UVClamp`、`DownSize` 窗口、`m_flRTW/H` 全按它算）⇒ 采样越界 ⇒ 引擎回退 error texture ⇒ 紫黑格 | `S1Wrapper_FindFullFrameBuffer()` 里发现引擎给的那张小于 backbuffer 尺寸时，自建 `_se_panorama_scratch<n>`（同尺寸、用 `_rt_FullFrameFB` 的格式、`MATERIAL_RT_DEPTH_NONE`）+ Warning。探针：`CRenderAttributes::SetTextureValue` 里打印最终进 sampler 的 `ITexture*`（名字/尺寸）。**遗留**：紫格消失，但模糊结果仍偏白（背景变均匀浅灰），待继续查 |
| P53 | **`HRenderTexture::GetResourceHandle()` 不能当纹理身份** | 用它比对"是不是同一张纹理"得出自相矛盾的结论（据此误判"layer RT 与 scratch 不冲突"） | 每次调用可能包一个新的 wrapper，地址会变 | 要比就比底层 `ITexture*`，或直接打 `ITexture::GetName()` |
| P54 | **用整屏平均亮度判"泛白/累加"是错的** | 一度以为"每帧累加 +60 亮度"，并据此下结论 | 那个值跟着**视频画面**的明暗走（同一脚本两轮：198→203.5 与 213.7→200.4） | 靠**看截图** + 具体探针；亮度只能当粗筛 |
| P55 | **"关掉背景视频"的 A/B 会截到 199x34 报错框** | 整轮 A/B 无效，还以为"关掉视频就全黑" | 上个进程没退干净 ⇒ 第二实例报错框（同 P39）；裁剪图还是白的 | 启动前杀 `hl2|cstrike|source|launcher`；截图前校验窗口 ≥1000 宽（窗口发现在 0s 就要怀疑是残留窗口） |
| P56 | **"算法逐字一致" ⇒ 问题在更底层的封装** | 反复读模糊 C++ 代码找不到差异 | `ApplyFastGaussianBlur` / `DownSize` / `DrawDownSizeRTtoRT` / `DrawUpSizeRTtoRT` 与 CS:GO **逐字一致**（还有 `panorama_ps30.fxc` 的模糊分支、组合公式 `1/9/18/36`、`g_flNumBlurPixelsPerSide=4` 也都一致，唯一差别是端口为绕开 d3dx9 "循环里不能 texld" 手工展开了 taps，算术等价） | 两步定位法：**① 函数级 diff**（`build/_blurdiff.ps1` 抠同名函数做 `Compare-Object`）先排除"抄错"；**② 短路二分**（把某一段直接 `return`/注释掉）看现象出现在哪一步。两者合起来才定位到 P52；也说明 shader/算法那层是清白的 |

## H. 2026-09-16 追加二（模糊收尾）

| # | 坑 | 症状 | 根因 | 解法 / 关联 |
|---|---|---|---|---|
| P57 | **blurrects 的 id 查找用错了根**（模糊结果取错源） | 菜单背景模糊出来是"均匀浅灰"，不是模糊的视频 | `panorama/seport/gameclient/csgo_blurtarget.cpp` 自写了 `SE_PortFindRootPanel()`（爬到**整棵树最顶**），CS:GO 用的是 `CUI_Root::GetRootForWindow( GetParentWindow() )->FindChildTraverse()` ⇒ 爬到更高层后匹配到**别的同名面板或错矩形** | 改成 CS:GO 那套（端口早有 `CUI_Root::GetRootForWindow`，`ui_root.cpp:115`，popup/contextmenu/tooltip 三个管理器都在用；旧注释"端口没有"是过期的）。效果：背景变回**模糊的视频** |
| P58 | `ui_root.h` 的 include 顺序 | `error C2504: CGameEventListener` / `FireGameEvent override` | `ui_root.h` 依赖 game-client 公共头 | 照 `ui_popup_manager.cpp` 的顺序：先 `panorama/se_gameclient_common.h`，再 `panorama/ui_root.h`；这个文件里**不要**用 `stdafx_client.h` |
| P59 | `FindChildTraverse()` 返回值 | `error C2664: ToPanel2D` 参数不匹配 | `CUI_Root::FindChildTraverse()` 返回的就是 `CPanel2D*` | 不要套 `ToPanel2D()`（CS:GO 也是直接用） |
| P60 | **外部 AI 给的 API 名先 grep 源码** | 附件里说"Valve Panorama CSS 有 `mipmapgaussian()`，`mainmenu.css` 就用了" ⇒ 差点又开一条错线 | 实测：端口 + CS:GO 源码 + 全内容 styles **各 0 处**；`mainmenu.css` 用的是 `blur: fastgaussian( 8,8,5 )` 等，端口**已实现** | 任何"应该用 XXX"的建议先三处 grep：端口 / CS:GO 源码 / 内容，再动手 |
| P61 | 内容里的"SE port hack"多半是**死代码** | `mainmenu.css:1243-1251` 写着"RT based blur / mix-blend-mode 不适用，关掉"并 `blur: none`，但探针显示那些面板模糊**照旧在跑** | 部署内容走打包的 `code.pbin`，松散文件不参与（P50） | 看到这类 hack 别当成现状依据；要么确认它真生效，要么删掉（现在是误导源）。另外 `mix-blend-mode` 端口其实**是齐的**（`styleproperties.cpp:448` / `styles.cpp:1719` / `uianimationengine` / `uipanel.cpp` / `source2surface.cpp` 的 Screen/Multiply/Additive/SRGBadditive/Opaque 分支与 CS:GO 逐行对得上） |
| P62 | 逐函数对拍的细节 | 一次性 `Compare-Object` 比“函数级对拍”看不出真差异 | 探针行把两个文件搞乱，对不齐 | 用 `build/_rendercmp.ps1`：花括号匹配抠出函数体 → **先滤掉探针行**（`s_nSE|TEMPORARY|SE port (\(|BLURAPPLY|...|fprintf|fclose`）→ 再 `git diff --no-index`。模糊路径 15 个函数里 9 个 0 差异，其余只差两个**默认关闭**的 `SE_PortBackdropBlit()` 块 |

## I. 2026-09-17 追加三（模糊：**根因未找到，挂起归档**）

用户决定：**关掉模糊，把"背景模糊（blurrects）为什么不生效"当成"暂时查不出来的 bug"存档，以后再修。**
关闭方式：`panorama/source2/renderer/source2surface.cpp` 的 `SE_PortSupportsBlurPasses()` 固定返回
`false`（函数名上标注 PARKED）。它只关掉 `PopCompositingLayer()` 里的 blur pass 分支，其余一切不变；
要重新开工把返回值改回 `true` 即可，`@panorama_disable_blur` 这个 convar 仍然在。

| # | 项 | 结论 |
|---|---|---|
| P63 | **关掉模糊后的两种实测结果**（`build/_bluroff_verify3.ps1`，1280x720 窗口直抓） | ① **背景视频在** ⇒ 背景全黑（整窗 mean 35.3）：视频画面**只在 blur pass 那条路径上被画出来**，关掉 pass 就没有任何东西把视频合成到屏幕上。② **背景视频关掉**（默认静态背景）⇒ 背景图正常出现、UI 布局完整（侧边栏 / 新闻面板 / 右上"重新连接·放弃"都在），但**仍然泛白**（整窗 mean 203.6，>200 像素 65.6%；新闻面板区 199.2 / 视频区 227.5）⇒ **泛白不是模糊造成的**，是"原始背景太亮 + 没有任何压暗"（CS:GO 靠 `CSGOBlurTarget` 的模糊+压暗遮住） |
| P64 | **已经排除的原因**（都验过了，别再重复走） | 15 个模糊/合成函数与 CS:GO 逐字（P56/P62）；`panorama_ps30.fxc` 模糊分支一致；`_rt_FullFrameFB2` 32x32 占位 RT 已用自建 scratch RT 顶掉（P52）；scratch RT 尺寸已修；sampler 绑定的 `ITexture*` 已探针确认（P53）；blurrects 的 id 查找已换成 CS:GO 那套（P57）；`blur` 属性解析正常（`SE_PORT_BLURSTR: in='fastgaussian( 8, 8, 5 )' -> passes=5 stddev=8/8`）；per-layer RT（`PushCompositingLayer` / `ActivateRenderTargetAndClear`）已在 `44c35c1e` 落地 |
| P65 | **剩下的疑点（下次从这接）** | ① blur pass 里 `SetupBlurPanelAttr()` 交给像素着色器的那组 **blur rect（`ATTR_*BlurRect`）与采样 UV 是否落在 layer RT 的正确区域**——"逐字一样"的函数吃到的输入不同，只可能是这里；② `ApplyFastGaussianBlur` 取源纹理用的是 **layer 自己的 RT** 还是**父 RT**（CS:GO 是前者，端口在"直画父目标"改造后可能仍是后者）；③ 打开 pass 时出现的是**贴图包装器的 error texture**（紫黑格/纯白），说明**源纹理句柄本身是无效的**——先抓"进 sampler 的到底是什么纹理、尺寸多少、是否 error"，比继续读算法更快 |

**度量口径提醒**（P54）：整屏平均亮度会被背景画面明暗带着走，只能当粗筛；判"这一笔画了什么"要靠
`build/_draw_*` 那份 DRAW 探针栈 + 截图。


## J. 2026-09-17 追加四（玩家头像 / 玩家卡片）

用户报的症状是"右上角该放头像的地方是个感叹号 / 头像没移植"。查完之后：**头像面板、读文件、解码、上纹理
全是好的**——坏的是"卡片整张被内容藏了"。这一节的教训主要不在渲染，而在**数据桩把内容推到了错误的分支**。

| # | 项 | 结论 |
|---|---|---|
| P66 | **本地玩家卡片被隐藏的真因 = 桩对象 truthy 让内容以为"玩家在队伍里"** | 代码路径：`party.js::_IsSessionActive()` 问 `LobbyAPI.IsSessionActive()`（占位对象 = truthy）⇒ 认为有大厅 ⇒ `_RefreshPartyMembers()` 继续 ⇒ `PartyListAPI.GetCount()` 也是对象 ⇒ `numPlayersActuallyInParty >= PartyListAPI.GetPartySessionUiThreshold()`（对象比较，两边都退化成 0）成立 ⇒ `_UpdateMembersList()`：`$('#PartyList').RemoveClass('hidden')` + **`friendsList.HideLocalPlayer( true )`** ⇒ `elLocalPlayer.SetHasClass('hidden', true)`，CSS `.player-card.hidden { opacity:0; visibility:collapse; }` ⇒ **`JsLocalPlayercard` 整张塌掉**，里面的头像永不布局/不绘制。可见症状：右上角是**队伍列表**（两个占位头像块 + `[]/5`），没有玩家卡片。<br>**修法（纯数据，改 shim 即可，不用重编）**：答 `LobbyAPI.IsSessionActive → false`（关键——`_IsSessionActive()` 的 early-out 里**自己会调 `HideLocalPlayer( false )` 把卡片放回来**）、`LobbyAPI.BIsHost → false`、`LobbyAPI.GetHostSteamID → ""`、`PartyListAPI.GetCount → 1`、`GetPartySessionUiThreshold → 5`、`GetFriendIsTalking → false`、`SessionUtil.GetMaxLobbySlotsForGameMode → 5`。改完 `PartyList` 与 `[]/5` 一起消失、卡片回来 |
| P67 | **别只看"哪块没显示"，先分清"是没请求 / 没加载 / 没布局 / 没绘制"** | 三条证据各管一层，缺哪条都容易误判：<br>① `SE_AVPROBE: url='...' idValid=1`（`csgo_avatarimage.cpp`）⇒ **解析对了**；<br>② `SE_AVPROBE: image loaded 'file://{images}/avatars/76561198000000001.png'` ⇒ **图确实加载成功**（到这一行还看不到东西，就不要再查读文件/解码了）；<br>③ `se_ui_probe.txt` 的 `MENUTREE`（`SE_PortDumpPanelTree`，`engine/panoramaenginehandler.cpp:508`）显示 `JsLocalPlayercard visible=0` ⇒ **是"没布局"**，问题在可见性而不是渲染。<br>注：这份 MENUTREE 的 `w/h` 在端口里一直是 0（不可信），但 `visible` / 结构 / `ch=` 可信，够定位层级 |
| P68 | **头像读文件的正确姿势（脱离 Steam 的本构建）** | 按序找 `{images}/avatars/<steamid64>.{png,jpg,svg}` → `<accountid>.*` → `local.*` → 布局里的 `defaultsrc`；`{images}` = `materials/panorama/images`（`panorama.cfg`），所以文件放 `<mod>/materials/panorama/images/avatars/`（该目录内容自带 15 张 `avatar_sub_*_small.vtf`）。<br>存在性判断**不要**只信 `GetLocalPathForRelativePath()` 的返回值——端口里它给出的形式会让 `UIFileSystem()->FileExists()` 答 false（症状：永远回落到布局默认图）。现在同时试三种：`GetLocalPathForNamedPath("{images}") + /relative`（正斜杠）、同一路径改反斜杠、`g_pFullFileSystem->FileExists(name, "GAME")`。<br>补充事实：panorama 会把 `.png` 改写为 `_png.vtex`（`imageloader.cpp::FixupFileResourceToCompiledImage`），而 s1wrapper 的 `RestoreContentFileExtension`（`wrap_resource.cpp:63`）会在读盘前还原 —— 所以**松散 png 是能读的**（`avatars/local.png` 实测加载成功） |
| P69 | 零碎但会误导的几条 | ① `avatar.xml` 的头像槽是 `<Button class="avatar" acceptsinput="false">`，真正挂点击的是**外层** `JsPlayerCardAvatar`（`friendslist.js::_AddOpenPlayerCardAction`）；`FindChildInLayoutFile` 只跳"嵌套 layout 的 loader"（`m_bLoadedLayoutFile`），运行时面板照样能找到，所以点不动时要往"卡片有没有被藏"上想，不是先怀疑命中测试（`button.cpp` 与 CS:GO 逐字节相同，激活/冒泡没被端口改过）。② 队伍颜色三角来自 `PartyListAPI.GetPartyMemberSetting(xuid,'game/teamcolor')`，答数字（1..4）会 wash 成对应颜色——这就是那两个占位头像变**黄色**的原因。③ 用户自己加的探针也可能只写了一半（本轮 `csgo_avatarimage.cpp` 里就有一句没写完的 `Warning(`，直接编译不过），本地改动先 `git diff`/编译一次再跑 |


