//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "panorama/se_gameclient_common.h"
#include "ui_tooltip_text.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

// SE port (temporary tooltip probe, implemented in panorama/ui_tooltip_manager.cpp).
extern void SE_PortTooltipProbe( const char *pMsgFmt, ... );

REGISTER_PANEL2D( CUI_Tooltip_Text, TooltipText )

using namespace panorama;

CUI_Tooltip_Text::CUI_Tooltip_Text( CPanel2D *pParent, const char *pchName ) : CUI_Tooltip_Base( pParent, pchName )
{
	Initialize();
}

CUI_Tooltip_Text::CUI_Tooltip_Text( IUIWindow *pParent, const char *pchName ) : CUI_Tooltip_Base( pParent, pchName )
{
	Initialize();
}

void CUI_Tooltip_Text::Initialize()
{
	CPanel2D *pContentsPanel = GetContentsPanel();
	bool bLayoutLoaded = pContentsPanel ? pContentsPanel->BLoadLayout( "file://{resources}/layout/tooltips/tooltip_text.xml" ) : false;
	SE_PortTooltipProbe( "TTIP text BLoadLayout(tooltip_text.xml)=%d contents=%p", ( int )bLayoutLoaded, ( void * )pContentsPanel );
	DbgVerify( bLayoutLoaded );

	m_pTextLabel = pContentsPanel ? panel_cast< CLabel * >( pContentsPanel->FindChildInLayoutFile( "TextLabel" ) ) : nullptr;
	SE_PortTooltipProbe( "TTIP text label=%p", ( void * )m_pTextLabel );
}

CUI_Tooltip_Text::~CUI_Tooltip_Text()
{
}

void CUI_Tooltip_Text::SetText( const char *pszText )
{
	m_pTextLabel->SetTextWithDialogVariables( pszText );
}