//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: SE port - the gcsdk JSON/XML string escapers, for the ported cstrike15 uicomponents.
//
//          game/client/cstrike15/uicomponents/uicomponent_common.cpp does
//
//              // Using this from webapi_response to reuse some code.
//              extern void EmitJSONString( CUtlBuffer &outputBuffer, const char *pchValue );
//
//          In CS:GO that resolves against the game client's copy of webapi_response.  This tree has the
//          same function in gcsdk/webapi_response.cpp, but the gcsdk module cannot be linked into
//          panoramauiclient.dll (its headers collide with the panorama_s1wrapper tier1 this DLL is
//          built against - see the note in panorama/seport/gameclient/panorama/
//          se_gameclient_common.h).  The two functions are therefore repeated here, verbatim from
//          gcsdk/webapi_response.cpp (EmitJSONString / EmitXMLString), with only the include set
//          differing.
//
//=============================================================================//

#include "tier1/utlbuffer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

//-----------------------------------------------------------------------------
// Purpose: Helper for emitting properly escaped json string values
//-----------------------------------------------------------------------------
void EmitJSONString( CUtlBuffer &outputBuffer, const char *pchValue )
{
	outputBuffer.PutChar( '"' );

	if ( pchValue )
	{
		int i = 0;
		while( pchValue[i] )
		{
			switch ( pchValue[i] )
			{
			case '"':
				outputBuffer.Put( "\\\"", 2 );
				break;
			case '\\':
				outputBuffer.Put( "\\\\", 2 );
				break;
			case '\n':
				outputBuffer.Put( "\\n", 2 );
				break;
			case '\r':
				outputBuffer.Put( "\\r", 2 );
				break;
			case '\t':
				outputBuffer.Put( "\\t", 2 );
				break;
			default:
				if ( (uint8) pchValue[i] < 32 )
				{
					outputBuffer.Put( "\\u00", 4 );
					outputBuffer.PutChar( ( pchValue[i] & 16 ) ? '1' : '0' );
					outputBuffer.PutChar( "0123456789abcdef"[ pchValue[i] & 0xF ] );
				}
				else
				{
					outputBuffer.PutChar( pchValue[i] );
				}
			}
			++i;
		}
	}

	outputBuffer.PutChar( '"' );
}

//-----------------------------------------------------------------------------
// Purpose: Helper for emitting properly escaped XML string values, we always use UTF8,
// so we only really need to encode & ' " < >
//-----------------------------------------------------------------------------
void EmitXMLString( CUtlBuffer &outputBuffer, const char *pchValue )
{
	if ( pchValue )
	{
		int i = 0;
		while( pchValue[i] )
		{
			switch ( pchValue[i] )
			{
			case '&':
				outputBuffer.Put( "&amp;", 5 );
				break;
			case '\'':
				outputBuffer.Put( "&apos;", 6 );
				break;
			case '"':
				outputBuffer.Put( "&quot;", 6 );
				break;
			case '<':
				outputBuffer.Put( "&lt;", 4 );
				break;
			case '>':
				outputBuffer.Put( "&gt;", 4 );
				break;
			default:
				outputBuffer.PutChar( pchValue[i] );
			}
			++i;
		}
	}
}
