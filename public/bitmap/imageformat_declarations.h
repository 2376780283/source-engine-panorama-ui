//=========== SE port shim ==================================================//
//
// CS:GO splits the image format enums (ImageFormat, NormalDecodeMode_t, ...) into
// bitmap/imageformat_declarations.h and has bitmap/imageformat.h pull that in.  This
// Source 2013 tree already defines all of them directly in bitmap/imageformat.h, so
// re-exporting it here keeps CS:GO code (panorama_s1wrapper/wrap_texture.h) compiling
// without duplicating the enums (which would be a C2011 enum redefinition).
//
//=============================================================================//
#ifndef IMAGEFORMAT_DECLARATIONS_H
#define IMAGEFORMAT_DECLARATIONS_H

#include "bitmap/imageformat.h"

#endif // IMAGEFORMAT_DECLARATIONS_H
