//========= Copyright (C) Valve Corporation, All rights reserved. ============//
//
// Purpose: Helper for access to global game related functions
//
// $NoKeywords: $
//
// SE port (2026-09-16): CS:GO's game/client/cstrike15/uicomponents/uicomponent_gameinterface.cpp,
// ported so that the layout scripts get a *real* "GameInterfaceAPI" instead of the inert stand-in in
// se_api_shim.js.  GetSettingString/SetSettingString read and write the archived ConVars through
// g_pCVar exactly like CS:GO, which is what makes the CS:GO menu's "UI preferences" work - the main
// menu background movie (ui_mainmenu_bkgnd_movie) is the first one that matters here.
//
// Only two deviations from CS:GO's file:
//   1. #include "cbase.h" -> the port's shared game-client header (the ported game/client sources all
//      do this; cbase.h does not exist in this tree).
//   2. GetSettingString() answers "" for a setting that has no ConVar.  CS:GO's cstrike15 client
//      registers every key the scripts ask for, so its NULL return was never observable; here an
//      unregistered key would reach the scripts as undefined and
//      common/promoted_settings.js (".split( ':' )" on the result) would abort the whole main-menu
//      bootstrap.  The shim contract this port has always had is "unknown setting = empty string".
//
//=============================================================================//

#include "panorama/se_gameclient_common.h"
#include "uicomponent_gameinterface.h"
#include "uicomponent_settings.h"
//#include "uicomponent_mypersona.h"

// SE port: for HasCommandLineParm() (see the port-only members at the end of this file).
#include "tier0/icommandline.h"

#ifdef WIN32
// SE port (2026-09-18): ShellExecuteA for OpenURLInBrowser() (see the end of this file).
#include "winlite.h"
#include <shellapi.h>
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

//////////////////////////////////////////////////////////////////////////
//
// Component events
//


//////////////////////////////////////////////////////////////////////////
//
// Component instance
//
SF_COMPONENT_API_DEF_BEGIN( CUiComponent_GameInterface )
#define UI_COMPONENT_FUNCTIONLIST_ELEMENT( returntype, fnname ) SF_COMPONENT_FUNCTION_API_DEF( returntype, fnname, CUiComponent_GameInterface )
#include "uicomponent_gameinterface.functions.inc"
#undef UI_COMPONENT_FUNCTIONLIST_ELEMENT
SF_COMPONENT_API_DEF_END( CUiComponent_GameInterface )

PANORAMA_COMPONENT_API_DEF_BEGIN( CUiComponent_GameInterface )
#define UI_COMPONENT_FUNCTIONLIST_ELEMENT( returntype, fnname ) PANORAMA_COMPONENT_MARSHALL_HELPER_FUNCTION_API_DEF( returntype, fnname, CUiComponent_GameInterface )
#include "uicomponent_gameinterface.functions.inc"
#undef UI_COMPONENT_FUNCTIONLIST_ELEMENT
PANORAMA_COMPONENT_API_DEF_END( CUiComponent_GameInterface )

UI_COMPONENT_API_DEF_COMMON( CUiComponent_GameInterface, GameInterface )


CUiComponent_GameInterface::CUiComponent_GameInterface()
{
}

CUiComponent_GameInterface::~CUiComponent_GameInterface()
{
}

UI_COMPONENT_FUNCTION_IMPL( CUiComponent_GameInterface, ConsoleCommand )
{
	const char* szCommand = pui->Params_GetArgAsString( obj, 0 );

	engine->ClientCmd_Unrestricted( szCommand );
}

// SE port: CS:GO's implementation lives in game/client/cstrike15/clientmode_csnormal.cpp, which this
// tree does not have; it is ported next to the ConVar it reads (panoramauiclient/se_ui_settings.cpp).
extern const char* Helper_GetMouseEnableBindingName();
UI_COMPONENT_FUNCTION_IMPL( CUiComponent_GameInterface, GetMouseEnableBindingName )
{
	pui->Params_SetResult( obj, Helper_GetMouseEnableBindingName() );
}

/////////////////////////////////////////////////////////////////////
// FUNCTION		GetSettingString
// ARGUMENTS	none
//
//////////////////////////////////////////////////////////////////////
static CUiSettingsAliasEntry_t Helper_LookupSettingsPreference( char const *szSettingKey )
{
	// Check if there is an aliasing policy in place?
	UtlSymId_t symPref = g_mapUiSettingsAliases.Find( szSettingKey );
	CUiSettingsAliasEntry_t entry;
	if ( symPref == UTL_INVAL_SYMBOL )
	{
		entry.m_pCvar = g_pCVar ? g_pCVar->FindVar( szSettingKey ) : NULL;
	}
	else
	{
		entry = g_mapUiSettingsAliases[ symPref ];
	}

	// Validate that we have a cvar?
	if ( !entry.m_pCvar )
	{
		DevWarning( "Failed to find ui preference '%s'"
			"\n", szSettingKey );
	}

	return entry;
}

//-----------------------------------------------------------------------------
// SE port: CS:GO's cstrike15 client registers a ConVar for every ui preference its scripts touch
// (ui_playsettings_mode_official, ui_playsettings_maps_official_<mode>, ui_playsettings_flags_*, ...),
// so writing one always landed in an existing, *engine wide* ConVar.  This port has no such client, so
// every write to an unregistered key was silently dropped:
//
//   * the play page's map selection is saved per server type + game mode, which is why switching the
//     game mode kept showing the old map list - nothing was remembered;
//   * the mode flags ConVar stayed empty, which sends StartSearch() into the "choose flags" popup
//     instead of the queue (mainmenu_play.js:96 / util_gamemodeflags.js);
//   * the scripts' own state (used by se_session_sim.js to keep one state for all layouts) had nowhere
//     to live, and every layout has its own JavaScript context.
//
// Creating the ConVar on first write restores that behaviour: the value is then readable from every
// JavaScript context through GameInterfaceAPI.GetSettingString().
//-----------------------------------------------------------------------------
static ConVar *Helper_CreateSettingsPreference( char const *szSettingKey )
{
	if ( !g_pCVar || !szSettingKey || !szSettingKey[ 0 ] )
		return NULL;

	// Only the scripts' own preferences are created on demand - never an engine ConVar the game may
	// want to define itself later with a meaningful default.
	if ( V_strncmp( szSettingKey, "ui_", 3 ) != 0 && V_strncmp( szSettingKey, "se_", 3 ) != 0 )
		return NULL;

	// SE port (2026-09-18): the ConVar keeps the *pointer* to its name ("Name should be static data",
	// ConCommandBase::CreateBase) - the key comes in as a JavaScript string that dies when this call
	// returns, so the name must be copied out first or no later FindVar() by name can ever match it.
	char *pszName = new char[ V_strlen( szSettingKey ) + 1 ];
	V_strcpy( pszName, szSettingKey );

	ConVar *pCreated = new ConVar( pszName, "", FCVAR_ARCHIVE, "User interface preference" );

	// SE port (2026-09-18): ConCommandBase::CreateBase() only registers the fresh ConVar when
	// ConCommandBase::s_pAccessor is set, and that happens in ConVar_Register() - a call this module
	// never makes, so a preference created at runtime stayed out of ICvar's lookup.  The effect was
	// that a write was never readable again: every SetSettingString() created the preference "again"
	// (the same "created ui preference ConVar" line repeating in the log) and every
	// GetSettingString() answered "".  The news panel read-then-compared ui_news_last_read_link on
	// every seed, always saw "", and put popup_news.xml on screen every time (16 seeds -> 16 stacked
	// popups whose close button looked dead).  Registering explicitly on the same ICvar the lookups
	// use restores the read-after-write behaviour all these scripts rely on.
	g_pCVar->RegisterConCommand( pCreated );

	Msg( "[SE port] created ui preference ConVar '%s'\n", pszName );
	return pCreated;
}
char const * CUiComponent_GameInterface::GetSettingString( char const *szSettingKey )
{
	CUiSettingsAliasEntry_t entry = Helper_LookupSettingsPreference( szSettingKey );
	if ( !entry.m_pCvar )
	{
		// SE port: see the note at the top - the scripts treat this as a string, never as undefined.
		return "";
	}

	// Bitfield policy?
	if ( entry.m_eBehavior == k_EUiSettingsAliasBehavior_BitField )
	{
		uint32 uiBitMask = entry.m_pCvar->GetInt();
		return ( entry.m_uiValue & uiBitMask ) ? "1" : "0";
	}
	else if ( entry.m_eBehavior == k_EUiSettingsAliasBehavior_TruncateUint64AsUint32 )
	{
		static CFmtStr s_fmt;
		uint32 uiLowerDword = entry.m_pCvar->GetInt();
		s_fmt.Format( "%llu", ( uint64( entry.m_uiValue ) << 32 ) | uiLowerDword );
		return s_fmt.Access();
	}
	else
	{
		// Just return the value
		return entry.m_pCvar->GetString();
	}
}
UI_COMPONENT_FUNCTION_IMPL( CUiComponent_GameInterface, GetSettingString )
{
	char const *szSettingKey = pui->Params_GetArgAsString( obj, 0 );
	if ( char const *szSettingValue = GetSettingString( szSettingKey ) )
		pui->Params_SetResult( obj, szSettingValue );
}

/////////////////////////////////////////////////////////////////////
// FUNCTION		SetUISavedDataKey
// ARGUMENTS	"key::value"
//
//////////////////////////////////////////////////////////////////////
void CUiComponent_GameInterface::SetSettingString( char const *szSettingKey, char const *szSettingValue )
{
	CUiSettingsAliasEntry_t entry = Helper_LookupSettingsPreference( szSettingKey );
	if ( !entry.m_pCvar )
	{
		// SE port: see Helper_CreateSettingsPreference - a first write registers the preference.
		ConVar *pCreated = Helper_CreateSettingsPreference( szSettingKey );
		if ( !pCreated )
			return;

		entry = CUiSettingsAliasEntry_t( pCreated );
	}

	// Bitfield policy?
	if ( entry.m_eBehavior == k_EUiSettingsAliasBehavior_BitField )
	{
		uint32 uiBitMask = entry.m_pCvar->GetInt();
		if ( szSettingValue && ( szSettingValue[ 0 ] == '1' ) )
			uiBitMask |= entry.m_uiValue;
		else
			uiBitMask &= ~entry.m_uiValue;
		entry.m_pCvar->SetValue( int( uiBitMask ) );
	}
	else if ( entry.m_eBehavior == k_EUiSettingsAliasBehavior_TruncateUint64AsUint32 )
	{
		entry.m_pCvar->SetValue( int( uint32( Q_atoui64( szSettingValue ) & 0xFFFFFFFF ) ) );
	}
	else
	{
		// Just set the value
		entry.m_pCvar->SetValue( szSettingValue );
	}

	// SE port probe (bring-up aid): the play page stores its "last used" play settings through this
	// call (ui_playsettings_mode_official / ui_playsettings_maps_<server>_<mode> / ...), so logging
	// them shows whether a mode or map click actually reached the settings layer - and that is only
	// visible in a log file, which a run started without -condebug does not produce.
	//
	// The "se_probe" keys come from the JS simulation layer (se_session_sim.js::log): it has no other
	// way out of a normal run either, so a lobby action (invite / join / leave) shows up here as a
	// "SIMPROBE ..." line.
	bool const bIsPlaySettings = szSettingKey && V_strnicmp( szSettingKey, "ui_playsettings", 15 ) == 0;
	bool const bIsSimProbe = szSettingKey && V_strnicmp( szSettingKey, "se_probe", 8 ) == 0;
	if ( bIsPlaySettings || bIsSimProbe )
	{
		static int s_nSESettingsProbe = 0;
		static int s_nSESimProbe = 0;
		int const nWritten = bIsSimProbe ? s_nSESimProbe : s_nSESettingsProbe;
		int const nMax = bIsSimProbe ? 400 : 200;
		if ( nWritten < nMax )
		{
			if ( bIsSimProbe ) { ++s_nSESimProbe; } else { ++s_nSESettingsProbe; }
			FILE *fp = fopen( "D:\\cstrike\\se_ui_probe.txt", "a" );
			if ( fp )
			{
				fprintf( fp, "%s %s = %s\n", bIsSimProbe ? "SIMPROBE" : "SETTING",
					szSettingKey, szSettingValue ? szSettingValue : "(null)" );
				fflush( fp );
				fclose( fp );
			}
		}
	}

// 	if ( entry.m_pCvar->GetFlags() & FCVAR_ARCHIVE )
// 	{
// 		// Schedule writing the config
// 		// (in case multiple preference are updated at the same time, we'll write out config to disk with a small delay, but a full batch at once)
// 		CUiComponent_MyPersona::GetInstance()->RequestDelayedHostWriteConfig( CFmtStr( "%s = %s", szSettingKey, szSettingValue ) );
// 	}
}
UI_COMPONENT_FUNCTION_IMPL( CUiComponent_GameInterface, SetSettingString )
{
	char const *szSettingKey = pui->Params_GetArgAsString( obj, 0 );
	char const *szSettingValue = pui->Params_GetArgAsString( obj, 1 );
	SetSettingString( szSettingKey, szSettingValue );
}

//-----------------------------------------------------------------------------
// SE port: two members the deployed content uses but the 2019 source drop does not have.  They used to
// be answered by se_api_shim.js' proxy stub (a truthy object), so the content never noticed; now that
// GameInterfaceAPI is a real object a missing member throws a TypeError, and the exception takes the
// rest of the script with it (mainmenu.js:64 -> "Skipping rest of script", which then left the menu up
// without the operation/season popup and the game quit a few seconds later).
//-----------------------------------------------------------------------------

// mainmenu.js:64 - "if ( GameInterfaceAPI.GetEngineSoundSystemsRunning() ) { _ShowOperationLaunchPopup(); }".
// CS:GO answers with the number of engine sound systems that are up; this port always has the engine
// sound system, and 1 is also what the previous stub produced (a truthy value), so the operation
// launch popup keeps the branch it has always taken here.
UI_COMPONENT_FUNCTION_IMPL( CUiComponent_GameInterface, GetEngineSoundSystemsRunning )
{
	pui->Params_SetResult( obj, 1 );
}

// popup_prime_status.js:5 - "!GameInterfaceAPI.HasCommandLineParm( '-forceperfectworld' )".
// Straightforward in this tree: the launcher's command line is available through CommandLine().
UI_COMPONENT_FUNCTION_IMPL( CUiComponent_GameInterface, HasCommandLineParm )
{
	const char *pchParm = pui->Params_GetArgAsString( obj, 0 );
	const bool bPresent = ( pchParm && pchParm[ 0 ] && CommandLine() && CommandLine()->FindParm( pchParm ) ) ? true : false;
	pui->Params_SetResult( obj, bPresent );
}

// SE port (2026-09-18): store/news links.  The ported scripts route SteamOverlayAPI.OpenURL and
// OpenUrlInOverlayOrExternalBrowser here (see se_session_sim.js) - with the Steam overlay missing, a
// no-op made the store's "市场" tile and every news entry look dead.  Opening the URL with the OS
// default handler keeps them functional for a Steam-less build.
UI_COMPONENT_FUNCTION_IMPL( CUiComponent_GameInterface, OpenURLInBrowser )
{
	const char *szURL = pui->Params_GetArgAsString( obj, 0 );
	if ( !szURL || !szURL[ 0 ] )
		return;

#ifdef WIN32
	// The JS side logs the click into the ui probe as well; this line only reaches engine.log with
	// -condebug and exists to make a ShellExecute failure (unknown protocol etc.) traceable.
	Msg( "SE port: OpenURLInBrowser '%s'\n", szURL );
	::ShellExecuteA( NULL, "open", szURL, NULL, NULL, SW_SHOWNORMAL );
#else
	( void )szURL;
#endif
}
