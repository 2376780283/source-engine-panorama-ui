//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose:
//=============================================================================//
#pragma once

// SE port: CS:GO's game/client/cstrike15/panorama/popups/csgo_settings_enum.h, ported verbatim.
// Registers the two panel types the settings pages are built from:
//
//   CSGOSettingsEnum         - a row of CRadioButtons, each carrying a "value" attribute
//   CSGOSettingsEnumDropDown - the same, as a CDropDown
//
// Every one of them is bound to a ConVar by the layout's "convar" property, and the settings pages
// (layout/settings/settings_*.xml) are almost entirely made of them - without the two registered
// types the layout loader substituted a plain Panel, which is why the settings pages came out empty
// and why scripts aborted on the first call: settingsmenu_gamesettings.js:8 runs
// "clanTagDropdown.RemoveAllOptions()" on a #ClanTagsEnum panel, and a substituted plain Panel has
// no such method (a Panorama TypeError drops the rest of the calling script).

#include "panorama/controls/panelptr.h"
#include "panorama/panoramasymbol.h"
#include "panorama/controls/dropdown.h"

#include "csgo_iconvar_panorama_setter.h"

namespace panorama
{
	class CLabel;
	class CPanel2D;
};

class CCSGO_SettingsEnum : public panorama::CPanel2D, public CCSGO_iConvarPanoramaSetter
{
	DECLARE_PANEL2D( CCSGO_SettingsEnum, panorama::CPanel2D );

public:
	CCSGO_SettingsEnum( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_SettingsEnum( );
	virtual bool BSetProperty( panorama::CPanoramaSymbol symName, const char *pchValue ) OVERRIDE;
	virtual void OnShow();
	virtual ConVarRef& GetConVarRef( ) { return m_ConVar; }
	virtual void OnInitializedFromLayout() OVERRIDE;

private:
	bool EventButtonClicked( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::EPanelEventSource_t eSource );

	ConVarRef   m_ConVar;
	panorama::CLabel*		m_pLabel;
	panorama::CPanel2D*   m_pButtonPanel;
};

class CCSGO_SettingsEnumDropDown : public panorama::CDropDown, public CCSGO_iConvarPanoramaSetter
{
	DECLARE_PANEL2D( CCSGO_SettingsEnumDropDown, panorama::CDropDown );

public:

	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

	CCSGO_SettingsEnumDropDown( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_SettingsEnumDropDown();

	virtual bool BSetProperty( panorama::CPanoramaSymbol symName, const char *pchValue ) OVERRIDE;
	virtual void OnShow();
	virtual ConVarRef& GetConVarRef() OVERRIDE { return m_ConVar; }
	virtual void OnInitializedFromLayout() OVERRIDE;

	void RefreshDisplay();
	void RestoreCVarDefault();

private:
	bool EventDropdownSelectionChanged( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel );

	ConVarRef	m_ConVar;
};
