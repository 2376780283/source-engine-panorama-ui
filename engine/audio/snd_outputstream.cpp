//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: SE port - implementation of IAudioOutputStream (see snd_outputstream.h).
//
//          panorama's video player owns one of these per playing movie: it decodes in
//          video/video_mf/mf_video_player.cpp and pushes 16 bit PCM through
//          IVideoPlayerAudioCallback -> CVideoPlayerAudioRenderer (panorama/data/panoramavideoplayer.cpp)
//          -> IUISoundSystem::CreateAudioOutputStream -> panorama_s1wrapper's CSoundSystem ->
//          IEngineSound::CreateOutputStream -> here.  The audio thread then mixes whatever is queued
//          into the paint buffer from MIX_PaintChannels() (engine/audio/snd_mix.cpp).
//
//          Everything the mixer touches is behind m_Mutex, and the stream registry is behind
//          s_StreamMutex, so the UI thread (decode/queue) and the audio thread (mix) cannot race and
//          a stream cannot be destroyed while it is being mixed.
//
//=============================================================================//

#include "audio/snd_outputstream.h"

#include "engine/audio/snd_device.h"	// SOUND_44k

#include "tier0/memdbgon.h"

// Every live stream, so the audio thread can find them from MixAllIntoPaintBuffer().
static CUtlVector< CAudioOutputStream * >	s_Streams;
static CThreadFastMutex						s_StreamMutex;

// CThreadFastMutexAutoLock is a CS:GO-era helper that this tree's tier0/threadtools.h does not have;
// this tiny local RAII lock keeps the bodies below readable.
class CAudioStreamLock
{
public:
	explicit CAudioStreamLock( CThreadFastMutex &mutex ) : m_pMutex( &mutex ) { m_pMutex->Lock(); }
	~CAudioStreamLock() { m_pMutex->Unlock(); }
private:
	CThreadFastMutex *m_pMutex;
	CAudioStreamLock( const CAudioStreamLock & );
	CAudioStreamLock &operator=( const CAudioStreamLock & );
};

// TEMPORARY bring-up probe (remove once movie audio is confirmed audible): prints how much movie
// audio the mixer actually consumed, once every N quanta (~23ms per quantum at 44.1kHz).  The lines
// go through SE_PortUIProbe() (engine/panoramaenginehandler.cpp) because Msg() output is lost when the
// process dies and engine.log stops being flushed early.
#define SE_AUDIO_PROBE_QUANTA	256
static int s_nProbeQuanta = 0;

void SE_PortUIProbe( const char *pFmt, ... );

//-----------------------------------------------------------------------------
// Construction / destruction
//-----------------------------------------------------------------------------
CAudioOutputStream::CAudioOutputStream( uint nSampleRate, uint nChannels, uint nBits )
{
	m_nSampleRate = ( nSampleRate > 0 ) ? (int)nSampleRate : 44100;
	m_nChannels = ( nChannels == 1 ) ? 1 : 2;
	m_nBits = ( nBits > 0 ) ? (int)nBits : 16;

	// Comfortably more than the lead the video player keeps queued (SE_MF_AUDIO_LEAD_MS in
	// video/video_mf/mf_video_player.cpp): with a ring the size of the lead the queue sat permanently
	// full, MaxWriteSampleCount() was 0, and the player's staging buffer grew until it hit its stall
	// guard and threw seconds of audio away.
	m_nCapacity = ( m_nSampleRate * 3 ) / 2;
	if ( m_nCapacity < 4096 )
		m_nCapacity = 4096;

	m_vecSamples.SetSize( m_nCapacity * m_nChannels );
	m_nReadFrame = 0;
	m_nQueued = 0;
	m_flVolume = 1.0f;
	m_flVolumeCurrent = 1.0f;
	m_bPaused = false;
	m_flResamplePos = 0.0;
	m_nFramesMixed = 0;
	m_unMixedFramesTotal = 0;
	m_nOverflowWarnings = 0;

	{
		CAudioStreamLock lock( s_StreamMutex );
		s_Streams.AddToTail( this );
	}

	SE_PortUIProbe( "SE audio: opened a %d Hz / %d ch / %d bit movie stream (%.1f s buffer)\n",
				   m_nSampleRate, m_nChannels, m_nBits, (float)m_nCapacity / (float)m_nSampleRate );
}

CAudioOutputStream::~CAudioOutputStream()
{
	{
		CAudioStreamLock lock( s_StreamMutex );
		s_Streams.FindAndRemove( this );
	}

	SE_PortUIProbe( "SE audio: closed the %d Hz / %d ch movie stream (%u frames were mixed)\n",
				   m_nSampleRate, m_nChannels, m_nFramesMixed );
}

//-----------------------------------------------------------------------------
// IAudioOutputStream
//-----------------------------------------------------------------------------
void CAudioOutputStream::WriteAudioData( const int16 *pData, uint nSampleCount, uint nChannels )
{
	if ( !pData || nSampleCount == 0 || nChannels == 0 )
		return;

	CAudioStreamLock lock( m_Mutex );

	int nFrames = (int)nSampleCount;
	const int nFree = FreeSampleCount();
	if ( nFrames > nFree )
	{
		// The player is supposed to stay inside MaxWriteSampleCount(); if it does not, keep the
		// audio that fits and drop the tail rather than the oldest frames (the mixer is behind, so
		// the queued frames are the ones about to be played).
		if ( m_nOverflowWarnings++ < 8 )
		{
			Warning( "SE port audio: movie stream overflow (%d frames, %d free) - dropping %d frames\n",
					 nFrames, nFree, nFrames - nFree );
		}
		nFrames = nFree;
		if ( nFrames <= 0 )
			return;
	}

	const int nWriteFrame = ( m_nReadFrame + m_nQueued ) % m_nCapacity;
	int16 *pRing = m_vecSamples.Base();

	for ( int i = 0; i < nFrames; ++i )
	{
		int16 *pDst = &pRing[ ( ( nWriteFrame + i ) % m_nCapacity ) * m_nChannels ];

		if ( (int)nChannels == m_nChannels )
		{
			pDst[ 0 ] = pData[ i * 2 + 0 ];
			pDst[ m_nChannels > 1 ? 1 : 0 ] = pData[ i * 2 + 1 ];
		}
		else if ( nChannels == 1 )
		{
			// mono source -> stereo stream
			pDst[ 0 ] = pData[ i ];
			if ( m_nChannels > 1 )
				pDst[ 1 ] = pData[ i ];
		}
		else
		{
			// stereo source -> mono stream
			pDst[ 0 ] = (int16)( ( (int)pData[ i * 2 + 0 ] + (int)pData[ i * 2 + 1 ] ) / 2 );
		}
	}

	m_nQueued += nFrames;
}

void CAudioOutputStream::SetVolume( float flVolume )
{
	if ( flVolume < 0.0f )
		flVolume = 0.0f;
	else if ( flVolume > 1.0f )
		flVolume = 1.0f;

	m_flVolume = flVolume;
}

uint32 CAudioOutputStream::QueuedSampleCount()
{
	CAudioStreamLock lock( m_Mutex );
	return (uint32)m_nQueued;
}

uint32 CAudioOutputStream::MaxWriteSampleCount()
{
	CAudioStreamLock lock( m_Mutex );
	return (uint32)FreeSampleCount();
}

uint32 CAudioOutputStream::LatencySamplesCount()
{
	CAudioStreamLock lock( m_Mutex );
	return (uint32)m_nQueued;
}

void CAudioOutputStream::Pause()
{
	m_bPaused = true;
}

void CAudioOutputStream::Resume()
{
	m_bPaused = false;
}

uint32 CAudioOutputStream::ConsumeMixedFrameCount()
{
	CAudioStreamLock lock( m_Mutex );
	const uint32 nRet = m_nFramesMixed;
	m_nFramesMixed = 0;
	return nRet;
}

//-----------------------------------------------------------------------------
// Playback clock: how much of this stream has already been mixed out.  The video player adds this to
// the movie position it started from, so the picture is presented when the sound is heard instead of
// when a wall clock says so (the stream is created a few frames after Play() and the player keeps a
// lead queued, both of which put the sound behind the picture).
//-----------------------------------------------------------------------------
uint32 CAudioOutputStream::GetMixedMilliseconds()
{
	CAudioStreamLock lock( m_Mutex );
	if ( m_nSampleRate <= 0 )
		return 0;
	return (uint32)( ( m_unMixedFramesTotal * 1000ull ) / (uint64)m_nSampleRate );
}

//-----------------------------------------------------------------------------
// Ring buffer helpers (m_Mutex held by the caller)
//-----------------------------------------------------------------------------
void CAudioOutputStream::AdvanceRead( int nFrames )
{
	if ( nFrames <= 0 )
		return;

	if ( nFrames > m_nQueued )
		nFrames = m_nQueued;

	m_nReadFrame = ( m_nReadFrame + nFrames ) % m_nCapacity;
	m_nQueued -= nFrames;
}

void CAudioOutputStream::PeekFrame( int nFrameOffset, int16 *pnLeft, int16 *pnRight ) const
{
	const int16 *p = &m_vecSamples[ ( ( m_nReadFrame + nFrameOffset ) % m_nCapacity ) * m_nChannels ];
	*pnLeft = p[ 0 ];
	*pnRight = ( m_nChannels > 1 ) ? p[ 1 ] : p[ 0 ];
}

//-----------------------------------------------------------------------------
// Mixing (audio thread)
//-----------------------------------------------------------------------------
void CAudioOutputStream::MixIntoPaintBuffer( portable_samplepair_t *pFront, int nSampleCount )
{
	if ( m_bPaused || pFront == NULL || nSampleCount <= 0 )
		return;

	CAudioStreamLock lock( m_Mutex );

	if ( m_nQueued <= 0 )
		return;

	const double flRatio = (double)m_nSampleRate / (double)SOUND_44k;

	float flTarget = m_flVolume;
	if ( flTarget < 0.0f )
		flTarget = 0.0f;
	else if ( flTarget > 1.0f )
		flTarget = 1.0f;

	const float flStep = ( flTarget - m_flVolumeCurrent ) / (float)nSampleCount;
	float flGain = m_flVolumeCurrent;

	int nConsumed = 0;
	int nMixedFrames = 0;

	for ( int i = 0; i < nSampleCount; ++i )
	{
		const int nSrc = (int)m_flResamplePos;
		if ( nSrc >= m_nQueued )
			break;								// starved: the player has not queued more yet

		const float flFrac = (float)( m_flResamplePos - (double)nSrc );

		int16 nL0, nR0, nL1, nR1;
		PeekFrame( nSrc, &nL0, &nR0 );
		if ( nSrc + 1 < m_nQueued )
			PeekFrame( nSrc + 1, &nL1, &nR1 );
		else
		{
			nL1 = nL0;
			nR1 = nR0;
		}

		const float flL = ( nL0 + ( nL1 - nL0 ) * flFrac ) * flGain;
		const float flR = ( nR0 + ( nR1 - nR0 ) * flFrac ) * flGain;

		pFront[ i ].left += (int)flL;
		pFront[ i ].right += (int)flR;

		flGain += flStep;
		m_flResamplePos += flRatio;

		nConsumed = (int)m_flResamplePos;
		++nMixedFrames;
	}

	m_flVolumeCurrent = flTarget;

	if ( nConsumed > 0 )
	{
		AdvanceRead( nConsumed );
		m_flResamplePos -= (double)nConsumed;
		if ( m_flResamplePos < 0.0 )
			m_flResamplePos = 0.0;
	}

	m_nFramesMixed += (uint32)nMixedFrames;
	m_unMixedFramesTotal += (uint64)nMixedFrames;
}

void CAudioOutputStream::MixAllIntoPaintBuffer( portable_samplepair_t *pFront, int nSampleCount )
{
	if ( pFront == NULL || nSampleCount <= 0 )
		return;

	uint32 nMixedThisQuantum = 0;
	{
		CAudioStreamLock lock( s_StreamMutex );

		for ( int i = 0; i < s_Streams.Count(); ++i )
			s_Streams[ i ]->MixIntoPaintBuffer( pFront, nSampleCount );

		// TEMPORARY bring-up probe: proves movie audio really reaches the paint buffer (44.1kHz
		// quantum ~= 23ms, so this prints about every 6 seconds while a movie is playing).
		if ( s_Streams.Count() > 0 && ( ++s_nProbeQuanta % SE_AUDIO_PROBE_QUANTA ) == 0 )
		{
			for ( int i = 0; i < s_Streams.Count(); ++i )
				nMixedThisQuantum += s_Streams[ i ]->ConsumeMixedFrameCount();
		}
	}

	if ( nMixedThisQuantum > 0 )
	{
		SE_PortUIProbe( "SE audio: movie stream mixed %u frames over %d quanta (~%.0f frames/s)\n",
					   nMixedThisQuantum, SE_AUDIO_PROBE_QUANTA,
					   (double)nMixedThisQuantum * SOUND_44k / ( SE_AUDIO_PROBE_QUANTA * nSampleCount ) );
	}
}
