//========== Copyright (c) Valve Corporation, All rights reserved. ============
//
// SE port: DirectWrite -> CPU alpha mask text renderer.  See se_dwrite_cpu_text.h for why
// this exists.  This translation unit only needs DirectWrite; the pango backend's texture
// upload protocol (IUITextTextureStorage) is mirrored from text/uitextlayoutpango.cpp.
//
//=============================================================================

#include "stdafx.h"

#include <DWrite.h>

#include "se_dwrite_cpu_text.h"
#include "text/uitextlayoutwin32.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

// Scratch buffer for clearing the padded edges of an atlas region (the padding gives the
// glyph a black border so filtering cannot bleed neighbouring glyphs in).
static byte s_rgbaZeroFillBuffer[4096] = { 0 };

//-----------------------------------------------------------------------------
// Purpose: Rasterise one glyph run into an 8-bit coverage buffer
//-----------------------------------------------------------------------------
static bool SERasterizeGlyphRunToAlpha( IDWriteFactory *pFactory, DWRITE_MEASURING_MODE measuringMode,
										const DWRITE_GLYPH_RUN *pGlyphRun, float flBaselineX, float flBaselineY,
										DWRITE_RENDERING_MODE eRenderMode, DWRITE_TEXTURE_TYPE eTextureType,
										CUtlVector< byte > &vecAlpha, RECT &boundsOut )
{
	DWRITE_MATRIX transform = { 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f };

	IDWriteGlyphRunAnalysis *pAnalysis = NULL;
	HRESULT hr = pFactory->CreateGlyphRunAnalysis( pGlyphRun, 1.0f, &transform, eRenderMode, measuringMode,
											  flBaselineX, flBaselineY, &pAnalysis );
	if ( FAILED( hr ) || !pAnalysis )
		return false;

	bool bOk = false;
	RECT bounds;
	if ( SUCCEEDED( pAnalysis->GetAlphaTextureBounds( eTextureType, &bounds ) ) )
	{
		int nWidth = bounds.right - bounds.left;
		int nHeight = bounds.bottom - bounds.top;
		if ( nWidth > 0 && nHeight > 0 )
		{
			int nBytesPerPixel = ( eTextureType == DWRITE_TEXTURE_CLEARTYPE_3x1 ) ? 3 : 1;
			int nBufferBytes = nWidth * nHeight * nBytesPerPixel;
			byte *pRaster = (byte *)malloc( nBufferBytes );
			if ( pRaster )
			{
				if ( SUCCEEDED( pAnalysis->CreateAlphaTexture( eTextureType, &bounds, pRaster, nBufferBytes ) ) )
				{
					// Always hand the caller 8-bit coverage: ClearType rasterisation gives three
					// sub-pixel channels per pixel, which are averaged here.
					vecAlpha.SetSize( nWidth * nHeight );
					if ( nBytesPerPixel == 3 )
					{
						for ( int iPix = 0; iPix < nWidth * nHeight; ++iPix )
						{
							const byte *pSrc = pRaster + iPix * 3;
							vecAlpha[iPix] = (byte)( ( (int)pSrc[0] + pSrc[1] + pSrc[2] ) / 3 );
						}
					}
					else
					{
						V_memcpy( vecAlpha.Base(), pRaster, nWidth * nHeight );
					}

					boundsOut = bounds;
					bOk = true;
				}

				free( pRaster );
			}
		}
	}

	pAnalysis->Release();
	return bOk;
}


//-----------------------------------------------------------------------------
// Purpose: Rasterise one glyph run, preferring antialiased ClearType output
//-----------------------------------------------------------------------------
static bool SERasterizeGlyphRun( IDWriteFactory *pFactory, DWRITE_MEASURING_MODE measuringMode,
								const DWRITE_GLYPH_RUN *pGlyphRun, float flBaselineX, float flBaselineY,
								CUtlVector< byte > &vecAlpha, RECT &boundsOut )
{
	if ( SERasterizeGlyphRunToAlpha( pFactory, measuringMode, pGlyphRun, flBaselineX, flBaselineY,
									 DWRITE_RENDERING_MODE_CLEARTYPE_NATURAL, DWRITE_TEXTURE_CLEARTYPE_3x1,
									 vecAlpha, boundsOut ) )
	{
		return true;
	}

	// Bitmap-only fonts (and some rendering modes) reject ClearType; fall back to the
	// aliased 1-bit-per-pixel path, which every DirectWrite font supports.
	return SERasterizeGlyphRunToAlpha( pFactory, measuringMode, pGlyphRun, flBaselineX, flBaselineY,
									  DWRITE_RENDERING_MODE_ALIASED, DWRITE_TEXTURE_ALIASED_1x1,
									  vecAlpha, boundsOut );
}


//-----------------------------------------------------------------------------
// Purpose: Renderer that uploads each glyph run as alpha mask
//-----------------------------------------------------------------------------
class CSEDWriteMaskRenderer : public IDWriteTextRenderer
{
public:
	CSEDWriteMaskRenderer( IUITextTextureStorage *pStorage, CUtlVector< UITextOpacityMaskDataRange_t > *pDrawRanges, float flClipHeight )
		: m_cRefCount( 1 ), m_pStorage( pStorage ), m_pDrawRanges( pDrawRanges ), m_flClipHeight( flClipHeight ), m_iColorIndex( -1 )
	{
	}

	// IUnknown
	STDMETHOD_( unsigned long, AddRef )()
	{
		return ++m_cRefCount;
	}

	STDMETHOD( QueryInterface )( IID const &riid, void **ppvObject )
	{
		if ( riid == __uuidof( IUnknown ) || riid == __uuidof( IDWriteTextRenderer ) || riid == __uuidof( IDWritePixelSnapping ) )
		{
			*ppvObject = static_cast< IDWriteTextRenderer * >( this );
			AddRef();
			return S_OK;
		}

		*ppvObject = NULL;
		return E_NOINTERFACE;
	}

	STDMETHOD_( unsigned long, Release )()
	{
		unsigned long cRef = --m_cRefCount;
		if ( cRef == 0 )
			delete this;
		return cRef;
	}

	// IDWritePixelSnapping
	STDMETHOD( IsPixelSnappingDisabled )( void * /*clientDrawingContext*/, BOOL *isDisabled )
	{
		*isDisabled = FALSE;
		return S_OK;
	}

	STDMETHOD( GetCurrentTransform )( void * /*clientDrawingContext*/, DWRITE_MATRIX *transform )
	{
		transform->m11 = 1.0f; transform->m12 = 0.0f; transform->m21 = 0.0f;
		transform->m22 = 1.0f; transform->dx = 0.0f; transform->dy = 0.0f;
		return S_OK;
	}

	STDMETHOD( GetPixelsPerDip )( void * /*clientDrawingContext*/, FLOAT *pixelsPerDip )
	{
		*pixelsPerDip = 1.0f;
		return S_OK;
	}

	// IDWriteTextRenderer
	STDMETHOD( DrawGlyphRun )( void * /*clientDrawingContext*/, FLOAT baselineOriginX, FLOAT baselineOriginY,
							  DWRITE_MEASURING_MODE measuringMode, DWRITE_GLYPH_RUN const *glyphRun,
							  DWRITE_GLYPH_RUN_DESCRIPTION const * /*glyphRunDescription*/, IUnknown *clientDrawingEffect );

	STDMETHOD( DrawUnderline )( void * /*clientDrawingContext*/, FLOAT baselineOriginX, FLOAT baselineOriginY,
							   DWRITE_UNDERLINE const *underline, IUnknown *clientDrawingEffect );

	STDMETHOD( DrawStrikethrough )( void * /*clientDrawingContext*/, FLOAT baselineOriginX, FLOAT baselineOriginY,
								   DWRITE_STRIKETHROUGH const *strikethrough, IUnknown *clientDrawingEffect );

	STDMETHOD( DrawInlineObject )( void * /*clientDrawingContext*/, FLOAT /*originX*/, FLOAT /*originY*/,
								  IDWriteInlineObject * /*inlineObject*/, BOOL /*isSideways*/, BOOL /*isRightToLeft*/,
								  IUnknown * /*clientDrawingEffect*/ )
	{
		// Inline objects are images embedded in the text run; nothing to rasterise here.
		return S_OK;
	}

private:
	HRESULT UploadMask( const byte *pMask, int nWidth, int nHeight, int nMaxHeight, float flOffsetX, float flOffsetY );
	HRESULT UploadSolidRect( float flLeft, float flTop, float flWidth, float flHeight );

	unsigned long m_cRefCount;
	IUITextTextureStorage *m_pStorage;
	CUtlVector< UITextOpacityMaskDataRange_t > *m_pDrawRanges;
	float m_flClipHeight;
	int m_iColorIndex;
};


//-----------------------------------------------------------------------------
// Purpose: Upload one mask into the surface's text atlas and record the draw range
//-----------------------------------------------------------------------------
HRESULT CSEDWriteMaskRenderer::UploadMask( const byte *pMask, int nWidth, int nHeight, int nMaxHeight, float flOffsetX, float flOffsetY )
{
	if ( !pMask || nWidth <= 0 || nHeight <= 0 || !m_pStorage || !m_pDrawRanges )
		return S_OK;

	if ( nMaxHeight > 0 && nHeight > nMaxHeight )
		nHeight = nMaxHeight;

	if ( nHeight <= 0 )
		return S_OK;

	UITextTextureRegion_t region = m_pStorage->GetTextureRegion( nWidth, nHeight );
	if ( !region.m_hTexture )
		return S_OK;

	UITextOpacityMaskDataRange_t &range = ( *m_pDrawRanges )[ m_pDrawRanges->AddToTail() ];
	V_memset( &range, 0, sizeof( range ) );

	range.m_flTextureWidth = region.m_flTextureWidth;
	range.m_flTextureHeight = region.m_flTextureHeight;
	range.m_x0 = (float)( region.m_rect.m_iLeft + region.m_iPadding );
	range.m_y0 = (float)( region.m_rect.m_iTop + region.m_iPadding );
	range.m_x1 = range.m_x0 + (float)nWidth;
	range.m_y1 = range.m_y0 + (float)nHeight;
	range.m_flStringOffsetX = flOffsetX;
	range.m_flStringOffsetY = flOffsetY;
	range.m_iColorIndex = m_iColorIndex;
	range.m_hTexture = region.m_hTexture;

	m_pStorage->StartUpdateFontGlyphTexture( region.m_hTexture );

	// Zero the whole padded region first (black border), then the mask on top of it.
	int nPaddedWidth = region.m_rect.m_iRight - region.m_rect.m_iLeft;
	int nZeroBufferBytes = (int)sizeof( s_rgbaZeroFillBuffer );
	int yStart = region.m_rect.m_iTop;
	int yEnd = region.m_rect.m_iBottom;

	if ( nPaddedWidth >= nZeroBufferBytes )
	{
		while ( yStart < yEnd )
		{
			int xStart = region.m_rect.m_iLeft;
			while ( xStart < region.m_rect.m_iRight )
			{
				int xToDo = MIN( nZeroBufferBytes, region.m_rect.m_iRight - xStart );
				m_pStorage->UpdateFontGlyphTexture( region.m_hTexture, xStart, yStart, xToDo, 1, (void *)s_rgbaZeroFillBuffer );
				xStart += xToDo;
			}
			++yStart;
		}
	}
	else
	{
		int nYPerPass = MAX( 1, nZeroBufferBytes / MAX( 1, nPaddedWidth ) );
		while ( yStart < yEnd )
		{
			int nYToDo = MIN( nYPerPass, yEnd - yStart );
			m_pStorage->UpdateFontGlyphTexture( region.m_hTexture, region.m_rect.m_iLeft, yStart, nPaddedWidth, nYToDo, (void *)s_rgbaZeroFillBuffer );
			yStart += nYToDo;
		}
	}

	m_pStorage->UpdateFontGlyphTexture( region.m_hTexture, region.m_rect.m_iLeft + region.m_iPadding,
									   region.m_rect.m_iTop + region.m_iPadding, nWidth, nHeight, (void *)pMask );
	m_pStorage->EndUpdateFontGlyphTexture( region.m_hTexture );

	return S_OK;
}


//-----------------------------------------------------------------------------
// Purpose: Upload an axis aligned solid rect (underline / strikethrough)
//-----------------------------------------------------------------------------
HRESULT CSEDWriteMaskRenderer::UploadSolidRect( float flLeft, float flTop, float flWidth, float flHeight )
{
	int nWidth = (int)ceilf( flWidth );
	int nHeight = MAX( 1, (int)ceilf( flHeight ) );
	if ( nWidth <= 0 || nHeight <= 0 )
		return S_OK;

	int nBytes = nWidth * nHeight;
	CUtlVector< byte > vecSolid;
	vecSolid.SetSize( nBytes );
	V_memset( vecSolid.Base(), 0xFF, nBytes );

	return UploadMask( vecSolid.Base(), nWidth, nHeight, -1, flLeft, flTop );
}


//-----------------------------------------------------------------------------
// Purpose: Rasterise a glyph run
//-----------------------------------------------------------------------------
STDMETHODIMP CSEDWriteMaskRenderer::DrawGlyphRun( void * /*clientDrawingContext*/, FLOAT baselineOriginX, FLOAT baselineOriginY,
												 DWRITE_MEASURING_MODE measuringMode, DWRITE_GLYPH_RUN const *glyphRun,
												 DWRITE_GLYPH_RUN_DESCRIPTION const * /*glyphRunDescription*/, IUnknown *clientDrawingEffect )
{
	if ( !glyphRun || glyphRun->glyphCount == 0 || !m_pStorage || !m_pDrawRanges )
		return S_OK;

	if ( clientDrawingEffect )
	{
		CSECTextDrawingEffect *pEffect = dynamic_cast< CSECTextDrawingEffect * >( clientDrawingEffect );
		m_iColorIndex = pEffect ? pEffect->GetColorIndex() : UITextOpacityMaskDataRange_t::k_iColorIndexUnset;
	}
	else
	{
		m_iColorIndex = UITextOpacityMaskDataRange_t::k_iColorIndexUnset;
	}

	IDWriteFactory *pFactory = CUITextLayoutWin32::GetDWriteFactory();
	if ( !pFactory )
		return S_OK;

	// Snap to whole pixels: the masks are used 1:1 and it keeps the rasterised bounds integral.
	float flBaselineX = floorf( baselineOriginX );
	float flBaselineY = floorf( baselineOriginY );

	CUtlVector< byte > vecAlpha;
	RECT bounds;
	if ( !SERasterizeGlyphRun( pFactory, measuringMode, glyphRun, flBaselineX, flBaselineY, vecAlpha, bounds ) )
		return S_OK;

	int nWidth = bounds.right - bounds.left;
	int nHeight = bounds.bottom - bounds.top;
	if ( nWidth <= 0 || nHeight <= 0 )
		return S_OK;

	int nMaxHeight = -1;
	if ( m_flClipHeight > 0.0f )
	{
		if ( bounds.top >= (LONG)m_flClipHeight )
			return S_OK;

		nMaxHeight = (int)ceilf( m_flClipHeight - (float)bounds.top );
	}

	UploadMask( vecAlpha.Base(), nWidth, nHeight, nMaxHeight, (float)bounds.left, (float)bounds.top );
	return S_OK;
}


//-----------------------------------------------------------------------------
// Purpose: Draw an underline
//-----------------------------------------------------------------------------
STDMETHODIMP CSEDWriteMaskRenderer::DrawUnderline( void * /*clientDrawingContext*/, FLOAT baselineOriginX, FLOAT baselineOriginY,
												  DWRITE_UNDERLINE const *underline, IUnknown *clientDrawingEffect )
{
	if ( !underline || underline->width <= 0.0f )
		return S_OK;

	if ( clientDrawingEffect )
	{
		CSECTextDrawingEffect *pEffect = dynamic_cast< CSECTextDrawingEffect * >( clientDrawingEffect );
		m_iColorIndex = pEffect ? pEffect->GetColorIndex() : UITextOpacityMaskDataRange_t::k_iColorIndexUnset;
	}

	return UploadSolidRect( floorf( baselineOriginX + underline->offset ), floorf( baselineOriginY ),
							underline->width, MAX( 1.0f, underline->thickness ) );
}


//-----------------------------------------------------------------------------
// Purpose: Draw a strikethrough
//-----------------------------------------------------------------------------
STDMETHODIMP CSEDWriteMaskRenderer::DrawStrikethrough( void * /*clientDrawingContext*/, FLOAT baselineOriginX, FLOAT baselineOriginY,
													  DWRITE_STRIKETHROUGH const *strikethrough, IUnknown *clientDrawingEffect )
{
	if ( !strikethrough || strikethrough->width <= 0.0f )
		return S_OK;

	if ( clientDrawingEffect )
	{
		CSECTextDrawingEffect *pEffect = dynamic_cast< CSECTextDrawingEffect * >( clientDrawingEffect );
		m_iColorIndex = pEffect ? pEffect->GetColorIndex() : UITextOpacityMaskDataRange_t::k_iColorIndexUnset;
	}

	return UploadSolidRect( floorf( baselineOriginX + strikethrough->offset ), floorf( baselineOriginY ),
							strikethrough->width, MAX( 1.0f, strikethrough->thickness ) );
}


//-----------------------------------------------------------------------------
// Purpose: Drawing effect for colour ranges - see the note in the header.
//-----------------------------------------------------------------------------
CSECTextDrawingEffect::CSECTextDrawingEffect( int iColorIndex ) :
	m_cRefCount( 1 ),
	m_iColorIndex( iColorIndex )
{
}


CSECTextDrawingEffect::~CSECTextDrawingEffect()
{
}


STDMETHODIMP_( unsigned long ) CSECTextDrawingEffect::AddRef()
{
	return ++m_cRefCount;
}


STDMETHODIMP_( unsigned long ) CSECTextDrawingEffect::Release()
{
	unsigned long cRef = --m_cRefCount;
	if ( cRef == 0 )
		delete this;

	return cRef;
}


STDMETHODIMP CSECTextDrawingEffect::QueryInterface( IID const &riid, void **ppvObject )
{
	if ( riid == __uuidof( IUnknown ) )
	{
		*ppvObject = static_cast< IUnknown * >( this );
		AddRef();
		return S_OK;
	}

	*ppvObject = NULL;
	return E_NOINTERFACE;
}

//-----------------------------------------------------------------------------
// Purpose: Rasterise the whole layout
//-----------------------------------------------------------------------------
bool panorama::SEDrawTextLayoutToAlphaMasks( CUITextLayoutWin32 *pLayout, CUtlVector< UITextOpacityMaskDataRange_t > &drawRanges,
											 float flClipHeight, IUITextTextureStorage *pStorage )
{
	if ( !pLayout || !pStorage )
		return false;

	IDWriteTextLayout *pTextLayout = pLayout->GetDWriteTextLayout();
	if ( !pTextLayout )
		return false;

	CSEDWriteMaskRenderer renderer( pStorage, &drawRanges, flClipHeight );

	HRESULT hr = pTextLayout->Draw( NULL, &renderer, 0.0f, 0.0f );
	Assert( SUCCEEDED( hr ) );
	return SUCCEEDED( hr );
}
