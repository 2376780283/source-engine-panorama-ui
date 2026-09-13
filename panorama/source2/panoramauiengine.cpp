//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
//==================================================================================================

#if defined( DX_TO_GL_ABSTRACTION )
#include "togl/rendermechanism.h"
// SE port (temporary bring-up probe): the process exits silently while the UI engine is being set up,
// and Warning() output is lost with it, so these probes append straight to a file.
#endif

#include "stdafx.h"
// SE port (temporary bring-up probe): the process exits silently while the UI engine is being set up,
// and Warning() output is lost with it, so these probes append straight to a file.

#include "resourcesystem/iresourcesystem.h"
// SE port (temporary bring-up probe): the process exits silently while the UI engine is being set up,
// and Warning() output is lost with it, so these probes append straight to a file.

// SE port: g_pIMEManager / g_pPanoramaUIEngine / panorama::g_IUITextServices and
// SOUNDEMITTERSYSTEM_INTERFACE_VERSION are declared in interfaces/interfaces.h under PANORAMA_ENABLE
// (CS:GO reached it through its own include chain).
#include "interfaces/interfaces.h"
// SE port (temporary bring-up probe): the process exits silently while the UI engine is being set up,
// and Warning() output is lost with it, so these probes append straight to a file.

#include "panorama/source2/ipanoramaui.h"
// SE port (temporary bring-up probe): the process exits silently while the UI engine is being set up,
// and Warning() output is lost with it, so these probes append straight to a file.
#include "panoramauiengine.h"
// SE port (temporary bring-up probe): the process exits silently while the UI engine is being set up,
// and Warning() output is lost with it, so these probes append straight to a file.
#include "inputsystem/InputEnums.h"
// SE port (temporary bring-up probe): the process exits silently while the UI engine is being set up,
// and Warning() output is lost with it, so these probes append straight to a file.
#include "igameevents.h"
// SE port (temporary bring-up probe): the process exits silently while the UI engine is being set up,
// and Warning() output is lost with it, so these probes append straight to a file.

#include "panorama/panorama.h"
// SE port (temporary bring-up probe): the process exits silently while the UI engine is being set up,
// and Warning() output is lost with it, so these probes append straight to a file.
#include "panorama/iuiengine.h"
// SE port (temporary bring-up probe): the process exits silently while the UI engine is being set up,
// and Warning() output is lost with it, so these probes append straight to a file.
#include "panorama/uijsregistration.h"
// SE port (temporary bring-up probe): the process exits silently while the UI engine is being set up,
// and Warning() output is lost with it, so these probes append straight to a file.
#if defined( SOURCE2_PANORAMA )
#include "uitoplevelwindowsource2.h"
// SE port (temporary bring-up probe): the process exits silently while the UI engine is being set up,
// and Warning() output is lost with it, so these probes append straight to a file.
#endif
#if defined( PANORAMA_SE_CPU_TEXT )
#include "seport/se_dwrite_cpu_text.h"
// SE port (temporary bring-up probe): the process exits silently while the UI engine is being set up,
// and Warning() output is lost with it, so these probes append straight to a file.
#endif
#if defined( PANORAMA_USE_S1WRAPPER )
#include "engine/IEngineSound.h"
// SE port (temporary bring-up probe): the process exits silently while the UI engine is being set up,
// and Warning() output is lost with it, so these probes append straight to a file.
// There is a complete clustermess about how S1 engine can be accessed from panorama.dll
// adding a few #defines to prevent including headers that cause compile errors, this is a total hack
#define RESOURCESTREAM_H
#define RESOURCEFILE_H
#define RESOURCETYPE_H
#include "cdll_int.h"
// SE port (temporary bring-up probe): the process exits silently while the UI engine is being set up,
// and Warning() output is lost with it, so these probes append straight to a file.
#endif

#include "iimemanager.h"
// SE port (temporary bring-up probe): the process exits silently while the UI engine is being set up,
// and Warning() output is lost with it, so these probes append straight to a file.
#include "resourcesystem/iresourcesystem.h"
// SE port (temporary bring-up probe): the process exits silently while the UI engine is being set up,
// and Warning() output is lost with it, so these probes append straight to a file.

// NOTE: This must be the last file included!!!
#include "tier0/memdbgon.h"
// SE port (temporary bring-up probe): the process exits silently while the UI engine is being set up,
// and Warning() output is lost with it, so these probes append straight to a file.

#if defined( DX_TO_GL_ABSTRACTION )
// Placed here so inlines placed in dxabstract.h can access gGL
COpenGLEntryPoints *gGL = NULL;
#endif



CPanoramaUIEngine s_PanoramaUIEngine;
CPanoramaUIEngine *g_pPanoramaUIEngineImpl = &s_PanoramaUIEngine;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CPanoramaUIEngine, IPanoramaUIEngine, PANORAMAUI_ENGINE_INTERFACE_VERSION, s_PanoramaUIEngine )

#ifndef PANORAMA_USE_S1WRAPPER

DEFINE_LOGGING_CHANNEL_NO_TAGS( LOG_PANORAMA, "Panorama", 0, LV_DEFAULT, Color( 75, 245, 200, 255 ) );
DEFINE_LOGGING_CHANNEL_NO_TAGS( LOG_PANORAMA_SCRIPT, "PanoramaScript", 0, LV_DEFAULT, Color( 200, 255, 255, 255 ) );

#endif

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
static AppSystemInfo_t s_pDependencies[] =
{
	{ "panorama_text_pango", PANORAMA_TEXT_SERVICES_INTERFACE_VERSION },
#if ( defined( PLATFORM_WINDOWS_PC ) )
	{ "imemanager", IMEMANAGER_INTERFACE_VERSION },
#endif
	{ NULL, NULL }
};

//-------------------------------------------------------------------------
// Constructor
//-------------------------------------------------------------------------
CPanoramaUIEngine::CPanoramaUIEngine()
{
	m_pUIEngine = NULL;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
const AppSystemInfo_t *CPanoramaUIEngine::GetDependencies()
{
	return s_pDependencies;
}

//-------------------------------------------------------------------------
// Connect interfaces
//-------------------------------------------------------------------------
bool CPanoramaUIEngine::Connect( CreateInterfaceFn factory )
{
	if ( !factory )
	{
		return false;
	}

	if ( !BaseClass::Connect( factory ) )
	{
		return false;
	}

#if defined ( DX_TO_GL_ABSTRACTION )
#if defined( ALLOW_TEXT_MODE )
	static const bool cbTextMode = CommandLine()->HasParm( "-textmode" );
#else
	static const bool cbTextMode = false;
#endif

	if ( !cbTextMode && !gGL )
	{
		gGL = ToGLConnectLibraries( factory );
		gGL = GetOpenGLEntryPoints(0);
	}
#endif


#if ( defined( PLATFORM_WINDOWS_PC ) )
	g_pIMEManager = ( IIMEManager* )factory( IMEMANAGER_INTERFACE_VERSION, NULL );
	Assert( g_pIMEManager );
#endif

#if defined( PANORAMA_USE_S1WRAPPER )
	g_pEnginesound = ( IEngineSound* )factory( IENGINESOUND_CLIENT_INTERFACE_VERSION, NULL );
	g_pSoundEmitterSystemBase = (ISoundEmitterSystemBase *)factory( SOUNDEMITTERSYSTEM_INTERFACE_VERSION, NULL );
	g_pEngineclient = ( IVEngineClient* ) factory( VENGINE_CLIENT_INTERFACE_VERSION, NULL );
#endif
	m_pShaderDeviceMgr = (IShaderDeviceMgr*)factory( SHADER_DEVICE_MGR_INTERFACE_VERSION, NULL );


	g_IUITextServices = ( panorama::IUITextServices * ) factory( PANORAMA_TEXT_SERVICES_INTERFACE_VERSION, NULL );

#if defined( PANORAMA_SE_CPU_TEXT )
	// SE port: CS:GO takes the text backend from the separate panorama_text_pango module (the
	// first entry of s_pDependencies above), which its app system group loads through the
	// dependency list.  Source Engine 2013's app system group has no such list and no module in
	// this tree exposes PANORAMA_TEXT_SERVICES_INTERFACE_VERSION, so the framework's own
	// DirectWrite/CPU backend is instantiated here.  The factory lookup above stays first, so a
	// real provider still wins.
	if ( !g_IUITextServices )
	{
		Warning( "panorama: no PanoramaTextServices001 provider - using the built-in DirectWrite text services\n" );
		g_IUITextServices = new panorama::CSEPanoramaTextServicesWin32();
	}
#endif

	// Initialize the console variables.
	ConVar_Register();

#if defined( PANORAMA_USE_S1WRAPPER )
	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f );
#endif




	return true;
}


//-------------------------------------------------------------------------
// Disconnect interfaces
//-------------------------------------------------------------------------
void CPanoramaUIEngine::Disconnect()
{
	ShutdownUIEngine();
	BaseClass::Disconnect();
}


//-------------------------------------------------------------------------
// Query interfaces
//-------------------------------------------------------------------------
void *CPanoramaUIEngine::QueryInterface( const char *pInterfaceName )
{
	if ( !V_strncmp( pInterfaceName, PANORAMAUI_ENGINE_INTERFACE_VERSION, V_strlen( PANORAMAUI_ENGINE_INTERFACE_VERSION ) + 1 ) )
	{
		return static_cast<IPanoramaUIEngine *>(this);
	}

	return BaseClass::QueryInterface( pInterfaceName );
}


#ifdef PANORAMA_USE_S1WRAPPER
bool launcher_keypair_verifymsg( const byte *pubData, int cbData, const byte *pubPublicKey, int cbPublicKey, const byte *pubSignature, int cbSignature );
extern bool( *g_pfnPanoramaResourceFileIntegrityCheck )( CUtlBuffer &bufFileData, void *&pvResourceData, int &numResourceBytes );
static bool PanoramaResourceFileIntegrityCheck( CUtlBuffer &bufFileData, void *&pvResourceData, int &numResourceBytes )
{
	// File layout expected:
	// 4-bytes of the header 'P' 'A' 'N' 1
	// 256 bytes of digest signature
	// [zipblob]
	// 1-byte of the version (same as in the header, but this byte is included in the digest)
	const int numSignatureDigestBytes = 512;
	if ( bufFileData.TellPut() < 64 + numSignatureDigestBytes )
		return false;

	// SE port: accept any container version the port knows the layout of (1 = the CS:GO 2019 sources,
	// 2 = the retail 2023 code.pbin); see the note on PANORAMA_ZIPFILE_VERSION_MAX.
	const char chMagic[3] = { 'P', 'A', 'N' };
	if ( memcmp( bufFileData.Base(), chMagic, 3 ) )
		return false;

	unsigned char chPackVersion = ( unsigned char )( ( char * ) bufFileData.Base() )[3];
	if ( chPackVersion < 1 || chPackVersion > PANORAMA_ZIPFILE_VERSION_MAX )
		return false;

	if ( ( unsigned char )( ( char * ) bufFileData.Base() )[ bufFileData.TellPut() - 1 ] != chPackVersion )
		return false;

	pvResourceData = ( ( char * ) bufFileData.Base() ) + 4 + numSignatureDigestBytes;
	numResourceBytes = bufFileData.TellPut() - 4 - numSignatureDigestBytes - 1;

	// SE port: CS:GO verifies the signed panorama resource pack against the public key in
	// devtools/bin/certificates/panoramapack.public.h, which does not exist in this tree (and this
	// tree has no package signing infrastructure).  The retail code.pbin IS a signed pack, so
	// rejecting signed packs made it impossible to load the actual CS:GO UI at all.
	//
	// What is checked here instead is only the container structure (the PAN/version header and the
	// trailing version byte above, which bounds pvResourceData/numResourceBytes); the RSA digest is
	// *not* verified.  The pack is read from the user's own game installation, so this is the same
	// trust level the rest of this port's file loading has.
#if !defined( PANORAMA_HAVE_SIGNED_PACKS )
	{
		static bool s_bWarnedPanoramaPackSignature = false;
		if ( !s_bWarnedPanoramaPackSignature )
		{
			s_bWarnedPanoramaPackSignature = true;
			Warning( "panorama: accepting signed resource pack without verifying its signature "
					 "(no pack verification infrastructure in this build); %d resource bytes\n", numResourceBytes );
		}
	}
	return numResourceBytes > 0;
#else
	const byte CertificateData[] = {
#include PANORAMA_PACK_PUBLIC_KEY_HEADER
	};
	return launcher_keypair_verifymsg( ( const byte * ) pvResourceData, numResourceBytes + 1, CertificateData, sizeof( CertificateData ), ( const byte * )( ( ( char * ) bufFileData.Base() ) + 4 ), numSignatureDigestBytes );
#endif
}

static void PanoramaReleaseMaterialSystemObjects( int nChangeFlags )
{
	if ( g_pPanoramaUIEngineImpl && g_pPanoramaUIEngineImpl->m_pUIEngine )
		( ( panorama::CUIEngineSource2* )( g_pPanoramaUIEngineImpl->m_pUIEngine ) )->OnDeviceLost();
}

static void PanoramaRestoreMaterialSystemObjects( int nChangeFlags )
{
	if ( g_pPanoramaUIEngineImpl && g_pPanoramaUIEngineImpl->m_pUIEngine )
		( ( panorama::CUIEngineSource2* )( g_pPanoramaUIEngineImpl->m_pUIEngine ) )->OnDeviceRestored();
}

// SE port: Source Engine 2013's IMaterialSystem::AddReleaseFunc takes a void(*)() callback while
// AddRestoreFunc keeps CS:GO's int nChangeFlags argument (see MaterialBuffer{}Release,Restore}Func_t
// in public/materialsystem/imaterialsystem.h).  The release flag is not used by the panorama
// renderer, so a thin wrapper bridges that one signature.
static void PanoramaReleaseMaterialSystemObjectsSE() { PanoramaReleaseMaterialSystemObjects( 0 ); }
#endif


//-------------------------------------------------------------------------
// Init
//-------------------------------------------------------------------------
InitReturnVal_t CPanoramaUIEngine::Init()
{
	InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
	{
		return nRetVal;
	}

#ifdef PANORAMA_USE_S1WRAPPER
	// Make sure we configure the loader integrity checking before we initialize the resource system S1 wrapper
	g_pfnPanoramaResourceFileIntegrityCheck = PanoramaResourceFileIntegrityCheck;
	g_pResourceSystem->Init();

	if ( m_pShaderDeviceMgr )
	{
		m_pDeviceCallbacks = new DeviceCallbacks();
		m_pDeviceCallbacks->m_pPanoramaUIEngine = this;
		m_pShaderDeviceMgr->AddDeviceDependentObject( m_pDeviceCallbacks );
	}

	if ( g_pMaterialSystem )
	{
		g_pMaterialSystem->AddReleaseFunc( PanoramaReleaseMaterialSystemObjectsSE );
		g_pMaterialSystem->AddRestoreFunc( PanoramaRestoreMaterialSystemObjects );
	}
	
#endif

	ConVar_Register();
	return nRetVal;
}


//-------------------------------------------------------------------------
// Create UI Engine object, remember to use ConnectPanoramaUIEngine() from panorama_client in your UI module
//-------------------------------------------------------------------------
bool CPanoramaUIEngine::SetupUIEngine()
{
	m_pUIEngine = panorama::CreatePanoramaUIEngineInternal();
	if ( !m_pUIEngine )
		return false;

	return true;
}


//-------------------------------------------------------------------------
// Shutdown previously created UI Engine
//-------------------------------------------------------------------------
void CPanoramaUIEngine::ShutdownUIEngine()
{
	if ( m_pUIEngine )
	{
		m_pUIEngine->Shutdown();
		delete m_pUIEngine;
		m_pUIEngine = NULL;
	}
}


//-------------------------------------------------------------------------
// Access UI engine, generally use global panorama::UIEngine() accessor instead as shorthand
//-------------------------------------------------------------------------
panorama::IUIEngine * CPanoramaUIEngine::AccessUIEngine()
{
	return m_pUIEngine;
}


//-------------------------------------------------------------------------
// Pass UI events to the UI engine
//-------------------------------------------------------------------------
bool CPanoramaUIEngine::HandleInputEvent( const InputEvent_t &event, const CUtlVector<panorama::IUIWindow *> &vecWindowInputOrder, bool bOnlyIfFocused )
{
	FOR_EACH_VEC( vecWindowInputOrder, i )
	{
		if ( ( (panorama::CTopLevelWindowSource2 *)vecWindowInputOrder[ i ] )->BIsVisible() &&  vecWindowInputOrder[ i ]->GetNumVisibleTopLevelPanels() > 0 &&
			( event.m_hWnd == PLAT_WINDOW_INVALID || event.m_hWnd == ( (panorama::CTopLevelWindowSource2 *)vecWindowInputOrder[ i ] )->GetPlatWindow() ) )
		{
			if ( !bOnlyIfFocused || vecWindowInputOrder[ i ]->BHasFocus() )
			{
				if ( ( (panorama::CTopLevelWindowSource2 *)vecWindowInputOrder[ i ] )->HandleInputEvent( event ) )
				{
					return true;
				}
				else if ( vecWindowInputOrder[ i ]->BForceConsumeKBAndMouseInputEvents() )
				{
					switch ( event.m_nType )
					{
					case IE_ButtonPressed:
					case IE_ButtonReleased:
					case IE_ButtonDoubleClicked:
					case IE_AnalogValueChanged:
					case IE_ButtonPressedRepeating:
					case IE_KeyTyped:
					case IE_KeyCodeTyped:
					case IE_KeyCodeReleased:
					case IE_LocateMouseClick:
						return true;
					}
				}
			}
		}
	}

	return false;
}


//-------------------------------------------------------------------------
// Expose UIInputEngine IME control.
//-------------------------------------------------------------------------
void CPanoramaUIEngine::SetIMEAllowed( bool bAllowed )
{
	if ( m_pUIEngine )
	{
		m_pUIEngine->UIInputEngine()->SetIMEAllowed( bAllowed );
	}
}


//-------------------------------------------------------------------------
// Clean up windows marked for close.
//-------------------------------------------------------------------------
int CPanoramaUIEngine::DeleteClosedWindows()
{
	if ( !m_pUIEngine )
	{
		return 0;
	}
	
#ifdef PANORAMA_USE_S1WRAPPER
	CUtlVector< panorama::IUIWindow* > vecWindows;
#else
	CUtlVectorFixedGrowableCompat< panorama::IUIWindow*, 8 > vecWindows;
#endif
	m_pUIEngine->GetWindowsForDebugger( vecWindows );
	
	int nClosed = 0;
	for ( int i = 0; i < vecWindows.Count(); i++ )
	{
		panorama::CTopLevelWindowSource2 *pWindow = (panorama::CTopLevelWindowSource2*)vecWindows[i];
		if ( pWindow->IsClosed() )
		{
			delete pWindow;
			nClosed++;
		}
	}
	return nClosed;
}


//-------------------------------------------------------------------------
// Shutdown interfaces
//-------------------------------------------------------------------------
void CPanoramaUIEngine::Shutdown( void )
{
	ShutdownUIEngine();
	ConVar_Unregister();
#ifdef PANORAMA_USE_S1WRAPPER
	g_pResourceSystem->Shutdown();
#endif

	BaseClass::Shutdown();
}


#ifdef PANORAMA_USE_S1WRAPPER
//////////////////////////////////////////////////////////////////////////
//
// Cryptography support code
//
//////////////////////////////////////////////////////////////////////////

#include "tier0/memdbgoff.h"
// SE port (temporary bring-up probe): the process exits silently while the UI engine is being set up,
// and Warning() output is lost with it, so these probes append straight to a file.

#ifdef Verify
#undef Verify
#endif

#define bswap_16 __bswap_16
#define bswap_64 __bswap_64

#if defined( PANORAMA_HAVE_SIGNED_PACKS )
// SE port: Crypto++ (cryptlib.h/rsa.h) is not vendored in this tree and this build does not sign
// panorama packs, so the verifier below is compiled as a rejecting stub.
#include "cryptlib.h"
// SE port (temporary bring-up probe): the process exits silently while the UI engine is being set up,
// and Warning() output is lost with it, so these probes append straight to a file.
#include "rsa.h"
// SE port (temporary bring-up probe): the process exits silently while the UI engine is being set up,
// and Warning() output is lost with it, so these probes append straight to a file.
#endif

// Special usage here in the launcher without linking in tier0 tslist implementation
// list of auto-seeded RNG pointers
// these are very expensive to construct, so it makes sense to cache them

bool launcher_keypair_verifymsg( const byte *pubData, int cbData, const byte *pubPublicKey, int cbPublicKey, const byte *pubSignature, int cbSignature )
{
#if !defined( PANORAMA_HAVE_SIGNED_PACKS )
	NOTE_UNUSED( pubData );
	NOTE_UNUSED( cbData );
	NOTE_UNUSED( pubPublicKey );
	NOTE_UNUSED( cbPublicKey );
	NOTE_UNUSED( pubSignature );
	NOTE_UNUSED( cbSignature );
	return false;
#else
	try           // handle any exceptions crypto++ may throw
	{
		CryptoPP::StringSource stringSourcePublicKey( pubPublicKey, cbPublicKey, true );
		CryptoPP::RSASSA_PKCS1v15_SHA_Verifier pub( stringSourcePublicKey );

		return pub.VerifyMessage( pubData, cbData, pubSignature, cbSignature );
	}
	catch ( ... )
	{
	}
	return false;
#endif // PANORAMA_HAVE_SIGNED_PACKS
}
#endif
