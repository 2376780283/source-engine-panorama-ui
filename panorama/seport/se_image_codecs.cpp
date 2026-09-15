//========= Copyright Valve Corporation, All rights reserved. ============//
//
// SE port: image codec entry points required by the panorama modules.
//
// CS:GO implements these on top of external libraries that do not exist in the Source Engine 2013
// tree this port targets:
//
//   ConvertJpegToRGBA / BResizeImageRGBA   -> common/jpegloader.cpp + libjpeg  (thirdparty/libjpeg
//                                             is present, but the prebuilt libjpeg.lib CS:GO links
//                                             is not; the in-tree loader also includes the header
//                                             as "jpeglib/jpeglib.h", a path that does not exist)
//   ConvertPNGToRGBA                       -> common/pngloader.cpp + libpng   (same situation)
//
// SVG needs no replacement: common/svg/svgloader.cpp (CS:GO's loader) is compiled into the panorama
// framework as-is, on top of the in-tree parsifal rewrite and the cairo subset implemented by
// panorama/seport/se_cairo_shim/, so ConvertSVGToRGBA() is the real one.  The stub that used to live
// in this file returned false and left every .svg icon in the UI blank.
//
// SE does vendor stb_image / stb_image_resize (thirdparty/stb), so the JPEG/PNG decoders and the
// bilinear resizer are implemented here on top of those.  The public declarations in
// common/jpegloader.h and common/pngloader.h are honoured unchanged, so callers are unaffected.
//
// ============//
#include "tier0/platform.h"
#include "tier0/dbg.h"
#include "tier1/utlbuffer.h"

#include "jpegloader.h"
#include "pngloader.h"

#include <stdlib.h>

// stb_image is header only; the implementation may live in exactly one translation unit.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#include "stb/stb_image.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb/stb_image_resize.h"


//-----------------------------------------------------------------------------
// Purpose: shared decoder used by the JPEG/PNG entry points below
//-----------------------------------------------------------------------------
static bool ConvertToRGBAWithStb( const byte *pubData, int cubData, CUtlBuffer &bufOutput, int &width, int &height, int *pcubUsed )
{
	width = 0;
	height = 0;

	if ( !pubData || cubData <= 0 || !stbi_info_from_memory( pubData, cubData, &width, &height, NULL ) )
		return false;

	int nComponents = 0;
	// Request 4 channels (RGBA) - this matches what the CS:GO loaders produce.
	stbi_uc *pPixels = stbi_load_from_memory( pubData, cubData, &width, &height, &nComponents, 4 );
	if ( !pPixels || width <= 0 || height <= 0 )
	{
		if ( pPixels )
			stbi_image_free( pPixels );
		width = 0;
		height = 0;
		return false;
	}

	const int cbImage = width * height * 4;
	bufOutput.Purge();
	bufOutput.EnsureCapacity( cbImage );
	bufOutput.Put( pPixels, cbImage );

	stbi_image_free( pPixels );

	if ( pcubUsed )
	{
		// stb_image does not report how many source bytes it consumed; the whole buffer is the
		// best (and for every caller in this tree, correct) answer.
		*pcubUsed = cubData;
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Convert Jpeg data to raw RGBA
//-----------------------------------------------------------------------------
bool ConvertJpegToRGBA( const byte *pubJpegData, int cubJpegData, CUtlBuffer &bufOutput, int &width, int &height, int *pcubUsed )
{
	return ConvertToRGBAWithStb( pubJpegData, cubJpegData, bufOutput, width, height, pcubUsed );
}


//-----------------------------------------------------------------------------
// Purpose: Convert PNG data to raw RGBA
//-----------------------------------------------------------------------------
bool ConvertPNGToRGBA( const byte *pubPNGData, int cubPNGData, CUtlBuffer &bufOutput, int &width, int &height )
{
	return ConvertToRGBAWithStb( pubPNGData, cubPNGData, bufOutput, width, height, NULL );
}


//-----------------------------------------------------------------------------
// Purpose: Resize an RGBA image.
//
// Semantics follow common/jpegloader_common.cpp: either nNewWidth or nNewHeight may be -1 (aspect
// ratio is then derived from the other), and when both are given the image is scaled to fit inside
// that box without changing the aspect ratio, with the remainder filled with transparent black.
//-----------------------------------------------------------------------------
bool BResizeImageRGBA( CUtlBuffer &bufRGBA, int nWidth, int nHeight, int &nNewWidth, int &nNewHeight, bool bIsPreMultipledAlpha )
{
	NOTE_UNUSED( bIsPreMultipledAlpha );

	if ( nWidth <= 0 || nHeight <= 0 )
		return false;

	const int cbSource = nWidth * nHeight * 4;
	if ( bufRGBA.TellMaxPut() < cbSource )
		return false;

	// Work out the target box.
	if ( nNewWidth <= 0 && nNewHeight <= 0 )
		return false;

	if ( nNewWidth <= 0 )
		nNewWidth = Max( 1, (int)( (float)nNewHeight * (float)nWidth / (float)nHeight + 0.5f ) );
	if ( nNewHeight <= 0 )
		nNewHeight = Max( 1, (int)( (float)nNewWidth * (float)nHeight / (float)nWidth + 0.5f ) );

	// Fit the source inside the requested box, preserving aspect ratio (letterboxing otherwise).
	const float flScale = Min( (float)nNewWidth / (float)nWidth, (float)nNewHeight / (float)nHeight );
	const int nScaledWidth = Max( 1, Min( nNewWidth, (int)( (float)nWidth * flScale + 0.5f ) ) );
	const int nScaledHeight = Max( 1, Min( nNewHeight, (int)( (float)nHeight * flScale + 0.5f ) ) );

	CUtlBuffer bufScaled;
	bufScaled.EnsureCapacity( nScaledWidth * nScaledHeight * 4 );

	unsigned char *pScaled = (unsigned char *)bufScaled.Base();
	if ( !stbir_resize_uint8( (const unsigned char *)bufRGBA.Base(), nWidth, nHeight, 0,
							  pScaled, nScaledWidth, nScaledHeight, 0, 4 ) )
	{
		return false;
	}
	bufScaled.SeekPut( CUtlBuffer::SEEK_HEAD, nScaledWidth * nScaledHeight * 4 );

	// Compose the final image.
	CUtlBuffer bufOutput;
	const int cbOutput = nNewWidth * nNewHeight * 4;
	bufOutput.EnsureCapacity( cbOutput );
	memset( bufOutput.Base(), 0, cbOutput );

	const int nOffsetX = ( nNewWidth - nScaledWidth ) / 2;
	const int nOffsetY = ( nNewHeight - nScaledHeight ) / 2;
	unsigned char *pOutput = (unsigned char *)bufOutput.Base();
	const unsigned char *pSource = (const unsigned char *)bufScaled.Base();

	for ( int y = 0; y < nScaledHeight; ++y )
	{
		memcpy( pOutput + ( ( y + nOffsetY ) * nNewWidth + nOffsetX ) * 4,
				pSource + ( y * nScaledWidth ) * 4,
				nScaledWidth * 4 );
	}

	bufOutput.SeekPut( CUtlBuffer::SEEK_HEAD, cbOutput );

	bufRGBA.Purge();
	bufRGBA.EnsureCapacity( cbOutput );
	bufRGBA.Put( bufOutput.Base(), cbOutput );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Convert SVG data to raw RGBA.
//
// This is CS:GO's ConvertSVGToRGBA(), in common/svg/svgloader.cpp - which is compiled into this
// framework library, so no implementation belongs here.  It parses the SVG with parsifal (the
// in-tree rewrite the layout loader also uses) and rasterises it through
// panorama/seport/se_cairo_shim/, producing premultiplied ARGB32 that the loader then
// unpremultiplies in place.
//-----------------------------------------------------------------------------
