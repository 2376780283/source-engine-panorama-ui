//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: SE port of the CS:GO main-menu background movie setup - see se_background_movie.h.
//
//=============================================================================//

// This is the module-side translation unit: mark the entry point for export (the engine includes the
// header without this define and resolves the symbol at run time).
#define SE_BACKGROUND_MOVIE_BUILD_DLL 1
#include "seport/se_background_movie.h"

#include "stdafx_client.h"
#include "panorama/controls/panel2d.h"
#include "panorama/uiengine.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

// The engine does not link any panorama library on purpose (see engine/wscript: engine.dll grabs
// IPanoramaUIClient/IPanoramaUIEngine from panoramauiclient.dll's factory instead), so this entry
// point is exported from the module and the engine resolves it with GetProcAddress.
extern "C" SE_BACKGROUND_MOVIE_EXPORT bool SE_PortLoadMainMenuBackgroundMovie( panorama::IUIPanel *pMenuRoot, const char *pchMovieName )
{
	if ( !pMenuRoot || !pchMovieName || !pchMovieName[0] )
	{
		Msg( "SE port: background movie skipped (no menu root or movie name)\n" );
		return false;
	}

	// Step 1: CS:GO's LoadBackgroundMovie() - the layout leaves this parent empty, the snippet has
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

	IUIPanel *pMoviePanel = pMovieParent->FindChildInLayoutFile( "MainMenuMovie" );
	if ( !pMoviePanel )
	{
		Msg( "SE port: snippet did not create the '#MainMenuMovie' panel\n" );
		return false;
	}

	// Step 2: the same three calls mainmenu.js::_SetBackgroundMovie() makes.  That function already
	// ran once while the layout was loading (the panel did not exist yet, so it returned early at its
	// IsValid() check), hence doing the calls here.  They go through the movie panel's JS methods
	// (SetMovie/SetSound/Play), which avoids an IUIPanel* -> CPanel2D* cast - the two are separate
	// hierarchies in this tree.
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
