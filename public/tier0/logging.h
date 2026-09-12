//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: Minimal Source2-style logging shim for the Panorama port.
//          This engine (SE) has no Source2 logging subsystem. Upstream dbg.h
//          defines placeholder Log_* macros that reference a non-existent
//          ::Log(); we redefine them here to something self-contained that
//          compiles and routes to the engine console (Msg/Warning/Error).
//
//=============================================================================//
#ifndef TIER0_LOGGING_H
#define TIER0_LOGGING_H
#ifdef _WIN32
#pragma once
#endif

#include "tier0/platform.h"
#include "tier0/dbg.h"

// Undo upstream dbg.h placeholders (they call a non-existent ::Log()).
#undef Log_Msg
#undef Log_Warning
#undef Log_Error
#undef Log_Detailed
#undef DEFINE_LOGGING_CHANNEL_NO_TAGS

// Channels are declared via DECLARE_LOGGING_CHANNEL (values are unused - they
// are consumed as the ignored "channel" macro argument).
#ifndef DECLARE_LOGGING_CHANNEL
#define DECLARE_LOGGING_CHANNEL( name ) enum { name };
#endif
#define DEFINE_LOGGING_CHANNEL_NO_TAGS( name, ... )

// CS:GO code declares channel variables of this type (e.g. s1wrapper/wrap_misc.cpp)
// and passes them to Log_Msg/Log_Warning, which ignore the channel argument here.
typedef int LoggingChannelID_t;
const LoggingChannelID_t INVALID_LOGGING_CHANNEL_ID = -1;

// CS:GO's logging API is Log_Msg( ChannelID, [Color], Format, ... ).  Source 2013 has no logging
// subsystem, so the channel is ignored, an optional Color argument (the form the IME text-entry
// code uses) is dropped, and the rest is forwarded to the engine console via MsgV/WarningV/ErrorV.
// The colour overloads are templates so this header does not have to pull in Color.h.
inline void PanoramaLog_Msg( const char *pFmt, ... )
{
	va_list args;
	va_start( args, pFmt );
	MsgV( pFmt, args );
	va_end( args );
}

template < typename TCOLOR >
inline void PanoramaLog_Msg( const TCOLOR & /*color*/, const char *pFmt, ... )
{
	va_list args;
	va_start( args, pFmt );
	MsgV( pFmt, args );
	va_end( args );
}

inline void PanoramaLog_Warning( const char *pFmt, ... )
{
	va_list args;
	va_start( args, pFmt );
	WarningV( pFmt, args );
	va_end( args );
}

template < typename TCOLOR >
inline void PanoramaLog_Warning( const TCOLOR & /*color*/, const char *pFmt, ... )
{
	va_list args;
	va_start( args, pFmt );
	WarningV( pFmt, args );
	va_end( args );
}

inline void PanoramaLog_Error( const char *pFmt, ... )
{
	va_list args;
	va_start( args, pFmt );
	ErrorV( pFmt, args );
	va_end( args );
}

template < typename TCOLOR >
inline void PanoramaLog_Error( const TCOLOR & /*color*/, const char *pFmt, ... )
{
	va_list args;
	va_start( args, pFmt );
	ErrorV( pFmt, args );
	va_end( args );
}

#define Log_Msg( ch, ... )       PanoramaLog_Msg( __VA_ARGS__ )
#define Log_Detailed( ch, ... )  PanoramaLog_Msg( __VA_ARGS__ )
#define Log_Warning( ch, ... )   PanoramaLog_Warning( __VA_ARGS__ )
#define Log_Error( ch, ... )     PanoramaLog_Error( __VA_ARGS__ )

// Channel used by the CS:GO IME / text entry code.
DECLARE_LOGGING_CHANNEL( LOG_GENERAL );

#endif // TIER0_LOGGING_H
