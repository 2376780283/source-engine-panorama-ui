//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Custom panorama panel for a page of inventory items
//
// SE port: see csgo_inventory_item_list.h for what changed relative to CS:GO.
//
//=============================================================================//

#include "panorama/se_gameclient_common.h"

#include "panorama/csgo_inventory_item_list.h"
#include "panorama/se_faux_econ.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

REGISTER_PANEL2D_FACTORY( CCSGO_InventoryItemList, InventoryItemList );

DEFINE_PANORAMA_EVENT( CSGOInventoryItemLoaded );
DEFINE_PANORAMA_EVENT( SetInventoryFilter );
DEFINE_PANORAMA_EVENT( UpdateItemTile );

using namespace panorama;

CCSGO_InventoryItemList::CCSGO_InventoryItemList( CPanel2D *pParent, const char *pchID ) : BaseClass( pParent, pchID )
{
	// CS:GO loads inventory_item_list.xml here (DbgVerify( BLoadLayout( ... ) )).  That layout carries
	// the list's own sizing and contains an <InventoryItemList> element; in CS:GO that nested element
	// is what ends up holding the properties.  Loading it from inside this constructor constructs the
	// nested instance recursively, so a guard lets the nested copy come up without re-loading the same
	// file (it stays an empty child) and the sizing is applied explicitly instead - the values are the
	// ones the layout declares (itemwidth/itemheight/spacersize/spacerperiod).
	static bool s_bSEInventoryListLoadingLayout = false;
	if ( !s_bSEInventoryListLoadingLayout )
	{
		s_bSEInventoryListLoadingLayout = true;
		DbgVerify( BLoadLayout( "file://{resources}/layout/inventory_item_list.xml" ) );
		s_bSEInventoryListLoadingLayout = false;
	}

	SetListItemSize( CUILength( 192.0f, CUILength::k_EUILengthLength ), CUILength( 218.0f, CUILength::k_EUILengthLength ) );
	SetSpacerPeriodAndSize( 0, CUILength( 0.0f, CUILength::k_EUILengthLength ) );

	m_strTileLayoutFile.Set( "file://{resources}/layout/itemtile.xml" );
	SetAcceptsInput( true );

	SetLoadListItemFunction( [ this ]( panorama::CPanel2D *pParent, int nItemIndex, panorama::CPanel2D *pReuseItemPanel ) -> CPanel2D*
	{
		static const CPanoramaSymbol k_symItemID( "itemid" );
		static const CPanoramaSymbol k_symContextMenuFilter( "context_menu_filter" );
		static const CPanoramaSymbol k_symFilterCategory( "filter_category" );

		if ( nItemIndex < 0 || nItemIndex >= m_vecItems.Count() )
			return NULL;

		const char *pchItemID = m_vecItems[ nItemIndex ].Get();

		CPanel2D *pItemPanel = pReuseItemPanel;
		if ( !pItemPanel )
		{
			pItemPanel = new CPanel2D( pParent, NULL );
			pItemPanel->BLoadLayout( m_strTileLayoutFile.Get() );
		}

		// The tile's JS (itemtile.js) reads these back as strings - "itemid" goes to
		// InventoryAPI.GetItemName()/ItemInfo.* which is where the port's item data lives.
		pItemPanel->SetAttribute( k_symItemID, pchItemID );
		pItemPanel->SetAttribute( k_symContextMenuFilter, m_strTileContextMenuFilter.Get() );
		pItemPanel->SetAttribute( k_symFilterCategory, m_strPresentationList.Get() );

		// CS:GO resolves the icon through the econ item (CEconItemView::GetInventoryImage() under
		// {images_econ}); the port's catalog carries the path directly - see se_faux_econ.h.
		CImagePanel *pImage = panel_cast< CImagePanel* >( pItemPanel->FindChild( "ItemImage" ) );
		if ( pImage )
		{
			const se_faux_econ::CatalogItem_t *pItem = se_faux_econ::FindItemByID( pchItemID );
			if ( pItem && pItem->m_pchImage )
				pImage->SetImage( pItem->m_pchImage );
		}

		DispatchEvent( CSGOInventoryItemLoaded(), pItemPanel );

		return pItemPanel;
	} );

	RegisterEventHandler( SetInventoryFilter(), this, &CCSGO_InventoryItemList::EventSetCategorySortAndFilters );

	// CS:GO also registers for PanoramaComponent_Inventory_PlayerEquipSlotChanged() here to refresh the
	// equipped dots on a single tile.  The port's JS layer never raises that event (there is no GC to
	// change the equip state behind the UI's back), so there is nothing to re-register for.
}

CCSGO_InventoryItemList::~CCSGO_InventoryItemList()
{
}

void CCSGO_InventoryItemList::SetupJavascriptObjectTemplate()
{
	BaseClass::SetupJavascriptObjectTemplate();
	RegisterJSAccessorReadOnly( "count", PANORAMA_DELEGATE( &CCSGO_InventoryItemList::GetItemCount ) );
}

bool CCSGO_InventoryItemList::EventSetCategorySortAndFilters( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel, const char *szCategory, const char *szSubCategory /*= "any"*/, const char *szGroup /*= "any"*/, const char *szSort /*= nullptr*/, const char *szFilterString /*= nullptr*/, const char *szSubStringFilter /*=nullptr*/ )
{
	( void )ptrPanel;
	SetCategorySortAndFilters( szCategory, szSubCategory, szGroup, szSort, szFilterString, szSubStringFilter );
	return true;
}

bool CCSGO_InventoryItemList::SetCategorySortAndFilters( const char *szCategory, const char *szSubCategory /*= "any"*/, const char *szGroup /*= "any"*/, const char *szSort /*= nullptr*/, const char *szFilterString /*= nullptr*/, const char *szSubStringFilter /*= nullptr */ )
{
	if ( !szCategory || !szCategory[ 0 ] )
		return false;

	// CS:GO looks (category -> subcategory -> group) up in the local player's client inventory view and
	// warns when a tier is missing; the faux layer answers the same question for its own catalog.
	if ( !se_faux_econ::IsKnownCategory( szCategory ) )
	{
		Warning( "CCSGO_InventoryItemList: Failed to find category %s\n", szCategory );
		return false;
	}

	if ( !szSubCategory || !szSubCategory[ 0 ] )
		szSubCategory = "any";
	if ( !szGroup || !szGroup[ 0 ] )
		szGroup = "any";

	// Copy the selected view tier into our internal display list (CS:GO: FOR_EACH_MAP( pGroup->m_Items ) ).
	m_vecItems.RemoveAll();
	se_faux_econ::CollectItemIDs( szCategory, szSubCategory, szGroup, m_vecItems );

	// The two filters CS:GO applies here are both name based:
	//   * szFilterString is the legacy CInventoryFilters expression ("quality:unusual, ..."), and
	//   * szSubStringFilter is the plain "search by item name" one (UTIL_FilterByItemName).
	// The port's item names are localized by the JS layer (InventoryAPI.GetItemName() reads the
	// se_port token table), so C++ has no name to match against; the search panel filters in JS
	// through InventoryAPI.SetInventorySortAndFilters() + GetInventoryCount()/GetInventoryItemIDByIndex()
	// instead.  Filtering here stays a no-op rather than dropping items on a guess.
	( void )szFilterString;
	( void )szSubStringFilter;

	se_faux_econ::SortItemIDs( m_vecItems, szSort );

	// Reload the list with the new contained items; the filters remembered below are handed to each
	// tile's attributes while it is populated (delayed load).
	UpdateListItems( m_vecItems.Count() );
	m_strPresentationList = szCategory;

	return m_vecItems.Count() > 0;
}

bool CCSGO_InventoryItemList::BSetProperties( const CUtlVector< ParsedPanelProperty_t > &vecProperties )
{
	static const CPanoramaSymbol k_symInventoryFilter( "inventoryfilter" );
	static const CPanoramaSymbol k_symItemLayoutFile( "itemlayout" );
	static const CPanoramaSymbol k_symItemContextMenuFilter( "item_context_menu_filter" );

	for ( const ParsedPanelProperty_t &prop : vecProperties )
	{
		if ( prop.m_symName == k_symInventoryFilter )
		{
			SetCategorySortAndFilters( prop.m_pchValue, "any", "any" );
		}
		else if ( prop.m_symName == k_symItemLayoutFile )
		{
			m_strTileLayoutFile.Set( prop.m_pchValue );
		}
		else if ( prop.m_symName == k_symItemContextMenuFilter )
		{
			m_strTileContextMenuFilter.Set( prop.m_pchValue );
		}
	}

	return BaseClass::BSetProperties( vecProperties );
}
