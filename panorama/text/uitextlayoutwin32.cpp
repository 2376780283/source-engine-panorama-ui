//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"
#include "uitextlayoutwin32.h"
// NOTE (SE port): CS:GO included "uienginewin32.h" here, which drags in D3D10/D2D/OpenVR
// (that is one of the reasons the win32 text files are $ExcludedFromBuild in panorama.vpc).
// This TU does not reference any symbol from it.
#if defined( PANORAMA_SE_CPU_TEXT )
// SE port: only the font *package* loaders are dropped (they need protobuf-generated sources that
// this tree does not have - see PANORAMA_SE_CPU_TEXT in panorama/wscript), and the Direct2D renderer
// header stays out too (it pulls D3D10/D2D types and CS:GO's container set into this TU).  What the
// CPU path needs instead - the drawing effect - comes from the backend's own header.
#include "seport/se_dwrite_cpu_text.h"
#else
#include "uifontfileloaderwin32.h"
#include "renderer/dwritetextrenderer.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

// Statics
IDWriteFactory *CUITextLayoutWin32::s_pDWriteFactory = NULL;
IDWriteFontCollection *CUITextLayoutWin32::s_pCustomFontCollection = NULL;

// SE port: this tree's CUtlSortVector takes no less-context functor (CS:GO's constructor took
// DefLessFuncCtx(...)), and the default CUtlString comparison is what CS:GO's context compared with.
CUtlSortVector< CUtlString > CUITextLayoutWin32::m_vecSortedValidFontNames;


//-----------------------------------------------------------------------------
// Purpose: Helper to convert an EFontStyle to a DWrite font style
//-----------------------------------------------------------------------------
DWRITE_FONT_STYLE GetDWriteFontStyle( EFontStyle eFontStyle )
{
	DWRITE_FONT_STYLE dwstyle = DWRITE_FONT_STYLE_NORMAL;
	switch( eFontStyle )
	{
	case k_EFontStyleItalic:
		dwstyle = DWRITE_FONT_STYLE_ITALIC;
		break;
	default:
		break;
	}

	return dwstyle;
}


//-----------------------------------------------------------------------------
// Purpose: Helper to convert an EFontWeight to a DWrite font weight
//-----------------------------------------------------------------------------
DWRITE_FONT_WEIGHT GetDWriteFontWeight( EFontWeight eFontWeight )
{
	DWRITE_FONT_WEIGHT dwweight = DWRITE_FONT_WEIGHT_NORMAL;
	switch( eFontWeight )
	{
	case k_EFontWeightMedium:
		dwweight = DWRITE_FONT_WEIGHT_MEDIUM;
		break;
	case k_EFontWeightBold:
		dwweight = DWRITE_FONT_WEIGHT_BOLD;
		break;
	case k_EFontWeightBlack:
		dwweight = DWRITE_FONT_WEIGHT_BLACK;
		break;
	case k_EFontWeightLight:
		dwweight = DWRITE_FONT_WEIGHT_LIGHT;
		break;
	case k_EFontWeightThin:
		dwweight = DWRITE_FONT_WEIGHT_THIN;
		break;
	case k_EFontWeightSemiBold:
		dwweight = DWRITE_FONT_WEIGHT_SEMI_BOLD;
		break;
	default:
		break;
	}

	return dwweight;
}


//-----------------------------------------------------------------------------
// Purpose: SE port helper - this tree has no UCS-4 converter, so UCS-4 text (which the panorama
//			layout/style files never use) is turned into UTF-8 here.
//-----------------------------------------------------------------------------
static void SEConvertUCS4ToUTF8( const uchar32 *pchUCS4, CUtlVector< char > &vecOut )
{
	vecOut.RemoveAll();

	if ( pchUCS4 )
	{
		for ( ; *pchUCS4 != 0; ++pchUCS4 )
		{
			uint32 unChar = (uint32)*pchUCS4;
			char rgchEncoded[4];
			int nBytes = 0;

			if ( unChar < 0x80 )
			{
				rgchEncoded[0] = (char)unChar;
				nBytes = 1;
			}
			else if ( unChar < 0x800 )
			{
				rgchEncoded[0] = (char)( 0xC0 | ( unChar >> 6 ) );
				rgchEncoded[1] = (char)( 0x80 | ( unChar & 0x3F ) );
				nBytes = 2;
			}
			else if ( unChar < 0x10000 )
			{
				rgchEncoded[0] = (char)( 0xE0 | ( unChar >> 12 ) );
				rgchEncoded[1] = (char)( 0x80 | ( ( unChar >> 6 ) & 0x3F ) );
				rgchEncoded[2] = (char)( 0x80 | ( unChar & 0x3F ) );
				nBytes = 3;
			}
			else
			{
				rgchEncoded[0] = (char)( 0xF0 | ( unChar >> 18 ) );
				rgchEncoded[1] = (char)( 0x80 | ( ( unChar >> 12 ) & 0x3F ) );
				rgchEncoded[2] = (char)( 0x80 | ( ( unChar >> 6 ) & 0x3F ) );
				rgchEncoded[3] = (char)( 0x80 | ( unChar & 0x3F ) );
				nBytes = 4;
			}

			for ( int i = 0; i < nBytes; ++i )
				vecOut.AddToTail( rgchEncoded[i] );
		}
	}

	vecOut.AddToTail( '\0' );
}


//-----------------------------------------------------------------------------
// Purpose: Helper to create a DWrite text range
//-----------------------------------------------------------------------------
DWRITE_TEXT_RANGE CUITextLayoutWin32::GetDWriteTextRange( uint unCharStartIndex, uint unCharEndIndex )
{
	if ( unCharStartIndex > unCharEndIndex )
		std::swap( unCharStartIndex, unCharEndIndex );

	uint32 unWCharStartIndex = CharIndexToLayoutWCharIndex( unCharStartIndex );
	uint32 unWCharEndIndex = CharIndexToLayoutWCharIndex( unCharEndIndex );
	
	DWRITE_TEXT_RANGE dwRange = { unWCharStartIndex, unWCharEndIndex - unWCharStartIndex + 1 };
	return dwRange;
}


//-----------------------------------------------------------------------------
// Purpose: Custom drawing effect that tells our text renderer where to draw
//-----------------------------------------------------------------------------
class CTextDrawEffectNoop : public IUnknown
{
public:
	CTextDrawEffectNoop() { m_cRefCount = 1; }
	~CTextDrawEffectNoop() {}

	// IUnknown
	STDMETHOD_(unsigned long, AddRef)()
	{
		return ++m_cRefCount;
	}

	STDMETHOD(QueryInterface)( IID const& riid, void** ppvObject )
	{
		if (__uuidof(IUnknown) == riid)
		{
			*ppvObject = this;
		}
		else
		{
			*ppvObject = NULL;
			return E_FAIL;
		}

		AddRef();
		return S_OK;
	}

	STDMETHOD_(unsigned long, Release)()
	{
		m_cRefCount--;
		if ( m_cRefCount == 0 )
		{
			delete this;
			return 0;
		}

		return m_cRefCount;
	}	

private:
	unsigned long m_cRefCount;	
};


//-----------------------------------------------------------------------------
// Purpose: Custom inline object (reserves height and width in the text layout)
//-----------------------------------------------------------------------------
class CReservedInlineObject : public IDWriteInlineObject
{
public:
	CReservedInlineObject( float flWidth, float flHeight ) { m_cRefCount = 1; m_flWidth = flWidth; m_flHeight = flHeight; }

	IFACEMETHOD( QueryInterface )(IID const& riid, void** ppvObject)
	{
		if ( __uuidof(IUnknown) == riid || __uuidof(IDWriteInlineObject) == riid )
		{
			AddRef();
			*ppvObject = this;
			return S_OK;
		}
		*ppvObject = NULL;
		return E_FAIL;
	}

	IFACEMETHOD_( unsigned long, AddRef )()
	{
		return ++m_cRefCount;
	}

	IFACEMETHOD_( unsigned long, Release )()
	{
		if ( --m_cRefCount == 0 )
		{
			delete this;
			return 0;
		}
		return m_cRefCount;
	}

	IFACEMETHOD( Draw )( void* clientDrawingContext, IDWriteTextRenderer* renderer, FLOAT originX, FLOAT originY, BOOL isSideways, BOOL isRightToLeft, IUnknown* clientDrawingEffect )
	{
		return S_OK;
	}

	IFACEMETHOD( GetMetrics )( OUT DWRITE_INLINE_OBJECT_METRICS* metrics )
	{
		metrics->width = m_flWidth;
		metrics->height = m_flHeight;
		metrics->baseline = m_flHeight;
		metrics->supportsSideways = FALSE;
		return S_OK;
	}

	IFACEMETHOD( GetOverhangMetrics )( OUT DWRITE_OVERHANG_METRICS* overhangs )
	{
		overhangs->left = 0;
		overhangs->top = 0;
		overhangs->right  = 0;
		overhangs->bottom = 0;
		return S_OK;
	}

	IFACEMETHOD( GetBreakConditions )( OUT DWRITE_BREAK_CONDITION* breakConditionBefore, OUT DWRITE_BREAK_CONDITION* breakConditionAfter )
	{
		*breakConditionBefore = DWRITE_BREAK_CONDITION_NEUTRAL;
		*breakConditionAfter = DWRITE_BREAK_CONDITION_NEUTRAL;
		return S_OK;
	}

private:
	unsigned long m_cRefCount;
	float m_flWidth;
	float m_flHeight;
};


//-----------------------------------------------------------------------------
// Purpose: manually declare the IDWriteTextLayout1 layout so we don't need to update to the Win8 DX sdk
//-----------------------------------------------------------------------------
interface DWRITE_DECLARE_INTERFACE( "9064D822-80A7-465C-A986-DF65F78B8FEB" ) IDWriteTextLayout1 : public IDWriteTextLayout
{
	/// <summary>
	/// Enables/disables pair-kerning on the given range.
	/// </summary>
	/// <param name="isPairKerningEnabled">The Boolean flag indicates whether text is pair-kerned.</param>
	/// <param name="textRange">Text range to which this change applies.</param>
	/// <returns>
	/// Standard HRESULT error code.
	/// </returns>
	STDMETHOD( SetPairKerning )(
	BOOL isPairKerningEnabled,
	DWRITE_TEXT_RANGE textRange
	) PURE;

	/// <summary>
	/// Get whether or not pair-kerning is enabled at given position.
	/// </summary>
	/// <param name="currentPosition">The current text position.</param>
	/// <param name="isPairKerningEnabled">The Boolean flag indicates whether text is pair-kerned.</param>
	/// <param name="textRange">The position range of the current format.</param>
	/// <returns>
	/// Standard HRESULT error code.
	/// </returns>
	STDMETHOD( GetPairKerning )(
		UINT32 currentPosition,
		_Out_ BOOL* isPairKerningEnabled,
		_Out_opt_ DWRITE_TEXT_RANGE* textRange = NULL
		) PURE;

	/// <summary>
	/// Sets the spacing between characters.
	/// </summary>
	/// <param name="leadingSpacing">The spacing before each character, in reading order.</param>
	/// <param name="trailingSpacing">The spacing after each character, in reading order.</param>
	/// <param name="minimumAdvanceWidth">The minimum advance of each character,
	///     to prevent characters from becoming too thin or zero-width. This
	///     must be zero or greater.</param>
	/// <param name="textRange">Text range to which this change applies.</param>
	/// <returns>
	/// Standard HRESULT error code.
	/// </returns>
	STDMETHOD( SetCharacterSpacing )(
		FLOAT leadingSpacing,
		FLOAT trailingSpacing,
		FLOAT minimumAdvanceWidth,
		DWRITE_TEXT_RANGE textRange
		) PURE;

	/// <summary>
	/// Gets the spacing between characters.
	/// </summary>
	/// <param name="currentPosition">The current text position.</param>
	/// <param name="leadingSpacing">The spacing before each character, in reading order.</param>
	/// <param name="trailingSpacing">The spacing after each character, in reading order.</param>
	/// <param name="minimumAdvanceWidth">The minimum advance of each character,
	///     to prevent characters from becoming too thin or zero-width. This
	///     must be zero or greater.</param>
	/// <param name="textRange">The position range of the current format.</param>
	/// <returns>
	/// Standard HRESULT error code.
	/// </returns>
	STDMETHOD( GetCharacterSpacing )(
		UINT32 currentPosition,
		_Out_ FLOAT* leadingSpacing,
		_Out_ FLOAT* trailingSpacing,
		_Out_ FLOAT* minimumAdvanceWidth,
		_Out_opt_ DWRITE_TEXT_RANGE* textRange = NULL
		) PURE;
};


//-----------------------------------------------------------------------------
// Purpose: Init globals
//-----------------------------------------------------------------------------
bool CUITextLayoutWin32::BInitGlobals()
{
	VPROF_BUDGET( "CUITextLayoutWin32::BInitGlobals()", VPROF_BUDGETGROUP_TENFOOT );
	if ( s_pDWriteFactory == NULL )
	{
#if defined( PANORAMA_SE_CPU_TEXT )
		// SE port: g_DWriteCreateFactory is set up by CUIEngineWin32::BInitialize(), which is the
		// Source 1 backend this port does not use, so DWrite is loaded here directly.
		typedef HRESULT ( WINAPI *tDWriteCreateFactoryFn )( DWRITE_FACTORY_TYPE factoryType, REFIID iid, IUnknown **factory );

		static HMODULE hModDWrite = LoadLibraryA( "DWrite.dll" );
		if ( !hModDWrite )
		{
			Warning( "CUITextLayoutWin32::BInitGlobals(): DWrite.dll is not available\n" );
			return false;
		}

		tDWriteCreateFactoryFn pfnDWriteCreateFactory = (tDWriteCreateFactoryFn)GetProcAddress( hModDWrite, "DWriteCreateFactory" );
		if ( !pfnDWriteCreateFactory )
			return false;

		if ( FAILED( pfnDWriteCreateFactory( DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown **)&s_pDWriteFactory ) ) )
		{
			s_pDWriteFactory = NULL;
			return false;
		}
#else
		if ( FAILED( g_DWriteCreateFactory( DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown **)&s_pDWriteFactory ) ) )
		{
			s_pDWriteFactory = NULL;
			return false;
		}

		// Singleton instance
		UIFontCollectionLoader::SetInstance( new UIFontCollectionLoader() );
		UIFontFileLoader::SetInstance( new UIFontFileLoader() );
#endif
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Load up custom font collection
//-----------------------------------------------------------------------------
bool CUITextLayoutWin32::BLoadCustomFontCollection( const char *pchPathForCustomFonts )
{
	CUITextLayoutWin32::BInitGlobals();

#if defined( PANORAMA_SE_CPU_TEXT )
	// SE port: custom font collections come out of CS:GO's encrypted font *packages*, which are
	// implemented by uifontfileloaderwin32.cpp + steamextra/common/uifontfile.cpp (protobuf
	// generated sources that this tree does not have).  System fonts keep working.
	Warning( "CUITextLayoutWin32::BLoadCustomFontCollection( \"%s\" ): custom font collections are not "
			 "supported by this build; using the system font collection.\n", pchPathForCustomFonts ? pchPathForCustomFonts : "?" );
	return false;
#else
	DbgVerify( SUCCEEDED( s_pDWriteFactory->RegisterFontFileLoader( UIFontFileLoader::GetLoader() ) ) );
	DbgVerify( SUCCEEDED( s_pDWriteFactory->RegisterFontCollectionLoader( UIFontCollectionLoader::GetLoader() ) ) );

	HRESULT hr = s_pDWriteFactory->CreateCustomFontCollection( UIFontCollectionLoader::GetLoader(), pchPathForCustomFonts, V_strlen( pchPathForCustomFonts )+1, &s_pCustomFontCollection );
	if ( SUCCEEDED( hr ) )
	{
		return true;
	}
	else
	{
		AssertMsg2( false, "CreateCustomFontCollection failed for %s, hr: 0x%X", pchPathForCustomFonts, hr );
		s_pDWriteFactory->UnregisterFontCollectionLoader( UIFontCollectionLoader::GetLoader() );
		s_pDWriteFactory->UnregisterFontFileLoader( UIFontFileLoader::GetLoader() );
		s_pCustomFontCollection = NULL;
		return false;
	}
#endif // PANORAMA_SE_CPU_TEXT
}


//-----------------------------------------------------------------------------
// Purpose: Calculates the dimenions and character offsets for clipping text
//-----------------------------------------------------------------------------
#if !defined( PANORAMA_SE_CPU_TEXT )
// SE port: only the Direct2D draw path clipped against the line metrics, so this (and
// CalculatedTextClip_t, which lives in renderer/dwritetextrenderer.h) is not compiled here.
bool BCalculateTextClip( CalculatedTextClip_t *pResults, IDWriteTextLayout *pDWriteLayout, float flClipHeight )
{
	VPROF_BUDGET( "BCalculateTextClip", VPROF_BUDGETGROUP_STEAMUI );
	// init
	pResults->iFirstChar = 0;
	pResults->iLastChar = 0;
	pResults->flFirstLineOffset = 0.0f;
	pResults->flLastLineOffset = 0.0f;

	// get line metrics
	UINT32 unMaxLineCount = 10;
	DWRITE_LINE_METRICS lineMetrics[10];
	DWRITE_LINE_METRICS *pMetricsAlloced = lineMetrics;

	HRESULT hr = pDWriteLayout->GetLineMetrics( pMetricsAlloced, unMaxLineCount, &unMaxLineCount );
	if ( hr == HRESULT_FROM_WIN32( ERROR_INSUFFICIENT_BUFFER ) && unMaxLineCount > 0 )
	{
		pMetricsAlloced = new DWRITE_LINE_METRICS[unMaxLineCount];
		hr = pDWriteLayout->GetLineMetrics( pMetricsAlloced, unMaxLineCount, &unMaxLineCount );
	}

	if ( FAILED( hr ) )
		return false;

	float flYPosition = 0.0f;
	int iCurrentLineStart = 0;
	bool bFirstLineFound = false;
	bool bLastLineFound = false;

	for ( UINT32 i = 0; i < unMaxLineCount; ++i )
	{
		// Skip lines that are out of bounds completely
		if ( flYPosition <= flClipHeight )
		{
			if ( !bFirstLineFound )
			{
				bFirstLineFound = true;
				pResults->flFirstLineOffset = flYPosition;
				pResults->iFirstChar = iCurrentLineStart;
			}
		}
		else
		{
			// If we found one line, and are not outside of bounds again, we are done.
			if ( bFirstLineFound && !bLastLineFound )
			{
				bLastLineFound = true;
				pResults->flLastLineOffset = flYPosition + pMetricsAlloced[i].height;
				pResults->iLastChar = iCurrentLineStart + pMetricsAlloced[i].length - 1;
				break;
			}
		}

		flYPosition += pMetricsAlloced[i].height;
		iCurrentLineStart += pMetricsAlloced[i].length;
	}

	// Might have not found the last line, since we hit end of vector if all lines are visible.
	if ( bFirstLineFound && !bLastLineFound )
	{
		pResults->flLastLineOffset = flYPosition;
		pResults->iLastChar = iCurrentLineStart;
	}

	if ( pMetricsAlloced != lineMetrics )
		delete[] pMetricsAlloced;

	return true;
}
#endif // PANORAMA_SE_CPU_TEXT


//-----------------------------------------------------------------------------
// Purpose: Draw 
//-----------------------------------------------------------------------------
bool CUITextLayoutWin32::BDraw( CUtlVector<UITextOpacityMaskDataRange_t> &drawRanges, const UITextFormatProperties_t *pFormatProps, int cFormatProps, IUITextTextureStorage *pStorage, float flHeight, void *pRenderContext )
{
#if defined( PANORAMA_SE_CPU_TEXT )
	// SE port: rasterise straight into the surface's text atlas instead of going through the
	// Direct2D render target the CS:GO renderer expects (see seport/se_dwrite_cpu_text.cpp).
	NOTE_UNUSED( pFormatProps );
	NOTE_UNUSED( cFormatProps );
	NOTE_UNUSED( pRenderContext );
	return SEDrawTextLayoutToAlphaMasks( this, drawRanges, flHeight, pStorage );
#else
	CalculatedTextClip_t textClip;
	if ( !BCalculateTextClip( &textClip, GetDWriteTextLayout(), flHeight ) )
	{
		AssertMsg( false, "Failed getting line metrics for text" );
		return false;
	}


	{
		VPROF_BUDGET( "CD3D10D2DSurface::GetCachedTextOpacityMask - render line", VPROF_BUDGETGROUP_TENFOOT );

		DWriteTextRendererContext_t renderContext;
		renderContext.m_pTextLayout = this;
		renderContext.m_textClip = textClip;

		CDWriteTextRenderer *pRender = (CDWriteTextRenderer*)pRenderContext;

		D2D1_POINT_2F alphaTexOrigin = { 0, 0 };
		pRender->BeginDraw( &drawRanges );
		HRESULT hr = GetDWriteTextLayout()->Draw( &renderContext, pRender, alphaTexOrigin.x, alphaTexOrigin.y );
		pRender->EndDraw();
		Assert( SUCCEEDED( hr ) );
	}
	return true;
#endif // PANORAMA_SE_CPU_TEXT
}


//-----------------------------------------------------------------------------
// Purpose: Free globals
//-----------------------------------------------------------------------------
void CUITextLayoutWin32::FreeGlobals()
{
	if ( s_pCustomFontCollection )
	{
		SAFE_RELEASE( s_pCustomFontCollection );

#if !defined( PANORAMA_SE_CPU_TEXT )
		s_pDWriteFactory->UnregisterFontCollectionLoader( UIFontCollectionLoader::GetLoader() );
		s_pDWriteFactory->UnregisterFontFileLoader( UIFontFileLoader::GetLoader() );
		UIFontCollectionLoader::ReleaseInstance();
		UIFontFileLoader::ReleaseInstance();
#endif
	}

	SAFE_RELEASE( s_pDWriteFactory );
}


//-----------------------------------------------------------------------------
// Purpose: Get list of valid font names, sorted
//-----------------------------------------------------------------------------
const CUtlSortVector< CUtlString > &CUITextLayoutWin32::GetSortedValidFontNames()
{
	if ( m_vecSortedValidFontNames.Count() == 0 )
	{
		CUITextLayoutWin32::BInitGlobals();

		IDWriteFontCollection * rgFontCollections[2] = { NULL, NULL };
		rgFontCollections[0] = s_pCustomFontCollection;
		s_pDWriteFactory->GetSystemFontCollection( &(rgFontCollections[1]) );

		for( int iCollection=0; iCollection < V_ARRAYSIZE( rgFontCollections ); ++iCollection )
		{
			IDWriteFontCollection *pFontCollection = rgFontCollections[iCollection];

			// SE port: with PANORAMA_SE_CPU_TEXT there is no custom collection (custom font
			// packages are not supported by this build), so the entry stays NULL.
			if ( !pFontCollection )
				continue;

			UINT32 familyCount = pFontCollection->GetFontFamilyCount();
			for ( UINT32 i = 0; i < familyCount; ++i )
			{
				IDWriteFontFamily* pFontFamily = NULL;
				HRESULT hres = pFontCollection->GetFontFamily( i, &pFontFamily );
				if ( SUCCEEDED( hres ) )
				{
					if ( iCollection == 0 )
					{
						Msg( "Found a font family in custom collection\n" );
					}
					IDWriteLocalizedStrings* pFamilyNames = NULL;
					hres = pFontFamily->GetFamilyNames(&pFamilyNames);

					UINT32 index = 0;
					BOOL exists = false;

					wchar_t localeName[LOCALE_NAME_MAX_LENGTH];
					localeName[0] = 0;

					if ( SUCCEEDED(hres) )
					{
						hres = pFamilyNames->FindLocaleName( L"en-us", &index, &exists );

						// Get the string length.
						if ( SUCCEEDED(hres) )
						{
							UINT32 length = 0;
							hres = pFamilyNames->GetStringLength(index, &length);

							if ( SUCCEEDED( hres ) )
							{
								wchar_t* pwchName = new wchar_t[length+1];
								hres = pFamilyNames->GetString(index, pwchName, length+1);
								if ( SUCCEEDED( hres ) )
								{
									CStrAutoEncode strEncode( pwchName );
									m_vecSortedValidFontNames.Insert( CUtlString( strEncode.ToString() ) );
								}
								delete[] pwchName;
							}
						}

						pFamilyNames->Release();
					}
					pFontFamily->Release();
				}
			}
		}

		if ( rgFontCollections[1] )
			rgFontCollections[1]->Release();
		
	}

	return m_vecSortedValidFontNames;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CUITextLayoutWin32::CUITextLayoutWin32()
{
	m_pTextFormat = NULL;
	m_pTextLayout = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CUITextLayoutWin32::~CUITextLayoutWin32()
{
	SAFE_RELEASE( m_pTextFormat );
	SAFE_RELEASE( m_pTextLayout );
}


//-----------------------------------------------------------------------------
// Purpose: Initialization
//-----------------------------------------------------------------------------
bool CUITextLayoutWin32::BInitialize( const void *pRawText, int cbRawText, int cTextChars, EPanoramaTextEncoding eTextEncoding, const IUITextServices::TextLayoutParams_t *pParams, UITextLayoutFontMetrics_t *pLayoutMetrics )
{
	VPROF_BUDGET_DETAILED( "CUITextLayoutWin32::BInitialize", VPROF_BUDGETGROUP_TENFOOT );
	if ( !s_pDWriteFactory )
		return false;

	Assert( pLayoutMetrics == nullptr );

	// SE port: Source Engine's CStrAutoEncode has no Set()/ToUTF16() - it is constructed from the
	// string it converts, and this tree has no UCS-4 helper on it either, so UCS-4 text is turned
	// into UTF-8 first and the converter is built from that (it has to outlive the conversion).
	CUtlVector< char > vecUTF8Text;
	if ( eTextEncoding == k_EPanoramaTextEncodingUChar32 )
	{
		SEConvertUCS4ToUTF8( (const uchar32 *)pRawText, vecUTF8Text );
	}

	CStrAutoEncode strEncode( ( eTextEncoding == k_EPanoramaTextEncodingUChar32 ) ? vecUTF8Text.Base() : (const char *)pRawText );
	const wchar_t *pwchText;

	switch( eTextEncoding )
	{
	case k_EPanoramaTextEncodingUTF8:
		pwchText = strEncode.ToWString();
		break;
	case k_EPanoramaTextEncodingUChar16:
		pwchText = (const wchar_t*)pRawText;
		break;
	case k_EPanoramaTextEncodingUChar32:
		pwchText = strEncode.ToWString();
		break;
	default:
		AssertMsg( false, "Unknown text encoding" );
		return false;
	}

	DWRITE_FONT_WEIGHT dwweight = GetDWriteFontWeight( pParams->m_weight );
	DWRITE_FONT_STYLE dwstyle = GetDWriteFontStyle( pParams->m_style );

	CStrAutoEncode strName( pParams->m_pchFontName );

	if ( FAILED( s_pDWriteFactory->CreateTextFormat( strName.ToWString(), s_pCustomFontCollection, dwweight, dwstyle, DWRITE_FONT_STRETCH_NORMAL, pParams->m_flSize, L"", &m_pTextFormat ) ) )
	{
		if ( FAILED( s_pDWriteFactory->CreateTextFormat( strName.ToWString(), NULL, dwweight, dwstyle, DWRITE_FONT_STRETCH_NORMAL, pParams->m_flSize, L"", &m_pTextFormat ) ) )
		{
			m_pTextFormat = NULL;
			return false;
		}
	}

	if ( pParams->m_flLineHeight != k_flFloatNotSet )
	{
		DWRITE_LINE_SPACING_METHOD method = DWRITE_LINE_SPACING_METHOD_UNIFORM;
		float flBaseline = 0.8f * pParams->m_flLineHeight;
		m_pTextFormat->SetLineSpacing( method, pParams->m_flLineHeight, flBaseline );
	}

	if ( !pParams->m_bWrap )
		m_pTextFormat->SetWordWrapping( DWRITE_WORD_WRAPPING_NO_WRAP );
	else
		m_pTextFormat->SetWordWrapping( DWRITE_WORD_WRAPPING_WRAP );

	DWRITE_TRIMMING trimming = { DWRITE_TRIMMING_GRANULARITY_NONE, 0, 0 };
	IDWriteInlineObject *inlineObject = NULL;
	if ( pParams->m_bEllipsis )
	{
		trimming.granularity = DWRITE_TRIMMING_GRANULARITY_CHARACTER;
		s_pDWriteFactory->CreateEllipsisTrimmingSign( m_pTextFormat, &inlineObject );
	}

	m_pTextFormat->SetTrimming( &trimming, inlineObject );
	SAFE_RELEASE( inlineObject );

	switch ( pParams->m_align )
	{
	case k_ETextAlignLeft:
		m_pTextFormat->SetTextAlignment( DWRITE_TEXT_ALIGNMENT_LEADING );
		break;
	case k_ETextAlignRight:
		m_pTextFormat->SetTextAlignment( DWRITE_TEXT_ALIGNMENT_TRAILING );
		break;
	case k_ETextAlignCenter:
		m_pTextFormat->SetTextAlignment( DWRITE_TEXT_ALIGNMENT_CENTER );
		break;
	default:
		break;
	}

	m_pTextFormat->SetParagraphAlignment( DWRITE_PARAGRAPH_ALIGNMENT_NEAR );

	if ( FAILED( s_pDWriteFactory->CreateTextLayout( pwchText, V_wcslen( pwchText ), m_pTextFormat, pParams->m_flMaxWidth, pParams->m_flMaxHeight, &m_pTextLayout ) ) )
	{
		m_pTextLayout = NULL;
		return false;
	}

	if ( pParams->m_nLetterSpacing && m_pTextLayout )
	{
		IDWriteTextLayout1 *pLayout;
		if ( m_pTextLayout->QueryInterface( __uuidof(IDWriteTextLayout1), (LPVOID *)&pLayout ) == S_OK && pLayout )
		{
			HRESULT hr = pLayout->SetCharacterSpacing( (float)pParams->m_nLetterSpacing / 2, (float)pParams->m_nLetterSpacing / 2, 0, { 0, UINT_MAX } );
			Assert( hr == S_OK );
			pLayout->Release();
		}
	}

	m_vecLayoutText16.CopyArray( pwchText, V_wcslen( pwchText ) + 1 );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Gets the required size to fully draw the text layout
//-----------------------------------------------------------------------------
void CUITextLayoutWin32::GetRequiredSize( float &flWidth, float &flHeight )
{
	VPROF_BUDGET_DETAILED( "CUITextLayoutWin32::GetRequiredSize", VPROF_BUDGETGROUP_TENFOOT );
	DWRITE_TEXT_METRICS metrics;
	if ( !m_pTextLayout || FAILED( m_pTextLayout->GetMetrics( &metrics ) ) )
	{
		flWidth = 0;
		flHeight = 0;
		return;
	}

	flWidth = metrics.width;
	flHeight = metrics.height;
}


//-----------------------------------------------------------------------------
// Purpose:  Hit tests a point against the text layout
//  
// unHitRunLength returns the character index hit
// bIsTrailingHit indicates whether the hit is on the leading or trailing side of the char
// bInside is true if the hit is within the text region, when false the position closest the point is returned as
// the character hit offset
//-----------------------------------------------------------------------------
void CUITextLayoutWin32::HitTestPoint( Vector2D point, uint32 &unFirstHitOffset, bool &bIsTrailingHit, bool &bIsInsideString )
{
	BOOL bIsTrailing = FALSE;
	BOOL bIsInside = FALSE;
	DWRITE_HIT_TEST_METRICS metrics;
	if ( !m_pTextLayout || FAILED( m_pTextLayout->HitTestPoint( point.x, point.y, &bIsTrailing, &bIsInside, &metrics ) ) )
	{
		bIsTrailingHit = false;
		bIsInsideString = false;
		unFirstHitOffset = 0;
	}
	else
	{
		bIsTrailingHit = bIsTrailing ? true : false;
		bIsInsideString = bIsInside ? true : false;
		unFirstHitOffset = LayoutWCharIndexToCharIndex( metrics.textPosition );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Determines the layout coordinates for a given character offset, 
// coordinates are relative to top left of text layout
//-----------------------------------------------------------------------------
void CUITextLayoutWin32::GetCharacterCoordinates( uint32 unCharIndex, IUITextLayout::HitTestRegionRect_t &charRegionRect )
{
	FLOAT x;
	FLOAT y;
	DWRITE_HIT_TEST_METRICS metrics;
	if ( !m_pTextLayout || FAILED( m_pTextLayout->HitTestTextPosition( CharIndexToLayoutWCharIndex( unCharIndex ), FALSE, &x, &y, &metrics ) ) )
	{
		charRegionRect.topLeft.x = 0.0f;
		charRegionRect.topLeft.y = 0.0f;
		charRegionRect.bottomRight.x = 0.0f;
		charRegionRect.bottomRight.y = 0.0f;
	}
	else
	{
		charRegionRect.topLeft.x = metrics.left;
		charRegionRect.topLeft.y = metrics.top;
		charRegionRect.bottomRight.x = metrics.left+metrics.width;
		charRegionRect.bottomRight.y = metrics.top+metrics.height;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Determines a vector of rects enclosing a range of text, normally 
// used for getting selection highlight regions
//-----------------------------------------------------------------------------
void CUITextLayoutWin32::GetCharacterRangeCoordinates( uint32 unCharStartIndex, uint32 unCharEndIndex, CUtlVector<IUITextLayout::HitTestRegionRect_t> &vecRangeRegionRects ) 
{
	vecRangeRegionRects.RemoveAll();

	if ( !m_pTextLayout )
		return;

	FLOAT fXOffset = 0.0f;
	FLOAT fYOffset = 0.0f;

	DWRITE_HIT_TEST_METRICS metrics[10];
	DWRITE_HIT_TEST_METRICS *pMetricsAlloced = metrics;
	UINT32 unActualHitTestMetrics = 0;

	// DWrite wants these in order, we don't care though
	if ( unCharStartIndex > unCharEndIndex )
		std::swap( unCharStartIndex, unCharEndIndex );

	uint32 unWCharStartIndex = CharIndexToLayoutWCharIndex( unCharStartIndex );
	uint32 unWCharEndIndex = CharIndexToLayoutWCharIndex( unCharEndIndex );

	HRESULT hr = m_pTextLayout->HitTestTextRange( unWCharStartIndex, unWCharEndIndex-unWCharStartIndex, fXOffset, fYOffset, pMetricsAlloced, V_ARRAYSIZE( metrics ), &unActualHitTestMetrics );
	if ( hr == HRESULT_FROM_WIN32( ERROR_INSUFFICIENT_BUFFER ) && unActualHitTestMetrics > 0 )
	{
		pMetricsAlloced = new DWRITE_HIT_TEST_METRICS[ unActualHitTestMetrics ];
		hr = m_pTextLayout->HitTestTextRange( unWCharStartIndex, unWCharEndIndex-unWCharStartIndex, fXOffset, fYOffset, pMetricsAlloced, unActualHitTestMetrics, &unActualHitTestMetrics );
	}

	if ( SUCCEEDED( hr ) )
	{
		for( UINT32 i=0; i < unActualHitTestMetrics; ++i )
		{
			int iRect = vecRangeRegionRects.AddToTail();
			vecRangeRegionRects[iRect].topLeft.x = pMetricsAlloced[i].left;
			vecRangeRegionRects[iRect].topLeft.y = pMetricsAlloced[i].top;
			vecRangeRegionRects[iRect].bottomRight.x = pMetricsAlloced[i].left + pMetricsAlloced[i].width;
			vecRangeRegionRects[iRect].bottomRight.y = pMetricsAlloced[i].top + pMetricsAlloced[i].height;

			// returning length to match arguments (so for a single char, will be 0,1)
			vecRangeRegionRects[iRect].unCharStart = LayoutWCharIndexToCharIndex( pMetricsAlloced[i].textPosition );
			vecRangeRegionRects[iRect].unCharEnd = LayoutWCharIndexToCharIndex( pMetricsAlloced[i].textPosition + pMetricsAlloced[i].length );

			vecRangeRegionRects[iRect].bIsText = (pMetricsAlloced[i].isText == TRUE);
			vecRangeRegionRects[iRect].bIsTrimmed = (pMetricsAlloced[i].isTrimmed == TRUE);
		}
	}

	if ( pMetricsAlloced != metrics )
		delete[] pMetricsAlloced;
}


//-----------------------------------------------------------------------------
// Purpose: Sets the font name for a specified character range
//-----------------------------------------------------------------------------
void CUITextLayoutWin32::SetFontName( uint32 unCharStartIndex, uint32 unCharEndIndex, const char *pchFontName )
{
	Assert( m_pTextLayout );
	if ( !m_pTextLayout )
		return;

	DWRITE_TEXT_RANGE dwRange = GetDWriteTextRange( unCharStartIndex, unCharEndIndex );
	HRESULT hr = m_pTextLayout->SetFontFamilyName( CStrAutoEncode( pchFontName ).ToWString(), dwRange );
	Assert( SUCCEEDED( hr ) );
}


//-----------------------------------------------------------------------------
// Purpose: Sets the font size for a specified character range
//-----------------------------------------------------------------------------
void CUITextLayoutWin32::SetFontSize( uint32 unCharStartIndex, uint32 unCharEndIndex, float flFontSize )
{
	Assert( m_pTextLayout );
	if ( !m_pTextLayout )
		return;

	DWRITE_TEXT_RANGE dwRange = GetDWriteTextRange( unCharStartIndex, unCharEndIndex );
	HRESULT hr = m_pTextLayout->SetFontSize( flFontSize, dwRange );
	Assert( SUCCEEDED( hr ) );
}


//-----------------------------------------------------------------------------
// Purpose: Sets the font style for a specified character range
//-----------------------------------------------------------------------------
void CUITextLayoutWin32::SetFontStyle( uint32 unCharStartIndex, uint32 unCharEndIndex, EFontStyle eFontStyle )
{
	Assert( m_pTextLayout );
	if ( !m_pTextLayout )
		return;

	DWRITE_FONT_STYLE dwStyle = GetDWriteFontStyle( eFontStyle );
	DWRITE_TEXT_RANGE dwRange = GetDWriteTextRange( unCharStartIndex, unCharEndIndex );
	HRESULT hr = m_pTextLayout->SetFontStyle( dwStyle, dwRange );
	Assert( SUCCEEDED( hr ) );
}


//-----------------------------------------------------------------------------
// Purpose: Sets the font weight for a specified character range
//-----------------------------------------------------------------------------
void CUITextLayoutWin32::SetFontWeight( uint32 unCharStartIndex, uint32 unCharEndIndex, EFontWeight eFontWeight )
{
	Assert( m_pTextLayout );
	if ( !m_pTextLayout )
		return;

	DWRITE_FONT_WEIGHT dwWeight = GetDWriteFontWeight( eFontWeight );
	DWRITE_TEXT_RANGE dwRange = GetDWriteTextRange( unCharStartIndex, unCharEndIndex );
	HRESULT hr = m_pTextLayout->SetFontWeight( dwWeight, dwRange );
	Assert( SUCCEEDED( hr ) );
}


//-----------------------------------------------------------------------------
// Purpose: Underlines the specified character range
//-----------------------------------------------------------------------------
void CUITextLayoutWin32::SetUnderline( uint32 unCharStartIndex, uint32 unCharEndIndex, bool bUnderline )
{
	Assert( m_pTextLayout );
	if ( !m_pTextLayout )
		return;

	DWRITE_TEXT_RANGE dwRange = GetDWriteTextRange( unCharStartIndex, unCharEndIndex );
	HRESULT hr = m_pTextLayout->SetUnderline( bUnderline ? TRUE : FALSE, dwRange );
	Assert( SUCCEEDED( hr ) );
}


//-----------------------------------------------------------------------------
// Purpose: Sets strikethrough on the specified character range
//-----------------------------------------------------------------------------
void CUITextLayoutWin32::SetStrikethrough( uint32 unCharStartIndex, uint32 unCharEndIndex, bool bStrikethrough )
{
	Assert( m_pTextLayout );
	if ( !m_pTextLayout )
		return;

	DWRITE_TEXT_RANGE dwRange = GetDWriteTextRange( unCharStartIndex, unCharEndIndex );
	HRESULT hr = m_pTextLayout->SetStrikethrough( bStrikethrough ? TRUE : FALSE, dwRange );
	Assert( SUCCEEDED( hr ) );
}


//-----------------------------------------------------------------------------
// Purpose: Indicates that an inline shape should replace the character at that given index
//-----------------------------------------------------------------------------
void CUITextLayoutWin32::SetInlineObject( uint32 unCharIndex, float flWidth, float flHeight )
{
	Assert( m_pTextLayout );
	if ( !m_pTextLayout )
		return;

	IDWriteInlineObject *pObj = new CReservedInlineObject( flWidth, flHeight );
	DWRITE_TEXT_RANGE dwRange = GetDWriteTextRange( unCharIndex, unCharIndex );
	HRESULT hr = m_pTextLayout->SetInlineObject( pObj, dwRange );
	Assert( SUCCEEDED( hr ) );

	SAFE_RELEASE( pObj );
}


//-----------------------------------------------------------------------------
// Purpose: Marks a range of text needing a color. This will affect measurement
//-----------------------------------------------------------------------------
void CUITextLayoutWin32::MarkColorRangeForMeasurement( uint32 unCharStartIndex, uint32 unCharEndIndex, int iColorIndex )
{
	Assert( m_pTextLayout );
	if ( !m_pTextLayout )
		return;

#if defined( PANORAMA_SE_CPU_TEXT )
	CSECTextDrawingEffect *pEffect = new CSECTextDrawingEffect( iColorIndex );
#else
	CTextDrawingEffect *pEffect = new CTextDrawingEffect( iColorIndex );
#endif
	DWRITE_TEXT_RANGE dwRange = GetDWriteTextRange( unCharStartIndex, unCharEndIndex );
	HRESULT hr = m_pTextLayout->SetDrawingEffect( pEffect, dwRange );
	Assert( SUCCEEDED( hr ) );

	SAFE_RELEASE( pEffect );
}


//-----------------------------------------------------------------------------
// Purpose: Convert wchar_t index from DWrite to char index in string
//-----------------------------------------------------------------------------
uint32 CUITextLayoutWin32::LayoutWCharIndexToCharIndex( uint32 unWCharIndex )
{
	const wchar_t *pwch = m_vecLayoutText16.Base();
	if ( !pwch || pwch[0] == 0 || unWCharIndex == 0 )
		return 0;

#ifdef DBGFLAG_ASSERT
	const char *pwchStart = pwch;
#endif
	int64 nWCharLeft = (int64)unWCharIndex;
	uint32 unCharIndex = 0;
	while( nWCharLeft > 0 )
	{
		if ( !pwch[0] )
		{
			AssertMsg( false, "Layout wchar_t index %u out of range (%u text bytes)",
					   unWCharIndex, (uint32)( pwch - pwchStart ) );
			break;
		}
		
		bool bError;
		uchar32 uVal;
		uint32 unBytes = V_UTF16ToUChar32( pwch, uVal, bError );
		if ( bError )
		{
			AssertMsg( false, "Invalid UTF16 string in CUITextLayoutWin32" );
			break;
		}

		pwch += unBytes / sizeof(wchar_t);
		nWCharLeft -= unBytes / sizeof(wchar_t);
		++unCharIndex;
	}

	return unCharIndex;
}


//-----------------------------------------------------------------------------
// Purpose: Convert char index to wchar_t index as needed by DWrite
//-----------------------------------------------------------------------------
uint32 CUITextLayoutWin32::CharIndexToLayoutWCharIndex( uint32 unCharIndex )
{
	const wchar_t *pwch = m_vecLayoutText16.Base();
	if ( !pwch || pwch[0] == 0 || unCharIndex == 0 )
		return 0;

#ifdef DBGFLAG_ASSERT
	uint32 unCharIndexStart = unCharIndex;
#endif
	uint32 unWCharIndex = 0;
	while( unCharIndex > 0 )
	{
		if ( !pwch[unWCharIndex] )
		{
			AssertMsg( false, "Layout char index %u out of range (%u text chars)",
					   unCharIndexStart, unCharIndexStart - unCharIndex );
			break;
		}

		bool bError;
		uchar32 uVal;
		unWCharIndex += V_UTF16ToUChar32( pwch + unWCharIndex, uVal, bError ) / sizeof(wchar_t);
		if ( bError )
		{
			AssertMsg( false, "Invalid UTF16 string in CUITextLayoutWin32" );
			break;
		}
		
		--unCharIndex;
	}

	return unWCharIndex;
}
