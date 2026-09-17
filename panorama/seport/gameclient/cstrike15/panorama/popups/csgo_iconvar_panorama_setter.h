//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose:
//=============================================================================//
#pragma once

// SE port: CS:GO's game/client/cstrike15/panorama/popups/csgo_iconvar_panorama_setter.h, unchanged
// except for the include below.  CS:GO reaches ConVarRef through its cbase.h chain; the ported
// game-client sources include panorama/se_gameclient_common.h instead, and this header is also
// reached from csgo_settings_enum.h / csgo_settings_slider.h, so the dependency is spelled out here.
#include "convar.h"

class CCSGO_iConvarPanoramaSetter
{
public:

	void RestoreDefault()
	{
		ConVarRef &ref = GetConVarRef();
		if ( ref.IsValid() )
		{
			ref.SetValue( ref.GetDefault() );
		}
	}

private:
	virtual ConVarRef& GetConVarRef() = 0;
};
