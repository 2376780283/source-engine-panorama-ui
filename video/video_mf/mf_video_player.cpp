//========= Copyright (c) Valve LLC, All rights reserved. ============//
//
// Purpose: SE port - implementation of the "libvideo" player API (common/video/ivideoplayer.h) on top
//          of Windows Media Foundation.
//
//          Valve ships this as a prebuilt library (panorama.vpc links lib\common\$PLATFORM\video.lib)
//          and the SDK has no source for it, so panorama's movie panel (the CS:GO main menu
//          background video, <Movie src=".../videos/*.webm">) had to stay disabled.  Media Foundation
//          decodes WebM/VP8/VP9 on Windows 10+ (VP9 Video Extensions + Web Media Extensions), so no
//          third party library is needed: the source reader hands us NV12 frames and we hand the
//          Y/U/V planes to panorama's already working IUIDoubleBufferedYUV420Texture path.
//
//          Frames are decoded lazily from VideoPlaybackRunFrame() (called by CUIEngine::RunFrame on
//          the main thread) instead of a decode thread - no locking, and the texture upload happens
//          on the thread that owns the D3D device.
//
//          Audio: the reader's audio stream is selected as 16 bit PCM and pumped into panorama's
//          IVideoPlayerAudioCallback from the same per frame call (the callback feeds the engine's
//          IAudioOutputStream).  Not implemented: in-memory playback, seek accuracy beyond the
//          source reader's own seeking, playback speed other than 1.0 is honoured in the frame
//          scheduler only.
//
//=============================================================================

#if defined( _WIN32 )
// Media Foundation needs at least Vista; the engine is built with an older target.
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>

// waf's MSVC link task does not forward plain system library names from a wscript `libs` list, so
// ask the linker directly (the Windows SDK lib path is already in the environment).
#pragma comment( lib, "mfplat.lib" )		// MFCreateAttributes / MFCreateMediaType / MFStartup
#pragma comment( lib, "mfreadwrite.lib" )	// MFCreateSourceReaderFromURL
#pragma comment( lib, "mfuuid.lib" )		// MF_MT_* / MFVideoFormat_* / MFMediaType_* GUIDs

#include "tier0/basetypes.h"
#include "tier0/dbg.h"
#include "tier0/platform.h"
#include "tier0/threadtools.h"
#include "tier1/utlvector.h"
#include "video/ivideoplayer.h"

#ifndef SE_MF_VIDEO_PROBE
// bring-up aid: 1 while the video path is being brought up (writes a few lines per player/load to
// D:\cstrike\se_ui_probe.txt, the same file the engine probes use)
#define SE_MF_VIDEO_PROBE 1
#endif

#if SE_MF_VIDEO_PROBE
// bring-up probe (same file the engine/handler probes write)
#include <stdio.h>
static void SE_MFProbe( const char *pFmt, ... )
{
	FILE *fp = fopen( "D:\\cstrike\\se_ui_probe.txt", "a" );
	if ( !fp )
		return;
	va_list args;
	va_start( args, pFmt );
	vfprintf( fp, pFmt, args );
	va_end( args );
	fflush( fp );
	fclose( fp );
}
#else
#define SE_MFProbe( ... ) ( (void)0 )
#endif

//-----------------------------------------------------------------------------
// Bring-up diagnostic: fault isolation for the YUV upload path.
// D:\cstrike\se_yuv_mode.txt holds one digit, read once per process:
//   0 (or absent) = normal, present every decoded frame
//   1             = present the first frame only, then decode without presenting
//   2             = never present (decode + discard), so the upload path is never entered
// This isolates "the upload is broken" from "the movie player lifetime is broken".
//-----------------------------------------------------------------------------
static int SE_YUVMode()
{
	static int s_nMode = -1;
	if ( s_nMode < 0 )
	{
		s_nMode = 0;
		FILE *fp = fopen( "D:\\cstrike\\se_yuv_mode.txt", "r" );
		if ( fp )
		{
			int nRead = 0;
			if ( fscanf( fp, "%d", &nRead ) == 1 && nRead >= 0 && nRead <= 2 )
				s_nMode = nRead;
			fclose( fp );
		}
	}
	return s_nMode;
}

#define SE_SAFE_RELEASE( p ) do { if ( p ) { ( p )->Release(); ( p ) = NULL; } } while ( 0 )

//-----------------------------------------------------------------------------
// Audio bring-up.
// The panorama audio renderer (CVideoPlayerAudioRenderer, panorama/data/panoramavideoplayer.cpp)
// marshals InitAudioOutput() to the UI thread and blocks until it has run there.  This player is
// pumped from VideoPlaybackRunFrame(), which *is* the UI thread, so calling it inline would
// deadlock - it goes to a short lived worker thread instead.
//
// The request is refcounted because the player can be destroyed while the worker is still inside
// the call: CPanoramaVideoPlayer::Stop() calls MarkShuttingDown() on the renderer first, which
// unblocks the waiting InitAudioOutput() immediately, and the worker then only touches this object.
//-----------------------------------------------------------------------------
#define SE_MF_AUDIO_LEAD_MS 250		// how much audio is kept queued in the engine's stream ahead of the video clock
#define SE_MF_AUDIO_CATCHUP_MS 150		// audio older than this behind the picture is dropped (see CollectAudioSample)

struct SE_MFAudioInitRequest
{
	CInterlockedInt				m_nRefs;
	IVideoPlayerAudioCallback	*m_pCallback;
	int							m_nSampleRate;
	int							m_nChannels;
	volatile bool				m_bDone;
	volatile bool				m_bResult;

	void AddRef() { ++m_nRefs; }
	void Release() { if ( --m_nRefs == 0 ) delete this; }
};

static uintp SE_MFAudioInitThreadProc( void *pParam )
{
	SE_MFAudioInitRequest *pReq = (SE_MFAudioInitRequest *)pParam;
	pReq->m_bResult = pReq->m_pCallback ? pReq->m_pCallback->InitAudioOutput( pReq->m_nSampleRate, pReq->m_nChannels ) : false;
	pReq->m_bDone = true;
	SE_MFProbe( "MF audio: InitAudioOutput(%d Hz, %d ch) -> %d\n", pReq->m_nSampleRate, pReq->m_nChannels, pReq->m_bResult ? 1 : 0 );
	pReq->Release();
	return 0;
}

//-----------------------------------------------------------------------------
// Frame scheduler / decoding
//-----------------------------------------------------------------------------
class CMFVideoPlayer : public IVideoPlayer
{
public:
	CMFVideoPlayer( IVideoPlayerEventCallback *pEvent, IVideoPlayerVideoCallback *pVideo, IVideoPlayerAudioCallback *pAudio )
		: m_pEventCallback( pEvent ), m_pVideoCallback( pVideo ), m_pAudioCallback( pAudio )
	{
	}

	virtual ~CMFVideoPlayer()
	{
		Stop();
		ShutdownReader();
	}

	// ---- IVideoPlayer ---------------------------------------------------------------------
	virtual bool BLoad( const char *pchURL )
	{
		if ( !pchURL || !pchURL[0] )
			return false;

		ShutdownReader();

		// MF wants a wide path; the panorama side already resolved a full path for us.
		wchar_t wszPath[MAX_PATH * 2];
		if ( MultiByteToWideChar( CP_UTF8, 0, pchURL, -1, wszPath, V_ARRAYSIZE( wszPath ) ) == 0 )
		{
			if ( MultiByteToWideChar( CP_ACP, 0, pchURL, -1, wszPath, V_ARRAYSIZE( wszPath ) ) == 0 )
			{
				m_eError = k_EVideoPlayerPlaybackErrorGeneric;
				return false;
			}
		}

		IMFAttributes *pAttributes = NULL;
		MFCreateAttributes( &pAttributes, 1 );
		if ( pAttributes )
		{
			// let MF do the format decoding (WebM/VP8/VP9 -> NV12) and stay on the file's own clock
			pAttributes->SetUINT32( MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, FALSE );
			// NOTE: the audio equivalent (MF_SOURCE_READER_ENABLE_AUDIO_PROCESSING) does not exist in
			// this SDK's headers, and the source reader inserts the audio decoder (Vorbis/Opus) plus a
			// format converter for the first audio stream anyway - see SetupAudio().
		}

		HRESULT hr = MFCreateSourceReaderFromURL( wszPath, pAttributes, &m_pReader );
		SE_SAFE_RELEASE( pAttributes );
		if ( FAILED( hr ) || !m_pReader )
		{
			SE_MFProbe( "MF BLoad FAILED '%s' hr=0x%08X\n", pchURL, (unsigned)hr );
			m_eError = k_EVideoPlayerPlaybackErrorFailedDownload;
			ShutdownReader();
			return false;
		}

		// ask for NV12 (what the VP8/VP9 decoders produce and what panorama wants)
		m_bNV12 = true;
		if ( !SetVideoType( MFVideoFormat_NV12 ) )
		{
			// fall back to RGB32 if this machine's codec cannot do NV12
			m_bNV12 = false;
			if ( !SetVideoType( MFVideoFormat_RGB32 ) )
			{
				m_eError = k_EVideoPlayerPlaybackErrorGeneric;
				ShutdownReader();
				return false;
			}
		}

		// video is required, audio is optional (SetupAudio() selects it when the file has a track and
		// panorama handed us an audio callback to feed)
		m_pReader->SetStreamSelection( MF_SOURCE_READER_ALL_STREAMS, FALSE );
		m_pReader->SetStreamSelection( MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE );
		SetupAudio();

		// duration (MF_PD_DURATION is in 100ns units; the source reader exposes it as an attribute)
		PROPVARIANT varDuration;
		PropVariantInit( &varDuration );
		if ( SUCCEEDED( m_pReader->GetPresentationAttribute( MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &varDuration ) ) )
		{
			if ( varDuration.vt == VT_UI8 )
				m_unDurationMS = (uint32)( varDuration.uhVal.QuadPart / 10000 );
			else if ( varDuration.vt == VT_I8 )
				m_unDurationMS = (uint32)( varDuration.hVal.QuadPart / 10000 );
			PropVariantClear( &varDuration );
		}

		m_bLoaded = true;
		m_bEndOfStream = false;
		m_eState = k_EVideoPlayerPlaybackStateStop;
		m_eError = k_EVideoPlayerPlaybackErrorNone;
		SE_MFProbe( "MF loaded '%s' %dx%d %.1ffps dur=%ums nv12=%d audio=%d\n", pchURL, m_nWidth, m_nHeight,
			m_flFrameInterval > 0 ? 1.0 / m_flFrameInterval : 0.0, m_unDurationMS, m_bNV12 ? 1 : 0, m_bAudioConfigured ? 1 : 0 );
		return true;
	}

	virtual bool BLoad( const byte *pubData, uint cubData )
	{
		// not implemented: panorama only ever loads from a resolved file path
		return false;
	}

	virtual void Play()
	{
		if ( !m_bLoaded || !m_pReader )
			return;

		if ( m_bEndOfStream )
		{
			// restart from the beginning
			PROPVARIANT var;
			PropVariantInit( &var );
			var.vt = VT_I8;
			var.hVal.QuadPart = 0;
			m_pReader->SetCurrentPosition( GUID_NULL, var );
			PropVariantClear( &var );
			m_bEndOfStream = false;
			SE_SAFE_RELEASE( m_pPendingSample );
		}

		m_flStartTime = Plat_FloatTime();
		m_unPlaybackBaseMS = GetCurrentPlaybackTime();
		m_eState = k_EVideoPlayerPlaybackStatePlay;
		StartAudioInit();
		m_bAudioClockNeedsBase = true;		// the sound becomes the clock once it is actually mixing (see GetCurrentPlaybackTime)
		m_bAudioPaused = false;
		if ( m_bAudioReady && m_pAudioCallback )
			m_pAudioCallback->Resume();
		if ( m_pEventCallback )
			m_pEventCallback->VideoPlayerEvent( k_EVideoPlayerEventPlaybackStateChange );
	}

	virtual void Stop()
	{
		StopAudio();

		if ( m_pReader )
		{
			PROPVARIANT var;
			PropVariantInit( &var );
			var.vt = VT_I8;
			var.hVal.QuadPart = 0;
			m_pReader->SetCurrentPosition( GUID_NULL, var );
			PropVariantClear( &var );
		}
		SE_SAFE_RELEASE( m_pPendingSample );
		m_bEndOfStream = false;
		m_unPlaybackBaseMS = 0;
		m_flStartTime = Plat_FloatTime();
		m_eState = k_EVideoPlayerPlaybackStateStop;
	}

	virtual void Pause()
	{
		if ( m_eState == k_EVideoPlayerPlaybackStatePlay )
		{
			// keep the media time we reached so Resume picks up from there
			m_unPlaybackBaseMS = GetCurrentPlaybackTime();
			m_eState = k_EVideoPlayerPlaybackStatePause;
			m_bAudioPaused = true;
			if ( m_bAudioReady && m_pAudioCallback )
				m_pAudioCallback->Pause();
			if ( m_pEventCallback )
				m_pEventCallback->VideoPlayerEvent( k_EVideoPlayerEventPlaybackStateChange );
		}
	}

	virtual void SetPlaybackSpeed( float flPlaybackSpeed )
	{
		m_flSpeed = ( flPlaybackSpeed > 0.0f ) ? flPlaybackSpeed : 1.0f;
	}

	virtual void Seek( uint unSeekMS )
	{
		if ( !m_pReader )
			return;

		PROPVARIANT var;
		PropVariantInit( &var );
		var.vt = VT_I8;
		var.hVal.QuadPart = (LONGLONG)unSeekMS * 10000;
		if ( SUCCEEDED( m_pReader->SetCurrentPosition( GUID_NULL, var ) ) )
		{
			SE_SAFE_RELEASE( m_pPendingSample );
			m_bEndOfStream = false;
			m_unPlaybackBaseMS = unSeekMS;
			m_flStartTime = Plat_FloatTime();
			// drop the audio that is still queued for the old position and start a fresh stream
			StopAudio();
			if ( m_eState == k_EVideoPlayerPlaybackStatePlay )
			{
				m_bAudioPaused = false;
				StartAudioInit();
				m_bAudioClockNeedsBase = true;
			}
		}
		PropVariantClear( &var );
	}

	virtual void SetRepeat( bool bRepeat ) { m_bRepeat = bRepeat; }
	virtual void SuggestMaxVeritcalResolution( int nHeight ) { (void)nHeight; }

	virtual EVideoPlayerPlaybackState GetPlaybackState() { return m_eState; }
	virtual bool IsStoppedForBuffering() { return false; }
	virtual float GetPlaybackSpeed() { return m_flSpeed; }

	virtual uint32 GetDuration() { return m_unDurationMS; }

	virtual uint32 GetCurrentPlaybackTime()
	{
		if ( m_eState != k_EVideoPlayerPlaybackStatePlay )
			return m_unPlaybackBaseMS;

		const uint32 unWallMS = GetWallClockTime();

		// While the sound is running it is the clock.  The engine's stream reports exactly how much of
		// the soundtrack it has mixed out, and this player drops the audio that was already behind the
		// picture when the stream came up, so the picture can simply be presented where the sound is
		// (a wall clock drifts away from it: the stream is created a few frames after Play(), a seek
		// restarts it, and the player keeps a lead queued in front of what is being heard).
		if ( m_bAudioReady && m_pAudioCallback )
		{
			const uint32 unMixedMS = m_pAudioCallback->GetMixedMilliseconds();
			if ( m_bAudioClockNeedsBase && unMixedMS > 0 )
			{
				// hand over without a jump: the sound continues from where the picture is right now
				m_unAudioClockBaseMS = unWallMS - unMixedMS;
				m_bAudioClockNeedsBase = false;
			}

			if ( !m_bAudioClockNeedsBase )
			{
				const uint32 unAudioMS = m_unAudioClockBaseMS + unMixedMS;
				if ( unAudioMS > m_unLastAudioClockMS )
				{
					m_unLastAudioClockMS = unAudioMS;
					m_flAudioClockProgress = Plat_FloatTime();
				}
				else if ( m_unLastAudioClockMS > 0 && Plat_FloatTime() - m_flAudioClockProgress > 0.5 )
				{
					// the stream stopped advancing (no device, starved queue) - do not freeze the movie
					m_bAudioClockGaveUp = true;
				}

				if ( !m_bAudioClockGaveUp && m_unLastAudioClockMS > 0 )
					return ( m_unDurationMS && m_unLastAudioClockMS > m_unDurationMS ) ? m_unDurationMS : m_unLastAudioClockMS;
			}
		}

		return unWallMS;
	}

	// The wall clock the picture used before the sound worked; also the reference the audio catch-up in
	// CollectAudioSample() compares against.
	uint32 GetWallClockTime() const
	{
		if ( m_eState == k_EVideoPlayerPlaybackStatePlay )
		{
			double flElapsed = ( Plat_FloatTime() - m_flStartTime ) * (double)m_flSpeed;
			uint32 unTime = m_unPlaybackBaseMS + (uint32)( flElapsed * 1000.0 );
			return ( m_unDurationMS && unTime > m_unDurationMS ) ? m_unDurationMS : unTime;
		}
		return m_unPlaybackBaseMS;
	}

	virtual EVideoPlayerPlaybackError GetPlaybackError() { return m_eError; }

	virtual void GetVideoResolution( int *pnWidth, int *pnHeight )
	{
		if ( pnWidth ) *pnWidth = m_nWidth;
		if ( pnHeight ) *pnHeight = m_nHeight;
	}

	virtual int GetVideoDownloadRate() { return 0; }
	virtual int GetVideoRepresentationCount() { return 1; }
	virtual bool BGetVideoRepresentationInfo( int iRep, int *pnWidth, int *pnHeight )
	{
		if ( iRep != 0 ) return false;
		GetVideoResolution( pnWidth, pnHeight );
		return true;
	}
	virtual void ForceVideoRepresentation( int iRep ) { (void)iRep; }
	virtual int GetCurrentVideoRepresentation() { return 0; }
	virtual void GetVideoSegmentInfo( int *pnCurrent, int *pnTotal )
	{
		if ( pnCurrent ) *pnCurrent = 1;
		if ( pnTotal ) *pnTotal = 1;
	}

	virtual bool BHasAudioTrack()
	{
		// true once BLoad() found a decodable audio track (see SetupAudio()); panorama uses this to
		// decide whether to raise its audio-start event and to drive the movie volume slider
		return m_bAudioConfigured;
	}

	// ---- driven by VideoPlaybackRunFrame() -------------------------------------------------
	void Update()
	{
		if ( !m_pReader || !m_bLoaded )
			return;

		if ( m_eState != k_EVideoPlayerPlaybackStatePlay )
			return;

		const uint32 unTargetMS = GetCurrentPlaybackTime();

	// SE port bring-up probe: the video path runs *on the UI thread* (VideoPlaybackRunFrame), so its
	// per frame cost is part of the menu's frame time.  ReadNextSample() is a synchronous (blocking)
	// Media Foundation call that decodes a frame, and PresentSample() does the planar conversion plus
	// the texture upload - timing the two apart decides where the time goes (a decode thread is a
	// very different fix from a cheaper conversion).  One "MFFRAME" line per second.
	static double s_flMFWindowStart = 0.0;
	static int s_nMFDecodes = 0, s_nMFPresents = 0, s_nMFProbe = 0;
	static double s_flMFDecodeSumMs = 0.0, s_flMFDecodeMaxMs = 0.0;
	static double s_flMFPresentSumMs = 0.0, s_flMFPresentMaxMs = 0.0;

	for ( int iGuard = 0; iGuard < 8; ++iGuard )
	{
		if ( !m_pPendingSample )
		{
			double const flDecodeStart = Plat_FloatTime();
			bool const bRead = ReadNextSample();
			double const flDecodeMs = ( Plat_FloatTime() - flDecodeStart ) * 1000.0;
			++s_nMFDecodes;
			s_flMFDecodeSumMs += flDecodeMs;
			if ( flDecodeMs > s_flMFDecodeMaxMs ) { s_flMFDecodeMaxMs = flDecodeMs; }
			if ( !bRead )
				break;
		}

		if ( !m_pPendingSample )
			break;

		LONGLONG llSampleTime = 0;
		m_pPendingSample->GetSampleTime( &llSampleTime );
		const uint32 unSampleMS = (uint32)( llSampleTime / 10000 );
		if ( unSampleMS > unTargetMS )
			break;						// not due yet - wait for the next call

		// Late frames are *dropped* instead of presented.  The playback clock is the wall clock, and a
		// window that was in the background (engine throttles RunFrame when occluded) comes back
		// seconds behind - presenting every one of those frames turned the return into a burst of
		// 150+ decodes/copies (measured: ~0.7-1.5 s of stutter right after refocusing).  The audio
		// side already drops late samples the same way (SE_MF_AUDIO_CATCHUP_MS).
		if ( unTargetMS - unSampleMS > 250 )
		{
			SE_SAFE_RELEASE( m_pPendingSample );
			continue;
		}

		double const flPresentStart = Plat_FloatTime();
		PresentSample( m_pPendingSample );
		double const flPresentMs = ( Plat_FloatTime() - flPresentStart ) * 1000.0;
		++s_nMFPresents;
		s_flMFPresentSumMs += flPresentMs;
		if ( flPresentMs > s_flMFPresentMaxMs ) { s_flMFPresentMaxMs = flPresentMs; }
		SE_SAFE_RELEASE( m_pPendingSample );
	}

	{
		double const flNow = Plat_FloatTime();
		if ( s_flMFWindowStart <= 0.0 ) { s_flMFWindowStart = flNow; }
		if ( flNow - s_flMFWindowStart >= 1.0 )
		{
			if ( s_nMFProbe < 900 )
			{
				++s_nMFProbe;
				SE_MFProbe( "MFFRAME #%d decodes=%d dec_avg=%.2fms dec_max=%.2fms presents=%d pres_avg=%.2fms pres_max=%.2fms\n",
					s_nMFProbe, s_nMFDecodes,
					s_nMFDecodes ? ( s_flMFDecodeSumMs / s_nMFDecodes ) : 0.0, s_flMFDecodeMaxMs,
					s_nMFPresents,
					s_nMFPresents ? ( s_flMFPresentSumMs / s_nMFPresents ) : 0.0, s_flMFPresentMaxMs );
			}
			s_flMFWindowStart = flNow;
			s_nMFDecodes = 0;
			s_nMFPresents = 0;
			s_flMFDecodeSumMs = 0.0;
			s_flMFDecodeMaxMs = 0.0;
			s_flMFPresentSumMs = 0.0;
			s_flMFPresentMaxMs = 0.0;
		}
	}

	PumpAudio();

		// TEMPORARY A/V sync probe: wall clock vs the audio reader's position and the engine queue.
		if ( m_bAudioReady && m_pAudioCallback && Plat_FloatTime() - m_flSyncProbeTime >= 1.0 )
		{
			m_flSyncProbeTime = Plat_FloatTime();
			const uint32 unQueuedBytes = m_pAudioCallback->GetRemainingCommittedAudio();
			const double flBytesPerSecond = (double)m_nAudioSampleRate * (double)m_nAudioBytesPerFrame;
			const double flQueuedMS = ( flBytesPerSecond > 0.0 ) ? ( (double)unQueuedBytes * 1000.0 / flBytesPerSecond ) : 0.0;
			const double flCommittedMS = ( m_nAudioSampleRate > 0 )
				? ( (double)m_unAudioCommittedFrames * 1000.0 / (double)m_nAudioSampleRate ) : 0.0;
			SE_MFProbe( "MF sync wall=%u lastAudioSample=%u committed=%.0fms queued=%.0fms latency=%ums\n",
				GetCurrentPlaybackTime(), m_unAudioLastSampleMS, flCommittedMS, flQueuedMS,
				m_pAudioCallback->GetPlaybackLatency() );
		}

		if ( m_bEndOfStream && !m_pPendingSample )
		{
			if ( m_bRepeat )
			{
				Seek( 0 );
				m_flStartTime = Plat_FloatTime();
			}
			else
			{
				m_eState = k_EVideoPlayerPlaybackStateStop;
				if ( m_pEventCallback )
					m_pEventCallback->VideoPlayerEvent( k_EVideoPlayerEventEnd );
			}
		}
	}

private:
	bool SetVideoType( const GUID &subType )
	{
		IMFMediaType *pType = NULL;
		if ( FAILED( MFCreateMediaType( &pType ) ) )
			return false;
		pType->SetGUID( MF_MT_MAJOR_TYPE, MFMediaType_Video );
		pType->SetGUID( MF_MT_SUBTYPE, subType );
		const HRESULT hr = m_pReader->SetCurrentMediaType( MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, pType );
		SE_SAFE_RELEASE( pType );
		if ( FAILED( hr ) )
			return false;

		// read back what we actually got
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
			else
				m_flFrameInterval = 1.0 / 30.0;

			GUID guidSubType = GUID_NULL;
			pCurrent->GetGUID( MF_MT_SUBTYPE, &guidSubType );
			m_bNV12 = ( guidSubType == MFVideoFormat_NV12 );
			SE_SAFE_RELEASE( pCurrent );
		}
		return true;
	}

	bool ReadNextSample()
	{
		DWORD dwStreamFlags = 0;
		LONGLONG llTime = 0;
		IMFSample *pSample = NULL;
		const HRESULT hr = m_pReader->ReadSample( MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, NULL, &dwStreamFlags, &llTime, &pSample );
		if ( FAILED( hr ) )
		{
			SE_MFProbe( "MF ReadSample failed hr=0x%08X\n", (unsigned)hr );
			m_eError = k_EVideoPlayerPlaybackErrorGeneric;
			m_bEndOfStream = true;
			return false;
		}

		if ( dwStreamFlags & MF_SOURCE_READERF_ENDOFSTREAM )
		{
			SE_SAFE_RELEASE( pSample );
			m_bEndOfStream = true;
			return false;
		}

		if ( dwStreamFlags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED )
		{
			// resolution/format changed mid stream - re-read it
			IMFMediaType *pCurrent = NULL;
			if ( SUCCEEDED( m_pReader->GetCurrentMediaType( MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pCurrent ) ) && pCurrent )
			{
				UINT32 unWidth = 0, unHeight = 0;
				MFGetAttributeSize( pCurrent, MF_MT_FRAME_SIZE, &unWidth, &unHeight );
				if ( unWidth && unHeight )
				{
					m_nWidth = (int)unWidth;
					m_nHeight = (int)unHeight;
					m_nStride = (int)MFGetAttributeUINT32( pCurrent, MF_MT_DEFAULT_STRIDE, unWidth );
				}
				SE_SAFE_RELEASE( pCurrent );
			}
		}

		if ( !pSample )
			return true;					// stream tick / gap - keep going next call

		m_pPendingSample = pSample;
		return true;
	}

	void PresentSample( IMFSample *pSample )
	{
		if ( !pSample || !m_pVideoCallback || m_nWidth <= 0 || m_nHeight <= 0 )
			return;

		IMFMediaBuffer *pBuffer = NULL;
		if ( FAILED( pSample->ConvertToContiguousBuffer( &pBuffer ) ) || !pBuffer )
			return;

		BYTE *pData = NULL;
		DWORD cbData = 0;
		if ( FAILED( pBuffer->Lock( &pData, NULL, NULL ) ) || !pData )
		{
			SE_SAFE_RELEASE( pBuffer );
			return;
		}

		const int nYUVMode = SE_YUVMode();
		if ( nYUVMode == 2 || ( nYUVMode == 1 && m_nFramesPresented >= 1 ) )
		{
			++m_nFramesPresented;
			if ( m_nFramesPresented <= 3 )
				SE_MFProbe( "MF skip present #%d (mode=%d)\n", m_nFramesPresented, nYUVMode );
		}
		else if ( m_bNV12 )
		{
			// NV12 = Y plane + interleaved U/V; panorama wants three separate planes (I420 style).
			//
			// SE port fix (2026-09-14): copy out of the locked Media Foundation buffer into our own
			// memory instead of handing panorama a pointer into it.  The texture upload may be
			// deferred, and the frame buffer must not be referenced after Unlock() (handing it out
			// directly was the prime suspect for the heap corruption that followed the first
			// presented frames).  Rows are copied with the source stride and delivered tightly
			// packed, so a padded MF stride cannot make the uploader read past the plane either.
			int nSrcStride = se_abs_int( m_nStride );
			if ( nSrcStride < m_nWidth )
				nSrcStride = m_nWidth;

			const int nChromaWidth = m_nWidth / 2;
			const int nChromaHeight = m_nHeight / 2;

			m_yPlane.SetCount( m_nWidth * m_nHeight );
			m_uvPlane.SetCount( nChromaWidth * nChromaHeight );
			m_vPlane.SetCount( nChromaWidth * nChromaHeight );

			uint8 *pY = m_yPlane.Base();
			const uint8 *pSrcY = pData;
			for ( int y = 0; y < m_nHeight; ++y )
			{
				memcpy( pY + (size_t)y * m_nWidth, pSrcY + (size_t)y * nSrcStride, (size_t)m_nWidth );
			}

			uint8 *pU = m_uvPlane.Base();
			uint8 *pV = m_vPlane.Base();
			const uint8 *pUV = pData + (size_t)nSrcStride * m_nHeight;
			for ( int y = 0; y < nChromaHeight; ++y )
			{
				const uint8 *pSrc = pUV + (size_t)nSrcStride * y;
				uint8 *pDstU = pU + (size_t)y * nChromaWidth;
				uint8 *pDstV = pV + (size_t)y * nChromaWidth;
				for ( int x = 0; x < nChromaWidth; ++x )
				{
					pDstU[x] = pSrc[x * 2 + 0];
					pDstV[x] = pSrc[x * 2 + 1];
				}
			}

			m_pVideoCallback->BPresentYUV420Texture( (uint)m_nWidth, (uint)m_nHeight, (void *)pY,
				(void *)pU, (void *)pV, (uint)m_nWidth, (uint)nChromaWidth, (uint)nChromaWidth );
			++m_nFramesPresented;
			if ( m_nFramesPresented <= 3 )
			{
				SE_MFProbe( "MF present #%d %dx%d srcStride=%d (buffer lock ok)\n",
					m_nFramesPresented, m_nWidth, m_nHeight, nSrcStride );
			}
		}
		else
		{
			// RGB32 fallback: convert to I420 (BT.601 limited range, matching the WebM/VP8 default)
			const int nChromaWidth = m_nWidth / 2;
			const int nChromaHeight = m_nHeight / 2;
			m_yPlane.SetCount( m_nWidth * m_nHeight );
			m_uvPlane.SetCount( nChromaWidth * nChromaHeight );
			m_vPlane.SetCount( nChromaWidth * nChromaHeight );

			uint8 *pY = m_yPlane.Base();
			uint8 *pU = m_uvPlane.Base();
			uint8 *pV = m_vPlane.Base();

			for ( int y = 0; y < m_nHeight; ++y )
			{
				const uint8 *pRow = pData + (size_t)se_abs_int( m_nStride ) * y;
				for ( int x = 0; x < m_nWidth; ++x )
				{
					const uint8 b = pRow[x * 4 + 0];
					const uint8 g = pRow[x * 4 + 1];
					const uint8 r = pRow[x * 4 + 2];
					const int nY = ( ( 66 * r + 129 * g + 25 * b + 128 ) >> 8 ) + 16;
					pY[(size_t)y * m_nWidth + x] = (uint8)clamp( nY, 0, 255 );
					if ( ( ( x & 1 ) == 0 ) && ( ( y & 1 ) == 0 ) )
					{
						const int nU = ( ( -38 * r - 74 * g + 112 * b + 128 ) >> 8 ) + 128;
						const int nV = ( ( 112 * r - 94 * g - 18 * b + 128 ) >> 8 ) + 128;
						pU[(size_t)( y / 2 ) * nChromaWidth + ( x / 2 )] = (uint8)clamp( nU, 0, 255 );
						pV[(size_t)( y / 2 ) * nChromaWidth + ( x / 2 )] = (uint8)clamp( nV, 0, 255 );
					}
				}
			}

			m_pVideoCallback->BPresentYUV420Texture( (uint)m_nWidth, (uint)m_nHeight, (void *)pY,
				(void *)pU, (void *)pV, (uint)m_nWidth, (uint)nChromaWidth, (uint)nChromaWidth );
			++m_nFramesPresented;
		}

		pBuffer->Unlock();
		SE_SAFE_RELEASE( pBuffer );
	}

	// ---- audio -------------------------------------------------------------------------------
	// SetupAudio(): does the file have an audio track, and can the reader hand it to us as 16 bit
	// interleaved PCM?  Everything else (decoding Vorbis/Opus, resampling, channel mapping) is done
	// by the Media Foundation audio processing objects the source reader inserts.
	void SetupAudio()
	{
		m_bAudioConfigured = false;
		if ( !m_pAudioCallback || !m_pReader )
			return;

		// a native type is how the reader reports "there is a stream here" (nullptr = none)
		IMFMediaType *pNative = NULL;
		const HRESULT hrNative = m_pReader->GetNativeMediaType( MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, &pNative );
		const bool bHasTrack = SUCCEEDED( hrNative ) && pNative != NULL;
		SE_SAFE_RELEASE( pNative );
		if ( !bHasTrack )
		{
			SE_MFProbe( "MF audio: no audio track (GetNativeMediaType hr=0x%08X)\n", (unsigned)hrNative );
			return;
		}

		m_pReader->SetStreamSelection( MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE );

		IMFMediaType *pPCM = NULL;
		if ( FAILED( MFCreateMediaType( &pPCM ) ) )
			return;
		pPCM->SetGUID( MF_MT_MAJOR_TYPE, MFMediaType_Audio );
		pPCM->SetGUID( MF_MT_SUBTYPE, MFAudioFormat_PCM );
		pPCM->SetUINT32( MF_MT_AUDIO_BITS_PER_SAMPLE, 16 );
		const HRESULT hrType = m_pReader->SetCurrentMediaType( MF_SOURCE_READER_FIRST_AUDIO_STREAM, NULL, pPCM );
		SE_SAFE_RELEASE( pPCM );
		if ( FAILED( hrType ) )
		{
			// no decoder for this codec on this machine - the video still plays, just silently
			SE_MFProbe( "MF audio: SetCurrentMediaType(PCM) failed hr=0x%08X\n", (unsigned)hrType );
			m_pReader->SetStreamSelection( MF_SOURCE_READER_FIRST_AUDIO_STREAM, FALSE );
			return;
		}

		// read back what the reader actually negotiated
		IMFMediaType *pCurrent = NULL;
		if ( FAILED( m_pReader->GetCurrentMediaType( MF_SOURCE_READER_FIRST_AUDIO_STREAM, &pCurrent ) ) || !pCurrent )
		{
			m_pReader->SetStreamSelection( MF_SOURCE_READER_FIRST_AUDIO_STREAM, FALSE );
			return;
		}

		m_nAudioChannels = (int)MFGetAttributeUINT32( pCurrent, MF_MT_AUDIO_NUM_CHANNELS, 0 );
		m_nAudioSampleRate = (int)MFGetAttributeUINT32( pCurrent, MF_MT_AUDIO_SAMPLES_PER_SECOND, 0 );
		const UINT32 unBits = MFGetAttributeUINT32( pCurrent, MF_MT_AUDIO_BITS_PER_SAMPLE, 16 );
		SE_SAFE_RELEASE( pCurrent );

		if ( m_nAudioChannels <= 0 || m_nAudioSampleRate <= 0 || unBits != 16 )
		{
			SE_MFProbe( "MF audio: unusable PCM type %d Hz %d ch %d bit\n",
				m_nAudioSampleRate, m_nAudioChannels, (int)unBits );
			m_pReader->SetStreamSelection( MF_SOURCE_READER_FIRST_AUDIO_STREAM, FALSE );
			return;
		}

		m_nAudioBytesPerFrame = m_nAudioChannels * (int)sizeof( int16 );
		m_bAudioConfigured = true;
		SE_MFProbe( "MF audio: track selected, %d Hz %d ch 16 bit PCM\n", m_nAudioSampleRate, m_nAudioChannels );
	}

	void StartAudioInit()
	{
		m_bAudioAbandoned = false;
		if ( !m_bAudioConfigured || m_bAudioInitStarted || !m_pAudioCallback )
			return;

		SE_MFAudioInitRequest *pReq = new SE_MFAudioInitRequest;
		pReq->m_nRefs = 2;					// one reference for us, one for the thread
		pReq->m_pCallback = m_pAudioCallback;
		pReq->m_nSampleRate = m_nAudioSampleRate;
		pReq->m_nChannels = m_nAudioChannels;
		pReq->m_bDone = false;
		pReq->m_bResult = false;

		m_pAudioInitRequest = pReq;
		m_bAudioInitStarted = true;
		// detached: the request is refcounted, so nothing has to wait for the thread to exit.
		// (the 4 argument overload is the only unambiguous one - the 3 argument ones differ by
		// whether the third parameter is a ThreadId_t* or a stack size)
		ReleaseThreadHandle( CreateSimpleThread( SE_MFAudioInitThreadProc, pReq, (ThreadId_t *)NULL, 0 ) );
	}

	void CollectAudioInitResult()
	{
		if ( !m_pAudioInitRequest || !m_pAudioInitRequest->m_bDone )
			return;

		m_bAudioReady = m_pAudioInitRequest->m_bResult;
		m_pAudioInitRequest->Release();
		m_pAudioInitRequest = NULL;

		if ( m_bAudioReady )
			m_bAudioClockNeedsBase = true;		// the sound takes over the clock when it starts mixing
	}

	void StopAudio()
	{
		m_bAudioAbandoned = true;
		m_bAudioInitStarted = false;
		m_bAudioReady = false;
		m_bAudioEndOfStream = false;
		m_bAudioPaused = false;
		m_vecAudioPending.RemoveAll();
		// the clock goes back to the wall clock until a new stream is up
		m_bAudioClockNeedsBase = true;
		m_unAudioClockBaseMS = 0;
		m_unLastAudioClockMS = 0;
		m_bAudioClockGaveUp = false;

		if ( m_pAudioInitRequest )			// an in-flight init finishes on its own (refcounted)
		{
			m_pAudioInitRequest->Release();
			m_pAudioInitRequest = NULL;
		}

		if ( m_pAudioCallback )
			m_pAudioCallback->FreeAudioOutput();	// never blocks: the renderer drops the stream on the UI thread
	}

	// PumpAudio(): called per frame on the UI thread (from Update(), i.e. VideoPlaybackRunFrame).
	// Keeps the engine's stream fed, but only up to SE_MF_AUDIO_LEAD_MS ahead of the video clock, so
	// a fast machine does not decode the whole movie into the output queue.
	void PumpAudio()
	{
		CollectAudioInitResult();
		if ( !m_bAudioReady || !m_pAudioCallback || m_bAudioPaused || m_bAudioAbandoned )
			return;

		const uint32 unLeadBytes = (uint32)( ( (uint64)m_nAudioSampleRate * (uint64)m_nAudioBytesPerFrame * SE_MF_AUDIO_LEAD_MS ) / 1000 );
		// the staging buffer counts towards the lead too - otherwise it keeps growing while the engine's
		// queue is already full, and the player's stall guard throws seconds of audio away
		const uint32 unStagedBytes = (uint32)( m_vecAudioPending.Count() * (int)sizeof( int16 ) );
		if ( m_pAudioCallback->GetRemainingCommittedAudio() + unStagedBytes >= unLeadBytes )
			return;

		for ( int iGuard = 0; iGuard < 16; ++iGuard )		// bounded work per frame
		{
			if ( !m_pAudioCallback->IsReadyForAudioData() )
				break;

			IMFSample *pSample = NULL;
			if ( !ReadAudioSample( &pSample ) || !pSample )
				break;

			CollectAudioSample( pSample );
			SE_SAFE_RELEASE( pSample );
		}
	}

	bool ReadAudioSample( IMFSample **ppSample )
	{
		*ppSample = NULL;
		if ( !m_pReader )
			return false;

		DWORD dwStreamFlags = 0;
		LONGLONG llTime = 0;
		const HRESULT hr = m_pReader->ReadSample( MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, NULL, &dwStreamFlags, &llTime, ppSample );
		if ( FAILED( hr ) )
		{
			SE_MFProbe( "MF audio: ReadSample failed hr=0x%08X\n", (unsigned)hr );
			m_bAudioEndOfStream = true;
			return false;
		}

		if ( dwStreamFlags & MF_SOURCE_READERF_ENDOFSTREAM )
		{
			SE_SAFE_RELEASE( *ppSample );
			m_bAudioEndOfStream = true;
			SE_MFProbe( "MF audio: end of stream\n" );
			return false;
		}

		if ( dwStreamFlags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED )
			SE_MFProbe( "MF audio: media type changed mid stream\n" );

		return ( *ppSample != NULL );
	}

	// CollectAudioSample(): copy the decoded bytes into our own staging buffer and hand as much as
	// the callback accepts to it.  The callback owns a fixed 16 KB buffer, so a short write leaves a
	// remainder here for the next frame instead of dropping audio.
	void CollectAudioSample( IMFSample *pSample )
	{
		IMFMediaBuffer *pBuffer = NULL;
		if ( FAILED( pSample->ConvertToContiguousBuffer( &pBuffer ) ) || !pBuffer )
			return;

		BYTE *pData = NULL;
		DWORD cbData = 0;
		if ( FAILED( pBuffer->Lock( &pData, NULL, &cbData ) ) || !pData || cbData == 0 )
		{
			SE_SAFE_RELEASE( pBuffer );
			return;
		}

		const int nSamples = (int)( cbData / sizeof( int16 ) );

		// Where in the movie this audio belongs.  The reader hands us the audio for the current file
		// position, while the picture may already be further along (the engine stream is created a few
		// frames after Play(), or after a seek/pause) - playing that audio would put the whole
		// soundtrack behind the picture, so it is dropped until the sound catches up with the picture.
		// From there on the picture follows the sound (see GetCurrentPlaybackTime()).
		LONGLONG llSampleTime = 0;
		pSample->GetSampleTime( &llSampleTime );
		const uint32 unSampleMS = (uint32)( llSampleTime / 10000 );
		m_unAudioLastSampleMS = unSampleMS;		// TEMPORARY sync probe value
		if ( unSampleMS + SE_MF_AUDIO_CATCHUP_MS < GetWallClockTime() )
		{
			++m_nAudioDroppedSamples;
			if ( m_nAudioDroppedSamples <= 3 || ( m_nAudioDroppedSamples % 100 ) == 0 )
				SE_MFProbe( "MF audio: dropping stale audio (%ums) while catching up, #%d\n", unSampleMS, m_nAudioDroppedSamples );
			pBuffer->Unlock();
			SE_SAFE_RELEASE( pBuffer );
			return;
		}

		const int nFirst = m_vecAudioPending.Count();
		m_vecAudioPending.SetCount( nFirst + nSamples );
		memcpy( m_vecAudioPending.Base() + nFirst, pData, (size_t)nSamples * sizeof( int16 ) );
		++m_nAudioSamplesQueued;

		pBuffer->Unlock();
		SE_SAFE_RELEASE( pBuffer );

		FlushAudioPending();
	}

	void FlushAudioPending()
	{
		if ( !m_pAudioCallback )
			return;

		const int nChannels = ( m_nAudioChannels > 0 ) ? m_nAudioChannels : 1;
		int nOffset = 0;									// in samples (int16), interleaved
		const int nTotal = m_vecAudioPending.Count();

		while ( nOffset + nChannels <= nTotal )
		{
			if ( !m_pAudioCallback->IsReadyForAudioData() )
				break;

			uint32 unChunkBytes = m_pAudioCallback->GetAudioBufferSize();
			if ( unChunkBytes < sizeof( int16 ) )
				break;

			int nChunkSamples = (int)( unChunkBytes / sizeof( int16 ) );
			nChunkSamples -= nChunkSamples % nChannels;		// whole frames only
			if ( nChunkSamples <= 0 )
				break;
			nChunkSamples = MIN( nChunkSamples, nTotal - nOffset );

			void *pDst = m_pAudioCallback->GetAudioBuffer();
			if ( !pDst )
				break;

			memcpy( pDst, m_vecAudioPending.Base() + nOffset, (size_t)nChunkSamples * sizeof( int16 ) );
			// the renderer's CommitAudioBuffer takes bytes and divides by (channels * sizeof(int16))
			m_pAudioCallback->CommitAudioBuffer( (uint32)nChunkSamples * sizeof( int16 ) );
			m_unAudioCommittedFrames += (uint64)( nChunkSamples / nChannels );
			nOffset += nChunkSamples;
		}

		if ( nOffset > 0 )
			m_vecAudioPending.RemoveMultiple( 0, nOffset );

		if ( m_nAudioSamplesQueued <= 3 && nOffset > 0 )
			SE_MFProbe( "MF audio: queued %d samples, %d bytes in flight\n", nOffset, (int)nOffset * (int)sizeof( int16 ) );

		// if the engine never drains the queue, drop it rather than growing without bound
		const int nMaxPending = ( m_nAudioSampleRate > 0 ? m_nAudioSampleRate : 48000 ) * nChannels * 2;
		if ( m_vecAudioPending.Count() > nMaxPending )
		{
			SE_MFProbe( "MF audio: stream stalled, dropping %d queued samples\n", m_vecAudioPending.Count() );
			m_vecAudioPending.RemoveAll();
		}
	}

	void ShutdownReader()
	{
		StopAudio();
		SE_SAFE_RELEASE( m_pPendingSample );
		SE_SAFE_RELEASE( m_pReader );
		m_bLoaded = false;
		m_bEndOfStream = false;
		m_bHasAudio = false;
		m_bAudioConfigured = false;
		m_nAudioChannels = 0;
		m_nAudioSampleRate = 0;
		m_nAudioBytesPerFrame = 0;
		m_nWidth = 0;
		m_nHeight = 0;
		m_nStride = 0;
		m_unDurationMS = 0;
		m_unPlaybackBaseMS = 0;
		m_eState = k_EVideoPlayerPlaybackStateStop;
	}

	static int se_abs_int( int v ) { return v < 0 ? -v : v; }

	IVideoPlayerEventCallback *m_pEventCallback;
	IVideoPlayerVideoCallback *m_pVideoCallback;
	IVideoPlayerAudioCallback *m_pAudioCallback;

	// SE port fix (2026-09-14): the constructor only initialised the three callbacks, so every other
	// member started as garbage.  panorama destroys a freshly created player whenever a background
	// image layer swaps its video player (CBackgroundImageLayer::ReloadImage -> CSmartPtr::operator=
	// -> ~CPanoramaVideoPlayer -> CMFVideoPlayer::Stop), and Stop() then touched the garbage
	// m_pReader - that was the 0xC0000005 at mf_video_player.cpp:206.  In-class initialisers keep
	// every path (Stop/BLoad/Play/the destructor) safe on a player that was never loaded.
	IMFSourceReader *m_pReader = NULL;
	IMFSample *m_pPendingSample = NULL;

	bool m_bLoaded = false;
	bool m_bPlaying = false;
	bool m_bRepeat = true;			// the menu background movies loop
	bool m_bEndOfStream = false;
	bool m_bHasAudio = false;
	bool m_bNV12 = true;

	// ---- audio -------------------------------------------------------------------------------
	// m_bAudioConfigured: BLoad() found an audio track and the reader agreed to 16 bit PCM.
	// m_bAudioInitStarted/Ready: the (detached) worker that calls InitAudioOutput() on the renderer.
	bool m_bAudioConfigured = false;
	bool m_bAudioInitStarted = false;
	bool m_bAudioReady = false;
	bool m_bAudioPaused = false;
	bool m_bAudioAbandoned = false;
	bool m_bAudioEndOfStream = false;
	int m_nAudioChannels = 0;
	int m_nAudioSampleRate = 0;
	int m_nAudioBytesPerFrame = 0;
	int m_nAudioSamplesQueued = 0;
	SE_MFAudioInitRequest *m_pAudioInitRequest = NULL;
	CUtlVector<int16> m_vecAudioPending;		// decoded samples the callback had no room for yet

	// TEMPORARY A/V sync probe (2026-09-16): the video clock is a wall clock while the sound is fed
	// as fast as the engine's stream accepts it, so the two can drift apart.  These counters let the
	// probe below print where the audio timeline actually is.
	uint64 m_unAudioCommittedFrames = 0;		// PCM frames handed to the engine's stream
	uint32 m_unAudioLastSampleMS = 0;			// media time of the last audio sample read
	double m_flSyncProbeTime = 0.0;
	int m_nAudioDroppedSamples = 0;			// audio dropped to catch up with the picture

	// The playback clock while the sound is running: m_unAudioClockBaseMS is the movie position the
	// stream took over at, and the engine stream's mixed-milliseconds count advances it.
	uint32 m_unAudioClockBaseMS = 0;
	uint32 m_unLastAudioClockMS = 0;
	double m_flAudioClockProgress = 0.0;
	bool m_bAudioClockNeedsBase = true;
	bool m_bAudioClockGaveUp = false;

	float m_flSpeed = 1.0f;
	double m_flStartTime = 0.0;
	double m_flFrameInterval = 0.0;
	uint32 m_unPlaybackBaseMS = 0;
	uint32 m_unDurationMS = 0;
	int m_nWidth = 0;
	int m_nHeight = 0;
	int m_nStride = 0;
	int m_nFramesPresented = 0;

	EVideoPlayerPlaybackState m_eState = k_EVideoPlayerPlaybackStateStop;
	EVideoPlayerPlaybackError m_eError = k_EVideoPlayerPlaybackErrorNone;

	CUtlVector<uint8> m_yPlane;
	CUtlVector<uint8> m_uvPlane;
	CUtlVector<uint8> m_vPlane;
};

//-----------------------------------------------------------------------------
// Live players (CreateVideoPlayer/DestroyVideoPlayer + per frame pump)
//-----------------------------------------------------------------------------
static CUtlVector<CMFVideoPlayer *> g_MFPlayers;
static bool g_bMFStarted = false;
static bool g_bCOMInitialized = false;

void VideoPlaybackInitialize()
{
	if ( g_bMFStarted )
		return;

	const HRESULT hrCom = CoInitializeEx( NULL, COINIT_MULTITHREADED );
	g_bCOMInitialized = SUCCEEDED( hrCom );

	const HRESULT hr = MFStartup( MF_VERSION, MFSTARTUP_LITE );
	g_bMFStarted = SUCCEEDED( hr );
	SE_MFProbe( "MF VideoPlaybackInitialize: com=0x%08X mf=0x%08X started=%d\n", (unsigned)hrCom, (unsigned)hr, g_bMFStarted ? 1 : 0 );
}

void VideoPlaybackShutdown()
{
	g_MFPlayers.RemoveAll();

	if ( g_bMFStarted )
	{
		MFShutdown();
		g_bMFStarted = false;
	}
	if ( g_bCOMInitialized )
	{
		CoUninitialize();
		g_bCOMInitialized = false;
	}
}

void VideoPlaybackRunFrame()
{
	for ( int i = 0; i < g_MFPlayers.Count(); ++i )
	{
		if ( g_MFPlayers[i] )
			g_MFPlayers[i]->Update();
	}
}

IVideoPlayer *CreateVideoPlayer( IVideoPlayerEventCallback *pEventCallback, IVideoPlayerVideoCallback *pVideoCallback, IVideoPlayerAudioCallback *pAudioCallback )
{
	if ( !g_bMFStarted )
		VideoPlaybackInitialize();

	CMFVideoPlayer *pPlayer = new CMFVideoPlayer( pEventCallback, pVideoCallback, pAudioCallback );
	if ( !pPlayer )
		return NULL;

	g_MFPlayers.AddToTail( pPlayer );
	SE_MFProbe( "MF CreateVideoPlayer -> %p\n", pPlayer );
	return pPlayer;
}

void DeleteVideoPlayer( IVideoPlayer *pVideoPlayer )
{
	if ( !pVideoPlayer )
		return;

	for ( int i = 0; i < g_MFPlayers.Count(); ++i )
	{
		if ( g_MFPlayers[i] == pVideoPlayer )
		{
			g_MFPlayers.Remove( i );
			break;
		}
	}
	delete pVideoPlayer;
}
