//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: SE port - the include set that stands in for CS:GO's game-client "cbase.h" in the ported
//          copies of game/client/panorama sources (they live under panorama/seport/gameclient/, see
//          docs/panorama_stage2_plan.md).
//
//          In CS:GO `#include "cbase.h"` in those files resolves to game/client/cbase.h, i.e. the
//          whole CS:GO game client.  This port has no game client of its own (the mod's client is
//          CS:S, game/client/cstrike), so the ported copies include this instead.  It provides
//          exactly what they use:
//            * the panorama client PCH (stdafx_client.h) - the same first include the other ported
//              game-client files use, e.g. panorama/seport/gameclient/csgo_blurtarget.cpp
//            * IGameEvent / CGameEventListener (CUI_Root derives from CGameEventListener)
//            * v8 (CUI_JS_Panel holds a v8::Persistent)
//
//          Keep this file small and explicit: anything added here is a dependency the port has to be
//          able to satisfy.
//
//=============================================================================//

#ifndef SE_GAMECLIENT_COMMON_H
#define SE_GAMECLIENT_COMMON_H
#pragma once

#include "stdafx_client.h"

// SE port: the engine-provided globals the ported sources use (gameeventmanager, gameuifuncs).  They
// live in their own small header so a module that only needs the globals - e.g. the
// panoramauiclient.cpp bootstrap that populates them from the app system factory - does not have to
// pull in this file (and with it the PCH and v8).
#include "panorama/se_gameclient_globals.h"

#include "game/shared/GameEventListener.h"

// SE port: ui_popup_manager.cpp's YldShowPopup() waits for a frame with
// `GCSDK::GJobCur().BYieldingWaitOneFrame()` (public/gcsdk/job.h).  This DLL cannot include that: the
// GCSDK headers do not include their own dependencies and, once public/gcsdk/gcbase.h pulls in the
// protobuf/tsmultimempool chain, they collide with the panorama_s1wrapper tier1 this module is built
// against (duplicate k_nMillion/... from the two tier0 copies, UTLMEMORYPOOL_GROW_FAST missing, ...).
// The call goes through this hook instead; the implementation and its limitation are documented in
// se_gameclient_globals.cpp.
void SE_PortYldWaitOneFrame();

#include "v8.h"

// CS:GO's cbase.h chain provides FStrEq via game/client/cdll_util.h (ui_root.cpp uses it for event
// names).  That header drags in the whole game client, so the same helper - same body, i.e. the
// pointer-equality shortcut plus a case-insensitive compare - is repeated here.
inline bool FStrEq( const char *sz1, const char *sz2 )
{
	return ( sz1 == sz2 || V_stricmp( sz1, sz2 ) == 0 );
}

// SE port: ui_popup_manager.h / popups/*.h use these two as default arguments.  In CS:GO they arrive
// through the PCH (game/shared/econ/econ_item_constants.h: `typedef uint8 style_index_t;` and
// game/shared/econ/econ_item_schema.h: `#define INVALID_STYLE_INDEX ((style_index_t)-1)`).  This port
// has no econ layer at all (game/shared/econ/ holds only ihasowner.h), and the only uses in the
// ported code are those declarations - EconItemIDs_t stays an incomplete type, because the CS:GO
// bodies that would dereference it (CUI_Popup_Generic::SetEconItemIconVisible) are inside
// `#if !defined( CSTRIKE15 )`.  The two definitions are therefore repeated verbatim.
typedef uint8 style_index_t;
#define INVALID_STYLE_INDEX ((style_index_t)-1)

// SE port: CUI_Root::ShowGenericPopup() uses CUI_PopupManager, which is batch C of
// docs/panorama_stage2_plan.md.
//   0 = the call compiles to a message instead (no link dependency on ui_popup_manager.cpp)
//   1 = the CS:GO code path is used as-is; batch C (ui_popup_manager.* + popups/* + cstrike15
//       csgo_popup_manager/csgo_globalpopups) is compiled into the DLL now, so this is on.
#define SE_PORT_HAVE_POPUP_MANAGER 1

// SE port: ui_root.cpp ends with a CUIRootGameSystem global that dispatches the panorama
// GameSystemInit / GameSystemShutdown events.  It derives from CAutoGameSystem
// (game/shared/igamesystem.h), a game-side framework class whose constructor is implemented in
// game/shared/igamesystem.cpp - a module this DLL does not link, and there is no game system loop in
// panoramauiclient.dll to drive it anyway.
//   0 = the global is not compiled and the two events are never dispatched.  Checked with a
//       tree-wide search: no script in the shipped CS:GO content registers for either event.
//   1 = compile it (only possible together with linking the game system framework)
#define SE_PORT_HAVE_GAMESYSTEM 0

#endif // SE_GAMECLIENT_COMMON_H
