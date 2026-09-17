//========== Copyright (c) Valve Corporation, All rights reserved. ============
//
// SE port: CDirIterator on top of the Win32 find-file API - see se_diriterator.h for why it
// exists.  Only the three accessors the Panorama font loader calls are implemented; filenames
// stay 8-bit (the font packages are ASCII), so no Unicode conversion step is needed.
//
//=============================================================================

#include "stdafx.h"

#include <windows.h>

#include "se_diriterator.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

//-----------------------------------------------------------------------------
// Purpose: Open a session on the wildcard (the first match is handed out by the first BNextFile)
//-----------------------------------------------------------------------------
void CDirIterator::Open( const char *pchSearchPath )
{
	V_strncpy( m_rgchSearchPath, pchSearchPath, sizeof( m_rgchSearchPath ) );
	V_FixSlashes( m_rgchSearchPath );

	_WIN32_FIND_DATAA *pFindData = new _WIN32_FIND_DATAA();
	V_memset( pFindData, 0, sizeof( *pFindData ) );
	m_pFindData = pFindData;

	HANDLE hFind = FindFirstFileA( m_rgchSearchPath, pFindData );
	if ( hFind == INVALID_HANDLE_VALUE )
	{
		m_hFind = INVALID_HANDLE_VALUE;
		m_bFirstFile = false;
		m_bValid = false;
		return;
	}

	m_hFind = hFind;
	m_bFirstFile = true;
	m_bValid = false;
	m_bCurrentlyDir = false;
}


//-----------------------------------------------------------------------------
// Purpose: Close the session and release the find data
//-----------------------------------------------------------------------------
void CDirIterator::Close()
{
	if ( m_hFind && m_hFind != INVALID_HANDLE_VALUE )
	{
		FindClose( (HANDLE)m_hFind );
	}
	m_hFind = INVALID_HANDLE_VALUE;

	delete (_WIN32_FIND_DATAA *)m_pFindData;
	m_pFindData = NULL;

	m_bFirstFile = false;
	m_bValid = false;
	m_bCurrentlyDir = false;
	m_rgchFileName[0] = 0;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor taking a directory and a wildcard within it
//-----------------------------------------------------------------------------
CDirIterator::CDirIterator( const char *pchPath, const char *pchPattern ) :
	m_hFind( INVALID_HANDLE_VALUE ),
	m_pFindData( NULL ),
	m_bFirstFile( false ),
	m_bValid( false ),
	m_bCurrentlyDir( false )
{
	m_rgchSearchPath[0] = 0;
	m_rgchFileName[0] = 0;

	char szSearch[1024];
	V_strncpy( szSearch, pchPath ? pchPath : "", sizeof( szSearch ) );

	int nLen = V_strlen( szSearch );
	if ( nLen > 0 && szSearch[nLen - 1] != '/' && szSearch[nLen - 1] != '\\' )
	{
		V_strncat( szSearch, CORRECT_PATH_SEPARATOR_S, sizeof( szSearch ) );
	}
	V_strncat( szSearch, pchPattern ? pchPattern : "*", sizeof( szSearch ) );

	Open( szSearch );
}


//-----------------------------------------------------------------------------
// Purpose: Constructor taking a complete wildcard
//-----------------------------------------------------------------------------
CDirIterator::CDirIterator( const char *pchSearchPath ) :
	m_hFind( INVALID_HANDLE_VALUE ),
	m_pFindData( NULL ),
	m_bFirstFile( false ),
	m_bValid( false ),
	m_bCurrentlyDir( false )
{
	m_rgchSearchPath[0] = 0;
	m_rgchFileName[0] = 0;

	Open( pchSearchPath );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CDirIterator::~CDirIterator()
{
	Close();
}


//-----------------------------------------------------------------------------
// Purpose: Advance to the next entry, skipping the "." / ".." pseudo entries
//-----------------------------------------------------------------------------
bool CDirIterator::BNextFile()
{
	if ( !m_hFind || m_hFind == INVALID_HANDLE_VALUE || !m_pFindData )
	{
		m_bValid = false;
		return false;
	}

	_WIN32_FIND_DATAA *pFindData = (_WIN32_FIND_DATAA *)m_pFindData;

	if ( m_bFirstFile )
	{
		// FindFirstFileA() already positioned us on the first match
		m_bFirstFile = false;
	}
	else if ( !FindNextFileA( (HANDLE)m_hFind, pFindData ) )
	{
		m_bValid = false;
		return false;
	}

	while ( pFindData->cFileName[0] == '.' &&
			( pFindData->cFileName[1] == 0 || ( pFindData->cFileName[1] == '.' && pFindData->cFileName[2] == 0 ) ) )
	{
		if ( !FindNextFileA( (HANDLE)m_hFind, pFindData ) )
		{
			m_bValid = false;
			return false;
		}
	}

	m_bCurrentlyDir = ( pFindData->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) != 0;
	V_strncpy( m_rgchFileName, pFindData->cFileName, sizeof( m_rgchFileName ) );
	m_bValid = true;
	return true;
}
