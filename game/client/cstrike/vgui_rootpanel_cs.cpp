//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "vgui_int.h"
#include "ienginevgui.h"
#include "c_csrootpanel.h"

#include "se_background_video.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

C_CSRootPanel *g_pCSRootPanel = NULL;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VGUI_CreateClientDLLRootPanel( void )
{
	g_pCSRootPanel = new C_CSRootPanel( enginevgui->GetPanel( PANEL_CLIENTDLL ) );

	// SE port: webm main menu background, drawn behind the stock menu panels (see
	// cstrike/se_background_video.cpp).  It is a child of the client root panel, so it replaces the
	// 3D menu background map and the translucent menu panels stay on top of it.
	SE_CreateBackgroundMoviePanel( g_pCSRootPanel );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VGUI_DestroyClientDLLRootPanel( void )
{
	SE_DestroyBackgroundMoviePanel();

	delete g_pCSRootPanel;
	g_pCSRootPanel = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Game specific root panel
// Output : vgui::Panel
//-----------------------------------------------------------------------------
vgui::VPANEL VGui_GetClientDLLRootPanel( void )
{
	return g_pCSRootPanel->GetVPanel();
}