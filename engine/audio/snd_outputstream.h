//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: SE port - a separate PCM stream that connects to the engine's main output.
//
//          This is the engine half of CS:GO's IAudioOutputStream (public/engine/IEngineSound.h).
//          Panorama's video player asks for one through panorama's IUISoundSystem /
//          ISoundSystem::CreateOutputStream() (panorama_s1wrapper/wrap_sound.cpp ->
//          IEngineSound::CreateOutputStream()), queues decoded movie audio into it from the UI
//          thread with WriteAudioData(), and the audio thread mixes it into the paintbuffer once per
//          MIX_PaintChannels() quantum (see CAudioOutputStream::MixAllIntoPaintBuffer).
//
//          CS:GO itself does exactly this inside its engine; the SDK has the interface
//          (public/engine/IEngineSound.h) but no implementation, so it is implemented here.
//
//=============================================================================//

#ifndef SND_OUTPUTSTREAM_H
#define SND_OUTPUTSTREAM_H
#ifdef _WIN32
#pragma once
#endif

#include "engine/IEngineSound.h"
#include "engine/audio/snd_mix_buf.h"		// portable_samplepair_t
#include "tier0/threadtools.h"
#include "tier1/utlvector.h"

class CAudioOutputStream : public IAudioOutputStream
{
public:
	CAudioOutputStream( uint nSampleRate, uint nChannels, uint nBits );
	virtual ~CAudioOutputStream();

	//---- IAudioOutputStream ----------------------------------------------------------------------
	// nSampleCount is per channel, as the interface documents
	virtual void WriteAudioData( const int16 *pData, uint nSampleCount, uint nChannels );
	virtual void SetVolume( float flVolume );
	virtual uint32 QueuedSampleCount();
	virtual uint32 MaxWriteSampleCount();
	virtual uint32 LatencySamplesCount();
	virtual void Pause();
	virtual void Resume();

	//---- engine side (called from the audio thread) ----------------------------------------------
	// Adds the queued samples of every live stream to the front left/right of one 44.1kHz
	// paintbuffer quantum.  Called after the DSP stages, so movie audio is dry, but before the
	// buffer is clipped, so the master volume (applied during S_TransferPaintBuffer) and the
	// device's "mute on focus loss" handling still apply to it.
	static void MixAllIntoPaintBuffer( portable_samplepair_t *pFront, int nSampleCount );

	int GetSampleRate() const { return m_nSampleRate; }
	int GetChannelCount() const { return m_nChannels; }
	bool BIsPaused() const { return m_bPaused; }

	// bring-up probe helper: frames the mixer consumed since the last call, and resets the counter
	uint32 ConsumeMixedFrameCount();

	// milliseconds of this stream that have already been mixed out (CS:GO's video player clock)
	virtual uint32 GetMixedMilliseconds();

private:
	// the ring buffer is interleaved, m_nCapacity/m_nQueued count samples *per channel*, and the
	// read/write positions are frame (per channel sample) indices into it
	int FreeSampleCount() const { return m_nCapacity - m_nQueued; }
	void AdvanceRead( int nFrames );
	void PeekFrame( int nFrameOffset, int16 *pnLeft, int16 *pnRight ) const;
	void MixIntoPaintBuffer( portable_samplepair_t *pFront, int nSampleCount );

	CThreadFastMutex	m_Mutex;
	CUtlVector<int16>	m_vecSamples;		// ring buffer
	int					m_nCapacity;		// frames the ring buffer holds
	int					m_nReadFrame;		// ring index of the oldest frame
	int					m_nQueued;			// frames waiting to be mixed
	int					m_nSampleRate;
	int					m_nChannels;
	int					m_nBits;
	volatile float		m_flVolume;
	float				m_flVolumeCurrent;	// ramps towards m_flVolume over one mix quantum
	volatile bool		m_bPaused;
	double				m_flResamplePos;	// fractional frame position, only used when resampling
	uint32				m_nFramesMixed;		// bring-up probe counter
	uint64				m_unMixedFramesTotal;	// everything the mixer has taken out of this stream
	int					m_nOverflowWarnings;
};

#endif // SND_OUTPUTSTREAM_H
