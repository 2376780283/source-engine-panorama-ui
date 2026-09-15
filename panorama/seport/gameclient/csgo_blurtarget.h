//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: SE port of D:\CSGO2019\game\client\cstrike15\panorama\csgo_blurtarget.{h,cpp}
//          (panel type "CSGOBlurTarget").
//
//          The CS:GO main menu marks the panels it wants blurred with this class and lists the
//          target panels in the "blurrects" attribute, e.g.
//              <CSGOBlurTarget id="MainMenuNavBarLeft" class="..." blurrects="CSGOLoadingScreen">
//          Without the class the layout loader substitutes a plain Panel
//          (panorama/layout/layoutfile.cpp::BAddPanel) and the background stays sharp.
//
//          The blur backend itself is already ported end to end - PushBlurPanels() ->
//          BlurPanelsCommand_t -> ExtractBlurRectangles() -> CSource2Surface blur targets ->
//          the gaussian taps in panorama_ps30.fxc - so this class only has to collect the rectangle
//          list and hand it to AccessRenderEngine().
//
//=============================================================================//

#ifndef SE_GAMECLIENT_CSGO_BLURTARGET_H
#define SE_GAMECLIENT_CSGO_BLURTARGET_H
#pragma once

#include "panorama/controls/panel2d.h"
#include "panorama/iuipanel.h"
#include "panorama/controls/panelptr.h"

class CCSGO_BlurTarget : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CCSGO_BlurTarget, panorama::CPanel2D );

public:
	CCSGO_BlurTarget( panorama::CPanel2D *pParent, const char *pchID );

	virtual void SetupJavascriptObjectTemplate() OVERRIDE;
	virtual void Paint() OVERRIDE;

	// Adding / Removing blur rectangles dynamically
	// (Note that deleted panels will automatically be removed from m_vecBlurRects in Paint())
	void AddBlurPanel( CPanel2D *pPanel );
	void RemoveBlurPanel( CPanel2D *pPanel );

protected:

	bool m_bBlurRectInitialized;
	void GrabRects();

	// List of panels to track. Each panel in that list will correspond to a blur rectangle
	// List populated on the first call to Paint (reading "blurrects" xml attribute)
	// or dynamically by calling AddBlurPanel
	// Every call to CCSGO_BlurTarget::Paint() will check that panels have not been deleted.
	// If a panel has been deleted, we will delete the corresponding entry in m_vecBlurRects.
	CUtlVector< panorama::CPanelPtr< panorama::IUIPanel > > m_vecBlurRects;

	// List of panels used to build the BlurPanelsCommand_t render command
	// Populated in Paint() from m_vecBlurRects (valid panels only)
	CUtlVector<uint64> m_vecPaintBlurRects;
};

#endif // SE_GAMECLIENT_CSGO_BLURTARGET_H
