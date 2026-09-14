# libparsifal-0.8.3 (SE port)

Self-contained replacement for the **Parsifal XML Parser** that CS:GO's panorama
layout loader is written against
(`panorama/layout/layoutfile.cpp` includes
`thirdparty/libparsifal-0.8.3/include/libparsifal/parsifal.h`).

## Why not the real thing

The upstream tarball (`libparsifal-0.8.3.tar.gz`, Toni Uusitalo, public domain)
is **no longer obtainable**:

| source | result |
| --- | --- |
| `saunalahti.fi/~samiuus/toni/xmlproc/` (author's site, cited by the RPM spec) | dead |
| `downloads.sourceforge.net/project/libparsifal/...` + mirror hosts | returns the project HTML page |
| netbsd/freebsd distfile mirrors | 404 |
| web.archive.org | unreachable |
| `nillerusr/source-engine` `thirdparty/libparsifal-0.8.3` | does not exist (the name only appears in `devtools/bin/fixcopyrights.py`'s list) |

Only the *headers* survived (CS:GO `public/parsifal/*.h`), so this port ships a
fresh implementation of the subset the panorama code actually uses.

## Layout

```
include/libparsifal/parsifal.h   API-compatible header (self-contained)
src/parsifal.cpp                 the reader (streamed input, UTF-8)
```

## Used API surface

```
XMLParser_Create / XMLParser_Parse / XMLParser_Free
XMLParser_GetCurrentLine / XMLParser_GetCurrentColumn
XMLVector_Get
handlers: startElementHandler endElementHandler charactersHandler
          ignorableWhitespaceHandler startCDATAHandler errorHandler
XML_OK / XML_ABORT
```

## Behaviour notes

* well-formed, **non-validating**; no DTD validation, no namespaces
  (`uri` is always `NULL`, `localName`/`prefix` are derived by splitting `qname`
  on `:`), no external entities
* predefined entities plus `&#nnn;` / `&#xHH;` are decoded in text and
  attribute values
* **undefined entities (`&foo;`) are passed through literally** — upstream
  reports `ERR_XMLP_UNDEF_ENTITY`, but being lenient cannot break content that
  was valid for upstream
* CDATA sections, comments and processing instructions are handled; `<!DOCTYPE ...>`
  (including an internal `[ ... ]` subset) is skipped
* the whole stream is read into memory first (layout files are small)
* per-parser state is stashed in `parser->reader` while parsing so the
  `XMLParser_GetCurrent*` accessors work from inside handler callbacks
