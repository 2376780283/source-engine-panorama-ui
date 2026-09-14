//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: SE port of D:\CSGO2019\game\client\cstrike15\panorama\csgo_backbufferimage.{h,cpp}
//          (panel type "CSGOBackbufferImagePanel").
//
//          The CS:GO main menu puts one of these behind the menu (see mainmenu.xml, panel
//          "mainmenu-content__blur-target" / the background image layers) to show a *frozen copy* of
//          the back buffer - the 3D scene behind the UI - inside the layout.  Without the class the
//          layout loader substitutes a plain Panel (panorama/layout/layoutfile.cpp::BAddPanel) and
//          that area stays empty.
//
//          It is a CRenderPanel: the panel rectangle is handed to a render-thread callback, which
//          copies the back buffer into "_rt_FullFrameFB" and draws it back with
//          "screenspace_general".
//
//          Two Source 2013 adaptations (see the .cpp):
//            * game/client/view_scene.h's UpdateScreenEffectTexture()/GetFullFrameFrameBufferTexture()
//              belong to the CS:S game client, which this DLL does not link - reimplemented locally
//              against IMaterialSystem + the well known render target.
//            * IMatRenderContext::PushScissorRect/PopScissorRect do not exist here (single scissor);
//              SetScissorRect() is used instead.
//
//=============================================================================//

#ifndef SE_GAMECLIENT_CSGO_BACKBUFFERIMAGE_H
#define SE_GAMECLIENT_CSGO_BACKBUFFERIMAGE_H
#pragma once

#include "materialsystem/MaterialSystemUtil.h"		// CMaterialReference
#include "panorama/controls/panel2d.h"
#include "panorama/controls/source2/renderpanel.h"


class CCSGO_BackBufferImageRenderer : public panorama::CRenderThreadCallback
{
public:

	CCSGO_BackBufferImageRenderer();
	virtual ~CCSGO_BackBufferImageRenderer();

	virtual void RenderThreadCallback( Vector4D *pScissorRect, float x0, float y0, float x1, float y1, bool bEnableSSAA ) OVERRIDE;

private:

	CMaterialReference  m_ScreenSpaceMaterial;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CCSGO_BackbufferImagePanel : public panorama::CRenderPanel
{
	DECLARE_PANEL2D( CCSGO_BackbufferImagePanel, panorama::CRenderPanel );

public:

	CCSGO_BackbufferImagePanel( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_BackbufferImagePanel();

private:

	CRefPtr< CCSGO_BackBufferImageRenderer > m_pRenderer;
};

#endif	// SE_GAMECLIENT_CSGO_BACKBUFFERIMAGE_H
