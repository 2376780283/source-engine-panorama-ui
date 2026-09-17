# Panorama 模型查看器（查看游戏自带模型）—— 方案（2026-09-16 定稿待做）

> 状态：⬜ **未开工**。思路已定，留待后续。
> 关联：`csgo_panorama_port_checklist.md` §9 待办、`csgo_panorama_port_pitfalls.md`（踩坑表）。
> 目标：在 Panorama UI 里**查看本游戏自带的 `.mdl` 模型**（CS:S 的武器/角色等），
> 不要求是 CS:GO 的皮肤物品，也**不依赖 econ/库存数据、不依赖 GC**。

---

## 1. 为什么可行（关键事实，均已查证）

| 事实 | 位置 | 意义 |
|---|---|---|
| ⭐ `IVModelRender` 实现**在 engine.dll 内** | `engine/l_studio.cpp:812 class CModelRender : public IVModelRender`；导出 `EXPOSE_SINGLE_INTERFACE_GLOBALVAR(..., VENGINE_HUDMODEL_INTERFACE_VERSION, s_ModelRender)`（L1009） | 不用碰 client.dll；该接口本名就是 "HUD model"，专为**无场景画模型**设计 |
| ⭐ panoramauiclient 已能从同一 factory 取引擎接口 | `panoramauiclient/panoramauiclient.cpp:189/199/200/204`（`PANORAMAUI_ENGINE` / `VENGINE_GAMEUIFUNCS` / `GameEventManager2` / `VENGINE_CLIENT_INTERFACE_VERSION`） | 再加一行 `factory(VENGINE_HUDMODEL_INTERFACE_VERSION)` + `factory(VMODELINFO_CLIENT_INTERFACE_VERSION)` 即可 |
| ⭐ 引擎里已有"无 client entity 也能画 .mdl"的完整模板 | `engine/staticpropmgr.cpp`：`class CStaticProp : public IClientUnknown, public IClientRenderable, public ICollideable`（L118）；`SetupBones()` L683、`GetModel()` L704、`DrawModelSlow()` L970 → `modelrender->DrawModelExStaticProp(sInfo)` | 照抄结构；`IClientRenderable`（`public/iclientrenderable.h`，282 行）有 ~44 个纯虚，绝大多数填 `NULL/false` |
| RT（渲染到纹理）与"把 RT 画进面板"都有现成先例 | 创建 RT：`g_pMaterialSystem->CreateNamedRenderTargetTextureEx`（`panorama_s1wrapper/wrap_texture.cpp:1194/1202` 等多处）；面板显示 RT：`panorama/seport/gameclient/csgo_backbufferimage.cpp` | 面板侧属"改名字"级别 |
| VPK 里的模型**不用解包** | 引擎 model loader（`IVModelInfoClient::FindOrLoadModel`） | `cstrike_pak*.vpk` 里的武器/角色模型可直接加载 |

## 2. 实现步骤

### 2.1 引擎侧（新文件 `engine/se_modelpreview.{h,cpp}`）
1. 精简版 renderable：`class CSE_ModelPreviewRenderable : public IClientUnknown, public IClientRenderable`，
   照 `CStaticProp` 填全部纯虚（`GetRenderOrigin/Angles`、`GetModel`、`RenderHandle`、`GetColorModulation`、
   `GetRenderBounds(Worldspace)`、`GetShadowHandle`→无效、`ShadowCastType`→`SHADOWS_NONE`、`GetBody`/`GetSkin`…）。
2. `SE_PortModelPreview_SetModel( const char *pszModel )`：`modelinfo->GetModel( index )`（先 `FindOrLoadModel`），
   失败则回退 `error.mdl` 并打警告；缓存 `studiohdr_t*`。
3. `SE_PortModelPreview_Render()`：每帧（或 dirty 时）
   ① 保存 RT/viewport（`IMatRenderContext::PushRenderTargetAndViewport` 或记录后恢复）；
   ② 绑定 `_se_modelpreview` RT 并 `ClearColor4ub`；
   ③ 设置 view state + `g_pStudioRender->SetAmbientLightColors(white)`（用法见 `l_studio.cpp:1620`）、
      `SetViewState(...)`（`l_studio.cpp:2199`）、`SetColorModulation/SetAlphaModulation`（`l_studio.cpp:1029/1030`）；
   ④ `modelrender->DrawModel( pRenderable, flags, instance )`；
   ⑤ 恢复 RT/viewport。
4. 与渲染帧的挂点：在现有 `PanoramaRunFrame`（`engine/panoramaenginehandler.cpp`，引擎每帧调）里按需调用。
5. 角度/缩放：由 ConVar（如 `se_modelpreview_angles` / `se_modelpreview_dist`）驱动，JS 侧改 ConVar。

### 2.2 Panorama 侧（新文件 `panorama/seport/gameclient/csgo_modelpreviewpanel.{h,cpp}`）
1. 照 `csgo_backbufferimage.cpp` 写面板 + renderer：把 `_se_modelpreview` 纹理按 `CRenderAttributes::SetTextureValue`
   绑进 fancy quad 绘制。
2. JS 方法：`SetModel( path )`、`SetAngles( p, y, r )`、`SetDistance( d )`（内部调 `ConVarRef::SetValue` 或引擎桥）。
3. **必须列进 `panoramauiclient/wscript` 的 source**（静态库里的 `REGISTER_PANEL2D_FACTORY` 不会被拉进链接——踩过多次）。
4. 可选：把现在的空桩 `ItemPreviewPanel`（`seport_panel_stubs.*`）换成这个真类，让内容里的"检视"页直接显示游戏模型。

### 2.3 内容侧
1. 测试布局：`mods/panorama_test/panorama/layout/se_modelviewer.xml`（面板 + 上一个/下一个按钮 + 模型名标签）。
2. 模型列表：
   - 快速版：写死几把武器（`models/weapons/w_rif_ak47.mdl` 等，需按本 mod 实际路径核对）；
   - 完整版：扫 `<mod>\models\**\*.mdl`（散文件只有 8 个）或走 filesystem 遍历 VPK（+0.5~1 天）。

## 3. 风险与对策

| 风险 | 对策 |
|---|---|
| **光照**：RT 里无场景光 ⇒ 模型可能全黑/全白 | 渲染前 `SetAmbientLightColors(white)` + `SetViewState(...)`（引擎无光照时就这么干，见 `l_studio.cpp:1620`）。预留 1–2 天调试 |
| RT / viewport 状态互踩（本项目在合成层/blur 上踩过） | 渲染前后成对保存恢复；先在独立测试布局里验证，别一上来就插进主菜单 |
| `IClientRenderable` 44 个纯虚 | 大量照抄 `CStaticProp`；缺哪个编译器会点名 |
| 模型路径/VPK 可见性 | `FindOrLoadModel` 失败会返回 `error.mdl` ⇒ 便于区分"路径错"与"渲染错" |
| `-dxlevel 95` / 无 map 时能否画模型 | 先在主菜单（非地图内）验证；必要时挂到 `SCR_UpdateScreen` 的合适槽位 |

## 4. 待用户拍板的两个点（开工前问）

1. **形态**：(A) 独立"模型查看器"页（最小、最快）；(B) 直接让内容里的"检视/涂鸦"页（现在的 `ItemPreviewPanel` 空桩）显示游戏模型。
2. **模型来源**：先写死列表 / 扫目录 / 遍历 VPK 列出全部。

## 5. 验收标准

- 游戏内打开测试布局，面板里**能看到指定 `.mdl`**，鼠标拖动可旋转视角，进程稳定 30s 无崩溃。
- 切换模型（列表下一个）后画面正确更新；换模型不泄漏 RT。
- 不引入任何 econ / GC / Steam 依赖。

## 6. 估算

| 步骤 | 时间 |
|---|---|
| 引擎侧 renderable + RTT 渲染（含光照调试） | 2–3 天 |
| panorama 面板 + 纹理绑定 | 1 天 |
| 内容接线（模型列表 / 鼠标旋转）+ 实机验证 | 1–2 天 |
| **合计** | **≈ 1 周** |
| 后置可选 | 动画序列播放、皮肤/贴花、接入既有"检视"页 |

## 7. 与"库存"那条路的区别（避免混淆）

- 库存（`InventoryAPI`）需要 **econ schema + 假 SOCache**（因为真库存要 GC，不可得，见 §记忆 2026-09-16(22)）。
- 本方案**完全不碰 econ**：模型路径由我们直接指定，显示是"游戏自带资源"。
- 若将来要做"库存里显示物品"，可以把本方案的面板复用作 `ItemImage`/`ItemPreviewPanel` 的**渲染后端**。
