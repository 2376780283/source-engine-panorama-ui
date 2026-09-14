//========= Copyright Valve Corporation, All rights reserved. ============//
//
// SE port: a minimal stand-in for PCRE's C++ wrapper (pcrecpp), implemented on top of <regex>.
//
// panorama/controls/debug/debuglayout.cpp includes both "pcre/pcrecpp.h" and <pcrecpp.h> and uses
// exactly three things from them:
//
//     static pcrecpp::RE_Options reOptions( PCRE_CASELESS );
//     static pcrecpp::RE reWWWLong( "((https?|ftp|file)://|www\\.)([^<]*)", reOptions );
//     reWWWLong.GlobalReplace( "<a href=\"\\1\\3\">\\1\\3</a>", &strChat );
//
// CS:GO links the prebuilt pcre/pcrecpp libraries from lib/common, which do not exist in this tree
// (and neither do the PCRE sources), so this header provides that subset.  The pattern syntax used
// above - and PCRE's \1..\9 back-references in the replacement - are translated to std::regex and
// expanded by hand.  Only the members actually referenced are implemented; add to this file (rather
// than editing the panorama source) if more of the pcrecpp surface is ever needed.
//
// ============//
#ifndef SE_PCRECPP_SHIM_H
#define SE_PCRECPP_SHIM_H

#include <string>
#include <regex>

// From pcre.h (the real library's option bits) - only the flag panorama passes is needed.
#ifndef PCRE_CASELESS
#define PCRE_CASELESS 0x00000001
#endif

namespace pcrecpp {

//-------------------------------------------------------------------------
// Purpose: option carrier matching the pcrecpp::RE_Options constructor used by panel code
//-------------------------------------------------------------------------
class RE_Options
{
public:
	RE_Options() : m_bCaseless( false ) {}
	RE_Options( int nFlags ) : m_bCaseless( ( nFlags & PCRE_CASELESS ) != 0 ) {}

	void set_caseless( bool bCaseless ) { m_bCaseless = bCaseless; }
	bool caseless() const { return m_bCaseless; }

private:
	bool m_bCaseless;
};


//-------------------------------------------------------------------------
// Purpose: the subset of pcrecpp::RE that the panorama debug layout pane uses
//-------------------------------------------------------------------------
class RE
{
public:
	RE( const char *pchPattern, const RE_Options &options = RE_Options() )
		: m_regex( pchPattern ? pchPattern : "", options.caseless() ? std::regex::icase : std::regex::ECMAScript )
	{
	}

	// Replaces every match of the pattern in *pstrTarget.  The replacement template uses PCRE's
	// \1..\9 (and \0) back-references; "\\" is an escaped backslash, as in PCRE.
	bool GlobalReplace( const char *pchReplacement, std::string *pstrTarget ) const
	{
		if ( !pstrTarget )
			return false;

		const std::string &strTarget = *pstrTarget;
		std::string strOut;
		strOut.reserve( strTarget.size() );

		std::string::const_iterator itStart = strTarget.begin();
		for ( std::sregex_iterator it( strTarget.begin(), strTarget.end(), m_regex ), itEnd; it != itEnd; ++it )
		{
			const std::smatch &match = *it;
			strOut.append( itStart, strTarget.begin() + match.position() );
			AppendExpanded( strOut, pchReplacement, match );
			itStart = strTarget.begin() + match.position() + match.length();
		}
		strOut.append( itStart, strTarget.end() );

		*pstrTarget = strOut;
		return true;
	}

	// True if the whole string matches the pattern (pcrecpp::RE::FullMatch).
	bool FullMatch( const std::string &strTarget ) const
	{
		return std::regex_match( strTarget, m_regex );
	}

	// True if any part of the string matches the pattern (pcrecpp::RE::PartialMatch).
	bool PartialMatch( const std::string &strTarget ) const
	{
		return std::regex_search( strTarget, m_regex );
	}

private:
	// Expands a PCRE-style replacement template against one match.
	static void AppendExpanded( std::string &strOut, const char *pchReplacement, const std::smatch &match )
	{
		for ( const char *p = pchReplacement; p && *p; ++p )
		{
			if ( *p != '\\' || p[1] == 0 )
			{
				strOut.push_back( *p );
				continue;
			}

			++p;
			if ( *p >= '0' && *p <= '9' )
			{
				const size_t nGroup = (size_t)( *p - '0' );
				if ( nGroup < match.size() && match[ nGroup ].matched )
				{
					strOut.append( match[ nGroup ].first, match[ nGroup ].second );
				}
			}
			else
			{
				// \\ and any other escaped character stand for themselves.
				strOut.push_back( *p );
			}
		}
	}

	std::regex m_regex;
};

} // namespace pcrecpp

#endif // SE_PCRECPP_SHIM_H
