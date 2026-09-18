//========= Copyright (C) Valve Corporation, All rights reserved. ============//
//
// Purpose: SE port - the game-side data the ported CUiComponent_GameInterface needs.
//
//   * g_mapUiSettingsAliases - CS:GO defines this in uicomponent_settings.cpp, which is the settings
//     *component* (a large list of UI_SETTINGS_CVAR registrations that this port does not need yet).
//     The map itself is all the gameinterface component touches, so it is defined here for now.  A
//     setting only needs an entry here when its UI alias differs in type from the ConVar (bitfield /
//     uint64-truncating); plain settings are resolved through g_pCVar->FindVar() by name.
//
//   * ui_mainmenu_bkgnd_movie - the ConVar behind the CS:GO main menu background movie
//     (settings_video.xml's background dropdown writes it, mainmenu.js::_SetBackgroundMovie reads it
//     and turns it into "file://{resources}/videos/<value>.webm").  CS:GO registers it outside the
//     panorama tree (its cstrike15 client), so it is registered here.  FCVAR_ARCHIVE so the choice
//     survives a restart, and settable from the console / command line:
//         ui_mainmenu_bkgnd_movie setest        (in game)
//         +ui_mainmenu_bkgnd_movie setest       (on the launcher's command line)
//
//   * Helper_GetMouseEnableBindingName - CS:GO has it in clientmode_csnormal.cpp (not in this tree);
//     ported verbatim next to the ConVar it reads.
//
//=============================================================================//

#include "panorama/se_gameclient_common.h"
#include "uicomponent_settings.h"
#include "uicomponent_gameinterface.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

CUtlStringMap< CUiSettingsAliasEntry_t > g_mapUiSettingsAliases;

//-----------------------------------------------------------------------------
// Purpose: which webm the CS:GO main menu plays behind its panels.  The value is the base name of a
//          file in <mod>/panorama/videos/.  Ships with the map-named ones (anubis, nuke, cbble, ...)
//          plus "setest" (the port's fast-moving test pattern, build/_mk_testvideo.ps1).
//-----------------------------------------------------------------------------
ConVar ui_mainmenu_bkgnd_movie( "ui_mainmenu_bkgnd_movie", "anubis720", FCVAR_ARCHIVE,
	"Main menu background movie (<mod>/panorama/videos/<value>.webm)" );

//------------------------------------------------------------------------------
// Purpose: scoreboard mouse-selection binding name, shown in the UI
//-----------------------------------------------------------------------------
ConVar cl_scoreboard_mouse_enable_binding( "cl_scoreboard_mouse_enable_binding", "+attack2", FCVAR_ARCHIVE, "Name of the binding to enable mouse selection in the scoreboard" );

//-----------------------------------------------------------------------------
// SE port (2026-09-18): the two switches for the startup popups.  They are plain ConVars of this
// module, so they need the explicit RegisterConCommand() in SE_PortInstallGameInterfaceBindings()
// below - see uicomponent_gameinterface.cpp::Helper_CreateSettingsPreference for why a ConVar of
// this module does not reach ICvar by itself.  Registration happens during SetupUIEngine(), which
// Host_Init calls before Host_ReadConfiguration(), so config.cfg / autoexec.cfg / the console can set
// them; the menu scripts read them through GameInterfaceAPI.GetSettingString().
//   se_popup_news    1 (default) = the news panel's "unread article" popup (popup_news.xml) comes up
//                    once per launch, as the content scripts intend; 0 = the demo feed is pre-marked
//                    read and the popup never comes up.
//   se_popup_legacy  1 (default) = the "Legacy version of CS:GO" notice comes up once per launch
//                    (the retail legacy branch's behaviour); 0 = never show it.
//-----------------------------------------------------------------------------
ConVar se_popup_news( "se_popup_news", "1", FCVAR_ARCHIVE,
	"Show the unread-news popup once per launch (1) or never (0)" );
ConVar se_popup_legacy( "se_popup_legacy", "1", FCVAR_ARCHIVE,
	"Show the 'Legacy version of CS:GO' notice once per launch (1) or never (0)" );

const char* Helper_GetMouseEnableBindingName()
{
	const char* szScoreboardKey = cl_scoreboard_mouse_enable_binding.GetString();

	// Hackily get the localization token for keys commonly bound (ie available from options menu)
	// or just pass the binding name otherwise. Not sure if there's a way to map binding->loc token
	// (guessing not as not all bindings have localized names?).
	if ( !V_stricmp( szScoreboardKey, "+attack2" ) )
	{
		szScoreboardKey = "#SFUI_WeaponSpecial";
	}
	else if ( !V_stricmp( szScoreboardKey, "+jump" ) )
	{
		szScoreboardKey = "#SFUI_Jump";
	}
	else if ( !V_stricmp( szScoreboardKey, "+duck" ) )
	{
		szScoreboardKey = "#SFUI_Duck";
	}
	else if ( !V_stricmp( szScoreboardKey, "+speed" ) )
	{
		szScoreboardKey = "#SFUI_Walk";
	}
	else if ( !V_stricmp( szScoreboardKey, "+use" ) )
	{
		szScoreboardKey = "#SFUI_Use";
	}

	return szScoreboardKey;
}


//-----------------------------------------------------------------------------
// Purpose: install the ported component's JavaScript global ("GameInterfaceAPI").
//          Called once from panoramauiclient.cpp::SetupUIEngine(), next to the UiToolkitAPI install.
//-----------------------------------------------------------------------------
void SE_PortInstallGameInterfaceBindings()
{
	static bool s_bInstalled = false;
	if ( s_bInstalled )
		return;

	if ( !panorama::UIEngine() )
	{
		Warning( "SE port: cannot install the GameInterfaceAPI bindings yet - no panorama UIEngine\n" );
		return;
	}

	s_bInstalled = true;

	IUiComponentGlobalInstanceBase *pGameInterface = CUiComponent_GameInterface::GetInstance();
	pGameInterface->InstallPanoramaBindings();

	// SE port (2026-09-18): hand the two popup switches to ICvar.  This runs inside
	// CPanoramaUIClient::SetupUIEngine(), which Host_Init calls before Host_ReadConfiguration(), so a
	// "se_popup_news 0" / "se_popup_legacy 0" line in config.cfg (or autoexec.cfg, or the console)
	// reaches the menu scripts - and FCVAR_ARCHIVE keeps the choice across launches.
	if ( g_pCVar )
	{
		g_pCVar->RegisterConCommand( &se_popup_news );
		g_pCVar->RegisterConCommand( &se_popup_legacy );
		Msg( "SE port: popup switches se_popup_news='%s' se_popup_legacy='%s'\n",
			se_popup_news.GetString(), se_popup_legacy.GetString() );
	}

	Msg( "SE port: installed the GameInterfaceAPI JS bindings (CUiComponent_GameInterface); "
		 "ui_mainmenu_bkgnd_movie='%s'\n", ui_mainmenu_bkgnd_movie.GetString() );
}
