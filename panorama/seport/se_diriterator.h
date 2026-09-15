//========== Copyright (c) Valve Corporation, All rights reserved. ============
//
// SE port: CDirIterator - CS:GO's directory iterator, for the Panorama font loaders.
//
// CS:GO declares CDirIterator in public/tier1/fileio.h and implements it in tier1/fileio.cpp.
// This tree's public/tier1/fileio.h is the Source 2013 one (CPathString + CDirWatcher only) and
// its tier1/fileio.cpp has no such class, so the surface text/uifontfileloaderwin32.cpp actually
// uses - BNextFile()/BCurrentIsDir()/CurrentFileName() over a wildcard such as
// "D:\cstrike\cstrike\panorama\fonts\*" - is reimplemented here on the Win32 find-file API.
//
// It stays in seport/ on purpose: only the port's font loader needs it, so public/ is left alone.
// The font folder is handed to the loader as an absolute OS path by
// CUIEngine::RegisterCustomFontPath(), which is what CS:GO's own loader walked as well.
//
//=============================================================================

#ifndef SE_PORT_DIRITERATOR_H
#define SE_PORT_DIRITERATOR_H

#ifdef _WIN32
#pragma once
#endif

//-----------------------------------------------------------------------------
// Purpose: Iterates the files in a directory (Win32 find-file based)
//-----------------------------------------------------------------------------
class CDirIterator
{
public:
	// pchPath: directory, pchPattern: wildcard within it ("*", "*.vfont", ...)
	CDirIterator( const char *pchPath, const char *pchPattern );
	// pchSearchPath: a complete wildcard ("D:/game/cstrike/panorama/fonts/*")
	explicit CDirIterator( const char *pchSearchPath );
	~CDirIterator();

	// Advances to the next entry.  The first call yields the first entry; false means the end.
	bool BNextFile();

	// Valid while BNextFile() has returned true for the current entry.
	bool IsValid() const { return m_bValid; }
	bool BCurrentIsDir() const { return m_bCurrentlyDir; }
	const char *CurrentFileName() const { return m_bValid ? m_rgchFileName : ""; }

private:
	void Open( const char *pchSearchPath );
	void Close();

	void *m_hFind;			// HANDLE from FindFirstFileA()
	void *m_pFindData;		// _WIN32_FIND_DATAA (kept out of the header - no windows.h here)
	bool m_bFirstFile;
	bool m_bValid;
	bool m_bCurrentlyDir;
	char m_rgchSearchPath[1024];
	char m_rgchFileName[1024];
};

#endif // SE_PORT_DIRITERATOR_H
