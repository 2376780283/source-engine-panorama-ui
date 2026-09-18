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

//-----------------------------------------------------------------------------
// SE port: see IRenderContext::CopyBackBufferToTexture.  Called before a backdrop-blur layer binds its
// own target, while the back buffer is still the current render target.
//-----------------------------------------------------------------------------
void CRenderContext::CopyBackBufferToTexture( HRenderTexture hDest )
{
	if ( !hDest.IsValid() )
		return;

	S1Wrapper_Texture_t *pTexture = (S1Wrapper_Texture_t *)hDest.GetResourceHandle()->m_handle;
	if ( !pTexture )
		return;

	ITexture *pS1Texture = pTexture->GetS1Texture();
	if ( !pS1Texture )
		return;

	m_pMatRenderContext->CopyRenderTargetToTexture( pS1Texture );
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

// SE port (bring-up diagnostic, 2026-09-17): draw every panorama quad with the opaque blend state.
// In this wrapper the blend state lives in the material's $blendstate (the panorama/panoramafancy
// shaders map it to a BlendFunc in SHADOW_STATE), so forcing BLENDSTATE_MIX_OPAQUE here is the same
// experiment as OverrideBlend(true) + BlendFunc(SHADER_BLEND_ONE, SHADER_BLEND_ZERO) around the
// panorama pass: if the main menu's washed-out look goes away with it on, the problem is the blend
// state of the pass, not the content.  No FCVAR_DEVELOPMENTONLY: release builds hide those.
static ConVar s_convarSEPortOpaqueBlend( "se_port_opaque_blend", "0" );

// SE port: the attributes the shader reads back through $renderattr live here rather than in the render
// context, because the material (which holds the var) outlives the context.  See irendercontext.h.
CRenderAttributes CRenderContext::m_apSEAttrStore[ SE_ATTR_STORE_COUNT ];
bool CRenderContext::m_abSEAttrStoreValid[ SE_ATTR_STORE_COUNT ] = { false, false };
int				CRenderContext::m_nScissorRects = 0;
ResourceData_t	CRenderContext::m_backBufferResourceData = { 0, RESOURCE_TYPE_BACKBUFFER };
HRenderTexture	CRenderContext::m_hCurrentRT = &CRenderContext::m_backBufferResourceData;

bool CRenderContext::UpdateMaterial()
{
	// Decide which material we'll use

	// SE port (bring-up diagnostic): force the opaque blend state when se_port_opaque_blend is on.
	const int nBlendIndex = s_convarSEPortOpaqueBlend.GetBool() ? (int)BLENDSTATE_MIX_OPAQUE : (int)m_blendState;

	if ( m_nPanMaterial == PANORAMA_MATERIAL )
	{
		m_pMaterial = m_apPanMaterial[nBlendIndex];
	}
	else
	{
		// SE port: per-texType routing to a $srgbread 0 material (YUV video exemption, CS:GO
		// PanDxSetTexturesFancy semantics) was tried and ROLLED BACK - regular draws regressed to
		// washed-out (pitfalls P104).  All fancy draws use the sRGB-read material again.
		m_pMaterial = m_apFancyMaterial[nBlendIndex];
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

	// SE port (bring-up aid, 2026-09-17 "washed out main menu"): list, in submission order, every draw
	// that lands on one of two sample points of the client area - 300,200 sits in the flat pale field of
	// the news panel and 1000,300 sits on the background video - together with everything that decides
	// the pixel it leaves behind: the device rect, the blend state the shader will program (the material
	// index, see s1wrapperRenderAttributes.h) and the two gradient stops, which for the fancy quad
	// material are vertex colour 0 and 1 (panoramafancy_vs30.fxc) and are already premultiplied and in
	// linear space by the time they get here.  Walking this stack shows which draw paints the pale film
	// instead of having to bisect the layout.
	{
		static int s_nSECtxDrawLogged = 0;
		s_nSECtxDrawLogged++;

		if ( s_nSECtxDrawLogged <= 6000 && m_pBaseVB && m_nVertCount >= 3 )
		{
			// the geometry is position + N texcoords, and only the fancy quad material carries colours
			const int nVertexStride = ( m_nPanMaterial == PANORAMA_MATERIAL_FANCYQUAD ) ? 6 : 4;
			const Vector4D *pV = (const Vector4D *)m_pBaseVB;

			float flMinX = 1e30f, flMaxX = -1e30f, flMinY = 1e30f, flMaxY = -1e30f;
			for ( int i = 0; i < m_nVertCount; ++i )
			{
				const Vector4D &v = pV[ i * nVertexStride ];
				flMinX = MIN( flMinX, v.x ); flMaxX = MAX( flMaxX, v.x );
				flMinY = MIN( flMinY, v.y ); flMaxY = MAX( flMaxY, v.y );
			}

			int nVX = 0, nVY = 0, nVW = 0, nVH = 0;
			m_pMatRenderContext->GetViewport( nVX, nVY, nVW, nVH );

			ITexture *pRT = m_pMatRenderContext->GetRenderTarget();
			const int nRTW = pRT ? pRT->GetActualWidth() : 0;
			const int nRTH = pRT ? pRT->GetActualHeight() : 0;

			// SE port (bring-up aid): the render target and the bound texture name identify *which* panel
			// is being drawn - a layer's own render target and a blur scratch texture have their names on
			// them, so a full-screen draw that comes out white can be traced to the surface it samples.
			char szRTName[ 64 ];
			V_strcpy_safe( szRTName, "-" );
			if ( pRT )
			{
				const uintp nRTBits = (uintp)pRT;
				if ( nRTBits >= 0x10000 && nRTBits < 0x7FFF0000 && ( nRTBits & 3 ) == 0 )
				{
					V_strncpy( szRTName, pRT->GetName(), sizeof( szRTName ) - 1 );
					szRTName[ sizeof( szRTName ) - 1 ] = 0;
				}
			}

			char szTex0[ 64 ];
			V_strcpy_safe( szTex0, "-" );
			char szTexExtra[ 192 ];
			V_strcpy_safe( szTexExtra, "-" );
			int nProbeTexType = 0, nProbeBlur = 0, nProbeFastBlur = 0, nProbeDownsample = 0, nProbePremul = 0, nProbeGradComplex = 0, nProbeGrad2Stop = 0;
			bool bAttrUsable = false;
			if ( m_pAttr )
			{
				const uintp nAttrBits = (uintp)m_pAttr;
				bAttrUsable = ( nAttrBits >= 0x10000 && nAttrBits < 0x7FFF0000 && ( nAttrBits & 3 ) == 0 );
			}
			if ( bAttrUsable )
			{
				// SE port (bring-up aid): a texture dropped on the floor is *not* a black pixel in D3D9 - an
				// unbound sampler reads 1,1,1,1, so a draw that samples a texture the port failed to bind
				// comes out solid white.  Print every slot the shaders can sample, so a white full-screen
				// draw can be told apart from "the sampler was never bound".
				auto SE_ProbeTex = [ & ]( RenderAttrTexture_t nAttr, char *pOut, int nOut ) -> bool
				{
					ITexture *pTex = NULL;
					m_pAttr->GetValue( &pTex, nAttr );
					const uintp nBits = (uintp)pTex;
					if ( nBits >= 0x10000 && nBits < 0x7FFF0000 && ( nBits & 3 ) == 0 )
					{
						V_strncpy( pOut, pTex->GetName(), nOut - 1 );
						pOut[ nOut - 1 ] = 0;
						return true;
					}
					V_strncpy( pOut, pTex ? "<bad-ptr>" : "-", nOut );
					return false;
				};

				SE_ProbeTex( ATTR_Texture0, szTex0, sizeof( szTex0 ) );

				char szExtra1[ 64 ], szExtra2[ 64 ], szOutColor[ 64 ], szOutInterm[ 64 ], szOutDepth[ 64 ];
				SE_ProbeTex( ATTR_Texture1, szExtra1, sizeof( szExtra1 ) );
				SE_ProbeTex( ATTR_Texture2, szExtra2, sizeof( szExtra2 ) );
				SE_ProbeTex( ATTR_Panorama_OutputColor, szOutColor, sizeof( szOutColor ) );
				SE_ProbeTex( ATTR_Panorama_OutputIntermediate, szOutInterm, sizeof( szOutInterm ) );
				SE_ProbeTex( ATTR_Panorama_OutputDepth, szOutDepth, sizeof( szOutDepth ) );
				V_sprintf_safe( szTexExtra, "tex1=[%s] tex2=[%s] outColor=[%s] outInterm=[%s] outDepth=[%s]",
					szExtra1, szExtra2, szOutColor, szOutInterm, szOutDepth );

				nProbeTexType = m_pAttr->GetValue( ATTR_D_TEXTURETYPE );
				nProbeBlur = m_pAttr->GetValue( ATTR_D_TEX2DBLUR );
				nProbeFastBlur = m_pAttr->GetValue( ATTR_D_TEX2DFASTBLUR );
				nProbeDownsample = m_pAttr->GetValue( ATTR_D_TEX2DDOWNSAMPLE );
				nProbePremul = m_pAttr->GetValue( ATTR_D_PREMULTIPLY_ALPHA );
				nProbeGradComplex = m_pAttr->GetValue( ATTR_D_GRADIENT_COMPLEX );
				nProbeGrad2Stop = m_pAttr->GetValue( ATTR_D_GRADIENT_TWOSTOP );
			}

			// clip -> device (y flips)
			const float flDevX0 = ( flMinX * 0.5f + 0.5f ) * nVW + nVX;
			const float flDevX1 = ( flMaxX * 0.5f + 0.5f ) * nVW + nVX;
			const float flDevY0 = ( 0.5f - flMaxY * 0.5f ) * nVH + nVY;
			const float flDevY1 = ( 0.5f - flMinY * 0.5f ) * nVH + nVY;

			const bool bBigDraw = ( flDevX1 - flDevX0 ) >= 1200.0f && ( flDevY1 - flDevY0 ) >= 600.0f;

			static const float s_flPts[ 2 ][ 2 ] = { { 300.0f, 200.0f }, { 1000.0f, 300.0f } };

			// Only the draws that land on the surface itself are interesting.  A panel with its own layer
			// render target draws in that target's coordinate space and is composited later, by its own
			// draw on the surface - so a smaller viewport can never be read as "this pixel of the screen".
			const bool bSurfaceRT = ( nRTW >= 1200 || nRTW == 0 ) && nVW >= 1200;

			int nHit = -1;
			if ( bSurfaceRT )
			{
				for ( int i = 0; i < 2 && nHit < 0; ++i )
				{
					if ( s_flPts[ i ][ 0 ] >= flDevX0 && s_flPts[ i ][ 0 ] <= flDevX1 &&
						 s_flPts[ i ][ 1 ] >= flDevY0 && s_flPts[ i ][ 1 ] <= flDevY1 )
					{
						nHit = i;
					}
				}
			}

			if ( nHit >= 0 || ( bBigDraw && s_nSECtxDrawLogged <= 1400 ) )
			{
				const Vector4D &vColor0 = pV[ 1 ];
				const Vector4D &vColor1 = pV[ nVertexStride == 6 ? 2 : 1 ];

				// what is already in the render target at that pixel: the result of everything drawn below
				// this one - and, for the first draw of the frame, the backdrop the engine handed panorama.
				// A backdrop of ~255 here is what makes every translucent panorama draw come out pale.
				unsigned char rgbaPre[4] = { 0, 0, 0, 0 };
				int nPrePt = nHit;
				if ( nPrePt < 0 && s_flPts[ 0 ][ 0 ] >= flDevX0 && s_flPts[ 0 ][ 0 ] <= flDevX1 &&
					 s_flPts[ 0 ][ 1 ] >= flDevY0 && s_flPts[ 0 ][ 1 ] <= flDevY1 )
				{
					nPrePt = 0;		// the full-screen draws cover the sample point too, so read it for them
				}
				if ( nPrePt >= 0 )
				{
					m_pMatRenderContext->ReadPixels( (int)s_flPts[ nPrePt ][ 0 ], (int)s_flPts[ nPrePt ][ 1 ], 1, 1, rgbaPre, IMAGE_FORMAT_RGBA8888 );
				}

				FILE *fp = fopen( "D:\\cstrike\\se_ui_probe.txt", "a" );
				if ( fp )
				{
					fprintf( fp, "DRAW #%d pt=%d blend=%d mat=%d rt=%dx%d[%s] vp=%d,%d %dx%d dev=%.0f,%.0f..%.0f,%.0f (%.0fx%.0f) pre=(%d,%d,%d,%d) col0=(%.3f,%.3f,%.3f,%.3f) col1=(%.3f,%.3f,%.3f,%.3f) texType=%d tex0=[%s] blur=%d fast=%d ds=%d prem=%d gradC=%d grad2=%d %s\n",
						s_nSECtxDrawLogged, nHit, (int)m_blendState, m_nPanMaterial,
						nRTW, nRTH, szRTName, nVX, nVY, nVW, nVH,
						flDevX0, flDevY0, flDevX1, flDevY1, flDevX1 - flDevX0, flDevY1 - flDevY0,
						rgbaPre[0], rgbaPre[1], rgbaPre[2], rgbaPre[3],
						vColor0.x, vColor0.y, vColor0.z, vColor0.w,
						vColor1.x, vColor1.y, vColor1.z, vColor1.w,
						nProbeTexType, szTex0, nProbeBlur, nProbeFastBlur, nProbeDownsample, nProbePremul,
						nProbeGradComplex, nProbeGrad2Stop, szTexExtra );
					fflush( fp );
					fclose( fp );
				}
			}
		}
	}

	// Update material ( anything we push via the material rather than src1 context e.g. blendstate)
	if ( !UpdateMaterial() )
		return;

	m_pMatRenderContext->Bind( m_pMaterial, NULL );

	// SE port (bring-up aid): the material system's scissor rect is global state, and the panorama pass
	// runs right after the VGUI/HUD pass - anything still enabled there would clip our quad away.
	// The panorama surface does its own clipping, so drop the scissor for this draw.
	m_pMatRenderContext->SetScissorRect( 0, 0, 0, 0, false );

	// Finalise mesh building
	// SE port: the material has to be handed to GetDynamicMesh() as pAutoBind - that is what makes the
	// dynamic mesh adopt *this* material's vertex format (position + 5 texcoords for fancy quads).
	// Without it the mesh keeps whatever format the previous draw left behind, and CMeshBuilder's writes
	// end up in the wrong fields, so the quads are built from garbage and never rasterize.
	IMesh* pMesh = m_pMatRenderContext->GetDynamicMesh( true, NULL, NULL, m_pMaterial );

	// SE port (bring-up aid): show the format the mesh actually uses.
	{
		static int s_nSEMeshProbe = 0;
		if ( s_nSEMeshProbe < 4 )
		{
			s_nSEMeshProbe++;
		}
	}
	UpdateMesh( pMesh );

	pMesh->Draw();

	// SE port (bring-up aid): read one pixel back from just inside the quad we submitted.  This tells
	// "the draw landed on the back buffer" (then something later in the frame paints over it) apart from
	// "the draw never reached D3D at all" - which the counters alone cannot distinguish.
	{
		static int s_nSEReadbackProbe = 0;
		if ( s_nSEReadbackProbe < 4 && m_pBaseVB && m_nVertCount >= 3 )
		{
			s_nSEReadbackProbe++;

			int nVX = 0, nVY = 0, nVW = 0, nVH = 0;
			m_pMatRenderContext->GetViewport( nVX, nVY, nVW, nVH );

			// the vertices are already in clip space (-1..1); map the first one back to a pixel and step
			// a few pixels inside the quad
			const Vector4D *pVerts = (const Vector4D *)m_pBaseVB;
			int nPixelX = nVX + (int)( ( pVerts[0].x * 0.5f + 0.5f ) * (float)nVW ) + 4;
			int nPixelY = nVY + (int)( ( 0.5f - pVerts[0].y * 0.5f ) * (float)nVH ) + 4;

			unsigned char rgba[4] = { 0, 0, 0, 0 };
			m_pMatRenderContext->ReadPixels( nPixelX, nPixelY, 1, 1, rgba, IMAGE_FORMAT_RGBA8888 );

		}
	}
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

