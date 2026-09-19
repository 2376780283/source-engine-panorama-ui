//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: SE port (2026-09-18) - the game/dev console host, from CS:GO's
//          game/client/cstrike15/gameui/gameconsole.{h,cpp}.
//
//          WHY THIS FILE IS HERE ("moving the console into the panorama module", user request):
//
//          CS:GO's console is VGUI2 (vgui_controls::CConsoleDialog / CConsolePanel), but it is *owned by
//          the client-side GameUI module* (game/client/cstrike15/gameui/) - the same module that hosts the
//          panorama UI (CGameUI there).  The engine never creates it itself: CEngineVGui::Init() asks
//          m_GameUIFactory for GAMECONSOLE_INTERFACE_VERSION, and by default m_GameUIFactory *is* the
//          client DLL's factory (g_ClientFactory), so the console comes out of the module that also runs
//          panorama.  Only "-gameuidll" makes the engine load the separate gameui.dll and take the
//          console from there.
//
//          This fork still ships the CS:S gameui.dll, and that is where the console used to come from -
//          which is why the panorama layer needed engine-side workarounds (reparenting the console panel
//          to the engine root while the VGUI menu is hidden, see vgui_baseui_interface.cpp).  The port's
//          equivalent of "the client-side GameUI module" is panoramauiclient.dll - the module that hosts
//          the panorama UI - so the console lives here now, exactly as in CS:GO:
//
//              engine/vgui_baseui_interface.cpp:  IGameConsole <- panoramauiclient.dll (this class)
//                                                 ...falling back to gameui.dll when this DLL has none
//
//          The panel itself is the stock vgui2 console (we already have CS:GO's CConsoleDialog /
//          CConsolePanel in vgui2/vgui_controls/consoledialog.cpp - the port's copy matches CS:GO's
//          except for two CS:GO-only ConVars this engine does not have).
//
//=============================================================================//

#ifndef SE_GAMECONSOLE_H
#define SE_GAMECONSOLE_H
#ifdef _WIN32
#pragma once
#endif

#include "GameUI/IGameConsole.h"

class CGameConsoleDialog;

//-----------------------------------------------------------------------------
// Purpose: VGui implementation of the game/dev console
//-----------------------------------------------------------------------------
class CGameConsole : public IGameConsole
{
public:
	CGameConsole();
	~CGameConsole();

	// sets up the console for use
	void Initialize();

	// activates the console, makes it visible and brings it to the foreground
	virtual void Activate();
	// hides the console
	virtual void Hide();
	// clears the console
	virtual void Clear();

	void HideImmediately( void );

	// returns true if the console is currently in focus
	virtual bool IsConsoleVisible();

	// activates the console after a delay
	void ActivateDelayed(float time);

	virtual void SetParent( intp parent );

	// hides and deletes panel
	void Shutdown( void );

	static void OnCmdCondump();
private:

	bool m_bInitialized;
	CGameConsoleDialog *m_pConsole;
};

extern CGameConsole &GameConsole();

//-----------------------------------------------------------------------------
// SE port (bring-up probe): the console path is hard to follow from outside (this module's Msg/Warning
// are easy to lose and the interesting steps happen before any vgui console exists), so the ported files
// record what they did to a file - same pattern as the other SE probes.  Defined in gameconsole.cpp,
// used by gameconsole.cpp and panoramauiclient/se_gameconsole.cpp.
//-----------------------------------------------------------------------------
void SE_PortConsoleProbe( const char *pFmt, ... );

//-----------------------------------------------------------------------------
// SE port: CS:GO registers its condump from this file (a module-level CON_COMMAND).  This module never
// calls ConVar_Register(), so a ConCommand declared here would never reach ICvar - the bridge
// (panoramauiclient/se_gameconsole.cpp) calls this instead, which also takes condump away from the CS:S
// gameui.dll console (that instance is no longer initialized once this DLL provides IGameConsole, so its
// condump handler would have nothing to dump).
//-----------------------------------------------------------------------------
void SE_PortRegisterConsoleCommands();

#endif // SE_GAMECONSOLE_H
