//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef UITEXTSERVICESWIN32_H
#define UITEXTSERVICESWIN32_H

#include "uitextlayoutwin32.h"
// SE port: CS:GO pooled the text layouts with gcsdk's CThreadSafeClassMemoryPool
// (gcsdk/steamextra/tier1/tsmempool.h).  Its implementation (tsmempool.cpp) does not compile against
// Source Engine's tier0 headers, and layouts are short-lived objects created and freed on the render
// thread, so they are allocated directly instead - see the SE port notes in uitextserviceswin32.cpp.

namespace panorama
{

//
// Interface that provides low-level text services
//
class CUITextServicesWin32 : public IUITextServices
{
public:
    CUITextServicesWin32();
    
	virtual void InitializeServices() OVERRIDE;
	virtual void ShutdownServices() OVERRIDE;
	
	virtual bool BLoadCustomFontCollection( const char *pchContainerDir, const char *pchPathForCustomFonts ) OVERRIDE;
	virtual bool BLoadCustomFontFile( const char *pchFontName, const char *pchFullPath ) OVERRIDE;

	virtual IUITextLayout *CreateTextLayout( const void *pRawText, int cbRawText, int cTextChars, EPanoramaTextEncoding eTextEncoding, const TextLayoutParams_t *pParams, UITextLayoutFontMetrics_t *pLayoutMetrics = nullptr );
	virtual void FreeTextLayout( IUITextLayout *pLayout ) OVERRIDE;

	virtual const CUtlSortVector< CUtlString > &GetSortedValidFontNames() OVERRIDE;

	virtual IUITextTextureCache *CreateTextTextureCache( IUITextTextureProvider *pProvider ) OVERRIDE;
	virtual void FreeTextTextureCache( IUITextTextureCache *pCache ) OVERRIDE;

	virtual IUITextLayoutDrawCache *CreateTextLayoutDrawCache( IUITextTextureStorage *pStorage ) OVERRIDE;
	virtual void FreeTextLayoutDrawCache( IUITextLayoutDrawCache *pCache ) OVERRIDE;
};

} // namespace panorama

#endif // UITEXTSERVICESWIN32_H
