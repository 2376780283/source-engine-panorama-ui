// SE port - batch E driver: exercise the UiToolkitAPI JavaScript global that CS:GO publishes through
// its cstrike15 uicomponents (CUiComponent_UiToolkit).  Loaded by layout/se_panels_test.xml.
//
// In CS:GO every one of these comes from
//     game/client/cstrike15/uicomponents/uicomponent_uitoolkit.cpp
// and the global object is installed by InstallPanoramaBindings() ->
// PANORAMA_COMPONENT_API_INSTALL( CUiComponent_UiToolkit, this, "UiToolkitAPI", ... ).
//
// Everything is reported through $.Msg, which lands in engine.log with -condebug.
( function ()
{
	$.Msg( "SE port: se_popup_probe.js running - UiToolkitAPI is " + ( typeof UiToolkitAPI ) );

	// Give the menu time to lay out and paint, then show a real popup through the ported
	// CUI_PopupManager (CUI_Popup_Generic) and also exercise a tooltip + the JS callback plumbing.
	$.Schedule( 3.0, function ()
	{
		try
		{
			$.Msg( "SE port: ShowGenericPopupOk ..." );
			var hPopup = UiToolkitAPI.ShowGenericPopupOk(
				"SE port batch E",
				"CUiComponent_UiToolkit + CUI_PopupManager are live: this window comes from JS.",
				"generic",
				function () { $.Msg( "SE port: popup OK callback fired" ); } );
			$.Msg( "SE port: ShowGenericPopupOk returned " + hPopup );
		}
		catch ( e )
		{
			$.Msg( "SE port: ShowGenericPopupOk threw: " + e );
		}

		try
		{
			$.Msg( "SE port: ShowTextTooltip ..." );
			UiToolkitAPI.ShowTextTooltip( "SeCustomLayout", "tooltip from UiToolkitAPI" );
		}
		catch ( e )
		{
			$.Msg( "SE port: ShowTextTooltip threw: " + e );
		}
	} );
} )();
