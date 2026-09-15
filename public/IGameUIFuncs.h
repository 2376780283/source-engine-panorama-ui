//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef IGAMEUIFUNCS_H
#define IGAMEUIFUNCS_H
#ifdef _WIN32
#pragma once
#endif

#include "tier0/platform.h"      // DEFINE_ENUM_BITWISE_OPERATORS (tier0/basetypes.h)
#include "vgui/KeyCode.h"
// InputContextHandle_t / INPUT_CONTEXT_HANDLE_INVALID come from iinputstacksystem.h, which is also
// what CS:GO's IGameUIFuncs.h includes (this is the file the cursor/input-context plumbing lives in).
#include "inputsystem/iinputstacksystem.h"

//-----------------------------------------------------------------------------
// SE port (CS:GO addition): the panorama input capture flags.  CS:GO declares them here; the engine
// (CPanoramaEngineHandler) and the client UI both use them.
//-----------------------------------------------------------------------------
namespace panorama
{
	class IUIPanel;

	enum EGameInputFlags {
		k_EGameInputFlagsNone = 0,

		k_EGameInputUIEnableMouseCursor = 0x1,
		k_EGameInputUIEnableControllerInput = 0x4,
		k_EGameInputUIEnableKeyInput = 0x8,

		k_EGameInputDenyGameMouseMovement = 0x10,
		k_EGameInputDenyGameMouseClicks = 0x20,
		k_EGameInputDenyGameControllerInput = 0x40,
		k_EGameInputDenyGameKeys = 0x80,

		// UI consumes all user input
		k_EGameInputCaptureAll
			= k_EGameInputUIEnableMouseCursor
			| k_EGameInputUIEnableControllerInput
			| k_EGameInputUIEnableKeyInput
			| k_EGameInputDenyGameMouseMovement
			| k_EGameInputDenyGameMouseClicks
			| k_EGameInputDenyGameControllerInput
			| k_EGameInputDenyGameKeys,

		// UI has mouse control, but un-handled clicks
		// and other input are passed through to the game
		k_EGameInputShareMouse
			= k_EGameInputUIEnableMouseCursor
			| k_EGameInputDenyGameMouseMovement,

		// UI has mouse control, other input is passed through to the game
		// This means mouse bindings will not reach the game, you probably never want this for an in-game panel
		k_EGameInputCaptureMouse
			= k_EGameInputShareMouse
			| k_EGameInputDenyGameMouseClicks,

		// UI consumes all keyboard events
		// but mouse control remains in-game (e.g. text chat)
		k_EGameInputCaptureKeyboard
			= k_EGameInputUIEnableKeyInput
			| k_EGameInputDenyGameKeys,
	};

	DEFINE_ENUM_BITWISE_OPERATORS( EGameInputFlags );
}

abstract_class IGameUIFuncs
{
public:
	virtual bool		IsKeyDown( const char *keyname, bool& isdown ) = 0;
	virtual const char	*GetBindingForButtonCode( ButtonCode_t code ) = 0;
	virtual ButtonCode_t GetButtonCodeForBind( const char *pBind ) = 0;
	virtual void		GetVideoModes( struct vmode_s **liststart, int *count ) = 0;
	virtual void		SetFriendsID( uint friendsID, const char *friendsName ) = 0;
	virtual void		GetDesktopResolution( int &width, int &height ) = 0;
	virtual bool		IsConnectedToVACSecureServer() = 0;

	//-----------------------------------------------------------------------------
	// SE port (CS:GO additions): panorama hosting entry points.  CS:GO declares these as pure virtual
	// and implements them in CGameUIFuncs (engine/sys_dll2.cpp), forwarding to the panorama engine
	// handler.  They are defaulted here so the existing Source Engine implementations of
	// IGameUIFuncs keep compiling; the engine overrides them during the M4 integration.
	//-----------------------------------------------------------------------------
	virtual void *AddPanoramaView( const char *pchViewName, void *pWindow ) { return NULL; }
	virtual void RemovePanoramaView( void *pWindow ) {}
	virtual void PanoramaRunFrame( int nSlot ) {}
	virtual void PanoramaRenderFrame( int nSlot ) {}
	virtual InputContextHandle_t GetPanoramaInputContext() { return INPUT_CONTEXT_HANDLE_INVALID; }
	virtual uint64 PanoramaAddDenyAllInputToGame( panorama::IUIPanel *pPanel, const char *pchDebugContextName ) { return 0; }
	virtual void PanoramaReleaseDenyAllInputToGame( uint64 handle ) {}
	virtual bool PanoramaDeniesInputToGame() { return false; }
	virtual uint64 PanoramaAddGameInputHandler( panorama::IUIPanel *pPanel, panorama::EGameInputFlags inputFlags, const char *pchDebugContextName ) { return 0; }
	virtual void PanoramaReleaseGameInputHandler( uint64 handle ) {}
	virtual bool PanoramaDeniesInputToGame( panorama::EGameInputFlags eFlags ) { return false; }
	virtual uint64 PanoramaAddDenyMouseInputToGame( panorama::IUIPanel *pPanel, const char *pchDebugContextName ) { return 0; }
	virtual void PanoramaReleaseDenyMouseInputToGame( uint64 handle ) {}
	virtual bool IsPanoramaInECOMode() { return false; }
};

#define VENGINE_GAMEUIFUNCS_VERSION "VENGINE_GAMEUIFUNCS_VERSION005"

#endif // IGAMEUIFUNCS_H
