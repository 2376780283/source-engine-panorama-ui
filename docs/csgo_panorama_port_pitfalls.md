# CS:GO Panorama 移植 —— 踩坑表

> 配套：`csgo_panorama_port_checklist.md`（任务清单）、`csgo_panorama_port_breakthroughs.md`（重大突破）、
> `csgo_panorama_port_plan.md`（计划）、`CSGO_PANORAMA_PORT_NOTES.md`（实操速查）。
>
> 每条尽量给出：**症状 → 根因 → 解法 → 关联**。P 编号可被其他文档引用。
> 更新：2026-09-19（N 节：泛白破案 P101–P104）

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

---

## K. 2026-09-17 追加五（匹配 / 排位：**“JS 异常探针”这套排查法**）

用户报的是“PLAY 页点模式没反应：下划线动了、地图列表不动、‘开始’/‘取消’死了”。最后真凶是
`mainmenu_play.js:1323` 把 **`SteamOverlayAPI.IsEnabled()`（桩对象）塞进 `.enabled` 的 bool 槽**——JS 一抛，
**该函数之后的执行链全断**（地图列表填充就在后面）。这一节记的是**方法**：手动启动看不到控制台，
所有证据都来自探针文件 `D:\cstrike\se_ui_probe.txt`。

| # | 项 | 结论 |
|---|---|---|
| P70 | **Panorama 的 JS 异常 = 静默中止（“点了没反应”的第一嫌疑）** | V8 抛异常后**同一脚本剩余部分全部不执行**（控制台只留一行 `*** Skipping rest of script ***`，手动启动连这行都没有）。所以“点击后 UI 停在半成品”要**先找这次点击后的第一条 JSEXC**，不要从 UI 现象反推。本例异常点 = `mainmenu_play.js:1323`，被它带走的 = `_ShowActiveMapSelectionTab` 之后的**全部**地图列表/按钮状态刷新 |
| P71 | **探针 = 手动测试的唯一证据通道** | 手动启动（无 `-condebug`）时控制台日志拿不到。现有三类探针都写 `D:\cstrike\se_ui_probe.txt`：<br>① `panorama/uiengine.cpp::OutputJSExceptionToConsole` → `JSEXC #N` + `文件:行:列` + `>> 出错源码行` + **完整 JS 堆栈**（每次启动从 #1 重编号，可据此把文件切成一轮一轮）；<br>② `uicomponent_gameinterface.cpp::SetSettingString` → `SETTING key = value`（判“这次点击有没有走到设置写入”）；<br>③ 图片 `IMG/IMGF/IMGPAINT`、合成层 `SE_LAYER`、面板树 `MENUTREE`。<br>读法：`build\se_run.cmd _scan_log.ps1 D:\cstrike\se_ui_probe.txt <pattern> <条数>` → 结果写 `build/_logcheck.txt`，再按行号读 |
| P72 | **`([])` 判据：桩对象漏进了类型槽** | 异常文本 `V8ParamToPanoramaType expected bool type to convert, but got something else ([])` = 某个 API 返回了**占位桩**而落点是 bool/数字/字符串。`([])` 就是桩的 `Symbol.toPrimitive` 输出（`se_api_shim.js::makeStub` 把字符串化设成 `"[]"`、数字设成 0）。<br>**修法 = 在 `se_api_shim.js` 里 `seDefine( "<Api>.<方法>", function(){ return <正确类型>; } )`**（false / 0 / "" / 逗号串），不是“看起来合理”的桩。<br>命名约定：`Is*/B*/Has*/Can*/Should*` ⇒ bool；`Get*Count/Length/Size` ⇒ 数字；`Get*Name/Id/url` ⇒ 字符串 |
| P73 | **分流技巧：用 SETTING 判“点击有没有生效”** | `Select-String build/_logcheck.txt -Pattern 'ui_playsettings_mode_official = (\S+)' \| Group-Object` 一眼看出用户点过哪些模式。修复前统计：`competitive 162 / casual 0` ⇒ **点“休闲”从没写进设置** ⇒ 断在“点击→设置写入”，不用去查 UI 刷新；修复后同一统计出现 `casual 8 / scrimcomp2v2 12 / deathmatch 2 / skirmish 6 / survival 10` |
| P74 | **修复闭环：看资源加载行，不看截图** | 修 JS → `build\se_run.cmd _deploy_se_js.ps1`（**只拷 3 个 JS，不用重编**）→ 用户重测 → 重扫探针。行为证据：<br>① 休闲 18 张 `map_icon_*.vsvg` 全被加载（含只有休闲才有的 `cs_militia / cs_italy / cs_assault / de_canals`）⇒ **地图列表确实随模式刷新了**；<br>② `MF loaded '.../videos/search.webm' 910x512 dur=10010ms` + `skillgroup10.vsvg` ⇒ **“正在寻找比赛”界面真的建起来了**。<br>另：`ui_playsettings_maps_official_casual = mg_casualsigma` 是**中间态**（先写地图组名、内容随后展开成 18 张图），不是 bug |
| P75 | 本轮改的 6 处（全在 shim/sim）+ 3 处待修 | ① `SteamOverlayAPI.IsEnabled → false`（**真凶**，bool 槽）；② `InventoryAPI.IsCraftReady → false`；③ `GameStateAPI.IsReportCategoryEnabledForSelectedPlayer → false`；④ `GameTypesAPI.GetMapGroupAttributeSubKeys` 改回**逗号串**（内容 `tooltip_lobby_settings.js:237` 要 `.split(',')`，给数组会抛）；⑤ 新补 `$.LocalizeSafe`（**引擎根本没这个方法**：模式详情弹窗 `popups/popup_play_gamemodeflags.js:8`、WATCH 赛事表 `mainmenu_watch_eventsched.js:410`、`commonutil.js` 的国家/语言名都靠它）；⑥ `GameStateAPI.IsQueuedMatchmaking[Mode_Team] → false`（`mainmenu.js:345` 暂停菜单导航栏的 bool 槽）。<br>待修（同一天探针里的新 JSEXC）：`mainmenu_inventory_search.js:60`（`GetSelected()` 为 null ⇒ 库存页列表断）；`mainmenu_watch.js:289`（赛事选手表越界）；`operation_mission.js:297`（`missionGameMode` 收桩，行动页/HUD 任务用，本端口走不到） |
| P76 | **探针文件的生命周期** | 探针是**追加写**、跨轮累积（本轮 3.7 MB / 3.2 万行）；`_deploy_all.ps1` 会清空它，`_deploy_se_js.ps1` **不会**。让用户重测前先看文件大小/修改时间，或直接按 `JSEXC #1` 分段（每次游戏启动重新从 #1 编号） |


## L. 2026-09-18 追加五（UI 声音移植）

用户要求“先移植声音和 tooltip”。**声音**这一侧静态链路有 3 处断点，全部修掉（见 P77–P83）；
**tooltip** 静态链路本来就已经完整（见 P84），等实机验证。

| # | 项 | 结论 |
|---|---|---|
| P77 | **UI 声音的完整链路（CS:GO 名字 → 脚本 → 波形 → 引擎）** | 内容：`$.DispatchEvent('PlaySoundEffect', 'UIPanorama.generic_button_press', 'MOUSE')` → `panorama/uisoundsystem.cpp::OnPlaySoundEffect` → `PlaySound(name, panel, k_ESoundType_Effects, 1.0f, 0.5f, 1.0f)`。CS:GO 的播放走 **sound ops**：`g_pSoundOpSystem`（= `panorama_s1wrapper/wrap_sound.cpp::CSoundOpSystem`）→ `ISoundEmitterSystemBase::GetSoundIndex/GetParametersForSoundEx`（解析 `scripts/game_sounds_ui_panorama.txt` 得到 wave）→ `IEngineSound::EmitSound(...)`。上游参照实现是 `panorama/source2/uisoundsystemsource2.cpp::PlaySound`（144–169 行） |
| P78 | **两个“声明了、定义了、但从未赋值”的全局** | `wrap_sound.cpp` 的 `g_pEnginesound` / `g_pSoundEmitterSystemBase` 一直是 NULL ⇒ `StartSoundEvent` 在 NULL 检查处直接 `return 0`，UI 全静音。修法 = `panoramauiclient/panoramauiclient.cpp::Connect()` 从 app system factory 取（同 gameuifuncs/gameeventmanager 的路子）。**接口版本别信 `public/interfaces/interfaces.h`**：它的 `SOUNDEMITTERSYSTEM_INTERFACE_VERSION = "VSoundEmitter003"` 是过期的；本树真正发布的是 **`VSoundEmitter002`**（`public/SoundEmitterSystem/isoundemittersystembase.h:21`，实测 DLL 字节里也只有 002）。引擎侧 = **`IEngineSoundClient003`**（`engine/EngineSoundClient.cpp` 的 EXPOSE）。验证小技巧：把界面字符串当 needle 从 DLL 里挖（`_sc1.ps1` 那种写法） |
| P79 | **接口可能“晚到”，Connect 里取一次不够** | `soundemittersystem.dll` 是由 **game client app system group** 加载的（`game/client/cdll_client_int.cpp:444 AddAppSystem "soundemittersystem"`），panorama 自己的 `Connect()` 可能比它早 ⇒ 那次 factory 查询会拿到 NULL。修法 = 把 factory 存进 `g_pPanoramaConnectFactory`（wrap_sound.cpp 定义、panoramauiclient.cpp 赋值），`StartSoundEvent` 首次播放时**再试一次** |
| P80 | **`SUPPORTS_AUDIO` 在本移植里根本没定义** | 所以 `CUISoundSystem::PlaySound` 里的音频设备分支（`CreateAudioSound/Play/SetVolumePan`）**整段被编译掉**：命中的 wav/mp3 文件也会直接 `return NULL`。UI 声音唯一能走的路径就是 sound ops 回退（P78），故 `PlaySound` 末尾加了镜像 `CUISoundSystemSource2` 的回退块（`g_pSoundOpSystem->StartSoundEvent(...)`，音量走 `GetSoundVolume()`，返回 guid 编码成 `HAUDIOSAMPLE`） |
| P81 | **内容侧清单是占位 = 整份脚本从未被解析** | `D:\cstrike\cstrike\scripts\game_sounds_manifest.txt` 之前是“placeholder / no sound emitter scripts”，于是 `game_sounds_ui_panorama.txt` 从来没进过 sound emitter 数据库，`GetSoundIndex("UIPanorama.*")` 必然失败。manifest **只认 `precache_file` / `preload_file` / `faceposer_file`** 三种子键（`soundemittersystem/soundemittersystembase.cpp:205`），别的键只会打 bogus file type 警告。repo 源在 `mods/panorama_test/scripts/`，已让 `_deploy_se_js.ps1` 一并部署 |
| P82 | **波形文件必须实际存在** | 脚本引用 `sound/ui/panorama/<name>.wav`，但游戏目录里没有（`D:\se` 才是素材树，110 个文件）。`build\_deploy_ui_sounds.ps1`（只补缺失，可重复跑）把 `D:\se\sound\ui\panorama` → `D:\cstrike\cstrike\sound\ui\panorama`；**首次部署后删掉 `cstrike\sound\sound.cache`** 让引擎重新扫描（否则可能沿用旧目录索引） |
| P83 | **CS:GO 的波形前缀 `~` / `+`（本树引擎不认识）** | CS:GO 的 `public/soundchars.h` 把 `~`=CHAR_HRTF、`+`=CHAR_RADIO 也算 sound chars（`PSkipSoundChars` 会跳过），它的 UI 脚本里大量 `"~+UI/panorama/..."`、`"+UI\panorama\..."`（还混着反斜杠）；**本树的 `soundchars.h` 没有这两个字符**，引擎按原字符串查磁盘必然找不到。修法 = `wrap_sound.cpp` 交给 `EmitSound` 之前自己剥前缀（`while(*p=='~'||*p=='+')++p`）+ `V_FixSlashes`。远期正解是照 CS:GO 把 soundchars.h 的两个字符补上（要重编 engine.dll，本轮先不动） |
| P84 | **tooltip 链路静态核对（本轮没发现缺环）** | ① 工厂：`REGISTER_PANEL2D_FACTORY(CCSGO_UI_TooltipManager, CSGOTooltipManager)`，且 `csgo_ui_tooltip_manager.cpp` 与 5 个 tooltips/*.cpp 都在 `panoramauiclient/wscript`（64–71 行）；② 内容：`mainmenu.xml:321 <CSGOTooltipManager id="TooltipManager" hittest="false"/>` + `csgo_mainmenu.cpp:147 SetTooltipManager(...)`；③ 布局：`tooltips/tooltip_{base,text,title_text,title_image_text}.xml` 全在游戏目录；④ JS：内容是 `UiToolkitAPI.ShowTextTooltip(id/text…)` 传**字符串 ID**，port 的 `uicomponent_uitoolkit.h` 与原版 **逐行一致**（对比过 D:\CSGO2019）。⇒ 若实机仍无 tooltip，下一个怀疑点是运行时判定：`CUI_TooltipManager::ShouldHandleTooltipEvent`（窗口可见性 / 是否正在拖拽）与 `EnsureTooltip` 的布局加载日志 |
| P85 | **本会话环境坑（复述 + 一条新经验）** | 终端仍会间歇性“Command produced no output”/`Get-ChildItem` 找不到（PATH 坏了）：用 **VS Code 任务**（`SE full build` / `SE deploy`）最稳；非要跑脚本就先让脚本把结果 `Set-Content` 到 `build\_*.txt` 再 `read_file`。构建后想确认“我的改动真的进 DLL 了”，用**界面字符串/版本串**当 needle 挖二进制，比看时间戳可靠 |
| P86 | **改内容 manifest 没用！本安装的 `scripts/game_sounds_manifest.txt` 读到的是冻结/VPK 原版** | 探针 `SNDOP db/script[i]` 显示数据库里是 `scripts\game_sounds_hostages.txt` 等 9 个 CS:S 原版脚本（反斜杠=原版文件），**不是**我们部署到 `D:\cstrike\cstrike\scripts\` 的松散文件——所以“把 `game_sounds_ui_panorama.txt` 写进 mod manifest”完全无效。**正解：运行时直接调接口** `ISoundEmitterSystemBase::AddSoundOverrides( "scripts/game_sounds_ui_panorama.txt" )`（→ `AddSoundsFromFile(..., bIsOverride=true)`），实测 `sounds 840→997、scripts 9→10`，随后 `UIPanorama.*` 全部可查。写在 `wrap_sound.cpp` 接口解析之后：`GetSoundIndex("UIPanorama.generic_button_press") < 0` 就补一次（幂等） |
| P87 | **`ModInit()` 是谁调的、菜单里靠不靠得住** | 声音脚本库的装载入口是 `ModInit()`，Source 2013 里由**游戏侧** `CSoundEmitterSystem` game system 在关卡/系统初始化时调用（`game/shared/SoundEmitterSystem.cpp:227`）。本进程确实被调过（scripts=9），但**不能依赖**：菜单场景可能没人调。`wrap_sound.cpp` 里保留“`GetNumSoundScripts()==0` 就自救调一次 ModInit”的兜底 |
| P88 | **`V_FixSlashes(buf)` 在 Windows 上把 `/` 改成 `\`（方向是反的！）** | 所以“归一化斜杠”用默认参数会把 `UI/panorama/x.wav` 变成 `UI\panorama\x.wav` 交给引擎（文件系统对反斜杠的容忍度不保证）。要正斜杠必须 `V_FixSlashes( buf, '/' )`。修完探针里是 `UI/panorama/...` |
| P89 | **波形文件不止 `sound/ui/panorama/`** | 脚本另外引用 `sound/UI/*.wav`（`buttonrollover/menu_accept/lobby_notification_*/xp_*` …）、`sound/common/null.wav`、`player/vo/{leet,phoenix,sas}/Affirmative*.wav` 共 43 个，全在 `D:\se`。`build\_deploy_ui_sounds.ps1` 已改成**解析脚本里的 wave 引用 → 缺什么从 D:\se 补什么 → 删 `sound.cache`**（一次补了 42 个；剩下 `ui/panorama/panorama/case_awarded_2_rare_01.wav` 是脚本自身的笔误，`D:\se` 也没有）。缺波形时引擎日志会有 `Failed to load sound "..."`，探针里对应 `playing=0` |
| P90 | **事件名回退链（设置值被拼进事件名）** | 菜单音乐事件是 `'UIPanorama.BG_' + ui_mainmenu_bkgnd_movie`（`mainmenu.js:118`，移植的 `seport/se_background_movie.cpp` 也这么拼），而设置值是 `anubis720` ⇒ 查 `UIPanorama.BG_anubis720`，脚本条目却是 `UIPanorama.BG_Anubis`。`wrap_sound.cpp` 解析顺序：原名 → **去掉最后一段的末尾数字** → **大小写不敏感扫** `GetSoundCount()/GetSoundName()`。实测 `BG_anubis720 → BG_Anubis` 命中并播放（`playing=1`） |
| P91 | **探针判据：`SNDOP played guid=N playing=M`** | `playing=1` = 引擎混音器确认在播（`IEngineSound::IsSoundStillPlaying`），是最强证据；`guid>0` 只说明进了 SND 系统。失败常见两种：`FAIL no index`（脚本库没这条，查 P86/P87/P90）与 `playing=0`（多半波形缺失，查 P89 与 engine.log 的 `Failed to load sound`） |

## M. 2026-09-18 追加六（ui 偏好 ConVar 注册 / popup_news 叠加弹窗）
| P92 | **运行时 `new ConVar` 的两个前提**（本轮“设置写不进去”的真因） | ① **名字要常驻**：`ConCommandBase::CreateBase` 注释写明 “Name should be static data”，而 `Params_GetArgAsString` 给的 JS 字符串随调用结束失效 ⇒ 必须自己复制一份（本端口用 `new char[]` 泄漏式保存）；② **必须显式注册**：`CreateBase` 只在 `ConCommandBase::s_pAccessor` 非空时 `Init()`（= 注册进 ICvar），而本模块（panoramauiclient.dll）从不调用 `ConVar_Register()` ⇒ 运行时创建的偏好永远进不了 `FindVar` 的查找表。症状：日志里同一行 `[SE port] created ui preference ConVar 'X'` **反复出现**（每次 Set 都重新创建），`GetSettingString` 恒返回 ""。修法：`uicomponent_gameinterface.cpp::Helper_CreateSettingsPreference` 里复制名字 + `g_pCVar->RegisterConCommand( pCreated )`。影响面：`ui_playsettings_*`（play 页记忆）、`se_session_state`（sim 的跨上下文状态）、`ui_news_last_read_link`（新闻“已读”） |
| P93 | **“关闭点了没反应”可能是“关掉了一个，下面还叠着 16 个”** | 新闻 RSS 落地后 `mainmenu_news.js::_OnRssFeedReceived` 会对第一条未读新闻调 `UiToolkitAPI.ShowCustomLayoutPopupParameters( '', popup_news.xml )`；同一份布局脚本会在**多个 JS 上下文**里各跑一遍（实测 `新闻: 演示条目已注入 x3` ×16 ⇒ 16 个上下文各弹一个 popup_news，位置完全重叠）。点“关闭”每下只关掉最上面的一个 ⇒ 用户看到“没反应”。**判据：探针里同一 seed 日志重复 N 次 = N 个上下文**。根治= 让“已读”设置真的能读写（见 P92） |
| P94 | **popup_news 关闭的新兜底（+诊断）** | `se_session_sim.js::hardenNewsPopupClose`：在弹窗上下文（有 `PopupNews` 全局）把 `id-close-button` 的 onactivate 换成“先 `$.DispatchEvent('UIPopupButtonClicked','')`（原行为），0.9 秒后若弹窗还在就 `AddClass('Hidden') + DeleteAsync(0)`”（同 `CUI_PopupManager::ForceClosePopups` 的删法）。正常关闭时弹窗 0.5s 后就销毁、该回调随之取消 ⇒ 兜底只在引擎链路失效时才动手。探针会留下 `弹窗: 关闭按钮点击` / `弹窗: 引擎关闭未生效, 强制删除` 两条判据（后者出现 = 引擎关闭链路真断了，要查 `CUI_Popup::OnPopupButtonClicked` 的注册/派发链） |
| P95 | **“点击没反应”的最新变体：同一个弹窗叠了两个（mainmenu.js 被两个上下文执行）** | 症状：“Legacy version of CS:GO”双按钮弹窗，点“确定”画面不变（点 STEAM SUPPORT 浏览器会开 —— 证明点击链本身是好的，只能是在关“另一个”弹窗）。根因：`mainmenu.js` 被不止一个 JS 上下文执行（嵌套的 `<CSGOMainMenu>` 那份也把布局脚本跑了一遍。判据：探针里 `商店按钮: 已安装` ×2），而它每个上下文都 `$.RegisterForUnhandledEvent('CSGOShowMainMenu', MainMenu.OnShowMainMenu)` → 同一个弹窗管理器里放下两个完全重叠的弹窗。**修法（JS，不用重编）**：`se_session_sim.js::dedupeLegacyPopup` 挂钩 `UiToolkitAPI.ShowGenericPopupTwoOptions`，用 `se_legacy_popup_shown` 设置做进程级去重（第一个放行，其余记 `弹窗: 重复的 Legacy 弹窗已拦截`）。另有 `popupStackProbe` 每 2 秒记一次 `弹窗栈[...]: 可见=N/M`。**根因（双层上下文）未动**——它还会把 ~30 个事件处理器全注册两遍，以后再遇到“事件效果 ×2”就往这里查 |
| P96 | **`se_api_shim` 的占位全局会把“只该在特定上下文跑”的代码弄脏** | shim 对每个上下文都装 `MainMenu/NewsPanel/PopupNews/...` 占位桩（`makeStub` 任意成员都 truthy）⇒ ① 旧的 `seedNewsFeed` 靠 `g.NewsPanel` 真值判断 ⇒ **每个上下文都“注入”了一次演示新闻**（探针同一条 ×16，害得计数全乱）；② `installStoreEntryButton` 在 40 次重试后给每个上下文打一行 `#JsNewsPanel 未出现`。**修法：看上下文根面板 id**（`$.GetContextPanel().id === 'JsNewsPanel'` 才是真新闻布局上下文——`FindChildInContext` 找不到“自己”）。判据：真上下文才有 `root.id === 'JsNewsPanel'` / `'MainMenu'` |
| P97 | **cfg 开关：`se_popup_news` / `se_popup_legacy`（默认都是 1）** | 两个启动弹窗的开关，实现在 `panoramauiclient/se_ui_settings.cpp`：模块级 ConVar + 在 `SE_PortInstallGameInterfaceBindings()` 里显式 `g_pCVar->RegisterConCommand()`（原因同 P92②）。**注册时机在 `Host_ReadConfiguration()` 之前**（`SetupUIEngine` 由 Host_Init 在 config 读取前调用）⇒ `config.cfg` / `autoexec.cfg` / 控制台都能设；FCVAR_ARCHIVE 让选择跨启动保留。JS 侧（`se_session_sim.js`）：`se_popup_news=1` 不预标记“已读” → `popup_news` 每次启动弹一次；`=0` 预标记 → 不弹。`se_popup_legacy=0` → 直接丢弃 Legacy 弹窗（探针记 `弹窗: Legacy 弹窗已被 se_popup_legacy=0 关闭`）。启动时 engine.log 有 `SE port: popup switches se_popup_news='..' se_popup_legacy='..'` 一行可核对 |



## N. 2026-09-19 追加七（**主菜单泛白根因破案**：纹理采样丢了 sRGB 解码）

| # | 项 | 结论 |
|---|---|---|
| P101 | **泛白真因 = panorama shader 绑纹理时丢了 `TEXTURE_BINDFLAGS_SRGBREAD`** | CS:GO 的 `panorama_cshader.cpp:162` / `panoramafancy_cshader.cpp:174-186` 用 `BindTexture( sampler, TEXTURE_BINDFLAGS_SRGBREAD, ... )` 让硬件在采样时做 sRGB→线性解码；本树 `CBaseShader::BindTexture()` 没有这个参数，移植时把 flag 丢了 ⇒ **整条管线的其余部分都是对的**（颜色在 `ColorFromABGR` 里转线性、混合按线性、输出经 `EnableSRGBWrite` 编码一次），唯独纹理按原始 sRGB 值进管线 ⇒ 每张图/视频帧多吃一次 gamma 编码 = 低对比、发白（mean 203.6）。**修复**：两个 shader 的 SHADOW_STATE 补 `pShaderShadow->EnableSRGBRead( sampler, true )`（SE 的 transition table 在 PC 上把它提交成 `D3DSAMP_SRGBTEXTURE`，`TransitionTable.cpp ApplySRGBReadEnable`，管线现成）。**此前"压暗层没画上"的推断作废**（见 P103 的实测） |
| P102 | **注意：PanDx 直连 D3D9 路径在本端口是死代码** | `source2surface.cpp` 定义了 `PANORAMA_SE_MATERIALSYSTEM_DRAW`，`PanDxInit()` 直接 return（`g_bPanDx=false`）⇒ 所有 `D3DRS_SRGBWRITEENABLE`/`D3DSAMP_SRGBTEXTURE` 的 PanDx 设置全都没跑，实际渲染走 materialsystem + `panorama_vs30/ps30` shader。**读源码时别被 PanDx 路径误导**——状态要看 shader 的 SHADOW_STATE 和 transition table |
| P103 | **灰阶测试法（本次破案的关键工具）** | 散文件测试布局 `D:\cstrike\cstrike\panorama\layout\se_gamma_test.xml`（`+panorama_menu se_gamma_test.xml` 加载，配套 `styles/se_gamma_test.css` + `materials/panorama/images/se_gamma_ramp.png`；脚本 `build/_gamma_make_assets3.ps1`、`_gamma_run2.ps1`（PrintWindow 截图，防遮挡）、`_gamma_bands.ps1`）。**注意三点**：① 本解析器不支持内联 `<style>` 块（会报 "Found duplicate panel description"），根面板不能带 id（"This ID is set in code"）——CSS 放散文件 include；② 游戏忽略 `-w/-h`（用存下的 1024x576），且 **panorama 按 1280x720 渲染后被裁切**（不是缩放）进窗口，采样坐标要按这个算；③ 基线（修复前）：灰阶 17→73（=encode(17/255) 精确吻合）、纯色 #808080→128 精确、rgba 压暗层（rgba(0,0,0,.5) over 128→92 等 12 块）全部吻合"线性混合+输出编码"模型 ⇒ **压暗层一直都在画且数学正确，泛白的是图本身**。修复后 13 条灰阶 0,17,...,204 全部精确还原，主菜单整窗 mean 203.6 → 96.4（`build/_mm_shot.png`） |
| P104 | **YUV 视频平面必须豁免 sRGB 读**（修复第一版把视频搞成绿紫假色） | shader 枚举是 **RGBA=1 / Alpha=2 / YUV=3 / YCoCg=4**（`panoramafancy_ps30.fxc` 顶部），CS:GO 对 texType==YUV 的三个平面用 `TEXTURE_BINDFLAGS_NONE`（`PanDxSetTexturesFancy`）：I8/L8 平面要按原始值采样，因为 fxc 的 YUV 分支自己做了 `pow(c,2.2)` 线性化——解码一次 + pow 一次 = 双重线性化 = 色度炸裂（本机驱动对 L8 也应用 sRGB 读）。**尝试过**的等价方案（`SHADER_PARAM( SRGBREAD )` 门控 + wrapper 第二套 `$srgbread 0` fancy 材质 `m_apFancyMaterialNV`（irendercontext.h）+ `UpdateMaterial()` 按 `ATTR_D_TEXTURETYPE==3/4` 选材质）：**普通绘制的 sRGB 读也被弄丢**（灰阶回退到 0,73,...，用户实测泛白回归），**已整体回退**。嫌疑：同一 shader 两个只差 `D3DSAMP_SRGBTEXTURE` 的 snapshot 在 transition table 里的状态往返；根因未查。**最终修法（09-19 第二阶段，已验证）** = YUV 平面 I8→A8 + fxc YUV 分支改读 .a（sRGB LUT 不作用于 alpha，与驱动无关；fxc 是运行时编译的，`vertexshaderdx8.h:29` DYNAMIC_SHADER_COMPILE，改完即生效），panorama 电影颜色恢复正常（mean 89.5）；下次方向：查 CTransitionTable 的 diff/Apply、给 IShaderAPI 加动态 SetSamplerSRGBRead、或 MF 播放器直接出 RGB32。完整案例见 `csgo_panorama_port_breakthroughs.md` T1。**遗留**：blur pass 的源若来自引擎写的 RT（如 `_rt_FullFrameFB`，引擎不做 sRGB 写），解码会把世界画面错误线性化——重新开工模糊（P65）时要把这个差异一并考虑 |
