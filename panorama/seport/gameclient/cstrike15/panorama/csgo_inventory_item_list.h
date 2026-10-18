//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Custom panorama panel for a page of inventory items
//
// SE port: this is CS:GO's game/client/cstrike15/panorama/csgo_inventory_item_list.* with the econ
// calls redirected to the port's faux econ layer (panorama/se_faux_econ.h - see that file's header
// for why the port has no real econ).  Everything else keeps CS:GO's shape: the list is a
// CDelayLoadList, the content creates it by name ("$.CreatePanel('InventoryItemList', parent, id)")
// and drives it with the SetInventoryFilter event, and each tile is itemtile.xml with the item id
// handed over as a panel attribute.
//
//=============================================================================//

#ifndef CSGO_INVENTORY_ITEM_LIST_H
#define CSGO_INVENTORY_ITEM_LIST_H

#pragma once

#include "panorama/controls/delayloadlist.h"
#include "tier1/utlstring.h"
#include "tier1/utlvector.h"

DECLARE_PANEL_EVENT0( CSGOInventoryItemLoaded );
DECLARE_PANEL_EVENT6( SetInventoryFilter, const char *, const char *, const char *, const char *, const char *, const char * );
DECLARE_PANEL_EVENT1( UpdateItemTile, const char * );

class CCSGO_InventoryItemList : public panorama::CDelayLoadList
{
	DECLARE_PANEL2D( CCSGO_InventoryItemList, panorama::CDelayLoadList );

public:
	CCSGO_InventoryItemList( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_InventoryItemList();

	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

	bool SetCategorySortAndFilters( const char *szCategory, const char *szSubCategory = "any", const char *szGroup = "any", const char *szSort = NULL, const char *szFilterString = NULL, const char *szSubStringFilter = NULL );
	int GetItemCount( void ) const { return m_vecItems.Count(); }

protected:
	// CPanel2D overrides
	virtual bool BSetProperties( const CUtlVector< panorama::ParsedPanelProperty_t > &vecProperties ) OVERRIDE;

	bool EventSetCategorySortAndFilters( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel, const char *szCategory, const char *szSubCategory = "any", const char *szGroup = "any", const char *szSort = NULL, const char *szFilterString = NULL, const char *szSubStringFilter = NULL );

	// CS:GO stores itemid_t here; this port's items are the JS layer's string ids
	// ("se_store_<defindex>_<paintindex>"), so the vector holds the id strings and the tiles get the
	// same string back through their "itemid" attribute - see se_faux_econ.h.
	CUtlVector< CUtlString > m_vecItems;
	CUtlString m_strTileLayoutFile;
	CUtlString m_strTileContextMenuFilter;
	CUtlString m_strPresentationList;
};

#endif // CSGO_INVENTORY_ITEM_LIST_H
