//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: SE port - implementations of the game-client panel stand-ins, see the note at the top of
//          seport_panel_stubs.h.
//
//          Every class here is registered with the same type name the CS:GO layout loader and the
//          content scripts use, so the substitution path in layoutfile.cpp::BAddPanel() no longer
//          applies to them and their stylesheet rules (which select on the type) keep matching.
//
//=============================================================================//

#include "panorama/se_gameclient_common.h"
#include "seport/gameclient/seport_panel_stubs.h"

using namespace panorama;

//-----------------------------------------------------------------------------
// Purpose: helper - true when symName is one of the given game-client properties (these carry econ /
//          Steam data that this port has no source for; accepting them keeps the layout parse clean).
//-----------------------------------------------------------------------------
static bool SE_PortStubIsProperty( CPanoramaSymbol symName, const char *const *ppchNames, int nCount )
{
	for ( int i = 0; i < nCount; ++i )
	{
		if ( symName == CPanoramaSymbol( ppchNames[i] ) )
			return true;
	}

	return false;
}

// ============================================================================
// ItemPreviewPanel  (CS:GO: CUI_ItemPreviewPanel - 6861 lines + a 2859 line renderer)
// ============================================================================
REGISTER_PANEL2D_FACTORY( CSEPortStub_ItemPreviewPanel, ItemPreviewPanel )

CSEPortStub_ItemPreviewPanel::CSEPortStub_ItemPreviewPanel( CPanel2D *pParent, const char *pchID )
: BaseClass( pParent, pchID )
{
}

bool CSEPortStub_ItemPreviewPanel::BSetProperty( CPanoramaSymbol symName, const char *pchValue )
{
	// "item" / "manifest" select what to preview, the other two configure the preview scene.
	static const char *s_pchSwallowed[] = { "item", "manifest", "enable_floorshadow", "mouse_rotate" };
	if ( SE_PortStubIsProperty( symName, s_pchSwallowed, V_ARRAYSIZE( s_pchSwallowed ) ) )
		return true;

	return BaseClass::BSetProperty( symName, pchValue );
}

void CSEPortStub_ItemPreviewPanel::SetupJavascriptObjectTemplate()
{
	BaseClass::SetupJavascriptObjectTemplate();

	// The JS member names are CS:GO's (game/client/panorama/ui_itempreview_panel.cpp:365-412).  Every
	// one of them is a no-op here: there is no 3D scene to configure.  Registering the names is what
	// matters - an unknown member is a TypeError, and one TypeError aborts the rest of the calling
	// script (that is the "vanityPanel.SetSceneAngles is not a function" path in mainmenu.js).
	RegisterJSAccessor( "manifest", PANORAMA_DELEGATE( &CSEPortStub_ItemPreviewPanel::JSGetEmpty ), PANORAMA_DELEGATE( &CSEPortStub_ItemPreviewPanel::JSSetSwallow ) );
	RegisterJSAccessor( "item", PANORAMA_DELEGATE( &CSEPortStub_ItemPreviewPanel::JSGetEmpty ), PANORAMA_DELEGATE( &CSEPortStub_ItemPreviewPanel::JSSetSwallow ) );

#define SE_PORT_PREVIEW_NOOP( jsName )	RegisterJSMethod( jsName, PANORAMA_DELEGATE( &CSEPortStub_ItemPreviewPanel::JSNoop ) )

	SE_PORT_PREVIEW_NOOP( "SetScene" );
	SE_PORT_PREVIEW_NOOP( "SetSceneAngles" );
	SE_PORT_PREVIEW_NOOP( "SetSceneOffset" );		// used by controlslibrary.js / charlineup
	SE_PORT_PREVIEW_NOOP( "SetSceneRotation" );
	SE_PORT_PREVIEW_NOOP( "SetSceneIntroRotation" );
	SE_PORT_PREVIEW_NOOP( "SetSceneIntroFOV" );
	SE_PORT_PREVIEW_NOOP( "SetPlayerModel" );
	SE_PORT_PREVIEW_NOOP( "EquipPlayerFromLoadout" );
	SE_PORT_PREVIEW_NOOP( "EquipPlayerWithItem" );
	SE_PORT_PREVIEW_NOOP( "ResetAnimation" );
	SE_PORT_PREVIEW_NOOP( "QueueSequence" );
	SE_PORT_PREVIEW_NOOP( "LayerSequence" );
	SE_PORT_PREVIEW_NOOP( "SetPanelLightingAmount" );
	SE_PORT_PREVIEW_NOOP( "SetFlashlightAmount" );
	SE_PORT_PREVIEW_NOOP( "SetFlashlightPulseFlicker" );
	SE_PORT_PREVIEW_NOOP( "SetFlashlightRotation" );
	SE_PORT_PREVIEW_NOOP( "SetFlashlightColor" );
	SE_PORT_PREVIEW_NOOP( "SetFlashlightPosition" );
	SE_PORT_PREVIEW_NOOP( "SetFlashlightAngle" );
	SE_PORT_PREVIEW_NOOP( "SetFlashlightFOV" );
	SE_PORT_PREVIEW_NOOP( "SetFlashlightNearFarZ" );
	SE_PORT_PREVIEW_NOOP( "SetAmbientLightColor" );
	SE_PORT_PREVIEW_NOOP( "SetDirectionalLightModify" );
	SE_PORT_PREVIEW_NOOP( "SetDirectionalLightPulseFlicker" );
	SE_PORT_PREVIEW_NOOP( "SetDirectionalLightRotation" );
	SE_PORT_PREVIEW_NOOP( "SetDirectionalLightAmount" );
	SE_PORT_PREVIEW_NOOP( "SetDirectionalLightColor" );
	SE_PORT_PREVIEW_NOOP( "SetDirectionalLightDirection" );
	SE_PORT_PREVIEW_NOOP( "SetCameraPosition" );
	SE_PORT_PREVIEW_NOOP( "SetCameraAngles" );
	SE_PORT_PREVIEW_NOOP( "SetCameraPreset" );
	SE_PORT_PREVIEW_NOOP( "SetEconItemTextureSize" );
	SE_PORT_PREVIEW_NOOP( "SetFloatingFloorAlpha" );
	SE_PORT_PREVIEW_NOOP( "SetParticleSystemOffsetPosition" );
	SE_PORT_PREVIEW_NOOP( "SetParticleSystemOffsetAngles" );
	SE_PORT_PREVIEW_NOOP( "AddParticleSystem" );
	SE_PORT_PREVIEW_NOOP( "TogglePause" );
	SE_PORT_PREVIEW_NOOP( "Pause" );
	SE_PORT_PREVIEW_NOOP( "EnableRendering" );
	SE_PORT_PREVIEW_NOOP( "SetAsActivePreviewPanel" );
	SE_PORT_PREVIEW_NOOP( "Play" );
	SE_PORT_PREVIEW_NOOP( "Stop" );

	// Names the shipped content calls that CS:GO 2019's ui_itempreview_panel.cpp does not register
	// (the content is newer than that source drop - e.g. characteranims.js calls
	// SetPlayerCharacterItemID, which appears nowhere in D:\CSGO2019).  Registered for the same
	// reason as the rest: an unknown member is a TypeError that aborts the calling script.
	SE_PORT_PREVIEW_NOOP( "SetPlayerCharacterItemID" );
	SE_PORT_PREVIEW_NOOP( "SetSceneModel" );
	SE_PORT_PREVIEW_NOOP( "SetActiveSceneContext" );
	SE_PORT_PREVIEW_NOOP( "CreateSceneContexts" );
	SE_PORT_PREVIEW_NOOP( "ApplyActivityModifier" );
	SE_PORT_PREVIEW_NOOP( "ResetActivityModifiers" );
	SE_PORT_PREVIEW_NOOP( "PlayActivity" );
	SE_PORT_PREVIEW_NOOP( "PlaySequence" );
	SE_PORT_PREVIEW_NOOP( "RestoreLightingState" );

#undef SE_PORT_PREVIEW_NOOP
}

// ============================================================================
// ItemPreviewSlider / ItemPreviewColorSlider / ItemPreviewDebug
// ============================================================================
REGISTER_PANEL2D_FACTORY( CSEPortStub_ItemPreviewSlider, ItemPreviewSlider )
REGISTER_PANEL2D_FACTORY( CSEPortStub_ItemPreviewColorSlider, ItemPreviewColorSlider )
REGISTER_PANEL2D_FACTORY( CSEPortStub_ItemPreviewDebug, ItemPreviewDebug )

CSEPortStub_ItemPreviewSlider::CSEPortStub_ItemPreviewSlider( CPanel2D *pParent, const char *pchID )
: BaseClass( pParent, pchID )
{
}

CSEPortStub_ItemPreviewColorSlider::CSEPortStub_ItemPreviewColorSlider( CPanel2D *pParent, const char *pchID )
: BaseClass( pParent, pchID )
{
}

CSEPortStub_ItemPreviewDebug::CSEPortStub_ItemPreviewDebug( CPanel2D *pParent, const char *pchID )
: BaseClass( pParent, pchID )
{
}

// ============================================================================
// CSGOChat  (CS:GO: CCSGO_Chat + the friends list / lobby UI components)
// ============================================================================
REGISTER_PANEL2D_FACTORY( CSEPortStub_CSGOChat, CSGOChat )

CSEPortStub_CSGOChat::CSEPortStub_CSGOChat( CPanel2D *pParent, const char *pchID )
: BaseClass( pParent, pchID )
{
}
