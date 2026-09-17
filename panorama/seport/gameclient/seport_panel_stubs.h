//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: SE port - stand-ins for the CS:GO game-client panel classes this port does not implement
//          (ItemImage, ItemPreviewPanel, ItemPreviewSlider, ItemPreviewColorSlider, ItemPreviewDebug,
//          CSGOAvatarImage, CSGOChat).
//
//          The content uses them by name (66 <ItemImage>, 38 <CSGOAvatarImage>, 36 <ItemPreviewPanel>,
//          11 <ItemPreviewSlider>, 4 <ItemPreviewColorSlider>, 1 <ItemPreviewDebug>, 1 <CSGOChat> in
//          the deployed layouts, plus $.CreatePanel( "ItemImage" / "CSGOAvatarImage" / "ItemPreviewPanel"
//          ) in the scripts).  Until the type is registered the layout loader substitutes a plain Panel
//          (panorama/layout/layoutfile.cpp::BAddPanel) and appends the original type name as a CSS
//          class.  That keeps the layout loadable, but leaves two holes:
//
//            * the widgets draw nothing at all (no item icon, no avatar, no 3D preview), and
//            * every JS call into them - "vanityPanel.SetSceneAngles( ... )", "elItemImage.itemid = x",
//              "elAvatarImage.SetDefaultImage( ... )" - is a TypeError ("is not a function").  A
//              Panorama exception drops the *rest of the calling script*, which is why
//              mainmenu.js::_InitVanity stops at line 971 and _OnHomeButtonPressed stops at its Pause().
//
//          These stubs close both holes without porting the real classes:
//
//            * the type names are registered, so no substitution happens and the stylesheet rules that
//              select on those types keep applying,
//            * the whole JS surface the content uses is registered and accepted (no-ops.  Panorama only
//              rejects a call that passes *fewer* arguments than the delegate declares, see
//              JSMethodCallTuple() in public/panorama/uijsregistration.h, so one zero-argument no-op
//              can back every name),
//            * the game-client specific XML properties (itemid, large, steamid, manifest, ...) are
//              swallowed instead of failing the property parse, and
//            * the two classes that are CImagePanel-derived in CS:GO stay CImagePanel-derived here, so
//              "src" / ItemImage's inherited image handling and CSGOAvatarImage's "defaultsrc" still
//              display the (default) art the content asks for.
//
//          The real classes need the econ data layer (ui_econ_item_image.cpp, ui_itempreview_panel.cpp
//          + its 3D renderer) and Steam avatar bits, so they stay out of scope - see the notes in
//          docs/panorama_stage2_plan.md.
//
//=============================================================================//

#ifndef SE_PORT_GAMECLIENT_PANEL_STUBS_H
#define SE_PORT_GAMECLIENT_PANEL_STUBS_H
#pragma once

#include "panorama/controls/panel2d.h"
#include "panorama/controls/image.h"
#include "panorama/iuipanel.h"

//-----------------------------------------------------------------------------
// Purpose: single econ item icon.  CS:GO: CItemImagePanel, type "ItemImage".
//-----------------------------------------------------------------------------
class CSEPortStub_ItemImage : public panorama::CImagePanel
{
	DECLARE_PANEL2D( CSEPortStub_ItemImage, panorama::CImagePanel );

public:
	CSEPortStub_ItemImage( panorama::CPanel2D *pParent, const char *pchID );

	virtual bool BSetProperty( panorama::CPanoramaSymbol symName, const char *pchValue ) OVERRIDE;
	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

	// JS surface (the content assigns these; the values are deliberately ignored).
	int JSGetItemID() const;
	void JSSetItemID( int nItemID );
	bool JSGetLarge() const;
	void JSSetLarge( bool bValue );
	bool JSGetSmall() const;
	void JSSetSmall( bool bValue );
};

//-----------------------------------------------------------------------------
// Purpose: the 3D item / character preview host.  CS:GO: CUI_ItemPreviewPanel (6861 lines + a 2859
//          line renderer), type "ItemPreviewPanel".  This stub answers the whole JS surface no-op.
//-----------------------------------------------------------------------------
class CSEPortStub_ItemPreviewPanel : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CSEPortStub_ItemPreviewPanel, panorama::CPanel2D );

public:
	CSEPortStub_ItemPreviewPanel( panorama::CPanel2D *pParent, const char *pchID );

	virtual bool BSetProperty( panorama::CPanoramaSymbol symName, const char *pchValue ) OVERRIDE;
	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

	// One no-op backs every registered method name.
	void JSNoop() {}
	CUtlString JSGetEmpty() const { return CUtlString( "" ); }
	void JSSetSwallow( CUtlString ) {}
};

//-----------------------------------------------------------------------------
// Purpose: the two sliders that live inside the item preview page, plus the debug panel.
//          CS:GO: CUI_ItemPreviewSlider / CUI_ItemPreviewColorSlider / CUI_ItemPreviewDebug.
//-----------------------------------------------------------------------------
class CSEPortStub_ItemPreviewSlider : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CSEPortStub_ItemPreviewSlider, panorama::CPanel2D );

public:
	CSEPortStub_ItemPreviewSlider( panorama::CPanel2D *pParent, const char *pchID );
};

class CSEPortStub_ItemPreviewColorSlider : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CSEPortStub_ItemPreviewColorSlider, panorama::CPanel2D );

public:
	CSEPortStub_ItemPreviewColorSlider( panorama::CPanel2D *pParent, const char *pchID );
};

class CSEPortStub_ItemPreviewDebug : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CSEPortStub_ItemPreviewDebug, panorama::CPanel2D );

public:
	CSEPortStub_ItemPreviewDebug( panorama::CPanel2D *pParent, const char *pchID );
};

//-----------------------------------------------------------------------------
// Purpose: the party chat container.  CS:GO: CCSGO_Chat, type "CSGOChat".  It needs the matchmaking
//          framework and the friends-list/lobby UI components, so it is a container here.
//-----------------------------------------------------------------------------
class CSEPortStub_CSGOChat : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CSEPortStub_CSGOChat, panorama::CPanel2D );

public:
	CSEPortStub_CSGOChat( panorama::CPanel2D *pParent, const char *pchID );
};

#endif // SE_PORT_GAMECLIENT_PANEL_STUBS_H
