//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: SE port of game/client/cstrike15/panorama/csgo_mainmenu.cpp.
//
//          Everything that talks to panorama is CS:GO's code, unchanged.  What this tree cannot give
//          it is the CS:GO *game client* (cs_gamerules / c_cs_playerresource / clientmode_csnormal /
//          csgo_teamselectmenu / uicomponent_settings) and the CS:GO gameui module (GameUI() +
//          ICSGOGameUIStateListener).  Those parts are behind SE_PORT_HAVE_CSGO_GAMECLIENT and have
//          port fallbacks, each with a "SE port" note at the call site.
//
//          The UI state transitions are important here: CS:GO's CGameUI publishes them, and this class
//          reacts by loading/unloading the background movie and vanity panel, showing/hiding the whole
//          window and taking/releasing the "deny game input" lock.  With no gameui module, the state is
//          derived from engine->IsInGame() in Update() (which the engine calls once per frame, see
//          engine/panoramaenginehandler.cpp::PanoramaRunFrame).
//
//=============================================================================//

#include "panorama/se_gameclient_common.h"
#include "csgo_mainmenu.h"

#include "csgo_popup_manager.h"
#include "panorama/ui_context_menu_manager.h"
#include "csgo_ui_tooltip_manager.h"
#include "panorama/controls/movieplayer.h"
#include "panorama/popups/ui_popup_generic.h"

#include "IGameUIFuncs.h"

// SE port: only needed by the CS:GO game-client code paths (see SE_PORT_HAVE_CSGO_GAMECLIENT).
#if SE_PORT_HAVE_CSGO_GAMECLIENT
#include "csgo_teamselectmenu.h"
#include "cs_gamerules.h"
#include "clientmode_csnormal.h"
#include "c_cs_playerresource.h"
#include "uicomponents/uicomponent_settings.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

//-----------------------------------------------------------------------------
// SE port: 0 = the CS:GO game client that the guarded code below needs is not in this tree (this is
// the CS:S client).  Flip to 1 only together with porting those modules.
//-----------------------------------------------------------------------------
#define SE_PORT_HAVE_CSGO_GAMECLIENT 0

//-----------------------------------------------------------------------------
// SE port: GameUI().HideGameUI() (CS:GO's CGameUI).  This tree's engine hides the VGUI gameui panel
// itself (engine/panoramaenginehandler.cpp, on by default while a panorama menu exists), and the
// engine's `gameui_hide` command is exactly what CEngineVGui::HideGameUI() does when a game is
// running - which is the only case where this is reached (the pause menu).
//-----------------------------------------------------------------------------
static void SE_PortHideGameUI()
{
	if ( engine )
	{
		engine->ClientCmd_Unrestricted( "gameui_hide" );
	}
}

REGISTER_PANEL2D_FACTORY( CCSGO_MainMenu, CSGOMainMenu )

//
// Panorama event declarations
//

DECLARE_PANORAMA_EVENT0( CSGOShowMainMenu );
DECLARE_PANORAMA_EVENT0( CSGOHideMainMenu );
DECLARE_PANORAMA_EVENT0( CSGOQuit );
DECLARE_PANORAMA_EVENT0( CSGOQuitConfirmed );

// Pause menu mode
DECLARE_PANORAMA_EVENT0( CSGOShowPauseMenu );
DECLARE_PANORAMA_EVENT0( CSGOHidePauseMenu );
DECLARE_PANORAMA_EVENT0( CSGOMainMenuResumeGame );
DECLARE_PANORAMA_EVENT0( CSGOMainMenuDisconnect );
DECLARE_PANORAMA_EVENT0( CSGOMainMenuDisconnectConfirmed );
DECLARE_PANORAMA_EVENT0( CSGOMainMenuSwitchTeams );


//
// Panorama event definitions
//

DEFINE_PANORAMA_EVENT( CSGOSettings );
DEFINE_PANORAMA_EVENT( CSGOShowMainMenu );
DEFINE_PANORAMA_EVENT( CSGOHideMainMenu );
DEFINE_PANORAMA_EVENT( CSGOQuit );
DEFINE_PANORAMA_EVENT( CSGOQuitConfirmed );
DEFINE_PANORAMA_EVENT( CSGOMainMenuUpdate );

DEFINE_PANORAMA_EVENT( CSGOShowPauseMenu );
DEFINE_PANORAMA_EVENT( CSGOHidePauseMenu );
DEFINE_PANORAMA_EVENT( CSGOMainMenuResumeGame );
DEFINE_PANORAMA_EVENT( CSGOMainMenuDisconnect );
DEFINE_PANORAMA_EVENT( CSGOMainMenuDisconnectConfirmed );
DEFINE_PANORAMA_EVENT( CSGOMainMenuSwitchTeams );


using namespace panorama;

//-----------------------------------------------------------------------------
// Static data members
//-----------------------------------------------------------------------------
/*static*/ CCSGO_MainMenu *CCSGO_MainMenu::s_pMainMenu = NULL;

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CCSGO_MainMenu::CCSGO_MainMenu(CPanel2D *pParent, const char *pchID)
:
	CUI_Root( pParent, pchID ),
	m_hDenyInputToGame( 0 )
{
	Assert(s_pMainMenu == NULL);
	s_pMainMenu = this;

	RequireLoadLayout( "file://{resources}/layout/mainmenu.xml" );


	// Tell the root about these controls
	SetPopupManager( panorama::panel_cast<CCSGO_PopupManager *>( RequireChildInLayoutFile( "PopupManager" ), true ) );
	SetTooltipManager( panorama::panel_cast<CCSGO_UI_TooltipManager *>( RequireChildInLayoutFile( "TooltipManager" ), true ) );
	SetContextMenuManager( panorama::panel_cast<CUI_ContextMenuManager *>( RequireChildInLayoutFile( "ContextMenuManager" ), true ) );

	/*
	CPanel2D* pBrowser = RequireChildInLayoutFile("CSGOBlogPanel");
	if ( pBrowser )
	{
		pBrowser->SetAcceptsInput(true);
		pBrowser->SetAcceptsFocus(true);
	}
	*/

	m_bIsInGame = false;

	m_pSteamNotificationsPlaceholderPanel = RequireChildInLayoutFile( "SteamNotificationsPlaceholder" );
	m_pMainMenuMovieParent = RequireChildInLayoutFile( "MainMenuMovieParent" );
	m_pMainMenuMovie = nullptr;
	m_pVanityPanelParent = RequireChildInLayoutFile( "MainMenuVanityParent" );
	m_pVanityPanel = nullptr;
	m_pInputPanel = RequireChildInLayoutFile( "MainMenuInput" );

	m_pInputPanel->SetAcceptsInput( true );
	m_pInputPanel->SetAcceptsFocus( true );
	m_pInputPanel->SetInputNamespace( "CSGO_mainmenu" );

	// set up events
	RegisterEventHandler(CSGOQuit(), this, &CCSGO_MainMenu::EventQuitClicked);
	RegisterEventHandler(CSGOQuitConfirmed(), this, &CCSGO_MainMenu::EventOnQuitConfirmed);

	RegisterEventHandler( SetPopupBackgroundBlur(), this, &CCSGO_MainMenu::EventSetPopupBackgroundBlur );

	RegisterEventHandler( CSGOMainMenuResumeGame(), this, &CCSGO_MainMenu::EventResumeGame );
	RegisterEventHandler( CSGOMainMenuDisconnect(), this, &CCSGO_MainMenu::EventDisconnect );
	RegisterEventHandler( CSGOMainMenuDisconnectConfirmed(), this, &CCSGO_MainMenu::EventDisconnectConfirmed );
	RegisterEventHandler( CSGOMainMenuSwitchTeams(), this, &CCSGO_MainMenu::EventSwitchTeams );

	// SE port: CS:GO does
	//     GameUI().RegisterGameUIStateListener( this );
	//     OnCSGOGameUIStateChange( CSGO_GAME_UI_STATE_INVALID, GameUI().GetGameUIState() );
	// here.  This tree has no CS:GO gameui module, so the state transitions are driven by Update()
	// (called once per frame from engine/panoramaenginehandler.cpp::PanoramaRunFrame).  The initial
	// transition is the one CS:GO's CGameUI publishes at startup, so the menu starts up visible with
	// its movie/vanity panel loaded and the "deny game input" lock taken, exactly as there.
	m_nSEPortLastGameUIState = CSGO_GAME_UI_STATE_INVALID;
	OnCSGOGameUIStateChange( CSGO_GAME_UI_STATE_INVALID, CSGO_GAME_UI_STATE_MAINMENU );

	// SE port: CS:GO sets the game client global g_flReadyToCheckForPCBootInvite here (deferred
	// commands such as item preview / match download); this tree has no CS:GO game client.

	m_bInitialDisplay = true;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCSGO_MainMenu::~CCSGO_MainMenu()
{
	if ( m_hDenyInputToGame )
	{
		gameuifuncs->PanoramaReleaseDenyAllInputToGame( m_hDenyInputToGame );
		m_hDenyInputToGame = 0;
	}

	// SE port: CS:GO calls GameUI().UnregisterGameUIStateListener( this ) here.

	Assert(s_pMainMenu == this);
	s_pMainMenu = NULL;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCSGO_MainMenu::Update(void)
{
	// SE port: the CS:GO gameui module's state machine, reduced to the two states this class reacts to.
	// CS:GO's CGameUI publishes CSGO_GAME_UI_STATE_MAINMENU while the client sits at the main menu and
	// CSGO_GAME_UI_STATE_PAUSEMENU while a game is running (the main menu doubles as the pause menu).
	{
		CSGOGameUIState_t nNewState = ( engine && engine->IsInGame() ) ? CSGO_GAME_UI_STATE_PAUSEMENU : CSGO_GAME_UI_STATE_MAINMENU;
		if ( nNewState != m_nSEPortLastGameUIState )
		{
			CSGOGameUIState_t nOldState = m_nSEPortLastGameUIState;
			m_nSEPortLastGameUIState = nNewState;
			OnCSGOGameUIStateChange( nOldState, nNewState );
		}
	}

	IUIWindow *pWindow = GetParentWindow();
	if ( pWindow && pWindow->BIsVisible() )
	{
		// SE port: CS:GO passes NULL; with two DispatchEvent overloads (const IUIPanel* and
		// const IUIPanelClient*) that is ambiguous for MSVC 14.4, so the target type is spelled out.
		DispatchEvent(CSGOMainMenuUpdate(), ( const panorama::IUIPanel * )NULL);

		if ( m_bInitialDisplay )
		{
			RemoveClass( "InitialDisplay" );
			m_bInitialDisplay = false;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCSGO_MainMenu::OnCSGOGameUIStateChange( CSGOGameUIState_t nOldState, CSGOGameUIState_t nNewState )
{
	bool bWasVisible = GetParentWindow()->BIsVisible();
	DevMsg( "CCSGO_MainMenu - bWasVisible=%s\n", ( bWasVisible ? "true" : "false" ) );
	bool bVisible = false;

	// Hide main menu panel if previous state was CSGO_GAME_UI_STATE_MAINMENU or CSGO_GAME_UI_STATE_PAUSEMENU

	if ( nOldState == CSGO_GAME_UI_STATE_MAINMENU )
	{
		bVisible = false;
		DispatchEvent( CSGOHideMainMenu(), ( const panorama::IUIPanel * )NULL );

		UnloadVanityPanel();
		UnloadBackgroundMovie();
	}
	else if ( nOldState == CSGO_GAME_UI_STATE_PAUSEMENU )
	{
		bVisible = false;
		DispatchEvent( CSGOHidePauseMenu(), ( const panorama::IUIPanel * )NULL );
	}

	// Show main menu panel is the new state is CSGO_GAME_UI_STATE_MAINMENU or CSGO_GAME_UI_STATE_PAUSEMENU

	if ( nNewState == CSGO_GAME_UI_STATE_MAINMENU )
	{
		bVisible = true;
		LoadBackgroundMovie();
		LoadVanityPanel();

		// SE port: CS:GO calls UTIL_UpdateKeyBindings() here (a game-client helper that pushes the
		// user's binds into the panorama keybinding registry).  The port registers CS:GO's keybinding
		// file instead - see SE_PortInstallUiComponentBindings().

		DispatchEvent( CSGOShowMainMenu(), ( const panorama::IUIPanel * )NULL );
	}
	else if ( nNewState == CSGO_GAME_UI_STATE_PAUSEMENU )
	{
		bVisible = true;
		DispatchEvent( CSGOShowPauseMenu(), ( const panorama::IUIPanel * )NULL );
	}

	if ( bVisible != bWasVisible )
	{
		if ( bVisible )
		{
			AssertMsgAlways( !m_hDenyInputToGame, "MainMenu - Make sure to call PanoramaReleaseDenyAllInputToGame before calling PanoramaAddDenyAllInputToGame again\n" );
			m_hDenyInputToGame = gameuifuncs->PanoramaAddDenyAllInputToGame( this->UIPanel(), "MainMenu" );
			m_pInputPanel->SetFocus();
		}
		else
		{
			// HACK - Calling SetInputFocusContext( NULL ) to clear hover data.
			// Artificially reseting mouse position to (0, 0) in order to avoid using 
			// last mouse position when the main menu top level window becomes 
			// visible again 
			// (JIRA CSGO-1325 tooltips persisting across pause menu dismissal)
			GetParentWindow()->UIWindowInput()->SetInputFocusContext( NULL );
			GetParentWindow()->UIWindowInput()->OnMouseMove( 0.0f, 0.0f );
			
			GetParentWindow()->UIWindowInput()->SetInputFocus( NULL, false, false );
			if ( m_hDenyInputToGame )
			{
				gameuifuncs->PanoramaReleaseDenyAllInputToGame( m_hDenyInputToGame );
				m_hDenyInputToGame = 0;
			}
		}

		GetParentWindow()->SetVisible( bVisible );
	}
}


//-----------------------------------------------------------------------------
// Purpose: "CSGOQuitConfirmed" event handler
//-----------------------------------------------------------------------------
bool CCSGO_MainMenu::EventOnQuitConfirmed(void)
{
	// do not explicitly abandon the lobby ( if we were connected to one ), the user may be restarting the client.

#ifdef CSGO_PORT
	g_pHostStateMgr->RequestHS_Quit();
#else
	engine->ClientCmd_Unrestricted("quit");
#endif

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: "CSGOQuit" event handler
//-----------------------------------------------------------------------------
bool CCSGO_MainMenu::EventQuitClicked(void)
{
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: "SetPopupBackgroundBlur" event handler
//-----------------------------------------------------------------------------
bool CCSGO_MainMenu::EventSetPopupBackgroundBlur( bool bEnable )
{
	SetHasClass( "PopupBackgroundBlur", bEnable );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: "CSGOMainMenuResumeGame" event handler
//-----------------------------------------------------------------------------
bool CCSGO_MainMenu::EventResumeGame()
{
	// SE port: CS:GO calls GameUI().HideGameUI() here.
	SE_PortHideGameUI();
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: "CSGOMainMenuDisconnect" event handler
//-----------------------------------------------------------------------------
bool CCSGO_MainMenu::EventDisconnect()
{
	// Open Disconnection confirmation dialog box

	CUI_PopupManager *pPopupManager = GetPopupManager();
#if SE_PORT_HAVE_CSGO_GAMECLIENT
	bool bGameIsOver = ( !CSGameRules() || ( CSGameRules()->GetGamePhase() == GAMEPHASE_MATCH_ENDED ) );
#else
	// SE port: CS:GO picks the confirmation text from the game rules / player resource / quest state
	// (CSGameRules, ClientModeCSNormal, C_CSPlayer, C_CS_PlayerResource).  None of that exists in this
	// tree, so the default "are you sure you want to leave this match?" text is used whenever a game
	// is running - which is the only case this dialog is shown in.
	bool bGameIsOver = ( !engine || !engine->IsInGame() );
#endif
	if ( pPopupManager && !bGameIsOver )
	{
		char const *szTitle = "#SFUI_PauseMenu_ExitGameConfirmation_Title";
		char const *szMessageDefault = "#SFUI_PauseMenu_ExitGameConfirmation_Message";
		char const *szMessage = szMessageDefault;
		if ( engine->IsHLTV() || engine->IsPlayingDemo() )
		{
			szTitle = "#SFUI_PauseMenu_ExitGameConfirmation_TitleWatch";
			szMessage = "#SFUI_PauseMenu_ExitGameConfirmation_MessageWatch";

#if SE_PORT_HAVE_CSGO_GAMECLIENT
			if ( engine->GetDemoPlaybackParameters() && engine->GetDemoPlaybackParameters()->m_bAnonymousPlayerIdentity )
			{
				szTitle = "#SFUI_PauseMenu_ExitGameConfirmation_TitleOverwatch";
				szMessage = "#SFUI_PauseMenu_ExitGameConfirmation_MessageOverwatch";
			}
#endif
			// SE port: IVEngineClient in this tree has no GetDemoPlaybackParameters(), so the Overwatch
			// wording cannot be selected (it needs the demo playback parameters).
		}
#if SE_PORT_HAVE_CSGO_GAMECLIENT
		else if ( CSGameRules() && CSGameRules()->IsQueuedMatchmaking() )
		{
			szTitle = "#SFUI_PauseMenu_ExitGameConfirmation_TitleQueuedMatchmaking";
			szMessage = "#SFUI_PauseMenu_ExitGameConfirmation_MessageQueuedMatchmaking";

			if ( CSGameRules()->IsPlayingCooperativeGametype() )
			{
				szTitle = "#SFUI_PauseMenu_ExitGameConfirmation_TitleQueuedGuardian";
				szMessage = "#SFUI_PauseMenu_ExitGameConfirmation_MessageQueuedGuardian";
			}
		}

		if ( ( szMessage == szMessageDefault ) && CSGameRules() && CSGameRules()->IsQuestEligible()
			&& ( CSGameRules()->GetGamePhase() != GAMEPHASE_MATCH_ENDED )
			&& !CSGameRules()->IsWarmupPeriod() )
		{
			// See if we have a mission progress?
			bool bMissionProgress = false;
			for ( uint32 i = ClientModeCSNormal::sm_mapQuestProgressUncommitted.FirstInorder();
				i != ClientModeCSNormal::sm_mapQuestProgressUncommitted.InvalidIndex();
				i = ClientModeCSNormal::sm_mapQuestProgressUncommitted.NextInorder( i ) )
			{
				if ( ClientModeCSNormal::sm_mapQuestProgressUncommitted.Element( i ).m_numNormalPoints > 0 )
				{
					bMissionProgress = true;
					break;
				}
			}

			if ( bMissionProgress )
			{
				szMessage = "#SFUI_PauseMenu_ExitGameConfirmation_MessageMission";
			}
			else
			{
				// Check if local user has non-zero score?
				C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
				C_CS_PlayerResource *cs_PR = static_cast<C_CS_PlayerResource *>( g_PR );
				if ( pLocalPlayer && cs_PR )
				{
					if ( cs_PR->GetScore( pLocalPlayer->entindex() ) > 0 )
					{
						szMessage = "#SFUI_PauseMenu_ExitGameConfirmation_MessageXP";
					}
				}
			}
		}
#endif

		CUI_Popup_Generic *pPopup = new CUI_Popup_Generic( pPopupManager, nullptr, this );
		pPopup->SetDisplayYesNo( szTitle, szMessage, "CSGOMainMenuDisconnectConfirmed()", nullptr );
		pPopupManager->ShowPopup( pPopup );

		// Keep track of the open popup so that we can close it if we are getting
		// the "cs_game_disconnected" game event
		m_pDisconnectPopup = pPopup;
	}
	else
	{
		EventDisconnectConfirmed();
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: "CSGOMainMenuDisconnectConfirmed" event handler
//-----------------------------------------------------------------------------
bool CCSGO_MainMenu::EventDisconnectConfirmed()
{
	m_pDisconnectPopup = nullptr;

// 	extern ConVar gotv_theater_container;	// Disconnecting from a game stops GOTV theater
// 	gotv_theater_container.SetValue( "" );

	engine->ClientCmd_Unrestricted( "disconnect" );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: "CSGOMainMenuSwitchTeams" event handler
//-----------------------------------------------------------------------------
bool CCSGO_MainMenu::EventSwitchTeams()
{
#if SE_PORT_HAVE_CSGO_GAMECLIENT
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( pPlayer && pPlayer->CanShowTeamMenu() )
	{

		// delay team select until after we close pause menu so that the DenyInputToGame calls don't overlap
		panorama::DispatchEventAsync( 0.1f, CSGOShowTeamSelectMenu(), ( const panorama::IUIPanelClient* )nullptr, true );

		GameUI().HideGameUI();
	}
#else
	// SE port: CS:GO opens its team select menu here (C_CSPlayer::CanShowTeamMenu + the
	// CSGOShowTeamSelectMenu event that CCSGO_TeamSelectMenu listens for).  This tree has no such panel
	// yet, so switching teams from the pause menu closes the menu the same way "resume" does.
	SE_PortHideGameUI();
#endif

	return true;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCSGO_MainMenu::FireGameEvent(IGameEvent *event)
{
	BaseClass::FireGameEvent( event );

	if ( !V_stricmp( event->GetName(), "cs_game_disconnected" ) )
	{
		CUI_Popup *pDisconnectPopup = panorama::panel_cast<CUI_Popup*>( m_pDisconnectPopup.Get() );
		CUI_PopupManager *pPopupManager = GetPopupManager();
		if ( pDisconnectPopup && pPopupManager )
		{
			pPopupManager->CloseIfVisible( pDisconnectPopup, true );
			m_pDisconnectPopup = nullptr;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Expose JS members
//-----------------------------------------------------------------------------
void CCSGO_MainMenu::SetupJavascriptObjectTemplate()
{
	BaseClass::SetupJavascriptObjectTemplate();

	RegisterJSMethod( "IsMultiplayer", PANORAMA_DELEGATE( &CCSGO_MainMenu::IsMultiplayer ) );
	RegisterJSMethod( "IsTraining", PANORAMA_DELEGATE( &CCSGO_MainMenu::IsTraining ) );
	RegisterJSMethod( "IsGotvSpectating", PANORAMA_DELEGATE( &CCSGO_MainMenu::IsGotvSpectating ) );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CCSGO_MainMenu::IsMultiplayer()
{
	bool bMultiplayer = engine->IsInGame() && !engine->IsHLTV();
#if SE_PORT_HAVE_CSGO_GAMECLIENT
	if ( bMultiplayer &&
		engine->IsClientLocalToActiveServer() && ( !g_pMatchFramework ||
			!g_pMatchFramework->GetMatchSession() ||
			V_stricmp( g_pMatchFramework->GetMatchSession()->GetSessionSettings()->GetString( "system/network" ), "LIVE" ) )
		)
	{
		bMultiplayer = false;
	}
#endif
	// SE port: CS:GO also consults the match framework (g_pMatchFramework) here; this tree has none
	// (g_pMatchFramework does not exist), so a running game is reported as multiplayer.

	return bMultiplayer;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CCSGO_MainMenu::IsTraining()
{
	// SE port: CS:GO asks its game rules here (CSGameRules()->IsPlayingTraining()); this tree has no
	// CS:GO game rules, so the menu is never in "training" mode.
	return false;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CCSGO_MainMenu::IsGotvSpectating()
{
	bool bQ = ( engine->IsHLTV() || engine->IsPlayingDemo() );

	return bQ;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCSGO_MainMenu::LoadBackgroundMovie()
{
	m_pMainMenuMovieParent->RequireLoadLayoutSnippet( "MainMenuMovieSnippet" );
	m_pMainMenuMovie = panorama::panel_cast<CMoviePlayer *>(m_pMainMenuMovieParent->FindChildInLayoutFile( "MainMenuMovie" ));
	if ( m_pMainMenuMovie )
	{
		m_pMainMenuMovie->Play();
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCSGO_MainMenu::UnloadBackgroundMovie()
{
	if ( m_pMainMenuMovie )
	{
		m_pMainMenuMovie->Stop();

		// RemoveAndDeleteChildren below used to be DeleteAsync, but that breaks if state toggles from 
		// game->main, main->game, game->main all in the same frame (which can happen if client becomes 
		// unresponsive on team select)
		m_pMainMenuMovieParent->RemoveAndDeleteChildren();	
		m_pMainMenuMovie = nullptr;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCSGO_MainMenu::LoadVanityPanel()
{
	m_pVanityPanelParent->RequireLoadLayoutSnippet( "MainMenuVanitySnippet" );
	m_pVanityPanel = m_pVanityPanelParent->FindChildInLayoutFile( "JsMainmenu_Vanity" );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCSGO_MainMenu::UnloadVanityPanel()
{
	if ( m_pVanityPanel )
	{
		// RemoveAndDeleteChildren below used to be DeleteAsync, see comment in UnloadBackgroundMovie
		m_pVanityPanelParent->RemoveAndDeleteChildren();
		m_pVanityPanel = nullptr;

	}
}
