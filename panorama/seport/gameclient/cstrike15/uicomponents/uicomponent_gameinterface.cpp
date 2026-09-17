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
		return;

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
