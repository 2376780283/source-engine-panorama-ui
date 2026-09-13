//============ Copyright (c) Valve Corporation, All rights reserved. ==========
//
//=============================================================================

#ifndef PANORAMAENGINEHANDLER_H
#define PANORAMAENGINEHANDLER_H
#pragma once

#include "tier0/platwindow.h"
#include "inputsystem/InputEnums.h"
#include "panorama/panorama.h"
#include "panorama/iuiengine.h"
#include "panorama/source2/ipanoramaui.h"
#include "panorama/iuiwindow.h"
#include "tier1/utlpriorityqueue.h"
#include "igame.h"
#include "IGameUIFuncs.h"

// SE port: pulling in the panorama UI headers drags <winbase.h> in with it, and the Windows SDK defines
// a no-argument Yield() macro there.  That silently breaks the thread/job declarations included later
// (IJob::Yield, CThread::Yield, ...) - tier0/threadtools.h carries the very same guard for the same
// reason.  Drop it for everything that includes this header.
#ifdef Yield
#undef Yield
#endif

//-----------------------------------------------------------------------------
// Panorama Engine code
//-----------------------------------------------------------------------------
class CPanoramaEngineHandler
{
	void RunFrame();
public:

	CPanoramaEngineHandler();

	bool IsPanoramaEnabled() const { return m_bValid; }

	virtual InitReturnVal_t Init();
	void Shutdown();

	bool ProcessUserInput( const InputEvent_t &ie );
	void ChangeResolution( const int nWindowWidth, const int nWindowHeight );

	panorama::IUIPanelClient *AddPanoramaView( const char *pchViewName, panorama::IUIWindow *pWindow );
	void RemovePanoramaView( panorama::IUIWindow *pWindow );

	void PanoramaRunFrame(int nSlot);
	void PanoramaRenderFrame(int nSlot);

	bool IsInECOMode() const;

	panorama::IUIEngine *UIEngine() { return m_pUIEngine; }

	// SE port: hosted-UI smoke test view.  CS:GO's engine never creates views itself - its game DLL does
	// that through IGameUIFuncs::AddPanoramaView - so until the game side is ported this creates one from
	// mods/panorama_test/panorama/layout/test.xml (file://{resources}/layout/test.xml in the mounted mod).
	// Reached through -panoramatest on the command line, or the "panorama_test" console command at runtime.
	bool CreatePanoramaTestView();
	void DestroyPanoramaTestView();
	bool HasPanoramaTestView() const { return m_pTestWindow != NULL; }

	// Dumps what the hosted UI is currently doing ("panorama_status").
	void PrintPanoramaStatus();

#if ( PLATFORM_WINDOWS && DEVELOPMENT_ONLY ) 
	bool IsDebuggerShown() { return m_bShowDebugger; };
	void SetDebuggerShown( bool bShow ) { m_bShowDebugger = bShow; }
	void ToggleDebugger();

	bool OnCloseDebuggerWindow();

	void SaveDebuggerDimentions();

	void CopyDebuggerRtBits( uint32* pBitsOut, uint32 nWd, uint32 nHt )
	{
		m_pDebugWindow->CopyDebuggerRtBits( pBitsOut, m_nDebuggerW, m_nDebuggerH);
	}
	int m_nDebuggerX;
	int m_nDebuggerY;
	int m_nDebuggerW;
	int m_nDebuggerH;
	void OnDebuggerResize( uint32 nNewWidth, uint32 nNewHeight );
#else

	bool IsDebuggerShown() { return false; }

#endif

	// Denies input to the game by filtering input events
	// Note that only mouse and key events will be filtered. It is important not to 
	// filter events such as IE_AppActivated and let the game handle such events
	// (eg. to correctly show/hide cursor)
	// AddGameInputHandler will return a non zero handle. AddGameInputHandler takes a
	// panel as a parameters. If the panel is valid, panorama will only filter input events
	// if the panel and its top level window are visible (checked every frame in RunFrame)
	// (Note that we are only checking the visibility of the panel and its top level window 
	// but not all ancestors of the  given panel, therefore it is still possible for the 
	// panel to not be visible on the screen but still deny input to the game). 
	// If pPanel is NULL, Panorama will always filter input events until the corresponding 
	// ReleaseGameInputHandler is being called.
	// ReleaseGameInputHandler takes a handle previously returned by AddGameInputHandler.
	uint64 AddGameInputHandler( panorama::IUIPanel *pPanel, panorama::EGameInputFlags eFlags, const char *pchDebugContextName );
	void ReleaseGameInputHandler( uint64 handle );
	bool DeniesInputToGame( panorama::EGameInputFlags eFlags ) const { return ( m_eGameInputFlags & eFlags ) == eFlags; }
	void DumpDenyAllInputToGame() const;
	
	void SetIMEAllowed( bool bAllowed );

	InputContextHandle_t GetInputContext() { return m_hPanoramaInputContext; }

#if ( PLATFORM_WINDOWS && DEVELOPMENT_ONLY )
	IPanoramaClientDebugger *m_pDebugger;
#endif

private:

	bool OnActivateMainWindow();
	bool OnWindowShutdown( panorama::IUIWindow *pWindow );
	void RecalculateInputOrder();

#if ( PLATFORM_WINDOWS && DEVELOPMENT_ONLY )
	bool OnCreateDebuggerWindow();
	bool OnBeginDebuggerInspect();
	PlatWindow_t CreateAppWindow( const char *pTitle, int nPlatWindowFlags, int x, int y, int w, int h );
#endif

	void OnProfileOnEvent();
	void OnProfileOffEvent();


private:

	bool m_bValid;

#if ( PLATFORM_WINDOWS && DEVELOPMENT_ONLY )
	bool m_bShowDebugger;
	panorama::IUIWindow *m_pDebugWindow;
	PlatWindow_t m_hDebuggerWindow;
	uint64 m_hDebuggerDenyInputToGame = 0;
#endif

    panorama::IUIEngine *m_pUIEngine;
	
	struct ViewEntry_t
	{
		ViewEntry_t() {}
		CUtlString m_sViewName;
		panorama::IUIWindow *m_pWindow;
	};
	static bool ViewPriorityOrder( ViewEntry_t const &lhs, ViewEntry_t const &rhs, void *pCtx );
	CUtlVector<ViewEntry_t> m_Views; // kept sorted in ascending priority order
	CUtlVector<CUtlString> m_vecViewsToRemove;
	CUtlVector<panorama::IUIWindow *> m_vecWindowInputOrder;
	CUtlVector<panorama::IUIWindow*> m_pWindows;

	// The -panoramatest / "panorama_test" view, NULL when it hasn't been created.
	panorama::IUIWindow *m_pTestWindow;

	int m_nMainWindowWidth;
	int m_nMainWindowHeight;

	InputContextHandle_t m_hPanoramaInputContext;
	uint64 m_nNextInputHandle;
	struct DenyInputEntry_t
	{
		uint64 m_handle;
		panorama::PanelHandle_t m_panelHandle;
		CUtlString m_debugName;
		panorama::EGameInputFlags m_eGameInputFlags;
	};
	CUtlVector< DenyInputEntry_t > m_denyAllInputEntries;
	// Flag used in CPanoramaEngineHandler::ProcessUserInput to filter input events
	// This flag is being recomputed every frame in CPanoramaEngineHandler::RunFrame
	// Flag set to true if a panel from m_denyAllInputEntries is visible
	panorama::EGameInputFlags m_eGameInputFlags;

	bool m_bInECOMode;
};

CPanoramaEngineHandler &PanoramaEngineHandler();

#endif // PANORAMAENGINEHANDLER_H
