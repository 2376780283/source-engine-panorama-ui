//=========== libparsifal-0.8.3 - SE port implementation =======================//
//
// Self-contained XML reader implementing the Parsifal subset used by
// panorama/layout/layoutfile.cpp.  See include/libparsifal/parsifal.h for why
// this exists instead of the upstream sources.
//
// Scope: well-formed, non-validating, UTF-8 input; no DTD, no namespaces
// (uri is always NULL), no external entities.  Element content and attribute
// values get the five predefined entities plus numeric character references
// decoded; CDATA sections, comments and processing instructions are handled.
//
// NOTE: the build forces /TP (compile as C++), so this is written as C++ with
// C linkage through the header's extern "C" block.
//=============================================================================//

#include "../include/libparsifal/parsifal.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

namespace
{

//-----------------------------------------------------------------------------
// byte buffer helpers
//-----------------------------------------------------------------------------
struct SGrowBuf
{
	XMLCH *pData;
	int nLen;
	int nAlloc;
};

void GrowBufEnsure( SGrowBuf *pBuf, int nAdditional )
{
	if ( pBuf->nLen + nAdditional + 1 <= pBuf->nAlloc )
		return;

	int nNewAlloc = pBuf->nAlloc ? pBuf->nAlloc : 4096;
	while ( nNewAlloc < pBuf->nLen + nAdditional + 1 )
		nNewAlloc *= 2;

	pBuf->pData = (XMLCH *)realloc( pBuf->pData, nNewAlloc );
	pBuf->nAlloc = nNewAlloc;
}

void GrowBufAppend( SGrowBuf *pBuf, const XMLCH *pData, int nLen )
{
	if ( nLen <= 0 )
		return;

	GrowBufEnsure( pBuf, nLen );
	memcpy( pBuf->pData + pBuf->nLen, pData, nLen );
	pBuf->nLen += nLen;
	pBuf->pData[ pBuf->nLen ] = 0;
}

void GrowBufAppendChar( SGrowBuf *pBuf, XMLCH ch )
{
	GrowBufAppend( pBuf, &ch, 1 );
}

void GrowBufFree( SGrowBuf *pBuf )
{
	free( pBuf->pData );
	pBuf->pData = NULL;
	pBuf->nLen = pBuf->nAlloc = 0;
}

//-----------------------------------------------------------------------------
// parser state (stashed in parser->reader so the handler callbacks can find it
// through XMLParser_GetCurrentLine/Column)
//-----------------------------------------------------------------------------
struct SParseState
{
	LPXMLPARSER pParser;
	const XMLCH *pIn;
	int nInLen;
	int nPos;
	int nLine;
	int nCol;

	XMLCH **ppElementStack;	// owned element name copies
	int nElementCount;
	int nElementAlloc;

	bool bFailed;
	bool bSawRoot;
	bool bRootClosed;
};

void SetParserError( SParseState *pState, int nCode, const char *pchFormat, ... )
{
	if ( pState->bFailed )
		return;

	pState->bFailed = true;

	LPXMLPARSER pParser = pState->pParser;
	pParser->ErrorCode = nCode;
	pParser->ErrorLine = pState->nLine;
	pParser->ErrorColumn = pState->nCol;

	va_list args;
	va_start( args, pchFormat );
	char rgchMessage[ 512 ];
	vsnprintf( rgchMessage, sizeof( rgchMessage ), pchFormat, args );
	va_end( args );
	rgchMessage[ sizeof( rgchMessage ) - 1 ] = 0;

	strncpy( (char *)pParser->ErrorString, rgchMessage, sizeof( pParser->ErrorString ) - 1 );
	pParser->ErrorString[ sizeof( pParser->ErrorString ) - 1 ] = 0;

	if ( pParser->errorHandler )
		pParser->errorHandler( pParser );
}

//-----------------------------------------------------------------------------
// input scanning helpers
//-----------------------------------------------------------------------------
inline bool AtEnd( SParseState *pState )
{
	return pState->nPos >= pState->nInLen;
}

void AdvanceChars( SParseState *pState, int nCount )
{
	for ( int i = 0; i < nCount && !AtEnd( pState ); ++i )
	{
		if ( pState->pIn[ pState->nPos ] == '\n' )
		{
			++pState->nLine;
			pState->nCol = 1;
		}
		else
		{
			++pState->nCol;
		}
		++pState->nPos;
	}
}

bool Matches( SParseState *pState, const char *pchLiteral )
{
	int nLen = (int)strlen( pchLiteral );
	if ( pState->nPos + nLen > pState->nInLen )
		return false;

	return memcmp( pState->pIn + pState->nPos, pchLiteral, nLen ) == 0;
}

XMLCH PeekAt( SParseState *pState, int nOffset )
{
	if ( pState->nPos + nOffset >= pState->nInLen )
		return 0;

	return pState->pIn[ pState->nPos + nOffset ];
}

void SkipWhitespace( SParseState *pState )
{
	while ( !AtEnd( pState ) )
	{
		XMLCH ch = pState->pIn[ pState->nPos ];
		if ( ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' )
			AdvanceChars( pState, 1 );
		else
			break;
	}
}

inline bool IsWhiteSpaceChar( XMLCH ch )
{
	return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

inline bool IsNameStartChar( XMLCH ch )
{
	return ( ch >= 'a' && ch <= 'z' ) || ( ch >= 'A' && ch <= 'Z' ) || ch == '_' || ch == ':' || ch >= 0x80;
}

inline bool IsNameChar( XMLCH ch )
{
	return IsNameStartChar( ch ) || ( ch >= '0' && ch <= '9' ) || ch == '-' || ch == '.';
}

// Reads an XML name, returns an owned NUL terminated copy (NULL on error)
XMLCH *ReadName( SParseState *pState )
{
	if ( AtEnd( pState ) || !IsNameStartChar( pState->pIn[ pState->nPos ] ) )
	{
		SetParserError( pState, ERR_XMLP_INVALID_NAME, "Invalid XML name at line %d, column %d", pState->nLine, pState->nCol );
		return NULL;
	}

	int nStart = pState->nPos;
	while ( !AtEnd( pState ) && IsNameChar( pState->pIn[ pState->nPos ] ) )
		AdvanceChars( pState, 1 );

	int nLen = pState->nPos - nStart;
	XMLCH *pName = (XMLCH *)malloc( nLen + 1 );
	memcpy( pName, pState->pIn + nStart, nLen );
	pName[ nLen ] = 0;
	return pName;
}

// Skips "<!...>" style declarations, honouring quotes and an internal [ ] subset
bool SkipDeclaration( SParseState *pState )
{
	AdvanceChars( pState, 2 );	// "<!"

	int nBracketDepth = 0;
	XMLCH chQuote = 0;

	while ( !AtEnd( pState ) )
	{
		XMLCH ch = pState->pIn[ pState->nPos ];

		if ( chQuote )
		{
			if ( ch == chQuote )
				chQuote = 0;
			AdvanceChars( pState, 1 );
			continue;
		}

		if ( ch == '"' || ch == '\'' )
		{
			chQuote = ch;
		}
		else if ( ch == '[' )
		{
			++nBracketDepth;
		}
		else if ( ch == ']' )
		{
			if ( nBracketDepth > 0 )
				--nBracketDepth;
		}
		else if ( ch == '>' && nBracketDepth == 0 )
		{
			AdvanceChars( pState, 1 );
			return true;
		}

		AdvanceChars( pState, 1 );
	}

	SetParserError( pState, ERR_XMLP_READER_FATAL, "Unterminated declaration at line %d, column %d", pState->nLine, pState->nCol );
	return false;
}

// Skips to just past pchTerminator (e.g. "-->" or "?>")
bool SkipToTerminator( SParseState *pState, const char *pchTerminator )
{
	while ( !AtEnd( pState ) )
	{
		if ( Matches( pState, pchTerminator ) )
		{
			AdvanceChars( pState, (int)strlen( pchTerminator ) );
			return true;
		}
		AdvanceChars( pState, 1 );
	}

	SetParserError( pState, ERR_XMLP_READER_FATAL, "Unterminated \"%s\" at line %d, column %d", pchTerminator, pState->nLine, pState->nCol );
	return false;
}

//-----------------------------------------------------------------------------
// entity handling
//-----------------------------------------------------------------------------
void AppendUTF8CodePoint( SGrowBuf *pBuf, unsigned int unValue )
{
	if ( unValue < 0x80 )
	{
		GrowBufAppendChar( pBuf, (XMLCH)unValue );
	}
	else if ( unValue < 0x800 )
	{
		GrowBufAppendChar( pBuf, (XMLCH)( 0xC0 | ( unValue >> 6 ) ) );
		GrowBufAppendChar( pBuf, (XMLCH)( 0x80 | ( unValue & 0x3F ) ) );
	}
	else if ( unValue < 0x10000 )
	{
		GrowBufAppendChar( pBuf, (XMLCH)( 0xE0 | ( unValue >> 12 ) ) );
		GrowBufAppendChar( pBuf, (XMLCH)( 0x80 | ( ( unValue >> 6 ) & 0x3F ) ) );
		GrowBufAppendChar( pBuf, (XMLCH)( 0x80 | ( unValue & 0x3F ) ) );
	}
	else
	{
		GrowBufAppendChar( pBuf, (XMLCH)( 0xF0 | ( unValue >> 18 ) ) );
		GrowBufAppendChar( pBuf, (XMLCH)( 0x80 | ( ( unValue >> 12 ) & 0x3F ) ) );
		GrowBufAppendChar( pBuf, (XMLCH)( 0x80 | ( ( unValue >> 6 ) & 0x3F ) ) );
		GrowBufAppendChar( pBuf, (XMLCH)( 0x80 | ( unValue & 0x3F ) ) );
	}
}

// Expands the predefined entities and numeric character references in
// [pStart, pStart+nLen) into pOut.  Unknown named entities are copied through
// verbatim (see the note in parsifal.h).
bool DecodeEntities( SParseState *pState, const XMLCH *pStart, int nLen, SGrowBuf *pOut )
{
	int i = 0;
	while ( i < nLen )
	{
		if ( pStart[ i ] != '&' )
		{
			GrowBufAppendChar( pOut, pStart[ i ] );
			++i;
			continue;
		}

		// find the ';'
		int nEntityLen = 0;
		while ( i + nEntityLen < nLen && pStart[ i + nEntityLen ] != ';' && nEntityLen < 32 )
			++nEntityLen;

		if ( i + nEntityLen >= nLen || pStart[ i + nEntityLen ] != ';' )
		{
			// not an entity, emit literally
			GrowBufAppendChar( pOut, pStart[ i ] );
			++i;
			continue;
		}

		const XMLCH *pName = pStart + i + 1;
		int nNameLen = nEntityLen - 1;

		if ( nNameLen == 2 && pName[ 0 ] == 'l' && pName[ 1 ] == 't' )
			GrowBufAppendChar( pOut, '<' );
		else if ( nNameLen == 2 && pName[ 0 ] == 'g' && pName[ 1 ] == 't' )
			GrowBufAppendChar( pOut, '>' );
		else if ( nNameLen == 3 && pName[ 0 ] == 'a' && pName[ 1 ] == 'm' && pName[ 2 ] == 'p' )
			GrowBufAppendChar( pOut, '&' );
		else if ( nNameLen == 4 && pName[ 0 ] == 'q' && pName[ 1 ] == 'u' && pName[ 2 ] == 'o' && pName[ 3 ] == 't' )
			GrowBufAppendChar( pOut, '"' );
		else if ( nNameLen == 4 && pName[ 0 ] == 'a' && pName[ 1 ] == 'p' && pName[ 2 ] == 'o' && pName[ 3 ] == 's' )
			GrowBufAppendChar( pOut, '\'' );
		else if ( nNameLen >= 2 && pName[ 0 ] == '#' )
		{
			unsigned int unValue = 0;
			if ( pName[ 1 ] == 'x' || pName[ 1 ] == 'X' )
			{
				for ( int n = 2; n < nNameLen; ++n )
				{
					XMLCH ch = pName[ n ];
					unsigned int unDigit = ( ch >= '0' && ch <= '9' ) ? ( ch - '0' ) :
						( ch >= 'a' && ch <= 'f' ) ? ( ch - 'a' + 10 ) :
						( ch >= 'A' && ch <= 'F' ) ? ( ch - 'A' + 10 ) : 0xFFFFFFFF;
					if ( unDigit == 0xFFFFFFFF )
						break;
					unValue = unValue * 16 + unDigit;
				}
			}
			else
			{
				for ( int n = 1; n < nNameLen; ++n )
				{
					XMLCH ch = pName[ n ];
					if ( ch < '0' || ch > '9' )
						break;
					unValue = unValue * 10 + ( ch - '0' );
				}
			}

			if ( unValue == 0 || unValue > 0x10FFFF )
			{
				SetParserError( pState, ERR_XMLP_ILLEGAL_CHAR, "Illegal character reference at line %d, column %d", pState->nLine, pState->nCol );
				return false;
			}

			AppendUTF8CodePoint( pOut, unValue );
		}
		else
		{
			// undefined entity: pass through verbatim (upstream would fail here)
			GrowBufAppend( pOut, pStart + i, nEntityLen + 1 );
		}

		i += nEntityLen + 1;
	}

	return true;
}

//-----------------------------------------------------------------------------
// element stack
//-----------------------------------------------------------------------------
void PushElement( SParseState *pState, XMLCH *pName )
{
	if ( pState->nElementCount == pState->nElementAlloc )
	{
		int nNewAlloc = pState->nElementAlloc ? pState->nElementAlloc * 2 : 32;
		pState->ppElementStack = (XMLCH **)realloc( pState->ppElementStack, nNewAlloc * sizeof( XMLCH * ) );
		pState->nElementAlloc = nNewAlloc;
	}

	pState->ppElementStack[ pState->nElementCount++ ] = pName;
}

XMLCH *PopElement( SParseState *pState )
{
	if ( pState->nElementCount == 0 )
		return NULL;

	return pState->ppElementStack[ --pState->nElementCount ];
}

XMLCH *TopElement( SParseState *pState )
{
	if ( pState->nElementCount == 0 )
		return NULL;

	return pState->ppElementStack[ pState->nElementCount - 1 ];
}

void FreeElementStack( SParseState *pState )
{
	for ( int i = 0; i < pState->nElementCount; ++i )
		free( pState->ppElementStack[ i ] );

	free( pState->ppElementStack );
	pState->ppElementStack = NULL;
	pState->nElementCount = pState->nElementAlloc = 0;
}

//-----------------------------------------------------------------------------
// handler dispatch helpers
//-----------------------------------------------------------------------------
const XMLCH *DeriveLocalName( const XMLCH *pName )
{
	const XMLCH *pColon = (const XMLCH *)strchr( (const char *)pName, ':' );
	return pColon ? pColon + 1 : pName;
}

void DerivePrefix( const XMLCH *pName, XMLCH **ppPrefix )
{
	*ppPrefix = NULL;
	const XMLCH *pColon = (const XMLCH *)strchr( (const char *)pName, ':' );
	if ( !pColon || pColon == pName )
		return;

	int nLen = (int)( pColon - pName );
	*ppPrefix = (XMLCH *)malloc( nLen + 1 );
	memcpy( *ppPrefix, pName, nLen );
	( *ppPrefix )[ nLen ] = 0;
}

bool CallCharactersHandler( SParseState *pState, const XMLCH *pText, int nLen )
{
	LPXMLPARSER pParser = pState->pParser;

	bool bAllWhitespace = true;
	for ( int i = 0; i < nLen; ++i )
	{
		if ( !IsWhiteSpaceChar( pText[ i ] ) )
		{
			bAllWhitespace = false;
			break;
		}
	}

	XML_CHARACTERS_HANDLER pHandler = bAllWhitespace ? pParser->ignorableWhitespaceHandler : pParser->charactersHandler;
	if ( !pHandler )
		pHandler = pParser->charactersHandler;
	if ( !pHandler )
		pHandler = pParser->ignorableWhitespaceHandler;

	if ( pHandler && pHandler( pParser->UserData, pText, nLen ) != XML_OK )
	{
		SetParserError( pState, ERR_XMLP_ABORT, "Aborted by characters handler at line %d, column %d", pState->nLine, pState->nCol );
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// text run (entity decoded)
//-----------------------------------------------------------------------------
bool EmitText( SParseState *pState, const XMLCH *pStart, int nLen )
{
	SGrowBuf decoded = { NULL, 0, 0 };

	if ( !DecodeEntities( pState, pStart, nLen, &decoded ) )
	{
		GrowBufFree( &decoded );
		return false;
	}

	bool bOk = CallCharactersHandler( pState, decoded.pData ? decoded.pData : (const XMLCH *)"", decoded.nLen );
	GrowBufFree( &decoded );
	return bOk;
}

//-----------------------------------------------------------------------------
// attribute list
//-----------------------------------------------------------------------------
struct SAttributeList
{
	XMLRUNTIMEATT **ppAtts;
	int nCount;
	int nAlloc;
	XMLVECTOR vec;
};

void AttListInit( SAttributeList *pList )
{
	memset( pList, 0, sizeof( *pList ) );
}

void AttListFree( SAttributeList *pList )
{
	for ( int i = 0; i < pList->nCount; ++i )
	{
		XMLRUNTIMEATT *pAtt = pList->ppAtts[ i ];
		free( pAtt->qname );
		free( pAtt->value );
		free( pAtt->prefix );
		free( pAtt );
	}

	free( pList->ppAtts );
	free( pList->vec.items );
	AttListInit( pList );
}

void AttListBuildVector( SAttributeList *pList )
{
	pList->vec.items = (void **)malloc( pList->nCount * sizeof( void * ) );
	for ( int i = 0; i < pList->nCount; ++i )
		pList->vec.items[ i ] = pList->ppAtts[ i ];

	pList->vec.length = pList->vec.size = pList->nCount;
}

bool AttListAdd( SAttributeList *pList, XMLRUNTIMEATT *pAtt )
{
	if ( pList->nCount == pList->nAlloc )
	{
		int nNewAlloc = pList->nAlloc ? pList->nAlloc * 2 : 8;
		pList->ppAtts = (XMLRUNTIMEATT **)realloc( pList->ppAtts, nNewAlloc * sizeof( XMLRUNTIMEATT * ) );
		pList->nAlloc = nNewAlloc;
	}

	pList->ppAtts[ pList->nCount++ ] = pAtt;
	return true;
}

bool ParseAttribute( SParseState *pState, const XMLCH *pchElementName, SAttributeList *pList )
{
	XMLCH *pAttName = ReadName( pState );
	if ( !pAttName )
		return false;

	SkipWhitespace( pState );

	if ( AtEnd( pState ) || pState->pIn[ pState->nPos ] != '=' )
	{
		SetParserError( pState, ERR_XMLP_WS_REQUIRED, "Attribute \"%s\" of <%s> is missing a value", (const char *)pAttName, (const char *)pchElementName );
		free( pAttName );
		return false;
	}
	AdvanceChars( pState, 1 );

	SkipWhitespace( pState );

	if ( AtEnd( pState ) || ( pState->pIn[ pState->nPos ] != '"' && pState->pIn[ pState->nPos ] != '\'' ) )
	{
		SetParserError( pState, ERR_XMLP_INVALID_ATT_VALUE, "Attribute \"%s\" of <%s> has an unquoted value", (const char *)pAttName, (const char *)pchElementName );
		free( pAttName );
		return false;
	}

	XMLCH chQuote = pState->pIn[ pState->nPos ];
	AdvanceChars( pState, 1 );

	int nValueStart = pState->nPos;
	while ( !AtEnd( pState ) && pState->pIn[ pState->nPos ] != chQuote )
		AdvanceChars( pState, 1 );

	if ( AtEnd( pState ) )
	{
		SetParserError( pState, ERR_XMLP_INVALID_ATT_VALUE, "Unterminated value for attribute \"%s\" of <%s>", (const char *)pAttName, (const char *)pchElementName );
		free( pAttName );
		return false;
	}

	int nValueLen = pState->nPos - nValueStart;
	AdvanceChars( pState, 1 );	// closing quote

	SGrowBuf decodedValue = { NULL, 0, 0 };
	if ( !DecodeEntities( pState, pState->pIn + nValueStart, nValueLen, &decodedValue ) )
	{
		GrowBufFree( &decodedValue );
		free( pAttName );
		return false;
	}

	// XML forbids duplicate attributes
	for ( int i = 0; i < pList->nCount; ++i )
	{
		if ( strcmp( (const char *)pList->ppAtts[ i ]->qname, (const char *)pAttName ) == 0 )
		{
			SetParserError( pState, ERR_XMLP_DUPL_ATTRIBUTE, "Duplicate attribute \"%s\" in <%s>", (const char *)pAttName, (const char *)pchElementName );
			GrowBufFree( &decodedValue );
			free( pAttName );
			return false;
		}
	}

	XMLRUNTIMEATT *pAtt = (XMLRUNTIMEATT *)calloc( 1, sizeof( XMLRUNTIMEATT ) );
	pAtt->qname = pAttName;
	pAtt->localName = (XMLCH *)DeriveLocalName( pAttName );
	DerivePrefix( pAttName, &pAtt->prefix );
	pAtt->uri = NULL;
	pAtt->nameBuf.str = pAttName;
	pAtt->nameBuf.len = (int)strlen( (const char *)pAttName );

	if ( decodedValue.pData )
	{
		pAtt->value = decodedValue.pData;	// ownership transferred
		pAtt->valBuf.str = decodedValue.pData;
		pAtt->valBuf.len = decodedValue.nLen;
	}
	else
	{
		pAtt->value = (XMLCH *)calloc( 1, 1 );
		pAtt->valBuf.str = pAtt->value;
		pAtt->valBuf.len = 0;
	}

	return AttListAdd( pList, pAtt );
}

bool ParseAttributes( SParseState *pState, const XMLCH *pchElementName, SAttributeList *pList, bool *pbSelfClosing )
{
	*pbSelfClosing = false;

	for ( ;; )
	{
		SkipWhitespace( pState );

		if ( AtEnd( pState ) )
		{
			SetParserError( pState, ERR_XMLP_UNCLOSED_TAG, "Unexpected end of file inside <%s> tag", (const char *)pchElementName );
			return false;
		}

		XMLCH ch = pState->pIn[ pState->nPos ];

		if ( ch == '>' )
		{
			AdvanceChars( pState, 1 );
			return true;
		}

		if ( ch == '/' )
		{
			if ( PeekAt( pState, 1 ) != '>' )
			{
				SetParserError( pState, ERR_XMLP_INVALID_TOKEN, "Expected \">\" after \"/\" in <%s> tag", (const char *)pchElementName );
				return false;
			}
			AdvanceChars( pState, 2 );
			*pbSelfClosing = true;
			return true;
		}

		if ( !ParseAttribute( pState, pchElementName, pList ) )
			return false;
	}
}

//-----------------------------------------------------------------------------
// start tag: <name att="value" ...> or <name .../>
//-----------------------------------------------------------------------------
bool ParseStartTag( SParseState *pState )
{
	LPXMLPARSER pParser = pState->pParser;

	AdvanceChars( pState, 1 );	// '<'

	XMLCH *pName = ReadName( pState );
	if ( !pName )
		return false;

	SAttributeList atts;
	AttListInit( &atts );

	bool bSelfClosing = false;
	if ( !ParseAttributes( pState, pName, &atts, &bSelfClosing ) )
	{
		AttListFree( &atts );
		free( pName );
		return false;
	}

	AttListBuildVector( &atts );

	if ( !pState->bSawRoot )
	{
		pState->bSawRoot = true;
		pParser->DocumentElement = (XMLCH *)malloc( strlen( (const char *)pName ) + 1 );
		strcpy( (char *)pParser->DocumentElement, (const char *)pName );
	}
	else if ( pState->nElementCount == 0 )
	{
		SetParserError( pState, ERR_XMLP_MULTIPLE_TOP, "Multiple top level elements (found <%s>) at line %d, column %d", (const char *)pName, pState->nLine, pState->nCol );
		AttListFree( &atts );
		free( pName );
		return false;
	}

	if ( pParser->startElementHandler &&
		 pParser->startElementHandler( pParser->UserData, NULL, DeriveLocalName( pName ), pName, &atts.vec ) != XML_OK )
	{
		SetParserError( pState, ERR_XMLP_ABORT, "Aborted by element handler at line %d, column %d", pState->nLine, pState->nCol );
		AttListFree( &atts );
		free( pName );
		return false;
	}

	AttListFree( &atts );

	if ( bSelfClosing )
	{
		// <name/> fires the end handler immediately
		if ( pParser->endElementHandler &&
			 pParser->endElementHandler( pParser->UserData, NULL, DeriveLocalName( pName ), pName ) != XML_OK )
		{
			SetParserError( pState, ERR_XMLP_ABORT, "Aborted by element handler at line %d, column %d", pState->nLine, pState->nCol );
			free( pName );
			return false;
		}

		free( pName );
	}
	else
	{
		PushElement( pState, pName );	// the stack takes ownership
	}

	return !pState->bFailed;
}

//-----------------------------------------------------------------------------
// end tag: </name>
//-----------------------------------------------------------------------------
bool ParseEndTag( SParseState *pState )
{
	LPXMLPARSER pParser = pState->pParser;

	AdvanceChars( pState, 2 );	// "</"

	XMLCH *pName = ReadName( pState );
	if ( !pName )
		return false;

	SkipWhitespace( pState );

	if ( AtEnd( pState ) || pState->pIn[ pState->nPos ] != '>' )
	{
		SetParserError( pState, ERR_XMLP_EXPECTED_FOUND, "Expected \">\" to close </%s>", (const char *)pName );
		free( pName );
		return false;
	}
	AdvanceChars( pState, 1 );

	XMLCH *pOpenName = TopElement( pState );
	if ( !pOpenName )
	{
		SetParserError( pState, ERR_XMLP_INVALID_END_TAG, "Unexpected end tag </%s> at line %d, column %d", (const char *)pName, pState->nLine, pState->nCol );
		free( pName );
		return false;
	}

	if ( strcmp( (const char *)pOpenName, (const char *)pName ) != 0 )
	{
		SetParserError( pState, ERR_XMLP_INVALID_END_TAG, "End tag </%s> does not match open element <%s> at line %d, column %d",
						(const char *)pName, (const char *)pOpenName, pState->nLine, pState->nCol );
		free( pName );
		return false;
	}

	PopElement( pState );
	free( pOpenName );

	if ( pParser->endElementHandler &&
		 pParser->endElementHandler( pParser->UserData, NULL, DeriveLocalName( pName ), pName ) != XML_OK )
	{
		SetParserError( pState, ERR_XMLP_ABORT, "Aborted by element handler at line %d, column %d", pState->nLine, pState->nCol );
		free( pName );
		return false;
	}

	free( pName );

	if ( pState->nElementCount == 0 )
		pState->bRootClosed = true;

	return true;
}

//-----------------------------------------------------------------------------
// CDATA: <![CDATA[ ... ]]>
//-----------------------------------------------------------------------------
bool ParseCDATA( SParseState *pState )
{
	LPXMLPARSER pParser = pState->pParser;

	AdvanceChars( pState, 9 );	// "<![CDATA["

	int nStart = pState->nPos;
	while ( !AtEnd( pState ) && !Matches( pState, "]]>" ) )
		AdvanceChars( pState, 1 );

	if ( AtEnd( pState ) )
	{
		SetParserError( pState, ERR_XMLP_READER_FATAL, "Unterminated CDATA section at line %d, column %d", pState->nLine, pState->nCol );
		return false;
	}

	int nLen = pState->nPos - nStart;

	if ( pParser->startCDATAHandler && pParser->startCDATAHandler( pParser->UserData ) != XML_OK )
	{
		SetParserError( pState, ERR_XMLP_ABORT, "Aborted by CDATA handler at line %d, column %d", pState->nLine, pState->nCol );
		return false;
	}

	if ( pParser->charactersHandler && pParser->charactersHandler( pParser->UserData, pState->pIn + nStart, nLen ) != XML_OK )
	{
		SetParserError( pState, ERR_XMLP_ABORT, "Aborted by characters handler at line %d, column %d", pState->nLine, pState->nCol );
		return false;
	}

	if ( pParser->endCDATAHandler && pParser->endCDATAHandler( pParser->UserData ) != XML_OK )
	{
		SetParserError( pState, ERR_XMLP_ABORT, "Aborted by CDATA handler at line %d, column %d", pState->nLine, pState->nCol );
		return false;
	}

	AdvanceChars( pState, 3 );	// "]]>"
	return true;
}

} // anonymous namespace

//=============================================================================
// public API
//=============================================================================
LPXMLPARSER XMLParser_Create( LPXMLPARSER *ppParser )
{
	if ( !ppParser )
		return NULL;

	XMLPARSER *pParser = (XMLPARSER *)calloc( 1, sizeof( XMLPARSER ) );
	if ( !pParser )
		return NULL;

	pParser->XMLFlags = XMLFLAG_NAMESPACES;

	*ppParser = pParser;
	return pParser;
}

void XMLParser_Free( LPXMLPARSER pParser )
{
	if ( !pParser )
		return;

	free( pParser->DocumentElement );
	pParser->DocumentElement = NULL;

	free( pParser );
}

int XMLParser_Parse( LPXMLPARSER pParser, LPFNINPUTSRC inputSrc, void *inputData, const XMLCH *pchEncoding )
{
	(void)pchEncoding;

	if ( !pParser || !inputSrc )
		return XML_ABORT;

	// ---- read the whole stream up front (layout files are small) ----------
	SGrowBuf input = { NULL, 0, 0 };
	for ( ;; )
	{
		XMLCH rgubChunk[ 8192 ];
		int nActual = 0;
		int bEOF = inputSrc( rgubChunk, (int)sizeof( rgubChunk ), &nActual, inputData );

		if ( nActual > 0 )
			GrowBufAppend( &input, rgubChunk, nActual );

		if ( bEOF || nActual <= 0 )
			break;

		if ( input.nLen > ( 64 * 1024 * 1024 ) )
		{
			GrowBufFree( &input );
			pParser->ErrorCode = ERR_XMLP_READER_FATAL;
			strncpy( (char *)pParser->ErrorString, "XML input stream too large", sizeof( pParser->ErrorString ) - 1 );
			pParser->ErrorString[ sizeof( pParser->ErrorString ) - 1 ] = 0;
			if ( pParser->errorHandler )
				pParser->errorHandler( pParser );
			return XML_ABORT;
		}
	}

	SParseState state;
	memset( &state, 0, sizeof( state ) );
	state.pParser = pParser;
	state.pIn = input.pData;
	state.nInLen = input.nLen;
	state.nPos = 0;
	state.nLine = 1;
	state.nCol = 1;

	// skip a UTF-8 BOM
	if ( state.nInLen >= 3 && state.pIn[ 0 ] == 0xEF && state.pIn[ 1 ] == 0xBB && state.pIn[ 2 ] == 0xBF )
		state.nPos = 3;

	// the callbacks reach this state through parser->reader
	pParser->reader = (LPBUFFEREDISTREAM)&state;

	pParser->ErrorCode = 0;
	pParser->ErrorLine = 0;
	pParser->ErrorColumn = 0;
	pParser->ErrorString[ 0 ] = 0;

	if ( pParser->startDocumentHandler && pParser->startDocumentHandler( pParser->UserData ) != XML_OK )
		SetParserError( &state, ERR_XMLP_ABORT, "Aborted by document handler" );

	while ( !state.bFailed && !AtEnd( &state ) )
	{
		if ( state.pIn[ state.nPos ] != '<' )
		{
			int nStart = state.nPos;
			while ( !AtEnd( &state ) && state.pIn[ state.nPos ] != '<' )
				AdvanceChars( &state, 1 );

			if ( state.bRootClosed )
			{
				// trailing text after the root element: only whitespace is legal
				bool bAllWhitespace = true;
				for ( int i = nStart; i < state.nPos; ++i )
				{
					if ( !IsWhiteSpaceChar( state.pIn[ i ] ) )
					{
						bAllWhitespace = false;
						break;
					}
				}
				if ( !bAllWhitespace )
				{
					SetParserError( &state, ERR_XMLP_INVALID_AT_TOP, "Unexpected text after the root element at line %d, column %d", state.nLine, state.nCol );
					break;
				}
			}

			EmitText( &state, state.pIn + nStart, state.nPos - nStart );
			continue;
		}

		if ( Matches( &state, "<!--" ) )
		{
			AdvanceChars( &state, 4 );
			SkipToTerminator( &state, "-->" );
			continue;
		}

		if ( Matches( &state, "<![CDATA[" ) )
		{
			ParseCDATA( &state );
			continue;
		}

		if ( Matches( &state, "<?" ) )
		{
			AdvanceChars( &state, 2 );
			SkipToTerminator( &state, "?>" );
			continue;
		}

		if ( Matches( &state, "<!" ) )
		{
			SkipDeclaration( &state );
			continue;
		}

		if ( Matches( &state, "</" ) )
		{
			ParseEndTag( &state );
			continue;
		}

		ParseStartTag( &state );
	}

	if ( !state.bFailed && state.nElementCount != 0 )
	{
		SetParserError( &state, ERR_XMLP_UNCLOSED_TAG, "Unexpected end of file: <%s> was not closed", (const char *)TopElement( &state ) );
	}

	if ( !state.bFailed && !state.bSawRoot )
		SetParserError( &state, ERR_XMLP_INVALID_AT_TOP, "No root element found" );

	if ( !state.bFailed && pParser->endDocumentHandler && pParser->endDocumentHandler( pParser->UserData ) != XML_OK )
		SetParserError( &state, ERR_XMLP_ABORT, "Aborted by document handler" );

	pParser->reader = NULL;

	FreeElementStack( &state );
	GrowBufFree( &input );

	return state.bFailed ? pParser->ErrorCode : XML_OK;
}

int XMLParser_GetCurrentLine( LPXMLPARSER pParser )
{
	if ( !pParser || !pParser->reader )
		return 0;

	return ( (SParseState *)pParser->reader )->nLine;
}

int XMLParser_GetCurrentColumn( LPXMLPARSER pParser )
{
	if ( !pParser || !pParser->reader )
		return 0;

	return ( (SParseState *)pParser->reader )->nCol;
}

void *XMLVector_Get( LPXMLVECTOR pVector, int nIndex )
{
	if ( !pVector || nIndex < 0 || nIndex >= pVector->length )
		return NULL;

	return pVector->items[ nIndex ];
}

XMLCH *XMLParser_GetVersionString( void )
{
	return (XMLCH *)"0.8.3-seport";
}

int XMLNormalizeBuf( XMLCH *pBuf, int nLen )
{
	if ( !pBuf )
		return 0;

	int nWrite = 0;
	for ( int nRead = 0; nRead < nLen; ++nRead )
	{
		if ( pBuf[ nRead ] == '\r' )
		{
			pBuf[ nWrite++ ] = '\n';
			if ( nRead + 1 < nLen && pBuf[ nRead + 1 ] == '\n' )
				++nRead;
		}
		else
		{
			pBuf[ nWrite++ ] = pBuf[ nRead ];
		}
	}

	if ( nWrite < nLen )
		pBuf[ nWrite ] = 0;

	return nWrite;
}

// not implemented by this port - kept so that callers still link
LPXMLRUNTIMEATT XMLParser_GetNamedItem( LPXMLPARSER pParser, const XMLCH *pchName )
{
	(void)pParser;
	(void)pchName;
	return NULL;
}

XMLCH *XMLParser_GetSystemID( LPXMLPARSER pParser )
{
	(void)pParser;
	return NULL;
}

XMLCH *XMLParser_GetPublicID( LPXMLPARSER pParser )
{
	(void)pParser;
	return NULL;
}

XMLCH *XMLParser_GetPrefixMapping( LPXMLPARSER pParser, const XMLCH *pchPrefix )
{
	(void)pParser;
	(void)pchPrefix;
	return NULL;
}

LPXMLENTITY XMLParser_GetCurrentEntity( LPXMLPARSER pParser )
{
	(void)pParser;
	return NULL;
}
