//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: SE port - globals that the ported game-client panorama sources need but that Source 2013
//          defines in other modules (see the individual notes below).
//
//=============================================================================//

#include "panorama/se_gameclient_common.h"

// Both globals are declared in se_gameclient_globals.h and filled in from the app system factory by
// CPanoramaUIClient::Connect() (panoramauiclient/panoramauiclient.cpp), exactly like Source 2013's
// game/client/cdll_client_int.cpp does for the game client DLL.

// game/shared/GameEventListener.h declares this and uses it inside CGameEventListener::
// ListenForGameEvent(); Source 2013 defines it in game/client/cdll_client_int.cpp and
// game/server/gameinterface.cpp, neither of which is linked into panoramauiclient.dll.
//
// While it is NULL, ListenForGameEvent() simply skips gameeventmanager->AddListener(), so CUI_Root
// just does not receive "colorblind_mode_changed" / "cs_match_end_restart".  Connect() now asks the
// engine for it (INTERFACEVERSION_GAMEEVENTSMANAGER2).
IGameEventManager2 *gameeventmanager = NULL;

// public/panorama/uiinputcapture.h dereferences it from CGameInputCapture::Enable()/Disable(), which
// CUI_Popup uses (ui_popup.cpp) to take/give back game input while a popup is up.  The engine side is
// implemented in engine/sys_dll2.cpp (CGameUIFuncs::PanoramaAddGameInputHandler); Connect() obtains
// the interface from the same factory the engine hands us, so popups get real input capture.
IGameUIFuncs *gameuifuncs = NULL;

// CS:GO's game client global of the same name (game/client/cdll_client_int.cpp), taken from the app
// system factory in Connect() as VENGINE_CLIENT_INTERFACE_VERSION.
IVEngineClient *engine = NULL;

// SE port: stands in for GCSDK::GJobCur().BYieldingWaitOneFrame() in ui_popup_manager.cpp's
// YldShowPopup() (CS:GO's yielding popup API).
//
// CS:GO's GCSDK job system is not linked into this DLL (see the note in se_gameclient_common.h), and
// no job/coroutine context exists in the port either: CUI_PopupManager::YldShowPopup() is only
// reached from CS:GO game code running inside a GC job, and this port has no such caller - the
// shipped CS:GO scripts drive popups through the non-yielding UIShowGenericPopup* events, which land
// in ShowPopup()/ShowPopupAsync() and work normally.
//
// So the honest behaviour here is: there is nothing to yield to, report it if it ever happens, and
// return so the UI thread keeps running (a Sleep-based wait would wedge the whole panorama frame
// loop, as YldShowPopup() is called from the UI thread).
void SE_PortYldWaitOneFrame()
{
	AssertMsg( false, "SE port: yielding popup API (YldShowPopup) was called, but this port has no GCSDK job to yield to." );
}
