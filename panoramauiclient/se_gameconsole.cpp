//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: SE port (2026-09-18) - the engine-facing half of "the console lives in the panorama module".
//
//          CS:GO's engine gets IGameConsole from the client-side GameUI module: CEngineVGui::Init()
//          does m_GameUIFactory(GAMECONSOLE_INTERFACE_VERSION), and m_GameUIFactory is g_ClientFactory
//          unless -gameuidll is given.  The console class itself is
//          game/client/cstrike15/gameui/gameconsole.cpp - the same module that hosts panorama.
//
//          This module plays that role here.  The engine asks for the export below first
//          (engine/panoramaenginehandler.cpp::SE_PortGetPanoramaGameConsole, called from
//          engine/vgui_baseui_interface.cpp::CEngineVGui::Init) and only falls back to the CS:S
//          gameui.dll console when this returns NULL.
//
//          The console panel is a vgui2 panel (vgui_controls::CConsoleDialog), so this module has to
//          bind the vgui2 interfaces the same way gameui.dll does in CGameUI::Initialize()
//          (gameui/GameUI_Interface.cpp: "vgui::VGui_InitInterfacesList / VGui_InitMatSysInterfacesList")
//          before the panel can be created.  The factory to use is the app system factory this module's
//          Connect() received - the engine binds its own vgui interfaces from the same one
//          (engine/vgui_baseui_interface.cpp: "VGui_InitMatSysInterfacesList( "BaseUI", &g_AppSystemFactory, 1 )").
//
//=============================================================================//

#include "stdafx_client.h"

#include "panorama/se_gameclient_globals.h"

// The console class (CS:GO's cstrike15/gameui/gameconsole.cpp, ported).
#include "panorama/seport/gameclient/cstrike15/gameui/gameconsole.h"

// GameUI/IGameConsole.h - the interface version the engine asks for.
#include "GameUI/IGameConsole.h"

// vgui::VGui_InitInterfacesList - checks that this module's vgui interface globals are set.
// NOTE: gameui.dll and the engine call the *MatSys* variant (VGui_InitMatSysInterfacesList) because they
// also use the matsys_controls panels; the console only needs the core vgui interfaces, so the base call
// is used here and the matsys_controls library is not dragged into this module.  If a panel ever turns
// out to need one of the matsys globals, switch to the MatSys call and link matsys_controls (that is how
// gameui/wscript does it).
#include "vgui_controls/Controls.h"
// ConnectTier3Libraries + the vgui::g_pVGui* globals it fills (see SE_PortEnsureVGuiInterfaces).
#include "tier3/tier3.h"
// The interface version strings the vgui lookups below use.
#include "vgui/IVGui.h"
#include "vgui/IInput.h"
#include "vgui/IPanel.h"
#include "vgui/ISurface.h"
#include "vgui/IScheme.h"
#include "vgui/ISystem.h"
#include "vgui/ILocalize.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#include <stdio.h>

// SE port: stored by CPanoramaUIClient::Connect() (definition in panorama_s1wrapper/wrap_sound.cpp, the
// same global the UI sound ops retry their interface lookups with).
extern CreateInterfaceFn g_pPanoramaConnectFactory;

// SE port (bring-up probe): defined in the ported gameconsole.cpp (shared with this file).
// void SE_PortConsoleProbe( const char *pFmt, ... );   -- declared in gameconsole.h

//-----------------------------------------------------------------------------
// Purpose: bind this module's vgui2 interfaces (once).  Returns false when the factory cannot provide
//          them - the engine then keeps the CS:S gameui.dll console.
//-----------------------------------------------------------------------------
static bool SE_PortEnsureVGuiInterfaces()
{
	static int s_nState = 0;	// 0 = not tried, 1 = ok, -1 = failed
	if ( s_nState != 0 )
		return ( s_nState == 1 );

	if ( !g_pPanoramaConnectFactory )
	{
		SE_PortConsoleProbe( "no app system factory yet\n" );
		Warning( "SE port: no app system factory yet, cannot bind the panorama console's vgui interfaces\n" );
		return false;
	}

	// The vgui interface globals vgui_controls checks (g_pVGui/g_pVGuiInput/g_pVGuiPanel/g_pVGuiSurface/
	// g_pVGuiSchemeManager/g_pVGuiSystem, plus g_pVGuiLocalize for the console's "#Console_Title") are
	// only ever assigned by ConnectTier3Libraries().  This module never called it - see the
	// commented-out s_pDependencies in panoramauiclient.cpp - so without this, VGui_InitInterfacesList()
	// bails out with "vgui_controls is missing a required interface!" and the engine keeps the CS:S
	// gameui.dll console.  gameui.dll fills the same set (through ConnectTier1/2/3Libraries in
	// gameui/GameUI_Interface.cpp::CGameUI::Initialize); only the vgui ones are filled in here, because
	// the rest of tier3 (material system / mdl cache / studiorender) has never been connected in this
	// module and publishing those globals could change other code's behaviour.
	CreateInterfaceFn fnFactory = g_pPanoramaConnectFactory;
	if ( !g_pVGui )
		g_pVGui = (vgui::IVGui *)fnFactory( VGUI_IVGUI_INTERFACE_VERSION, NULL );
	if ( !g_pVGuiInput )
		g_pVGuiInput = (vgui::IInput *)fnFactory( VGUI_INPUT_INTERFACE_VERSION, NULL );
	if ( !g_pVGuiPanel )
		g_pVGuiPanel = (vgui::IPanel *)fnFactory( VGUI_PANEL_INTERFACE_VERSION, NULL );
	if ( !g_pVGuiSurface )
		g_pVGuiSurface = (vgui::ISurface *)fnFactory( VGUI_SURFACE_INTERFACE_VERSION, NULL );
	if ( !g_pVGuiSchemeManager )
		g_pVGuiSchemeManager = (vgui::ISchemeManager *)fnFactory( VGUI_SCHEME_INTERFACE_VERSION, NULL );
	if ( !g_pVGuiSystem )
		g_pVGuiSystem = (vgui::ISystem *)fnFactory( VGUI_SYSTEM_INTERFACE_VERSION, NULL );
	if ( !g_pVGuiLocalize )
		g_pVGuiLocalize = (vgui::ILocalize *)fnFactory( VGUI_LOCALIZE_INTERFACE_VERSION, NULL );

	SE_PortConsoleProbe( "factory=%p vgui=%p input=%p panel=%p surface=%p scheme=%p system=%p localize=%p\n",
		(void *)g_pPanoramaConnectFactory, (void *)g_pVGui, (void *)g_pVGuiInput,
		(void *)g_pVGuiPanel, (void *)g_pVGuiSurface, (void *)g_pVGuiSchemeManager,
		(void *)g_pVGuiSystem, (void *)g_pVGuiLocalize );

	// NOTE: same list gameui.dll and the engine initialize (gameui/GameUI_Interface.cpp::
	// CGameUI::Initialize, engine/vgui_baseui_interface.cpp::CEngineVGui::Init) - they call the MatSys
	// variant, this module the base one (see the include note above).
	bool bOK = vgui::VGui_InitInterfacesList( "PanoramaGameUI", &g_pPanoramaConnectFactory, 1 );
	SE_PortConsoleProbe( "VGui_InitInterfacesList -> %d\n", (int)bOK );

	if ( !bOK )
	{
		Warning( "SE port: vgui interfaces unavailable - the panorama console stays disabled\n" );
		s_nState = -1;
		return false;
	}

	s_nState = 1;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: hand the engine the console, CS:GO style.  The engine calls Initialize() on the returned
//          object (engine/vgui_baseui_interface.cpp), which creates the panel - that is why the vgui
//          interfaces have to be bound here, before returning.
//
//          GetProcAddress entry point (undecorated name on purpose): engine.dll links no panorama
//          library, so it resolves this out of this DLL's export table - same pattern as
//          SE_PortPanoramaEscapePressed (se_escape.cpp) and SE_PortMainMenuTick (se_mainmenu_tick.cpp).
//-----------------------------------------------------------------------------
extern "C" __declspec( dllexport ) IGameConsole *SE_PortGetGameConsole()
{
	if ( !SE_PortEnsureVGuiInterfaces() )
		return NULL;

	// CS:GO's gameui.dll registers condump against its console (gameui/GameConsole.cpp: CON_COMMAND at
	// the bottom); CS:GO's cstrike15 console does the same from gameconsole.cpp.  Move it over - the CS:S
	// gameui console is never initialized in this configuration, so leaving it there would leave condump
	// with nothing to dump.
	SE_PortRegisterConsoleCommands();

	SE_PortConsoleProbe( "returning console %p\n", (void *)&GameConsole() );

	Msg( "SE port: IGameConsole provided by panoramauiclient.dll (the console lives in the panorama "
		 "module, like CS:GO's cstrike15/gameui console in client.dll)\n" );
	return &GameConsole();
}
