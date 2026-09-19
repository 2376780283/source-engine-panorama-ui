# CS:GO Panorama 移植 —— 重大突破记录

> 配套:`csgo_panorama_port_pitfalls.md`(踩坑表,P 编号被本文引用)、`csgo_panorama_port_checklist.md`(任务清单)。
> 本文记录**已闭环的重大问题**:现象、定位过程、根因、修复实现(具体到文件/函数)、验收数据。
> 每篇按"别人拿着它能否独立复现/续作"的标准写。

---

## T1. 主菜单泛白(整体发白、低对比)根因破案与修复(2026-09-19)✅

### 0. 一句话结论

**panorama 的两个 shader(`panorama_dx9` / `panoramafancy_dx9`)绑定纹理时丢了 CS:GO 的
`TEXTURE_BINDFLAGS_SRGBREAD` 标志,纹理按原始 sRGB 值进入"线性混合 + 输出 sRGB 编码"的渲染管线,
每张图/视频帧被多提亮一次 gamma(17 → 73 就是encode(17/255)),这就是主菜单泛白。**
修复 = 在两个 shader 的 `SHADOW_STATE` 里补 `pShaderShadow->EnableSRGBRead( sampler, true )`
(SE 的 transition table 会把它提交成 `D3DSAMP_SRGBTEXTURE`,管线是现成的)。
整窗平均亮度 **203.6 → 96.4**,13 条灰阶从"全部偏亮"修到**逐条精确还原**。

### 1. 症状与历史(为什么查了很久)

- 主菜单整体泛白:整窗 mean 203.6(>200 的像素 65.6%),背景图/视频像蒙了一层白纱,对比度很低。
- 之前怀疑过模糊(blur)路径,把模糊关掉(`SE_PortSupportsBlurPasses()` → false,P63)后**依然泛白**,
  于是归因为"CS:GO 靠 CSGOBlurTarget 模糊+压暗,端口两条都缺"(checklist §8.2 的候选方向:
  `#JsNewsPanel` 的 `rgba(40,40,40,0.3)` 压暗层没画上 / opacity / sRGB 写入)。
- **这个归因后来被证明错了一半**:压暗层一直都在画、而且数学是对的;泛白的是"图本身"。

### 2. 定位过程(可复现,工具都在仓库里)

核心方法:**不猜,造一个"答案已知"的画面,实测每个环节**。灰阶测试布局
(`D:\cstrike\cstrike\panorama\layout\se_gamma_test.xml`,散文件,`+panorama_menu se_gamma_test.xml` 加载;
生成脚本 `build/_gamma_make_assets3.ps1`,运行/截图 `build/_gamma_run2.ps1`,量灰阶 `build/_gamma_bands.ps1`):

1. **第一轮(布局踩坑)**:内联 `<style>` 块不被解析器支持(报 "Found duplicate panel description",
   这个 panorama 只有 `<styles>` include 区);根面板不能带 `id`("Top most panel should not have an ID")。
   ⇒ CSS 拆成散文件 `styles/se_gamma_test.css` include 进来,根面板用 class。
2. **第二轮(灰阶条)**:一张 16 级灰阶 PNG(0,17,34,...,255 竖条)+ 三行纯色/压暗块。
   实测(P103 有完整数字):
   - 灰阶 17 → 屏上 **73** = `sRGBEncode(17/255)=73.5` **精确吻合** ⇒ 图像被多编码一次 gamma;
   - 纯色 `#808080` → **128 精确** ⇒ 纯色路径没错;
   - 12 块压暗采样(如 `rgba(0,0,0,0.5)` 叠在 128 上 → 92)全部吻合
     **"线性空间 alpha 混合 + 输出一次 sRGB 编码"模型**(0.5×lin(128)=0.108 → encode → 93.3 ≈ 92)。
   - ⇒ 结论收敛:**颜色/混合/输出编码全对,唯独纹理采样没有 sRGB→线性解码**。
3. **代码对拍**:CS:GO 的 `panorama_cshader.cpp:162`、`panoramafancy_cshader.cpp:174-186` 用
   `BindTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_SRGBREAD, pTexture, 0 )`;本树
   `CBaseShader::BindTexture()` 没有这个参数(端口注释里也写了"takes no TextureBindFlags_t argument"),
   移植时把 flag 丢了。而 SE 的接收端是现成的:
   `IShaderShadow::EnableSRGBRead()`(`public/materialsystem/ishadershadow.h:312`)→
   `CShaderShadowDX8::EnableSRGBRead()`(写进 shadow snapshot)→
   `TransitionTable.cpp ApplySRGBReadEnable()` 在 PC 上 `SetSamplerState(D3DSAMP_SRGBTEXTURE, ...)`,
   BlurFilterX/Y、DecalModulate 等原生 shader 早就这么用 ⇒ 管线可靠。
4. **重要提醒(P102)**:本端口 `PANORAMA_SE_MATERIALSYSTEM_DRAW` 让 `PanDxInit()` 直接 return,
   CS:GO 源码里那套 `D3DSAMP_SRGBTEXTURE`/`D3DRS_SRGBWRITEENABLE` 的 **PanDx 直连 D3D9 路径是死代码**,
   实际渲染走 materialsystem + `panorama_vs30/ps30`。读源码时别被 PanDx 路径带偏。

### 3. 修复实现(最终采用的就是这么多)

两个文件,各加一行(位置都在 `SHADER_DRAW { SHADOW_STATE { ... } }` 里 `EnableTexture` 之后):

- `materialsystem/stdshaders/panorama_dx9.cpp`(shader `panorama`):
  ```cpp
  pShaderShadow->EnableSRGBRead( SHADER_SAMPLER0, true );
  ```
- `materialsystem/stdshaders/panoramafancy_dx9.cpp`(shader `panoramafancy`):
  ```cpp
  pShaderShadow->EnableSRGBRead( SHADER_SAMPLER0, true );
  pShaderShadow->EnableSRGBRead( SHADER_SAMPLER1, true );
  pShaderShadow->EnableSRGBRead( SHADER_SAMPLER2, true );
  pShaderShadow->EnableSRGBRead( SHADER_SAMPLER3, true );
  ```
- 构建目标:**`stdshader_dx9`**(不是 materialsystem!`materialsystem/stdshaders/wscript`,
  `PROJECT_NAME='stdshader_dx9'`),产物 `build/materialsystem/stdshaders/stdshader_dx9.dll`,
  **部署到 `D:\cstrike\bin` 和 `D:\cstrike\cstrike\bin` 两处**(常规 deploy 脚本清单里没有这个 DLL,要手动拷)。
- 不需要动 fxc/.inc:EnableSRGBRead 是 shadow 状态,不改字节码。

### 4. 验收数据(修复前 → 修复后)

| 项 | 修复前 | 修复后 |
|---|---|---|
| 灰阶条(源 0,17,34,...,204) | `0,73,95,102,124,141,...`(全部偏亮) | **`0,17,34,51,68,85,102,119,136,153,170,187,204` 逐条精确** |
| 主菜单整窗 mean | 203.6 | **96.4**(`build/_mm_shot.png`) |
| 压暗层(navbar/news 的 rgba wash) | 本来就对 | 不变(仍对) |
| 纯色块 | 本来就精确 | 不变 |

### 5. YUV 视频的例外(已修复,2026-09-19 第二阶段)

- **shader 枚举**(`panoramafancy_ps30.fxc` 顶部,与 C++ `source2surface.h:191` 一致):
  `RGBA=1 / Alpha=2 / YUV=3 / YCoCg=4`。
- CS:GO 对 texType==YUV 的三个平面用 `TEXTURE_BINDFLAGS_NONE`(`PanDxSetTexturesFancy`):I8/L8 亮度值
  必须按原始值采样,因为 fxc 的 YUV 分支**自己做 `pow(c,2.2)` 线性化**——平面被硬件解码一次 +
  shader 再 pow 一次 = 双重线性化 = 绿紫假色(本机驱动对 L8 也应用 sRGB 读)。
- **YUV 平面必须豁免硬件 sRGB 解码**(CS:GO 对 texType==YUV 用 `TEXTURE_BINDFLAGS_NONE`,
  `PanDxSetTexturesFancy`):fxc 的 YUV 分支自己做 `pow(2.2)` 线性化,要求平面是原始 BT.601 值;
  若被解码一次再 pow 一次 = 双重线性化 = 绿紫假色(本机驱动对 L8 也应用解码)。
- **尝试过并回退的方案**:"`$srgbread` 材质参数门控 + wrapper 按 texType 选第二套材质"
  (等价 CS:GO 的按 draw 设 flag)——把普通绘制的 sRGB 读也弄丢了(灰阶回退 0,73,...),已回退。
  嫌疑:同一 shader 两个只差 `D3DSAMP_SRGBTEXTURE` 的 snapshot 在 transition table 里的往返;根因未查。
- **最终修法(已验证)**:**YUV 平面格式 I8→A8 + fxc 改读 .a 通道**——sRGB 解码 LUT 作用于 RGB/亮度
  通道,永远不作用于 alpha 通道,所以 A8 平面天然免疫、与驱动无关,且不需要任何按 draw 的状态切换:
  1. `panorama/source2/uirenderdevicesource2.cpp`:Y/U/V 三张平面的 `CTextureCreationDesc/CTextureDesc`
     `m_nImageFormat` 6 处 `IMAGE_FORMAT_I8` → `IMAGE_FORMAT_A8`(上传布局同为 1 字节/像素,不变);
  2. `materialsystem/stdshaders/panoramafancy_ps30.fxc` YUV 分支:三个 `Tex2D(...).r` → `.a`;
  3. **fxc 是运行时编译的**:`vertexshaderdx8.h:29` 无条件 `#define DYNAMIC_SHADER_COMPILE`,
     `GetShaderSourcePath()` 用 `__FILE__` 定位到源码树 `materialsystem/stdshaders/`,
     `D3DXCompileShader`(d3dx9_33)按 combo 现场编译 ⇒ **改 fxc 保存即生效,不用构建**;
     `.inc` 只是 combo 步长表,不含字节码,无需改动。
  4. 构建:`waf build --targets=panoramauiclient`(格式改动所在),部署 panoramauiclient.dll。
  验收:主菜单 vertigo.webm 蓝天/集装箱全部真实色彩(整窗 mean 89.5),灰阶仍逐条精确。
- **架构事实(谁在渲染 webm 背景)**:真 CS:GO 渲染在 **panorama 内部**——`mainmenu.xml` 的
  `MainMenuMovieSnippet` 里的 `<Movie id="MainMenuMovie">`(CMoviePlayer),由
  `CCSGO_MainMenu::LoadBackgroundMovie()` 加载,经 `IUIDoubleBufferedYUV420Texture`(三平面)→
  fancy shader YUV 分支,在合成层里画,主菜单的 blur 采样它。本端口有**两条桥并存**:
  ① panorama 桥(移植的 LoadBackgroundMovie,真菜单里由 tick 激活,探针可见
  `texType=3 tex0=[1_panorama_texture_y] ...`);② VGUI 兜底桥(`game/client/cstrike/se_background_video.cpp`,
  CPU 解码 NV12→RGBA,画在 VGUI client root 上、panorama UI **后面**,panorama 电影正常时被盖住看不见,
  且有 MF 初始化偶发失败的黑屏问题)。电影修好后 VGUI 桥可以考虑关闭(`se_background_video_enable 0`)。
- **模糊重开时注意**(P104 遗留):blur 的源若含引擎写的 RT(`_rt_FullFrameFB` 等,引擎不做 sRGB 写),
  EnableSRGBRead 会把世界画面错误线性化——重开模糊(P65)时要把这个差异一并考虑。

### 6. 涉及文件/脚本清单

| 类型 | 路径 |
|---|---|
| 修复(生效) | `materialsystem/stdshaders/panorama_dx9.cpp`、`panoramafancy_dx9.cpp`(EnableSRGBRead) |
| 测试布局 | `D:\cstrike\cstrike\panorama\layout\se_gamma_test.xml` + `styles\se_gamma_test.css`(散文件,不入 pbin) |
| 测试图 | `D:\cstrike\cstrike\materials\panorama\images\se_gamma_ramp.png`(16 级灰阶) |
| 测试脚本 | `build/_gamma_make_assets3.ps1`(造素材)、`build/_gamma_run2.ps1`(运行+PrintWindow 截图+色块测量)、`build/_gamma_bands.ps1`(量灰阶)、`build/_mm_verify.ps1`(主菜单整窗 mean) |
| 仓库内布局副本 | `mods/panorama_test/panorama/layout/`(需要时同步) |
| 相关坑位 | P101(根因)、P102(PanDx 死代码)、P103(灰阶测试法与基线数字)、P104(YUV 豁免尝试与回退) |
