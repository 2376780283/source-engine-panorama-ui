//========= Copyright (c) Valve Corporation, All rights reserved. ============//
//
// Purpose: SE port - the software rasteriser behind the cairo subset that
//          common/svg/svgloader.cpp (CS:GO's SVG loader, used verbatim here)
//          draws with.
//
//          seport/se_cairo_shim/cairo.h explains why a shim is needed instead of
//          the real library.  This file implements it with libc/libm only, so
//          the SVG loader has no new dependency.
//
//------------------------------------------------------------------------------
// How the loader maps onto this implementation
//------------------------------------------------------------------------------
//  * Path points are transformed into DEVICE space as they are added, exactly
//    like cairo's cairo_path_fixed_t.  Relative commands transform the delta,
//    cairo_get_current_point() returns device coordinates.  Because the path
//    lives in device space, cairo_save()/cairo_restore() deliberately do NOT
//    save/restore it: RenderEllipseElement() brackets its
//    translate/scale/arc with save/restore and then fills the path afterwards,
//    and every other save/restore in the loader is paired with a
//    cairo_new_path()/cairo_clip() (which clears the path, as cairo documents)
//    before the next shape is built.
//  * Filling, stroking and clipping all go through one scanline rasteriser:
//    four sub-scanlines per pixel row and exact horizontal coverage per span.
//  * CAIRO_FORMAT_ARGB32 is premultiplied BGRA in memory on little endian
//    hosts, which is what cairo produces and what ConvertSVGToRGBA() expects:
//    it unpremultiplies that buffer in place before handing the data over as
//    R8G8B8A8.
//  * cairo_push_group()/cairo_pop_group_to_source() use an intermediate surface
//    of the same size as the target, sampled in device space, so
//    cairo_paint_with_alpha() composites the group's opacity exactly.
//  * Strokes are expanded into a filled outline (segment quads + joins + caps)
//    and rasterised with the non-zero rule; the pen is scaled by the CTM like
//    cairo does, but the pen is never sheared/elliptical (an approximation that
//    only shows up under a non-uniform transform with a stroked shape).
//
//=============================================================================//

#include "cairo.h"

#include <math.h>
#include <new>
#include <stdlib.h>
#include <string.h>

//-----------------------------------------------------------------------------
// Tunables
//-----------------------------------------------------------------------------
#define SE_SUBSAMPLES			4		// sub-scanlines per pixel row
#define SE_CURVE_MIN_SEGMENTS	4
#define SE_CURVE_MAX_SEGMENTS	32
#define SE_ARC_SEGMENTS			48		// segments for a full circle
#define SE_ROUND_SEGMENTS		16		// round caps / joins
#define SE_MITER_LIMIT			4.0
#define SE_PI					3.14159265358979323846

#define SE_MAX_PIXELS_PER_RASTER	( 4096 * 4096 )

// Pixels written by the last fill/stroke, reported through
// SE_PortCairoShimLastPaintPixelCount() as a "did anything draw at all" sanity check.
static int g_nLastPaintPixelCount = 0;


//-----------------------------------------------------------------------------
// A minimal growable array.  The shim intentionally depends on nothing but
// libc/libm - no engine headers, no STL - because only the SVG loader uses it.
//-----------------------------------------------------------------------------
template < typename T >
struct SEArray
{
	T *m_pData;
	int m_nCount;
	int m_nAlloc;

	SEArray() : m_pData( NULL ), m_nCount( 0 ), m_nAlloc( 0 ) {}
	~SEArray() { free( m_pData ); }

	int Count() const { return m_nCount; }
	T &operator []( int i ) { return m_pData[i]; }
	const T &operator []( int i ) const { return m_pData[i]; }

	void RemoveAll() { m_nCount = 0; }

	void EnsureCapacity( int n )
	{
		if ( n <= m_nAlloc )
			return;

		int nNew = ( m_nAlloc > 0 ) ? m_nAlloc : 8;
		while ( nNew < n )
			nNew *= 2;

		m_pData = (T *)realloc( m_pData, (size_t)nNew * sizeof( T ) );
		m_nAlloc = nNew;
	}

	T &AddToTail()
	{
		EnsureCapacity( m_nCount + 1 );
		return m_pData[ m_nCount++ ];
	}

private:
	SEArray( const SEArray & );
	SEArray &operator =( const SEArray & );
};


//-----------------------------------------------------------------------------
// Types
//-----------------------------------------------------------------------------
struct SEPoint
{
	double x, y;
};

enum ESEPathItem
{
	SE_PATH_MOVE = 0,
	SE_PATH_LINE,
	SE_PATH_CURVE,
	SE_PATH_CLOSE,
};

struct SEPathItem
{
	int m_Type;
	double m_x[3];
	double m_y[3];
};

struct SEStop
{
	double m_flOffset;
	double m_r, m_g, m_b, m_a;
};

enum ESEPatternType
{
	SE_PATTERN_SOLID = 0,
	SE_PATTERN_LINEAR,
	SE_PATTERN_RADIAL,
	SE_PATTERN_SURFACE,
};

struct _cairo_surface
{
	unsigned char *m_pData;
	int m_nWidth;
	int m_nHeight;
	int m_nStride;
	cairo_format_t m_Format;
	int m_nRefs;
};

struct _cairo_pattern
{
	int m_nRefs;
	ESEPatternType m_Type;

	double m_rgba[4];					// SE_PATTERN_SOLID

	double m_x0, m_y0, m_x1, m_y1;		// SE_PATTERN_LINEAR / SE_PATTERN_RADIAL
	double m_r0, m_r1;

	SEArray<SEStop> m_Stops;
	cairo_extend_t m_Extend;

	cairo_matrix_t m_Matrix;			// user -> device, snapshotted by cairo_set_source
	bool m_bHasMatrix;

	cairo_surface_t *m_pSurface;		// SE_PATTERN_SURFACE (a group)
};

struct SEGState
{
	cairo_matrix_t m_Matrix;
	cairo_operator_t m_Operator;
	cairo_fill_rule_t m_FillRule;
	double m_flLineWidth;
	cairo_line_cap_t m_LineCap;
	cairo_line_join_t m_LineJoin;
	cairo_pattern_t *m_pSource;

	// Per pixel coverage of the clip region (0..1), shared between saved states.
	float *m_pClip;
	int *m_pClipRefs;					// refcount for m_pClip; NULL when there is no clip
};

struct _cairo
{
	cairo_surface_t *m_pTarget;			// where drawing goes right now
	cairo_surface_t *m_pRootTarget;		// the surface the caller created

	SEArray<cairo_surface_t *> m_GroupSurfaces;		// cairo_push_group() stack
	SEArray<SEGState> m_SavedStates;				// cairo_save() stack

	SEGState m_State;

	SEArray<SEPathItem> m_Path;
	bool m_bHasCurrentPoint;
	double m_flCurrentX;
	double m_flCurrentY;

	cairo_status_t m_Status;
	int m_nLastPaintPixels;
};

// Geometry produced by flattening a path: a run of points split into contours.
struct SEFlatPath
{
	SEArray<SEPoint> m_Points;
	SEArray<int> m_Start;				// first point index of each contour
	SEArray<int> m_Closed;

	void RemoveAll() { m_Points.RemoveAll(); m_Start.RemoveAll(); m_Closed.RemoveAll(); }
	int ContourCount() const { return m_Start.Count(); }
	int ContourStart( int i ) const { return m_Start[i]; }
	int ContourEnd( int i ) const { return ( i + 1 < m_Start.Count() ) ? m_Start[i + 1] : m_Points.Count(); }

	int BeginContour( bool bClosed )
	{
		m_Start.AddToTail() = m_Points.Count();
		m_Closed.AddToTail() = bClosed ? 1 : 0;

		// the contour index, so callers can index m_Start / m_Closed with it (ContourStart and
		// ContourEnd take this index, not a point index)
		return m_Start.Count() - 1;
	}
};

struct SEEdge
{
	double x0, y0, x1, y1;
};

struct SECrossing
{
	double x;
	int nDir;
};

static int SECrossingCompare( const void *pA, const void *pB )
{
	double a = ( (const SECrossing *)pA )->x;
	double b = ( (const SECrossing *)pB )->x;
	if ( a < b ) return -1;
	if ( a > b ) return 1;
	return 0;
}


//-----------------------------------------------------------------------------
// Small helpers
//-----------------------------------------------------------------------------
static double SEMin( double a, double b ) { return a < b ? a : b; }
static double SEMax( double a, double b ) { return a > b ? a : b; }

static int SEClamp( int nValue, int nMin, int nMax )
{
	if ( nValue < nMin ) return nMin;
	if ( nValue > nMax ) return nMax;
	return nValue;
}

static bool SEIsFinite( double flValue )
{
	// NaN and infinities all fail this comparison
	return ( flValue >= -1e300 && flValue <= 1e300 );
}

static void SETransformPoint( const cairo_matrix_t *pMatrix, double x, double y, double *pOutX, double *pOutY )
{
	*pOutX = pMatrix->xx * x + pMatrix->xy * y + pMatrix->x0;
	*pOutY = pMatrix->yx * x + pMatrix->yy * y + pMatrix->y0;
}

static void SETransformDistance( const cairo_matrix_t *pMatrix, double x, double y, double *pOutX, double *pOutY )
{
	*pOutX = pMatrix->xx * x + pMatrix->xy * y;
	*pOutY = pMatrix->yx * x + pMatrix->yy * y;
}

// Average scale factor of the CTM, used for line widths and curve tessellation.
static double SEMatrixScale( const cairo_matrix_t *pMatrix )
{
	double x, y;
	SETransformDistance( pMatrix, 1.0, 0.0, &x, &y );
	double flScaleX = sqrt( x * x + y * y );
	SETransformDistance( pMatrix, 0.0, 1.0, &x, &y );
	double flScaleY = sqrt( x * x + y * y );

	if ( !SEIsFinite( flScaleX ) || flScaleX <= 0.0 )
		flScaleX = 1.0;
	if ( !SEIsFinite( flScaleY ) || flScaleY <= 0.0 )
		flScaleY = 1.0;

	return 0.5 * ( flScaleX + flScaleY );
}

static double SEDistance( double x0, double y0, double x1, double y1 )
{
	double dx = x1 - x0;
	double dy = y1 - y0;
	return sqrt( dx * dx + dy * dy );
}


//-----------------------------------------------------------------------------
// Clip masks (per pixel coverage, shared between saved states)
//-----------------------------------------------------------------------------
static void SEClipRelease( float *pClip, int *pRefs )
{
	if ( !pClip || !pRefs )
		return;

	if ( --( *pRefs ) <= 0 )
	{
		free( pRefs );
		free( pClip );
	}
}

static float *SEClipFull( int nWidth, int nHeight, int **ppRefs )
{
	const int nPixels = nWidth * nHeight;
	float *pClip = (float *)malloc( (size_t)nPixels * sizeof( float ) );
	int *pRefs = (int *)malloc( sizeof( int ) );
	if ( !pClip || !pRefs )
	{
		free( pClip );
		free( pRefs );
		*ppRefs = NULL;
		return NULL;
	}

	for ( int i = 0; i < nPixels; ++i )
		pClip[i] = 1.0f;

	*pRefs = 1;
	*ppRefs = pRefs;
	return pClip;
}

static float *SEClipClone( const float *pClip, int *pRefs, int nWidth, int nHeight, int **ppNewRefs )
{
	if ( !pClip )
		return SEClipFull( nWidth, nHeight, ppNewRefs );

	const int nPixels = nWidth * nHeight;
	float *pNew = (float *)malloc( (size_t)nPixels * sizeof( float ) );
	int *pNewRefs = (int *)malloc( sizeof( int ) );
	if ( !pNew || !pNewRefs )
	{
		free( pNew );
		free( pNewRefs );
		*ppNewRefs = NULL;
		return NULL;
	}

	memcpy( pNew, pClip, (size_t)nPixels * sizeof( float ) );
	*pNewRefs = 1;
	*ppNewRefs = pNewRefs;
	return pNew;
}


//-----------------------------------------------------------------------------
// Patterns
//-----------------------------------------------------------------------------
static void SEPatternDestroy( cairo_pattern_t *pPattern )
{
	if ( !pPattern )
		return;

	if ( --pPattern->m_nRefs > 0 )
		return;

	if ( pPattern->m_pSurface )
		cairo_surface_destroy( pPattern->m_pSurface );

	delete pPattern;
}

static cairo_pattern_t *SEPatternCreate( ESEPatternType nType )
{
	cairo_pattern_t *pPattern = new _cairo_pattern;
	if ( !pPattern )
		return NULL;

	memset( pPattern, 0, sizeof( _cairo_pattern ) );
	// SEArray has a constructor, so re-init it after the memset
	new ( &pPattern->m_Stops ) SEArray<SEStop>();

	pPattern->m_Type = nType;
	pPattern->m_nRefs = 1;
	pPattern->m_Extend = CAIRO_EXTEND_PAD;	// cairo's default for gradient patterns
	cairo_matrix_init_identity( &pPattern->m_Matrix );
	return pPattern;
}

static void SEEvalStops( const cairo_pattern_t *pPattern, double flT, double *pR, double *pG, double *pB, double *pA )
{
	const int nStops = pPattern->m_Stops.Count();
	if ( nStops == 0 )
	{
		*pR = *pG = *pB = *pA = 0.0;
		return;
	}

	// extend handling
	if ( flT < 0.0 || flT > 1.0 )
	{
		switch ( pPattern->m_Extend )
		{
		case CAIRO_EXTEND_NONE:
			*pR = *pG = *pB = *pA = 0.0;
			return;
		case CAIRO_EXTEND_REPEAT:
			flT -= floor( flT );
			break;
		case CAIRO_EXTEND_REFLECT:
			flT = fabs( fmod( flT, 2.0 ) );
			if ( flT > 1.0 )
				flT = 2.0 - flT;
			break;
		case CAIRO_EXTEND_PAD:
		default:
			flT = ( flT < 0.0 ) ? 0.0 : 1.0;
			break;
		}
	}

	if ( flT <= pPattern->m_Stops[0].m_flOffset )
	{
		const SEStop &stop = pPattern->m_Stops[0];
		*pR = stop.m_r; *pG = stop.m_g; *pB = stop.m_b; *pA = stop.m_a;
		return;
	}

	const SEStop &last = pPattern->m_Stops[nStops - 1];
	if ( flT >= last.m_flOffset )
	{
		*pR = last.m_r; *pG = last.m_g; *pB = last.m_b; *pA = last.m_a;
		return;
	}

	for ( int i = 1; i < nStops; ++i )
	{
		const SEStop &stop = pPattern->m_Stops[i];
		if ( flT > stop.m_flOffset )
			continue;

		const SEStop &prev = pPattern->m_Stops[i - 1];
		double flRange = stop.m_flOffset - prev.m_flOffset;
		double flMix = ( flRange > 1e-9 ) ? ( flT - prev.m_flOffset ) / flRange : 0.0;

		// cairo interpolates in premultiplied space
		double a = prev.m_a + ( stop.m_a - prev.m_a ) * flMix;
		*pR = ( prev.m_r * prev.m_a + ( stop.m_r * stop.m_a - prev.m_r * prev.m_a ) * flMix );
		*pG = ( prev.m_g * prev.m_a + ( stop.m_g * stop.m_a - prev.m_g * prev.m_a ) * flMix );
		*pB = ( prev.m_b * prev.m_a + ( stop.m_b * stop.m_a - prev.m_b * prev.m_a ) * flMix );

		if ( a > 1e-6 )
		{
			*pR /= a;
			*pG /= a;
			*pB /= a;
		}
		else
		{
			*pR = *pG = *pB = 0.0;
		}
		*pA = a;
		return;
	}

	*pR = last.m_r; *pG = last.m_g; *pB = last.m_b; *pA = last.m_a;
}

// Samples a pattern at a device space location, returning straight (non
// premultiplied) RGBA.
static void SESamplePattern( const cairo_pattern_t *pPattern, double x, double y, double *pR, double *pG, double *pB, double *pA )
{
	if ( !pPattern )
	{
		*pR = *pG = *pB = *pA = 0.0;
		return;
	}

	switch ( pPattern->m_Type )
	{
	case SE_PATTERN_SOLID:
		*pR = pPattern->m_rgba[0];
		*pG = pPattern->m_rgba[1];
		*pB = pPattern->m_rgba[2];
		*pA = pPattern->m_rgba[3];
		return;

	case SE_PATTERN_SURFACE:
	{
		const cairo_surface_t *pSurface = pPattern->m_pSurface;
		if ( !pSurface || !pSurface->m_pData )
		{
			*pR = *pG = *pB = *pA = 0.0;
			return;
		}

		int nX = (int)floor( x );
		int nY = (int)floor( y );
		if ( nX < 0 || nY < 0 || nX >= pSurface->m_nWidth || nY >= pSurface->m_nHeight )
		{
			*pR = *pG = *pB = *pA = 0.0;
			return;
		}

		const unsigned char *pPixel = pSurface->m_pData + (size_t)nY * pSurface->m_nStride + (size_t)nX * 4;
		double a = pPixel[3] / 255.0;
		*pA = a;
		if ( a > 1e-6 )
		{
			*pR = ( pPixel[2] / 255.0 ) / a;
			*pG = ( pPixel[1] / 255.0 ) / a;
			*pB = ( pPixel[0] / 255.0 ) / a;
		}
		else
		{
			*pR = *pG = *pB = 0.0;
		}
		return;
	}

	case SE_PATTERN_LINEAR:
	case SE_PATTERN_RADIAL:
		break;
	}

	// Gradients: device -> user space through the pattern matrix
	double ux = x, uy = y;
	if ( pPattern->m_bHasMatrix )
	{
		cairo_matrix_t inv = pPattern->m_Matrix;
		if ( cairo_matrix_invert( &inv ) != CAIRO_STATUS_SUCCESS )
		{
			*pR = *pG = *pB = *pA = 0.0;
			return;
		}
		SETransformPoint( &inv, x, y, &ux, &uy );
	}

	double flT = 0.0;
	if ( pPattern->m_Type == SE_PATTERN_LINEAR )
	{
		double dx = pPattern->m_x1 - pPattern->m_x0;
		double dy = pPattern->m_y1 - pPattern->m_y0;
		double flLenSq = dx * dx + dy * dy;
		if ( flLenSq < 1e-12 )
		{
			*pR = *pG = *pB = *pA = 0.0;
			return;
		}
		flT = ( ( ux - pPattern->m_x0 ) * dx + ( uy - pPattern->m_y0 ) * dy ) / flLenSq;
	}
	else
	{
		double flRadius = pPattern->m_r1 - pPattern->m_r0;
		if ( fabs( flRadius ) < 1e-12 )
		{
			*pR = *pG = *pB = *pA = 0.0;
			return;
		}
		// The loader always passes a zero focal radius and (for the icons it draws) focal ==
		// centre, for which this is the exact solution of the radial gradient cone.
		flT = ( SEDistance( ux, uy, pPattern->m_x0, pPattern->m_y0 ) - pPattern->m_r0 ) / flRadius;
	}

	SEEvalStops( pPattern, flT, pR, pG, pB, pA );
}

// Snapshots the transform a pattern is used with (cairo does this in set_source)
static void SESetSource( struct _cairo *cr, cairo_pattern_t *pPattern )
{
	if ( !pPattern )
		return;

	if ( cr->m_State.m_pSource )
		SEPatternDestroy( cr->m_State.m_pSource );

	pPattern->m_nRefs++;
	cr->m_State.m_pSource = pPattern;

	if ( pPattern->m_Type == SE_PATTERN_SURFACE )
	{
		// Group surfaces cover the whole target 1:1, so they are sampled in device space.
		cairo_matrix_init_identity( &pPattern->m_Matrix );
		pPattern->m_bHasMatrix = false;
	}
	else
	{
		pPattern->m_Matrix = cr->m_State.m_Matrix;
		pPattern->m_bHasMatrix = true;
	}
}


//-----------------------------------------------------------------------------
// Path building
//-----------------------------------------------------------------------------
static void SEClearPath( struct _cairo *cr )
{
	cr->m_Path.RemoveAll();
	cr->m_bHasCurrentPoint = false;
}

static void SEAddMove( struct _cairo *cr, double x, double y )
{
	SEPathItem &item = cr->m_Path.AddToTail();
	item.m_Type = SE_PATH_MOVE;
	item.m_x[0] = x;
	item.m_y[0] = y;

	cr->m_bHasCurrentPoint = true;
	cr->m_flCurrentX = x;
	cr->m_flCurrentY = y;
}

static void SEAddLine( struct _cairo *cr, double x, double y )
{
	SEPathItem &item = cr->m_Path.AddToTail();
	item.m_Type = SE_PATH_LINE;
	item.m_x[0] = x;
	item.m_y[0] = y;

	cr->m_bHasCurrentPoint = true;
	cr->m_flCurrentX = x;
	cr->m_flCurrentY = y;
}

static void SEAddCurve( struct _cairo *cr, double x1, double y1, double x2, double y2, double x3, double y3 )
{
	SEPathItem &item = cr->m_Path.AddToTail();
	item.m_Type = SE_PATH_CURVE;
	item.m_x[0] = x1; item.m_y[0] = y1;
	item.m_x[1] = x2; item.m_y[1] = y2;
	item.m_x[2] = x3; item.m_y[2] = y3;

	cr->m_bHasCurrentPoint = true;
	cr->m_flCurrentX = x3;
	cr->m_flCurrentY = y3;
}

static void SEAddClose( struct _cairo *cr )
{
	if ( cr->m_Path.Count() == 0 )
		return;

	SEPathItem &item = cr->m_Path.AddToTail();
	item.m_Type = SE_PATH_CLOSE;

	// the current point becomes the start of the sub path
	for ( int i = cr->m_Path.Count() - 1; i >= 0; --i )
	{
		if ( cr->m_Path[i].m_Type == SE_PATH_MOVE )
		{
			cr->m_flCurrentX = cr->m_Path[i].m_x[0];
			cr->m_flCurrentY = cr->m_Path[i].m_y[0];
			cr->m_bHasCurrentPoint = true;
			break;
		}
	}
}

static void SEAppendCubic( SEFlatPath &path, double x0, double y0, double x1, double y1, double x2, double y2, double x3, double y3 )
{
	double flLen = SEDistance( x0, y0, x1, y1 ) + SEDistance( x1, y1, x2, y2 ) + SEDistance( x2, y2, x3, y3 );
	int nSegments = (int)ceil( sqrt( flLen ) );
	nSegments = SEClamp( nSegments, SE_CURVE_MIN_SEGMENTS, SE_CURVE_MAX_SEGMENTS );

	for ( int i = 1; i <= nSegments; ++i )
	{
		double t = (double)i / (double)nSegments;
		double mt = 1.0 - t;

		double a = mt * mt * mt;
		double b = 3.0 * mt * mt * t;
		double c = 3.0 * mt * t * t;
		double d = t * t * t;

		SEPoint &pt = path.m_Points.AddToTail();
		pt.x = a * x0 + b * x1 + c * x2 + d * x3;
		pt.y = a * y0 + b * y1 + c * y2 + d * y3;
	}
}

// Flattens the context's current path into device space contours.
static void SEFlattenPath( const struct _cairo *cr, SEFlatPath &path )
{
	int nContour = -1;

	// the device space current point as the items are walked (path items hold absolute device
	// space points, but a contour that starts with a LINE/CURVE still needs its start point)
	double flCurX = 0.0, flCurY = 0.0;
	bool bHasCur = false;

	for ( int i = 0; i < cr->m_Path.Count(); ++i )
	{
		const SEPathItem &item = cr->m_Path[i];

		if ( item.m_Type == SE_PATH_MOVE )
		{
			nContour = path.BeginContour( false );
			{
				SEPoint &pt = path.m_Points.AddToTail();
				pt.x = item.m_x[0];
				pt.y = item.m_y[0];
			}
			flCurX = item.m_x[0];
			flCurY = item.m_y[0];
			bHasCur = true;
			continue;
		}

		if ( item.m_Type == SE_PATH_CLOSE )
		{
			if ( nContour >= 0 )
			{
				path.m_Closed[nContour] = 1;

				// the current point becomes the start of the sub path
				flCurX = path.m_Points[path.ContourStart( nContour )].x;
				flCurY = path.m_Points[path.ContourStart( nContour )].y;
				bHasCur = true;
			}
			nContour = -1;
			continue;
		}

		// a path that starts with a line/curve (cairo allows this) begins its own sub path
		if ( nContour < 0 )
		{
			nContour = path.BeginContour( false );
			SEPoint &pt = path.m_Points.AddToTail();
			pt.x = flCurX;
			pt.y = flCurY;
			bHasCur = true;
		}

		if ( item.m_Type == SE_PATH_LINE )
		{
			SEPoint &pt = path.m_Points.AddToTail();
			pt.x = item.m_x[0];
			pt.y = item.m_y[0];
			flCurX = item.m_x[0];
			flCurY = item.m_y[0];
			continue;
		}

		// SE_PATH_CURVE: flatten from the current point through the three control points
		SEAppendCubic( path, flCurX, flCurY, item.m_x[0], item.m_y[0], item.m_x[1], item.m_y[1], item.m_x[2], item.m_y[2] );
		flCurX = item.m_x[2];
		flCurY = item.m_y[2];
	}

	( void )bHasCur;
}


//-----------------------------------------------------------------------------
// Rasterisation
//-----------------------------------------------------------------------------
static void SEBuildEdges( const SEFlatPath &path, SEArray<SEEdge> &edges )
{
	for ( int nContour = 0; nContour < path.ContourCount(); ++nContour )
	{
		const int nStart = path.ContourStart( nContour );
		const int nEnd = path.ContourEnd( nContour );
		const int nPoints = nEnd - nStart;
		if ( nPoints < 2 )
			continue;

		for ( int i = 0; i < nPoints; ++i )
		{
			const SEPoint &p0 = path.m_Points[nStart + i];
			const SEPoint &p1 = path.m_Points[nStart + ( ( i + 1 ) % nPoints )];

			if ( p0.x == p1.x && p0.y == p1.y )
				continue;

			SEEdge &edge = edges.AddToTail();
			edge.x0 = p0.x; edge.y0 = p0.y;
			edge.x1 = p1.x; edge.y1 = p1.y;
		}
	}
}

// Rasterises a set of edges into a coverage buffer covering the region
// returned through the out parameters (caller frees the buffer).
static float *SEBuildCoverage( const SEArray<SEEdge> &edges, int nWidth, int nHeight, cairo_fill_rule_t nRule,
							   int *pnRegionX, int *pnRegionY, int *pnRegionW, int *pnRegionH )
{
	*pnRegionX = *pnRegionY = *pnRegionW = *pnRegionH = 0;

	if ( edges.Count() == 0 || nWidth <= 0 || nHeight <= 0 )
		return NULL;

	double flMinX = 1e300, flMinY = 1e300, flMaxX = -1e300, flMaxY = -1e300;
	for ( int i = 0; i < edges.Count(); ++i )
	{
		const SEEdge &edge = edges[i];
		if ( !SEIsFinite( edge.x0 ) || !SEIsFinite( edge.y0 ) || !SEIsFinite( edge.x1 ) || !SEIsFinite( edge.y1 ) )
			continue;

		flMinX = SEMin( flMinX, SEMin( edge.x0, edge.x1 ) );
		flMinY = SEMin( flMinY, SEMin( edge.y0, edge.y1 ) );
		flMaxX = SEMax( flMaxX, SEMax( edge.x0, edge.x1 ) );
		flMaxY = SEMax( flMaxY, SEMax( edge.y0, edge.y1 ) );
	}

	if ( flMinX > flMaxX || flMinY > flMaxY )
		return NULL;

	int nRegionX = SEClamp( (int)floor( flMinX ), 0, nWidth );
	int nRegionY = SEClamp( (int)floor( flMinY ), 0, nHeight );
	int nRegionX1 = SEClamp( (int)ceil( flMaxX ), 0, nWidth );
	int nRegionY1 = SEClamp( (int)ceil( flMaxY ), 0, nHeight );

	if ( nRegionX1 <= nRegionX || nRegionY1 <= nRegionY )
		return NULL;

	const int nRegionW = nRegionX1 - nRegionX;
	const int nRegionH = nRegionY1 - nRegionY;
	if ( (double)nRegionW * (double)nRegionH > (double)SE_MAX_PIXELS_PER_RASTER )
		return NULL;

	float *pCoverage = (float *)calloc( (size_t)nRegionW * (size_t)nRegionH, sizeof( float ) );
	if ( !pCoverage )
		return NULL;

	SEArray<SECrossing> crossings;
	crossings.EnsureCapacity( edges.Count() );

	const double flSubSampleStep = 1.0 / (double)SE_SUBSAMPLES;

	for ( int nSub = 0; nSub < SE_SUBSAMPLES; ++nSub )
	{
		double flY = (double)nRegionY + ( (double)nSub + 0.5 ) * flSubSampleStep;
		if ( flY < flMinY || flY > flMaxY )
			continue;

		crossings.RemoveAll();
		for ( int i = 0; i < edges.Count(); ++i )
		{
			const SEEdge &edge = edges[i];
			if ( !SEIsFinite( edge.x0 ) || !SEIsFinite( edge.x1 ) || !SEIsFinite( edge.y0 ) || !SEIsFinite( edge.y1 ) )
				continue;

			// half open rule, so vertices on the scan line are counted once
			if ( edge.y0 <= flY && edge.y1 > flY )
			{
				double flT = ( flY - edge.y0 ) / ( edge.y1 - edge.y0 );
				SECrossing &crossing = crossings.AddToTail();
				crossing.x = edge.x0 + flT * ( edge.x1 - edge.x0 );
				crossing.nDir = 1;
			}
			else if ( edge.y1 <= flY && edge.y0 > flY )
			{
				double flT = ( flY - edge.y1 ) / ( edge.y0 - edge.y1 );
				SECrossing &crossing = crossings.AddToTail();
				crossing.x = edge.x1 + flT * ( edge.x0 - edge.x1 );
				crossing.nDir = -1;
			}
		}

		const int nCrossings = crossings.Count();
		if ( nCrossings < 2 )
			continue;

		qsort( crossings.m_pData, (size_t)nCrossings, sizeof( SECrossing ), SECrossingCompare );

		int nWinding = 0;
		const int nRow = (int)floor( flY ) - nRegionY;
		if ( nRow < 0 || nRow >= nRegionH )
			continue;

		for ( int i = 0; i + 1 < nCrossings; ++i )
		{
			nWinding += crossings[i].nDir;

			bool bInside;
			if ( nRule == CAIRO_FILL_RULE_EVEN_ODD )
				bInside = ( ( i & 1 ) == 0 );
			else
				bInside = ( nWinding != 0 );

			if ( !bInside )
				continue;

			double flX0 = crossings[i].x;
			double flX1 = crossings[i + 1].x;
			if ( flX1 <= flX0 )
				continue;

			if ( flX1 <= (double)nRegionX || flX0 >= (double)( nRegionX + nRegionW ) )
				continue;

			flX0 = SEMax( flX0, (double)nRegionX );
			flX1 = SEMin( flX1, (double)( nRegionX + nRegionW ) );

			int nPixelX0 = (int)floor( flX0 );
			int nPixelX1 = (int)floor( flX1 );
			nPixelX1 = SEClamp( nPixelX1, nRegionX, nRegionX + nRegionW - 1 );

			float *pRow = pCoverage + (size_t)nRow * nRegionW;
			for ( int nPixel = nPixelX0; nPixel <= nPixelX1; ++nPixel )
			{
				double flLeft = SEMax( flX0, (double)nPixel );
				double flRight = SEMin( flX1, (double)( nPixel + 1 ) );
				if ( flRight <= flLeft )
					continue;

				pRow[nPixel - nRegionX] += (float)( ( flRight - flLeft ) * flSubSampleStep );
			}
		}
	}

	*pnRegionX = nRegionX;
	*pnRegionY = nRegionY;
	*pnRegionW = nRegionW;
	*pnRegionH = nRegionH;
	return pCoverage;
}


//-----------------------------------------------------------------------------
// Compositing
//-----------------------------------------------------------------------------
static void SEBlendPixel( cairo_surface_t *pSurface, int nX, int nY, const cairo_pattern_t *pPattern,
						  cairo_operator_t nOperator, double flCoverage, double flAlphaMul, int *pnPixelsPainted )
{
	if ( flCoverage <= 0.0 )
		return;

	double sr, sg, sb, sa;
	SESamplePattern( pPattern, (double)nX + 0.5, (double)nY + 0.5, &sr, &sg, &sb, &sa );

	sa *= flAlphaMul * flCoverage;
	if ( sa <= 0.0 )
		return;

	unsigned char *pPixel = pSurface->m_pData + (size_t)nY * pSurface->m_nStride + (size_t)nX * 4;

	// destination is premultiplied BGRA
	double db = pPixel[0] / 255.0;
	double dg = pPixel[1] / 255.0;
	double dr = pPixel[2] / 255.0;
	double da = pPixel[3] / 255.0;

	double srp = sr * sa;
	double sgp = sg * sa;
	double sbp = sb * sa;

	double ob, og, orr, oa;
	if ( nOperator == CAIRO_OPERATOR_SOURCE )
	{
		// replace the destination inside the coverage
		double flMix = flCoverage;
		ob = sbp + db * ( 1.0 - flMix );
		og = sgp + dg * ( 1.0 - flMix );
		orr = srp + dr * ( 1.0 - flMix );
		oa = sa + da * ( 1.0 - flMix );
	}
	else
	{
		// OVER (every other operator the loader uses behaves as OVER)
		ob = sbp + db * ( 1.0 - sa );
		og = sgp + dg * ( 1.0 - sa );
		orr = srp + dr * ( 1.0 - sa );
		oa = sa + da * ( 1.0 - sa );
	}

	if ( ob < 0.0 ) ob = 0.0; if ( ob > 1.0 ) ob = 1.0;
	if ( og < 0.0 ) og = 0.0; if ( og > 1.0 ) og = 1.0;
	if ( orr < 0.0 ) orr = 0.0; if ( orr > 1.0 ) orr = 1.0;
	if ( oa < 0.0 ) oa = 0.0; if ( oa > 1.0 ) oa = 1.0;

	pPixel[0] = (unsigned char)( ob * 255.0 + 0.5 );
	pPixel[1] = (unsigned char)( og * 255.0 + 0.5 );
	pPixel[2] = (unsigned char)( orr * 255.0 + 0.5 );
	pPixel[3] = (unsigned char)( oa * 255.0 + 0.5 );

	if ( pnPixelsPainted )
		( *pnPixelsPainted )++;
}

static void SECompositeCoverage( struct _cairo *cr, const float *pCoverage, int nRegionX, int nRegionY, int nRegionW, int nRegionH, double flAlphaMul )
{
	cairo_surface_t *pSurface = cr->m_pTarget;
	if ( !pSurface || !pSurface->m_pData )
		return;

	const float *pClip = cr->m_State.m_pClip;
	int nPainted = 0;

	for ( int y = 0; y < nRegionH; ++y )
	{
		const float *pRow = pCoverage + (size_t)y * nRegionW;
		const float *pClipRow = pClip ? ( pClip + (size_t)( nRegionY + y ) * pSurface->m_nWidth + nRegionX ) : NULL;

		for ( int x = 0; x < nRegionW; ++x )
		{
			double flCoverage = pRow[x];
			if ( pClipRow )
				flCoverage *= pClipRow[x];

			SEBlendPixel( pSurface, nRegionX + x, nRegionY + y, cr->m_State.m_pSource, cr->m_State.m_Operator,
						  flCoverage, flAlphaMul, &nPainted );
		}
	}

	cr->m_nLastPaintPixels = nPainted;
}

static void SECompositePaint( struct _cairo *cr, double flAlphaMul )
{
	cairo_surface_t *pSurface = cr->m_pTarget;
	if ( !pSurface || !pSurface->m_pData )
		return;

	const float *pClip = cr->m_State.m_pClip;
	int nPainted = 0;

	for ( int y = 0; y < pSurface->m_nHeight; ++y )
	{
		for ( int x = 0; x < pSurface->m_nWidth; ++x )
		{
			double flCoverage = pClip ? pClip[(size_t)y * pSurface->m_nWidth + x] : 1.0;
			SEBlendPixel( pSurface, x, y, cr->m_State.m_pSource, cr->m_State.m_Operator, flCoverage, flAlphaMul, &nPainted );
		}
	}

	cr->m_nLastPaintPixels = nPainted;
}


//-----------------------------------------------------------------------------
// Stroking: turn the flattened path into a filled outline
//-----------------------------------------------------------------------------
static void SEAddPolygon( SEFlatPath &path, const SEPoint *pPoints, int nCount )
{
	if ( nCount < 3 )
		return;

	// Enforce a consistent orientation: the outline pieces overlap and are rasterised with the
	// non-zero rule, so opposite orientations would cancel out and punch holes.
	double flArea = 0.0;
	for ( int i = 0; i < nCount; ++i )
	{
		const SEPoint &p0 = pPoints[i];
		const SEPoint &p1 = pPoints[( i + 1 ) % nCount];
		flArea += p0.x * p1.y - p1.x * p0.y;
	}

	path.BeginContour( true );
	if ( flArea >= 0.0 )
	{
		for ( int i = 0; i < nCount; ++i )
			path.m_Points.AddToTail() = pPoints[i];
	}
	else
	{
		for ( int i = nCount - 1; i >= 0; --i )
			path.m_Points.AddToTail() = pPoints[i];
	}
}

static void SEAddSegmentQuad( SEFlatPath &path, const SEPoint &p0, const SEPoint &p1, double flHalfWidth )
{
	double dx = p1.x - p0.x;
	double dy = p1.y - p0.y;
	double flLen = sqrt( dx * dx + dy * dy );
	if ( flLen < 1e-9 )
		return;

	// outward normals (both sides), the winding is normalised inside SEAddPolygon
	double nx = -dy / flLen * flHalfWidth;
	double ny = dx / flLen * flHalfWidth;

	SEPoint quad[4];
	quad[0].x = p0.x + nx; quad[0].y = p0.y + ny;
	quad[1].x = p1.x + nx; quad[1].y = p1.y + ny;
	quad[2].x = p1.x - nx; quad[2].y = p1.y - ny;
	quad[3].x = p0.x - nx; quad[3].y = p0.y - ny;
	SEAddPolygon( path, quad, 4 );
}

static void SEAddDisc( SEFlatPath &path, const SEPoint &pCenter, double flRadius )
{
	if ( flRadius <= 0.0 )
		return;

	SEPoint points[SE_ROUND_SEGMENTS];
	for ( int i = 0; i < SE_ROUND_SEGMENTS; ++i )
	{
		double flAngle = 2.0 * SE_PI * (double)i / (double)SE_ROUND_SEGMENTS;
		points[i].x = pCenter.x + cos( flAngle ) * flRadius;
		points[i].y = pCenter.y + sin( flAngle ) * flRadius;
	}
	SEAddPolygon( path, points, SE_ROUND_SEGMENTS );
}

static void SEAddJoin( SEFlatPath &path, const SEPoint &pPrev, const SEPoint &p, const SEPoint &pNext,
					   double flHalfWidth, cairo_line_join_t nJoin )
{
	if ( nJoin == CAIRO_LINE_JOIN_ROUND )
	{
		SEAddDisc( path, p, flHalfWidth );
		return;
	}

	double d1x = p.x - pPrev.x, d1y = p.y - pPrev.y;
	double d2x = pNext.x - p.x, d2y = pNext.y - p.y;
	double flLen1 = sqrt( d1x * d1x + d1y * d1y );
	double flLen2 = sqrt( d2x * d2x + d2y * d2y );
	if ( flLen1 < 1e-9 || flLen2 < 1e-9 )
		return;

	d1x /= flLen1; d1y /= flLen1;
	d2x /= flLen2; d2y /= flLen2;

	double flCross = d1x * d2y - d1y * d2x;
	if ( fabs( flCross ) < 1e-9 )
		return;			// collinear, the quads already meet

	// left normals; the outer side is the one the path turns away from
	double n1x = -d1y, n1y = d1x;
	double n2x = -d2y, n2y = d2x;
	double flSign = ( flCross > 0.0 ) ? -1.0 : 1.0;

	SEPoint corner1, corner2;
	corner1.x = p.x + flSign * n1x * flHalfWidth;
	corner1.y = p.y + flSign * n1y * flHalfWidth;
	corner2.x = p.x + flSign * n2x * flHalfWidth;
	corner2.y = p.y + flSign * n2y * flHalfWidth;

	if ( nJoin == CAIRO_LINE_JOIN_BEVEL )
	{
		SEPoint triangle[3];
		triangle[0] = p;
		triangle[1] = corner1;
		triangle[2] = corner2;
		SEAddPolygon( path, triangle, 3 );
		return;
	}

	// miter
	double bx = ( n1x + n2x ), by = ( n1y + n2y );
	double flBisector = sqrt( bx * bx + by * by );
	if ( flBisector < 1e-9 )
	{
		SEPoint triangle[3];
		triangle[0] = p;
		triangle[1] = corner1;
		triangle[2] = corner2;
		SEAddPolygon( path, triangle, 3 );
		return;
	}

	bx /= flBisector;
	by /= flBisector;

	double flCosHalf = fabs( bx * n1x + by * n1y );
	if ( flCosHalf < 1e-6 )
		flCosHalf = 1e-6;

	double flMiterLength = flHalfWidth / flCosHalf;
	if ( flMiterLength > flHalfWidth * SE_MITER_LIMIT )
		flMiterLength = flHalfWidth * SE_MITER_LIMIT;		// beveled off, like cairo's miter limit

	SEPoint quad[4];
	quad[0] = p;
	quad[1] = corner1;
	quad[2].x = p.x + bx * flMiterLength;
	quad[2].y = p.y + by * flMiterLength;
	quad[3] = corner2;
	SEAddPolygon( path, quad, 4 );
}

static void SEAddCap( SEFlatPath &path, const SEPoint &p, double flDirX, double flDirY, double flHalfWidth, cairo_line_cap_t nCap )
{
	if ( nCap == CAIRO_LINE_CAP_ROUND )
	{
		SEAddDisc( path, p, flHalfWidth );
		return;
	}

	if ( nCap != CAIRO_LINE_CAP_SQUARE )
		return;

	double flLen = sqrt( flDirX * flDirX + flDirY * flDirY );
	if ( flLen < 1e-9 )
		return;

	flDirX /= flLen;
	flDirY /= flLen;

	// extend the segment by half a width, using the same quad helper
	SEPoint p0 = p;
	SEPoint p1;
	p1.x = p.x + flDirX * flHalfWidth;
	p1.y = p.y + flDirY * flHalfWidth;
	SEAddSegmentQuad( path, p0, p1, flHalfWidth );
}

static void SEBuildStrokePath( const struct _cairo *cr, const SEFlatPath &source, SEFlatPath &stroke )
{
	double flScale = SEMatrixScale( &cr->m_State.m_Matrix );
	double flHalfWidth = 0.5 * cr->m_State.m_flLineWidth * flScale;
	if ( flHalfWidth < 0.1 )
		flHalfWidth = 0.1;		// keep thin "hairlines" visible

	for ( int nContour = 0; nContour < source.ContourCount(); ++nContour )
	{
		const int nStart = source.ContourStart( nContour );
		const int nCount = source.ContourEnd( nContour ) - nStart;
		if ( nCount < 2 )
			continue;

		const bool bClosed = ( source.m_Closed[nContour] != 0 );

		const int nSegments = bClosed ? nCount : ( nCount - 1 );
		for ( int i = 0; i < nSegments; ++i )
		{
			const SEPoint &p0 = source.m_Points[nStart + i];
			const SEPoint &p1 = source.m_Points[nStart + ( ( i + 1 ) % nCount )];
			SEAddSegmentQuad( stroke, p0, p1, flHalfWidth );
		}

		// joins at every interior vertex
		for ( int i = 0; i < nCount; ++i )
		{
			if ( !bClosed && ( i == 0 || i == nCount - 1 ) )
				continue;

			const SEPoint &pPrev = source.m_Points[nStart + ( i + nCount - 1 ) % nCount];
			const SEPoint &p = source.m_Points[nStart + i];
			const SEPoint &pNext = source.m_Points[nStart + ( i + 1 ) % nCount];
			SEAddJoin( stroke, pPrev, p, pNext, flHalfWidth, cr->m_State.m_LineJoin );
		}

		if ( !bClosed )
		{
			const SEPoint &pFirst = source.m_Points[nStart];
			const SEPoint &pSecond = source.m_Points[nStart + 1];
			const SEPoint &pLast = source.m_Points[nStart + nCount - 1];
			const SEPoint &pPrev = source.m_Points[nStart + nCount - 2];

			SEAddCap( stroke, pFirst, pFirst.x - pSecond.x, pFirst.y - pSecond.y, flHalfWidth, cr->m_State.m_LineCap );
			SEAddCap( stroke, pLast, pLast.x - pPrev.x, pLast.y - pPrev.y, flHalfWidth, cr->m_State.m_LineCap );
		}
	}
}


//-----------------------------------------------------------------------------
// Fill / stroke / clip implementations
//-----------------------------------------------------------------------------
static void SEDoFill( struct _cairo *cr, bool bPreservePath )
{
	if ( !cr->m_pTarget || cr->m_Path.Count() == 0 )
	{
		if ( !bPreservePath )
			SEClearPath( cr );
		return;
	}

	SEFlatPath path;
	SEFlattenPath( cr, path );

	SEArray<SEEdge> edges;
	SEBuildEdges( path, edges );

	int nRegionX, nRegionY, nRegionW, nRegionH;
	float *pCoverage = SEBuildCoverage( edges, cr->m_pTarget->m_nWidth, cr->m_pTarget->m_nHeight, cr->m_State.m_FillRule,
										&nRegionX, &nRegionY, &nRegionW, &nRegionH );
	if ( pCoverage )
	{
		SECompositeCoverage( cr, pCoverage, nRegionX, nRegionY, nRegionW, nRegionH, 1.0 );
		free( pCoverage );
	}
	else
	{
		cr->m_nLastPaintPixels = 0;
	}

	g_nLastPaintPixelCount = cr->m_nLastPaintPixels;

	if ( !bPreservePath )
		SEClearPath( cr );
}

static void SEDoStroke( struct _cairo *cr )
{
	if ( !cr->m_pTarget || cr->m_Path.Count() == 0 )
	{
		SEClearPath( cr );
		return;
	}

	if ( cr->m_State.m_flLineWidth <= 0.0 )
	{
		SEClearPath( cr );
		return;
	}

	SEFlatPath path;
	SEFlattenPath( cr, path );

	SEFlatPath stroke;
	SEBuildStrokePath( cr, path, stroke );

	SEArray<SEEdge> edges;
	SEBuildEdges( stroke, edges );

	int nRegionX, nRegionY, nRegionW, nRegionH;
	float *pCoverage = SEBuildCoverage( edges, cr->m_pTarget->m_nWidth, cr->m_pTarget->m_nHeight, CAIRO_FILL_RULE_WINDING,
										&nRegionX, &nRegionY, &nRegionW, &nRegionH );
	if ( pCoverage )
	{
		SECompositeCoverage( cr, pCoverage, nRegionX, nRegionY, nRegionW, nRegionH, 1.0 );
		free( pCoverage );
	}
	else
	{
		cr->m_nLastPaintPixels = 0;
	}

	g_nLastPaintPixelCount = cr->m_nLastPaintPixels;
	SEClearPath( cr );
}

static void SEDoClip( struct _cairo *cr )
{
	if ( !cr->m_pTarget || cr->m_Path.Count() == 0 )
	{
		SEClearPath( cr );
		return;
	}

	// copy on write: saved states may still be using this mask
	if ( cr->m_State.m_pClipRefs && *cr->m_State.m_pClipRefs > 1 )
	{
		int *pNewRefs = NULL;
		float *pNewClip = SEClipClone( cr->m_State.m_pClip, cr->m_State.m_pClipRefs,
									   cr->m_pTarget->m_nWidth, cr->m_pTarget->m_nHeight, &pNewRefs );
		SEClipRelease( cr->m_State.m_pClip, cr->m_State.m_pClipRefs );
		cr->m_State.m_pClip = pNewClip;
		cr->m_State.m_pClipRefs = pNewRefs;
	}
	else if ( !cr->m_State.m_pClip )
	{
		cr->m_State.m_pClip = SEClipFull( cr->m_pTarget->m_nWidth, cr->m_pTarget->m_nHeight, &cr->m_State.m_pClipRefs );
	}

	SEFlatPath path;
	SEFlattenPath( cr, path );

	SEArray<SEEdge> edges;
	SEBuildEdges( path, edges );

	int nRegionX, nRegionY, nRegionW, nRegionH;
	float *pCoverage = SEBuildCoverage( edges, cr->m_pTarget->m_nWidth, cr->m_pTarget->m_nHeight, cr->m_State.m_FillRule,
										&nRegionX, &nRegionY, &nRegionW, &nRegionH );
	if ( pCoverage )
	{
		float *pClip = cr->m_State.m_pClip;
		if ( pClip )
		{
			for ( int y = 0; y < nRegionH; ++y )
			{
				float *pClipRow = pClip + (size_t)( nRegionY + y ) * cr->m_pTarget->m_nWidth + nRegionX;
				const float *pRow = pCoverage + (size_t)y * nRegionW;
				for ( int x = 0; x < nRegionW; ++x )
					pClipRow[x] *= pRow[x];
			}
		}
		free( pCoverage );
	}
	else if ( cr->m_State.m_pClip )
	{
		// the path covers nothing: the clip becomes empty
		memset( cr->m_State.m_pClip, 0, (size_t)cr->m_pTarget->m_nWidth * cr->m_pTarget->m_nHeight * sizeof( float ) );
	}

	// cairo clears the current path in cairo_clip()
	SEClearPath( cr );
}


//-----------------------------------------------------------------------------
// State
//-----------------------------------------------------------------------------
static void SEStateInit( SEGState *pState )
{
	cairo_matrix_init_identity( &pState->m_Matrix );
	pState->m_Operator = CAIRO_OPERATOR_OVER;
	pState->m_FillRule = CAIRO_FILL_RULE_WINDING;
	pState->m_flLineWidth = 1.0;
	pState->m_LineCap = CAIRO_LINE_CAP_BUTT;
	pState->m_LineJoin = CAIRO_LINE_JOIN_MITER;
	pState->m_pSource = NULL;
	pState->m_pClip = NULL;
	pState->m_pClipRefs = NULL;
}

static void SEStateDispose( SEGState *pState )
{
	if ( pState->m_pSource )
	{
		SEPatternDestroy( pState->m_pSource );
		pState->m_pSource = NULL;
	}

	if ( pState->m_pClip )
	{
		SEClipRelease( pState->m_pClip, pState->m_pClipRefs );
		pState->m_pClip = NULL;
		pState->m_pClipRefs = NULL;
	}
}


//-----------------------------------------------------------------------------
// Surfaces
//-----------------------------------------------------------------------------
static cairo_surface_t *SECreateSurface( unsigned char *pData, cairo_format_t format, int nWidth, int nHeight, int nStride )
{
	cairo_surface_t *pSurface = new _cairo_surface;
	if ( !pSurface )
		return NULL;

	pSurface->m_pData = pData;
	pSurface->m_nWidth = nWidth;
	pSurface->m_nHeight = nHeight;
	pSurface->m_nStride = nStride;
	pSurface->m_Format = format;
	pSurface->m_nRefs = 1;
	return pSurface;
}

cairo_surface_t *cairo_image_surface_create_for_data( unsigned char *data, cairo_format_t format, int width, int height, int stride )
{
	if ( !data || width <= 0 || height <= 0 || stride < width * 4 )
		return NULL;

	return SECreateSurface( data, format, width, height, stride );
}

int cairo_image_surface_get_width( cairo_surface_t *surface ) { return surface ? surface->m_nWidth : 0; }
int cairo_image_surface_get_height( cairo_surface_t *surface ) { return surface ? surface->m_nHeight : 0; }
int cairo_image_surface_get_stride( cairo_surface_t *surface ) { return surface ? surface->m_nStride : 0; }
unsigned char *cairo_image_surface_get_data( cairo_surface_t *surface ) { return surface ? surface->m_pData : NULL; }

void cairo_surface_destroy( cairo_surface_t *surface )
{
	if ( !surface )
		return;

	if ( --surface->m_nRefs > 0 )
		return;

	delete surface;
}

cairo_status_t cairo_surface_status( cairo_surface_t *surface )
{
	return ( surface && surface->m_pData ) ? CAIRO_STATUS_SUCCESS : CAIRO_STATUS_NO_MEMORY;
}

void cairo_surface_flush( cairo_surface_t *surface ) { ( void )surface; }
void cairo_surface_mark_dirty( cairo_surface_t *surface ) { ( void )surface; }


//-----------------------------------------------------------------------------
// Context
//-----------------------------------------------------------------------------
cairo_t *cairo_create( cairo_surface_t *target )
{
	if ( !target )
		return NULL;

	struct _cairo *cr = new _cairo;
	if ( !cr )
		return NULL;

	memset( cr, 0, sizeof( struct _cairo ) );
	new ( &cr->m_GroupSurfaces ) SEArray<cairo_surface_t *>();
	new ( &cr->m_SavedStates ) SEArray<SEGState>();
	new ( &cr->m_Path ) SEArray<SEPathItem>();

	cr->m_pTarget = target;
	cr->m_pRootTarget = target;
	target->m_nRefs++;

	SEStateInit( &cr->m_State );
	cr->m_Status = CAIRO_STATUS_SUCCESS;
	return cr;
}

void cairo_destroy( cairo_t *cr )
{
	if ( !cr )
		return;

	SEStateDispose( &cr->m_State );

	for ( int i = 0; i < cr->m_SavedStates.Count(); ++i )
		SEStateDispose( &cr->m_SavedStates[i] );

	for ( int i = 0; i < cr->m_GroupSurfaces.Count(); ++i )
		cairo_surface_destroy( cr->m_GroupSurfaces[i] );

	if ( cr->m_pRootTarget )
		cairo_surface_destroy( cr->m_pRootTarget );

	delete cr;
}

cairo_status_t cairo_status( cairo_t *cr ) { return cr ? cr->m_Status : CAIRO_STATUS_NULL_POINTER; }
cairo_surface_t *cairo_get_target( cairo_t *cr ) { return cr ? cr->m_pRootTarget : NULL; }

void cairo_save( cairo_t *cr )
{
	if ( !cr )
		return;

	SEGState &saved = cr->m_SavedStates.AddToTail();
	saved = cr->m_State;

	if ( saved.m_pSource )
		saved.m_pSource->m_nRefs++;
	if ( saved.m_pClipRefs )
		( *saved.m_pClipRefs )++;
}

void cairo_restore( cairo_t *cr )
{
	if ( !cr )
		return;

	const int nStates = cr->m_SavedStates.Count();
	if ( nStates == 0 )
	{
		cr->m_Status = CAIRO_STATUS_INVALID_RESTORE;
		return;
	}

	SEStateDispose( &cr->m_State );
	cr->m_State = cr->m_SavedStates[nStates - 1];	// ownership moves to the current state
	cr->m_SavedStates.m_nCount--;
}

void cairo_push_group( cairo_t *cr )
{
	if ( !cr || !cr->m_pRootTarget )
		return;

	const int nWidth = cr->m_pRootTarget->m_nWidth;
	const int nHeight = cr->m_pRootTarget->m_nHeight;
	const int nStride = nWidth * 4;

	// an intermediate surface with the same geometry as the root target, so the group can be
	// used as a pattern without any device offset
	unsigned char *pData = (unsigned char *)calloc( (size_t)nStride * (size_t)nHeight, 1 );
	if ( !pData )
		return;

	cairo_surface_t *pGroup = SECreateSurface( pData, CAIRO_FORMAT_ARGB32, nWidth, nHeight, nStride );
	if ( !pGroup )
	{
		free( pData );
		return;
	}

	cairo_save( cr );
	cr->m_GroupSurfaces.AddToTail() = pGroup;
	cr->m_pTarget = pGroup;
}

void cairo_pop_group_to_source( cairo_t *cr )
{
	if ( !cr )
		return;

	const int nGroups = cr->m_GroupSurfaces.Count();
	if ( nGroups == 0 )
	{
		cr->m_Status = CAIRO_STATUS_INVALID_POP_GROUP;
		return;
	}

	cairo_surface_t *pGroup = cr->m_GroupSurfaces[nGroups - 1];
	cr->m_GroupSurfaces.m_nCount--;

	cr->m_pTarget = ( cr->m_GroupSurfaces.Count() > 0 ) ? cr->m_GroupSurfaces[cr->m_GroupSurfaces.Count() - 1] : cr->m_pRootTarget;

	cairo_restore( cr );

	// the group becomes the source; the pattern owns a reference to the surface and the
	// context drops its own, so releasing the group here frees it once the source is replaced
	cairo_pattern_t *pPattern = SEPatternCreate( SE_PATTERN_SURFACE );
	if ( pPattern )
	{
		pPattern->m_pSurface = pGroup;
		SESetSource( cr, pPattern );
		SEPatternDestroy( pPattern );
	}
	else
	{
		cairo_surface_destroy( pGroup );
	}
}

void cairo_set_operator( cairo_t *cr, cairo_operator_t op ) { if ( cr ) cr->m_State.m_Operator = op; }
void cairo_set_fill_rule( cairo_t *cr, cairo_fill_rule_t fill_rule ) { if ( cr ) cr->m_State.m_FillRule = fill_rule; }
void cairo_set_line_width( cairo_t *cr, double width ) { if ( cr ) cr->m_State.m_flLineWidth = width; }
void cairo_set_line_cap( cairo_t *cr, cairo_line_cap_t line_cap ) { if ( cr ) cr->m_State.m_LineCap = line_cap; }
void cairo_set_line_join( cairo_t *cr, cairo_line_join_t line_join ) { if ( cr ) cr->m_State.m_LineJoin = line_join; }

void cairo_set_source( cairo_t *cr, cairo_pattern_t *source )
{
	if ( !cr || !source )
		return;

	SESetSource( cr, source );
}

void cairo_set_source_rgba( cairo_t *cr, double red, double green, double blue, double alpha )
{
	if ( !cr )
		return;

	cairo_pattern_t *pPattern = SEPatternCreate( SE_PATTERN_SOLID );
	if ( !pPattern )
		return;

	pPattern->m_rgba[0] = red;
	pPattern->m_rgba[1] = green;
	pPattern->m_rgba[2] = blue;
	pPattern->m_rgba[3] = alpha;

	SESetSource( cr, pPattern );
	SEPatternDestroy( pPattern );
}


//-----------------------------------------------------------------------------
// Paths
//-----------------------------------------------------------------------------
void cairo_new_path( cairo_t *cr ) { if ( cr ) SEClearPath( cr ); }

void cairo_move_to( cairo_t *cr, double x, double y )
{
	if ( !cr || !SEIsFinite( x ) || !SEIsFinite( y ) )
		return;

	double devX, devY;
	SETransformPoint( &cr->m_State.m_Matrix, x, y, &devX, &devY );
	SEAddMove( cr, devX, devY );
}

void cairo_rel_move_to( cairo_t *cr, double dx, double dy )
{
	if ( !cr || !cr->m_bHasCurrentPoint )
		return;

	double devDx, devDy;
	SETransformDistance( &cr->m_State.m_Matrix, dx, dy, &devDx, &devDy );
	SEAddMove( cr, cr->m_flCurrentX + devDx, cr->m_flCurrentY + devDy );
}

void cairo_line_to( cairo_t *cr, double x, double y )
{
	if ( !cr || !SEIsFinite( x ) || !SEIsFinite( y ) )
		return;

	double devX, devY;
	SETransformPoint( &cr->m_State.m_Matrix, x, y, &devX, &devY );

	if ( !cr->m_bHasCurrentPoint )
	{
		SEAddMove( cr, devX, devY );
		return;
	}

	SEAddLine( cr, devX, devY );
}

void cairo_rel_line_to( cairo_t *cr, double dx, double dy )
{
	if ( !cr || !cr->m_bHasCurrentPoint )
		return;

	double devDx, devDy;
	SETransformDistance( &cr->m_State.m_Matrix, dx, dy, &devDx, &devDy );
	SEAddLine( cr, cr->m_flCurrentX + devDx, cr->m_flCurrentY + devDy );
}

void cairo_curve_to( cairo_t *cr, double x1, double y1, double x2, double y2, double x3, double y3 )
{
	if ( !cr )
		return;

	double d1x, d1y, d2x, d2y, d3x, d3y;
	SETransformPoint( &cr->m_State.m_Matrix, x1, y1, &d1x, &d1y );
	SETransformPoint( &cr->m_State.m_Matrix, x2, y2, &d2x, &d2y );
	SETransformPoint( &cr->m_State.m_Matrix, x3, y3, &d3x, &d3y );

	if ( !cr->m_bHasCurrentPoint )
		SEAddMove( cr, d1x, d1y );

	SEAddCurve( cr, d1x, d1y, d2x, d2y, d3x, d3y );
}

void cairo_rel_curve_to( cairo_t *cr, double dx1, double dy1, double dx2, double dy2, double dx3, double dy3 )
{
	if ( !cr || !cr->m_bHasCurrentPoint )
		return;

	double d1x, d1y, d2x, d2y, d3x, d3y;
	SETransformDistance( &cr->m_State.m_Matrix, dx1, dy1, &d1x, &d1y );
	SETransformDistance( &cr->m_State.m_Matrix, dx2, dy2, &d2x, &d2y );
	SETransformDistance( &cr->m_State.m_Matrix, dx3, dy3, &d3x, &d3y );

	SEAddCurve( cr, cr->m_flCurrentX + d1x, cr->m_flCurrentY + d1y,
					cr->m_flCurrentX + d2x, cr->m_flCurrentY + d2y,
					cr->m_flCurrentX + d3x, cr->m_flCurrentY + d3y );
}

void cairo_rectangle( cairo_t *cr, double x, double y, double width, double height )
{
	if ( !cr )
		return;

	cairo_move_to( cr, x, y );
	cairo_rel_line_to( cr, width, 0.0 );
	cairo_rel_line_to( cr, 0.0, height );
	cairo_rel_line_to( cr, -width, 0.0 );
	cairo_close_path( cr );
}

void cairo_arc( cairo_t *cr, double xc, double yc, double radius, double angle1, double angle2 )
{
	if ( !cr || radius <= 0.0 )
		return;

	double flSweep = angle2 - angle1;
	bool bFullCircle = ( fabs( flSweep ) >= 2.0 * SE_PI - 1e-9 );
	int nSegments = (int)ceil( fabs( flSweep ) / ( 2.0 * SE_PI ) * (double)SE_ARC_SEGMENTS );
	nSegments = SEClamp( nSegments, 4, 4 * SE_ARC_SEGMENTS );
	if ( bFullCircle && nSegments < 3 )
		nSegments = SE_ARC_SEGMENTS;

	// user space arc start
	double flStartX = xc + cos( angle1 ) * radius;
	double flStartY = yc + sin( angle1 ) * radius;
	double devX, devY;
	SETransformPoint( &cr->m_State.m_Matrix, flStartX, flStartY, &devX, &devY );

	if ( cr->m_bHasCurrentPoint )
	{
		// cairo adds a line from the current point to the start of the arc
		if ( fabs( cr->m_flCurrentX - devX ) > 1e-9 || fabs( cr->m_flCurrentY - devY ) > 1e-9 )
			SEAddLine( cr, devX, devY );
	}
	else
	{
		SEAddMove( cr, devX, devY );
	}

	int nSteps = bFullCircle ? SE_ARC_SEGMENTS : nSegments;
	for ( int i = 1; i <= nSteps; ++i )
	{
		double flAngle = angle1 + flSweep * (double)i / (double)nSteps;
		double x = xc + cos( flAngle ) * radius;
		double y = yc + sin( flAngle ) * radius;
		SETransformPoint( &cr->m_State.m_Matrix, x, y, &devX, &devY );
		SEAddLine( cr, devX, devY );
	}
}

void cairo_close_path( cairo_t *cr ) { if ( cr ) SEAddClose( cr ); }

int cairo_has_current_point( cairo_t *cr ) { return ( cr && cr->m_bHasCurrentPoint ) ? 1 : 0; }

void cairo_get_current_point( cairo_t *cr, double *x, double *y )
{
	if ( !cr )
		return;

	if ( x ) *x = cr->m_bHasCurrentPoint ? cr->m_flCurrentX : 0.0;
	if ( y ) *y = cr->m_bHasCurrentPoint ? cr->m_flCurrentY : 0.0;
}

void cairo_fill( cairo_t *cr ) { if ( cr ) SEDoFill( cr, false ); }
void cairo_fill_preserve( cairo_t *cr ) { if ( cr ) SEDoFill( cr, true ); }
void cairo_stroke( cairo_t *cr ) { if ( cr ) SEDoStroke( cr ); }
void cairo_clip( cairo_t *cr ) { if ( cr ) SEDoClip( cr ); }

void cairo_paint( cairo_t *cr ) { if ( cr ) SECompositePaint( cr, 1.0 ); }
void cairo_paint_with_alpha( cairo_t *cr, double alpha ) { if ( cr ) SECompositePaint( cr, alpha ); }


//-----------------------------------------------------------------------------
// Transforms
//-----------------------------------------------------------------------------
void cairo_translate( cairo_t *cr, double tx, double ty )
{
	if ( !cr )
		return;

	cairo_matrix_t m;
	cairo_matrix_init( &m, 1.0, 0.0, 0.0, 1.0, tx, ty );
	cairo_transform( cr, &m );
}

void cairo_scale( cairo_t *cr, double sx, double sy )
{
	if ( !cr )
		return;

	cairo_matrix_t m;
	cairo_matrix_init( &m, sx, 0.0, 0.0, sy, 0.0, 0.0 );
	cairo_transform( cr, &m );
}

void cairo_rotate( cairo_t *cr, double angle )
{
	if ( !cr )
		return;

	double c = cos( angle );
	double s = sin( angle );
	cairo_matrix_t m;
	cairo_matrix_init( &m, c, s, -s, c, 0.0, 0.0 );
	cairo_transform( cr, &m );
}

void cairo_transform( cairo_t *cr, const cairo_matrix_t *matrix )
{
	if ( !cr || !matrix )
		return;

	cairo_matrix_t result;
	cairo_matrix_multiply( &result, &cr->m_State.m_Matrix, matrix );
	cr->m_State.m_Matrix = result;
}

void cairo_set_matrix( cairo_t *cr, const cairo_matrix_t *matrix )
{
	if ( !cr || !matrix )
		return;

	cr->m_State.m_Matrix = *matrix;
}

void cairo_get_matrix( cairo_t *cr, cairo_matrix_t *matrix )
{
	if ( !cr || !matrix )
		return;

	*matrix = cr->m_State.m_Matrix;
}

void cairo_identity_matrix( cairo_t *cr ) { if ( cr ) cairo_matrix_init_identity( &cr->m_State.m_Matrix ); }

void cairo_matrix_init( cairo_matrix_t *matrix, double xx, double yx, double xy, double yy, double x0, double y0 )
{
	if ( !matrix )
		return;

	matrix->xx = xx; matrix->yx = yx;
	matrix->xy = xy; matrix->yy = yy;
	matrix->x0 = x0; matrix->y0 = y0;
}

void cairo_matrix_init_identity( cairo_matrix_t *matrix )
{
	cairo_matrix_init( matrix, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0 );
}

// result = a * b (apply b first, then a)
void cairo_matrix_multiply( cairo_matrix_t *result, const cairo_matrix_t *a, const cairo_matrix_t *b )
{
	if ( !result || !a || !b )
		return;

	cairo_matrix_t r;
	r.xx = a->xx * b->xx + a->xy * b->yx;
	r.yx = a->yx * b->xx + a->yy * b->yx;
	r.xy = a->xx * b->xy + a->xy * b->yy;
	r.yy = a->yx * b->xy + a->yy * b->yy;
	r.x0 = a->xx * b->x0 + a->xy * b->y0 + a->x0;
	r.y0 = a->yx * b->x0 + a->yy * b->y0 + a->y0;

	*result = r;
}

void cairo_matrix_transform_point( const cairo_matrix_t *matrix, double *x, double *y )
{
	if ( !matrix || !x || !y )
		return;

	double devX, devY;
	SETransformPoint( matrix, *x, *y, &devX, &devY );
	*x = devX;
	*y = devY;
}

cairo_status_t cairo_matrix_invert( cairo_matrix_t *matrix )
{
	if ( !matrix )
		return CAIRO_STATUS_NULL_POINTER;

	double flDet = matrix->xx * matrix->yy - matrix->xy * matrix->yx;
	if ( !SEIsFinite( flDet ) || fabs( flDet ) < 1e-12 )
		return CAIRO_STATUS_INVALID_MATRIX;

	cairo_matrix_t inv;
	inv.xx = matrix->yy / flDet;
	inv.xy = -matrix->xy / flDet;
	inv.yx = -matrix->yx / flDet;
	inv.yy = matrix->xx / flDet;
	inv.x0 = ( matrix->xy * matrix->y0 - matrix->yy * matrix->x0 ) / flDet;
	inv.y0 = ( matrix->yx * matrix->x0 - matrix->xx * matrix->y0 ) / flDet;

	*matrix = inv;
	return CAIRO_STATUS_SUCCESS;
}


//-----------------------------------------------------------------------------
// Patterns
//-----------------------------------------------------------------------------
cairo_pattern_t *cairo_pattern_create_rgba( double red, double green, double blue, double alpha )
{
	cairo_pattern_t *pPattern = SEPatternCreate( SE_PATTERN_SOLID );
	if ( pPattern )
	{
		pPattern->m_rgba[0] = red;
		pPattern->m_rgba[1] = green;
		pPattern->m_rgba[2] = blue;
		pPattern->m_rgba[3] = alpha;
	}
	return pPattern;
}

cairo_pattern_t *cairo_pattern_create_linear( double x0, double y0, double x1, double y1 )
{
	cairo_pattern_t *pPattern = SEPatternCreate( SE_PATTERN_LINEAR );
	if ( pPattern )
	{
		pPattern->m_x0 = x0; pPattern->m_y0 = y0;
		pPattern->m_x1 = x1; pPattern->m_y1 = y1;
	}
	return pPattern;
}

cairo_pattern_t *cairo_pattern_create_radial( double cx0, double cy0, double radius0, double cx1, double cy1, double radius1 )
{
	cairo_pattern_t *pPattern = SEPatternCreate( SE_PATTERN_RADIAL );
	if ( pPattern )
	{
		pPattern->m_x0 = cx0; pPattern->m_y0 = cy0; pPattern->m_r0 = radius0;
		pPattern->m_x1 = cx1; pPattern->m_y1 = cy1; pPattern->m_r1 = radius1;
	}
	return pPattern;
}

void cairo_pattern_add_color_stop_rgba( cairo_pattern_t *pattern, double offset, double red, double green, double blue, double alpha )
{
	if ( !pattern )
		return;

	SEStop &stop = pattern->m_Stops.AddToTail();
	stop.m_flOffset = offset;
	stop.m_r = red; stop.m_g = green; stop.m_b = blue; stop.m_a = alpha;

	// keep the stops ordered: the loader adds them in document order
	for ( int i = pattern->m_Stops.Count() - 1; i > 0; --i )
	{
		if ( pattern->m_Stops[i - 1].m_flOffset <= pattern->m_Stops[i].m_flOffset )
			break;

		SEStop temp = pattern->m_Stops[i - 1];
		pattern->m_Stops[i - 1] = pattern->m_Stops[i];
		pattern->m_Stops[i] = temp;
	}
}

void cairo_pattern_set_extend( cairo_pattern_t *pattern, cairo_extend_t extend )
{
	if ( pattern )
		pattern->m_Extend = extend;
}

cairo_extend_t cairo_pattern_get_extend( cairo_pattern_t *pattern )
{
	return pattern ? pattern->m_Extend : CAIRO_EXTEND_NONE;
}

void cairo_pattern_set_matrix( cairo_pattern_t *pattern, const cairo_matrix_t *matrix )
{
	if ( !pattern || !matrix )
		return;

	pattern->m_Matrix = *matrix;
	pattern->m_bHasMatrix = true;
}

void cairo_pattern_destroy( cairo_pattern_t *pattern ) { SEPatternDestroy( pattern ); }


//-----------------------------------------------------------------------------
// Diagnostics
//-----------------------------------------------------------------------------
void cairo_debug_reset_static_data( void )
{
	// the shim keeps no static data; cairo_debug_reset_static_data() exists in the API (and is
	// called by the loader on shutdown) so this is a no-op
}

int SE_PortCairoShimLastPaintPixelCount( void )
{
	return g_nLastPaintPixelCount;
}
