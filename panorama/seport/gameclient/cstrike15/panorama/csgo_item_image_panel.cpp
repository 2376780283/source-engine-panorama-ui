//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Custom panorama panel for a single econ item icon (type "ItemImage")
//
// SE port: see csgo_item_image_panel.h for what was kept and what was dropped.
//
//=============================================================================//

#include "panorama/se_gameclient_common.h"

#include "panorama/csgo_item_image_panel.h"
#include "panorama/se_faux_econ.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

REGISTER_PANEL2D_FACTORY( CItemImagePanel, ItemImage );

using namespace panorama;

namespace
{
	// CS:GO uses CSSHelpers::BParseTrueFalse() for the "small" / "large" properties; the port has no
	// CSSHelpers, so the accepted spellings are the ones the content uses ("true", "1").
	bool SE_ParseTrueFalse( const char *pchValue, bool *pbOut )
	{
		if ( !pchValue )
			return false;

		if ( !V_stricmp( pchValue, "true" ) || !V_stricmp( pchValue, "yes" ) || !V_stricmp( pchValue, "1" ) )
		{
			*pbOut = true;
			return true;
		}

		if ( !V_stricmp( pchValue, "false" ) || !V_stricmp( pchValue, "no" ) || !V_stricmp( pchValue, "0" ) )
		{
			*pbOut = false;
			return true;
		}

		return false;
	}
}

CItemImagePanel::CItemImagePanel( CPanel2D *pParent, const char *pchID )
	: BaseClass( pParent, pchID )
{
	// CS:GO also hooks the ImageLoaded() / ImageFailedLoad() panel events here to drive the
	// "_small" -> unsized fallback chain.  The port's image panel does not expose those events (see
	// public/panorama/controls/image.h), and the catalog has no size variants to fall back to, so the
	// icon is set once.
}

void CItemImagePanel::SetupJavascriptObjectTemplate()
{
	BaseClass::SetupJavascriptObjectTemplate();

	RegisterJSAccessor( "itemid", PANORAMA_DELEGATE( &CItemImagePanel::GetItemID ), PANORAMA_DELEGATE( &CItemImagePanel::SetItemID ) );
	RegisterJSAccessor( "small", PANORAMA_DELEGATE( &CItemImagePanel::BUsingSmallImage ), PANORAMA_DELEGATE( &CItemImagePanel::SetUseSmallImage ) );
	RegisterJSAccessor( "large", PANORAMA_DELEGATE( &CItemImagePanel::BUsingLargeImage ), PANORAMA_DELEGATE( &CItemImagePanel::SetUseLargeImage ) );
}

bool CItemImagePanel::BSetProperty( CPanoramaSymbol symName, const char *pchValue )
{
	static CPanoramaSymbol symItemID( "itemid" );
	static CPanoramaSymbol symSmallImage( "small" );
	static CPanoramaSymbol symLargeImage( "large" );

	if ( symName == symItemID )
	{
		// CS:GO parses the property as a number (a combined def+paint id) and rejects it otherwise.
		// The port's ids are the JS layer's strings ("se_store_<def>_<paint>"), so the value is taken
		// as-is; an unknown id simply leaves the panel without an icon.
		SetItemID( CUtlString( pchValue ) );
	}
	else if ( symName == symSmallImage )
	{
		bool bValue = false;
		if ( SE_ParseTrueFalse( pchValue, &bValue ) )
			SetUseSmallImage( bValue );
	}
	else if ( symName == symLargeImage )
	{
		bool bValue = false;
		if ( SE_ParseTrueFalse( pchValue, &bValue ) )
			SetUseLargeImage( bValue );
	}

	return BaseClass::BSetProperty( symName, pchValue );
}

void CItemImagePanel::SetItemID( CUtlString strItemID )
{
	m_strItemID = strItemID;

	if ( !BSetFromItemID( m_strItemID ) )
	{
		// Not an item this build knows (or no icon for it): keep whatever image was set before, the
		// same way CS:GO leaves the panel alone when the lookup fails.
		m_strItemImage.Clear();
	}
}

bool CItemImagePanel::BSetFromItemID( const CUtlString &strItemID )
{
	const se_faux_econ::CatalogItem_t *pItem = se_faux_econ::FindItemByID( strItemID.Get() );
	if ( !pItem || !pItem->m_pchImage )
		return false;

	m_strItemImage.Set( pItem->m_pchImage );
	ApplyImage();
	return true;
}

void CItemImagePanel::ApplyImage()
{
	if ( m_strItemImage.IsEmpty() )
		return;

	const char *pchPath = m_strItemImage.Get();
	const char *pchVariant = m_strDefaultImageAlternateSize.Get();

	// The catalog path already ends in "_large.png" (that is the render CS:GO's item images are
	// stored as).  A "_small" request swaps the tail; anything the content ships only as "_large"
	// simply keeps it, which is why the swap falls back to the stored path.
	if ( pchVariant && pchVariant[ 0 ] && V_stricmp( pchVariant, "_large" ) )
	{
		const char *pchLargeSuffix = V_strstr( pchPath, "_large.png" );
		if ( pchLargeSuffix )
		{
			CFmtStr strVariant( "%.*s%s.png", ( int )( pchLargeSuffix - pchPath ), pchPath, pchVariant );
			SetImage( strVariant.Get() );
			return;
		}
	}

	SetImage( pchPath );
}

void CItemImagePanel::SetUseSmallImage( bool bUse )
{
	if ( bUse )
		m_strDefaultImageAlternateSize.Set( "_small" );
	else if ( m_strDefaultImageAlternateSize == "_small" )
		m_strDefaultImageAlternateSize.Clear();

	ApplyImage();
}

void CItemImagePanel::SetUseLargeImage( bool bUse )
{
	if ( bUse )
		m_strDefaultImageAlternateSize.Set( "_large" );
	else if ( m_strDefaultImageAlternateSize == "_large" )
		m_strDefaultImageAlternateSize.Clear();

	ApplyImage();
}
