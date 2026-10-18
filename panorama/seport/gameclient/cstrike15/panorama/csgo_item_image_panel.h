//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Custom panorama panel for a single econ item icon (type "ItemImage")
//
// SE port: CS:GO's game/client/cstrike15/panorama/csgo_item_image_panel.* with the econ lookups
// redirected to the port's faux catalog (se_faux_econ.h).  CS:GO resolves "itemid" through
// CPlayerInventory / the reference item and takes the icon name from the item's schema entry
// (CEconItemView::GetInventoryImage(), "{images_econ}/..."), appending "_large" / "_small" when the
// content asked for one of those two sizes.  The port's catalog already stores the finished path, so
// the same accessors become a tail swap on that path.
//
// What was dropped: the generated-image path (items whose icon the game renders on the fly) and the
// "item_name" / "item_rarity" dialog variables, because both need the real schema (names live in the
// JS layer here - InventoryAPI.GetItemName()).  The panel's job in this build is the icon; the tile's
// label and rarity bar come from the content's own scripts.
//
//=============================================================================//

#ifndef CSGO_ITEM_IMAGE_PANEL_H
#define CSGO_ITEM_IMAGE_PANEL_H

#pragma once

#include "panorama/controls/image.h"
#include "tier1/utlstring.h"

class CItemImagePanel : public panorama::CImagePanel
{
	DECLARE_PANEL2D( CItemImagePanel, panorama::CImagePanel );

public:
	CItemImagePanel( panorama::CPanel2D *pParent, const char *pchID );

	virtual bool BSetProperty( panorama::CPanoramaSymbol symName, const char *pchValue ) OVERRIDE;
	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

	// CS:GO registers these as the JS "itemid" / "small" / "large" accessors.
	CUtlString GetItemID() const { return m_strItemID; }
	void SetItemID( CUtlString strItemID );

	bool BUsingSmallImage() const { return m_strDefaultImageAlternateSize == "_small"; }
	void SetUseSmallImage( bool bUse );
	bool BUsingLargeImage() const { return m_strDefaultImageAlternateSize == "_large"; }
	void SetUseLargeImage( bool bUse );

protected:
	// CS:GO: BSetFromItemID / BSetFromEconItem.  Returns false for an id the catalog does not know,
	// which leaves whatever "src" the layout set untouched.
	bool BSetFromItemID( const CUtlString &strItemID );
	void ApplyImage();

	CUtlString m_strItemID;
	CUtlString m_strItemImage;					// catalog path, "file://{images_econ}/..._large.png"
	CUtlString m_strDefaultImageAlternateSize;	// "" (as stored) / "_large" / "_small"
};

#endif // CSGO_ITEM_IMAGE_PANEL_H
