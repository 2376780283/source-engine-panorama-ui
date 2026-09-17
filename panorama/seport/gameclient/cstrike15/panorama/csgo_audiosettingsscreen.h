//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose:
//=============================================================================//
#pragma once

// SE port: CS:GO's game/client/cstrike15/panorama/csgo_audiosettingsscreen.h.  The include set is
// this port's: CS:GO's "panorama/csgo_panorama.h" does not exist here (see
// panorama/seport/gameclient/panorama/se_gameclient_common.h), and "GameEventListener.h" /
// "gameui_interface.h" are leftovers in CS:GO's header - this class derives from CPanel2D only and
// uses neither.

#include "panorama/popups/csgo_settings_enum.h"

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
class CCSGO_AudioSettingsScreen : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CCSGO_AudioSettingsScreen, panorama::CPanel2D );

public:
	CCSGO_AudioSettingsScreen ( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_AudioSettingsScreen();

private:
	bool EventPanelLoaded( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel );
	bool EventSpeakerConfigurationChanged( void );
	bool EventAudioDeviceConfigurationChanged(void);
	bool EventVoiceSelectionChanged( void );
	bool EventAudioSettingsResetDefault( void );

	void SetSpeakerConfigurationToCurrent( void );
	void SetVoiceConfigurationToCurrent( void );
	void SetAudioDevices( void );

	void UpdateEnhanceStereo( void );

	CCSGO_SettingsEnumDropDown *m_pSpeakerCfgDropDown;
	CCSGO_SettingsEnumDropDown *m_pVoiceEnableDropDown;
	CCSGO_SettingsEnumDropDown *m_pDeviceCfgDropDown;

};
