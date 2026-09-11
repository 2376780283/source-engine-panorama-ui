/*===========================================================================
  libparsifal-0.8.3 - SE port (Source 2013 / CS:GO Panorama)

  API-compatible, self-contained replacement for the "Parsifal XML Parser"
  (Copyright (c) 2002-2004 Toni Uusitalo, public domain) that the CS:GO
  panorama layout loader (panorama/layout/layoutfile.cpp) is written against.

  WHY A REWRITE: the upstream tarball (libparsifal-0.8.3.tar.gz) is no longer
  obtainable - the author's site (saunalahti.fi/~samiuus/toni/xmlproc/) is gone
  and SourceForge/mirrors/Web-Archive all fail - and only the public headers
  survived in the CS:GO tree (public/parsifal).  This header therefore declares
  just the subset the panorama code uses, and src/parsifal.cpp implements it.

  Implementation differences to be aware of:
    * undefined entities (&foo;) are passed through literally instead of being
      reported as ERR_XMLP_UNDEF_ENTITY
    * no DTD/namespace processing: uri is always NULL, localName/prefix are
      derived by splitting qname on ':'
===========================================================================*/
#ifndef PARSIFAL__H
#define PARSIFAL__H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef XMLCH_DEFINED
#define XMLCH_DEFINED
typedef unsigned char XMLCH;
#endif

#define XML_OK    0
#define XML_ABORT 1

/* sax feature flags (kept for source compatibility) */
#define XMLFLAG_NAMESPACES 0x1
#define XMLFLAG_NAMESPACE_PREFIXES 0x2
#define XMLFLAG_EXTERNAL_GENERAL_ENTITIES 0x4
#define XMLFLAG_PRESERVE_GENERAL_ENTITIES 0x8
#define XMLFLAG_UNDEF_GENERAL_ENTITIES 0x10
#define XMLFLAG_PRESERVE_WS_ATTRIBUTES 0x20
#define XMLFLAG_CONVERT_EOL 0x40

enum tagXMLERRCODE {
	ERR_XMLP_MEMORY_ALLOC = 1,
	ERR_XMLP_READER_FATAL,
	ERR_XMLP_INVALID_TOKEN,
	ERR_XMLP_INVALID_NAME,
	ERR_XMLP_INVALID_END_TAG,
	ERR_XMLP_UNDEF_ENTITY,
	ERR_XMLP_WS_NOT_ALLOWED,
	ERR_XMLP_WS_REQUIRED,
	ERR_XMLP_UNCLOSED_TAG,
	ERR_XMLP_EXPECTED_FOUND,
	ERR_XMLP_EXPECTED_TOKEN,
	ERR_XMLP_MULTIPLE_TOP,
	ERR_XMLP_INVALID_AT_TOP,
	ERR_XMLP_UNDEF_NSPREFIX,
	ERR_XMLP_DUPL_ATTRIBUTE,
	ERR_XMLP_ENCODING,
	ERR_XMLP_UNSUP_ENCODING,
	ERR_XMLP_INVALID_DECL,
	ERR_XMLP_INVALID_ATT_VALUE,
	ERR_XMLP_ABORT,
	ERR_XMLP_ILLEGAL_CHAR,
	ERR_XMLP_RECURSIVE_ENTITY_REF,
	ERR_XMLP_IO,
	ERR_XMLP_SWITCH_ENCODING
};
typedef enum tagXMLERRCODE XMLERRCODE;

enum tagXMLENTITYTYPE {
	XML_ENTITY_INT_PARAM = 1,
	XML_ENTITY_INT_GEN,
	XML_ENTITY_EXT_PARAM,
	XML_ENTITY_EXT_GEN,
	XML_ENTITY_UNPARSED,
	XML_ENTITY_DOCTYPE
};
typedef enum tagXMLENTITYTYPE XMLENTITYTYPE;

typedef struct tagXMLENTITY
{
	XMLENTITYTYPE type;
	int len;
	int open;
	XMLCH *name;
	XMLCH *value;
	XMLCH *publicID;
	XMLCH *systemID;
	XMLCH *notation;
} XMLENTITY, *LPXMLENTITY;

/* upstream types we do not need the layout of (never dereferenced here) */
typedef struct tagXMLPARSERRUNTIME XMLPARSERRUNTIME, *LPXMLPARSERRUNTIME;
typedef struct tagXMLBUFFEREDISTREAM XMLBUFFEREDISTREAM, *LPBUFFEREDISTREAM;
struct tagXMLPARSER;

/* string buffer (upstream xmlsbuf.h) - only the fields the macros touch */
typedef struct tagXMLSTRINGBUF
{
	XMLCH *str;
	int len;
	int alloc;
} XMLSTRINGBUF, *LPXMLSTRINGBUF;

/* generic pointer vector (upstream xmlvect.h) */
typedef struct tagXMLVECTOR
{
	int length;
	int size;
	void **items;
} XMLVECTOR, *LPXMLVECTOR;

/* runtime attribute (upstream xmldef.h) */
typedef struct tagXMLRUNTIMEATT
{
	XMLCH *qname;
	XMLCH *value;
	XMLCH *uri;
	XMLCH *localName;
	XMLCH *prefix;
	XMLSTRINGBUF nameBuf;
	XMLSTRINGBUF valBuf;
} XMLRUNTIMEATT, *LPXMLRUNTIMEATT;

/* handlers */
typedef int (*XML_EVENT_HANDLER)(void *UserData);
typedef int (*XML_START_ELEMENT_HANDLER)(void *UserData, const XMLCH *uri,
										const XMLCH *localName, const XMLCH *qName,
										LPXMLVECTOR atts);
typedef int (*XML_END_ELEMENT_HANDLER)(void *UserData, const XMLCH *uri,
									  const XMLCH *localName, const XMLCH *qName);
typedef int (*XML_CHARACTERS_HANDLER)(void *UserData, const XMLCH *chars, int cbSize);
typedef int (*XML_PI_HANDLER)(void *UserData, const XMLCH *target, const XMLCH *data);
typedef int (*XML_START_DTD_HANDLER)(void *UserData, const XMLCH *name,
									const XMLCH *publicId, const XMLCH *systemId,
									int hasInternalSubset);
typedef int (*XML_XMLDECL_HANDLER)(void *UserData, const XMLCH *version,
								  const XMLCH *encoding, const XMLCH *standalone);
typedef int (*XML_RESOLVE_ENTITY_HANDLER)(void *UserData, LPXMLENTITY entity,
										 LPBUFFEREDISTREAM reader);
typedef int (*XML_SKIPPED_ENTITY_HANDLER)(void *UserData, const XMLCH *name);
typedef int (*XML_ENTITY_EVENT_HANDLER)(void *UserData, LPXMLENTITY entity);
typedef int (*XML_ATTRIBUTEDECL_HANDLER)(void *UserData, const XMLCH *eName,
	const XMLCH *aName, int type, const XMLCH *typeStr, int valueDef,
	const XMLCH *def);
typedef int (*XML_ELEMENTDECL_HANDLER)(void *UserData, const XMLCH *name,
	void *contentModel);
typedef int (*XML_NOTATIONDECL_HANDLER)(void *UserData, const XMLCH *name,
	const XMLCH *publicID, const XMLCH *systemID);

/* input source: fill buf with up to cBytes bytes, set *cBytesActual,
   return non-zero when the stream is exhausted (short read) */
typedef int (*LPFNINPUTSRC)(XMLCH *buf, int cBytes, int *cBytesActual, void *inputData);

typedef struct tagXMLPARSER
{
	LPBUFFEREDISTREAM reader;
	LPXMLPARSERRUNTIME prt;
	XMLCH *DocumentElement;
	XMLCH ErrorString[128];
	int ErrorCode;
	int ErrorLine;
	int ErrorColumn;
	void *UserData;
	unsigned long XMLFlags;

	XML_EVENT_HANDLER startDocumentHandler;
	XML_EVENT_HANDLER endDocumentHandler;
	XML_EVENT_HANDLER startCDATAHandler;
	XML_EVENT_HANDLER endCDATAHandler;
	XML_EVENT_HANDLER endDTDHandler;
	XML_CHARACTERS_HANDLER charactersHandler;
	XML_CHARACTERS_HANDLER ignorableWhitespaceHandler;
	XML_CHARACTERS_HANDLER commentHandler;
	XML_CHARACTERS_HANDLER defaultHandler;
	XML_START_ELEMENT_HANDLER startElementHandler;
	XML_END_ELEMENT_HANDLER endElementHandler;
	XML_PI_HANDLER processingInstructionHandler;
	XML_START_DTD_HANDLER startDTDHandler;
	XML_XMLDECL_HANDLER xmlDeclHandler;
	XML_SKIPPED_ENTITY_HANDLER skippedEntityHandler;
	XML_ENTITY_EVENT_HANDLER startEntityHandler;
	XML_ENTITY_EVENT_HANDLER endEntityHandler;
	XML_RESOLVE_ENTITY_HANDLER resolveEntityHandler;
	XML_RESOLVE_ENTITY_HANDLER externalEntityParsedHandler;
	XML_ATTRIBUTEDECL_HANDLER attributeDeclHandler;
	XML_ELEMENTDECL_HANDLER elementDeclHandler;
	XML_ENTITY_EVENT_HANDLER entityDeclHandler;
	XML_NOTATIONDECL_HANDLER notationDeclHandler;
	void (*errorHandler)(struct tagXMLPARSER *parser);
} XMLPARSER, *LPXMLPARSER;

#ifndef XMLAPI
#define XMLAPI
#endif

LPXMLPARSER XMLAPI XMLParser_Create(LPXMLPARSER *parser);
int XMLAPI XMLParser_Parse(LPXMLPARSER parser, LPFNINPUTSRC inputSrc, void *inputData, const XMLCH *encoding);
void XMLAPI XMLParser_Free(LPXMLPARSER parser);
LPXMLRUNTIMEATT XMLAPI XMLParser_GetNamedItem(LPXMLPARSER parser, const XMLCH *name);
XMLCH XMLAPI *XMLParser_GetSystemID(LPXMLPARSER parser);
XMLCH XMLAPI *XMLParser_GetPublicID(LPXMLPARSER parser);
XMLCH XMLAPI *XMLParser_GetPrefixMapping(LPXMLPARSER parser, const XMLCH *prefix);
int XMLAPI XMLParser_GetCurrentLine(LPXMLPARSER parser);
int XMLAPI XMLParser_GetCurrentColumn(LPXMLPARSER parser);
LPXMLENTITY XMLAPI XMLParser_GetCurrentEntity(LPXMLPARSER parser);
XMLCH XMLAPI *XMLParser_GetVersionString(void);
int XMLNormalizeBuf(XMLCH *buf, int len);

/* upstream xmlvect.h accessor used by layoutfile.cpp */
void *XMLAPI XMLVector_Get(LPXMLVECTOR vector, int index);

#define _XMLParser_SetFlag(parser,flag,valBool) \
	((valBool) ? (((LPXMLPARSER)parser)->XMLFlags |= (flag)) : \
	  (((LPXMLPARSER)parser)->XMLFlags &= ~(flag)) )

#define _XMLParser_GetFlag(parser,flag) \
	((((LPXMLPARSER)parser)->XMLFlags & (flag)) == (flag))

#define _XMLParser_AttIsDefaulted(att) (!(att->nameBuf.str))

#ifdef __cplusplus
}
#endif

#endif /* PARSIFAL__H */
