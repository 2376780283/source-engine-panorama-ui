//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose:
//=============================================================================//
#pragma once

// SE port: CS:GO's game/client/cstrike15/panorama/csgo_videosettingsscreen.h.  The include set is
// this port's: CS:GO's "panorama/csgo_panorama.h" does not exist here (see
// panorama/seport/gameclient/panorama/se_gameclient_common.h), and "GameEventListener.h" /
// "gameui_interface.h" are leftovers in CS:GO's header - the class derives from CPanel2D only.
// vmode_t comes from Source 2013's public/modes.h (the member names CS:GO's copy uses are the
// same), and KeyValues only as a forward declaration.

#include "panorama/controls/panel2d.h"
#include "panorama/popups/csgo_settings_slider.h"
#include "tier1/utlvector.h"
#include "modes.h"

class KeyValues;

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
class CCSGO_VideoSettingsScreen : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CCSGO_VideoSettingsScreen, panorama::CPanel2D );

public:
	CCSGO_VideoSettingsScreen( panorama::CPanel2D *pParent, const char *pchID );
	~CCSGO_VideoSettingsScreen();

	void DisplayModeChangeCallback( void );

private:
	bool EventPanelInit( void );	// User switched to video settings tab

	void SetControlsToCurrent();
	void SetResolutionBoxesToSelected();
	void SetResolutionBoxesToCurrent();
	void SetResolutionBoxesFromConfig( KeyValues *pDefaultConfigKeys );
	void SetResolutionBoxes( int nAspectRatioIndex, int nDispModeIndex, int nWidth, int nHeight );
	void GenerateWindowedModes( CUtlVector< vmode_t > &windowedModes, int nCount, vmode_t *pFullscreenModes );
	void PopulateResolutionDropDownAndSelectClosest( int nAspectRatioIndex, int nDispMode, int nWidth, int nHeight );
	void PopulateAAModes();
	void PopulateSettingsWithAutoOptions();
	
	void SetAAModeFromConfig( KeyValues *pDefaultConfigKeys );
	void SetAAModeFromConvars();
	void SetConvarsFromAAMode();
	
	void SetVSyncFromConfig( KeyValues *pDefaultConfigKeys );
	void SetVSyncFromConvars();
	void SetConvarsFromVSync();
	
	void SetQueueModeFromConfig( KeyValues *pConfig );
	void SetQueueModeFromConvar();

	// SetAdvancedOptionsFromConvars - those in the "advanced options" settings 
	void SetAdvancedOptionsFromConvars();

	// PrepareResolutionList: fills in usable res list, and returns index of entry closest 
	// to the given width and height
	int PrepareResolutionList( int nAspectRatioIndex, int nDispMode, int nWidth, int nHeight );
	
	int GetAspectRatioIndex( int width, int height );

	void SelectResolution( int nWidth, int nHeight );

	void GetSelectedResolution( int& nWidth, int& nHeight );
	int GetSelectedDisplayMode();

	// Enable/Disable widgets depending on options selected
	void EnableAspectAndResolutionControls( int nDisplayMode );
	void UpdateFullScreenOnlyOptions();

	CCSGO_SettingsEnumDropDown *m_pColorModeDropDown = nullptr;
	CCSGO_SettingsEnumDropDown *m_pAspectRatioDropDown = nullptr;
	CCSGO_SettingsEnumDropDown *m_pResolutionDropDown = nullptr;
	CCSGO_SettingsEnumDropDown *m_pDisplayModeDropDown = nullptr;
	CCSGO_SettingsEnumDropDown *m_pPowerSavingsDropDown = nullptr;
	CCSGO_SettingsEnumDropDown *m_pCSMQualityLevelDropDown = nullptr;
	CCSGO_SettingsEnumDropDown *m_pModelTextureDetailDropDown = nullptr;
	CCSGO_SettingsEnumDropDown *m_pEffectDetailDropDown = nullptr;
	CCSGO_SettingsEnumDropDown *m_pShaderDetailDropDown = nullptr;
	CCSGO_SettingsEnumDropDown *m_pFilteringModeDropDown = nullptr;
	CCSGO_SettingsEnumDropDown *m_pAAModeDropDown = nullptr;
	CCSGO_SettingsEnumDropDown *m_pFXAADropDown = nullptr;
	CCSGO_SettingsEnumDropDown *m_pVSyncDropDown = nullptr;
	CCSGO_SettingsEnumDropDown *m_pMatQueueModeDropDown = nullptr;
	CCSGO_SettingsEnumDropDown *m_pMotionBlurDropDown = nullptr;
	CSGO_SettingsSlider *m_pBrightness = nullptr;

	CUtlVector<vmode_t> m_aUsableResList;

	// Callbacks for aspect ratio and display mode change. Cause regenerations of usable resolutions
	bool EventAspectRatioSelectionChanged( void );
	bool EventDisplayModeSelectionChanged( void );
	bool EventResolutionSelectionChanged( void );
	bool EventApplyVideoSettings( void );
	bool EventVideoSettingsResetDefaults( void );

	bool m_bResolutionChanged;	// Resolution, display mode or aspect ratio changed
};
