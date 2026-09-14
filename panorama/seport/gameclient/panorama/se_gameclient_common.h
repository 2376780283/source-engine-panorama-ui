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

#include "igameevents.h"
#include "game/shared/GameEventListener.h"

// game/shared/GameEventListener.h needs this global; Source 2013 defines it in the game/client and
// game/server DLLs, this port defines its own copy in se_gameclient_globals.cpp.
extern IGameEventManager2 *gameeventmanager;

#include "v8.h"

// CS:GO's cbase.h chain provides FStrEq via game/client/cdll_util.h (ui_root.cpp uses it for event
// names).  That header drags in the whole game client, so the same helper - same body, i.e. the
// pointer-equality shortcut plus a case-insensitive compare - is repeated here.
inline bool FStrEq( const char *sz1, const char *sz2 )
{
	return ( sz1 == sz2 || V_stricmp( sz1, sz2 ) == 0 );
}

// SE port: CUI_Root::ShowGenericPopup() uses CUI_PopupManager, which belongs to the popup batch (C)
// of docs/panorama_stage2_plan.md and is not ported yet.
//   0 = the call compiles to a message instead (no link dependency on ui_popup_manager.cpp)
//   1 = the CS:GO code path is used as-is; flip this together with porting ui_popup_manager.*
#define SE_PORT_HAVE_POPUP_MANAGER 0

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
