//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: SE port - the engine-provided globals that the ported game-client sources expect.
//
//          Source 2013 defines these in the game client DLL (game/client/cdll_client_int.cpp):
//
//              IGameEventManager2 *gameeventmanager = NULL;   // "INTERFACEVERSION_GAMEEVENTSMANAGER2"
//              IGameUIFuncs *gameuifuncs = NULL;              // "VENGINE_GAMEUIFUNCS_VERSION"
//
//          both filled in from the app system factory the module's Connect() receives.  This port has
//          no game client DLL, so panoramauiclient.dll declares them here, defines them in
//          se_gameclient_globals.cpp and (mirroring cdll_client_int.cpp) fills them in from the
//          factory in CPanoramaUIClient::Connect().
//
//          Kept separate from se_gameclient_common.h on purpose: the bootstrap only needs the two
//          declarations, not the PCH/v8 include set.
//
//=============================================================================//

#ifndef SE_GAMECLIENT_GLOBALS_H
#define SE_GAMECLIENT_GLOBALS_H
#pragma once

#include "IGameUIFuncs.h"
#include "igameevents.h"
// IVEngineClient / VENGINE_CLIENT_INTERFACE_VERSION live in public/cdll_int.h in this tree (the
// engine exposes its implementation with EXPOSE_SINGLE_INTERFACE_GLOBALVAR in
// engine/cdll_engine_int.cpp).
#include "cdll_int.h"

// game/shared/GameEventListener.h needs this global from inside
// CGameEventListener::ListenForGameEvent().
extern IGameEventManager2 *gameeventmanager;

// public/panorama/uiinputcapture.h (used by ui_popup.h) calls into this global.
extern IGameUIFuncs *gameuifuncs;

// CS:GO's game client global of the same name: ui_popup_generic.cpp uses it to run the console
// command of a command popup (engine->ClientCmd_Unrestricted).
extern IVEngineClient *engine;

#endif // SE_GAMECLIENT_GLOBALS_H
