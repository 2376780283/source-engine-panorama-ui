# CS:GO Panorama 移植 —— 实操笔记（速查 + 踩坑）

> 配套文档：`docs/csgo_panorama_port_plan.md`（可行性分析 + 分阶段计划）。
> 本文件记录**实际操作手法、正确命令、踩过的坑**，供后续会话直接复用。
> 环境：Windows / VS2022 MSVC 14.44 / 本仓库 waf 构建。

---

## 1. 正确配置/构建命令（务必带齐参数）

**本仓库 waf 的默认值是 hl2 + 64 位**，不带参数会跑偏。工程实际意图是 cstrike / 32 位。

```text
# 配置（cstrike / 32 位 / release，与工程原 configuration.py 对齐）
waf.bat configure -T release --32bits --build-games=cstrike --prefix=build/out/ --disable-warns --enable-opus

# 全量构建 + 安装
waf.bat install

# 只构建某目标（快速迭代，如冒烟）
waf.bat build --targets=<target>

# 配置后必须重新 configure 才会反映 wscript 变更
```

> 旧的 `-T debug` 任务与 release 混用时会使对象缓存失效、触发全量重编，注意一致性。

产物位置：
- `build/out/cstrike/bin/` → `client.dll` / `server.dll`
- `build/out/hl2_launcher.exe` → 启动器（本仓库统一叫 hl2_launcher）

---

## 2. 常用命令速查

```text
git -C d:\source-engine status / branch / diff --stat   # git 必须用 -C（终端 cd 会漂移）
waf.bat configure ...                                    # 见上
waf.bat install / build --targets=X
```

---

## 3. 终端规避手法（重要）

交互式 PowerShell 在本会话多次出问题（PSReadLine 崩溃、`^U` 乱码注入、管道吞输出、
`$f=` 赋值被破坏、甚至 `powershell` 都找不到）。**规避方案**：

1. **长/复杂命令写成 `.ps1` 脚本文件**，用非交互方式执行（每跑都是新进程，绕开坏会话）：
   ```text
   powershell -NoProfile -ExecutionPolicy Bypass -File <script.ps1>
   # 若 powershell 不在 PATH（会话损坏），用完整路径：
   & 'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe' -NoProfile -ExecutionPolicy Bypass -File <script.ps1>
   ```
2. **避免内联变量赋值**（如 `$f='...'; Get-Content $f` 容易被注入破坏）→ 用字面路径，或用脚本读。
3. **避免把 waf 输出管道到 `Select-Object`**（常被吞）→ 让工具落盘，再用 `read_file`/脚本读结果文件**认准当次 run 的文件**（多次读旧文件误判过）。
4. `git` 一律 `git -C d:\source-engine`；脚本内显式 `Set-Location`。

---

## 4. 踩坑清单

### 4.1 构建/配置
- `waf configure` **默认 hl2 / 64 位**；不显式 `--build-games=cstrike --32bits` 就跑偏（首跑误产到 `build/out/hl2`，幸而佐证了工具链 OK）。
- 仓库此前只有“配置+对象”，从未 install → 基线验证要跑到产物落盘才算数。
- `scripts/waifulib/vpc_parser.py`（148 行）不支持复杂 VPC（`$Conditional/$Configuration/protobuf_builder`）→ **新模块手写 wscript**，VPC 只作文件清单参考。

### 4.2 CSGO2019 / Panorama 源码自身
- `public/panorama` SE 与 CSGO2019 是**不同支线**（内容整体分叉，`iuipanel.h` 差 263 行）→ 整组以 CSGO2019 覆盖，勿只补缺。
- **V8 include 无条件、双路径**：`SOURCE2_PANORAMA→../thirdparty/v8`；否则→`../external/v8`。SE 自带 external/v8 头（**无库**）；SOURCE2 路径需镜像到 `thirdparty/v8`。
- `iuiengine.h` source1（`!SOURCE2_PANORAMA`）分支 include `audio/iaudiointerface.h`，**该文件连 CSGO2019 都不存在** → 只能按 SOURCE2 模式编译。
- `CUtlVectorFixedGrowable`：`panoramatypes.h` 遗留 `int` 版 vs tier1 `size_t` 版冲突 → 守卫宏 `SE_TIER1_FIXEDGROWABLE`。
- **Source2 时代公共层缺失**：`tier0/logging.h` 通道子系统、普通 `CRefCount`（两库 refcount.h 只有 `CRefCountService*`）→ “纯头全绿”需系统性移植（Phase 2 tier 对齐工作包）。
- CSGO2019 无 `lib/`（缺 Valve 预编译）→ 不能自洽构建；构建体系与 SE 不同。

### 4.3 编译细节
- 头里 `#include "memdbgon.h"`（无 `tier0/` 前缀）→ 需把 `public/tier0` 设为直接 include 目录。
- `<openvr.h>` → include 目录加 `public/openvr`。
- 需从 CSGO 补的缺失头（已加）：`tier1/utlptrarray.h`、`tier0/platwindow.h`、`mathlib/beziercurve.h`。
- MSVC `/showIncludes` 刷屏 + **C4819 中文乱码**（代码页 1252 vs UTF-8），错误里文件名乱码 → 靠行号定位。
- 编译器一次只报前几个缺失头 → 一个 TU 只能一轮轮修。

---

## 5. 当前状态速查（2026-09-08）

- 分支：`csgo-panorama-port`（基线 `391effac`）。
- ✅ Phase 0：工具链确认；**cstrike/32 基线构建通过**。
- ✅ Phase 1：CSGO2019 `public/panorama` 覆盖合入（ADD21/OW77/SAME0，保留 SE 独有 2，现 100 文件）；报告 `docs/panorama_header_overlay_report.txt`；补齐 3 个缺失公共头；v8 镜像 `thirdparty/v8`；`panoramatypes.h` 加守卫。
- ✅ 纯头冒烟**诊断完成**（SOURCE2_PANORAMA 模式），全绿待 Phase 2 tier 对齐。
- 🧹 临时冒烟挂载已从 `wscript` 移除并 reconfigure，**仓库干净可构建**。
- ⏭️ 待办：`scratch/` 与 `scripts/dev/tmp_*.ps1` 提交前清理；Phase 1 提交；Phase 2 tier 对齐工作包。

---

## 6. 提交前检查（Phase 1 收尾）
- [ ] 删除 `scripts/dev/tmp_*.ps1`、`scratch/panorama_hdrcheck/`（或归档）。
- [ ] 确认 `wscript` 无 TEMP 挂载；`build/_cfg*.txt` 不入库（已被 .gitignore 覆盖则忽略）。
- [ ] 用 `waf install` 验证一次干净构建（cstrike/32）。
- [ ] 提交：`git add public/panorama public/tier1/utlptrarray.h public/tier0/platwindow.h public/mathlib/beziercurve.h thirdparty/v8 docs/ ...`，提交信息注明“对合入 CSGO 文件的本地适配（SE_TIER1_FIXEDGROWABLE 守卫等）”。
