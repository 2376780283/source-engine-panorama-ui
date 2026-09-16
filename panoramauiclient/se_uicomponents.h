//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: SE port - host for CS:GO's cstrike15 UI component framework (batch E of
//          docs/panorama_stage2_plan.md).
//
//          In CS:GO the components are created and installed by the game client's gameui module
//          (game/client/cstrike15/gameui/gameui_interface.cpp::CGameUI::Initialize):
//
//              m_arrUiComponents.AddToTail( CUiComponent_GameInterface::GetInstance() );
//              m_arrUiComponents.AddToTail( CUiComponent_UiToolkit::GetInstance() );
//              ...
//              FOR_EACH_VEC( m_arrUiComponents, i )
//                  m_arrUiComponents[i]->InstallPanoramaBindings();
//
//          This port has no cstrike15 gameui module, so the same two steps are done here, in the same
//          order, as soon as the panorama UIEngine exists.
//
//=============================================================================//

#ifndef SE_UICOMPONENTS_H
#define SE_UICOMPONENTS_H
#pragma once

// Creates the UI components this port ships (currently CUiComponent_UiToolkit) and installs their
// panorama JavaScript bindings - i.e. the "UiToolkitAPI" global.  Safe to call more than once; it
// needs the panorama UIEngine, so call it after that has been set up (panoramauiclient.cpp).
void SE_PortInstallUiComponentBindings();

// Installs the "GameInterfaceAPI" global (CUiComponent_GameInterface, se_ui_settings.cpp).  CS:GO
// installs it alongside UiToolkitAPI from CGameUI::Initialize(); same prerequisites.
void SE_PortInstallGameInterfaceBindings();

#endif // SE_UICOMPONENTS_H
