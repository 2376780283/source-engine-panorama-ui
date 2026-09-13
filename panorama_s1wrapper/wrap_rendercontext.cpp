//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================
//
//
//
//==================================================================================================

#include "s1wrapper.h"
// memdbgon must be the last include file in a .cpp file!!!

#include "filesystem.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"
#include "pixelwriter.h"
#include "pngloader.h"
#include "wrap_texture.h"

#include "../panorama/source2/renderer/source2surface.h"

#include <tier0/memdbgon.h>

//--------------------------------------------------------------------------------------------------
// render context
//--------------------------------------------------------------------------------------------------

void CRenderContext::AttachToCurrentThread()
{
}

void CRenderContext::Clear( const Vector4D *pClearColorArray, int nNumColors, int nFlags, int nStencilBitToCheck, int nStencilComparisonValue )
{
	bool bClearColor = ( nFlags & RENDER_CLEAR_FLAGS_CLEAR_COLOR ) ? 1 : 0;
	bool bClearDepth = ( nFlags & RENDER_CLEAR_FLAGS_CLEAR_DEPTH ) ? 1 : 0;
	bool bClearStencil = (nFlags & RENDER_CLEAR_FLAGS_CLEAR_STENCIL) ? 1 : 0;

#if ( PANDX_DRAW )
	if ( g_bPanDx )
	{
		PanDxClearColor( pClearColorArray[ 0 ].x * 255.0, pClearColorArray[ 0 ].y * 255.0, pClearColorArray[ 0 ].z * 255.0, pClearColorArray[ 0 ].w * 255.0 );
		PanDxClearBuffers( bClearColor, bClearDepth, bClearStencil );
	}
	else
#endif
	{
		m_pMatRenderContext->ClearColor4ub( pClearColorArray[ 0 ].x * 255.0, pClearColorArray[ 0 ].y * 255.0, pClearColorArray[ 0 ].z * 255.0, pClearColorArray[ 0 ].w * 255.0 );
		m_pMatRenderContext->ClearBuffers( bClearColor, bClearDepth, bClearStencil );
	}
}

void CRenderContext::SetViewports( int nCount, const RenderViewport_t* pViewports )
{
#if ( PANDX_DRAW )
	if ( g_bPanDx )
	{
		PanDxSetViewPort(pViewports->m_nTopLeftX, pViewports->m_nTopLeftY,
						 pViewports->m_nWidth, pViewports->m_nHeight,
						 pViewports->m_flMinZ, pViewports->m_flMaxZ );
	}
	else
#endif
	{
		m_pMatRenderContext->Viewport( pViewports->m_nTopLeftX, pViewports->m_nTopLeftY,
			pViewports->m_nWidth, pViewports->m_nHeight );

		m_pMatRenderContext->DepthRange( pViewports->m_flMinZ, pViewports->m_flMaxZ );
	}

	// Source2 is always setting the scissor rect with the viewport
	Rect_t rectScissor; // SE port: this tree's Rect_t is a plain aggregate (no 4-arg ctor)
    rectScissor.x = pViewports->m_nTopLeftX;
    rectScissor.y = pViewports->m_nTopLeftY;
    rectScissor.width = pViewports->m_nWidth;
    rectScissor.height = pViewports->m_nHeight;
	SetScissorRect( rectScissor );
}

void CRenderContext::GetViewport( RenderViewport_t *pViewport, int nViewport )
{
#if ( PANDX_DRAW )
	if ( g_bPanDx )
	{
		PanDxGetViewPort( pViewport->m_nTopLeftX, pViewport->m_nTopLeftY,
			pViewport->m_nWidth, pViewport->m_nHeight,
			pViewport->m_flMinZ, pViewport->m_flMaxZ );
	}
	else
#endif
	{
		int nViewportX, nViewportY, nViewportWidth, nViewportHeight;
		m_pMatRenderContext->GetViewport( nViewportX, nViewportY, nViewportWidth, nViewportHeight );

		pViewport->m_nTopLeftX = nViewportX;
		pViewport->m_nTopLeftY = nViewportY;
		pViewport->m_nWidth = nViewportWidth;
		pViewport->m_nHeight = nViewportHeight;

		pViewport->m_flMinZ = 0.0f;
		pViewport->m_flMaxZ = 1.0f;				// Can't query on src1
	}

}

void CRenderContext::PushDebuggerRenderTarget( const RenderTargetDesc_t &renderTargetDesc )
{
	HRenderTexture hNewRT = renderTargetDesc.m_pColorTargets[ 0 ];
	if ( !hNewRT.IsValid() ) return;
	S1Wrapper_Texture_t *pTexture = (S1Wrapper_Texture_t *)hNewRT.GetResourceHandle()->m_handle;
	m_pMatRenderContext->PushRenderTargetAndViewport( pTexture->GetS1Texture() );
	return;
}

void CRenderContext::PopDebuggerRenderTarget()
{
	m_pMatRenderContext->PopRenderTargetAndViewport();
	return;
}

bool CRenderContext::BindRenderTargets( const RenderTargetDesc_t &renderTargetDesc )
{
	// at least one valid color rt
	Assert( renderTargetDesc.m_pColorTargets[0] != RENDER_TEXTURE_HANDLE_INVALID );

	int rtID = 0;

	// No change ?
 	if ( renderTargetDesc.m_pColorTargets[ rtID ] == m_hCurrentRT )
 	{
 		return false;
 	}

	HRenderTexture hNewRT = renderTargetDesc.m_pColorTargets[ rtID ];

	if ( !hNewRT.IsValid() )
		return false;


#if ( PANDX_DRAW )
	if ( g_bPanDx )
	{
		if ( hNewRT.GetResourceHandle()->m_nType != RESOURCE_TYPE_BACKBUFFER )
		{
			S1Wrapper_Texture_t *pWrapperTexture = (S1Wrapper_Texture_t *)hNewRT.GetResourceHandle()->m_handle;
			ITexture* pTexture = pWrapperTexture->GetS1Texture();
			PanDxSetRenderTarget( pTexture );
		}
		else
		{
			PanDxSetRenderTarget( NULL );
		}

		m_hCurrentRT = hNewRT;
		return true;
	}
	else
#endif
	{
		// switching to a non back buffer

		if ( hNewRT.GetResourceHandle()->m_nType != RESOURCE_TYPE_BACKBUFFER )
		{
			// If we are moving away from a non back buffer, we should resolve and pop the RT

			if ( m_hCurrentRT.IsValid() && ( m_hCurrentRT.GetResourceHandle()->m_nType != RESOURCE_TYPE_BACKBUFFER ) )
			{
				m_pMatRenderContext->PopRenderTargetAndViewport();
			}

			// Now push the new RT

			S1Wrapper_Texture_t *pTexture = (S1Wrapper_Texture_t *)hNewRT.GetResourceHandle()->m_handle;
			m_pMatRenderContext->PushRenderTargetAndViewport( pTexture->GetS1Texture() );

			Rect_t scissorRect; // SE port: aggregate Rect_t (no 4-arg ctor in this tree)
                   scissorRect.x = 0;
                   scissorRect.y = 0;
                   scissorRect.width = pTexture->m_textureDesc.m_nWidth;
                   scissorRect.height = pTexture->m_textureDesc.m_nHeight;
			SetScissorRects( 1, &scissorRect );

			m_hCurrentRT = hNewRT;
			return true;
		}
		else // Switching back to backbuffer
		{
			if ( m_hCurrentRT.IsValid() && ( m_hCurrentRT.GetResourceHandle()->m_nType == RESOURCE_TYPE_BACKBUFFER ) )
			{
				return false;
			}

			m_pMatRenderContext->PopRenderTargetAndViewport();
			m_hCurrentRT = hNewRT;
			return true;
		}

	}


}

void CRenderContext::SetTextureData( HRenderTexture hTexture, const CTextureDesc *pDataDesc, 
									 const void *pData, int nDataSize, 
									 bool bIsPreTiled, int nSpecificMipLevelToSet, 
									 Rect3D_t const *pSubRectToUpdate, 
									 uint32 nTextureUpdateFlags, const DataRecycleDelegate_t *pDataRecycleDelegate )
{
	Rect3D_t nullrect;

	nullrect.x = nullrect.y = nullrect.z = 0;
	nullrect.depth = 1;

	nullrect.width = pDataDesc->m_nWidth;
	nullrect.height = pDataDesc->m_nHeight;

	if ( pSubRectToUpdate == nullptr )
	{
		pSubRectToUpdate = &nullrect;
	}

	S1Wrapper_Texture_t *pTexture = (S1Wrapper_Texture_t *)hTexture.GetResourceHandle()->m_handle;
	pTexture->SetTextureData( nullptr, pDataDesc->m_nImageFormat, pData, nDataSize, pSubRectToUpdate, pDataRecycleDelegate );
}


void CRenderContext::UpdateMesh( IMesh* pMesh )
{
	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, m_nVertCount / 3 );

	int nNumTextCoord = ( m_nPanMaterial == PANORAMA_MATERIAL_FANCYQUAD ) ? 5 : 3;

	Vector4D *pBase = (Vector4D*)m_pBaseVB;
	
	for ( int i = 0; i < m_nVertCount; i++ )
	{
		meshBuilder.Position3fv( (float*)pBase );
		pBase++;
		for ( int j = 0; j < nNumTextCoord; j++ )
		{
			meshBuilder.TexCoord4fv( j, (float*)pBase );
			pBase++;
		}
		meshBuilder.AdvanceVertex();

	}

	meshBuilder.End();

}

IMaterial* CRenderContext::m_apPanMaterial[] = { 0, };
IMaterial* CRenderContext::m_apFancyMaterial[] = { 0, };
int				CRenderContext::m_nScissorRects = 0;
ResourceData_t	CRenderContext::m_backBufferResourceData = { 0, RESOURCE_TYPE_BACKBUFFER };
HRenderTexture	CRenderContext::m_hCurrentRT = &CRenderContext::m_backBufferResourceData;

bool CRenderContext::UpdateMaterial()
{
	// Decide which material we'll use

	if ( m_nPanMaterial == PANORAMA_MATERIAL )
	{
		m_pMaterial = m_apPanMaterial[m_blendState];
	}
	else
	{
		m_pMaterial = m_apFancyMaterial[m_blendState];
	}

	// SE port: the "panorama" / "panoramafancy" shaders are CS:GO stdshader classes that have not
	// been ported to this tree yet, so CreateMaterial() hands us its error material and
	// "$renderattr" does not exist on it.  Bail out here instead of dereferencing a NULL
	// IMaterialVar (the old Assert() was compiled out in release builds -> hard crash).
	if ( !m_pMaterial || m_pMaterial->IsErrorMaterial() )
	{
		static bool bWarnedOnce = false;
		if ( !bWarnedOnce )
		{
			bWarnedOnce = true;
			Warning( "Panorama: the 'panorama'/'panoramafancy' shaders are not compiled into this "
					 "stdshader_dx9.dll yet - panorama draws are skipped, the game keeps running.\n" );
		}

		m_pMaterial = NULL;
		return false;
	}

	// Set the $renderattr
	
	bool bFound = false;
	IMaterialVar *pVar = m_pMaterial->FindVar( "$renderattr", &bFound );
	if ( !bFound || !pVar )
	{
		m_pMaterial = NULL;
		return false;
	}

#ifdef PLATFORM_64BITS
	intptr_t val = (intptr_t)m_pAttr;
	pVar->SetIntValue( val & 0xffffffff );
	
	bFound = false;
	pVar = m_pMaterial->FindVar( "$renderattr_high", &bFound );
	if ( bFound && pVar )
	{
		pVar->SetIntValue( ( val >> 32 ) & 0xffffffff );
	}
#else
	pVar->SetIntValue( uintp( m_pAttr ) );
#endif

	return true;
}


void CRenderContext::CtxDraw( RenderPrimitiveType_t type, int nFirstVertex, int nVertexCount )
{
	if ( type != RENDER_PRIM_TRIANGLES ) Error( "Panorama : Invalid prim type\n" );

	// SE port (bring-up aid): is panorama reaching the draw path at all?
	{
		static int s_nSECtxDrawLogged = 0;
		s_nSECtxDrawLogged++;
		if ( ( s_nSECtxDrawLogged % 120 ) == 1 )
		{
			ITexture *pProbeRT = m_pMatRenderContext->GetRenderTarget();
			int nProbeVX = 0, nProbeVY = 0, nProbeVW = 0, nProbeVH = 0;
			m_pMatRenderContext->GetViewport( nProbeVX, nProbeVY, nProbeVW, nProbeVH );

			Vector4D *pV = (Vector4D*)m_pBaseVB;
			Warning( "SE_PORT_DRAW: CtxDraw #%d verts=%d panMaterial=%d blend=%d rt=%s viewport=%d,%d %dx%d vb=%p v0=(%.2f,%.2f,%.2f,%.2f) v1=(%.2f,%.2f,%.2f,%.2f)\n",
				s_nSECtxDrawLogged, m_nVertCount, m_nPanMaterial, (int)m_blendState,
				pProbeRT ? pProbeRT->GetName() : "BACKBUFFER", nProbeVX, nProbeVY, nProbeVW, nProbeVH,
				m_pBaseVB,
				pV ? pV[0].x : 0.0f, pV ? pV[0].y : 0.0f, pV ? pV[0].z : 0.0f, pV ? pV[0].w : 0.0f,
				pV ? pV[1].x : 0.0f, pV ? pV[1].y : 0.0f, pV ? pV[1].z : 0.0f, pV ? pV[1].w : 0.0f );
		}
	}

	// Update material ( anything we push via the material rather than src1 context e.g. blendstate)
	if ( !UpdateMaterial() )
		return;

	m_pMatRenderContext->Bind( m_pMaterial, NULL );

	// Finalise mesh building
	IMesh* pMesh = m_pMatRenderContext->GetDynamicMesh( true );
	UpdateMesh( pMesh );

	pMesh->Draw();
}

void CRenderContext::SetCullMode( RenderCullMode_t eCullMode )
{
#if ( PANDX_DRAW )
	if ( g_bPanDx ) return;
#endif

	// Only ever set to NONE
    // SE port: this engine's MaterialCullMode_t has no NONE value.  Panorama only ever
    // asks for "no culling" (MATERIAL_CULLMODE_NONE in CS:GO), which means "leave the
    // current cull state alone", so only the CW case is forwarded.  (CS:GO's own code has
    // a duplicated CULL_BACKFACING test, making its CCW branch dead.)
    if ( eCullMode == RENDER_CULLMODE_CULL_BACKFACING )
    {
            m_pMatRenderContext->CullMode( MATERIAL_CULLMODE_CW );
    }
}

// void CRenderContext::SetBlendMode( RenderBlendMode_t eBlendMode, float const *pBlendFactor)
// {
// }

void CRenderContext::SetZBufferMode( RenderZBufferMode_t eZBufferMode )
{
	// only ever set to NONE so we let the shader do this for now
}

void CRenderContext::SetBlendState( RsBlendStateHandle_t blendState, float const *pBlendFactor, uint32 nSampleMask )
{
	m_blendState = blendState;
}

void CRenderContext::SetScissorRects( int nCount, const Rect_t *pRects )
{

#if ( PANDX_DRAW )
	if ( g_bPanDx )
	{
		PanDxSetScissor( nCount, pRects );
	}
	else
#endif
	{
           // SE port: this engine's IMatRenderContext has no scissor-rect stack - it takes a
           // single rect (left, top, right, bottom) plus an enable flag.  Apply the
           // intersection of the requested rects (nCount == 0 disables the scissor).
           ( void )m_nScissorRects;
           m_nScissorRects = nCount;

           if ( nCount <= 0 )
           {
                   m_pMatRenderContext->SetScissorRect( 0, 0, 0, 0, false );
           }
           else
           {
                   int nLeft = pRects[ 0 ].x;
                   int nTop = pRects[ 0 ].y;
                   int nRight = pRects[ 0 ].x + pRects[ 0 ].width;
                   int nBottom = pRects[ 0 ].y + pRects[ 0 ].height;

                   for ( int i = 1; i < nCount; ++i )
                   {
                           nLeft = Max( nLeft, pRects[ i ].x );
                           nTop = Max( nTop, pRects[ i ].y );
                           nRight = Min( nRight, pRects[ i ].x + pRects[ i ].width );
                           nBottom = Min( nBottom, pRects[ i ].y + pRects[ i ].height );
                   }

                   m_pMatRenderContext->SetScissorRect( nLeft, nTop, nRight, nBottom, true );
           }


}

// SE port: this brace closes CRenderContext::SetScissorRects().  In the CS:GO original the
// Source2 scissor-rect-stack code ended the function body; that code was replaced by the SE
// single-rect implementation above, so the function is closed here.
}

