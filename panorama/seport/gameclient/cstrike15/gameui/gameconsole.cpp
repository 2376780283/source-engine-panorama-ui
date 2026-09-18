//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: SE port - CS:GO's game/client/cstrike15/gameui/gameconsole.cpp.  See the long note in
//          gameconsole.h for why the console now lives in this module.
//
//=============================================================================//

#include <stdio.h>

#include "gameconsole.h"
#include "gameconsoledialog.h"
#include "vgui/ISurface.h"
#include "vgui/IScheme.h"

#include "keyvalues.h"
#include "vgui/vgui.h"
#include "vgui/IVGui.h"
#include "vgui_controls/Panel.h"
#include "convar.h"
#include "icvar.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static CGameConsole g_GameConsole;
//-----------------------------------------------------------------------------
// Purpose: singleton accessor
//-----------------------------------------------------------------------------
CGameConsole &GameConsole()
{
	return g_GameConsole;
}

//-----------------------------------------------------------------------------
// SE port (bring-up probe): see the note in gameconsole.h.
//-----------------------------------------------------------------------------
void SE_PortConsoleProbe( const char *pFmt, ... )
{
	FILE *fp = fopen( "D:\\cstrike\\se_console_probe.txt", "a" );
	if ( !fp )
		return;

	va_list args;
	va_start( args, pFmt );
	vfprintf( fp, pFmt, args );
	va_end( args );
	fflush( fp );
	fclose( fp );
}

// SE port: CS:GO has "EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CGameConsole, IGameConsole,
// GAMECONSOLE_INTERFACE_VERSION, g_GameConsole)" right here - it works there because the module's
// CreateInterface factory answers several interface versions at once (client.dll).  This DLL exports a
// single-interface CreateInterface for IPanoramaUIClient (panoramauiclient.cpp), so a second
// EXPOSE_SINGLE_INTERFACE here would collide.  The engine gets this object through the
// SE_PortGetGameConsole() export instead - same direction, different plumbing; see
// panoramauiclient/se_gameconsole.cpp and engine/panoramaenginehandler.cpp.

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CGameConsole::CGameConsole()
{
	m_bInitialized = false;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CGameConsole::~CGameConsole()
{
	m_bInitialized = false;
}

//-----------------------------------------------------------------------------
// Purpose: sets up the console for use
//-----------------------------------------------------------------------------
void CGameConsole::Initialize()
{
	m_pConsole = vgui::SETUP_PANEL( new CGameConsoleDialog() ); // we add text before displaying this so set it up now!

	// set the console to taking up most of the right-half of the screen
	int swide, stall;
	vgui::surface()->GetScreenSize(swide, stall);
	int offset = vgui::scheme()->GetProportionalScaledValue(16);

	m_pConsole->SetBounds(
		swide / 2 - (offset * 4),
		offset,
		(swide / 2) + (offset * 3),
		stall - (offset * 8));

	m_pConsole->InvalidateLayout( false, true );

	{
		int bx, by, bw, bh;
		m_pConsole->GetBounds( bx, by, bw, bh );
		SE_PortConsoleProbe( "Initialize: screen=%dx%d offset=%d bounds=(%d,%d,%d,%d) panel=%p scheme=%p embedded=%d\n",
			swide, stall, offset, bx, by, bw, bh, (void *)m_pConsole, (void *)vgui::scheme(), (int)vgui::surface()->GetEmbeddedPanel() );
	}

	// SE port (bring-up probe, TEMPORARY - remove with the probes): force a loud background so the next
	// screenshot can tell "painted but colourless" (bright block appears) from "not painted at all"
	// (screen stays black).
	m_pConsole->SetPaintBackgroundEnabled( true );
	m_pConsole->SetBgColor( Color( 200, 30, 30, 255 ) );

	m_bInitialized = true;
}

//-----------------------------------------------------------------------------
// Purpose: activates the console, makes it visible and brings it to the foreground
//-----------------------------------------------------------------------------
void CGameConsole::Activate()
{
	{
		int bx, by, bw, bh;
		if ( m_pConsole )
			m_pConsole->GetBounds( bx, by, bw, bh );
		else
			bx = by = bw = bh = -1;

		SE_PortConsoleProbe( "Activate: init=%d visible=%d bounds=(%d,%d,%d,%d) parent=%d embedded=%d\n",
			(int)m_bInitialized, m_pConsole ? (int)m_pConsole->IsVisible() : -1, bx, by, bw, bh,
			m_pConsole ? (int)m_pConsole->GetParent() : -1,
			(int)vgui::surface()->GetEmbeddedPanel() );
	}

	if (!m_bInitialized)
		return;

	vgui::surface()->RestrictPaintToSinglePanel(NULL);
	m_pConsole->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: hides the console
//-----------------------------------------------------------------------------
void CGameConsole::Hide()
{
	if (!m_bInitialized)
		return;

	m_pConsole->Hide();
}

//-----------------------------------------------------------------------------
// Purpose: skips animation and forces the immediate hiding of the panel
//-----------------------------------------------------------------------------
void CGameConsole::HideImmediately ( void )
{
	if ( !m_bInitialized )
		return;

	m_pConsole->SetVisible( false );
}


//-----------------------------------------------------------------------------
// Purpose: clears the console
//-----------------------------------------------------------------------------
void CGameConsole::Clear()
{
	if (!m_bInitialized)
		return;

	m_pConsole->Clear();
}


//-----------------------------------------------------------------------------
// Purpose: returns true if the console is currently in focus
//-----------------------------------------------------------------------------
bool CGameConsole::IsConsoleVisible()
{
	static bool s_bProbed = false;
	if ( !s_bProbed && m_pConsole )
	{
		s_bProbed = true;
		int bx, by, bw, bh;
		m_pConsole->GetBounds( bx, by, bw, bh );
		SE_PortConsoleProbe( "IsConsoleVisible(1st): visible=%d bounds=(%d,%d,%d,%d) parent=%d\n",
			(int)m_pConsole->IsVisible(), bx, by, bw, bh, (int)m_pConsole->GetParent() );
	}

	if (!m_bInitialized)
		return false;
	
	return m_pConsole->IsVisible();
}

//-----------------------------------------------------------------------------
// Purpose: activates the console after a delay
//-----------------------------------------------------------------------------
void CGameConsole::ActivateDelayed(float time)
{
	if (!m_bInitialized)
		return;

	m_pConsole->PostMessage(m_pConsole, new KeyValues("Activate"), time);
}

void CGameConsole::SetParent( intp parent )
{	
	if (!m_bInitialized)
		return;

	m_pConsole->SetParent( static_cast<vgui::VPANEL>( parent ));
}

void CGameConsole::Shutdown( void )
{
	if ( m_pConsole && m_bInitialized)
	{
		HideImmediately();
		m_pConsole->MarkForDeletion();
	}
}

//-----------------------------------------------------------------------------
// Purpose: static command handler
//-----------------------------------------------------------------------------
void CGameConsole::OnCmdCondump()
{
	if ( !g_GameConsole.m_bInitialized || !g_GameConsole.m_pConsole )
		return;

	g_GameConsole.m_pConsole->DumpConsoleTextToFile();
}

//-----------------------------------------------------------------------------
// SE port: CS:GO's "CON_COMMAND( condump, ... )" from the bottom of gameconsole.cpp.  Declared as a
// plain object because this module never calls ConVar_Register() (see the note in gameconsole.h); the
// bridge registers it explicitly, right after taking the name away from the CS:S gameui.dll console.
//-----------------------------------------------------------------------------
static ConCommand g_SEConsoleCondump( "condump", CGameConsole::OnCmdCondump, "dump the text currently in the console to condumpXX.log", FCVAR_DONTRECORD );

void SE_PortRegisterConsoleCommands()
{
	if ( !g_pCVar )
		return;

	// ICvar refuses to register a second command with an existing name ("unable to link %s and %s
	// because one or more is a ConCommand"), so the gameui.dll one has to be unregistered first -
	// otherwise the engine keeps the gameui handler, whose console is never initialized in this
	// configuration.  UnregisterConCommand() clears the object's registered flag, so the owning
	// module's later (destructor) unregister call is a harmless no-op.
	if ( ConCommand *pExisting = g_pCVar->FindCommand( "condump" ) )
	{
		if ( pExisting != &g_SEConsoleCondump )
		{
			g_pCVar->UnregisterConCommand( pExisting );
			Msg( "SE port: condump moved from the gameui.dll console to the panorama one\n" );
		}
	}

	g_pCVar->RegisterConCommand( &g_SEConsoleCondump );
}
