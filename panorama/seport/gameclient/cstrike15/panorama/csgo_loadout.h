//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Menu to change player loadout
//
// SE port: CS:GO's game/client/cstrike15/panorama/csgo_loadout.* with the loadout-state pieces
// dropped.  In CS:GO this panel is fed by CStrikeLoadout / CUiComponent_Loadout (which loadout item
// is equipped in which slot, per team) and it hosts a radial selector (CCSGO_RadialSelector).
// This tree has neither the loadout state nor the radial selector, so what is left is the panel
// itself: it loads loadout.xml, keeps the children the content drives (TeamLogo, ItemWheel,
// LoadoutItemList) and answers the two events the content raises (ShowLoadout /
// Loadout_FilterForPosition) by pointing its item list at the port's faux catalog.  The result is
// the loadout UI appearing and listing items; "which item is equipped" stays unimplemented.
//
//=============================================================================//

#ifndef CSGO_LOADOUT_H
#define CSGO_LOADOUT_H

#pragma once

#include "panorama/controls/panel2d.h"

class CCSGO_InventoryItemList;

DECLARE_PANORAMA_EVENT2( ShowLoadout, const char *, int );
DECLARE_PANORAMA_EVENT2( Loadout_FilterForPosition, int, int );

class CCSGO_Loadout : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CCSGO_Loadout, panorama::CPanel2D );

public:
	CCSGO_Loadout( panorama::CPanel2D *pParent, const char *pchID );

	bool EventShowLoadout( const char *szLoadoutSlot, int nTeam );
	bool EventFilterForPosition( int nTeam, int nPosition );

private:
	// CS:GO holds these as CPanelPtr (weak references); plain pointers are enough here - the loadout
	// panel and its children live as long as the main menu does.
	panorama::CPanel2D *m_pTeamLogo;
	panorama::CPanel2D *m_pItemWheel;
	CCSGO_InventoryItemList *m_pItemList;

	// Every "slot" (secondary / smg / rifle / heavy / gear) filters the same faux catalog in this
	// build - there is no loadout state to restrict it to.
	void ShowItemsForSlot( const char *szLoadoutSlot, int nTeam );
};

#endif // CSGO_LOADOUT_H
