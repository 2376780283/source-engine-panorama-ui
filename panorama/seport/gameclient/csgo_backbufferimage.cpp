//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: SE port of D:\CSGO2019\game\client\cstrike15\panorama\csgo_backbufferimage.cpp - see the
//          header for what the class is for and for the two Source 2013 adaptations.
//
//=============================================================================//

#include "stdafx_client.h"

#include "seport/gameclient/csgo_backbufferimage.h"

#include "materialsystem/imaterialsystem.h"
#include "materialsystem/itexture.h"
#include "materialsystem/imaterial.h"
#include "panorama/uijsregistration.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

REGISTER_PANEL2D_FACTORY( CCSGO_BackbufferImagePanel, CSGOBackbufferImagePanel )


//-----------------------------------------------------------------------------
// SE port: game/client/view_scene.h's GetFullFrameFrameBufferTexture() is CS:S game client code
// (it keeps a CTextureReference per index).  The engine creates the same render target for every
// mod (see engine/matsys_interface.cpp - "_rt_FullFrameFB"), so look it up through the material
// system instead.
//-----------------------------------------------------------------------------
static ITexture *SE_PortGetFullFrameFrameBufferTexture( int nTextureIndex )
{
	char szName[ 64 ];
	if ( nTextureIndex != 0 )
	{
		V_snprintf( szName, sizeof( szName ), "_rt_FullFrameFB%d", nTextureIndex );
	}
	else
	{
		V_strcpy_safe( szName, "_rt_FullFrameFB" );
	}

	ITexture *pTexture = materials->FindTexture( szName, TEXTURE_GROUP_RENDER_TARGET );
	Assert( pTexture );
	return pTexture;
}

//-----------------------------------------------------------------------------
// SE port: the same code as game/client/view_scene.h::UpdateScreenEffectTexture() - copy a
// rectangle of the current render target into the full frame buffer texture, scaling the
// destination when the texture is smaller than the render target (dx7 era rule, kept for parity).
//-----------------------------------------------------------------------------
static void SE_PortUpdateScreenEffectTexture( int nTextureIndex, int x, int y, int w, int h, Rect_t *pActualRect )
{
	Rect_t srcRect;
	srcRect.x = x;
	srcRect.y = y;
	srcRect.width = w;
	srcRect.height = h;

	CMatRenderContextPtr pRenderContext( materials );
	ITexture *pTexture = SE_PortGetFullFrameFrameBufferTexture( nTextureIndex );
	if ( !pTexture )
		return;

	int nSrcWidth = 0, nSrcHeight = 0;
	pRenderContext->GetRenderTargetDimensions( nSrcWidth, nSrcHeight );
	int nDestWidth = pTexture->GetActualWidth();
	int nDestHeight = pTexture->GetActualHeight();

	Rect_t destRect = srcRect;
	if ( nSrcWidth > nDestWidth || nSrcHeight > nDestHeight )
	{
		// the source and target sizes aren't necessarily the same, so lets figure it out here.
		float scaleX = ( float )nDestWidth / ( float )nSrcWidth;
		float scaleY = ( float )nDestHeight / ( float )nSrcHeight;
		destRect.x = srcRect.x * scaleX;
		destRect.y = srcRect.y * scaleY;
		destRect.width = srcRect.width * scaleX;
		destRect.height = srcRect.height * scaleY;
		destRect.x = clamp( destRect.x, 0, nDestWidth );
		destRect.y = clamp( destRect.y, 0, nDestHeight );
		destRect.width = clamp( destRect.width, 0, nDestWidth - destRect.x );
		destRect.height = clamp( destRect.height, 0, nDestHeight - destRect.y );
	}

	pRenderContext->CopyRenderTargetToTextureEx( pTexture, 0, &srcRect, &destRect );
	pRenderContext->SetFrameBufferCopyTexture( pTexture, nTextureIndex );

	if ( pActualRect )
	{
		*pActualRect = destRect;
	}
}


//-----------------------------------------------------------------------------
//
//	CCSGO_BackbufferImagePanel Methods
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
CCSGO_BackbufferImagePanel::CCSGO_BackbufferImagePanel( panorama::CPanel2D *pParent, const char *pchID )
:
	CRenderPanel( pParent, pchID ),
	m_pRenderer( new CCSGO_BackBufferImageRenderer() )
{
	SetRenderThreadCallback( m_pRenderer );
}


//-----------------------------------------------------------------------------
CCSGO_BackbufferImagePanel::~CCSGO_BackbufferImagePanel()
{}


//-----------------------------------------------------------------------------
//
//	CCSGO_BackBufferImageRenderer Methods
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
CCSGO_BackBufferImageRenderer::CCSGO_BackBufferImageRenderer()
{
	KeyValues *pVMTKeyValues = new KeyValues( "screenspace_general" );
	pVMTKeyValues->SetString( "$PIXSHADER", "unlitgeneric_ps20" );
	pVMTKeyValues->SetString( "$basetexture", "_rt_FullFrameFB" );
	pVMTKeyValues->SetInt( "$COPYALPHA", 1 );
	m_ScreenSpaceMaterial.Init( "PanoramaBackBufferScreenSpace", TEXTURE_GROUP_OTHER, pVMTKeyValues );
	m_ScreenSpaceMaterial->Refresh();
}


//-----------------------------------------------------------------------------
CCSGO_BackBufferImageRenderer::~CCSGO_BackBufferImageRenderer()
{
	// SE port: CS:GO's CMaterialReference::Shutdown( bool ) takes the "delete if unreferenced" flag;
	// Source 2013's takes no argument (CTextureReference is the one with the flag).
	m_ScreenSpaceMaterial.Shutdown();
}


//-----------------------------------------------------------------------------
void CCSGO_BackBufferImageRenderer::RenderThreadCallback( Vector4D *pScissorRect, float x0, float y0, float x1, float y1, bool bEnableSSAA )
{
	CMatRenderContextPtr pRenderContext( materials );

	int nLeft = RoundFloatToInt( x0 );
	int nRight = RoundFloatToInt( x1 );
	nLeft = Max( 0, nLeft );
	nRight = Max( nLeft, nRight );

	int nTop = RoundFloatToInt( y0 );
	int nBottom = RoundFloatToInt( y1 );
	nTop = Max( 0, nTop );
	nBottom = Max( nTop, nBottom );

	int nRenderWidth = nRight - nLeft;
	int nRenderHeight = nBottom - nTop;
	if ( nRenderWidth <= 0 || nRenderHeight <= 0 )
		return;

	//
	// Copy back buffer to "_rt_FullFrameFB"
	//

	ITexture *pSaveRenderTarget = pRenderContext->GetRenderTarget();
	pRenderContext->SetRenderTarget( NULL );	// set to the back buffer

	Rect_t actualRect;
	SE_PortUpdateScreenEffectTexture( 0, nLeft, nTop, nRenderWidth, nRenderHeight, &actualRect );
	ITexture *pRtFullFrame = SE_PortGetFullFrameFrameBufferTexture( 0 );
	if ( !pRtFullFrame )
	{
		pRenderContext->SetRenderTarget( pSaveRenderTarget );
		return;
	}

	pRenderContext->SetRenderTarget( pSaveRenderTarget );


	//
	// Draw "_rt_FullFrameFB" to the active render target using screenspace_general shader
	//

	pRenderContext->Viewport( nLeft, nTop, nRenderWidth, nRenderHeight );

	// SE port: Source 2013 has a single scissor rect (IMatRenderContext::SetScissorRect) rather than
	// CS:GO's Push/PopScissorRect stack, and the panorama wrapper clears it before every mesh draw -
	// the viewport above is what actually confines this draw.
	if ( pScissorRect )
	{
		pRenderContext->SetScissorRect( (int)pScissorRect->x, (int)pScissorRect->y,
			(int)( pScissorRect->x + pScissorRect->z ), (int)( pScissorRect->y + pScissorRect->w ), true );
	}

	// Ensure alpha is set to 0xff - screenspace_general is not writing to alpha
	pRenderContext->ClearColor4ub( 0, 0, 0, 255 );
	pRenderContext->ClearBuffers( true, false );

	pRenderContext->DrawScreenSpaceRectangle( m_ScreenSpaceMaterial, nLeft, nTop, nRenderWidth, nRenderHeight,
		actualRect.x, actualRect.y, actualRect.x + actualRect.width - 1, actualRect.y + actualRect.height - 1,
		pRtFullFrame->GetActualWidth(), pRtFullFrame->GetActualHeight() );

	if ( pScissorRect )
	{
		pRenderContext->SetScissorRect( 0, 0, 0, 0, false );
	}
}
