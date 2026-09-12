//========= Copyright Valve Corporation, All rights reserved. ============//
//
// SE port: the subset of CS:GO's interfaces/interfaces.cpp that the panorama modules need.
//
// CS:GO compiles that file into its shared "interfaces" library (interfaces/interfaces.vpc), which
// every module links.  Source Engine 2013 has no such waf project - the only copy in this tree,
// external/vpc/interfaces/interfaces.cpp, is a vendored vpc sample that is never compiled - and two
// panorama translation units need the definitions:
//
//   panorama/data/imageloader.cpp   -> g_pAsyncFileSystem
//   panorama/uiengine.cpp           -> panorama::g_IUITextServices  (assigned at runtime by the UI
//                                      engine through the PANORAMA_TEXT_SERVICES_INTERFACE_VERSION
//                                      factory; stays NULL when the split text module is absent)
//
// Only those two globals are defined here; the rest of CS:GO's file belongs to engine-facing layers
// that this port wires up separately (M4).
//
// ============//
#include "stdafx.h"     // framework PCH: brings in utlhashmap.h etc. that input/uiinput.h needs

#include "tier0/platform.h"
#include "tier0/dbg.h"
#include "tier0/memdbgon.h"

#include "interfaces/interfaces.h"
#include "panorama/uiengine.h"

IAsyncFileSystem *g_pAsyncFileSystem = 0;

// panorama/source2/imesource2.cpp, uienginesource2.cpp and panoramauiengine.cpp all call through
// this; the declaration comes from public/interfaces/interfaces.h (gated by PANORAMA_ENABLE, which
// this module now defines).
IIMEManager *g_pIMEManager = 0;

// NOTE: CS:GO guards these with #ifdef PANORAMA_ENABLE (its interfaces library is built both with and
// without panorama).  The framework library this file belongs to always contains panorama/uiengine.cpp,
// which references g_IUITextServices, so the definition is unconditional here.
panorama::IUITextServices *g_IUITextServices = 0;
