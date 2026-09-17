//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: SE port - the two GameUI helpers that the ported cstrike15 code uses.
//
//          In CS:GO these live in game/client/cstrike15/gameui/gameui_util.{h,cpp}.  This port has
//          the Source 2013 gameui module (gameui/gameui_util.cpp) instead, which does not provide
//          them and is a different module anyway (gameui.dll), so the implementations are repeated
//          here for the ported UI component code (batch E).
//
//          This header deliberately shadows gameui/gameui_util.h for the TUs of this target: the
//          ported code includes it as a bare "gameui_util.h", and ../panorama/seport/gameclient
//          comes before .. on the include path.
//
//=============================================================================//

#ifndef SE_PORT_GAMEUI_UTIL_H
#define SE_PORT_GAMEUI_UTIL_H
#pragma once

// Copies "oldName" into "newName", skipping leading '#' (to avoid loc-tag conflicts) and HTML-encoding
// what is left.  Used when a player name has to be shown as-is (uicomponent_uitoolkit.cpp).
void GameUI_MakeStringSafe( const wchar_t *oldName, wchar_t *newName, int destBufferSize );

#endif // SE_PORT_GAMEUI_UTIL_H
