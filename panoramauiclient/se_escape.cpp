//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: SE port - what the hosted panorama UI does when the escape key reaches it.
//
//          CS:GO gets this from its GameUI: there the GameUI *is* the panorama client UI
//          (game/client/cstrike15/gameui/gameui_interface.cpp - CGameUI is exposed as
//          GAMEUI_INTERFACE_VERSION), so "escape cancels the current screen" belongs to it: the top popup
//          is closed and the content's scripts see the standard panel "Cancelled" event
//          (mainmenu.js::_OnEscapeKeyPressed, mainmenu_inventory.js::ClosePopups, chat.js::Close,
//          mainmenu_watch.js::CloseSubMenuContent).
//
//          This fork still ships gameui.dll (the CS:S VGUI2 menu) and has no panorama GameUI yet - see the
//          "task B" note in engine/panoramaenginehandler.h.  Until it has one, the engine asks the
//          panorama module instead: engine/keys.cpp::PanoramaHandleInputEvent() calls
//          SE_PortHandlePanoramaEscape() (engine/panoramaenginehandler.cpp), which resolves the export
//          below out of this DLL's export table (engine.dll deliberately links no panorama library).
//
//=============================================================================//

#include "stdafx_client.h"

// SE port: the ported game-client headers expect the "cbase.h stand-in" to be included first (see the note
// at the top of se_gameclient_common.h).
#include "panorama/se_gameclient_common.h"
#include "panorama/ui_root.h"
#include "panorama/ui_popup_manager.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

// extern "C" so that GetProcAddress can find the undecorated name.
extern "C" __declspec( dllexport ) bool SE_PortPanoramaEscapePressed()
{
	// 1) An open popup is what escape cancels first.  A generic popup has no Cancelled handler of its own
	//    (popups/popup_generic.xml has no oncancel), so closing it is up to whoever owns the screen - in
	//    CS:GO that is the GameUI, here it is us.
	for ( int i = 0; i < CUI_Root::GetRootCount(); ++i )
	{
		CUI_Root *pRoot = CUI_Root::GetRootByIndex( i );
		CUI_PopupManager *pPopupManager = pRoot ? pRoot->GetPopupManager() : NULL;
		if ( pPopupManager && pPopupManager->IsAnyPopupVisible() )
		{
			pPopupManager->CloseAllVisiblePopups();
			return true;
		}
	}

	// 2) Nothing open: hand the key to the focused panel as the usual panel "Cancelled" event, which is
	//    exactly what the content registers for.  Escaping the sub-menu / going back home / resuming a
	//    paused game are all in those JS handlers.
	panorama::IUIWindow *pWindow = panorama::UIEngine() ? panorama::UIEngine()->GetFocusedWindow( false ) : NULL;
	panorama::IUIPanel *pFocus = pWindow ? pWindow->UIWindowInput()->GetInputFocus() : NULL;
	if ( pFocus )
	{
		if ( panorama::DispatchEvent( panorama::Cancelled(), pFocus, panorama::k_ePanelEventSourceKeyboard ) )
		{
			return true;
		}
	}

	// Escape stays unhandled: the engine then lets the remaining filters (VGUI, engine) see it, which is
	// what keeps a plain CS:S install working.
	return false;
}
