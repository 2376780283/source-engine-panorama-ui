// v8-debug.h compatibility shim.
//
// v8-debug.h was removed upstream in v8 6.9 (its contents moved into v8.h /
// v8-inspector.h). The CS:GO 2019 panorama sources still include it, so we
// provide this shim to keep them compiling against the v8 7.3.492 headers
// used by this port. The legacy v8::debug::Debug agent API is *not*
// available in 7.x; code paths relying on it must be disabled via
// V8_DEBUGGING_ENABLED (see panorama/ctx_debug.h).
#ifndef V8_DEBUG_H_COMPAT_SHIM_
#define V8_DEBUG_H_COMPAT_SHIM_

#include "v8.h"

#endif  // V8_DEBUG_H_COMPAT_SHIM_
