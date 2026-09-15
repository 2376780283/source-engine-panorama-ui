# external/v8 — vendored V8 for the CS:GO Panorama port

## Version

**V8 7.3.492** (Chromium 73 / Electron 5.x era), Windows x86 (32-bit), `/MT` static CRT.

Chosen because the port target (`panorama/uiengine.cpp`, the JS host) needs a *matching*
header + static-library pair, and a ready-made prebuilt monolith exists on NuGet:

| Artifact | Source |
| --- | --- |
| headers + static libs | NuGet `v8_monolithic.windows-latest.x86.release` **7.3.492** |
| `icudtl.dat` (ICU 63) | Electron **v5.0.13** `win32-ia32` zip (Chromium 73, same v8) |

## Layout (after running `scripts/dev/fetch_v8_deps.ps1`)

```
external/v8/
  include/                 # v8 7.3.492 public headers  (TRACKED in git)
    v8.h, v8-version.h, v8config.h, v8-platform.h, v8-inspector*.h, ...
    libplatform/libplatform.h
    v8-debug.h             # local compat shim (removed upstream in 6.9)
  lib/win/x86/             # 424 MB of .lib binaries        (GIT-IGNORED)
    v8_monolith.lib        # 424 MB - everything incl. snapshot
    v8_libbase.lib         # 2.2 MB
    v8_libplatform.lib     # 1.3 MB
  icu/                     # icudtl.dat (10 MB)             (GIT-IGNORED)
```

## Fetching the binaries

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\dev\fetch_v8_deps.ps1
```

## Linking

The static libs are MSVC `/MT` (static CRT) - the same CRT the Source Engine waf build
uses (`waf.bat configure -T release --32bits`, which injects `/MT`), so no LNK2038
`RuntimeLibrary` mismatch occurs.

```
v8_monolith.lib v8_libbase.lib v8_libplatform.lib
winmm.lib dbghelp.lib shlwapi.lib ws2_32.lib advapi32.lib userenv.lib
```

Verified end-to-end (compile -> link -> run -> execute JS -> ICU):

```
V8_VERSION=7.3.492
JS_OUT=Intl=object Date=Fri Mar 02 2018 22:13:20 GMT+0800 (...) Loc=1,234.5 | res=3
```

## Runtime requirements

1. **`icudtl.dat` must sit next to the executable.** ICU data is *not* embedded in this
   monolith; without it v8 aborts inside a JS isolate with
   `Failed to create ICU number_format, are ICU data files missing?`.
   `panorama/uiengine.cpp` already expects it at `bin\icudtl.dat`
   (`v8::V8::InitializeICU("bin\\icudtl.dat")`) - ship the file from `external/v8/icu/`.
2. **Use `V8::InitializeICU(path)` or `InitializeICUDefaultLocation("", ...)`.**
   `InitializeICUDefaultLocation(nullptr, ...)` dereferences the null `exec_path` and
   crashes with an access violation before ICU is loaded.

## Gotcha for future header bumps

`v8-debug.h` no longer exists upstream (removed in 6.9); the CS:GO sources still include
`../external/v8/include/v8-debug.h`, so this directory keeps a shim that just pulls in
`v8.h`. Legacy `v8::debug::*` agent APIs are gone from 7.x - keep debug code behind
`V8_DEBUGGING_ENABLED` disabled if it starts failing to compile.
