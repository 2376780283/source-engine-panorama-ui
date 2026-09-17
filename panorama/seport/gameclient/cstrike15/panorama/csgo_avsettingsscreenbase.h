//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose:
//=============================================================================//
#pragma once

// SE port: CS:GO's game/client/cstrike15/panorama/csgo_avsettingsscreenbase.h, ported verbatim.
// The two helpers the audio / video settings screens use to drive a CSGOSettingsEnumDropDown.
// (CS:GO's copy relies on the including .cpp having pulled csgo_settings_enum.h in already; this
// copy includes it so the header stands on its own.)

#include "panorama/popups/csgo_settings_enum.h"

#define INVALID_OPTION_VALUE -999
#define AUTO_OPTION_VALUE 9999999

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static inline void DropdownSelectOptionByIndex( CCSGO_SettingsEnumDropDown *pDropDown, int nIndex, bool bNotify = false )
{
	pDropDown->SetSelected( nIndex, bNotify );
	pDropDown->InvalidateOptions( false );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static inline int DropdownGetSelectedValue( CCSGO_SettingsEnumDropDown *pDropdown )
{
	panorama::CPanel2D *pSelectedPanel = pDropdown->GetSelected();
	Assert( pSelectedPanel );

	int nValue = INVALID_OPTION_VALUE;
	if ( pSelectedPanel )
	{
		nValue = pSelectedPanel->GetAttribute( "value", INVALID_OPTION_VALUE );
	}

	return nValue;
}
