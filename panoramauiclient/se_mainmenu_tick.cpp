//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: SE port - the frame hook for the ported CS:GO main menu panel class (batch E).
//
//          CS:GO ticks CCSGO_MainMenu from CGameUI::RunFrame() (game/client/cstrike15/gameui), which
//          this tree does not have.  engine.dll deliberately links no panorama library, so - like the
//          background-movie bridge next to this file - the tick is exported here and resolved by the
//          engine with GetProcAddress (see engine/panoramaenginehandler.cpp::PanoramaRunFrame).
//
//          The return value tells the engine whether the class is actually present: the layout has to
//          instantiate <CSGOMainMenu> for that, and when it does not (older content / a test layout)
//          the engine falls back to its own background-movie bridge.
//
//=============================================================================//

#include "stdafx_client.h"

// SE port: the ported headers expect the "cbase.h stand-in" to be included first (see the note at the
// top of se_gameclient_common.h) - ui_root.h gets CGameEventListener and the JS registration helpers
// from it.
#include "panorama/se_gameclient_common.h"
#include "panorama/csgo_mainmenu.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

// extern "C" so that GetProcAddress can find the undecorated name.
extern "C" __declspec( dllexport ) bool SE_PortMainMenuTick()
{
	CCSGO_MainMenu *pMainMenu = CCSGO_MainMenu::GetInstance();
	if ( !pMainMenu )
	{
		// The layout has to instantiate <CSGOMainMenu> for the class to exist.  When it does not (a test
		// layout, an older content pack) the engine falls back to its own background-movie bridge.
		return false;
	}

	pMainMenu->Update();
	return true;
}
