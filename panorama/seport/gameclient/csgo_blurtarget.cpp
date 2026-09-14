//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: SE port of D:\CSGO2019\game\client\cstrike15\panorama\csgo_blurtarget.cpp - see the
//          header for what the class is for.  Only one thing needed adapting: CS:GO looks the
//          "blurrects" ids up through CUI_Root::GetRootForWindow(), which is game-client code this
//          port does not have (see SE_PortFindRootPanel below).
//
//=============================================================================//

#include "stdafx_client.h"

#include "seport/gameclient/csgo_blurtarget.h"

#include "panorama/controls/panel2d.h"
#include "panorama/uijsregistration.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

REGISTER_PANEL2D_FACTORY( CCSGO_BlurTarget, CSGOBlurTarget )

//-----------------------------------------------------------------------------
// SE port: CS:GO resolves the "blurrects" panel names with
//     CUI_Root::GetRootForWindow( GetParentWindow() )->FindChildTraverse( name )
// CUI_Root is game-client code (game/client/panorama/ui_root.cpp) that this port does not have, and
// IUIPanel/IUIWindow expose no root accessor.  The names are authored against the layout file that
// contains the blur target, and FindChildTraverse() walks the whole subtree, so climbing to the
// topmost panel of this layout answers the same lookups.
//-----------------------------------------------------------------------------
static IUIPanel *SE_PortFindRootPanel( CPanel2D *pPanel )
{
	if ( !pPanel )
		return NULL;

	IUIPanel *pRoot = pPanel->UIPanel();
	while ( pRoot && pRoot->GetParent() )
		pRoot = pRoot->GetParent();

	return pRoot;
}

CCSGO_BlurTarget::CCSGO_BlurTarget( CPanel2D *pParent, const char *pchID )
:
	CPanel2D( pParent, pchID ),
	m_bBlurRectInitialized( false )
{
}


void CCSGO_BlurTarget::AddBlurPanel( CPanel2D *pPanel )
{
	CPanelPtr< IUIPanel > ptrPanel( pPanel );
	if ( !m_vecBlurRects.HasElement( ptrPanel ) )
	{
		m_vecBlurRects.AddToTail( ptrPanel );
	}
}


void CCSGO_BlurTarget::RemoveBlurPanel( CPanel2D *pPanel )
{
	CPanelPtr< IUIPanel > ptrPanel( pPanel );
	m_vecBlurRects.FindAndFastRemove( ptrPanel );
}


void CCSGO_BlurTarget::SetupJavascriptObjectTemplate()
{
	BaseClass::SetupJavascriptObjectTemplate();

	panorama::RegisterJSMethod( "AddBlurPanel", PANORAMA_DELEGATE( &CCSGO_BlurTarget::AddBlurPanel ) );
	panorama::RegisterJSMethod( "RemoveBlurPanel", PANORAMA_DELEGATE( &CCSGO_BlurTarget::RemoveBlurPanel ) );
}


void CCSGO_BlurTarget::GrabRects()
{

	if ( !m_bBlurRectInitialized )
	{
		// First time initialization
		// Populate m_vecBlurRects from the "blurrects" attribute
		// Note: Doing the work on the first "paint" / call instead of BSetProperty
		// as not all panels have been created by the time BSetProperty is called

		static const CPanoramaSymbol k_symBlurRects( "blurrects" );

		const char* pBlurRects = GetAttribute( k_symBlurRects, "" );
		if ( pBlurRects && pBlurRects[ 0 ] )
		{
			IUIPanel *pRoot = SE_PortFindRootPanel( this );

			CUtlVector< char*, CUtlMemory< char* > > vecPanelNames;
			V_SplitString( pBlurRects, " ", vecPanelNames );

			FOR_EACH_VEC( vecPanelNames, i )
			{
				CPanel2D* pRect = pRoot ? ToPanel2D( pRoot->FindChildTraverse( vecPanelNames[ i ] ) ) : NULL;
				if ( pRect )
				{
					CPanelPtr<IUIPanel> safeptr( pRect );
					m_vecBlurRects.AddToTail( safeptr );
				}
				else
				{
					Warning( "CCSGO_BlurTarget - Unable to find panel with the given id \"%s\"! Panel is possibly created dynamically.\n", vecPanelNames[ i ] );
				}

				// SE port: CS:GO leaks these strings; free them once the id has been consumed.
				delete[] vecPanelNames[ i ];
			}

			vecPanelNames.RemoveAll();
		}

		m_bBlurRectInitialized = true;
	}

}

void CCSGO_BlurTarget::Paint()
{
	GrabRects();

	if ( m_vecBlurRects.Count() )
	{
		SetRepaint( k_EPanelRepaintFull );		// Should add force repaint only when blur sources change if this is used a lot.
	}

	BaseClass::Paint();

	// Always regenerate list from m_vecBlurRects (valid panels only)
	m_vecPaintBlurRects.RemoveAll();

	if ( m_vecBlurRects.Count() )
	{
		CPanelPtr<IUIPanel> blurTargetPanel( this );
		m_vecPaintBlurRects.AddToTail( blurTargetPanel.GetHandleAsUInt64() );

		FOR_EACH_VEC_BACK( m_vecBlurRects, i )
		{
			IUIPanel *pTarget = m_vecBlurRects[i].Get();
			if ( pTarget )
			{
				m_vecPaintBlurRects.AddToTail( m_vecBlurRects[i].GetHandleAsUInt64( ) );
			}
			else
			{
				// Panel has been deleted. Removed entry from m_vecBlurRects
				// Valid to call FastRemove as iterating m_vecBlurRects backwards
				m_vecBlurRects.FastRemove( i );
			}
		}

		AccessRenderEngine()->PushBlurPanels( m_vecPaintBlurRects );
	}
}
