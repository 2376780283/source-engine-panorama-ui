//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Menu to change player loadout
//
// SE port: see csgo_loadout.h for what was kept and what was dropped.
//
//=============================================================================//

#include "panorama/se_gameclient_common.h"

#include "panorama/csgo_loadout.h"
#include "panorama/csgo_inventory_item_list.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

DEFINE_PANORAMA_EVENT( ShowLoadout );
DEFINE_PANORAMA_EVENT( Loadout_FilterForPosition );

REGISTER_PANEL2D_FACTORY( CCSGO_Loadout, CSGOLoadout );

using namespace panorama;

CCSGO_Loadout::CCSGO_Loadout( CPanel2D *pParent, const char *pchID )
	: CPanel2D( pParent, pchID )
	, m_pTeamLogo( NULL )
	, m_pItemWheel( NULL )
	, m_pItemList( NULL )
{
	SetInputNamespace( "loadout" );

	SetAcceptsInput( true );
	SetAcceptsFocus( true );

	BLoadLayout( "file://{resources}/layout/loadout.xml" );

	// CS:GO uses RequireChildInLayoutFile() for all three; the port only requires the item list (the
	// one child it actually needs) and looks the others up defensively, because the radial selector
	// is one of the CS:GO panel types this port still substitutes with a plain Panel.
	m_pTeamLogo = FindChildInLayoutFile( "TeamLogo" );
	m_pItemWheel = FindChildInLayoutFile( "ItemWheel" );
	m_pItemList = panel_cast< CCSGO_InventoryItemList* >( FindChildInLayoutFile( "LoadoutItemList" ) );

	if ( !m_pItemList )
	{
		Warning( "CCSGO_Loadout: loadout.xml has no LoadoutItemList panel - the item list stays empty\n" );
	}

	// CS:GO loads the "ItemWedge" snippet into each wedge of the radial selector here.  The selector is
	// a plain Panel in this build (see csgo_radial_selector.h), so there are no wedges to fill and the
	// loop is skipped rather than asserting on children that do not exist.

	if ( m_pItemList )
		ShowItemsForSlot( "any", 0 );

	RegisterForUnhandledEvent( ShowLoadout(), this, &CCSGO_Loadout::EventShowLoadout );
	RegisterEventHandler( Loadout_FilterForPosition(), this, &CCSGO_Loadout::EventFilterForPosition );
}

//-----------------------------------------------------------------------------
// Purpose: point the item list at the faux catalog for the requested slot
//-----------------------------------------------------------------------------
void CCSGO_Loadout::ShowItemsForSlot( const char *szLoadoutSlot, int nTeam )
{
	if ( !m_pItemList )
		return;

	// The port's catalog has a single category tree (se_faux_econ.cpp) and no per-slot loadouts, so
	// every slot shows the same items.  The calls below are the ones the content makes through the
	// SetInventoryFilter event (loadout.js::_UpdateItemListForSelectedSlot), which is why the item
	// list goes through its normal path: category / subcategory / group / sort.
	m_pItemList->SetCategorySortAndFilters( "inv_category_any", "any", "any", "inv_sort_age" );

	Msg( "CCSGO_Loadout: showing items for slot '%s' (team %d) - %d item(s)\n",
		 szLoadoutSlot ? szLoadoutSlot : "?", nTeam, m_pItemList->GetItemCount() );
}

bool CCSGO_Loadout::EventShowLoadout( const char *szLoadoutSlot, int nTeam )
{
	ShowItemsForSlot( szLoadoutSlot, nTeam );
	return true;
}

bool CCSGO_Loadout::EventFilterForPosition( int nTeam, int nPosition )
{
	// CS:GO maps the loadout position range to the wedge positions of the radial selector; with no
	// loadout state both the team and the position are informational only.
	Msg( "CCSGO_Loadout: filter for position %d (team %d)\n", nPosition, nTeam );
	ShowItemsForSlot( "any", nTeam );
	return true;
}
