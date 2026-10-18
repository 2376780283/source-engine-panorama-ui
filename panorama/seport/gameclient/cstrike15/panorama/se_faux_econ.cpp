//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: SE port - the data behind se_faux_econ.h (see that file for why this exists).
//
//=============================================================================//

#include "panorama/se_gameclient_common.h"

#include "panorama/se_faux_econ.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace se_faux_econ;

namespace
{
	// The category tags are the "Inv_Category_*" names the content localizes; "any" is what CS:GO's
	// own inventory shows first and is also the tag the list class treats as "everything".
	const char *k_Categories[] =
	{
		"inv_category_any",
		"inv_category_tools",
		"inv_category_container",
		"inv_category_collections",
	};

	const char *k_SubCategories[] =
	{
		"any",
	};

	// The catalog.  Item ids are "se_store_<def>_<paint>", the same form the JS layer builds and names
	// (se_session_sim.js: STORE_NAMES / the "SEPort_Op_*" tokens), so the tiles' names resolve through
	// the port's localization file.  The icons are CS:GO's own item art: {images_econ} is the mod's
	// resource/flash directory (panorama.cfg), and the files below are the ones the CS:GO install
	// ships (econ/tools, econ/status_icons, econ/weapon_cases and the generated weapon renders).
	CatalogItem_t k_Catalog[] =
	{
		// --- the store's own entries (Paris 2023 + the operations) -------------------------------
		{ 4883, 0, "inv_category_tools",       "any", "any", 4, "file://{images_econ}/econ/status_icons/blast_pickem_2023_pass_large.png" },
		{ 4888, 0, "inv_category_container",   "any", "any", 3, "file://{images_econ}/econ/weapon_cases/atlanta2017_bundleofall.png" },
		{ 6732, 0, "inv_category_container",   "any", "any", 3, "file://{images_econ}/econ/tools/sticker_crate_key.png" },

		// Operation Payback .. Vanguard have no coin art in this content; campaign.png is the generic
		// operation icon.  Bloodhound (6) .. Riptide (11) match the operation_<n>_gold coins the
		// install ships - the numbers are CS:GO's own operation numbers.
		{ 9101, 0, "inv_category_collections", "any", "any", 4, "file://{images_econ}/econ/tools/campaign.png" },
		{ 9102, 0, "inv_category_collections", "any", "any", 4, "file://{images_econ}/econ/tools/campaign.png" },
		{ 9103, 0, "inv_category_collections", "any", "any", 4, "file://{images_econ}/econ/tools/campaign.png" },
		{ 9104, 0, "inv_category_collections", "any", "any", 4, "file://{images_econ}/econ/tools/campaign.png" },
		{ 9105, 0, "inv_category_collections", "any", "any", 4, "file://{images_econ}/econ/tools/campaign.png" },
		{ 9106, 0, "inv_category_collections", "any", "any", 4, "file://{images_econ}/econ/status_icons/operation_6_gold_large.png" },
		{ 9107, 0, "inv_category_collections", "any", "any", 4, "file://{images_econ}/econ/status_icons/operation_7_gold_large.png" },
		{ 9108, 0, "inv_category_collections", "any", "any", 4, "file://{images_econ}/econ/status_icons/operation_8_gold_large.png" },
		{ 9109, 0, "inv_category_collections", "any", "any", 4, "file://{images_econ}/econ/status_icons/operation_9_gold_large.png" },
		{ 9110, 0, "inv_category_collections", "any", "any", 4, "file://{images_econ}/econ/status_icons/operation_10_gold_large.png" },
		{ 9111, 0, "inv_category_collections", "any", "any", 4, "file://{images_econ}/econ/status_icons/operation_11_gold_large.png" },

		// --- the operation's quests and rewards --------------------------------------------------
		{ 9201, 0, "inv_category_tools",       "any", "any", 3, "file://{images_econ}/econ/tools/mission.png" },
		{ 9202, 0, "inv_category_tools",       "any", "any", 3, "file://{images_econ}/econ/tools/mission.png" },
		{ 9203, 0, "inv_category_tools",       "any", "any", 3, "file://{images_econ}/econ/tools/mission.png" },

		{ 9301, 0, "inv_category_collections", "any", "any", 5, "file://{images_econ}/econ/default_generated/weapon_ak47_cu_ak47_asiimov_light_large.png" },
		{ 9302, 0, "inv_category_collections", "any", "any", 5, "file://{images_econ}/econ/default_generated/weapon_ak47_cu_ak47_anubis_light_large.png" },
		{ 9303, 0, "inv_category_collections", "any", "any", 5, "file://{images_econ}/econ/default_generated/weapon_ak47_gs_ak47_bloodsport_light_large.png" },
		{ 9304, 0, "inv_category_collections", "any", "any", 5, "file://{images_econ}/econ/default_generated/weapon_ak47_gs_ak47_empress_light_large.png" },
		{ 9305, 0, "inv_category_collections", "any", "any", 5, "file://{images_econ}/econ/default_generated/weapon_ak47_gs_ak47_nibbler_light_large.png" },
		{ 9306, 0, "inv_category_collections", "any", "any", 5, "file://{images_econ}/econ/default_generated/weapon_knife_karambit_am_fade_light_large.png" },
	};

	const int k_nCatalogCount = V_ARRAYSIZE( k_Catalog );

	bool ItemMatchesTier( const CatalogItem_t &item, const char *pchCategory, const char *pchSubCategory, const char *pchGroup )
	{
		// "any" is the wildcard in every tier, exactly like CS:GO's client inventory view treats it.
		const bool bCategoryMatches = ( !V_stricmp( pchCategory, "any" ) || !V_stricmp( pchCategory, "inv_category_any" ) )
									  || !V_stricmp( pchCategory, item.m_pchCategory );
		if ( !bCategoryMatches )
			return false;

		const bool bSubMatches = ( !pchSubCategory || !V_stricmp( pchSubCategory, "any" ) )
								 || !V_stricmp( pchSubCategory, item.m_pchSubCategory );
		if ( !bSubMatches )
			return false;

		const bool bGroupMatches = ( !pchGroup || !V_stricmp( pchGroup, "any" ) )
								   || !V_stricmp( pchGroup, item.m_pchGroup );
		return bGroupMatches;
	}
}

void se_faux_econ::MakeItemID( char *pchOut, int nOutBufferSize, uint32 unDefIndex, uint32 unPaintKit )
{
	V_snprintf( pchOut, nOutBufferSize, "se_store_%u_%u", unDefIndex, unPaintKit );
}

int se_faux_econ::GetCatalogCount()
{
	return k_nCatalogCount;
}

const CatalogItem_t &se_faux_econ::GetCatalogItem( int nIndex )
{
	if ( nIndex < 0 || nIndex >= k_nCatalogCount )
		nIndex = 0;

	return k_Catalog[ nIndex ];
}

const CatalogItem_t *se_faux_econ::FindItemByID( const char *pchItemID )
{
	if ( !pchItemID )
		return NULL;

	char szItemID[ 64 ];
	for ( int i = 0; i < k_nCatalogCount; ++i )
	{
		MakeItemID( szItemID, sizeof( szItemID ), k_Catalog[ i ].m_unDefIndex, k_Catalog[ i ].m_unPaintKit );
		if ( !V_stricmp( pchItemID, szItemID ) )
			return &k_Catalog[ i ];
	}

	return NULL;
}

int se_faux_econ::GetCategoryCount()
{
	return V_ARRAYSIZE( k_Categories );
}

const char *se_faux_econ::GetCategory( int nIndex )
{
	if ( nIndex < 0 || nIndex >= V_ARRAYSIZE( k_Categories ) )
		return k_Categories[ 0 ];

	return k_Categories[ nIndex ];
}

int se_faux_econ::GetSubCategoryCount( const char *pchCategory )
{
	// Every category currently exposes a single "any" tier - the UI builds one tab per entry, and
	// "any" is the label the content localizes as Inv_Category_any.
	( void )pchCategory;
	return V_ARRAYSIZE( k_SubCategories );
}

const char *se_faux_econ::GetSubCategory( const char *pchCategory, int nIndex )
{
	( void )pchCategory;

	if ( nIndex < 0 || nIndex >= V_ARRAYSIZE( k_SubCategories ) )
		return k_SubCategories[ 0 ];

	return k_SubCategories[ nIndex ];
}

bool se_faux_econ::IsKnownCategory( const char *pchCategory )
{
	for ( int i = 0; i < V_ARRAYSIZE( k_Categories ); ++i )
	{
		if ( !V_stricmp( pchCategory, k_Categories[ i ] ) )
			return true;
	}

	return false;
}

int se_faux_econ::CollectItemIDs( const char *pchCategory, const char *pchSubCategory, const char *pchGroup,
								  CUtlVector<CUtlString> &vecOutIDs )
{
	vecOutIDs.RemoveAll();

	char szItemID[ 64 ];
	for ( int i = 0; i < k_nCatalogCount; ++i )
	{
		if ( !ItemMatchesTier( k_Catalog[ i ], pchCategory, pchSubCategory, pchGroup ) )
			continue;

		MakeItemID( szItemID, sizeof( szItemID ), k_Catalog[ i ].m_unDefIndex, k_Catalog[ i ].m_unPaintKit );
		vecOutIDs.AddToTail( CUtlString( szItemID ) );
	}

	return vecOutIDs.Count();
}

int se_faux_econ::GetRarityForItemID( const char *pchItemID )
{
	for ( int i = 0; i < k_nCatalogCount; ++i )
	{
		char szItemID[ 64 ];
		MakeItemID( szItemID, sizeof( szItemID ), k_Catalog[ i ].m_unDefIndex, k_Catalog[ i ].m_unPaintKit );
		if ( !V_stricmp( pchItemID, szItemID ) )
			return ( int )k_Catalog[ i ].m_unRarity;
	}

	return 0;
}

void se_faux_econ::SortItemIDs( CUtlVector<CUtlString> &vecIDs, const char *pchSort )
{
	if ( !pchSort || !pchSort[ 0 ] || !V_stricmp( pchSort, "inv_sort_age" ) )
		return;		// the catalog order is the "age" order (newest additions last)

	if ( !V_stricmp( pchSort, "inv_sort_rarity" ) || !V_stricmp( pchSort, "inv_sort_quality" ) )
	{
		vecIDs.SortPredicate( []( const CUtlString &lhs, const CUtlString &rhs )
		{
			const int nLHS = GetRarityForItemID( lhs.Get() );
			const int nRHS = GetRarityForItemID( rhs.Get() );
			if ( nLHS != nRHS )
				return nLHS > nRHS;

			return V_stricmp( lhs.Get(), rhs.Get() ) < 0;
		} );
		return;
	}

	if ( !V_stricmp( pchSort, "inv_sort_alpha" ) )
	{
		vecIDs.SortPredicate( []( const CUtlString &lhs, const CUtlString &rhs )
		{
			return V_stricmp( lhs.Get(), rhs.Get() ) < 0;
		} );
		return;
	}

	// inv_sort_slot / inv_sort_collection / inv_sort_equipped / inv_sort_paint / inv_sort_wear have no
	// meaning for this data set (no loadout slots, no collections, no wear), so they keep the catalog
	// order instead of pretending to sort.
}
