//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: SE port - CS:GO's game/client/cstrike15/gameui/gameconsoledialog.{h,cpp}: the console
//          dialog itself (a vgui_controls::CConsoleDialog, i.e. the stock vgui2 console window).
//          See gameconsole.h for why it lives in the panorama module now.
//
//=============================================================================//

#ifndef SE_GAMECONSOLEDIALOG_H
#define SE_GAMECONSOLEDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/consoledialog.h"
#include <color.h>
#include "utlvector.h"
#include "vgui_controls/Frame.h"


//-----------------------------------------------------------------------------
// Purpose: Game/dev console dialog
//-----------------------------------------------------------------------------
class CGameConsoleDialog : public vgui::CConsoleDialog
{
	DECLARE_CLASS_SIMPLE( CGameConsoleDialog, vgui::CConsoleDialog );

public:
	CGameConsoleDialog();

private:
	MESSAGE_FUNC( OnClosedByHittingTilde, "ClosedByHittingTilde" );
	MESSAGE_FUNC_CHARPTR( OnCommandSubmitted, "CommandSubmitted", command );

	virtual void OnKeyCodeTyped( vgui::KeyCode code );
	virtual void OnCommand( const char *command );
};


#endif // SE_GAMECONSOLEDIALOG_H
