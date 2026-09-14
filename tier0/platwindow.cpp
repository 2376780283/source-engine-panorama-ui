//========= Copyright Valve Corporation, All rights reserved. ============//
//
// SE port: the window-coordinate helpers declared in public/tier0/platwindow.h.
//
// public/tier0/platwindow.h was brought over from CS:GO during the panorama phases because panorama's
// Source 2 top level window uses PlatWindow_t, but Source Engine 2013's tier0 never provided the
// implementation, so panorama/source2/uitoplevelwindowsource2.cpp left
// Plat_ScreenToWindowCoords() unresolved at link time.
//
// Only the two coordinate conversions are implemented for now - they are the only plat window entry
// points the panorama modules call.  The rest of the header (Plat_CreateWindow, Plat_SetWindowTitle,
// Plat_GetDesktopResolution, ...) is still unimplemented; M4 has to port CS:GO's tier0/platwindow.cpp
// when the engine integration starts creating/positioning windows through it.
//
// ============//
#include "tier0/platform.h"
#include "tier0/platwindow.h"

#if defined( PLATFORM_WINDOWS_PC ) || defined( _WIN32 )

#include <windows.h>	// POINT / HWND / ClientToScreen / ScreenToClient

//-----------------------------------------------------------------------------
// Convert window -> screen coordinates
//-----------------------------------------------------------------------------
void Plat_WindowToScreenCoords( PlatWindow_t hWnd, int &x, int &y )
{
	POINT pt;
	pt.x = x; pt.y = y;
	ClientToScreen( (HWND)hWnd, &pt );
	x = pt.x; y = pt.y;
}

void Plat_ScreenToWindowCoords( PlatWindow_t hWnd, int &x, int &y )
{
	POINT pt;
	pt.x = x; pt.y = y;
	ScreenToClient( (HWND)hWnd, &pt );
	x = pt.x; y = pt.y;
}

#else

// POSIX builds have no window manager backing yet - treat window and screen space as identical.
void Plat_WindowToScreenCoords( PlatWindow_t hWnd, int &x, int &y )
{
	NOTE_UNUSED( hWnd ); NOTE_UNUSED( x ); NOTE_UNUSED( y );
}

void Plat_ScreenToWindowCoords( PlatWindow_t hWnd, int &x, int &y )
{
	NOTE_UNUSED( hWnd ); NOTE_UNUSED( x ); NOTE_UNUSED( y );
}

#endif
