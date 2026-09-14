//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: SE port - a webm (or any Media Foundation decodable) video painted as the main menu
//          background of the Source 2013 client, *without* involving panorama at all.
//
//          The panorama route (video/video_mf/mf_video_player.cpp + the <Movie> panel) decodes and
//          uploads correctly but the CS:GO main menu that hosts it only fades its background layer
//          in once its JS/data layer reports the game as ready - which this port's shim never does.
//          This panel sidesteps all of that: it is a plain VGUI panel at the bottom of the client
//          root panel, it draws the frames itself and the stock CS:S menu keeps working on top.
//
//=============================================================================//

#ifndef SE_BACKGROUND_VIDEO_H
#define SE_BACKGROUND_VIDEO_H
#ifdef _WIN32

#include "vgui_controls/Panel.h"
#include "tier1/utlvector.h"

struct IMFSourceReader;
struct IMFSample;
struct IMFMediaType;

//-----------------------------------------------------------------------------
// Purpose: Full screen video background.  Frames are decoded lazily from Paint() (main thread, once
//          per frame at most) and uploaded through ISurface::DrawSetTextureRGBA, which is the same
//          path VGUI uses for its own dynamically updated textures.
//-----------------------------------------------------------------------------
class CSEBackgroundMoviePanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CSEBackgroundMoviePanel, vgui::Panel );

public:
	CSEBackgroundMoviePanel( vgui::Panel *pParent );
	virtual ~CSEBackgroundMoviePanel();

	virtual void Paint();
	virtual void OnScreenSizeChanged( int nOldWide, int nOldTall );
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

private:
	void EnsureMovie();			// (re)opens the movie when the convar changed or after a failure
	void OpenMovie( const char *pchRelativePath );
	void CloseMovie();
	bool DecodeNextFrame();		// pulls samples until the frame that is due now is in m_rgba
	void ConvertNV12( const unsigned char *pData, int nStride );

	int					m_nTextureId;
	bool				m_bTextureCreated;

	char				m_szOpenedPath[ 256 ];		// path m_pReader was opened with
	IMFSourceReader *	m_pReader;
	IMFSample *			m_pPendingSample;
	int					m_nWidth;
	int					m_nHeight;
	int					m_nStride;
	bool				m_bNV12;
	bool				m_bEndOfStream;
	double				m_flFrameInterval;
	double				m_flStartTime;
	int					m_nFramesPresented;

	CUtlVector< unsigned char >	m_rgba;			// tightly packed RGBA, m_nWidth*m_nHeight*4
};

//-----------------------------------------------------------------------------
// Created/destroyed by the client root panel (cstrike/vgui_rootpanel_cs.cpp)
//-----------------------------------------------------------------------------
void SE_CreateBackgroundMoviePanel( vgui::Panel *pParent );
void SE_DestroyBackgroundMoviePanel();

#endif // _WIN32
#endif // SE_BACKGROUND_VIDEO_H
