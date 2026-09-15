//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: SE port - see gameui_util.h next to this file.  The body is CS:GO's
//          game/client/cstrike15/gameui/gameui_util.cpp::GameUI_MakeStringSafe, copied verbatim
//          (only the include set differs).
//
//=============================================================================//

#include "panorama/se_gameclient_common.h"
#include "gameui_util.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

//-----------------------------------------------------------------------------
// GameUI_MakeStringSafe
// Copy of ScaleformUIImpl::MakeStringSafe, which is to be deprecated shortly
//-----------------------------------------------------------------------------
void GameUI_MakeStringSafe( const wchar_t* oldName, wchar_t* newName, int destBufferSize )
{
	Assert( destBufferSize >= 2 * sizeof( wchar_t ) );

	// Empty strings stay empty
	if ( !*oldName )
	{
		newName[0] = L'\0';
		return;
	}

	// skip leading '#' to avoid loc-tag conflicts
	// $$$REI TODO: Remove skipping '#' since our usage of custom data should now be safe.
	while ( *oldName == L'#' )
		++oldName;

	// If we didn't write any characters but the input wasn't empty, replace entire input with "?"
	if ( !*oldName )
	{
		newName[0] = L'?';
		newName[1] = L'\0';
		return;
	}

	// Clean up HTML-characters
	int newNameBufSizeChars = destBufferSize / sizeof( wchar_t );
	V_BasicHtmlEntityEncode( newName, newNameBufSizeChars, oldName, V_wcslen( oldName ) );
}
