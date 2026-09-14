//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: SE port - webm/video main menu background for the stock Source 2013 client.
//          See se_background_video.h for why this exists next to the panorama route.
//
//=============================================================================//

#include "cbase.h"

#include "vgui/ISurface.h"
#include "vgui/IVGui.h"
#include "vgui_controls/Panel.h"
#include "filesystem.h"

#include "se_background_video.h"

#if defined( _WIN32 )
// Media Foundation needs at least Vista; the engine is built with an older target.
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0601

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>

// waf's MSVC link task does not forward plain system library names from a wscript `libs` list, so ask
// the linker directly (same trick as video/video_mf/mf_video_player.cpp).
#pragma comment( lib, "mfplat.lib" )
#pragma comment( lib, "mfreadwrite.lib" )
#pragma comment( lib, "mfuuid.lib" )
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef _WIN32

ConVar se_background_video( "se_background_video", "panorama/videos/anubis720.webm", FCVAR_ARCHIVE,
							"Video file (relative to the game dir) painted as the main menu background." );
ConVar se_background_video_enable( "se_background_video_enable", "1", FCVAR_ARCHIVE,
							"Enable the video main menu background." );

static bool s_bMFStarted = false;

#define SE_SAFE_RELEASE( p ) do { if ( p ) { ( p )->Release(); ( p ) = NULL; } } while ( 0 )

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CSEBackgroundMoviePanel::CSEBackgroundMoviePanel( vgui::Panel *pParent )
	: BaseClass( pParent, "SEBackgroundMovie" )
{
	m_nTextureId = -1;
	m_bTextureCreated = false;
	m_szOpenedPath[ 0 ] = 0;
	m_pReader = NULL;
	m_pPendingSample = NULL;
	m_nWidth = 0;
	m_nHeight = 0;
	m_nStride = 0;
	m_bNV12 = true;
	m_bEndOfStream = false;
	m_flFrameInterval = 1.0 / 30.0;
	m_flStartTime = -1.0;
	m_nFramesPresented = 0;

	SetPaintBackgroundEnabled( false );
	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );
	SetVisible( true );
	// Bottom of the client root panel: the stock menu paints over this.
	SetZPos( -9999 );
	SetBounds( 0, 0, ScreenWidth(), ScreenHeight() );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CSEBackgroundMoviePanel::~CSEBackgroundMoviePanel()
{
	CloseMovie();
	if ( m_nTextureId != -1 )
	{
		vgui::surface()->DestroyTextureID( m_nTextureId );
		m_nTextureId = -1;
	}
}

void CSEBackgroundMoviePanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	SetPaintBackgroundEnabled( false );
	SetBounds( 0, 0, ScreenWidth(), ScreenHeight() );
}

void CSEBackgroundMoviePanel::OnScreenSizeChanged( int nOldWide, int nOldTall )
{
	BaseClass::OnScreenSizeChanged( nOldWide, nOldTall );
	SetBounds( 0, 0, ScreenWidth(), ScreenHeight() );
}

//-----------------------------------------------------------------------------
// Purpose: (Re)opens the movie if the convar changed, closes it when disabled
//-----------------------------------------------------------------------------
void CSEBackgroundMoviePanel::EnsureMovie()
{
	const char *pchWanted = se_background_video.GetString();

	if ( !se_background_video_enable.GetBool() || !pchWanted[ 0 ] )
	{
		CloseMovie();
		return;
	}

	if ( m_pReader && V_stricmp( m_szOpenedPath, pchWanted ) == 0 )
		return;

	CloseMovie();
	OpenMovie( pchWanted );
}

//-----------------------------------------------------------------------------
// Purpose: Opens the movie through Media Foundation
//-----------------------------------------------------------------------------
void CSEBackgroundMoviePanel::OpenMovie( const char *pchRelativePath )
{
	char szFullPath[ MAX_PATH ];
	if ( !g_pFullFileSystem->RelativePathToFullPath( pchRelativePath, "GAME", szFullPath, sizeof( szFullPath ) ) )
	{
		Warning( "SE background video: '%s' does not resolve\n", pchRelativePath );
		return;
	}

	wchar_t wszPath[ MAX_PATH ];
	if ( !MultiByteToWideChar( CP_UTF8, 0, szFullPath, -1, wszPath, ARRAYSIZE( wszPath ) ) )
		return;

	if ( !s_bMFStarted )
	{
		// MFSTARTUP_LITE: no sockets, no work queue - the source reader is pulled by this thread.
		if ( FAILED( MFStartup( MF_VERSION, MFSTARTUP_LITE ) ) )
		{
			Warning( "SE background video: MFStartup failed\n" );
			return;
		}
		s_bMFStarted = true;
	}

	IMFAttributes *pAttributes = NULL;
	if ( FAILED( MFCreateAttributes( &pAttributes, 1 ) ) || !pAttributes )
		return;

	// no video processing: ask for the decoder's native output and read the planes ourselves
	pAttributes->SetUINT32( MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, FALSE );

	HRESULT hr = MFCreateSourceReaderFromURL( wszPath, pAttributes, &m_pReader );
	SE_SAFE_RELEASE( pAttributes );
	if ( FAILED( hr ) || !m_pReader )
	{
		Warning( "SE background video: cannot open '%s' (hr=0x%08X)\n", szFullPath, (unsigned)hr );
		m_pReader = NULL;
		return;
	}

	m_pReader->SetStreamSelection( MF_SOURCE_READER_ALL_STREAMS, FALSE );
	m_pReader->SetStreamSelection( MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE );

	// Ask for NV12 (VP8/VP9 decode to it on Windows 10+); fall back to the native type otherwise.
	bool bSet = false;
	IMFMediaType *pType = NULL;
	if ( SUCCEEDED( MFCreateMediaType( &pType ) ) && pType )
	{
		pType->SetGUID( MF_MT_MAJOR_TYPE, MFMediaType_Video );
		pType->SetGUID( MF_MT_SUBTYPE, MFVideoFormat_NV12 );
		bSet = SUCCEEDED( m_pReader->SetCurrentMediaType( MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, pType ) );
		SE_SAFE_RELEASE( pType );
	}

	// read back what we actually got
	m_nWidth = 0;
	m_nHeight = 0;
	m_nStride = 0;
	m_bNV12 = false;
	m_flFrameInterval = 1.0 / 30.0;

	IMFMediaType *pCurrent = NULL;
	if ( SUCCEEDED( m_pReader->GetCurrentMediaType( MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pCurrent ) ) && pCurrent )
	{
		UINT32 unWidth = 0, unHeight = 0;
		if ( SUCCEEDED( MFGetAttributeSize( pCurrent, MF_MT_FRAME_SIZE, &unWidth, &unHeight ) ) )
		{
			m_nWidth = (int)unWidth;
			m_nHeight = (int)unHeight;
		}
		m_nStride = (int)MFGetAttributeUINT32( pCurrent, MF_MT_DEFAULT_STRIDE, unWidth );
		if ( m_nStride == 0 )
			m_nStride = (int)unWidth;

		UINT32 unNum = 0, unDen = 0;
		if ( SUCCEEDED( MFGetAttributeRatio( pCurrent, MF_MT_FRAME_RATE, &unNum, &unDen ) ) && unNum && unDen )
			m_flFrameInterval = (double)unDen / (double)unNum;

		GUID guidSubType = GUID_NULL;
		pCurrent->GetGUID( MF_MT_SUBTYPE, &guidSubType );
		m_bNV12 = ( guidSubType == MFVideoFormat_NV12 );
		SE_SAFE_RELEASE( pCurrent );
	}

	if ( m_nWidth <= 0 || m_nHeight <= 0 || m_nWidth > 4096 || m_nHeight > 4096 )
	{
		Warning( "SE background video: unusable frame size %dx%d for '%s'\n", m_nWidth, m_nHeight, szFullPath );
		CloseMovie();
		return;
	}

	if ( !bSet )
	{
		// without NV12 there is no plane layout we can convert; bail out rather than paint noise
		Warning( "SE background video: '%s' has no NV12 output\n", szFullPath );
		CloseMovie();
		return;
	}

	m_rgba.SetCount( m_nWidth * m_nHeight * 4 );
	m_bEndOfStream = false;
	m_flStartTime = -1.0;
	m_nFramesPresented = 0;
	V_strncpy( m_szOpenedPath, pchRelativePath, sizeof( m_szOpenedPath ) );

	Msg( "SE background video: playing '%s' %dx%d stride=%d %.2ffps\n", pchRelativePath, m_nWidth, m_nHeight,
		 m_nStride, 1.0 / m_flFrameInterval );
}

//-----------------------------------------------------------------------------
// Purpose: Releases the movie
//-----------------------------------------------------------------------------
void CSEBackgroundMoviePanel::CloseMovie()
{
	SE_SAFE_RELEASE( m_pPendingSample );
	SE_SAFE_RELEASE( m_pReader );
	m_szOpenedPath[ 0 ] = 0;
	m_rgba.Purge();
	m_flStartTime = -1.0;
}

//-----------------------------------------------------------------------------
// Purpose: NV12 -> tightly packed RGBA (BT.601, limited range - same conversion the panorama
//          fancy-quad pixel shader uses for its YUV path, so colours match)
//-----------------------------------------------------------------------------
void CSEBackgroundMoviePanel::ConvertNV12( const unsigned char *pData, int nStride )
{
	if ( !pData || m_rgba.Count() < m_nWidth * m_nHeight * 4 )
		return;

	unsigned char *pDst = m_rgba.Base();
	const int nAbsStride = ( nStride < 0 ) ? -nStride : nStride;
	const unsigned char *pY = pData;
	const unsigned char *pUV = pData + (size_t)nAbsStride * m_nHeight;

	for ( int y = 0; y < m_nHeight; ++y )
	{
		const unsigned char *pYRow = pY + (size_t)nAbsStride * y;
		const unsigned char *pUVRow = pUV + (size_t)nAbsStride * ( y / 2 );
		unsigned char *pDstRow = pDst + (size_t)y * m_nWidth * 4;

		for ( int x = 0; x < m_nWidth; ++x )
		{
			const int nY = ( (int)pYRow[ x ] - 16 ) * 298;			// 1.164 * 256
			const int nU = (int)pUVRow[ ( x >> 1 ) * 2 + 0 ] - 128;
			const int nV = (int)pUVRow[ ( x >> 1 ) * 2 + 1 ] - 128;

			int r = ( nY + 409 * nV ) >> 8;							// 1.596 * 256
			int g = ( nY - 100 * nU - 208 * nV ) >> 8;				// 0.391 / 0.813
			int b = ( nY + 516 * nU ) >> 8;							// 2.018 * 256

			pDstRow[ x * 4 + 0 ] = (unsigned char)( r < 0 ? 0 : ( r > 255 ? 255 : r ) );
			pDstRow[ x * 4 + 1 ] = (unsigned char)( g < 0 ? 0 : ( g > 255 ? 255 : g ) );
			pDstRow[ x * 4 + 2 ] = (unsigned char)( b < 0 ? 0 : ( b > 255 ? 255 : b ) );
			pDstRow[ x * 4 + 3 ] = 255;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Pulls samples until the frame that is due now has been converted
//-----------------------------------------------------------------------------
bool CSEBackgroundMoviePanel::DecodeNextFrame()
{
	if ( !m_pReader || m_rgba.Count() == 0 )
		return false;

	const double flNow = Plat_FloatTime();
	if ( m_flStartTime < 0.0 )
		m_flStartTime = flNow;
	const uint32 unTargetMS = (uint32)( ( flNow - m_flStartTime ) * 1000.0 );

	for ( int iGuard = 0; iGuard < 8; ++iGuard )
	{
		if ( !m_pPendingSample )
		{
			DWORD dwFlags = 0;
			LONGLONG llTime = 0;
			IMFSample *pSample = NULL;
			const HRESULT hr = m_pReader->ReadSample( MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, NULL, &dwFlags, &llTime, &pSample );
			if ( FAILED( hr ) )
			{
				m_bEndOfStream = true;
				break;
			}
			if ( dwFlags & MF_SOURCE_READERF_ENDOFSTREAM )
			{
				SE_SAFE_RELEASE( pSample );
				m_bEndOfStream = true;
				break;
			}
			if ( !pSample )
				break;							// stream tick - try again next frame
			m_pPendingSample = pSample;
		}

		LONGLONG llSampleTime = 0;
		m_pPendingSample->GetSampleTime( &llSampleTime );
		if ( (uint32)( llSampleTime / 10000 ) > unTargetMS )
			break;								// not due yet

		IMFMediaBuffer *pBuffer = NULL;
		if ( SUCCEEDED( m_pPendingSample->ConvertToContiguousBuffer( &pBuffer ) ) && pBuffer )
		{
			BYTE *pLocked = NULL;
			DWORD cbLocked = 0;
			if ( SUCCEEDED( pBuffer->Lock( &pLocked, NULL, &cbLocked ) ) && pLocked )
			{
				if ( m_bNV12 )
					ConvertNV12( pLocked, m_nStride );
				pBuffer->Unlock();
				++m_nFramesPresented;
			}
			SE_SAFE_RELEASE( pBuffer );
		}
		SE_SAFE_RELEASE( m_pPendingSample );
	}

	// loop: the menu background has to keep running
	if ( m_bEndOfStream && !m_pPendingSample )
	{
		// VT_I8 holds no allocation, so there is nothing to free afterwards (PropVariantClear would
		// pull in another import library for no reason)
		PROPVARIANT var;
		PropVariantInit( &var );
		var.vt = VT_I8;
		var.hVal.QuadPart = 0;
		m_pReader->SetCurrentPosition( GUID_NULL, var );
		m_bEndOfStream = false;
		m_flStartTime = flNow;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Paints the current frame over the whole screen
//-----------------------------------------------------------------------------
void CSEBackgroundMoviePanel::Paint()
{
	if ( !se_background_video_enable.GetBool() || engine->IsInGame() )
	{
		// the stock menu (or the level) owns the screen now
		CloseMovie();
		return;
	}

	EnsureMovie();
	if ( !m_pReader )
		return;

	const int nWide = ScreenWidth();
	const int nTall = ScreenHeight();
	if ( GetWide() != nWide || GetTall() != nTall )
		SetBounds( 0, 0, nWide, nTall );

	DecodeNextFrame();
	if ( m_rgba.Count() == 0 || m_nWidth <= 0 || m_nHeight <= 0 )
		return;

	if ( !m_bTextureCreated )
	{
		// procedural: nothing on disk backs this texture, it is uploaded every frame below
		m_nTextureId = vgui::surface()->CreateNewTextureID( true );
		m_bTextureCreated = true;
	}

	// hardwareFilter=1 (smooth scaling, the video is rarely at the exact screen size),
	// forceReload=true (the contents change every frame, this is not a cache miss we can skip)
	vgui::surface()->DrawSetTextureRGBA( m_nTextureId, m_rgba.Base(), m_nWidth, m_nHeight, 1, true );
	vgui::surface()->DrawSetColor( 255, 255, 255, 255 );
	vgui::surface()->DrawSetTexture( m_nTextureId );
	vgui::surface()->DrawTexturedRect( 0, 0, nWide, nTall );
}

//-----------------------------------------------------------------------------
// External helpers (see vgui_rootpanel_cs.cpp)
//-----------------------------------------------------------------------------
static CSEBackgroundMoviePanel *s_pSEBackgroundMovie = NULL;

void SE_CreateBackgroundMoviePanel( vgui::Panel *pParent )
{
	if ( s_pSEBackgroundMovie || !pParent )
		return;

	s_pSEBackgroundMovie = new CSEBackgroundMoviePanel( pParent );
	Msg( "SE background video: panel created (se_background_video_enable=%d file='%s')\n",
		 se_background_video_enable.GetInt(), se_background_video.GetString() );
}

void SE_DestroyBackgroundMoviePanel()
{
	if ( s_pSEBackgroundMovie )
	{
		delete s_pSEBackgroundMovie;
		s_pSEBackgroundMovie = NULL;
	}
}

#else	// !_WIN32

// Nothing to do on the other platforms (this file is win32 only; the header hides the class too).

#endif	// _WIN32
