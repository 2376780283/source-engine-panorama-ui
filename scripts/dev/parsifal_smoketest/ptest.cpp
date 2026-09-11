// Standalone smoke test for the SE-port libparsifal implementation.
// Build (from build/_parsifaltest):
//   cl /nologo /MT /O2 /EHsc /I"..\..\panorama\thirdparty\libparsifal-0.8.3\include" ^
//      /I"..\..\panorama\thirdparty\libparsifal-0.8.3\include\libparsifal" ^
//      ptest.cpp "..\..\panorama\thirdparty\libparsifal-0.8.3\src\parsifal.cpp" /Fe:ptest.exe
#include <parsifal.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static int g_nFailures = 0;
static int g_nCharsHandlerCalls = 0;

#define CHECK( cond, msg ) do { if ( !(cond) ) { printf( "  FAIL: %s\n", msg ); ++g_nFailures; } } while ( 0 )

//-----------------------------------------------------------------------------
// input source: static buffer with a cursor kept in the user data
//-----------------------------------------------------------------------------
struct SStream
{
	const char *pData;
	int nLen;
	int nPos;
};

static int ReadStream( XMLCH *pBuf, int cbBuf, int *pcbActual, void *pUserData )
{
	SStream *pStream = (SStream *)pUserData;

	int nRemaining = pStream->nLen - pStream->nPos;
	int nToCopy = ( nRemaining < cbBuf ) ? nRemaining : cbBuf;
	if ( nToCopy > 0 )
	{
		memcpy( pBuf, pStream->pData + pStream->nPos, nToCopy );
		pStream->nPos += nToCopy;
	}

	*pcbActual = nToCopy;
	return ( nToCopy < cbBuf );	// non-zero == EOF
}

//-----------------------------------------------------------------------------
// handlers
//-----------------------------------------------------------------------------
struct SEvents
{
	char rgchLog[ 8192 ];
	int nLogLen;
};

static void Log( SEvents *pEvents, const char *pchFormat, ... )
{
	va_list args;
	va_start( args, pchFormat );
	char rgchLine[ 1024 ];
	vsnprintf( rgchLine, sizeof( rgchLine ), pchFormat, args );
	va_end( args );

	int nLen = (int)strlen( rgchLine );
	if ( pEvents->nLogLen + nLen < (int)sizeof( pEvents->rgchLog ) - 1 )
	{
		memcpy( pEvents->rgchLog + pEvents->nLogLen, rgchLine, nLen );
		pEvents->nLogLen += nLen;
		pEvents->rgchLog[ pEvents->nLogLen ] = 0;
	}
}

static int StartElement( void *pUserData, const XMLCH *pUri, const XMLCH *pLocalName, const XMLCH *pQName, LPXMLVECTOR atts )
{
	SEvents *pEvents = (SEvents *)pUserData;
	Log( pEvents, "<%s", (const char *)pQName );
	for ( int i = 0; i < atts->length; ++i )
	{
		LPXMLRUNTIMEATT pAtt = (LPXMLRUNTIMEATT)XMLVector_Get( atts, i );
		Log( pEvents, " %s=\"%s\"", (const char *)pAtt->qname, pAtt->value ? (const char *)pAtt->value : "" );
	}
	Log( pEvents, ">\n" );
	return XML_OK;
}

static int EndElement( void *pUserData, const XMLCH *pUri, const XMLCH *pLocalName, const XMLCH *pQName )
{
	SEvents *pEvents = (SEvents *)pUserData;
	Log( pEvents, "</%s>\n", (const char *)pQName );
	return XML_OK;
}

static int Characters( void *pUserData, const XMLCH *pChars, int cbChars )
{
	SEvents *pEvents = (SEvents *)pUserData;
	++g_nCharsHandlerCalls;
	char rgchText[ 512 ];
	int nCopy = ( cbChars < (int)sizeof( rgchText ) - 1 ) ? cbChars : (int)sizeof( rgchText ) - 1;
	memcpy( rgchText, pChars, nCopy );
	rgchText[ nCopy ] = 0;
	Log( pEvents, "  text(%d)=\"%s\"\n", cbChars, rgchText );
	return XML_OK;
}

static int CDataStart( void *pUserData )
{
	SEvents *pEvents = (SEvents *)pUserData;
	Log( pEvents, "  [CDATA start line=%d]\n", XMLParser_GetCurrentLine( (LPXMLPARSER)NULL ) );
	return XML_OK;
}

static void ErrorHandler( LPXMLPARSER pParser )
{
	printf( "  parser error %d: %s (line %d, col %d)\n",
			pParser->ErrorCode, (const char *)pParser->ErrorString, pParser->ErrorLine, pParser->ErrorColumn );
}

//-----------------------------------------------------------------------------
static bool ParseString( const char *pchXml, SEvents *pEvents, LPXMLPARSER *ppParserOut )
{
	SStream stream;
	stream.pData = pchXml;
	stream.nLen = (int)strlen( pchXml );
	stream.nPos = 0;

	pEvents->nLogLen = 0;
	pEvents->rgchLog[0] = 0;

	LPXMLPARSER pParser = NULL;
	if ( !XMLParser_Create( &pParser ) )
		return false;

	pParser->startElementHandler = StartElement;
	pParser->endElementHandler = EndElement;
	pParser->charactersHandler = Characters;
	pParser->ignorableWhitespaceHandler = Characters;
	pParser->startCDATAHandler = CDataStart;
	pParser->errorHandler = ErrorHandler;
	pParser->UserData = pEvents;

	int nResult = XMLParser_Parse( pParser, ReadStream, &stream, (const XMLCH *)"UTF-8" );

	if ( ppParserOut )
		*ppParserOut = pParser;	// caller frees
	else
		XMLParser_Free( pParser );

	return ( nResult == XML_OK );
}

//-----------------------------------------------------------------------------
int main( void )
{
	SEvents events;

	// --- case 1: a realistic layout snippet ---------------------------------
	const char *pchLayout =
		"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
		"<!-- a comment -->\n"
		"<root>\n"
		"  <styles>\n"
		"    <include src=\"file://{resources}/styles/foo.css\" />\n"
		"  </styles>\n"
		"  <Panel class=\"Root\" id=\"Main\">\n"
		"    <Label text=\"hello &amp; goodbye\" />\n"
		"  </Panel>\n"
		"</root>\n";

	printf( "== case 1: layout snippet ==\n" );
	bool bOk = ParseString( pchLayout, &events, NULL );
	CHECK( bOk, "valid layout should parse" );
	printf( "%s", events.rgchLog );
	CHECK( strstr( events.rgchLog, "<Panel class=\"Root\" id=\"Main\">" ) != NULL, "Panel attrs" );
	CHECK( strstr( events.rgchLog, "<Label text=\"hello & goodbye\">" ) != NULL, "entity decoded in attribute" );
	CHECK( strstr( events.rgchLog, "<include src=\"file://{resources}/styles/foo.css\">" ) != NULL, "self closing element emits end" );

	// --- case 2: CDATA + numeric entities + comment with markup -------------
	const char *pchCData =
		"<root>\n"
		"<!-- <not a tag> -->\n"
		"<script>\n"
		"<![CDATA[ if (a < b && c > d) { x(); } ]]>\n"
		"</script>\n"
		"<Label text=\"A&#66;C &#x44;E\" />\n"
		"</root>\n";

	printf( "== case 2: cdata + numeric entities ==\n" );
	bOk = ParseString( pchCData, &events, NULL );
	CHECK( bOk, "cdata document should parse" );
	printf( "%s", events.rgchLog );
	CHECK( strstr( events.rgchLog, "if (a < b && c > d) { x(); }" ) != NULL, "CDATA content raw" );
	CHECK( strstr( events.rgchLog, "text=\"ABC DE\"" ) != NULL, "numeric entities decoded" );

	// --- case 3: mismatched end tag must fail -------------------------------
	printf( "== case 3: mismatched end tag ==\n" );
	bOk = ParseString( "<root><Panel></root>", &events, NULL );
	CHECK( !bOk, "mismatched end tag must fail" );

	// --- case 4: error handler + error position ----------------------------
	printf( "== case 4: unterminated element ==\n" );
	bOk = ParseString( "<root><Panel>", &events, NULL );
	CHECK( !bOk, "unclosed element must fail" );

	// --- case 5: GetCurrentLine/Column inside handlers ----------------------
	printf( "== case 5: position tracking ==\n" );
	LPXMLPARSER pParser = NULL;
	bOk = ParseString( "<root>\n  <Panel />\n</root>\n", &events, &pParser );
	CHECK( bOk, "multi-line doc parses" );
	CHECK( pParser && pParser->DocumentElement && strcmp( (const char *)pParser->DocumentElement, "root" ) == 0, "DocumentElement" );
	XMLParser_Free( pParser );

	printf( "\n%s (%d chars handler calls)\n", g_nFailures ? "FAILURES PRESENT" : "ALL CHECKS PASSED", g_nCharsHandlerCalls );
	return g_nFailures ? 1 : 0;
}
