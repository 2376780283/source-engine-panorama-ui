//
// Scaleform component functions for game interface.  
//

UI_COMPONENT_FUNCTIONLIST_ELEMENT( void,		ConsoleCommand )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( void,		GetMouseEnableBindingName )

UI_COMPONENT_FUNCTIONLIST_ELEMENT( string,		GetSettingString )
// ARGUMENTS	"key"

UI_COMPONENT_FUNCTIONLIST_ELEMENT( void,		SetSettingString )
// ARGUMENTS	"key", "value"

// SE port (2026-09-16): the *deployed* CS:GO content is newer than this source drop and calls two
// members that the 2019 functions list above does not have.  Before GameInterfaceAPI became a real
// C++ object here, se_api_shim.js' proxy stub answered them (as a truthy object), which is why nobody
// noticed.  Both now have real port implementations - see uicomponent_gameinterface.cpp.
UI_COMPONENT_FUNCTIONLIST_ELEMENT( number,		GetEngineSoundSystemsRunning )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( number,		HasCommandLineParm )
// ARGUMENTS	"-parm"

// SE port (2026-09-18): opens a URL with the OS default handler (store "市场" links, news
// entries, popup links).  The scripts route SteamOverlayAPI.OpenURL /
// OpenUrlInOverlayOrExternalBrowser here - see se_session_sim.js.
UI_COMPONENT_FUNCTIONLIST_ELEMENT( void,		OpenURLInBrowser )
// ARGUMENTS	"url"
