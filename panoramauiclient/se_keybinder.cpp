//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: SE port - the engine-side half of the settings keyboard binder.
//
//          CS:GO reaches the key binder from engine/keys.cpp::PanoramaHandleInputEvent(), which calls
//          g_ClientDLL->HandleBindWidgetInputCapture( event ) before it lets the UI see the event:
//          while a binder row is armed it must swallow the raw key/mouse event, otherwise the key
//          would also reach the game and the main menu would react to it as well.
//
//          This port has no CS:GO client DLL to hold that method, but the same code lives in this
//          module (panorama/seport/gameclient/cstrike15/panorama/popups/csgo_settings_keybinder.cpp),
//          so the engine resolves the export below out of this DLL's export table - the same
//          GetProcAddress bridge se_escape.cpp / se_mainmenu_tick.cpp use (engine.dll deliberately
//          links no panorama library).  See engine/panoramaenginehandler.cpp::SE_PortHandleKeyBinderInput().
//
//=============================================================================//

#include "stdafx_client.h"

#include "panorama/se_gameclient_common.h"
// SE port fix (2026-09-16): this file lives in panoramauiclient/, so the bare header name only
// resolves if the compiler searches the including file's own directory - spell the path the way the
// cstrike15 sources do (the wscript puts seport/gameclient/cstrike15 on the include path).
#include "panorama/popups/csgo_settings_keybinder.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

// extern "C" so that GetProcAddress can find the undecorated name.
extern "C" __declspec( dllexport ) bool SE_PortKeyBinderHandleInputEvent( const InputEvent_t &inputEvent )
{
	return CCSGO_SettingsKeyBinder::HandleInputEvent( inputEvent );
}

// Kept alongside HandleInputEvent(): the engine asks whether a binder is armed before it routes an
// event at all (CS:GO's IsBindWidgetCapturingInput()).
extern "C" __declspec( dllexport ) bool SE_PortKeyBinderIsCapturingInput()
{
	return CCSGO_SettingsKeyBinder::IsCapturingInput();
}
