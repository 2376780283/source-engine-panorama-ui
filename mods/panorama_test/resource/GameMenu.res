//
// Minimal legacy GameUI menu for the panorama test mod.
//
// This mod has no Source 1 game UI of its own - the hosted panorama UI replaces it - but
// CBasePanel::CreateGameMenu() (gameui/BasePanel.cpp) Error()s out when this file cannot be loaded,
// and KeyValues refuses to load a root block without sub-keys, hence the single placeholder entry.
//
"GameMenu"
{
	"1"
	{
		"label"		"#GameUI_GameMenu_ResumeGame"
		"command"	"ResumeGame"
		"name"		"ResumeGame"
	}
}
