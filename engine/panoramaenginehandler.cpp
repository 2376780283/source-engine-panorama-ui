//============ Copyright (c) Valve Corporation, All rights reserved. ==========
//
//=============================================================================

// SE port: the panorama public headers are written against the framework's private stdafx.h, which
// pulls in utldelegate / KeyValues / the s1wrapper render interfaces in exactly the order they expect
// (including it here is what the panorama modules themselves do).  The engine has no panorama
// precompiled header, so it is included explicitly - the wrapper macros it defines (e.g. IsWindows())
// only affect this translation unit.
//
// NOTE: the path is relative to this file on purpose.  The engine's include paths deliberately point
// "panorama/..." at ../public/panorama (the Source 1 public interfaces this module talks to), so a
// bare "panorama/stdafx.h" would not resolve - and the framework's own stdafx.h is what provides the
// header set/order the panorama headers expect.
//
// The framework headers are written against CS:GO's CUtlMap/CUtlRBTree (CUtlMap takes an index type as
// its third template argument), so the wrapper's versions have to be pulled in ahead of Source Engine's
// own - which is exactly what panorama/stdafx.h does for the panorama modules, except that the engine
// gets Source Engine's tier1 by default.  PANORAMA_SE_CSGO_CONTAINERS switches the wrapper headers over
// for this translation unit only; every other engine TU keeps public/tier1 (they never see the framework).
#define PANORAMA_SE_CSGO_CONTAINERS 1
#include "../panorama_s1wrapper/tier1/utlrbtree.h"
#include "../panorama_s1wrapper/tier1/utlmap.h"

#include "../panorama/stdafx.h"
#include "tier0/icommandline.h"		// CommandLine()

#if defined( _WIN32 ) && !defined( _X360 )
#include <windows.h>
#endif

#include <stdarg.h>
#include <stdio.h>

#include "panoramaenginehandler.h"
#include "interfaces/interfaces.h"
#include "filesystem.h"
#include "fmtstr.h"
#include "bitmap/bitmap.h"
#include "inputsystem/iinputsystem.h"
#include "video/ivideoplayer.h"
#include "panorama/uievents.h"
#include "tier1/utldelegate.h"
#include "cl_steamauth.h"
#include "igame.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "materialsystem/itexture.h"
#include "materialsystem/imesh.h"
#include "utlsortvector.h"
#include "pixelwriter.h"
#include "tier1/KeyValues.h"
#include "iimemanager.h"
#include "cdll_int.h"

#include "inputsystem/InputEnums.h"
#include "inputsystem/iinputstacksystem.h"
#include "tier2/renderutils.h"
#include "videocfg/videocfg.h"
#include "tier0/vprof.h"
#include "cmd.h"		// Cbuf_AddText (SE port bring-up hook below)
#include "ienginevgui.h"	// VGuiPanel_t / EngineVGui()->GetPanel
#include "vgui_baseui_interface.h"	// EngineVGui()
#include "vgui/ipanel.h"	// vgui::ipanel()->SetVisible
#include <vgui_controls/Controls.h>	// vgui::ipanel() lives here
#include <vgui/ISurface.h>	// vgui::surface()->IsCursorLocked/IsCursorVisible (probe)
#include "seport/se_background_movie.h"	// SE_PortLoadMainMenuBackgroundMovie (background webm)

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


// DenyAllInputToGame detailed logging
#if 0
#define InputDevMsg DevMsg
#else
#define InputDevMsg( ... ) (void)(0)
#endif

extern IBaseClientDLL *g_ClientDLL;

const char *Key_BindingForKey( ButtonCode_t code );

using namespace panorama;

ConVar s_convarPanoramaECOMode( "@panorama_ECO_mode", "1", FCVAR_NONE, "0 - disable, 1 - default, 2 - force always ON" );

// SE port: the layout the hosted menu view loads.  "panorama_menu <layout>" retargets this at run time.
// It is base_mainmenu.xml (not mainmenu.xml!) because CS:GO's structure is
//     view -> base_mainmenu.xml -> <CSGOMainMenu> -> (CCSGO_MainMenu loads) mainmenu.xml
// and CCSGO_MainMenu::CCSGO_MainMenu() does RequireLoadLayout("mainmenu.xml") itself.  Handing
// mainmenu.xml to the *view* would make the class load the very layout it is being created from; the
// nested panel then gets discarded and the menu never appears at all (the class exists, so there is no
// "panel type not implemented" message either - the screen is simply empty).  Passing mainmenu.xml is
// still accepted and translated - see SE_PortNormalizeMenuLayout() in CreatePanoramaMenuView().
// NOTE: outside of the DEVELOPMENT_ONLY block below - that block is compiled out of release builds, which
// is exactly the build this port runs as.
ConVar panorama_menu_layout( "panorama_menu_layout", "file://{resources}/layout/base_mainmenu.xml", FCVAR_NONE,
	"Panorama layout loaded by the panorama_menu view" );

#if ( PLATFORM_WINDOWS && DEVELOPMENT_ONLY )

ConVar panorama_debugger_saved_width( "panorama_debugger_saved_width", "1280", FCVAR_ARCHIVE );
ConVar panorama_debugger_saved_height( "panorama_debugger_saved_height", "720", FCVAR_ARCHIVE );
ConVar panorama_debugger_saved_xpos( "panorama_debugger_saved_xpos", "0", FCVAR_ARCHIVE );
ConVar panorama_debugger_saved_ypos( "panorama_debugger_saved_ypos", "0", FCVAR_ARCHIVE );

#endif

extern ConVar cl_language;

static void CC_DumpDenyAllInputToGame( void )
{
	PanoramaEngineHandler().DumpDenyAllInputToGame();
}
static ConCommand panorama_dump_deny_input( "panorama_dump_deny_input", CC_DumpDenyAllInputToGame, "Dumps panels currently denying all input to the game", FCVAR_DEVELOPMENTONLY );

// SE port: create/destroy the hosted-UI smoke test view at runtime (same thing -panoramatest does at
// startup).  With no argument it toggles; "panorama_test 1" creates it and "panorama_test 0" removes it.
// NOTE: no FCVAR_DEVELOPMENTONLY - Source hides development-only commands in release builds, which is
// exactly the build this port runs as.
static void CC_PanoramaTest( const CCommand &args )
{
	bool bCreate = !PanoramaEngineHandler().HasPanoramaTestView();
	if ( args.ArgC() > 1 )
	{
		bCreate = ( V_atoi( args.Arg( 1 ) ) != 0 );
	}

	if ( bCreate )
	{
		PanoramaEngineHandler().CreatePanoramaTestView();
	}
	else
	{
		PanoramaEngineHandler().DestroyPanoramaTestView();
	}
}
static ConCommand panorama_test( "panorama_test", CC_PanoramaTest, "Creates (1) or destroys (0) the hosted panorama test view; no argument toggles" );

// SE port: create/destroy the CS:GO main-menu view (needs panorama/code.pbin in the mod - that pack
// contains layout/mainmenu.xml).  Same toggling as panorama_test.
static void CC_PanoramaMenu( const CCommand &args )
{
	bool bCreate = !PanoramaEngineHandler().HasPanoramaMenuView();
	bool bRetarget = false;
	if ( args.ArgC() > 1 )
	{
		const char *pchArg = args.Arg( 1 );
		if ( V_strcmp( pchArg, "0" ) == 0 || V_stricmp( pchArg, "false" ) == 0 )
		{
			bCreate = false;
		}
		else if ( V_strcmp( pchArg, "1" ) == 0 || V_stricmp( pchArg, "true" ) == 0 )
		{
			bCreate = true;
		}
		else
		{
			// SE port: "panorama_menu layout/console.xml" retargets the view at another layout without a
			// rebuild, which is what makes it possible to try CS:GO layouts one by one.  The panoramic
			// "file://{resources}/layout/" prefix is optional.
			CUtlString strLayout;
			if ( V_strnicmp( pchArg, "file:", 5 ) == 0 )
			{
				strLayout = pchArg;
			}
			else if ( V_strnicmp( pchArg, "layout/", 7 ) == 0 )
			{
				strLayout.Format( "file://{resources}/%s", pchArg );
			}
			else
			{
				strLayout.Format( "file://{resources}/layout/%s", pchArg );
			}

			if ( V_strcmp( strLayout.Get(), panorama_menu_layout.GetString() ) != 0 )
			{
				bRetarget = true;
			}

			panorama_menu_layout.SetValue( strLayout.Get() );
			bCreate = true;
		}
	}

	if ( bCreate && bRetarget && PanoramaEngineHandler().HasPanoramaMenuView() )
	{
		PanoramaEngineHandler().DestroyPanoramaMenuView();
	}

	if ( bCreate )
	{
		PanoramaEngineHandler().CreatePanoramaMenuView();
	}
	else
	{
		PanoramaEngineHandler().DestroyPanoramaMenuView();
	}
}
static ConCommand panorama_menu( "panorama_menu", CC_PanoramaMenu, "Creates (1) / destroys (0) the hosted CS:GO menu view, or loads a different layout (pano layout path); no argument toggles" );

static void CC_PanoramaStatus( const CCommand &args )
{
	PanoramaEngineHandler().PrintPanoramaStatus();
}
static ConCommand panorama_status( "panorama_status", CC_PanoramaStatus, "Dumps the state of the hosted panorama UI" );


CPanoramaEngineHandler &PanoramaEngineHandler()
{
	static CPanoramaEngineHandler s_PanoramaEngineHandler;
	return s_PanoramaEngineHandler;
}

//-----------------------------------------------------------------------------
// SE port (task A, console/escape routing): is the hosted panorama UI layer (the CS:GO main menu) up?
//
// CS:GO's engine never needs to ask this: its GameUI *is* the panorama UI, so "gameui_activate" shows the
// panorama menu and the VGUI console is not a child of it.  This fork still ships the CS:S VGUI2 main menu
// as the game UI, so the three places that used to reach for it unconditionally - CEngineVGui::ShowConsole()
// ("ActivateGameUI()" made '~' pop the CS:S menu open), CEngineVGui::ActivateGameUI() (what Esc calls) and
// CEngineVGui::IsConsoleVisible() - ask this first.  See engine/keys.cpp::IsESC() for the matching change to
// the key filter order.
//-----------------------------------------------------------------------------
bool SE_PortIsPanoramaMenuActive()
{
	return PanoramaEngineHandler().HasPanoramaMenuView();
}

//-----------------------------------------------------------------------------
// SE port (task A follow-up): hand the escape key to the hosted panorama UI.
//
// The panorama-side implementation lives in panoramauiclient.dll (panoramauiclient/se_escape.cpp): it
// closes a visible popup, or dispatches the panel "Cancelled" event the content listens for.  CS:GO keeps
// this in its GameUI, which is the panorama client UI there; this fork has no such GameUI yet ("task B"),
// so the engine forwards the key instead.  Same GetProcAddress bridge as SE_PortMainMenuTick below.
//-----------------------------------------------------------------------------
bool SE_PortHandlePanoramaEscape()
{
	typedef bool ( *SEPortEscapeFn )();
	static SEPortEscapeFn s_pfnSEEscape = NULL;
	static bool s_bSEEscapeResolved = false;
	if ( !s_bSEEscapeResolved )
	{
		s_bSEEscapeResolved = true;
		HMODULE hPanoramaModule = GetModuleHandleA( "panoramauiclient.dll" );
		if ( hPanoramaModule )
			s_pfnSEEscape = (SEPortEscapeFn)GetProcAddress( hPanoramaModule, "SE_PortPanoramaEscapePressed" );
	}

	return ( s_pfnSEEscape != NULL ) && s_pfnSEEscape();
}

//-----------------------------------------------------------------------------
// SE port (settings keyboard binder): CS:GO's input route gives the key binder the raw event before
// anything else (keys.cpp::PanoramaHandleInputEvent -> g_ClientDLL->HandleBindWidgetInputCapture).
// The implementation is in panoramauiclient.dll (panoramauiclient/se_keybinder.cpp); same
// GetProcAddress bridge as SE_PortHandlePanoramaEscape above.
//-----------------------------------------------------------------------------
bool SE_PortHandleKeyBinderInput( const InputEvent_t &inputEvent )
{
	typedef bool ( *SEPortKeyBinderInputFn )( const InputEvent_t & );
	static SEPortKeyBinderInputFn s_pfnSEKeyBinderInput = NULL;
	static bool s_bSEKeyBinderResolved = false;
	if ( !s_bSEKeyBinderResolved )
	{
		s_bSEKeyBinderResolved = true;
		HMODULE hPanoramaModule = GetModuleHandleA( "panoramauiclient.dll" );
		if ( hPanoramaModule )
			s_pfnSEKeyBinderInput = (SEPortKeyBinderInputFn)GetProcAddress( hPanoramaModule, "SE_PortKeyBinderHandleInputEvent" );
	}

	return ( s_pfnSEKeyBinderInput != NULL ) && s_pfnSEKeyBinderInput( inputEvent );
}

//-----------------------------------------------------------------------------
// LessFunc for rendering view order
//-----------------------------------------------------------------------------
bool CPanoramaEngineHandler::ViewPriorityOrder( CPanoramaEngineHandler::ViewEntry_t const &lhs, CPanoramaEngineHandler::ViewEntry_t const &rhs, void *pCtx )
{
	int nLeftPriority = lhs.m_pWindow ? lhs.m_pWindow->GetWindowPriority() : 0;
	int nRightPriority = rhs.m_pWindow ? rhs.m_pWindow->GetWindowPriority() : 0;

	// If the priorities are equal, then we tie-break on the pointer location for the window
	if ( nLeftPriority == nRightPriority )
		return ( lhs.m_pWindow >= rhs.m_pWindow );

	return ( nLeftPriority < nRightPriority );
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CPanoramaEngineHandler::CPanoramaEngineHandler()
{
	m_bValid = false;
	m_pUIEngine = NULL;
	m_pTestWindow = NULL;
	m_pMenuWindow = NULL;
	m_nMainWindowWidth = 0;
	m_nMainWindowHeight = 0;

#if ( PLATFORM_WINDOWS && DEVELOPMENT_ONLY )

	m_pDebugWindow = NULL;
	m_hDebuggerWindow = 0;
	m_bShowDebugger = false;
	m_nDebuggerX = m_nDebuggerY = 0;

#endif

	m_hPanoramaInputContext = INPUT_CONTEXT_HANDLE_INVALID;
	m_nNextInputHandle = 1;
	m_eGameInputFlags = panorama::k_EGameInputFlagsNone;
}


//-----------------------------------------------------------------------------
// Add a view to the system, that is a standalone top level panorama window
//-----------------------------------------------------------------------------
// SE port (bring-up aid): flushed probe file, defined below; declared here because AddPanoramaView
// (which is above the definition) reports what it does to a view.
void SE_PortUIProbe( const char *pFmt, ... );

panorama::IUIPanelClient *CPanoramaEngineHandler::AddPanoramaView( const char *pchViewName, panorama::IUIWindow *pWindow )
{
	ViewEntry_t view;
	view.m_sViewName = pchViewName;
	view.m_pWindow = pWindow;
	//view.m_Layer.Init();
	//view.m_Layer.AddPanoramaWindow( view.m_pWindow );

	if ( m_nMainWindowWidth != 0 && m_nMainWindowHeight != 0 )
	{
		view.m_pWindow->OnWindowResize( m_nMainWindowWidth, m_nMainWindowHeight );
		view.m_pWindow->SetWindowScaleFactor( m_nMainWindowHeight / 1080.0f );
		SE_PortUIProbe( "ADDVIEW %s : main=%dx%d -> scale=%.4f\n", pchViewName, m_nMainWindowWidth, m_nMainWindowHeight, m_nMainWindowHeight / 1080.0f );
	}
	else
	{
		view.m_pWindow->SetWindowScaleFactor( (float)pWindow->GetSurfaceHeight() / 1080.0f );
		SE_PortUIProbe( "ADDVIEW %s : main=0x0 -> scale from surface=%u = %.4f\n", pchViewName, pWindow->GetSurfaceHeight(), (float)pWindow->GetSurfaceHeight() / 1080.0f );
	}

	m_Views.SortedInsert( view, &ViewPriorityOrder, NULL ); // keep the views sorted in ascending priority

	RecalculateInputOrder();

	m_pWindows.AddToTail( pWindow );

	return g_pPanoramaUIClient->CreatePanel2D( view.m_pWindow, pchViewName );
}

void CPanoramaEngineHandler::RemovePanoramaView( panorama::IUIWindow *pWindow )
{
	m_pWindows.FindAndRemove( pWindow );

	for ( int i = 0; i < m_Views.Count(); i++ )
	{
		if ( m_Views[ i ].m_pWindow == pWindow )
		{
			m_Views.Remove( i );
			break;
		}
	}

	RecalculateInputOrder();

	return;
}


//-----------------------------------------------------------------------------
// SE port: create/destroy the hosted-UI smoke test view.  Reached through -panoramatest at startup and
// through the "panorama_test" console command at runtime.
//-----------------------------------------------------------------------------
bool CPanoramaEngineHandler::CreatePanoramaTestView()
{
	if ( m_pTestWindow )
	{
		Warning( "panorama: the test view already exists (\"panorama_test 0\" removes it)\n" );
		return true;
	}

	// NOTE: m_bValid is only set at the end of Init(), and this runs in the middle of it - so only
	// m_pUIEngine can be relied on here.
	if ( !m_pUIEngine )
	{
		Warning( "panorama: the hosted UI isn't initialized, can't create the test view\n" );
		return false;
	}

	int nTestWidth = 0;
	int nTestHeight = 0;
	materials->GetBackBufferDimensions( nTestWidth, nTestHeight );
	if ( !nTestWidth || !nTestHeight )
	{
		Warning( "panorama: no back buffer size yet, the test view was not created\n" );
		return false;
	}

	panorama::IUIWindow *pTestWindow = m_pUIEngine->CreateNewUILayerWindow( 0, 0, nTestWidth, nTestHeight, false, false, false, true, "PanoramaTest", INPUT_CONTEXT_HANDLE_INVALID );
	panorama::IUIPanelClient *pTestPanel = pTestWindow ? AddPanoramaView( "PanoramaTest", pTestWindow ) : NULL;
	if ( !pTestPanel )
	{
		Warning( "panorama: could not create the test view\n" );
		return false;
	}

	m_pTestWindow = pTestWindow;

	const char *pTestLayout = "file://{resources}/layout/test.xml";
	if ( !pTestPanel->UIPanel()->BLoadLayout( pTestLayout ) )
	{
		Warning( "panorama: could not load %s (the mod layout, see mods/panorama_test)\n", pTestLayout );
	}
	else
	{
		Warning( "panorama: loaded %s\n", pTestLayout );
	}

	// SE port (bring-up aid): dump the panel tree so we can see whether the layout actually built
	// panel children (an empty tree means the paint pass has nothing to draw).
	{
		struct SEDumpPanelTree
		{
			static void Dump( panorama::IUIPanel *pPanel, int nDepth )
			{
				if ( !pPanel || nDepth > 3 )
					return;

				Warning( "SE_PORT_TREE: %*s%s visible=%d children=%d w=%.1f h=%.1f\n",
					nDepth * 2, "", pPanel->GetID(),
					(int)pPanel->BIsVisible(), pPanel->GetChildCount(),
					pPanel->GetActualLayoutWidth(), pPanel->GetActualLayoutHeight() );
				for ( int i = 0; i < pPanel->GetChildCount(); ++i )
				{
					Dump( pPanel->GetChild( i ), nDepth + 1 );
				}
			}
		};

		SEDumpPanelTree::Dump( pTestPanel->UIPanel(), 0 );
	}

	return true;
}


void CPanoramaEngineHandler::DestroyPanoramaTestView()
{
	if ( !m_pTestWindow )
	{
		Warning( "panorama: there is no test view\n" );
		return;
	}

	panorama::IUIWindow *pTestWindow = m_pTestWindow;
	m_pTestWindow = NULL;

	RemovePanoramaView( pTestWindow );
	if ( m_pUIEngine )
	{
		m_pUIEngine->DestroyWindow( pTestWindow );
	}

	Warning( "panorama: test view destroyed\n" );
}


//-----------------------------------------------------------------------------
// SE port (bring-up aid): a probe that is still readable when we need it.
//
// Warning()/Msg() only reach engine.log during roughly the first three seconds of a run (the port's
// log file stops being written after that), which makes every measurement taken once layout has run
// worthless - and a force kill can lose the tail on top of that.  This appends to its own file and
// flushes immediately, so it survives the log gap, a hard kill and an early crash.
//-----------------------------------------------------------------------------
void SE_PortUIProbe( const char *pFmt, ... )
{
	FILE *fp = fopen( "D:\\cstrike\\se_ui_probe.txt", "a" );
	if ( !fp )
		return;

	// Seconds since the engine started, so this file can be lined up with the screenshots the test
	// harness takes.
	fprintf( fp, "[%8.2f] ", Plat_FloatTime() );

	va_list args;
	va_start( args, pFmt );
	vfprintf( fp, pFmt, args );
	va_end( args );

	fflush( fp );
	fclose( fp );
}

// The menu root, remembered so the tree can be dumped again once layout has run.
static panorama::IUIPanel *s_pSEProbeMenuRoot = NULL;
static int s_nSEProbeDumpLines = 0;

//-----------------------------------------------------------------------------
// Dumps panel sizes/scale.  Called right after BLoadLayout (everything is still 0x0 there) and again
// a couple of seconds in, which is the dump that actually answers "is the size/scale right".
//-----------------------------------------------------------------------------
void SE_PortDumpPanelTree( panorama::IUIPanel *pPanel, int nDepth )
{
	if ( !pPanel || nDepth > 8 || s_nSEProbeDumpLines > 400 )
		return;

	++s_nSEProbeDumpLines;

	SE_PortUIProbe( "MENUTREE %*s%-34s visible=%d ch=%d w=%.1f h=%.1f desired=%.1fx%.1f render=%.1f scale=%.2f/%.2f\n",
		nDepth * 2, "", pPanel->GetID(),
		(int)pPanel->BIsVisible(), pPanel->GetChildCount(),
		pPanel->GetActualLayoutWidth(), pPanel->GetActualLayoutHeight(),
		pPanel->GetDesiredLayoutWidth(), pPanel->GetDesiredLayoutHeight(),
		pPanel->GetActualRenderWidth(),
		pPanel->GetActualUIScaleX(), pPanel->GetActualUIScaleY() );

	for ( int i = 0; i < pPanel->GetChildCount(); ++i )
		SE_PortDumpPanelTree( pPanel->GetChild( i ), nDepth + 1 );
}

//-----------------------------------------------------------------------------
// SE port: the CS:GO main menu view.  See the note in the header - the markup comes out of the
// retail panorama/code.pbin pack.
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// SE port: translate layout names handed to the hosted menu view.
//
//   "mainmenu.xml" is CS:GO's *inner* menu layout - CCSGO_MainMenu::CCSGO_MainMenu() loads it itself -
//   while the view has to load base_mainmenu.xml (which contains <CSGOMainMenu>).  Passing mainmenu.xml
//   to the view makes the class load the very layout it is being created from: the nested panel is then
//   discarded and the menu never appears, without any warning (the panel type *is* registered, so there
//   is no "not implemented" message either - the screen is simply empty).  Old command lines keep
//   working because the name is translated here.
//
//   Returns the replacement name, or NULL when nothing has to change.
//-----------------------------------------------------------------------------
static const char *SE_PortNormalizeMenuLayout( const char *pchLayout )
{
	static char s_rgchNormalized[ 256 ];

	const char *pchFound = V_strstr( pchLayout, "mainmenu.xml" );
	if ( !pchFound )
		return NULL;

	// only when "mainmenu.xml" is the file name itself (not e.g. "mymainmenu.xml" or "foo_mainmenu.xml")
	if ( pchFound != pchLayout && pchFound[ -1 ] != '/' && pchFound[ -1 ] != '\\' )
		return NULL;

	const int nPrefix = (int)( pchFound - pchLayout );
	V_strncpy( s_rgchNormalized, pchLayout, Min( nPrefix + 1, (int)V_ARRAYSIZE( s_rgchNormalized ) ) );
	s_rgchNormalized[ nPrefix ] = '\0';
	V_strncat( s_rgchNormalized, "base_mainmenu.xml", V_ARRAYSIZE( s_rgchNormalized ) );
	return s_rgchNormalized;
}


bool CPanoramaEngineHandler::CreatePanoramaMenuView()
{
	if ( m_pMenuWindow )
	{
		Warning( "panorama: the main menu view already exists (\"panorama_menu 0\" removes it)\n" );
		return true;
	}

	if ( !m_pUIEngine )
	{
		Warning( "panorama: the hosted UI isn't initialized, can't create the main menu view\n" );
		return false;
	}

	int nWidth = 0;
	int nHeight = 0;
	materials->GetBackBufferDimensions( nWidth, nHeight );
	if ( !nWidth || !nHeight )
	{
		Warning( "panorama: no back buffer size yet, the main menu view was not created\n" );
		return false;
	}

	SE_PortUIProbe( "WINDOW backbuffer=%dx%d\n", nWidth, nHeight );

	// SE port: false here, exactly like CS:GO - game/client/cstrike15/gameui/gameui_interface.cpp:544
	// creates the menu/HUD windows with bUseCustomMouseCursor = false.  false means
	// CTopLevelWindowSource2::SetMouseCursor() maps the panel's cursor style onto a standard cursor and
	// hands it to the input system (IInputSystem::GetStandardCursor + SetCursorIcon), i.e. the pointer is
	// the OS one.  The custom-cursor path (true) is only used by the Steam/VR overlay windows
	// (panorama/uitoplevelwindowoverlay.cpp, panorama/uitoplevelwindowopenvroverlay.cpp), which have no
	// OS window of their own.
	//
	// This used to fail in both directions: with false, the input system's cursor calls were still no-op
	// stubs (see CInputSystem::GetStandardCursor), and with true nothing draws the cursor either because
	// CMouseCursorRender is never created in a SOURCE2_PANORAMA build (uitoplevelwindow.cpp:43-47) -
	// while the engine hides the OS cursor whenever the UI owns the mouse.  Net result: no pointer.
	panorama::IUIWindow *pMenuWindow = m_pUIEngine->CreateNewUILayerWindow( 0, 0, nWidth, nHeight, false, false, false, true, "CSGOMainMenu", INPUT_CONTEXT_HANDLE_INVALID );
	panorama::IUIPanelClient *pMenuPanel = pMenuWindow ? AddPanoramaView( "CSGOMainMenu", pMenuWindow ) : NULL;
	if ( !pMenuPanel )
	{
		Warning( "panorama: could not create the main menu view\n" );
		return false;
	}

	m_pMenuWindow = pMenuWindow;

	// NOTE: the CS:GO API shim (MyPersonaAPI, PartyListAPI, ...) is not run from here.  Panorama keeps one
	// JavaScript context per layout, so running it against the menu panel only helped the panel that was
	// already open - the layout's own scripts (and every sub-layout pulled in with <Frame src="..."/>) still
	// aborted on their first unknown global.  CLayoutFile::BAddJavaScript injects the shim as the first
	// script of every layout instead, which covers all of those contexts.

	// This is what CS:GO's CCSGOMainMenu loads (game/client/cstrike15/panorama/csgo_mainmenu.cpp).
	// NOTE: the layout has to be the *view* layout (base_mainmenu.xml = <Panel class="WindowRoot">
	// containing <CSGOMainMenu>), because the CSGOMainMenu panel class itself loads mainmenu.xml.
	const char *pMenuLayout = panorama_menu_layout.GetString();
	const char *pchTranslated = SE_PortNormalizeMenuLayout( pMenuLayout );
	if ( pchTranslated )
	{
		Msg( "SE port: menu layout '%s' -> '%s' (CCSGO_MainMenu loads mainmenu.xml itself; the view loads "
			"base_mainmenu.xml)\n", pMenuLayout, pchTranslated );
		panorama_menu_layout.SetValue( pchTranslated );
		pMenuLayout = panorama_menu_layout.GetString();
	}

	if ( !pMenuPanel->UIPanel()->BLoadLayout( pMenuLayout ) )
	{
		Warning( "panorama: could not load %s - see the layout/style parsing errors above (every panel type"
			" this port does not implement is reported as 'panel type ... is not implemented')\n", pMenuLayout );
	}
	else
	{
		Warning( "panorama: loaded %s\n", pMenuLayout );
	}

	{
		s_pSEProbeMenuRoot = pMenuPanel->UIPanel();
		s_nSEProbeDumpLines = 0;
		SE_PortUIProbe( "MENUTREE (right after BLoadLayout - layout has not run yet):\n" );
		SE_PortDumpPanelTree( s_pSEProbeMenuRoot, 0 );
	}

	return true;
}


void CPanoramaEngineHandler::DestroyPanoramaMenuView()
{
	if ( !m_pMenuWindow )
	{
		Warning( "panorama: there is no main menu view\n" );
		return;
	}

	panorama::IUIWindow *pMenuWindow = m_pMenuWindow;
	m_pMenuWindow = NULL;

	RemovePanoramaView( pMenuWindow );
	if ( m_pUIEngine )
	{
		m_pUIEngine->DestroyWindow( pMenuWindow );
	}

	Warning( "panorama: main menu view destroyed\n" );
}


//-----------------------------------------------------------------------------
// Purpose: dumps what the hosted UI is currently doing (the panorama_status command)
//-----------------------------------------------------------------------------
void CPanoramaEngineHandler::PrintPanoramaStatus()
{
	Msg( "panorama: enabled=%d  uiengine=%p  uiclient=%p\n", (int)m_bValid, (void *)m_pUIEngine, (void *)g_pPanoramaUIClient );
	Msg( "panorama: interfaces '%s' / '%s'\n", PANORAMAUI_ENGINE_INTERFACE_VERSION, PANORAMAUI_CLIENT_INTERFACE_VERSION );

	if ( !m_bValid || !m_pUIEngine )
	{
		Msg( "panorama: the hosted UI isn't initialized - no %s provider (is panoramauiclient.dll loaded?)\n", PANORAMAUI_ENGINE_INTERFACE_VERSION );
		return;
	}

	Msg( "panorama: views=%d windows=%d ecom=%d debugger=%d forceBuiltPaintCmdCaches=%d testView=%s menuView=%s\n",
		m_Views.Count(), m_pWindows.Count(), (int)IsInECOMode(), (int)IsDebuggerShown(),
		(int)m_pUIEngine->BShouldUseForceBuiltPaintCmdCaches(), m_pTestWindow ? "yes" : "no", m_pMenuWindow ? "yes" : "no" );

	for ( int i = 0; i < m_Views.Count(); ++i )
	{
		panorama::IUIWindow *pWindow = m_Views[ i ].m_pWindow;
		Msg( "panorama:   view[%d] '%s' surface=%ux%u window=%ux%u\n", i, m_Views[ i ].m_sViewName.String(),
			pWindow ? pWindow->GetSurfaceWidth() : 0, pWindow ? pWindow->GetSurfaceHeight() : 0,
			pWindow ? pWindow->GetWindowWidth() : 0, pWindow ? pWindow->GetWindowHeight() : 0 );
	}
}


void CPanoramaEngineHandler::PanoramaRunFrame(int nSlot)
{
	bool bInECOMode = IsInECOMode();;
	if ( m_bInECOMode != bInECOMode )
	{
		static ConVarRef s_convarPanoramaBlurECOMode( "@panorama_blur_ecomode" );
		s_convarPanoramaBlurECOMode.SetValue( bInECOMode );
		m_bInECOMode = bInECOMode;
	}
	
	int nWd, nHt;
	materials->GetBackBufferDimensions( nWd, nHt );
	ChangeResolution( nWd, nHt );

	// SE port (bring-up aid): drive "the game changed to a lower resolution at run time".  Panorama
	// decodes SVG/images at the panel size * UI scale and CS:GO reloads them on a resolution change
	// (CImageResourceManager::OnResolutionChange -> ReloadChangedImage); this hook switches the video
	// mode after N frames so that path can be exercised without touching the options menu.
	//   D:\cstrike\se_switch.txt   =>   "<frame>,<width>,<height>"
	// (a file, not a command line switch: command line arguments routed through the test harness get
	//  mangled before the engine ever sees them)
	{
		static int s_nSESwitchFrame = -2;
		static int s_nSESwitchW = 0, s_nSESwitchH = 0;
		static int s_nSEFrameCount = 0;

		if ( s_nSESwitchFrame == -2 )
		{
			s_nSESwitchFrame = -1;
			FILE *fp = fopen( "D:\\cstrike\\se_switch.txt", "r" );
			if ( fp )
			{
				int nFrame = 0, nW = 0, nH = 0;
				if ( fscanf( fp, "%d,%d,%d", &nFrame, &nW, &nH ) == 3 && nFrame > 0 && nW > 0 && nH > 0 )
				{
					s_nSESwitchFrame = nFrame;
					s_nSESwitchW = nW;
					s_nSESwitchH = nH;
					SE_PortUIProbe( "SWITCH-ARMED frame=%d -> %dx%d", nFrame, nW, nH );
				}
				fclose( fp );
			}
		}

		if ( s_nSESwitchFrame > 0 && ++s_nSEFrameCount == s_nSESwitchFrame )
		{
			char pCmd[128];
			V_snprintf( pCmd, sizeof( pCmd ), "mat_setvideomode %d %d 1\n", s_nSESwitchW, s_nSESwitchH );
			SE_PortUIProbe( "SWITCH frame %d -> %s", s_nSEFrameCount, pCmd );
			Cbuf_AddText( pCmd );
		}
	}

	// SE port: the hosted views are created while the video mode is still settling (in the port the back
	// buffer goes 1920x1080 -> 1280x1024 -> 1280x720 during startup), and ChangeResolution() early-outs
	// once its cached size matches, so a view created inside that window can keep the surface size and
	// UI scale of the old mode.  That draws the whole UI oversized and clipped at the right/bottom edges
	// (a 1080-tall layout on a 720-tall surface is exactly 1.5x too big).  Re-sync every view whose
	// surface disagrees with the current back buffer; OnWindowResize/SetWindowScaleFactor invalidate the
	// panels, so the layout is redone at the right size.
	for ( int i = 0; i < m_Views.Count(); ++i )
	{
		ViewEntry_t &view = m_Views[i];
		panorama::IUIWindow *pView = view.m_pWindow;
		if ( !pView )
			continue;

		if ( (int)pView->GetSurfaceWidth() != nWd || (int)pView->GetSurfaceHeight() != nHt )
		{
			static int s_nSEResyncProbe = 0;
			if ( s_nSEResyncProbe < 20 )
			{
				++s_nSEResyncProbe;
				SE_PortUIProbe( "RESYNC %s surface=%ux%u -> %dx%d (scale %.4f -> %.4f)\n",
					view.m_sViewName.Get(), pView->GetSurfaceWidth(), pView->GetSurfaceHeight(), nWd, nHt,
					pView->GetWindowScaleFactor(), nHt / 1080.0f );
			}

			pView->OnWindowResize( nWd, nHt );
			pView->SetWindowScaleFactor( nHt / 1080.0f );
		}
	}

	// SE port: "the panorama menu is the menu" - DEFAULT ON since 2026-09-15 (user decision A).
	//
	// WHY: engine/keys.cpp filters VGUI *before* panorama (CS:GO's own order - harmless there because
	// CS:GO has no VGUI main menu), and this fork still ships the CS:S VGUI main menu.  While that menu
	// is up, HandleVGuiKey() returns true for every mouse/key event, Key_Event() returns early and the
	// hosted panorama UI never sees a button event at all.  Measured with the same click test on the
	// real mainmenu.xml (2026-09-15):
	//     VGUI gameui visible : KEY ... vguiConsumed=1 on every click, 0 x "INPUT type=0", 0 x onactivate
	//     VGUI gameui hidden  : vguiConsumed=0, 7 x "INPUT type=0 ... consumed=1", and
	//                           RadioButton#MainMenuNavBarInventory/Settings - onactivate fired
	// So the panorama HitTest was never the problem - the CS:S VGUI layer above it was.
	//
	// Behaviour: while a panorama menu view ('+panorama_menu') exists, hide the VGUI gameui root panel.
	// This is what CS:GO's structure amounts to (it has no VGUI menu to hide).
	//   -se_keep_vgui_menu     : opt out, keep the CS:S VGUI menu visible/clickable as before
	//   -se_panorama_menu_only : the original explicit switch; still honoured (and now a no-op)
	// NOTE: EngineVGui()->HideGameUI() cannot be used here: at the main menu (a background level) it
	// deliberately does not hide anything (engine/vgui_baseui_interface.cpp).
	{
		static int s_nSEHideGameUI = -1;
		static bool s_bSEGameUIHidden = false;
		if ( s_nSEHideGameUI == -1 )
		{
			s_nSEHideGameUI = CommandLine()->FindParm( "-se_keep_vgui_menu" ) ? 0 : 1;
			SE_PortUIProbe( "SE vgui-menu handling: %s%s\n",
				s_nSEHideGameUI ? "hide the gameui panel while a panorama menu exists (default)"
				                : "keep the gameui panel (-se_keep_vgui_menu)",
				CommandLine()->FindParm( "-se_panorama_menu_only" ) ? " [-se_panorama_menu_only also passed]" : "" );
		}
		if ( s_nSEHideGameUI )
		{
			bool bWantHidden = ( m_pMenuWindow != NULL );
			if ( bWantHidden != s_bSEGameUIHidden )
			{
				vgui::VPANEL hGameUI = EngineVGui() ? EngineVGui()->GetPanel( PANEL_GAMEUIDLL ) : 0;
				if ( hGameUI && vgui::ipanel() )
				{
					s_bSEGameUIHidden = bWantHidden;
					vgui::ipanel()->SetVisible( hGameUI, !bWantHidden );
					SE_PortUIProbe( "SE vgui gameui panel %s (panorama menu %s)\n",
						bWantHidden ? "hidden" : "shown", m_pMenuWindow ? "exists" : "absent" );
				}
			}
		}
	}

	// SE port (batch E): tick the ported CS:GO main menu panel class.  CS:GO does this from
	// CGameUI::RunFrame(); this tree has no gameui module, so the class is ticked here.  The panel class
	// drives the menu state machine (main menu <-> pause menu), the background movie, the vanity panel
	// and the "deny game input" lock.  engine.dll links no panorama library, so the tick is resolved out
	// of panoramauiclient.dll's export table (same pattern as the movie bridge below).
	//
	// Its return value says whether the class exists: that requires mainmenu.xml to instantiate
	// <CSGOMainMenu>, and when it does the movie bridge below is not needed (and must not run, it would
	// load the movie snippet a second time).
	bool bSEMainMenuTickHandled = false;
	{
		typedef bool ( *SEPortMainMenuTickFn )();
		static SEPortMainMenuTickFn s_pfnSETick = NULL;
		static bool s_bSETickResolved = false;
		if ( !s_bSETickResolved )
		{
			s_bSETickResolved = true;
			HMODULE hPanoramaModule = GetModuleHandleA( "panoramauiclient.dll" );
			if ( hPanoramaModule )
				s_pfnSETick = (SEPortMainMenuTickFn)GetProcAddress( hPanoramaModule, "SE_PortMainMenuTick" );
			SE_PortUIProbe( "SE main menu tick bridge: fn=%p\n", s_pfnSETick );
		}
		bSEMainMenuTickHandled = ( s_pfnSETick != NULL ) && s_pfnSETick();
	}

	// SE port of game/client/cstrike15/panorama/csgo_mainmenu.cpp::CCSGO_MainMenu::LoadBackgroundMovie
	// (panorama background webm, 2026-09-14):
	// mainmenu.xml only *declares* the reusable snippet "MainMenuMovieSnippet" and leaves
	// MainMenuMovieParent empty - in CS:GO the game-client class instantiates it.  This tree has no
	// cstrike15 client, so '#MainMenuMovie' never existed, and mainmenu.js::_SetBackgroundMovie()
	// bailed out on its IsValid() check: no movie panel, no webm, no background.
	// Step 1 (here): instantiate the snippet.  Step 2: the layout's script reads
	// ui_mainmenu_bkgnd_movie and calls SetMovie/SetSound/Play - it already ran (before the panel
	// existed), so call it once more explicitly.
	{
		static int s_nSEBackgroundMovie = 0;	// 0 = not tried yet
		if ( !bSEMainMenuTickHandled && s_nSEBackgroundMovie == 0 && s_pSEProbeMenuRoot && m_pUIEngine )
		{
			panorama::IUIPanel *pMovieParent = s_pSEProbeMenuRoot->FindChildInLayoutFile( "MainMenuMovieParent" );
			if ( pMovieParent )
			{
				s_nSEBackgroundMovie = 1;

				// The snippet instantiation + SetMovie/Play live in the panorama module (it knows the movie
				// control types); engine.dll deliberately links no panorama library, so the helper is
				// resolved out of panoramauiclient.dll's export table.
				typedef bool ( *SEPortLoadMovieFn )( panorama::IUIPanel *, const char * );
				static SEPortLoadMovieFn s_pfnSELoadMovie = NULL;
				static bool s_bSEBridgeResolved = false;
				if ( !s_bSEBridgeResolved )
				{
					s_bSEBridgeResolved = true;
					HMODULE hPanoramaModule = GetModuleHandleA( "panoramauiclient.dll" );
					if ( hPanoramaModule )
						s_pfnSELoadMovie = (SEPortLoadMovieFn)GetProcAddress( hPanoramaModule, "SE_PortLoadMainMenuBackgroundMovie" );
					SE_PortUIProbe( "SE movie bridge: module=%p fn=%p\n", hPanoramaModule, s_pfnSELoadMovie );
				}

				const char *pchMovie = CommandLine()->ParmValue( "-se_background_movie", "anubis720" );
				bool bStarted = ( s_pfnSELoadMovie != NULL ) && s_pfnSELoadMovie( s_pSEProbeMenuRoot, pchMovie );
				SE_PortUIProbe( "SE background movie: load('%s') -> %d\n", pchMovie, bStarted ? 1 : 0 );
			}
		}
	}

	RunFrame();
}


void CPanoramaEngineHandler::PanoramaRenderFrame( int nSlot )
{
	VPROF( "PanoramaRenderFrame" );

	if ( ( nSlot == k_EPanoramaSlotBeginFrame ) || ( nSlot == k_EPanoramaSlotEndFrame ) )
	{
		g_pMaterialSystem->ResetPanoramaRenderState();
		return;
	}

	for ( int i = 0; i < m_pWindows.Count(); i++ )
	{
		panorama::IUIWindow* pWindow = m_pWindows[ i ];

#if ( PLATFORM_WINDOWS && DEVELOPMENT_ONLY )
		if ( pWindow != m_pDebugWindow )
#endif
		{
			pWindow->RenderWindow( (PlatWindow_t)game->GetMainWindow(), false );		// swap chain 0 should be pulled out of layer/view probably
		}
	}
	
#if ( PLATFORM_WINDOWS && DEVELOPMENT_ONLY )
	// Make sure debugger windows runs it's anim thread even if not visible, because the paint 
	// continues to run every frame anyway, generating empty paint buffers, and these must be 
	// consumed by anim otherwise they just keep adding up in the paint queue
	if ( m_pDebugWindow )
	{
		m_pDebugWindow->RenderWindow( (PlatWindow_t)m_hDebuggerWindow, true );
	}
#endif
}

//-----------------------------------------------------------------------------
// build the order of windows to fire input at
//-----------------------------------------------------------------------------
void CPanoramaEngineHandler::RecalculateInputOrder()
{
	m_vecWindowInputOrder.RemoveAll();

	// for input HIGHER priority come FIRST (because it's on top of the stack)
	for ( int i = m_Views.Count() - 1; i >= 0; --i )
	{
		if ( m_Views.Element( i ).m_pWindow )
		{
			m_vecWindowInputOrder.AddToTail( m_Views.Element( i ).m_pWindow );
		}
	}
}

//-----------------------------------------------------------------------------
// Initialization, shutdown
//-----------------------------------------------------------------------------

InitReturnVal_t CPanoramaEngineHandler::Init()
{
	if ( CommandLine()->CheckParm( "-scaleform" ) ) // panorama disabled
	{
		// Explicitly don't set m_bValid so all other calls will no-op
		return INIT_OK;
	}

	if ( !g_pPanoramaUIClient )
	{
        Warning( "PanoramaUIClient interface not set up\n" );
		return INIT_FAILED;
	}
	
	m_pUIEngine = g_pPanoramaUIClient->SetupUIEngine( cl_language.GetString(), (PlatWindow_t) game->GetMainWindow() );
	if ( !m_pUIEngine )
	{
        // Message already shown.
		return INIT_FAILED;
	}

	// Ensure Panorama IME is enabled/disabled
	bool bIMEEnabled = g_pInputSystem->IsIMEAllowed();
	if( g_pPanoramaUIEngine )
	{
		g_pPanoramaUIEngine->SetIMEAllowed( bIMEEnabled );
	}
	if( g_pIMEManager )
	{
		// Toggle IME enabled
		g_pIMEManager->SetIMEEnabled( bIMEEnabled );
	}

#if TEST_PANORAMA_CONSOLE
	int nWidth = 0;
	int nHeight = 0;
	g_pRenderDevice->GetBackBufferDimensions( g_pEngineServiceMgr->GetEngineSwapChain(), &nWidth, &nHeight );
	if ( !nWidth || !nHeight )
	{
		return INIT_FAILED;
	}
	panorama::IUIPanelClient *pPanel = AddPanoramaView( "PanoramaEngine", m_pUIEngine->CreateNewUILayerWindow( 0, 0, nWidth, nHeight, false, false, false, true, "TestPanoramaConsole" ) );
	pPanel->UIPanel()->GetParentWindow()->SetVisible( false );
	const char *pLayoutFilename = "file://{resources}/layout/console.xml";
	if ( !pPanel->UIPanel()->BLoadLayout( pLayoutFilename ) )
	{
		return INIT_FAILED;
	}
#endif

	// SE port: smoke-test view.  CS:GO's engine never creates views itself - its game DLL does that
	// through IGameUIFuncs::AddPanoramaView - so until the game side is ported this creates one from a
	// hand-written layout (mods/panorama_test/panorama/layout/test.xml, i.e.
	// file://{resources}/layout/test.xml inside the mounted mod).  Enabled with -panoramatest, or at
	// runtime with the "panorama_test" console command.
	if ( CommandLine()->CheckParm( "-panoramatest" ) )
	{
		CreatePanoramaTestView();
	}

#if ( PLATFORM_WINDOWS && DEVELOPMENT_ONLY ) && !defined (DX_TO_GL_ABSTRACTION)
	m_pUIEngine->RegisterForUnhandledEvent(m_pUIEngine->MakeSymbol("ToggleDebugger"), UtlMakeDelegate(this, &CPanoramaEngineHandler::ToggleDebugger).GetAbstractDelegate());
	m_pUIEngine->RegisterForUnhandledEvent( m_pUIEngine->MakeSymbol( "BeginDebuggerInspect" ), UtlMakeDelegate( this, &CPanoramaEngineHandler::OnBeginDebuggerInspect ).GetAbstractDelegate() );
#endif

// 	m_pUIEngine->RegisterForUnhandledEvent( m_pUIEngine->MakeSymbol( "CloseDebuggerWindow" ), UtlMakeDelegate( this, &CPanoramaEngineHandler::OnCloseDebuggerWindow ).GetAbstractDelegate() );
	m_pUIEngine->RegisterForUnhandledEvent( m_pUIEngine->MakeSymbol( "TopLevelWindowClose" ), UtlMakeDelegate( this, &CPanoramaEngineHandler::OnWindowShutdown ).GetAbstractDelegate() );
	m_pUIEngine->RegisterForUnhandledEvent( m_pUIEngine->MakeSymbol( "ActivateMainWindow" ), UtlMakeDelegate( this, &CPanoramaEngineHandler::OnActivateMainWindow ).GetAbstractDelegate() );

	// Initialize the input context.
	// Currently all panorama top level windows are using this input context.
	// Note that we currently have no way of moving an input context in the stack (Source2 can)
	// so ensure that CPanoramaEngineHandler::Init() is called before CEngineVGui::Init() in order
	// to have the VGUI input context on top.
	// SE port: the input stack system is not implemented in this port, so g_pInputStackSystem is NULL -
	// record an invalid context instead of dereferencing it (panorama input still flows through
	// ProcessUserInput, only the Source2-style input stack arbitration is missing).
	m_hPanoramaInputContext = g_pInputStackSystem ? g_pInputStackSystem->PushInputContext() : INPUT_CONTEXT_HANDLE_INVALID;

	m_bInECOMode = IsInECOMode();
	static ConVarRef s_convarPanoramaBlurECOMode( "@panorama_blur_ecomode" );
	s_convarPanoramaBlurECOMode.SetValue( m_bInECOMode );

	m_bValid = true;
	return INIT_OK;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CPanoramaEngineHandler::Shutdown( void )
{
	if ( m_bValid )
	{
		if ( m_hPanoramaInputContext != INPUT_CONTEXT_HANDLE_INVALID )
		{
			if ( g_pInputStackSystem )
				g_pInputStackSystem->PopInputContext();
			m_hPanoramaInputContext = INPUT_CONTEXT_HANDLE_INVALID;
		}

#if ( PLATFORM_WINDOWS && DEVELOPMENT_ONLY ) && !defined ( DX_TO_GL_ABSTRACTION )
		m_pUIEngine->UnregisterForUnhandledEvent(m_pUIEngine->MakeSymbol("ToggleDebugger"), UtlMakeDelegate(this, &CPanoramaEngineHandler::ToggleDebugger).GetAbstractDelegate());
		m_pUIEngine->UnregisterForUnhandledEvent( m_pUIEngine->MakeSymbol( "BeginDebuggerInspect" ), UtlMakeDelegate( this, &CPanoramaEngineHandler::OnBeginDebuggerInspect ).GetAbstractDelegate() );
#endif
// 		m_pUIEngine->UnregisterForUnhandledEvent( m_pUIEngine->MakeSymbol( "CloseDebuggerWindow" ), UtlMakeDelegate( this, &CPanoramaEngineHandler::OnCloseDebuggerWindow ).GetAbstractDelegate() );
		m_pUIEngine->UnregisterForUnhandledEvent( m_pUIEngine->MakeSymbol( "TopLevelWindowClose" ), UtlMakeDelegate( this, &CPanoramaEngineHandler::OnWindowShutdown ).GetAbstractDelegate() );
		m_pUIEngine->UnregisterForUnhandledEvent( m_pUIEngine->MakeSymbol( "ActivateMainWindow" ), UtlMakeDelegate( this, &CPanoramaEngineHandler::OnActivateMainWindow ).GetAbstractDelegate() );

// 		for ( int i = 0; i < m_Views.Count(); i++ )
// 		{
// 			m_Views.Element( i ).m_Layer.Shutdown();
// 		}
		m_Views.RemoveAll();
		m_vecWindowInputOrder.RemoveAll();


#if ( PLATFORM_WINDOWS && DEVELOPMENT_ONLY ) && !defined ( DX_TO_GL_ABSTRACTION )
		if ( m_pDebugger != NULL )
		{
			::SendMessage( (HWND)m_hDebuggerWindow, WM_CLOSE, 0, 0 );
		}
#endif

	}

    if ( g_pPanoramaUIClient )
    {
        g_pPanoramaUIClient->ShutdownUIEngine();
    }

#if ( PLATFORM_WINDOWS && DEVELOPMENT_ONLY )
	m_pDebugWindow = NULL;
#endif

	m_bValid = false;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CPanoramaEngineHandler::IsInECOMode() const
{
	static ConVarRef gpu_level( "gpu_level" );
	static ConVarRef gpu_mem_level( "gpu_mem_level" );

	return ( ( s_convarPanoramaECOMode.GetInt() == 2 ) ||
			 ( ( s_convarPanoramaECOMode.GetInt() == 1 ) &&
			   ( ( GetCPUInformation()->m_nLogicalProcessors < 3 ) ||
		       ( gpu_level.GetInt() <= GPU_LEVEL_MEDIUM ) ||
			   ( gpu_mem_level.GetInt() <= GPU_MEM_LEVEL_LOW ) ) ) );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CPanoramaEngineHandler::RunFrame()
{
	if ( m_bValid )
	{
		bool bUseForceBuiltPaintCmdCaches = !g_ClientDLL->HudShouldPaintThisFrame();

		// SE port (bring-up aid): this flag decides whether CTopLevelWindow::PerformLayout() runs at all.
		{
			static int s_nSEForceCacheProbe = 0;
			if ( s_nSEForceCacheProbe < 3 )
			{
				s_nSEForceCacheProbe++;
				Warning( "SE_PORT_FORCECACHE: clientHudPaints=%d forceCaches=%d debugger=%d\n",
					(int)g_ClientDLL->HudShouldPaintThisFrame(), (int)bUseForceBuiltPaintCmdCaches, (int)IsDebuggerShown() );
			}
		}

		m_pUIEngine->SetUseForceBuiltPaintCmdCaches( bUseForceBuiltPaintCmdCaches && !IsDebuggerShown() );
		m_pUIEngine->RunFrame();
	}

	// delete any views that were removed last frame
// 	if ( m_vecViewsToRemove.Count() )
// 	{
// 		FOR_EACH_VEC( m_vecViewsToRemove, iRemove )
// 		{
// 			for ( int iView = 0; iView < m_Views.Count(); iView++ )
// 			{
// 				ViewEntry_t &view = m_Views.Element( iView );
// 				if ( view.m_sViewName == m_vecViewsToRemove[ iRemove ] )
// 				{
// 					m_Views.Remove( iView );
// 					break;
// 				}
// 			}
// 		}
// 		m_vecViewsToRemove.RemoveAll();
// 
// 		RecalculateInputOrder();
//	}


	// Check if panorama is denying input to the game by iterating m_denyAllInputEntries
	// Only denying input to the game if a panel and its top level window are visible

	m_eGameInputFlags = k_EGameInputFlagsNone;
	FOR_EACH_VEC_BACK( m_denyAllInputEntries, nEntry )
	{
		const DenyInputEntry_t &denyEntry = m_denyAllInputEntries[nEntry];

		if ( denyEntry.m_panelHandle == PanelHandle_t::InvalidHandle() )
		{
			// AddDenyAllInputToGame called with a NULL panel, always filter input events
			m_eGameInputFlags |= denyEntry.m_eGameInputFlags;
		}
		else
		{
			panorama::IUIPanel *pPanel = m_pUIEngine->GetPanelPtr( denyEntry.m_panelHandle );
			if ( pPanel )
			{
				if ( pPanel->BIsVisible() && pPanel->GetParentWindow()->BIsVisible() )
				{
					m_eGameInputFlags |= denyEntry.m_eGameInputFlags;
				}
			}
			else
			{
				// Panel deleted without calling ReleaseDenyAllInputToGame, just remove it from the entries
				m_denyAllInputEntries.Remove( nEntry );
			}
		}
	}
	
	// Enable/Disable input context
	// This controls whether the mouse cursor is visible
	if ( g_pInputStackSystem )
	{
		g_pInputStackSystem->EnableInputContext( m_hPanoramaInputContext, ( m_eGameInputFlags & k_EGameInputUIEnableMouseCursor ) == k_EGameInputUIEnableMouseCursor );
	}

	// SE port: the OS cursor also has to be *visible* while the UI owns the mouse.  CS:GO leaves that to
	// the input stack system (IInputStackSystem::SetCursorVisible through EnableInputContext above), which
	// this tree does not have, and the game hides the cursor when it captures the mouse for gameplay
	// (::ShowCursor( FALSE ) - e.g. sys_mainwind.cpp:1360 hides it while the startup movies play) and
	// never shows it again.  So manage it here, with our own count so the game's own hide/show calls keep
	// working once the UI gives the mouse back.
	static int s_nSEPortCursorShows = 0;
	const bool bSEUIWantsCursor = ( m_eGameInputFlags & k_EGameInputUIEnableMouseCursor ) == k_EGameInputUIEnableMouseCursor;
	if ( bSEUIWantsCursor && s_nSEPortCursorShows == 0 )
	{
		++s_nSEPortCursorShows;

		// Stop VGUI from touching the cursor while the UI owns it.  VGUI's idea of "the panel under the
		// mouse" does not include panorama panels, so it keeps setting vgui::dc_none - i.e. NULL - on
		// every mouse move (vguimatsurface/Input.cpp: IE_SetCursor -> ActivateCurrentCursor ->
		// CursorSelect), which is what made the pointer disappear.  CMatSystemSurface::SetCursor() returns
		// early once the cursor is locked, which is exactly what is needed here.  (CS:GO does not have this
		// problem because its VGUI cursor is per input context - vguimatsurface/Cursor.cpp:205.)
		if ( vgui::surface() )
			vgui::surface()->LockCursor();

		if ( g_pInputSystem )
			g_pInputSystem->SetCursorIcon( g_pInputSystem->GetStandardCursor( INPUT_CURSOR_ARROW ) );

		while ( ::ShowCursor( TRUE ) < 0 ) { }
	}
	else if ( !bSEUIWantsCursor && s_nSEPortCursorShows > 0 )
	{
		--s_nSEPortCursorShows;

		if ( vgui::surface() )
			vgui::surface()->UnlockCursor();
		if ( g_pInputSystem )
			g_pInputSystem->ResetCursorIcon();

		::ShowCursor( FALSE );
	}

	// Panorama taking over cursor control if EnableMouseCursor set
	// cl_mouseenable 0 stops the game from handling mouse input
	ConVarRef cl_mouseenable( "cl_mouseenable" );
	if ( cl_mouseenable.IsValid() )
	{
		cl_mouseenable.SetValue( ( m_eGameInputFlags & k_EGameInputUIEnableMouseCursor ) != k_EGameInputUIEnableMouseCursor );
	}

	// If we are only denying mouse movement to the game, re-enable button events to be handled by the game
	// SE port: cl_mouseenable_buttons is a CS:GO client ConVar that this tree does not have out of the box
	// (it is declared in game/client/in_mouse.cpp now).  The IsValid() test keeps a per-frame ConVarRef
	// from spamming the console when a mod does not provide it.
	ConVarRef cl_mouseenable_buttons( "cl_mouseenable_buttons" );
	if ( cl_mouseenable_buttons.IsValid() )
	{
		cl_mouseenable_buttons.SetValue( ( m_eGameInputFlags & ( k_EGameInputUIEnableMouseCursor | k_EGameInputDenyGameMouseClicks ) ) == k_EGameInputUIEnableMouseCursor );
	}
}

//-----------------------------------------------------------------------------
// GetKeyModifierFlags uses the Windows GetKeyState function to retrieve the 
// status of the Shift/Control/Alt keys.
// This should be called from the WindowProc in response to a keyboard input
// message. It returns the state of the key at the time the input message was
// generated (see Microsoft docs for GetKeyState).
//-----------------------------------------------------------------------------
static uint GetKeyModifierFlags()
{
	int new_mods = 0;

#if !defined( POSIX ) && !defined( USE_SDL )
	if ( ::GetKeyState( VK_SHIFT ) & 0x8000 )
		new_mods |= IE_ShiftPressed;
	if ( ::GetKeyState( VK_CONTROL ) & 0x8000 )
		new_mods |= IE_ControlPressed;
	if ( ::GetKeyState( VK_MENU ) & 0x8000 )
		new_mods |= IE_AltPressed;
	if ( ( ::GetKeyState( VK_LWIN ) & 0x8000 ) || ( ::GetKeyState( VK_RWIN ) & 0x8000 ) )
		new_mods |= IE_GuiPressed;
#endif

	return new_mods;
}

//-----------------------------------------------------------------------------
// Purpose: handle user input for our main or debugger window
//-----------------------------------------------------------------------------
bool CPanoramaEngineHandler::ProcessUserInput( const InputEvent_t &inputEvent )
{
	if ( !m_bValid )
		return false;

#if ( PLATFORM_WINDOWS && DEVELOPMENT_ONLY && !DX_TO_GL_ABSTRACTION )
	// Close debugger if we are minimising

	if ( ( inputEvent.m_nType == IE_WindowSizeChanged ) && ( inputEvent.m_nData3 == 1 ) )
	{
		if ( IsDebuggerShown() ) ToggleDebugger();
	}
#endif

	// Skip some events based on input settings
	EGameInputFlags inputFlags = m_eGameInputFlags;
	bool bHandleEvent, bAlwaysConsume;
	switch ( inputEvent.m_nType )
	{
	case IE_KeyTyped:
	case IE_KeyCodeTyped:
	case IE_KeyCodeReleased:
		bHandleEvent = ( inputFlags & k_EGameInputUIEnableKeyInput ) != 0;
		bAlwaysConsume = ( inputFlags & k_EGameInputDenyGameKeys ) != 0;
		break;

	case IE_ButtonPressed:
	case IE_ButtonPressedRepeating:
	case IE_ButtonDoubleClicked:
	case IE_ButtonReleased:
		if ( inputEvent.m_nData >= ::KEY_FIRST && inputEvent.m_nData <= ::KEY_LAST )
		{
			bHandleEvent = ( inputFlags & k_EGameInputUIEnableKeyInput ) != 0;
			bAlwaysConsume = ( inputFlags & k_EGameInputDenyGameKeys ) != 0;
		}
		else if ( inputEvent.m_nData >= ::MOUSE_FIRST && inputEvent.m_nData <= ::MOUSE_LAST )
		{
			bHandleEvent = ( inputFlags & k_EGameInputUIEnableMouseCursor ) != 0;
			bAlwaysConsume = ( inputFlags & k_EGameInputDenyGameMouseClicks ) != 0;
		}
		else
		{
			// all buttons that are not on keyboard/mouse are assumed to be on 'controllers'
			// of various types
			bHandleEvent = ( inputFlags & k_EGameInputUIEnableControllerInput ) != 0;
			bAlwaysConsume = ( inputFlags & k_EGameInputDenyGameControllerInput ) != 0;
		}
		break;

	case IE_AnalogValueChanged:
		if ( inputEvent.m_nData >= 0 && inputEvent.m_nData < ::JOYSTICK_FIRST_AXIS )
		{
			// mouse events are located here in AnalogCode_t
			bHandleEvent = ( inputFlags & k_EGameInputUIEnableMouseCursor ) != 0;
			bAlwaysConsume = ( inputFlags & k_EGameInputDenyGameMouseMovement ) != 0;
		}
		else
		{
			bHandleEvent = ( inputFlags & k_EGameInputUIEnableControllerInput ) != 0;
			bAlwaysConsume = ( inputFlags & k_EGameInputDenyGameControllerInput ) != 0;
		}
		break;
	default:
		bHandleEvent = true;
		bAlwaysConsume = false;
	}

	// TEMP HACK to fix Panorama debugger
	//   Make sure panorama processes all input events to handle global keybinds such as 'F6'
	bHandleEvent = true;
	// END TEMP HACK

	if ( !bHandleEvent )
		return bAlwaysConsume;

	InputEvent_t updatedEvent = inputEvent;
	bool result = false;

	Assert( g_pPanoramaUIClient );
	if ( g_pPanoramaUIClient )
	{
		if ( ( inputEvent.m_nType == IE_AnalogValueChanged ) )
		{
			AnalogCode_t code = ( AnalogCode_t )inputEvent.m_nData;
			if ( ( code == MOUSE_XY ) )
			{
				updatedEvent.m_nType = IE_LocateMouseClick;
				updatedEvent.m_nData = inputEvent.m_nData2;
				updatedEvent.m_nData2 = inputEvent.m_nData3;

				Plat_WindowToScreenCoords( ( PlatWindow_t )updatedEvent.m_hWnd, updatedEvent.m_nData, updatedEvent.m_nData2 );
			}
		}
		else if ( ( inputEvent.m_nType == IE_ButtonPressed ) || ( inputEvent.m_nType == IE_ButtonReleased ) )
		{
			if ( inputEvent.m_nData >= ::KEY_FIRST && inputEvent.m_nData <= ::KEY_LAST )
			{
				updatedEvent.m_nData3 = GetKeyModifierFlags();
			}
			else if ( inputEvent.m_nData >= ::MOUSE_FIRST && inputEvent.m_nData <= ::MOUSE_5 )
			{
				updatedEvent.m_nData2 = GetKeyModifierFlags();
			}
		}

#if( PLATFORM_WINDOWS && DEVELOPMENT_ONLY )
		// If debugger is up and focused then only process input in the game window if it has focus, otherwise always process so hover works
		// without focus.  Would be better if we had more clear handling of which window is above the other in terms of window z-stack, but 
		// the plumbing makes that hard since we pump the debugger window elsewhere.
		if ( IsDebuggerShown() )
			result = g_pPanoramaUIClient->HandleInputEvent(updatedEvent, m_vecWindowInputOrder, true);
		else
#endif
			result = g_pPanoramaUIClient->HandleInputEvent(updatedEvent, m_vecWindowInputOrder, false);
	}

	// Don't eat this key if we didn't use it, and it happens to be the console toggle key
	if ( result == false )
	{
		if ( inputEvent.m_nType == IE_ButtonPressed )
		{
			ButtonCode_t code = (ButtonCode_t)inputEvent.m_nData;
			const char *kb = Key_BindingForKey( code );
			if( kb && !V_stricmp( kb, "toggleconsole" ) )
			{
				bAlwaysConsume = false;
			}
		}
	}

	if ( bAlwaysConsume )
		result = true;

	return result;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
uint64 CPanoramaEngineHandler::AddGameInputHandler( panorama::IUIPanel *pPanel, panorama::EGameInputFlags eFlags, const char *pchDebugContextName )
{
	PanelHandle_t panelHandle = PanelHandle_t::InvalidHandle();
	if ( pPanel )
	{
		panelHandle = m_pUIEngine->GetPanelHandle( pPanel );
	}
	
	DenyInputEntry_t entry;
	entry.m_handle = m_nNextInputHandle++;
	entry.m_panelHandle = panelHandle;
	entry.m_debugName = pchDebugContextName;
	entry.m_eGameInputFlags = eFlags;
	m_denyAllInputEntries.AddToTail( entry );
	
	if ( !m_nNextInputHandle )
	{
		// 0 is not a valid handle
		m_nNextInputHandle = 1;
	}
	
	InputDevMsg( 
		"AddGameInputHandler - handle=%llu, flags=0x%02x dbgContextName=%s, panelName=%s\n", 
		entry.m_handle,
		(int)entry.m_eGameInputFlags,
		( pchDebugContextName ? pchDebugContextName :"--" ),
		( ( pPanel && pPanel->BHasID() ) ? pPanel->GetID() :"--" ));

	return entry.m_handle;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPanoramaEngineHandler::ReleaseGameInputHandler( uint64 handle )
{
	InputDevMsg(
		"ReleaseGameInputHandler - handle=%llu\n",
		handle );

	int nMatch = -1;
	// We do not expect a lot of elements in m_denyAllInputEntries vector
	// Iterating the vector to find the element matching the handle should be fast.
	// Probably worth adding a warning if we start adding too many elements to m_denyAllInputEntries
	FOR_EACH_VEC( m_denyAllInputEntries, nEntry )
	{
		if ( m_denyAllInputEntries[nEntry].m_handle == handle )
		{
			nMatch = nEntry;
			break;
		}
	}
	if ( nMatch != -1 )
	{
		m_denyAllInputEntries.FastRemove( nMatch );
	}
	else
	{
		Warning( "Calling ReleaseGameInputHandler with an invalid handle. Cause: AddGameInputHandler never called or ReleaseGameInputHandler already called for the given handle." );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static CUtlString EnumGameInputFlagsToString( EGameInputFlags flags )
{
	CUtlString strFlags;

	// For now, only printing "deny" flags as Panorama will process all input events
	// (and so all k_EGameInputUIEnable... are irrelevant)
	if ( flags & k_EGameInputDenyGameMouseMovement )
	{
		strFlags += " DenyGameMovement";
	}
	if ( flags & k_EGameInputDenyGameMouseClicks )
	{
		strFlags += " DenyGameMouseClicks";
	}
	if ( flags & k_EGameInputDenyGameControllerInput )
	{
		strFlags += " DenyGameController";
	}
	if ( flags & k_EGameInputDenyGameKeys )
	{
		strFlags += " DenyGameKeys";
	}

	return strFlags;
}

void CPanoramaEngineHandler::DumpDenyAllInputToGame() const
{
	DevMsg( "\nDenyAllInputToGame dump:\n" );
	DevMsg( "m_eGameInputFlags = 0x%02x (%s)\n", ( int )m_eGameInputFlags, EnumGameInputFlagsToString( m_eGameInputFlags ).String() );
	FOR_EACH_VEC( m_denyAllInputEntries, nEntry )
	{
		const DenyInputEntry_t &denyEntry = m_denyAllInputEntries[ nEntry ];

		DevMsg( "\t0x%02x (%s) : %s (", ( int )denyEntry.m_eGameInputFlags, EnumGameInputFlagsToString( denyEntry.m_eGameInputFlags ).String(), denyEntry.m_debugName.String() );
		if ( denyEntry.m_panelHandle == PanelHandle_t::InvalidHandle() )
		{
			DevMsg( "NULL panel" );
		}
		else
		{
			panorama::IUIPanel *pPanel = m_pUIEngine->GetPanelPtr( denyEntry.m_panelHandle );
			if ( pPanel )
			{
				DevMsg( 
					"panel ID = %s, panel vis = %s, top level vis = %s",
					pPanel->GetID(),
					( pPanel->BIsVisible() ? "true" : "false" ),
					( pPanel->GetParentWindow()->BIsVisible() ? "true" : "false" ));
			}
			else
			{
				DevMsg( "panel deleted !!!" );
			}
		}
		DevMsg( ")\n" );
	}
	DevMsg( "End dump.\n\n" );
}


//-----------------------------------------------------------------------------
// Purpose: Pass IME control to panorama.
//-----------------------------------------------------------------------------
void CPanoramaEngineHandler::SetIMEAllowed( bool bAllowed )
{
	if ( !m_pUIEngine || !m_pUIEngine->UIInputEngine() )
	{
		return;
	}

	m_pUIEngine->UIInputEngine()->SetIMEAllowed( bAllowed );
}


#if ( PLATFORM_WINDOWS && DEVELOPMENT_ONLY ) && !defined (DX_TO_GL_ABSTRACTION)

//-----------------------------------------------------------------------------
// Purpose: Create debugger window if not already created
//-----------------------------------------------------------------------------
bool CPanoramaEngineHandler::OnCreateDebuggerWindow()
{
	// Moved from constructor, need to have loaded saved CVARS from disk which happens later in the startup process (after Init)
	m_nDebuggerX = CommandLine()->ParmValue( "-pdbgx", panorama_debugger_saved_xpos.GetInt() );
	m_nDebuggerY = CommandLine()->ParmValue( "-pdbgy", panorama_debugger_saved_ypos.GetInt() );
	m_nDebuggerW = CommandLine()->ParmValue( "-pdbgw", panorama_debugger_saved_width.GetInt() );
	m_nDebuggerH = CommandLine()->ParmValue( "-pdbgh", panorama_debugger_saved_height.GetInt() );

 	// Create the window
	int nPlatWindowFlags = 0;

	 m_hDebuggerWindow = CreateAppWindow( "Panorama Debugger", nPlatWindowFlags, m_nDebuggerX, m_nDebuggerY, m_nDebuggerW, m_nDebuggerH );
 	if ( m_hDebuggerWindow == PLAT_WINDOW_INVALID )
 		return false;

	if ( m_pDebugWindow )
	{
		RemovePanoramaView( m_pDebugWindow );
		UIEngine()->DestroyWindow( m_pDebugWindow );
	}

	m_pDebugWindow = m_pUIEngine->CreateNewUILayerWindow( m_nDebuggerX, m_nDebuggerY, m_nDebuggerW, m_nDebuggerH, false, false, false, true, "PanoramaDebugger", m_hPanoramaInputContext );
	panorama::IUIPanelClient *pDbgRootPanel = AddPanoramaView( "PanoramaDebugger", m_pDebugWindow );
	// Parent panel for debugger doesn't currently do anything but still needs a layout applied because it's visible, squelch an assert with an empty layout.
	pDbgRootPanel->UIPanel()->BLoadLayoutFromString( "<root><Panel hittest = \"false\"> <!-- Empty layout file --> </Panel></root>" );

	m_pDebugger = g_pPanoramaUIClient->CreateDebugger(m_pDebugWindow, "Debugger");

	m_pDebugWindow->OnWindowResize( m_nDebuggerW, m_nDebuggerH );
	m_pDebugWindow->SetWindowScaleFactor( 1.0f );			// Debugger text becomes too small if we scale it here.

	m_pDebugWindow->SetVisible(true);

	SetDebuggerShown( true );

	// 7LSTODO DenyInputToGame ????
	g_pInputSystem->DisableMouseCapture();

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Close debugger window
//-----------------------------------------------------------------------------
bool CPanoramaEngineHandler::OnCloseDebuggerWindow()
{
	if ( !m_pDebugWindow )
	{
		m_pDebugger = NULL;
 		m_hDebuggerWindow = PLAT_WINDOW_INVALID;
		return true;
	}

	if ( m_hDebuggerDenyInputToGame )
	{
		ReleaseGameInputHandler( m_hDebuggerDenyInputToGame );
		m_hDebuggerDenyInputToGame = 0;
	}
	SaveDebuggerDimentions();

	m_pDebugWindow->SetVisible(false);
	m_pUIEngine->CloseDebuggerWindow();
	delete m_pDebugger;
 	m_pDebugger = NULL;
	SetDebuggerShown( false );
	return true;
}

void CPanoramaEngineHandler::SaveDebuggerDimentions()
{
	panorama_debugger_saved_xpos.SetValue( m_nDebuggerX );
	panorama_debugger_saved_ypos.SetValue( m_nDebuggerY );
	panorama_debugger_saved_width.SetValue( m_nDebuggerW );
	panorama_debugger_saved_height.SetValue( m_nDebuggerH );
}

void CPanoramaEngineHandler::OnDebuggerResize( uint32 nNewWidth, uint32 nNewHeight )
{
	if ( m_pDebugWindow )
	{
		m_nDebuggerW = nNewWidth;
		m_nDebuggerH = nNewHeight;
		m_pDebugWindow->OnWindowResize( nNewWidth, nNewHeight );
		SaveDebuggerDimentions();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Tell debugger to enter inspection mode
//-----------------------------------------------------------------------------
bool CPanoramaEngineHandler::OnBeginDebuggerInspect()
{
	if (IsDebuggerShown())
	{
 		// We need the cursor to be visible in inspection mode
		if ( !m_hDebuggerDenyInputToGame )
		{
			m_hDebuggerDenyInputToGame = AddGameInputHandler( nullptr, k_EGameInputCaptureAll, "PanoramaDebugger" );
		}
 		m_pDebugger->BeginInspect();
	}
	return true;
}

#endif	// ( PLATFORM_WINDOWS && DEVELOPMENT_ONLY ) && !defined (DX_TO_GL_ABSTRACTION)


//-----------------------------------------------------------------------------
// Purpose: Main window was activated
//-----------------------------------------------------------------------------
bool CPanoramaEngineHandler::OnActivateMainWindow()
{
	// pick the first visible window in input order to be the one to inspect
	FOR_EACH_VEC( m_vecWindowInputOrder , i )
	{
		if ( m_vecWindowInputOrder[i]->BIsVisible() )
		{
			m_vecWindowInputOrder[i]->Activate( true );
			break;
		}
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: track windows closing
//-----------------------------------------------------------------------------
bool CPanoramaEngineHandler::OnWindowShutdown( IUIWindow *pWindow )
{
#if ( PLATFORM_WINDOWS && DEVELOPMENT_ONLY )
	if ( pWindow == m_pDebugWindow )
	{
		m_pUIEngine->CloseDebuggerWindow();
	}
#endif

	for ( int i = 0; i < m_Views.Count(); i++ )
	{
		ViewEntry_t &view = m_Views.Element( i );
		if ( pWindow == view.m_pWindow )
		{
//			view.m_Layer.Shutdown();
			view.m_pWindow = NULL;
			m_vecViewsToRemove.AddToTail( view.m_sViewName );
			RecalculateInputOrder();
			break;
		}
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: resize out windows hosted in the main render context if needed
//-----------------------------------------------------------------------------
void CPanoramaEngineHandler::ChangeResolution( const int nWindowWidth, const int nWindowHeight )
{
	// SE port probe: the UI scale comes from this height, so log the sequence (capped).
	{
		static int s_nSEChgResProbe = 0;
		if ( s_nSEChgResProbe < 30 )
		{
			++s_nSEChgResProbe;
			SE_PortUIProbe( "CHANGERES %d x %d (was %d x %d) views=%d scale=%.4f\n",
				nWindowWidth, nWindowHeight, m_nMainWindowWidth, m_nMainWindowHeight, m_Views.Count(),
				nWindowHeight / 1080.0f );
		}
	}

	if ( m_nMainWindowWidth != nWindowWidth || m_nMainWindowHeight != nWindowHeight )
	{
		if( (m_nMainWindowHeight > 0) && (m_nMainWindowHeight != nWindowHeight) )
		{
			float fRelativeScalefactor = (float)nWindowHeight / (float)m_nMainWindowHeight;
			m_pUIEngine->OnResolutionChange( fRelativeScalefactor );
		}

		m_nMainWindowWidth = nWindowWidth;
	m_nMainWindowHeight = nWindowHeight;

		for ( int i = 0; i < m_Views.Count(); i++ )
		{
			ViewEntry_t &view = m_Views.Element( i );
#if( PLATFORM_WINDOWS && DEVELOPMENT_ONLY )
			if (view.m_pWindow != m_pDebugWindow)
#endif
			{
				view.m_pWindow->OnWindowResize( nWindowWidth, nWindowHeight );
				view.m_pWindow->SetWindowScaleFactor( nWindowHeight / 1080.0f );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Turn on telemetry
//-----------------------------------------------------------------------------
void CPanoramaEngineHandler::OnProfileOnEvent()
{
#ifdef RAD_TELEMETRY_ENABLED
	TelemetrySetLevel( 7 );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Turn off telemetry
//-----------------------------------------------------------------------------
void CPanoramaEngineHandler::OnProfileOffEvent()
{
#ifdef RAD_TELEMETRY_ENABLED
	TelemetrySetLevel( 0 );
#endif
}

#if ( PLATFORM_WINDOWS && DEVELOPMENT_ONLY ) && !defined (DX_TO_GL_ABSTRACTION)

//--------------------------------------------------------------------------------------------------
// Toggle debugger status
//--------------------------------------------------------------------------------------------------

void CPanoramaEngineHandler::ToggleDebugger()
{
	bool bShow = !(IsDebuggerShown());

	if ( bShow )
	{
		OnCreateDebuggerWindow();
	}
	else
	{
		::SendMessage( (HWND)m_hDebuggerWindow, WM_CLOSE, 0, 0 );
	}

}

//--------------------------------------------------------------------------------------------------
// Panorama debugger window
// The debugger window has it's own window proc, creation etc.
// This is for 2 reasons :
// - src1 input only allows one attached window. We could try to use user events etc... but..
//	- the window sends input directly to panorama and panorama does not have to figure out the source
//
// - materialsystem/shaderapi
//	- Does not support additional swap chains via dx9
//	- additional swap chains can have issues on non primary output.
//	- hooking/overlays break additional swap chains
//
// So it's own window is the quickest solution. We can revisit as required (eg perf).
//
//--------------------------------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Window management
//-----------------------------------------------------------------------------

// ( Some interbal code from inputsystem duplication here )

static ButtonCode_t s_pScanToButtonCode[ 128 ] =
{
	//	0				1				2				3				4				5				6				7 
	//	8				9				A				B				C				D				E				F 
	::KEY_NONE, ::KEY_ESCAPE, ::KEY_1, ::KEY_2, ::KEY_3, ::KEY_4, ::KEY_5, ::KEY_6,			// 0
	::KEY_7, ::KEY_8, ::KEY_9, ::KEY_0, ::KEY_MINUS, ::KEY_EQUAL, ::KEY_BACKSPACE, ::KEY_TAB,		// 0 

	::KEY_Q, ::KEY_W, ::KEY_E, ::KEY_R, ::KEY_T, ::KEY_Y, ::KEY_U, ::KEY_I,			// 1
	::KEY_O, ::KEY_P, ::KEY_LBRACKET, ::KEY_RBRACKET, ::KEY_ENTER, ::KEY_LCONTROL, ::KEY_A, ::KEY_S,			// 1 

	::KEY_D, ::KEY_F, ::KEY_G, ::KEY_H, ::KEY_J, ::KEY_K, ::KEY_L, ::KEY_SEMICOLON,	// 2 
	::KEY_APOSTROPHE, ::KEY_BACKQUOTE, ::KEY_LSHIFT, ::KEY_BACKSLASH, ::KEY_Z, ::KEY_X, ::KEY_C, ::KEY_V,			// 2 

	::KEY_B, ::KEY_N, ::KEY_M, ::KEY_COMMA, ::KEY_PERIOD, ::KEY_SLASH, ::KEY_RSHIFT, ::KEY_PAD_MULTIPLY,// 3
	::KEY_LALT, ::KEY_SPACE, ::KEY_CAPSLOCK, ::KEY_F1, ::KEY_F2, ::KEY_F3, ::KEY_F4, ::KEY_F5,			// 3 

	::KEY_F6, ::KEY_F7, ::KEY_F8, ::KEY_F9, ::KEY_F10, ::KEY_NUMLOCK, ::KEY_SCROLLLOCK, ::KEY_HOME,		// 4
	::KEY_UP, ::KEY_PAGEUP, ::KEY_PAD_MINUS, ::KEY_LEFT, ::KEY_PAD_5, ::KEY_RIGHT, ::KEY_PAD_PLUS, ::KEY_END,		// 4 

	::KEY_DOWN, ::KEY_PAGEDOWN, ::KEY_INSERT, ::KEY_DELETE, ::KEY_NONE, ::KEY_NONE, ::KEY_NONE, ::KEY_F11,		// 5
	::KEY_F12, ::KEY_BREAK, ::KEY_NONE, ::KEY_NONE, ::KEY_NONE, ::KEY_NONE, ::KEY_NONE, ::KEY_NONE,		// 5

	::KEY_NONE, ::KEY_NONE, ::KEY_NONE, ::KEY_NONE, ::KEY_NONE, ::KEY_NONE, ::KEY_NONE, ::KEY_NONE,		// 6
	::KEY_NONE, ::KEY_NONE, ::KEY_NONE, ::KEY_NONE, ::KEY_NONE, ::KEY_NONE, ::KEY_NONE, ::KEY_NONE,		// 6 

	::KEY_NONE, ::KEY_NONE, ::KEY_NONE, ::KEY_NONE, ::KEY_NONE, ::KEY_NONE, ::KEY_NONE, ::KEY_NONE,		// 7
	::KEY_NONE, ::KEY_NONE, ::KEY_NONE, ::KEY_NONE, ::KEY_NONE, ::KEY_NONE, ::KEY_NONE, ::KEY_NONE		// 7 
};

static ButtonCode_t ButtonCode_ScanCodeToButtonCode( int lParam )
{
	int nScanCode = ( lParam >> 16 ) & 0xFF;
	if ( nScanCode > 127 )
		return ::KEY_NONE;

	ButtonCode_t result = s_pScanToButtonCode[ nScanCode ];

	bool bIsExtended = ( lParam & ( 1 << 24 ) ) != 0;
	if ( !bIsExtended )
	{
		switch ( result )
		{
		case ::KEY_HOME:
			return ::KEY_PAD_7;
		case ::KEY_UP:
			return ::KEY_PAD_8;
		case ::KEY_PAGEUP:
			return ::KEY_PAD_9;
		case ::KEY_LEFT:
			return ::KEY_PAD_4;
		case ::KEY_RIGHT:
			return ::KEY_PAD_6;
		case ::KEY_END:
			return ::KEY_PAD_1;
		case ::KEY_DOWN:
			return ::KEY_PAD_2;
		case ::KEY_PAGEDOWN:
			return ::KEY_PAD_3;
		case ::KEY_INSERT:
			return ::KEY_PAD_0;
		case ::KEY_DELETE:
			return ::KEY_PAD_DECIMAL;
		default:
			break;
		}
	}
	else
	{
		switch ( result )
		{
		case ::KEY_ENTER:
			return ::KEY_PAD_ENTER;
		case ::KEY_LALT:
			return ::KEY_RALT;
		case ::KEY_LCONTROL:
			return ::KEY_RCONTROL;
		case ::KEY_SLASH:
			return ::KEY_PAD_DIVIDE;
		case ::KEY_CAPSLOCK:
			return ::KEY_PAD_PLUS;
		}
	}

	return result;
}

static bool b_pVirtualKeyToButtonCodeInit = false;
static ButtonCode_t s_pVirtualKeyToButtonCode[ 256 ];

static void ButtonCode_InitKeyTranslationTable()
{
	// set virtual key translation table
	memset( s_pVirtualKeyToButtonCode, ::KEY_NONE, sizeof( s_pVirtualKeyToButtonCode ) );

	s_pVirtualKeyToButtonCode[ '0' ] = ::KEY_0;
	s_pVirtualKeyToButtonCode[ '1' ] = ::KEY_1;
	s_pVirtualKeyToButtonCode[ '2' ] = ::KEY_2;
	s_pVirtualKeyToButtonCode[ '3' ] = ::KEY_3;
	s_pVirtualKeyToButtonCode[ '4' ] = ::KEY_4;
	s_pVirtualKeyToButtonCode[ '5' ] = ::KEY_5;
	s_pVirtualKeyToButtonCode[ '6' ] = ::KEY_6;
	s_pVirtualKeyToButtonCode[ '7' ] = ::KEY_7;
	s_pVirtualKeyToButtonCode[ '8' ] = ::KEY_8;
	s_pVirtualKeyToButtonCode[ '9' ] = ::KEY_9;
	s_pVirtualKeyToButtonCode[ 'A' ] = ::KEY_A;
	s_pVirtualKeyToButtonCode[ 'B' ] = ::KEY_B;
	s_pVirtualKeyToButtonCode[ 'C' ] = ::KEY_C;
	s_pVirtualKeyToButtonCode[ 'D' ] = ::KEY_D;
	s_pVirtualKeyToButtonCode[ 'E' ] = ::KEY_E;
	s_pVirtualKeyToButtonCode[ 'F' ] = ::KEY_F;
	s_pVirtualKeyToButtonCode[ 'G' ] = ::KEY_G;
	s_pVirtualKeyToButtonCode[ 'H' ] = ::KEY_H;
	s_pVirtualKeyToButtonCode[ 'I' ] = ::KEY_I;
	s_pVirtualKeyToButtonCode[ 'J' ] = ::KEY_J;
	s_pVirtualKeyToButtonCode[ 'K' ] = ::KEY_K;
	s_pVirtualKeyToButtonCode[ 'L' ] = ::KEY_L;
	s_pVirtualKeyToButtonCode[ 'M' ] = ::KEY_M;
	s_pVirtualKeyToButtonCode[ 'N' ] = ::KEY_N;
	s_pVirtualKeyToButtonCode[ 'O' ] = ::KEY_O;
	s_pVirtualKeyToButtonCode[ 'P' ] = ::KEY_P;
	s_pVirtualKeyToButtonCode[ 'Q' ] = ::KEY_Q;
	s_pVirtualKeyToButtonCode[ 'R' ] = ::KEY_R;
	s_pVirtualKeyToButtonCode[ 'S' ] = ::KEY_S;
	s_pVirtualKeyToButtonCode[ 'T' ] = ::KEY_T;
	s_pVirtualKeyToButtonCode[ 'U' ] = ::KEY_U;
	s_pVirtualKeyToButtonCode[ 'V' ] = ::KEY_V;
	s_pVirtualKeyToButtonCode[ 'W' ] = ::KEY_W;
	s_pVirtualKeyToButtonCode[ 'X' ] = ::KEY_X;
	s_pVirtualKeyToButtonCode[ 'Y' ] = ::KEY_Y;
	s_pVirtualKeyToButtonCode[ 'Z' ] = ::KEY_Z;

	s_pVirtualKeyToButtonCode[ VK_NUMPAD0 ] = ::KEY_PAD_0;
	s_pVirtualKeyToButtonCode[ VK_NUMPAD1 ] = ::KEY_PAD_1;
	s_pVirtualKeyToButtonCode[ VK_NUMPAD2 ] = ::KEY_PAD_2;
	s_pVirtualKeyToButtonCode[ VK_NUMPAD3 ] = ::KEY_PAD_3;
	s_pVirtualKeyToButtonCode[ VK_NUMPAD4 ] = ::KEY_PAD_4;
	s_pVirtualKeyToButtonCode[ VK_NUMPAD5 ] = ::KEY_PAD_5;
	s_pVirtualKeyToButtonCode[ VK_NUMPAD6 ] = ::KEY_PAD_6;
	s_pVirtualKeyToButtonCode[ VK_NUMPAD7 ] = ::KEY_PAD_7;
	s_pVirtualKeyToButtonCode[ VK_NUMPAD8 ] = ::KEY_PAD_8;
	s_pVirtualKeyToButtonCode[ VK_NUMPAD9 ] = ::KEY_PAD_9;
	s_pVirtualKeyToButtonCode[ VK_DIVIDE ] = ::KEY_PAD_DIVIDE;
	s_pVirtualKeyToButtonCode[ VK_MULTIPLY ] = ::KEY_PAD_MULTIPLY;
	s_pVirtualKeyToButtonCode[ VK_SUBTRACT ] = ::KEY_PAD_MINUS;
	s_pVirtualKeyToButtonCode[ VK_ADD ] = ::KEY_PAD_PLUS;
	s_pVirtualKeyToButtonCode[ VK_RETURN ] = ::KEY_PAD_ENTER;
	s_pVirtualKeyToButtonCode[ VK_DECIMAL ] = ::KEY_PAD_DECIMAL;

	s_pVirtualKeyToButtonCode[ 0xdb ] = ::KEY_LBRACKET;
	s_pVirtualKeyToButtonCode[ 0xdd ] = ::KEY_RBRACKET;
	s_pVirtualKeyToButtonCode[ 0xba ] = ::KEY_SEMICOLON;
	s_pVirtualKeyToButtonCode[ 0xde ] = ::KEY_APOSTROPHE;
	s_pVirtualKeyToButtonCode[ 0xc0 ] = ::KEY_BACKQUOTE;
	s_pVirtualKeyToButtonCode[ 0xbc ] = ::KEY_COMMA;
	s_pVirtualKeyToButtonCode[ 0xbe ] = ::KEY_PERIOD;
	s_pVirtualKeyToButtonCode[ 0xbf ] = ::KEY_SLASH;
	s_pVirtualKeyToButtonCode[ 0xdc ] = ::KEY_BACKSLASH;
	s_pVirtualKeyToButtonCode[ 0xbd ] = ::KEY_MINUS;
	s_pVirtualKeyToButtonCode[ 0xbb ] = ::KEY_EQUAL;

	s_pVirtualKeyToButtonCode[ VK_RETURN ] = ::KEY_ENTER;
	s_pVirtualKeyToButtonCode[ VK_SPACE ] = ::KEY_SPACE;
	s_pVirtualKeyToButtonCode[ VK_BACK ] = ::KEY_BACKSPACE;
	s_pVirtualKeyToButtonCode[ VK_TAB ] = ::KEY_TAB;
	s_pVirtualKeyToButtonCode[ VK_CAPITAL ] = ::KEY_CAPSLOCK;
	s_pVirtualKeyToButtonCode[ VK_NUMLOCK ] = ::KEY_NUMLOCK;
	s_pVirtualKeyToButtonCode[ VK_ESCAPE ] = ::KEY_ESCAPE;
	s_pVirtualKeyToButtonCode[ VK_SCROLL ] = ::KEY_SCROLLLOCK;
	s_pVirtualKeyToButtonCode[ VK_INSERT ] = ::KEY_INSERT;
	s_pVirtualKeyToButtonCode[ VK_DELETE ] = ::KEY_DELETE;
	s_pVirtualKeyToButtonCode[ VK_HOME ] = ::KEY_HOME;
	s_pVirtualKeyToButtonCode[ VK_END ] = ::KEY_END;
	s_pVirtualKeyToButtonCode[ VK_PRIOR ] = ::KEY_PAGEUP;
	s_pVirtualKeyToButtonCode[ VK_NEXT ] = ::KEY_PAGEDOWN;
	s_pVirtualKeyToButtonCode[ VK_PAUSE ] = ::KEY_BREAK;
	s_pVirtualKeyToButtonCode[ VK_SHIFT ] = ::KEY_RSHIFT;
	s_pVirtualKeyToButtonCode[ VK_SHIFT ] = ::KEY_LSHIFT;	// SHIFT -> left SHIFT
	s_pVirtualKeyToButtonCode[ VK_MENU ] = ::KEY_RALT;
	s_pVirtualKeyToButtonCode[ VK_MENU ] = ::KEY_LALT;		// ALT -> left ALT
	s_pVirtualKeyToButtonCode[ VK_CONTROL ] = ::KEY_RCONTROL;
	s_pVirtualKeyToButtonCode[ VK_CONTROL ] = ::KEY_LCONTROL;	// CTRL -> left CTRL
	s_pVirtualKeyToButtonCode[ VK_LWIN ] = ::KEY_LWIN;
	s_pVirtualKeyToButtonCode[ VK_RWIN ] = ::KEY_RWIN;
	s_pVirtualKeyToButtonCode[ VK_APPS ] = ::KEY_APP;
	s_pVirtualKeyToButtonCode[ VK_UP ] = ::KEY_UP;
	s_pVirtualKeyToButtonCode[ VK_LEFT ] = ::KEY_LEFT;
	s_pVirtualKeyToButtonCode[ VK_DOWN ] = ::KEY_DOWN;
	s_pVirtualKeyToButtonCode[ VK_RIGHT ] = ::KEY_RIGHT;
	s_pVirtualKeyToButtonCode[ VK_F1 ] = ::KEY_F1;
	s_pVirtualKeyToButtonCode[ VK_F2 ] = ::KEY_F2;
	s_pVirtualKeyToButtonCode[ VK_F3 ] = ::KEY_F3;
	s_pVirtualKeyToButtonCode[ VK_F4 ] = ::KEY_F4;
	s_pVirtualKeyToButtonCode[ VK_F5 ] = ::KEY_F5;
	s_pVirtualKeyToButtonCode[ VK_F6 ] = ::KEY_F6;
	s_pVirtualKeyToButtonCode[ VK_F7 ] = ::KEY_F7;
	s_pVirtualKeyToButtonCode[ VK_F8 ] = ::KEY_F8;
	s_pVirtualKeyToButtonCode[ VK_F9 ] = ::KEY_F9;
	s_pVirtualKeyToButtonCode[ VK_F10 ] = ::KEY_F10;
	s_pVirtualKeyToButtonCode[ VK_F11 ] = ::KEY_F11;
	s_pVirtualKeyToButtonCode[ VK_F12 ] = ::KEY_F12;
}

static ButtonCode_t ButtonCode_VirtualKeyToButtonCode( int keyCode )
{
	if ( !b_pVirtualKeyToButtonCodeInit )
	{
		ButtonCode_InitKeyTranslationTable();
		b_pVirtualKeyToButtonCodeInit = true;
	}

	if ( keyCode < 0 || keyCode >= sizeof( s_pVirtualKeyToButtonCode ) / sizeof( s_pVirtualKeyToButtonCode[ 0 ] ) )
	{
		Assert( false );
		return ::KEY_NONE;
	}
	return s_pVirtualKeyToButtonCode[ keyCode ];
}

static void SendMousePos( InputEvent_t &event, HWND hWnd, LPARAM lParam )
{
	event.m_nType = IE_LocateMouseClick;
	event.m_nData = (short)LOWORD( lParam );
	event.m_nData2 = (short)HIWORD( lParam );
	Plat_WindowToScreenCoords( (PlatWindow_t)hWnd, event.m_nData, event.m_nData2 );
	PanoramaEngineHandler().ProcessUserInput( event );
}

static void SendButtonUp( InputEvent_t &event, int button )
{
	event.m_nType = IE_ButtonReleased;
	event.m_nData = button;
	event.m_nData2 = 0;
	PanoramaEngineHandler().ProcessUserInput( event );
}

static void SendButtonDown( InputEvent_t &event, int button )
{
	event.m_nType = IE_ButtonPressed;
	event.m_nData = button;
	event.m_nData2 = 0;
	PanoramaEngineHandler().ProcessUserInput( event );
}

static LRESULT CALLBACK DefWindowProc2( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	InputEvent_t event;
	memset( &event, 0, sizeof( event ) );

	static HBITMAP hBitmap = NULL;
	static uint32* pWindowBits = 0;

	event.m_hWnd = (PlatWindow_t)hWnd;

	int nWd = PanoramaEngineHandler().m_nDebuggerW;
	int nHt = PanoramaEngineHandler().m_nDebuggerH;


	switch ( message )
	{
		// 	case WM_CREATE:
		// //		::SetForegroundWindow( hWnd );
		// 		break;

	case WM_PAINT:
		PAINTSTRUCT 	ps;
		HDC 			hdc;
		BITMAP 			bitmap;
		HDC 			hdcMem;
		HGDIOBJ 		oldBitmap;

		BITMAPINFOHEADER bmih;
		bmih.biSize = sizeof( BITMAPINFOHEADER );
		bmih.biWidth = nWd;
		bmih.biHeight = -nHt;
		bmih.biPlanes = 1;
		bmih.biBitCount = 32;
		bmih.biCompression = BI_RGB;
		bmih.biSizeImage = 0;
		bmih.biXPelsPerMeter = 10;
		bmih.biYPelsPerMeter = 10;
		bmih.biClrUsed = 0;
		bmih.biClrImportant = 0;

		BITMAPINFO dbmi;
		ZeroMemory( &dbmi, sizeof( dbmi ) );
		dbmi.bmiHeader = bmih;

		hdc = BeginPaint( hWnd, &ps );

		// Create DIB
		if ( !hBitmap )
		{
			hBitmap = CreateDIBSection( hdc, &dbmi, DIB_RGB_COLORS, (void**)&pWindowBits, NULL, 0 );
		}
		// copy pixels into DIB.
		PanoramaEngineHandler().CopyDebuggerRtBits( pWindowBits, nWd, nHt );

		hdcMem = CreateCompatibleDC( hdc );
		oldBitmap = SelectObject( hdcMem, hBitmap );

		GetObject( hBitmap, sizeof( bitmap ), &bitmap );
		BitBlt( hdc, 0, 0, bitmap.bmWidth, bitmap.bmHeight, hdcMem, 0, 0, SRCCOPY );

		SelectObject( hdcMem, oldBitmap );
		DeleteDC( hdcMem );

		EndPaint( hWnd, &ps );
		break;

	case WM_CLOSE:
		PanoramaEngineHandler().OnCloseDebuggerWindow();
		break;

	case WM_MOUSEMOVE:
		SendMousePos( event, hWnd, lParam );
		break;

	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
		PanoramaEngineHandler().m_pDebugger->ForceEndInspect();
		SendButtonDown( event, ( message == WM_LBUTTONDOWN ) ? ::MOUSE_LEFT : ::MOUSE_RIGHT );
		break;

	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
		SendButtonUp( event, ( message == WM_LBUTTONUP ) ? ::MOUSE_LEFT : ::MOUSE_RIGHT );
		break;

	case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDBLCLK:
		event.m_nType = IE_ButtonDoubleClicked;
		event.m_nData = ( message == WM_LBUTTONDBLCLK ) ? ::MOUSE_LEFT : ::MOUSE_RIGHT;
		event.m_nData2 = 0;
		PanoramaEngineHandler().ProcessUserInput( event );
		break;

	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		// Suppress key repeats
		if ( !( lParam & ( 1 << 30 ) ) )
		{
			// NOTE: These two can be unequal! For example, keypad enter
			// which returns KEY_ENTER from virtual keys, and KEY_PAD_ENTER from scan codes
			// Since things like vgui care about virtual keys; we're going to
			// put both scan codes in the input message
			ButtonCode_t scanCode = ButtonCode_ScanCodeToButtonCode( lParam );
			ButtonCode_t virtualCode = ButtonCode_VirtualKeyToButtonCode( wParam );
			event.m_nType = IE_ButtonPressed;
			event.m_nData = scanCode;
			event.m_nData2 = virtualCode;
			PanoramaEngineHandler().ProcessUserInput( event );
		}
		break;

	case WM_MOUSEWHEEL:
	{
		event.m_nType = IE_AnalogValueChanged;
		event.m_nData = MOUSE_WHEEL;
		event.m_nData3 = (int)GET_WHEEL_DELTA_WPARAM( wParam ) / WHEEL_DELTA;
		PanoramaEngineHandler().ProcessUserInput( event );
	}
	break;

	case WM_CHAR:
		event.m_nType = IE_KeyTyped;
		event.m_nData = wParam;
		PanoramaEngineHandler().ProcessUserInput( event );
		break;

	case WM_MOVE:
		PanoramaEngineHandler().m_nDebuggerX = int16( LOWORD( lParam ) );
		PanoramaEngineHandler().m_nDebuggerY = int16( HIWORD( lParam ) );
		break;

	case WM_SIZE:
	{
		int w = LOWORD( lParam );
		int h = HIWORD( lParam );
		if ( wParam == SIZE_RESTORED && ( w != PanoramaEngineHandler().m_nDebuggerW || h != PanoramaEngineHandler().m_nDebuggerH ) )
		{
			PanoramaEngineHandler().OnDebuggerResize( w, h );
			if ( hBitmap != NULL )
			{
				DeleteObject( hBitmap );
				hBitmap = NULL;
			}
		}
	}
	break;

	case WM_KEYUP:
	case WM_SYSKEYUP:
		// Don't handle key ups if the key's already up. This can happen when we alt-tab back to the engine.
		//ButtonCode_t virtualCode = ButtonCode_VirtualKeyToButtonCode( wParam );
		ButtonCode_t scanCode = ButtonCode_ScanCodeToButtonCode( lParam );
		ButtonCode_t virtualCode = ButtonCode_VirtualKeyToButtonCode( wParam );
		event.m_nType = IE_ButtonReleased;
		event.m_nData = scanCode;
		event.m_nData2 = virtualCode;
		PanoramaEngineHandler().ProcessUserInput( event );
		break;
	}

	return DefWindowProc( hWnd, message, wParam, lParam );
}

static PlatWindow_t Plat_CreateWindow2( void *hInstance, const char *pTitle, int nWidth, int nHeight, int nFlags )
{
	WNDCLASSEX		wc;
	memset( &wc, 0, sizeof( wc ) );
	wc.cbSize = sizeof( wc );
	wc.style = CS_OWNDC | CS_DBLCLKS;
	wc.lpfnWndProc = DefWindowProc2;
	wc.hInstance = (HINSTANCE)hInstance;
	wc.lpszClassName = "ValvePanDebugger";
	wc.hIcon = NULL; //LoadIcon( s_HInstance, MAKEINTRESOURCE( IDI_LAUNCHER ) );
	wc.hIconSm = wc.hIcon;

	RegisterClassEx( &wc );

	DWORD style = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_SIZEBOX;

	RECT windowRect;
	windowRect.top = 0;
	windowRect.left = 0;
	windowRect.right = nWidth;
	windowRect.bottom = nHeight;

	// Compute rect needed for that size client area based on window style
	AdjustWindowRectEx( &windowRect, style, FALSE, 0 );

	// Create the window
	void *hWnd = CreateWindow( wc.lpszClassName, pTitle, style, 0, 0,
							   windowRect.right - windowRect.left, windowRect.bottom - windowRect.top,
							   NULL, NULL, (HINSTANCE)hInstance, NULL );

	return (PlatWindow_t)hWnd;
}


PlatWindow_t CPanoramaEngineHandler::CreateAppWindow( const char *pTitle, int nPlatWindowFlags, int x, int y, int w, int h )
{
	PlatWindow_t hWnd = Plat_CreateWindow2( NULL, pTitle, w, h, nPlatWindowFlags );
	if ( hWnd == PLAT_WINDOW_INVALID )
		return PLAT_WINDOW_INVALID;

	Plat_SetWindowPos( hWnd, x, y );

	return hWnd;
}

#endif // ( PLATFORM_WINDOWS && DEVELOPMENT_ONLY ) && !defined (DX_TO_GL_ABSTRACTION)


