//========= Copyright Valve Corporation, All rights reserved. ============//
//
// SE port: the include path CS:GO's SVG loader expects.
//
//   common/svg/svgloader.cpp (CS:GO, verbatim) does:
//
//       #include "../public/parsifal/parsifal.h"
//
//   In the CS:GO tree that resolves to its public/parsifal/parsifal.h (the stock libparsifal
//   0.8.3 headers).  This tree has no upstream libparsifal - the tarball is unobtainable - and
//   instead ships an API compatible rewrite under
//   panorama/thirdparty/libparsifal-0.8.3/ (see the header comment there), which the panorama
//   layout loader (panorama/layout/layoutfile.cpp) already uses.
//
//   Forwarding to it from this path keeps svgloader.cpp byte-identical to CS:GO and keeps the
//   parser headers in exactly one place (the rewrite also serves layoutfile.cpp).
//
//=============================================================================//

#ifndef PUBLIC_PARSIFAL_PARSIFAL_H
#define PUBLIC_PARSIFAL_PARSIFAL_H

#include "../../panorama/thirdparty/libparsifal-0.8.3/include/libparsifal/parsifal.h"

#endif // PUBLIC_PARSIFAL_PARSIFAL_H
