//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: SE port - CS:GO's game/client/cstrike15/gameui/gameconsoledialog.cpp.
//
//          Differences from CS:GO's file, all noted inline:
//            * the port has no CGameUI yet, so the "is the menu the current level" test CS:GO gets from
//              GameUI().IsInBackgroundLevel() is inlined here (same two engine calls that method makes)
//            * GetBindingForButtonCode is reached through the port's gameuifuncs global with a NULL guard
//            * CEG_* (CS:GO's exclusive-code guard macros) are dropped
//
//=============================================================================//

#include "gameconsoledialog.h"
#include "gameconsole.h"

// SE port: CS:GO's file includes gameui_interface.h for GameUI() (CGameUI) and engineinterface.h for
// the engine globals.  The port's shared game-client header provides the same globals (engine,
// gameuifuncs); see panorama/seport/gameclient/panorama/se_gameclient_globals.h.
#include "panorama/se_gameclient_globals.h"
#include "IGameUIFuncs.h"

#include "vgui/IInput.h"
#include "vgui/ISurface.h"
#include "vgui/KeyCode.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CGameConsoleDialog::CGameConsoleDialog() : BaseClass( NULL, "GameConsole", false )
{
	AddActionSignalTarget( this );
}


//-----------------------------------------------------------------------------
// Purpose: generic vgui command handler
//-----------------------------------------------------------------------------
void CGameConsoleDialog::OnCommand(const char *command)
{
	if ( !Q_stricmp( command, "Close" ) )
	{
		// SE port: CS:GO calls GameUI().IsInBackgroundLevel() here.  That method is
		// game/client/cstrike15/gameui/gameui_interface.cpp::CGameUI::IsInBackgroundLevel(); the port
		// has no CGameUI yet ("task B"), so the same test is inlined - it is exactly the body of that
		// method.
		const char *levelName = engine ? engine->GetLevelName() : NULL;
		if ( levelName && levelName[0] && engine->IsLevelMainMenuBackground() )
		{
			// Tell the engine we've hid the console, so that it unpauses the game
			// even though we're still sitting at the menu.
			engine->ClientCmd_Unrestricted( "unpause" );
		}
	}

	BaseClass::OnCommand(command);
}


//-----------------------------------------------------------------------------
// HACK: Allow F key bindings to operate even when typing in the text entry field
//-----------------------------------------------------------------------------
void CGameConsoleDialog::OnKeyCodeTyped(KeyCode code)
{
	BaseClass::OnKeyCodeTyped(code);
	
	// check for processing
	if ( m_pConsolePanel->TextEntryHasFocus() )
	{
		// HACK: Allow F key bindings to operate even here
		if ( code >= KEY_F1 && code <= KEY_F12 )
		{
			// See if there is a binding for the FKey
			const char *binding = gameuifuncs ? gameuifuncs->GetBindingForButtonCode( code ) : NULL;
			if ( binding && binding[0] )
			{
				// submit the entry as a console command
				char szCommand[256];
				Q_strncpy( szCommand, binding, sizeof( szCommand ) );
				engine->ClientCmd_Unrestricted( szCommand );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Submits a command
//-----------------------------------------------------------------------------
void CGameConsoleDialog::OnCommandSubmitted( const char *pCommand )
{
	engine->ClientCmd_Unrestricted( pCommand );
}


//-----------------------------------------------------------------------------
// Submits a command
//-----------------------------------------------------------------------------
void CGameConsoleDialog::OnClosedByHittingTilde()
{
	// CS:GO's version (the port's old CS:S file called GameUI().HideGameUI() here, which manipulated
	// the CS:S VGUI gameui panel - the very layer the panorama host hides while it owns the screen).
	GameConsole().HideImmediately();
}
