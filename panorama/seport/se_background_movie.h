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
// bails out on its IsValid() check.  This tree has no cstrike15 client, so the same three steps are
// offered here as a framework entry point that the engine can call.
//
//=============================================================================//
#ifndef SE_BACKGROUND_MOVIE_H
#define SE_BACKGROUND_MOVIE_H
#pragma once

namespace panorama
{
	class IUIPanel;
}

// panorama modules are all built with PANORAMA_EXPORTS (see panorama/wscript); the engine includes
// this header without it, so the declaration carries no dllimport and the engine resolves the
// symbol out of panoramauiclient.dll with GetProcAddress instead of linking.  Only the module-side
// translation unit defines SE_BACKGROUND_MOVIE_BUILD_DLL, so it is the one that gets dllexport.
#if defined( SE_BACKGROUND_MOVIE_BUILD_DLL ) && defined( _WIN32 )
#define SE_BACKGROUND_MOVIE_EXPORT __declspec( dllexport )
#else
#define SE_BACKGROUND_MOVIE_EXPORT
#endif

// Instantiates the main-menu background movie panel under the given menu root and starts playback.
// pchMovieName is the bare name used by mainmenu.js ( ui_mainmenu_bkgnd_movie, e.g. "anubis720" ).
// Returns true when the panel was found and playback was started.
// extern "C": the engine resolves this out of panoramauiclient.dll with GetProcAddress, and C++
// linkage would export the name mangled ("?SE_PortLoadMainMenuBackgroundMovie@@YA_N...").
extern "C" SE_BACKGROUND_MOVIE_EXPORT bool SE_PortLoadMainMenuBackgroundMovie( panorama::IUIPanel *pMenuRoot, const char *pchMovieName );

#endif // SE_BACKGROUND_MOVIE_H
