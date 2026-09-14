//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: SE port - globals that the ported game-client panorama sources need but that Source 2013
//          defines in other modules (see the individual notes below).
//
//=============================================================================//

#include "panorama/se_gameclient_common.h"

// game/shared/GameEventListener.h declares this and uses it inside CGameEventListener::
// ListenForGameEvent(); Source 2013 defines it in game/client/cdll_client_int.cpp and
// game/server/gameinterface.cpp, neither of which is linked into panoramauiclient.dll.
//
// NULL is safe - ListenForGameEvent() simply skips gameeventmanager->AddListener() - so CUI_Root just
// does not receive "colorblind_mode_changed" / "cs_match_end_restart" yet.  Wiring it up means asking
// the engine for its game event manager (INTERFACEVERSION_GAMEEVENTSMANAGER2 through the app system
// factory that this module's Connect() receives).
IGameEventManager2 *gameeventmanager = NULL;
