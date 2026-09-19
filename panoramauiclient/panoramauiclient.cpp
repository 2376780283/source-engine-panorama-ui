//====== Copyright �?2014-2015, Valve Corporation, All rights reserved. =======
//
// Purpose: IPanoramaUIClient app system implementation
//
//=============================================================================

// SE port: the CS:GO project compiled this file with a precompiled header.  Include the panorama
// client PCH explicitly: among other things it force-includes the CS:GO container headers
// (tier1/utlrbtree.h & utlmap.h from panorama_s1wrapper/tier1) *before* anything can pull in the
// Source 2013 versions - they share the UTLRBTREE_H/UTLMAP_H guards, and the Source 2013
// utlrbtree.h has no CompareOperands_t that panoramauiclient's headers need.
#include "stdafx_client.h"
// SE port: the two engine-provided globals the ported game-client sources need (gameuifuncs,
// gameeventmanager), filled in from the app system factory in Connect() below.
#include "panorama/se_gameclient_globals.h"
// SE port (temporary bring-up probe): the process exits silently while the UI engine is being set up,
// and Warning() output is lost with it, so these probes append straight to a file.

#ifdef PLATFORM_WINDOWS
#include "windows.h"
// SE port (temporary bring-up probe): the process exits silently while the UI engine is being set up,
// and Warning() output is lost with it, so these probes append straight to a file.
#endif

#include "interfaces/interfaces.h"
// SE port (temporary bring-up probe): the process exits silently while the UI engine is being set up,
// and Warning() output is lost with it, so these probes append straight to a file.
#include "filesystem.h"
// SE port (temporary bring-up probe): the process exits silently while the UI engine is being set up,
// and Warning() output is lost with it, so these probes append straight to a file.
#include "tier1/utldelegate.h"
// SE port (temporary bring-up probe): the process exits silently while the UI engine is being set up,
// and Warning() output is lost with it, so these probes append straight to a file.
#include "tier1/KeyValues.h"
// SE port (temporary bring-up probe): the process exits silently while the UI engine is being set up,
// and Warning() output is lost with it, so these probes append straight to a file.
#include "panorama/controls/panel2d.h"
// SE port (temporary bring-up probe): the process exits silently while the UI engine is being set up,
// and Warning() output is lost with it, so these probes append straight to a file.
#include "../panorama/controls/debug/debugger.h"
// SE port (temporary bring-up probe): the process exits silently while the UI engine is being set up,
// and Warning() output is lost with it, so these probes append straight to a file.
#include "panoramauiclient.h"
// SE port (temporary bring-up probe): the process exits silently while the UI engine is being set up,
// and Warning() output is lost with it, so these probes append straight to a file.
#include "panorama/uisettings.h"
// SE port (temporary bring-up probe): the process exits silently while the UI engine is being set up,
// and Warning() output is lost with it, so these probes append straight to a file.
#include "panorama/panoramatypes.h"
// SE port (UI sounds): provides IEngineSound and IENGINESOUND_CLIENT_INTERFACE_VERSION for the sound
// interfaces that Connect() below hands to the s1wrapper sound ops ("IEngineSoundClient003", which
// is what this tree's engine.dll publishes - see engine/EngineSoundClient.cpp).
#include "engine/IEngineSound.h"
// SE port (batch E): installs the cstrike15 UI component JS bindings (the "UiToolkitAPI" global).
#include "se_uicomponents.h"
// SE port (temporary bring-up probe): the process exits silently while the UI engine is being set up,
// and Warning() output is lost with it, so these probes append straight to a file.

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"
// SE port (temporary bring-up probe): the process exits silently while the UI engine is being set up,
// and Warning() output is lost with it, so these probes append straight to a file.

// SE port: with PANORAMA_EXPORTS defined (see the note in the wscript) public/panorama/iuiengine.h
// does not declare the client bootstrap API; it is defined in panorama/uiengineclient.cpp.
namespace panorama { extern void ConnectPanoramaUIEngine( IUIEngine *pEngine ); }

CPanoramaUIClient s_PanoramaUIClient;
CPanoramaUIClient *g_pPanoramaUIClientImpl = &s_PanoramaUIClient;

// SE port: public/interfaces/interfaces.h declares g_pPanoramaUIEngine as an extern tier3
// interface (DECLARE_TIER3_INTERFACE).  In CS:GO the engine module owns that global; here the
// panorama client proxies it, so provide the definition until the engine integration (M4) lands.
// It is fill in by CPanoramaUIClient::Connect() below.
IPanoramaUIEngine *g_pPanoramaUIEngine = NULL;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CPanoramaUIClient, IPanoramaUIClient, PANORAMAUI_CLIENT_INTERFACE_VERSION, s_PanoramaUIClient )

// SE port (2026-09-14): the main-menu background movie entry point (se_background_movie.cpp) is
// exported for engine.dll, which hosts the menu but links no panorama library.  It is compiled into
// this module's source list rather than the panorama static library: a dllexport inside a static lib
// has no effect, because an unreferenced object is never pulled into the link.

// Trivial thunk class to virtualize access to a CDebugger.
class CPanoramaClientDebugger : public IPanoramaClientDebugger
{
public:
    CPanoramaClientDebugger( panorama::CDebugger *pDebugger )
    {
        m_pDebugger = pDebugger;
    }
    virtual ~CPanoramaClientDebugger()
    {
        delete m_pDebugger;
    }

	virtual void BeginInspect() OVERRIDE
    {
        return m_pDebugger->BeginInspect();
    }

	virtual void ForceEndInspect() OVERRIDE
	{
		return m_pDebugger->ForceEndInspect();
	}


	virtual float GetSplitterPosition() OVERRIDE
    {
        return m_pDebugger->GetSplitterPosition();
    }
    
	virtual void SetSplitterPosition( float flParentFlowValue ) OVERRIDE
    {
        m_pDebugger->SetSplitterPosition( flParentFlowValue );
    }

private:
    panorama::CDebugger *m_pDebugger;
};

// Dummy singleton to implement panorama's IUISettings class. Used to pass through the language correctly
class CPanoramaUISettings : public panorama::IUISettings
{
public:
	static CPanoramaUISettings &Get()
	{
		static CPanoramaUISettings s_instance;
		return s_instance;
	}

	void SetUILanguage( const char *pszLanguage ) { m_strLanguage = pszLanguage; }
	virtual const char *GetUILanguage() OVERRIDE { return m_strLanguage; }

	virtual void GetPreferredResolution( int &dxPreferred, int &dyPreferred ) OVERRIDE
	{
		dxPreferred = 1920;
		dyPreferred = 1080;
	}

	virtual void UpdateGamepadMappingHints( const char *pszConnectedMappings ) OVERRIDE{}
	virtual bool BAllowOSModalDialog() OVERRIDE { return false; }

	virtual void GetDefaultAudioDevice( CUtlString &strDevice, CUtlString &strPort, CUtlString &strProfile ) OVERRIDE { strDevice = strPort = strProfile = ""; }
	virtual void SetDefaultAudioDevice( const char *pchDevice ) OVERRIDE { Assert( false ); }
	virtual void SetDefaultAudioPort( const char *pchPort ) OVERRIDE { Assert( false ); }
	virtual void SetDefaultAudioProfile( const char *pchProfile ) OVERRIDE{ Assert( false ); }

	virtual void GetDefaultVoiceDevice( CUtlString &strDevice ) OVERRIDE { strDevice = ""; }
	virtual void SetDefaultVoiceDevice( const char *pchDevice ) OVERRIDE { Assert( false ); }

	virtual bool GetUseSystemAudioForVoice() OVERRIDE { return true; }
	virtual void SetUseSystemAudioForVoice( bool bUseSystemAudio ) OVERRIDE { Assert( false ); }

	virtual panorama::ETextInputHandlerType_t GetActiveTextInputHandlerType() const OVERRIDE{ return panorama::k_ETextInputHandlerType_DaisyWheel; }
	virtual void SetDefaultTextInputHandlerType( panorama::ETextInputHandlerType_t eType ) OVERRIDE { Assert( false ); }

	virtual unsigned int /* CTextInputDualTouch::EDualtouchSuggestionMode */ GetOnScreenKeyboardSuggestionMode() const OVERRIDE { return 0; };
	virtual void SetOnScreenKeyboardSuggestionMode( unsigned int /* CTextInputDualTouch::EDualtouchSuggestionMode */ eSuggestionMode ) { };

	virtual ELanguage GetDefaultInputLanguage() const { return k_Lang_English; }
	virtual void SetDefaultInputLanguage( ELanguage eLanguage ) OVERRIDE{ Assert( false ); }

	virtual int GetDualTouchTutorialCompletionCount() const { return 0; }
	virtual void OnDualTouchTutorialCompleted() OVERRIDE { Assert( false ); }

private:
	CUtlString m_strLanguage;
};

// SE port: CS:GO's IAppSystem exposes a module dependency list (GetDependencies()); SE 2013's
// does not, and the engine's app system group resolves interfaces itself.  Kept for reference.
// static AppSystemInfo_t s_pDependencies[] =
// {
// #ifdef PANORAMA_USE_S1WRAPPER
// 	{ "filesystem_stdio" DLL_EXT_STRING	, ASYNCFILESYSTEM_INTERFACE_VERSION },
// 	{ "panorama" DLL_EXT_STRING			, PANORAMAUI_ENGINE_INTERFACE_VERSION },
// #else
// 	{ "panorama", PANORAMAUI_ENGINE_INTERFACE_VERSION },
// #endif
// 	{ NULL, NULL }
// };

CPanoramaUIClient::CPanoramaUIClient()
{
	// 7ls RequireKeyValuesSystem();
}

bool CPanoramaUIClient::Connect( CreateInterfaceFn factory )
{
	if ( !BaseClass::Connect( factory ) )
		return false;

	g_pPanoramaUIEngine = (IPanoramaUIEngine*)factory( PANORAMAUI_ENGINE_INTERFACE_VERSION, NULL );

	// SE port (batch C): the ported game-client sources need the same two engine globals Source 2013's
	// game client DLL grabs from the app system factory (game/client/cdll_client_int.cpp:920 and its
	// gameeventmanager lookup), so ask for them here - same versions, same order, same NULL tolerance.
	//
	//   gameuifuncs     - public/panorama/uiinputcapture.h dereferences it from
	//                     CGameInputCapture::Enable() (CUI_Popup takes game input while it is up);
	//                     the engine implementation lives in engine/sys_dll2.cpp (CGameUIFuncs).
	//   gameeventmanager- game/shared/GameEventListener.h uses it from ListenForGameEvent().
	gameuifuncs = (IGameUIFuncs *)factory( VENGINE_GAMEUIFUNCS_VERSION, NULL );
	gameeventmanager = (IGameEventManager2 *)factory( INTERFACEVERSION_GAMEEVENTSMANAGER2, NULL );
	//   engine          - ui_popup_generic.cpp runs the console command of a command popup through it
	//                     (ClientCmd_Unrestricted); the engine exposes its implementation as
	//                     VENGINE_CLIENT_INTERFACE_VERSION (engine/cdll_engine_int.cpp).
	engine = (IVEngineClient *)factory( VENGINE_CLIENT_INTERFACE_VERSION, NULL );

	//   enginesound / soundemittersystem - the panorama UI sound system (CUISoundSystem::PlaySound in
	//                     panorama/uisoundsystem.cpp) plays CS:GO's "UIPanorama.*" sound-script entries
	//                     through the s1wrapper sound ops (panorama_s1wrapper/wrap_sound.cpp).  Those
	//                     ops read g_pEnginesound and g_pSoundEmitterSystemBase, which this port had
	//                     declared and defined but never assigned - every PlaySoundEffect from the
	//                     content died on the wrapper's NULL check and the whole UI was silent.
	//                     The sound emitter system is loaded by the game client app system group
	//                     (game/client/cdll_client_int.cpp AddAppSystem "soundemittersystem") and may
	//                     not have connected yet, so the factory is also handed to wrap_sound.cpp,
	//                     which retries the lookups on first use.  Version strings: engine.dll
	//                     publishes IEngineSoundClient003 (engine/EngineSoundClient.cpp), and the
	//                     sound emitter system publishes VSoundEmitter002, the version that
	//                     public/SoundEmitterSystem/isoundemittersystembase.h declares (the
	//                     "VSoundEmitter003" in public/interfaces/interfaces.h is stale, hence the
	//                     literal below).
	extern class IEngineSound *g_pEnginesound;
	extern class ISoundEmitterSystemBase *g_pSoundEmitterSystemBase;
	extern CreateInterfaceFn g_pPanoramaConnectFactory;
	g_pPanoramaConnectFactory = factory;
	g_pEnginesound = (IEngineSound *)factory( IENGINESOUND_CLIENT_INTERFACE_VERSION, NULL );
	g_pSoundEmitterSystemBase = (ISoundEmitterSystemBase *)factory( "VSoundEmitter002", NULL );

	Msg( "panoramauiclient: gameuifuncs=%p gameeventmanager=%p engine=%p enginesound=%p soundemittersystem=%p\n",
		(void *)gameuifuncs, (void *)gameeventmanager, (void *)engine, (void *)g_pEnginesound, (void *)g_pSoundEmitterSystemBase );

	return true;

}

// SE port: see the note above the (commented out) s_pDependencies table.
// const AppSystemInfo_t *CPanoramaUIClient::GetDependencies()
// {
// 	return s_pDependencies;
// }

void CPanoramaUIClient::Disconnect()
{
	ShutdownUIEngine();
	BaseClass::Disconnect();
}

void *CPanoramaUIClient::QueryInterface( const char *pInterfaceName )
{
	if ( !V_strncmp( pInterfaceName, PANORAMAUI_CLIENT_INTERFACE_VERSION, V_strlen( PANORAMAUI_CLIENT_INTERFACE_VERSION ) + 1 ) )
	{
		return static_cast<IPanoramaUIClient *>(this);
	}

	return BaseClass::QueryInterface( pInterfaceName );
}

void CPanoramaUIClient::Shutdown( void )
{
	ShutdownUIEngine();
	BaseClass::Shutdown();
}

panorama::IUIEngine *CPanoramaUIClient::SetupUIEngine( const char *pszLanguage, PlatWindow_t hWindow )
{
	if ( !g_pPanoramaUIEngine )
	{
        Warning( "PanoramaUIEngine interface not set up\n");
		return NULL;
	}

	if ( !g_pPanoramaUIEngine->SetupUIEngine() )
	{
        Warning( "PanoramaUIEngine setup failed\n" );
		return NULL;
	}

    panorama::IUIEngine *pUIEngine = g_pPanoramaUIEngine->AccessUIEngine();
	ConnectPanoramaUIEngine( pUIEngine );

	if ( !panorama::UIEngine() )
	{
        Warning( "Panorama engine not set\n" );
		return NULL;
	}

	if ( !SetupNamedPaths() )
	{
        // Message already shown.
		return NULL;
	}
	
	pUIEngine->RegisterCustomFontPath( "panorama/fonts/" );

	// Startup subsystems and make UIEngine ready for real use
	CPanoramaUISettings::Get().SetUILanguage( pszLanguage );
	pUIEngine->StartupSubsystems( &CPanoramaUISettings::Get(), hWindow );

	// SE port (batch E): publish the UI component JavaScript bindings.  CS:GO does the equivalent from
	// game/client/cstrike15/gameui/gameui_interface.cpp::CGameUI::Initialize, right after the panorama
	// engine is connected and before any layout runs.
	SE_PortInstallUiComponentBindings();

	// SE port (2026-09-16): GameInterfaceAPI.  CS:GO installs CUiComponent_GameInterface from the same
	// CGameUI::Initialize() loop as CUiComponent_UiToolkit (see se_uicomponents.h).  With the real
	// component installed, GameInterfaceAPI.GetSettingString/SetSettingString read and write the
	// archived ConVars (ui_mainmenu_bkgnd_movie and friends) instead of the shim's placeholders.
	SE_PortInstallGameInterfaceBindings();

    return pUIEngine;
}

void CPanoramaUIClient::ShutdownUIEngine()
{
    if ( g_pPanoramaUIEngine )
    {
        g_pPanoramaUIEngine->ShutdownUIEngine();
    }
}

bool CPanoramaUIClient::HandleInputEvent( const InputEvent_t &event, const CUtlVector<panorama::IUIWindow *> &vecWindowInputOrder, bool bOnlyIfFocused )
{
    Assert( g_pPanoramaUIEngine );
	if ( g_pPanoramaUIEngine )
	{
        return g_pPanoramaUIEngine->HandleInputEvent( event, vecWindowInputOrder, bOnlyIfFocused );
    }

    return false;
}

panorama::IUIPanelClient *CPanoramaUIClient::CreatePanel2D( panorama::IUIWindow *pParent, const char *pID )
{
    panorama::CPanel2D *pPanel = new panorama::CPanel2D( pParent, pID );
    return static_cast<panorama::IUIPanelClient*>( pPanel );
}

IPanoramaClientDebugger *CPanoramaUIClient::CreateDebugger( panorama::IUIWindow *pParent, const char *pID )
{
    return new CPanoramaClientDebugger( new panorama::CDebugger( pParent, pID ) );
}

bool CPanoramaUIClient::SetupNamedPaths()
{
	const char *pConfigFilename = "panorama/panorama.cfg";

	KeyValues *pConfigKV = new KeyValues( "" );
	KeyValues::AutoDelete autoDeleteConfig( pConfigKV );


	// Load config KV

	bool bFailed = false;


#if DEVELOPMENT_ONLY
	// In development check for packed and signed panorama zip file,
	// if that file doesn't exist, then load from scattered files on local filesystem
	if ( !g_pFullFileSystem->FileExists( PANORAMA_ZIPFILE_NAME, NULL ) )
	{
		if ( !pConfigKV->LoadFromFile( g_pFullFileSystem, pConfigFilename, "GAME" ) )
		{
			bFailed = true;
		}
	}
	else
#endif
	{
		const char* pConfigFileContent = panorama::UIEngine()->UIFileSystem()->LoadFromPanZip( pConfigFilename );

		if ( pConfigFileContent )
		{
			bFailed = !pConfigKV->LoadFromBuffer( pConfigFilename, pConfigFileContent );
		}
		else
		{
			// SE port: this port runs without the packaged JS layer (panorama/code.pbin is not built),
			// so LoadFromPanZip() finds nothing.  Fall back to the loose file on the GAME search path,
			// exactly like the development path above does.
			Warning( "panorama: '%s' is not in the packaged UI - loading it from the mod instead\n", pConfigFilename );
			if ( !pConfigKV->LoadFromFile( g_pFullFileSystem, pConfigFilename, "GAME" ) )
			{
				bFailed = true;
			}
		}

	}
			
	if ( bFailed )
	{
		// SE port: CS:GO always ships this config (packed or loose) and gives up without it.  The port
		// cannot rely on it - the packed JS layer is not built here and the loose file's load has
		// proven timing dependent - so the named paths the test mod uses are registered directly.
		// file://{resources}/layout/<file>.xml then resolves to <mod>/panorama/layout/<file>.xml.
		Warning( "panorama: '%s' is unavailable - registering the built-in named path(s) instead\n", pConfigFilename );

		if ( panorama::UIEngine() )
		{
			panorama::UIEngine()->RegisterNamedLocalPath( "{resources}", "panorama/", false );
			return true;
		}

		return false;
	}
	
	KeyValues *pNamedPathsKV = pConfigKV->FindKey( "NamedPaths", false );
	if ( !pNamedPathsKV )
    {
        Warning( "Panorama configuration missing NamedPaths\n" );
		if ( g_pFullFileSystem )
		{
#ifndef PANORAMA_USE_S1WRAPPER
			g_pFullFileSystem->MarkContentCorrupt( false, pConfigFilename );
#endif
		}
		return false;
    }

	for ( KeyValues *pKV = pNamedPathsKV->GetFirstSubKey(); pKV; pKV = pKV->GetNextKey() )
	{
		CUtlString nameString = pKV->GetName();
		CUtlString valueString = pKV->GetString( "" );

		if ( !nameString.IsEmpty() )
		{
			nameString.ToLower();
			valueString.ToLower();
            panorama::UIEngine()->RegisterNamedLocalPath( CFmtStr( "{%s}", nameString.Get() ).Get(), valueString.Get(), false );
		}
	}

	return true;
}


//--------------------------------------------------------------------------------------------------
// STubs
//--------------------------------------------------------------------------------------------------

// void ConnectTier3Libraries( CreateInterfaceFn *pFactoryList, int nFactoryCount )
// {
// }
// 
// void DisconnectTier3Libraries()
// {
// }
