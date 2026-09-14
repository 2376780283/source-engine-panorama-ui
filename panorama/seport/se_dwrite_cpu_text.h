//========== Copyright (c) Valve Corporation, All rights reserved. ============
//
// SE port: DirectWrite text backend, CPU rasterisation.
//
// CS:GO ships its text services as a separate module (panorama_text_pango.dll,
// panorama_text_base.vpc) which needs pango + glib + cairo + harfbuzz; none of those
// (nor their prebuilt import libraries) exist in this Source Engine 2013 tree.  The
// native Windows backend that is in the tree (text/uitextlayoutwin32.cpp +
// renderer/dwritetextrenderer.cpp) draws through Direct2D render targets that belong to
// CD3D10D2DSurface, i.e. the Source 2 D3D10 renderer - not the s1wrapper/materialsystem
// surface this port renders with.
//
// This file fills the gap: it rasterises a DirectWrite text layout into plain 8-bit
// alpha masks and hands them to the surface through IUITextTextureStorage, exactly the
// way the pango backend does with freetype/cairo.  Only DirectWrite is used, so there is
// no external dependency.
//
// CUITextLayoutWin32 (layout, measurement, formatting, hit testing) stays as-is; only its
// BDraw() is redirected here (see PANORAMA_SE_CPU_TEXT in text/uitextlayoutwin32.cpp).
//
//=============================================================================

#ifndef SE_DWRITE_CPU_TEXT_H
#define SE_DWRITE_CPU_TEXT_H
#pragma once

#include <DWrite.h>

#include "panorama/text/iuitextservices.h"
#include "text/uitextserviceswin32.h"

namespace panorama
{

class CUITextLayoutWin32;

//
// The drawing effect attached to colour ranges: CS:GO's CTextDrawingEffect (declared in
// renderer/dwritetextrenderer.h, implemented in renderer/dwritetextrenderer.cpp) cannot be used here
// because that translation unit needs CD3D10D2DSurface's Direct2D render targets.  The class itself
// is just a ref-counted colour index, so it lives here under a port-specific name.
//
class CSECTextDrawingEffect : public IUnknown
{
public:
	CSECTextDrawingEffect( int iColorIndex );
	~CSECTextDrawingEffect();

	// IUnknown
	STDMETHOD_( unsigned long, AddRef )();
	STDMETHOD( QueryInterface )( IID const &riid, void **ppvObject );
	STDMETHOD_( unsigned long, Release )();

	int GetColorIndex() const { return m_iColorIndex; }

private:
	unsigned long m_cRefCount;
	int m_iColorIndex;
};

//
// The text services singleton the UI engine uses: CUITextServicesWin32 does the work, this adds the
// IAppSystem entry points CS:GO got from CTier3AppSystem (the win32 backend class derives from
// IUITextServices directly, which is an abstract IAppSystem in this tree).
//
class CSEPanoramaTextServicesWin32 : public CUITextServicesWin32
{
public:
	// IAppSystem
	virtual bool Connect( CreateInterfaceFn factory ) OVERRIDE { return true; }
	virtual void Disconnect() OVERRIDE {}
	virtual void *QueryInterface( const char *pInterfaceName ) OVERRIDE
	{
		if ( !V_strncmp( pInterfaceName, PANORAMA_TEXT_SERVICES_INTERFACE_VERSION, V_strlen( PANORAMA_TEXT_SERVICES_INTERFACE_VERSION ) + 1 ) )
			return static_cast< IUITextServices * >( this );

		return NULL;
	}
	virtual InitReturnVal_t Init() OVERRIDE { return INIT_OK; }
	virtual void Shutdown() OVERRIDE {}
};

//
// Rasterise the given layout into opacity masks, uploading each glyph run through pStorage.
// flClipHeight is the height the layout was measured against (text below it is clipped).
//
bool SEDrawTextLayoutToAlphaMasks( CUITextLayoutWin32 *pLayout, CUtlVector<UITextOpacityMaskDataRange_t> &drawRanges,
								   float flClipHeight, IUITextTextureStorage *pStorage );

} // namespace panorama

#endif // SE_DWRITE_CPU_TEXT_H
