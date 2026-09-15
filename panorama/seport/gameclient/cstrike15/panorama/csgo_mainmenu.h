//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: SE port of game/client/cstrike15/panorama/csgo_mainmenu.h - the CS:GO main menu panel
//          (which is also the pause menu).
//
//          This class is the reason the menu's popups/tooltips/context menus work: it derives from
//          CUI_Root, and CUI_Root registers itself per window (ui_root.cpp: s_vecWindowRootMap), which
//          is how CUI_PopupManager::GetPanelPopupManager()/UiToolkitAPI find the managers.
//
//          Two things in CS:GO's header come from the game client's gameui module
//          (game/client/cstrike15/gameui/gameui_interface.h): the CSGOGameUIState_t enum and
//          ICSGOGameUIStateListener (plus GameUI() to drive them).  This tree has no such module, so
//          the enum members and the listener signature are repeated verbatim below; the state itself
//          is derived from the engine in csgo_mainmenu.cpp::Update().
//
//=============================================================================//

#pragma once

#include "panorama/ui_root.h"

class CCSGO_MainMenu;
namespace panorama
{
	class CMoviePlayer;
}

DECLARE_PANORAMA_EVENT0( CSGOSettings );
DECLARE_PANORAMA_EVENT0(CSGOMainMenuUpdate);
DECLARE_PANORAMA_EVENT0( PanoramaMouseEnable );

//-----------------------------------------------------------------------------
// SE port: csgo_mainmenu.h includes gameui_interface.h for these two.  Values and member names are
// copied from game/client/cstrike15/gameui/gameui_interface.h:42-80 so that the body of
// OnCSGOGameUIStateChange() stays CS:GO's.  Only the two states the main menu actually reacts to are
// ever produced here (see csgo_mainmenu.cpp::Update()).
//-----------------------------------------------------------------------------
enum CSGOGameUIState_t
{
	CSGO_GAME_UI_STATE_INVALID = 0,
	CSGO_GAME_UI_STATE_LOADINGSCREEN,
	CSGO_GAME_UI_STATE_INGAME,
	CSGO_GAME_UI_STATE_MAINMENU,
	CSGO_GAME_UI_STATE_PAUSEMENU,
	CSGO_GAME_UI_STATE_INTROMOVIE,
	CSGO_GAME_UI_STATE_COUNT,
};

class SE_PortGameUIStateListener
{
public:
	virtual void OnCSGOGameUIStateChange( CSGOGameUIState_t nOldState, CSGOGameUIState_t nNewState ) {}
};

//-----------------------------------------------------------------------------
// Purpose: CSGO Main Menu
//                      Note that main menu is used as the pause menu as well
//-----------------------------------------------------------------------------
class CCSGO_MainMenu : public CUI_Root, public SE_PortGameUIStateListener
{
	DECLARE_PANEL2D( CCSGO_MainMenu, CUI_Root );

public:
	CCSGO_MainMenu(panorama::CPanel2D *pParent, const char *pchID);
	virtual ~CCSGO_MainMenu();

	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

	virtual void OnCSGOGameUIStateChange( CSGOGameUIState_t nOldState, CSGOGameUIState_t nNewState ) OVERRIDE;

	void Update( void );

	static CCSGO_MainMenu *GetInstance() { return s_pMainMenu; }

	// CGameEventListener
	virtual void FireGameEvent( IGameEvent *event ) OVERRIDE;

private:

	// Methods exposed to javascript
	bool IsMultiplayer();
	bool IsTraining();
	bool IsGotvSpectating();

	bool EventQuitClicked( void );
	bool EventOnQuitConfirmed( void );
	bool EventSetPopupBackgroundBlur( bool bEnable );
	bool EventResumeGame( void );
	bool EventDisconnect( void );
	bool EventDisconnectConfirmed( void );
	bool EventSwitchTeams( void );

	void LoadBackgroundMovie();
	void UnloadBackgroundMovie();
	void LoadVanityPanel();
	void UnloadVanityPanel();

private:

	panorama::CPanel2D *m_pSteamNotificationsPlaceholderPanel;

	static CCSGO_MainMenu *s_pMainMenu;

	bool m_bIsInGame;
	bool m_bInitialDisplay;

	panorama::CPanel2D* m_pMainMenuMovieParent;
	panorama::CMoviePlayer* m_pMainMenuMovie;
	panorama::CPanel2D* m_pVanityPanelParent;
	panorama::CPanel2D* m_pVanityPanel;
	panorama::CPanel2D *m_pInputPanel;      // Panel having focus
	panorama::CPanelPtr< panorama::CPanel2D > m_pDisconnectPopup;

	uint64 m_hDenyInputToGame;

	// SE port: last state handed to OnCSGOGameUIStateChange(), see Update().
	CSGOGameUIState_t m_nSEPortLastGameUIState;
};
