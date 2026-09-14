//========= Copyright (c) Valve Corporation, All rights reserved. ============//
//
// Purpose: Minimal cairo surface the SVG loader needs.  SE port shim.
//
//   common/svg/svgloader.cpp is CS:GO's SVG loader verbatim: it parses SVG with parsifal
//   (this port has its own libparifal implementation) and rasterises with cairo.  The engine
//   tree has no cairo and no pixman (only thirdparty/freetype and thirdparty/fontconfig), and
//   building cairo for MSVC/32-bit is a build-system project of its own.
//
//   The loader only uses a small, well defined subset of cairo - paths, a 3x2 transform
//   stack, solid/gradient fills, strokes, clipping, groups and paint - so this shim
//   implements exactly that subset with a software scanline rasteriser and no new
//   dependencies.  Nothing here is cairo's public ABI: only svgloader.cpp uses it, and it is
//   compiled into the panorama modules.
//
//   Format note: CAIRO_FORMAT_ARGB32 is premultiplied BGRA in memory on little-endian hosts,
//   and the loader hands that buffer straight to the caller, so the shim has to produce the
//   same layout that cairo would have.
//
//   Behaviour notes (all chosen to match what svgloader.cpp expects - see se_cairo_shim.cpp):
//     * path points are transformed into device space as they are added, and the current path is
//       NOT part of the saved graphics state.  RenderEllipseElement() relies on that: it does
//       save/translate/scale/arc/restore and only then fills the path.  cairo_clip() clears the
//       path (as cairo does), which is what keeps clip-path children from leaking into the shape
//       that follows.
//     * strokes are expanded into a filled outline, so the pen is never elliptical.
//     * cairo_push_group() creates an intermediate surface with the root target's geometry and
//       keeps the current clip.
//
//=============================================================================//

#ifndef SE_PORT_CAIRO_SHIM_H
#define SE_PORT_CAIRO_SHIM_H

#ifdef _WIN32
#pragma once
#endif

#include <stdint.h>

//-----------------------------------------------------------------------------
// Types
//-----------------------------------------------------------------------------
typedef struct _cairo cairo_t;
typedef struct _cairo_surface cairo_surface_t;
typedef struct _cairo_pattern cairo_pattern_t;

typedef struct cairo_matrix_t
{
	double xx, yx;
	double xy, yy;
	double x0, y0;
} cairo_matrix_t;

typedef enum cairo_status_t
{
	CAIRO_STATUS_SUCCESS = 0,
	CAIRO_STATUS_NO_MEMORY,
	CAIRO_STATUS_INVALID_MATRIX,
	CAIRO_STATUS_INVALID_RESTORE,
	CAIRO_STATUS_INVALID_POP_GROUP,
	CAIRO_STATUS_NULL_POINTER,
} cairo_status_t;

typedef enum cairo_format_t
{
	CAIRO_FORMAT_ARGB32 = 0,
	CAIRO_FORMAT_RGB24,
	CAIRO_FORMAT_A8,
	CAIRO_FORMAT_A1,
} cairo_format_t;

typedef enum cairo_operator_t
{
	CAIRO_OPERATOR_CLEAR = 0,
	CAIRO_OPERATOR_SOURCE,
	CAIRO_OPERATOR_OVER,
	CAIRO_OPERATOR_IN,
	CAIRO_OPERATOR_OUT,
	CAIRO_OPERATOR_ATOP,
	CAIRO_OPERATOR_DEST,
	CAIRO_OPERATOR_DEST_OVER,
	CAIRO_OPERATOR_DEST_IN,
	CAIRO_OPERATOR_DEST_OUT,
	CAIRO_OPERATOR_DEST_ATOP,
	CAIRO_OPERATOR_XOR,
	CAIRO_OPERATOR_ADD,
} cairo_operator_t;

typedef enum cairo_fill_rule_t
{
	CAIRO_FILL_RULE_WINDING = 0,
	CAIRO_FILL_RULE_EVEN_ODD,
} cairo_fill_rule_t;

typedef enum cairo_line_cap_t
{
	CAIRO_LINE_CAP_BUTT = 0,
	CAIRO_LINE_CAP_ROUND,
	CAIRO_LINE_CAP_SQUARE,
} cairo_line_cap_t;

typedef enum cairo_line_join_t
{
	CAIRO_LINE_JOIN_MITER = 0,
	CAIRO_LINE_JOIN_ROUND,
	CAIRO_LINE_JOIN_BEVEL,
} cairo_line_join_t;

typedef enum cairo_extend_t
{
	CAIRO_EXTEND_NONE = 0,
	CAIRO_EXTEND_REPEAT,
	CAIRO_EXTEND_REFLECT,
	CAIRO_EXTEND_PAD,
} cairo_extend_t;

//-----------------------------------------------------------------------------
// Surfaces
//-----------------------------------------------------------------------------
cairo_surface_t *cairo_image_surface_create_for_data( unsigned char *data, cairo_format_t format, int width, int height, int stride );
int cairo_image_surface_get_width( cairo_surface_t *surface );
int cairo_image_surface_get_height( cairo_surface_t *surface );
int cairo_image_surface_get_stride( cairo_surface_t *surface );
unsigned char *cairo_image_surface_get_data( cairo_surface_t *surface );
void cairo_surface_destroy( cairo_surface_t *surface );
cairo_status_t cairo_surface_status( cairo_surface_t *surface );
void cairo_surface_flush( cairo_surface_t *surface );
void cairo_surface_mark_dirty( cairo_surface_t *surface );

//-----------------------------------------------------------------------------
// Context
//-----------------------------------------------------------------------------
cairo_t *cairo_create( cairo_surface_t *target );
void cairo_destroy( cairo_t *cr );
cairo_status_t cairo_status( cairo_t *cr );
cairo_surface_t *cairo_get_target( cairo_t *cr );

void cairo_save( cairo_t *cr );
void cairo_restore( cairo_t *cr );
void cairo_push_group( cairo_t *cr );
void cairo_pop_group_to_source( cairo_t *cr );

void cairo_set_operator( cairo_t *cr, cairo_operator_t op );
void cairo_set_source( cairo_t *cr, cairo_pattern_t *source );
void cairo_set_source_rgba( cairo_t *cr, double red, double green, double blue, double alpha );
void cairo_set_fill_rule( cairo_t *cr, cairo_fill_rule_t fill_rule );
void cairo_set_line_width( cairo_t *cr, double width );
void cairo_set_line_cap( cairo_t *cr, cairo_line_cap_t line_cap );
void cairo_set_line_join( cairo_t *cr, cairo_line_join_t line_join );

//-----------------------------------------------------------------------------
// Paths
//-----------------------------------------------------------------------------
void cairo_new_path( cairo_t *cr );
void cairo_move_to( cairo_t *cr, double x, double y );
void cairo_rel_move_to( cairo_t *cr, double dx, double dy );
void cairo_line_to( cairo_t *cr, double x, double y );
void cairo_rel_line_to( cairo_t *cr, double dx, double dy );
void cairo_curve_to( cairo_t *cr, double x1, double y1, double x2, double y2, double x3, double y3 );
void cairo_rel_curve_to( cairo_t *cr, double dx1, double dy1, double dx2, double dy2, double dx3, double dy3 );
void cairo_rectangle( cairo_t *cr, double x, double y, double width, double height );
void cairo_arc( cairo_t *cr, double xc, double yc, double radius, double angle1, double angle2 );
void cairo_close_path( cairo_t *cr );

int cairo_has_current_point( cairo_t *cr );
void cairo_get_current_point( cairo_t *cr, double *x, double *y );

void cairo_fill( cairo_t *cr );
void cairo_fill_preserve( cairo_t *cr );
void cairo_stroke( cairo_t *cr );
void cairo_clip( cairo_t *cr );
void cairo_paint( cairo_t *cr );
void cairo_paint_with_alpha( cairo_t *cr, double alpha );

//-----------------------------------------------------------------------------
// Transforms
//-----------------------------------------------------------------------------
void cairo_translate( cairo_t *cr, double tx, double ty );
void cairo_scale( cairo_t *cr, double sx, double sy );
void cairo_rotate( cairo_t *cr, double angle );
void cairo_transform( cairo_t *cr, const cairo_matrix_t *matrix );
void cairo_set_matrix( cairo_t *cr, const cairo_matrix_t *matrix );
void cairo_get_matrix( cairo_t *cr, cairo_matrix_t *matrix );
void cairo_identity_matrix( cairo_t *cr );

void cairo_matrix_init( cairo_matrix_t *matrix, double xx, double yx, double xy, double yy, double x0, double y0 );
void cairo_matrix_init_identity( cairo_matrix_t *matrix );
void cairo_matrix_multiply( cairo_matrix_t *result, const cairo_matrix_t *a, const cairo_matrix_t *b );
void cairo_matrix_transform_point( const cairo_matrix_t *matrix, double *x, double *y );
cairo_status_t cairo_matrix_invert( cairo_matrix_t *matrix );

//-----------------------------------------------------------------------------
// Patterns (solid colour, linear and radial gradients)
//-----------------------------------------------------------------------------
cairo_pattern_t *cairo_pattern_create_rgba( double red, double green, double blue, double alpha );
cairo_pattern_t *cairo_pattern_create_linear( double x0, double y0, double x1, double y1 );
cairo_pattern_t *cairo_pattern_create_radial( double cx0, double cy0, double radius0, double cx1, double cy1, double radius1 );
void cairo_pattern_add_color_stop_rgba( cairo_pattern_t *pattern, double offset, double red, double green, double blue, double alpha );
void cairo_pattern_set_extend( cairo_pattern_t *pattern, cairo_extend_t extend );
cairo_extend_t cairo_pattern_get_extend( cairo_pattern_t *pattern );
void cairo_pattern_set_matrix( cairo_pattern_t *pattern, const cairo_matrix_t *matrix );
void cairo_pattern_destroy( cairo_pattern_t *pattern );

//-----------------------------------------------------------------------------
// SW port helpers (not part of cairo)
//-----------------------------------------------------------------------------
// cairo_debug_reset_static_data() is called by the loader on shutdown; this shim keeps no
// static data, so it only exists to satisfy the call site.
void cairo_debug_reset_static_data( void );

// The shim's rasteriser works in "coverage" units internally; this returns how many pixels
// were actually touched by the last fill/stroke, which the port logs once as a sanity check.
int SE_PortCairoShimLastPaintPixelCount( void );

#endif // SE_PORT_CAIRO_SHIM_H
