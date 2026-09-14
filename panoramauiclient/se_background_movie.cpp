//========= Copyright Valve Corporation, All rights reserved. ============//
//
// SE port (2026-09-14): CS:GO creates its main-menu background movie from the game-client panel
// class (game/client/cstrike15/panorama/csgo_mainmenu.cpp::CCSGO_MainMenu::LoadBackgroundMovie):
//
//     m_pMainMenuMovieParent->RequireLoadLayoutSnippet( "MainMenuMovieSnippet" );
//     m_pMainMenuMovie = panel_cast< CMoviePlayer * >( m_pMainMenuMovieParent->FindChildInLayoutFile( "MainMenuMovie" ) );
//     if ( m_pMainMenuMovie ) m_pMainMenuMovie->Play();
//
// mainmenu.xml only *declares* the snippet and leaves MainMenuMovieParent empty, so without that
// game-client code the '#MainMenuMovie' panel never exists and mainmenu.js::_SetBackgroundMovie()
// bails out on its IsValid() check - no webm, no background.
//
// This tree has no cstrike15 client, so the same two steps are provided here and exported for the
// engine (which hosts the menu but deliberately links no panorama library - see engine/wscript, the
// engine grabs IPanoramaUIClient/IPanoramaUIEngine out of this module's factory - so it resolves the
// symbol below with GetProcAddress instead of linking it).
//
// NOTE: the implementation lives in the panoramauiclient project on purpose.  An object sitting in a
// static library (panorama.lib) is only pulled into a link when something references it, so a
// dllexport there produced no export at all ("unresolved external symbol
// SE_PortLoadMainMenuBackgroundMovie" from panoramauiclient.exp).
//
//=============================================================================//

#define SE_BACKGROUND_MOVIE_BUILD_DLL 1
#include "seport/se_background_movie.h"

#include "stdafx_client.h"
// Same prerequisite headers movieplayer.cpp pulls in before its own header (the control header uses
// CButton/CLabel/CPtr on a popup class and the ParsedPanelProperty_t vector type).
#include "panorama/controls/button.h"
#include "panorama/controls/label.h"
#include "panorama/controls/vumeter.h"
#include "panorama/controls/slider.h"
#include "panorama/layout/csshelpers.h"
#include "panorama/renderer/styleproperties.h"
#include "panorama/uijsregistration.h"
#include "panorama/iuisoundsystem.h"
#include "panorama/controls/panel2d.h"
#include "panorama/uiengine.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

extern "C" SE_BACKGROUND_MOVIE_EXPORT bool SE_PortLoadMainMenuBackgroundMovie( panorama::IUIPanel *pMenuRoot, const char *pchMovieName )
{
	if ( !pMenuRoot || !pchMovieName || !pchMovieName[0] )
	{
		Msg( "SE port: background movie skipped (no menu root or movie name)\n" );
		return false;
	}

	// Step 1: CS:GO's LoadBackgroundMovie() - mainmenu.xml leaves this parent empty, the snippet has
	// the actual <Movie id="MainMenuMovie"> panel.
	IUIPanel *pMovieParent = pMenuRoot->FindChildInLayoutFile( "MainMenuMovieParent" );
	if ( !pMovieParent )
	{
		Msg( "SE port: background movie parent panel 'MainMenuMovieParent' not found\n" );
		return false;
	}

	if ( !pMovieParent->BLoadLayoutSnippet( "MainMenuMovieSnippet" ) )
	{
		Msg( "SE port: unable to instantiate layout snippet 'MainMenuMovieSnippet'\n" );
		return false;
	}

	if ( !pMovieParent->FindChildInLayoutFile( "MainMenuMovie" ) )
	{
		Msg( "SE port: snippet did not create the '#MainMenuMovie' panel\n" );
		return false;
	}

	// Step 2: the three calls mainmenu.js::_SetBackgroundMovie() makes.  That function already ran
	// while the layout was loading (the panel did not exist yet, so it returned early), hence doing
	// them here.  They go through the movie panel's own JS methods (SetMovie/SetSound/Play), which
	// avoids an IUIPanel* -> CPanel2D* cast - the two are separate hierarchies in this tree.
	char pchScript[1024];
	V_snprintf( pchScript, sizeof( pchScript ),
		"( function () {"
		" var p = $( '#MainMenuMovie' );"
		" if ( !p || !p.IsValid() ) { return; }"
		" p.SetMovie( 'file://{resources}/videos/%s.webm' );"
		" try { p.SetSound( 'UIPanorama.BG_%s' ); } catch ( e ) { }"
		" p.Play();"
		" } )();",
		pchMovieName, pchMovieName );

	UIEngine()->RunScript( pMenuRoot, pchScript, "SEPortBackgroundMovie", 0, 0, false, false );

	Msg( "SE port: background movie '%s' handed to #MainMenuMovie\n", pchMovieName );
	return true;
}
