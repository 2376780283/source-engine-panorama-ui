//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: SE port - a stand-in for Source 2's SplitScreenConVarRef.
//
//          CS:GO's csgo_audiosettingsscreen.cpp configures the audio cvars through
//          SplitScreenConVarRef (a ConVarRef with a split-screen slot index on every accessor):
//
//              SplitScreenConVarRef dsp_enhance_stereo( "dsp_enhance_stereo" );
//              dsp_enhance_stereo.GetBool( 0 );
//              dsp_enhance_stereo.SetValue( 0, 0 );
//
//          Source 2013's convar.h has no such class, and this port has no split screen, so every
//          accessor here forwards to the plain ConVarRef and ignores the slot index (slot 0 is the
//          only slot CS:GO's settings screens touch).
//
//          Kept out of public/ - nothing else in this tree wants it.
//
//=============================================================================//

#ifndef SE_PORT_SPLIT_SCREEN_CONVAR_H
#define SE_PORT_SPLIT_SCREEN_CONVAR_H
#pragma once

#include "convar.h"

class SplitScreenConVarRef
{
public:
	SplitScreenConVarRef( const char *pName ) : m_ConVar( pName, true ) {}
	SplitScreenConVarRef( const char *pName, bool bIgnoreMissing ) : m_ConVar( pName, bIgnoreMissing ) {}

	bool IsValid() const { return m_ConVar.IsValid(); }
	const char *GetName() const { return m_ConVar.GetName(); }

	const char *GetString( int nSlot = 0 ) const { return m_ConVar.GetString(); }
	float GetFloat( int nSlot = 0 ) const { return m_ConVar.GetFloat(); }
	int GetInt( int nSlot = 0 ) const { return m_ConVar.GetInt(); }
	bool GetBool( int nSlot = 0 ) const { return m_ConVar.GetBool(); }

	void SetValue( const char *pValue, int nSlot = 0 ) { m_ConVar.SetValue( pValue ); }
	void SetValue( float flValue, int nSlot = 0 ) { m_ConVar.SetValue( flValue ); }
	void SetValue( int nValue, int nSlot = 0 ) { m_ConVar.SetValue( nValue ); }

private:
	ConVarRef m_ConVar;
};

#endif // SE_PORT_SPLIT_SCREEN_CONVAR_H
