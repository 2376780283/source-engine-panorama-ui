//============ Copyright (c) Valve Corporation, All rights reserved. ==========
//
// SE port: the panorama interface globals the engine owns.
//
// CS:GO defines these in interfaces/interfaces.cpp, which is compiled into the "interfaces" library
// that every module links.  This tree has no such module, and the engine reaches the panorama UI
// client module through these globals:
//
//   engine/sys_dll2.cpp               - fills them from the module factory
//                                       (PANORAMAUI_CLIENT_INTERFACE_VERSION /
//                                        PANORAMAUI_ENGINE_INTERFACE_VERSION)
//   engine/panoramaenginehandler.cpp  - calls through g_pPanoramaUIClient
//                                       (SetupUIEngine/CreatePanel2D/HandleInputEvent/...)
//
// The engine links no panorama library: everything goes through these interfaces, which are declared
// in public/interfaces/interfaces.h and implemented by panoramauiclient.dll.
//
// ==========//
#include "tier0/platform.h"
#include "tier0/dbg.h"

#include "interfaces/interfaces.h"

#ifdef PANORAMA_ENABLE
IPanoramaUIEngine *g_pPanoramaUIEngine = 0;
IPanoramaUIClient *g_pPanoramaUIClient = 0;

// Like CS:GO's shared "interfaces" library, the panorama hosting TU also references the IME manager and
// the input stack system.  Source Engine has no input stack implementation at all, so both stay NULL:
// the IME paths are NULL-guarded by the ported code, and the handler's input context calls were given
// the same treatment (see the SE port notes in panoramaenginehandler.cpp).
//
// CAVEAT: these are the *engine's* copies of the variables.  panorama/seport/se_interfaces.cpp defines
// its own inside the panorama client DLL, so whatever the DLL stores there is invisible here.  Making
// the two agree needs the DLL to export an accessor (or the engine to link an import library) - until
// then the engine treats both as "not available", which is also how the DLL-side copies start out.
IIMEManager *g_pIMEManager = 0;
IInputStackSystem *g_pInputStackSystem = 0;
#endif
