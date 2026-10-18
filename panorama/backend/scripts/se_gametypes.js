"use strict";
// SE port: GENERATED - do not edit by hand.
// Source: gamemodes.txt (the game's own game mode / map group table)
// Generator: build\_gen_gametypes.ps1
// Consumed by se_session_sim.js, which answers GameTypesAPI.GetConfig() with it.
(function () {
	var g = Function("return this")();

	var types = {
		"classic": {
			"value": "0",
			"nameID": "#SFUI_GameTypeClassic",
			"gameModes": {
				"casual": {
					"value": "0",
					"nameID": "#SFUI_GameModeCasual",
					"descID": "#SFUI_GameModeCasualDesc",
					"descID_List": "#SFUI_GameModeCasualDescSPList",
					"uid": "1",
					"maxplayers": "20",
					"mapgroupsSP": {
						"random_classic": "0",
						"mg_de_dust2": "1",
						"mg_de_mirage": "2",
						"mg_de_inferno": "3",
						"mg_de_vertigo": "4",
						"mg_de_cbble": "5",
						"mg_de_anubis": "6",
						"mg_de_cache": "7",
						"mg_de_ancient": "8",
						"mg_de_train": "9",
						"mg_de_overpass": "10",
						"mg_de_nuke": "11",
						"mg_de_canals": "12",
						"mg_de_tuscan": "13",
						"mg_cs_agency": "14",
						"mg_cs_militia": "15",
						"mg_cs_office": "16",
						"mg_cs_italy": "17",
						"mg_cs_assault": "18"
					},
					"mapgroupsMP": {
						"mg_casualsigma": "0",
						"mg_casualdelta": "1",
						"mg_dust247": "2",
						"mg_hostage": "3"
					}
				},
				"competitive": {
					"value": "1",
					"nameID": "#SFUI_GameModeCompetitive",
					"descID": "#SFUI_GameModeCompetitiveDesc",
					"descID_List": "#SFUI_GameModeCompetitiveDescList",
					"uid": "2",
					"maxplayers": "10",
					"show_rich_presence_map_game": "#SFUI_Lobby_StatusRichPresenceSeparator_classic_competitive",
					"show_rich_presence_map_watch": "#SFUI_Lobby_StatusRichPresenceSeparator_classic_competitive",
					"show_rich_presence_map_review": "#SFUI_Lobby_StatusRichPresenceSeparator_classic_competitive",
					"mapgroupsSP": {
						"random_classic": "0",
						"mg_de_ancient": "1",
						"mg_de_anubis": "2",
						"mg_de_inferno": "3",
						"mg_de_mirage": "4",
						"mg_de_nuke": "5",
						"mg_de_overpass": "6",
						"mg_de_vertigo": "7",
						"mg_de_tuscan": "8",
						"mg_de_dust2": "9",
						"mg_de_train": "10",
						"mg_de_cache": "11",
						"mg_cs_agency": "12",
						"mg_cs_office": "13"
					},
					"mapgroupsMP": {
						"mg_lobby_mapveto": "0",
						"mg_de_ancient": "1",
						"mg_de_anubis": "2",
						"mg_de_inferno": "3",
						"mg_de_mirage": "4",
						"mg_de_nuke": "5",
						"mg_de_overpass": "6",
						"mg_de_vertigo": "7",
						"mg_de_tuscan": "8",
						"mg_de_dust2": "9",
						"mg_de_train": "10",
						"mg_de_cache": "11",
						"mg_cs_agency": "12",
						"mg_cs_office": "13"
					}
				},
				"scrimcomp2v2": {
					"value": "2",
					"nameID": "#SFUI_GameModeScrimComp2v2",
					"descID": "#SFUI_GameModeScrimComp2v2Desc",
					"descID_List": "#SFUI_GameModeScrimComp2v2DescList",
					"uid": "2",
					"maxplayers": "4",
					"show_rich_presence_map_game": "#SFUI_Lobby_StatusRichPresenceSeparator_classic_competitive",
					"show_rich_presence_map_watch": "#SFUI_Lobby_StatusRichPresenceSeparator_classic_competitive",
					"show_rich_presence_map_review": "#SFUI_Lobby_StatusRichPresenceSeparator_classic_competitive",
					"mapgroupsSP": {
						"mg_de_boyard": "0",
						"mg_de_chalice": "1",
						"mg_de_vertigo": "2",
						"mg_de_inferno": "3",
						"mg_de_overpass": "4",
						"mg_de_cbble": "5",
						"mg_de_train": "6",
						"mg_de_shortnuke": "7",
						"mg_de_shortdust": "8",
						"mg_de_lake": "9"
					},
					"mapgroupsMP": {
						"mg_de_boyard": "0",
						"mg_de_chalice": "1",
						"mg_de_vertigo": "2",
						"mg_de_inferno": "3",
						"mg_de_overpass": "4",
						"mg_de_cbble": "5",
						"mg_de_train": "6",
						"mg_de_shortnuke": "7",
						"mg_de_shortdust": "8",
						"mg_de_lake": "9"
					}
				},
				"scrimcomp5v5": {
					"value": "3",
					"nameID": "#SFUI_GameModeScrimComp5v5",
					"descID": "#SFUI_GameModeScrimComp5v5Desc",
					"descID_List": "#SFUI_GameModeScrimComp5v5DescList",
					"uid": "2",
					"maxplayers": "10",
					"show_rich_presence_map_game": "#SFUI_Lobby_StatusRichPresenceSeparator_classic_competitive",
					"show_rich_presence_map_watch": "#SFUI_Lobby_StatusRichPresenceSeparator_classic_competitive",
					"show_rich_presence_map_review": "#SFUI_Lobby_StatusRichPresenceSeparator_classic_competitive",
					"mapgroupsSP": {
						"random_classic": "0",
						"mg_de_mirage": "1",
						"mg_de_vertigo": "2",
						"mg_de_inferno": "3",
						"mg_de_overpass": "4",
						"mg_de_train": "5",
						"mg_de_nuke": "6",
						"mg_de_dust2": "7",
						"mg_de_cache": "8",
						"mg_cs_office": "9",
						"mg_cs_agency": "10"
					},
					"mapgroupsMP": {
						"mg_de_mirage": "0",
						"mg_de_vertigo": "1",
						"mg_de_inferno": "2",
						"mg_de_overpass": "3",
						"mg_de_train": "4",
						"mg_de_nuke": "5",
						"mg_de_dust2": "6",
						"mg_de_cache": "7",
						"mg_cs_office": "8",
						"mg_cs_agency": "9"
					}
				}
			}
		},
		"gungame": {
			"value": "1",
			"nameID": "#SFUI_GameTypeGungame",
			"gameModes": {
				"gungameprogressive": {
					"value": "0",
					"nameID": "#SFUI_GameModeGGProgressive",
					"descID": "#SFUI_GameModeGGProgressiveDesc",
					"descID_List": "#SFUI_GameModeGGProgressiveDescList",
					"uid": "11",
					"maxplayers": "10",
					"mapgroupsSP": {
						"random_ar": "0",
						"mg_ar_baggage": "1",
						"mg_ar_shoots": "2",
						"mg_ar_lake": "3",
						"mg_ar_stmarc": "4",
						"mg_ar_safehouse": "5",
						"mg_ar_lunacy": "6",
						"mg_ar_monastery": "7"
					},
					"mapgroupsMP": {
						"mg_armsrace": "0"
					}
				},
				"gungametrbomb": {
					"value": "1",
					"nameID": "#SFUI_GameModeGGBomb",
					"descID": "#SFUI_GameModeGGBombDesc",
					"descID_List": "#SFUI_GameModeGGBombDescList",
					"uid": "12",
					"maxplayers": "10",
					"mapgroupsSP": {
						"random_demo": "0",
						"mg_de_bank": "1",
						"mg_de_lake": "2",
						"mg_de_safehouse": "3",
						"mg_de_sugarcane": "4",
						"mg_de_stmarc": "5",
						"mg_de_shortdust": "6"
					},
					"mapgroupsMP": {
						"mg_demolition": "0"
					}
				},
				"deathmatch": {
					"value": "2",
					"nameID": "#SFUI_Deathmatch",
					"descID": "#SFUI_DeathmatchDesc",
					"descID_List": "#SFUI_GameModeDeathmatchDescList",
					"uid": "13",
					"maxplayers": "16",
					"mapgroupsSP": {
						"random_classic": "0",
						"mg_de_dust2": "1",
						"mg_de_mirage": "2",
						"mg_de_inferno": "3",
						"mg_de_vertigo": "4",
						"mg_de_cbble": "5",
						"mg_de_ancient": "6",
						"mg_de_cache": "7",
						"mg_de_anubis": "8",
						"mg_de_tuscan": "9",
						"mg_de_train": "10",
						"mg_de_overpass": "11",
						"mg_de_nuke": "12",
						"mg_de_canals": "13",
						"mg_cs_agency": "14",
						"mg_cs_militia": "15",
						"mg_cs_office": "16",
						"mg_cs_italy": "17",
						"mg_cs_assault": "18"
					},
					"mapgroupsMP": {
						"mg_casualsigma": "0",
						"mg_casualdelta": "1",
						"mg_dust247": "2",
						"mg_hostage": "3"
					}
				}
			}
		},
		"training": {
			"value": "2",
			"nameID": "#SFUI_GameTypeFreestyle",
			"singleplayeronly": "1",
			"gameModes": {
				"training": {
					"value": "0",
					"nameID": "#SFUI_GameTypeTraining",
					"descID": "#SFUI_GameModeTrainingDesc",
					"descID_List": "#SFUI_GameModeTrainingDescList",
					"showdisclaimer": "1",
					"uid": "21",
					"maxplayers": "1",
					"mapgroupsSP": {
						"mg_training1": "0"
					}
				}
			}
		},
		"custom": {
			"value": "3",
			"nameID": "#SFUI_GameTypeCustom",
			"gameModes": {
				"custom": {
					"value": "0",
					"nameID": "#SFUI_GameModeCustom",
					"descID": "#SFUI_GameModeCustomDesc",
					"descID_List": "#SFUI_GameModeCustomDescList",
					"showdisclaimer": "1",
					"uid": "30",
					"maxplayers": "100"
				}
			}
		},
		"cooperative": {
			"value": "4",
			"nameID": "#SFUI_GameTypeCooperative",
			"gameModes": {
				"cooperative": {
					"value": "0",
					"nameID": "#SFUI_GameModeCooperative",
					"descID": "#SFUI_GameModeCooperativeDesc",
					"maxplayers": "20",
					"show_rich_presence_map_game": "#SFUI_Lobby_StatusRichPresenceSeparator_classic_competitive",
					"show_rich_presence_map_watch": "#SFUI_Lobby_StatusRichPresenceSeparator_classic_competitive",
					"show_rich_presence_map_review": "#SFUI_Lobby_StatusRichPresenceSeparator_classic_competitive"
				},
				"coopmission": {
					"value": "1",
					"nameID": "#SFUI_GameModeCoopMission",
					"descID": "#SFUI_GameModeCooperativeDesc",
					"maxplayers": "10",
					"show_rich_presence_map_game": "#SFUI_Lobby_StatusRichPresenceSeparator_classic_competitive",
					"show_rich_presence_map_watch": "#SFUI_Lobby_StatusRichPresenceSeparator_classic_competitive",
					"show_rich_presence_map_review": "#SFUI_Lobby_StatusRichPresenceSeparator_classic_competitive"
				}
			}
		},
		"skirmish": {
			"value": "5",
			"nameID": "#SFUI_GameTypeSkirmish",
			"gameModes": {
				"skirmish": {
					"value": "0",
					"nameID": "#SFUI_GameModeSkirmish",
					"descID": "#SFUI_GameModeSkirmishDesc",
					"descID_List": "#SFUI_GameModeSkirmishDescList",
					"maxplayers": "12",
					"mapgroupsSP": {
						"mg_skirmish_armsrace": "0",
						"mg_skirmish_demolition": "1",
						"mg_skirmish_flyingscoutsman": "2",
						"mg_skirmish_retakes": "3"
					},
					"mapgroupsMP": {
						"mg_skirmish_armsrace": "0",
						"mg_skirmish_demolition": "1",
						"mg_skirmish_flyingscoutsman": "2",
						"mg_skirmish_retakes": "3"
					}
				}
			}
		},
		"freeforall": {
			"value": "6",
			"nameID": "#SFUI_GameTypeFreeForAll",
			"gameModes": {
				"survival": {
					"value": "0",
					"nameID": "#SFUI_GameModeSurvival",
					"descID": "#SFUI_GameModeSurvivalDesc",
					"maxplayers": "16",
					"show_rich_presence_map_game": "#SFUI_Lobby_StatusRichPresenceSeparator_classic_competitive",
					"show_rich_presence_map_watch": "#SFUI_Lobby_StatusRichPresenceSeparator_classic_competitive",
					"show_rich_presence_map_review": "#SFUI_Lobby_StatusRichPresenceSeparator_classic_competitive",
					"mapgroupsSP": {
						"mg_dz_sirocco": "0"
					},
					"mapgroupsMP": {
						"mg_dz_sirocco": "0"
					}
				}
			}
		}
	};

	var mapgroups = {
		"mg_op_op08": {
			"imagename": "mapgroup-op08",
			"nameID": "#SFUI_Mapgroup_op_op08",
			"tooltipID": "#SFUI_MapGroup_Tooltip_Desc_Operation",
			"tooltipMaps": "de_austria,de_shipped,de_lite,de_thrill,de_blackgold,cs_agency,cs_insertion",
			"name": "mg_op_op08",
			"show_medal_icon": "8Operation$OperationCoin",
			"show_rich_presence": "_op08",
			"grouptype": "op_op08",
			"icon_image_path": "map_icons/op08/mapgroup_icon_op08",
			"maps": {
				"de_austria": "",
				"de_shipped": "",
				"de_lite": "",
				"de_thrill": "",
				"de_blackgold": "",
				"cs_agency": "",
				"cs_insertion": ""
			}
		},
		"mg_op_op07": {
			"imagename": "mapgroup-op07",
			"nameID": "#SFUI_Mapgroup_op_op07",
			"tooltipID": "#SFUI_MapGroup_Tooltip_Desc_Operation",
			"tooltipMaps": "cs_cruise,de_coast,de_empire,de_mikla,de_royal,de_santorini,de_tulip",
			"name": "mg_op_op07",
			"show_medal_icon": "7Operation$OperationCoin",
			"show_rich_presence": "_op07",
			"grouptype": "op_op07",
			"icon_image_path": "map_icons/mapgroup_icon_op07",
			"maps": {
				"cs_cruise": "",
				"de_coast": "",
				"de_empire": "",
				"de_mikla": "",
				"de_royal": "",
				"de_santorini": "",
				"de_tulip": ""
			}
		},
		"mg_op_op06": {
			"imagename": "mapgroup-op06",
			"nameID": "#SFUI_Mapgroup_op_op06",
			"tooltipID": "#SFUI_MapGroup_Tooltip_Desc_Operation",
			"tooltipMaps": "de_rails,de_resort,de_zoo,de_log,de_season,cs_agency",
			"name": "mg_op_op06",
			"show_medal_icon": "6Operation$OperationCoin",
			"show_rich_presence": "_op06",
			"grouptype": "op_op06",
			"icon_image_path": "map_icons/mapgroup_icon_op06",
			"maps": {
				"de_rails": "",
				"de_resort": "",
				"de_zoo": "",
				"de_log": "",
				"de_season": "",
				"cs_agency": ""
			}
		},
		"mg_op_op05": {
			"imagename": "mapgroup-vanguard",
			"nameID": "#SFUI_Mapgroup_op_op05",
			"tooltipID": "#SFUI_MapGroup_Tooltip_Desc_Active",
			"tooltipMaps": "de_train,cs_workout,cs_backalley,de_marquis,de_facade,de_season,de_bazaar",
			"name": "mg_op_op05",
			"show_medal_icon": "5Operation$Community Season Five Summer 2014",
			"show_rich_presence": "_vanguard",
			"grouptype": "op_op05",
			"icon_image_path": "map_icons/mapgroup_icon_op05",
			"maps": {
				"de_train": "",
				"cs_workout": "",
				"cs_backalley": "",
				"de_marquis": "",
				"de_facade": "",
				"de_season": "",
				"de_bazaar": ""
			}
		},
		"mg_op_breakout": {
			"imagename": "mapgroup-breakout",
			"nameID": "#SFUI_mapgroup_op_breakout",
			"tooltipMaps": "de_castle,de_overgrown,de_blackgold,de_mist,cs_rush,cs_insertion",
			"tooltipID": "#SFUI_MapGroup_Tooltip_Desc_Operation",
			"name": "mg_op_breakout",
			"show_medal_icon": "4OpBreakout$Community Season Four Summer 2014",
			"show_rich_presence": "_breakout",
			"grouptype": "op_breakout",
			"icon_image_path": "map_icons/mapgroup_icon_op_breakout",
			"maps": {
				"de_castle": "",
				"de_overgrown": "",
				"de_blackgold": "",
				"de_mist": "",
				"cs_rush": "",
				"cs_insertion": ""
			}
		},
		"mg_active": {
			"imagename": "mapgroup-active",
			"nameID": "#SFUI_Mapgroup_active",
			"tooltipID": "#SFUI_MapGroup_Tooltip_Desc_Active",
			"tooltipMaps": "",
			"name": "mg_active",
			"grouptype": "active",
			"icon_image_path": "map_icons/mapgroup_icon_active",
			"maps": {
				"de_inferno": "",
				"de_train": "",
				"de_mirage": "",
				"de_nuke": "",
				"de_dust2": "",
				"de_overpass": "",
				"de_vertigo": ""
			}
		},
		"mg_casualdelta": {
			"imagename": "mapgroup-casualdelta",
			"nameID": "#SFUI_Mapgroup_casualdelta",
			"tooltipID": "#SFUI_MapGroup_Tooltip_Desc_CasualDelta",
			"tooltipMaps": "de_anubis,de_mirage,de_inferno,de_overpass,de_nuke,de_train",
			"name": "mg_casualdelta",
			"grouptype": "reserves",
			"icon_image_path": "map_icons/mapgroup_icon_reserves",
			"maps": {
				"de_anubis": "",
				"de_mirage": "",
				"de_inferno": "",
				"de_overpass": "",
				"de_nuke": "",
				"de_train": ""
			}
		},
		"mg_casualsigma": {
			"imagename": "mapgroup-casualsigma",
			"nameID": "#SFUI_Mapgroup_casualsigma",
			"tooltipID": "#SFUI_MapGroup_Tooltip_Desc_CasualSigma",
			"tooltipMaps": "de_canals,de_vertigo,de_cbble,de_ancient,de_cache,de_tuscan",
			"name": "mg_casualsigma",
			"grouptype": "reserves",
			"icon_image_path": "map_icons/mapgroup_icon_reserves",
			"maps": {
				"de_canals": "",
				"de_vertigo": "",
				"de_cbble": "",
				"de_ancient": "",
				"de_cache": "",
				"de_tuscan": ""
			}
		},
		"mg_reserves": {
			"imagename": "mapgroup-reserves",
			"nameID": "#SFUI_Mapgroup_reserves",
			"tooltipID": "#SFUI_MapGroup_Tooltip_Desc_Reserves",
			"tooltipMaps": "",
			"name": "mg_reserves",
			"grouptype": "reserves",
			"icon_image_path": "map_icons/mapgroup_icon_reserves",
			"maps": {
				"de_canals": "",
				"de_aztec": "",
				"de_dust": "",
				"de_cache": ""
			}
		},
		"mg_hostage": {
			"imagename": "mapgroup-hostage",
			"nameID": "#SFUI_Mapgroup_hostage",
			"tooltipID": "#SFUI_MapGroup_Tooltip_Desc_Hostage",
			"tooltipMaps": "cs_agency,cs_militia,cs_office,cs_italy,cs_assault",
			"name": "mg_hostage",
			"grouptype": "hostage",
			"icon_image_path": "map_icons/mapgroup_icon_hostage",
			"maps": {
				"cs_agency": "",
				"cs_militia": "",
				"cs_office": "",
				"cs_italy": "",
				"cs_assault": ""
			}
		},
		"mg_deathmatch": {
			"imagename": "mapgroup-bomb",
			"nameID": "#SFUI_Mapgroup_allclassic",
			"tooltipID": "#SFUI_MapGroup_Tooltip_Desc_DeathMatch",
			"name": "mg_deathmatch",
			"icon_image_path": "map_icons/mapgroup_icon_deathmatch",
			"maps": {
				"de_dust2": "",
				"de_inferno": "",
				"de_mirage": "",
				"de_cbble": "",
				"de_overpass": "",
				"de_dust": "",
				"de_aztec": "",
				"de_nuke": "",
				"de_vertigo": "",
				"cs_militia": "",
				"cs_assault": "",
				"cs_office": "",
				"cs_italy": "",
				"de_lake": "",
				"de_stmarc": "",
				"de_sugarcane": "",
				"de_bank": "",
				"de_safehouse": "",
				"de_shortdust": "",
				"ar_shoots": "",
				"ar_baggage": "",
				"ar_monastery": ""
			}
		},
		"mg_armsrace": {
			"imagename": "mapgroup-armsrace",
			"nameID": "#SFUI_Mapgroup_armsrace",
			"tooltipID": "#SFUI_MapGroup_Tooltip_Desc_Armsrace",
			"tooltipMaps": "de_lake,de_stmarc,de_bank,de_safehouse,ar_shoots,ar_baggage,ar_lunacy,ar_monastery",
			"name": "mg_armsrace",
			"icon_image_path": "map_icons/mapgroup_icon_armsrace",
			"maps": {
				"de_lake": "",
				"de_stmarc": "",
				"de_safehouse": "",
				"ar_shoots": "",
				"ar_baggage": "",
				"ar_lunacy": "",
				"ar_monastery": ""
			}
		},
		"mg_demolition": {
			"imagename": "mapgroup-demolition",
			"nameID": "#SFUI_Mapgroup_demolition",
			"tooltipID": "#SFUI_MapGroup_Tooltip_Desc_demo",
			"tooltipMaps": "de_lake,de_stmarc,de_sugarcane,de_bank,de_safehouse,de_shortdust",
			"name": "mg_demolition",
			"icon_image_path": "map_icons/mapgroup_icon_demolition",
			"maps": {
				"de_lake": "",
				"de_stmarc": "",
				"de_sugarcane": "",
				"de_bank": "",
				"de_safehouse": "",
				"de_shortdust": ""
			}
		},
		"mg_lowgravity": {
			"imagename": "mapgroup-demolition",
			"nameID": "#SFUI_Mapgroup_lowgravity",
			"tooltipID": "#SFUI_MapGroup_Tooltip_Desc_lowgravity",
			"tooltipMaps": "de_lake,de_safehouse,ar_dizzy,ar_lunacy,ar_shoots",
			"name": "mg_lowgravity",
			"icon_image_path": "map_icons/mapgroup_icon_demolition",
			"maps": {
				"de_lake": "",
				"de_safehouse": "",
				"ar_dizzy": "",
				"ar_lunacy": "",
				"ar_shoots": ""
			}
		},
		"mg_skirmish_stabstabzap": {
			"imagename": "mapgroup-stabstabzap",
			"nameID": "#Skirmish_CC_SSZ_name",
			"tooltipID": "#Skirmish_CC_SSZ_details",
			"tooltipMaps": "",
			"name": "mg_skirmish_stabstabzap",
			"icon_image_path": "map_icons/mapgroup_icon_skirmish",
			"maps": {
				"de_safehouse": "stabstabzap",
				"de_lake": "stabstabzap",
				"gd_rialto": "stabstabzap",
				"de_austria": "stabstabzap"
			}
		},
		"mg_skirmish_flyingscoutsman": {
			"imagename": "mapgroup-flyingscoutsman",
			"nameID": "#Skirmish_CC_FS_name",
			"tooltipID": "#Skirmish_CC_FS_details",
			"tooltipMaps": "",
			"name": "mg_skirmish_flyingscoutsman",
			"icon_image_path": "map_icons/mapgroup_icon_skirmish",
			"maps": {
				"de_lake": "flyingscoutsman",
				"de_safehouse": "flyingscoutsman",
				"ar_dizzy": "flyingscoutsman",
				"ar_lunacy": "flyingscoutsman",
				"ar_shoots": "flyingscoutsman"
			}
		},
		"mg_skirmish_retakes": {
			"imagename": "mapgroup-retakes",
			"nameID": "#Skirmish_CC_RT_name",
			"tooltipID": "#Skirmish_CC_RT_details",
			"tooltipMaps": "",
			"name": "mg_skirmish_retakes",
			"icon_image_path": "map_icons/mapgroup_icon_skirmish",
			"maps": {
				"de_inferno": "retakes",
				"de_mirage": "retakes",
				"de_dust2": "retakes",
				"de_nuke": "retakes",
				"de_overpass": "retakes",
				"de_train": "retakes",
				"de_vertigo": "retakes",
				"de_ancient": "retakes"
			}
		},
		"mg_skirmish_triggerdiscipline": {
			"imagename": "mapgroup-triggerdiscipline",
			"nameID": "#Skirmish_CC_TD_name",
			"tooltipID": "#Skirmish_CC_TD_details",
			"tooltipMaps": "",
			"name": "mg_skirmish_triggerdiscipline",
			"icon_image_path": "map_icons/mapgroup_icon_skirmish",
			"maps": {
				"de_austria": "triggerdiscipline",
				"de_inferno": "triggerdiscipline",
				"de_thrill": "triggerdiscipline",
				"de_mirage": "triggerdiscipline",
				"de_dust2": "triggerdiscipline",
				"de_lite": "triggerdiscipline"
			}
		},
		"mg_skirmish_headshots": {
			"imagename": "mapgroup-headshots",
			"nameID": "#Skirmish_DM_HS_name",
			"tooltipID": "#Skirmish_DM_HS_details",
			"tooltipMaps": "",
			"name": "mg_skirmish_headshots",
			"icon_image_path": "map_icons/mapgroup_icon_skirmish",
			"maps": {
				"cs_agency": "headshots",
				"de_inferno": "headshots",
				"de_blackgold": "headshots",
				"de_cache": "headshots",
				"de_cbble": "headshots",
				"de_nuke": "headshots"
			}
		},
		"mg_skirmish_huntergatherers": {
			"imagename": "mapgroup-huntergatherers",
			"nameID": "#Skirmish_TDM_HG_name",
			"tooltipID": "#Skirmish_TDM_HG_details",
			"tooltipMaps": "",
			"name": "mg_skirmish_huntergatherers",
			"icon_image_path": "map_icons/mapgroup_icon_skirmish",
			"maps": {
				"de_nuke": "huntergatherers",
				"de_dust2": "huntergatherers",
				"cs_insertion": "huntergatherers",
				"de_thrill": "huntergatherers",
				"de_canals": "huntergatherers",
				"de_cbble": "huntergatherers",
				"de_train": "huntergatherers"
			}
		},
		"mg_skirmish_heavyassaultsuit": {
			"imagename": "mapgroup-heavyassaultsuit",
			"nameID": "#Skirmish_CC_HAS_name",
			"tooltipID": "#Skirmish_CC_HAS_details",
			"tooltipMaps": "",
			"name": "mg_skirmish_heavyassaultsuit",
			"icon_image_path": "map_icons/mapgroup_icon_skirmish",
			"maps": {
				"de_dust2": "heavyassaultsuit",
				"de_mirage": "heavyassaultsuit",
				"de_overpass": "heavyassaultsuit",
				"de_shipped": "heavyassaultsuit",
				"de_austria": "heavyassaultsuit"
			}
		},
		"mg_skirmish_armsrace": {
			"imagename": "mapgroup-armsrace",
			"nameID": "#Skirmish_AR_name",
			"tooltipID": "#Skirmish_AR_details",
			"tooltipMaps": "",
			"name": "mg_skirmish_armsrace",
			"icon_image_path": "map_icons/mapgroup_icon_skirmish",
			"maps": {
				"de_lake": "armsrace",
				"ar_baggage": "armsrace",
				"de_safehouse": "armsrace",
				"de_stmarc": "armsrace",
				"ar_shoots": "armsrace",
				"ar_lunacy": "armsrace",
				"ar_monastery": "armsrace"
			}
		},
		"mg_skirmish_demolition": {
			"imagename": "mapgroup-demolition",
			"nameID": "#Skirmish_DEM_name",
			"tooltipID": "#Skirmish_DEM_details",
			"tooltipMaps": "",
			"name": "mg_skirmish_demolition",
			"icon_image_path": "map_icons/mapgroup_icon_skirmish",
			"maps": {
				"de_lake": "demolition",
				"de_safehouse": "demolition",
				"de_sugarcane": "demolition",
				"de_bank": "demolition",
				"de_stmarc": "demolition",
				"de_shortdust": "demolition"
			}
		},
		"mg_skirmish_dm_freeforall": {
			"imagename": "mapgroup-demolition",
			"nameID": "#Skirmish_DM_FFA_name",
			"tooltipID": "#Skirmish_DM_FFA_details",
			"tooltipMaps": "",
			"name": "mg_skirmish_dm_freeforall",
			"icon_image_path": "map_icons/mapgroup_icon_skirmish",
			"maps": {
				"de_dust2": "dm_freeforall",
				"de_inferno": "dm_freeforall",
				"de_mirage": "dm_freeforall",
				"de_cbble": "dm_freeforall",
				"de_overpass": "dm_freeforall",
				"de_nuke": "dm_freeforall",
				"de_vertigo": "dm_freeforall",
				"cs_militia": "dm_freeforall",
				"cs_assault": "dm_freeforall",
				"cs_office": "dm_freeforall",
				"cs_italy": "dm_freeforall",
				"de_lake": "dm_freeforall",
				"de_stmarc": "dm_freeforall",
				"de_ancient": "dm_freeforall"
			}
		},
		"mg_de_dust": {
			"imagename": "map-dust-overall",
			"nameID": "#SFUI_Map_de_dust",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Reserves",
			"name": "mg_de_dust",
			"icontag": "bomb",
			"grouptype": "reserves",
			"icon_image_path": "map_icons/map_icon_de_dust",
			"maps": {
				"de_dust": ""
			}
		},
		"mg_dust247": {
			"imagename": "map-dust2-overall",
			"nameID": "#SFUI_Map_de_dust2",
			"tooltipID": "#SFUI_MapGroup_Tooltip_Desc_Dust247",
			"name": "mg_dust247",
			"grouptype": "reserves",
			"icon_image_path": "map_icons/map_icon_de_dust2",
			"maps": {
				"de_dust2": ""
			}
		},
		"mg_de_dust2": {
			"imagename": "map-dust2-overall",
			"nameID": "#SFUI_Map_de_dust2",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Reserves",
			"name": "mg_de_dust2",
			"icontag": "bomb",
			"grouptype": "reserves",
			"icon_image_path": "map_icons/map_icon_de_dust2",
			"maps": {
				"de_dust2": ""
			}
		},
		"mg_de_aztec": {
			"imagename": "map-aztec-overall",
			"nameID": "#SFUI_Map_de_aztec",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Reserves",
			"name": "mg_de_aztec",
			"icontag": "bomb",
			"grouptype": "reserves",
			"icon_image_path": "map_icons/map_icon_de_aztec",
			"maps": {
				"de_aztec": ""
			}
		},
		"mg_de_inferno": {
			"imagename": "map-inferno-overall",
			"nameID": "#SFUI_Map_de_inferno",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Active_Op_over",
			"name": "mg_de_inferno",
			"icontag": "bomb",
			"grouptype": "active",
			"icon_image_path": "map_icons/map_icon_de_inferno",
			"maps": {
				"de_inferno": ""
			}
		},
		"mg_de_mirage": {
			"imagename": "map-mirage-overall",
			"nameID": "#SFUI_Map_de_mirage",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Active_Op_over",
			"name": "mg_de_mirage",
			"icontag": "bomb",
			"grouptype": "active",
			"icon_image_path": "map_icons/map_icon_de_mirage",
			"maps": {
				"de_mirage": ""
			}
		},
		"mg_de_mirage_scrimmagemap": {
			"imagename": "map-mirage-overall",
			"nameID": "#SFUI_Map_de_mirage_scrimmagemap",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Active_Op_over",
			"name": "mg_de_mirage_scrimmagemap",
			"icontag": "bomb",
			"grouptype": "reserves",
			"icon_image_path": "map_icons/map_icon_de_mirage",
			"maps": {
				"de_mirage_scrimmagemap": ""
			}
		},
		"mg_de_vertigo": {
			"imagename": "map-vertigo-overall",
			"nameID": "#SFUI_Map_de_vertigo",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Active_Op_over",
			"name": "mg_de_vertigo",
			"icontag": "bomb",
			"grouptype": "active",
			"icon_image_path": "map_icons/map_icon_de_vertigo",
			"maps": {
				"de_vertigo": ""
			}
		},
		"mg_cs_italy": {
			"imagename": "map-italy-overall",
			"nameID": "#SFUI_Map_cs_italy",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Hostage",
			"name": "mg_cs_italy",
			"icontag": "hostage",
			"grouptype": "hostage",
			"icon_image_path": "map_icons/map_icon_cs_italy",
			"maps": {
				"cs_italy": ""
			}
		},
		"mg_cs_office": {
			"imagename": "map-office-overall",
			"nameID": "#SFUI_Map_cs_office",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Hostage",
			"name": "mg_cs_office",
			"icontag": "hostage",
			"grouptype": "hostage",
			"icon_image_path": "map_icons/map_icon_cs_office",
			"maps": {
				"cs_office": ""
			}
		},
		"mg_cs_militia": {
			"imagename": "map-militia-overall",
			"nameID": "#SFUI_Map_cs_militia",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Hostage",
			"name": "mg_cs_militia",
			"icontag": "hostage",
			"grouptype": "reserves",
			"icon_image_path": "map_icons/map_icon_cs_militia",
			"maps": {
				"cs_militia": ""
			}
		},
		"mg_cs_assault": {
			"imagename": "map-assault-overall",
			"nameID": "#SFUI_Map_cs_assault",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Hostage",
			"name": "mg_cs_assault",
			"icontag": "hostage",
			"grouptype": "reserves",
			"icon_image_path": "map_icons/map_icon_cs_assault",
			"maps": {
				"cs_assault": ""
			}
		},
		"mg_de_overpass": {
			"imagename": "map-overpass-overall",
			"nameID": "#SFUI_Map_de_overpass",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Active_Op_over",
			"name": "mg_de_overpass",
			"icontag": "bomb",
			"grouptype": "active",
			"icon_image_path": "map_icons/map_icon_de_overpass",
			"maps": {
				"de_overpass": ""
			}
		},
		"mg_de_cbble": {
			"imagename": "map-cbble-overall",
			"nameID": "#SFUI_Map_de_cbble",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Reserves",
			"name": "mg_de_cbble",
			"icontag": "bomb",
			"grouptype": "reserves",
			"icon_image_path": "map_icons/map_icon_de_cbble",
			"maps": {
				"de_cbble": ""
			}
		},
		"mg_de_train": {
			"imagename": "map-train-overall",
			"nameID": "#SFUI_Map_de_train",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Reserves",
			"name": "mg_de_train",
			"icontag": "bomb",
			"grouptype": "reserves",
			"icon_image_path": "map_icons/map_icon_de_train",
			"maps": {
				"de_train": ""
			}
		},
		"mg_de_nuke": {
			"imagename": "map-nuke-overall",
			"nameID": "#SFUI_Map_de_nuke",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Active_Op_over",
			"name": "mg_de_nuke",
			"icontag": "bomb",
			"grouptype": "active",
			"icon_image_path": "map_icons/map_icon_de_nuke",
			"maps": {
				"de_nuke": ""
			}
		},
		"mg_gd_rialto": {
			"imagename": "map-rialto-overall",
			"nameID": "#SFUI_Map_gd_rialto",
			"name": "mg_gd_rialto",
			"icontag": "bomb",
			"icon_image_path": "map_icons/map_icon_gd_rialto",
			"maps": {
				"gd_rialto": ""
			}
		},
		"mg_gd_bank": {
			"imagename": "map-bank-overall",
			"nameID": "#SFUI_Map_gd_bank",
			"name": "mg_gd_bank",
			"icontag": "bomb",
			"icon_image_path": "map_icons/map_icon_gd_bank",
			"maps": {
				"gd_bank": ""
			}
		},
		"mg_gd_cbble": {
			"imagename": "map-cbble-overall",
			"nameID": "#SFUI_Map_gd_cbble",
			"name": "mg_gd_cbble",
			"icontag": "bomb",
			"icon_image_path": "map_icons/map_icon_de_cbble",
			"maps": {
				"gd_cbble": ""
			}
		},
		"mg_gd_lake": {
			"imagename": "map-boathouse-overall",
			"nameID": "#SFUI_Map_gd_lake",
			"name": "mg_gd_lake",
			"icon_image_path": "map_icons/map_icon_gd_lake",
			"maps": {
				"gd_lake": ""
			}
		},
		"mg_gd_sugarcane": {
			"imagename": "map-mill-overall",
			"nameID": "#SFUI_Map_gd_sugarcane",
			"name": "mg_gd_sugarcane",
			"icon_image_path": "map_icons/map_icon_gd_sugarcane",
			"maps": {
				"gd_sugarcane": ""
			}
		},
		"mg_gd_crashsite": {
			"imagename": "map-crashsite-overall",
			"nameID": "#SFUI_Map_gd_crashsite",
			"name": "mg_gd_crashsite",
			"icon_image_path": "map_icons/map_icon_gd_crashsite",
			"maps": {
				"gd_crashsite": ""
			}
		},
		"mg_gd_dizzy": {
			"imagename": "map-dizzy-overall",
			"nameID": "#SFUI_Map_ar_dizzy",
			"name": "mg_ar_dizzy",
			"icon_image_path": "map_icons/map_icon_ar_dizzy",
			"maps": {
				"gd_dizzy": ""
			}
		},
		"mg_coop_autumn": {
			"imagename": "map-dz_sirocco-overall",
			"nameID": "#SFUI_Map_coop_autumn",
			"name": "mg_coop_autumn",
			"icon_image_path": "map_icons/map_icon_coop_strike_map",
			"maps": {
				"coop_autumn": ""
			}
		},
		"mg_coop_fall": {
			"imagename": "map-dz_sirocco-overall",
			"nameID": "#SFUI_Map_coop_fall",
			"name": "mg_coop_fall",
			"icon_image_path": "map_icons/map_icon_coop_strike_map",
			"maps": {
				"coop_fall": ""
			}
		},
		"mg_coop_kasbah": {
			"imagename": "map-dz_sirocco-overall",
			"nameID": "#SFUI_Map_coop_kasbah",
			"name": "mg_coop_kasbah",
			"icon_image_path": "map_icons/map_icon_dz_sirocco",
			"maps": {
				"coop_kasbah": ""
			}
		},
		"mg_coop_cementplant": {
			"imagename": "map-cementplant-overall",
			"nameID": "#SFUI_Map_coop_cementplant",
			"name": "mg_coop_cementplant",
			"icon_image_path": "map_icons/map_icon_coop_cementplant",
			"maps": {
				"coop_cementplant": ""
			}
		},
		"mg_dz_frostbite": {
			"imagename": "map-blacksite-overall",
			"nameID": "#SFUI_Map_dz_frostbite",
			"name": "mg_dz_frostbite",
			"icon_image_path": "map_icons/map_icon_dz_frostbite",
			"maps": {
				"dz_frostbite": ""
			}
		},
		"mg_dz_junglety": {
			"imagename": "map-blacksite-overall",
			"nameID": "#SFUI_Map_dz_junglety",
			"name": "mg_dz_junglety",
			"icon_image_path": "map_icons/map_icon_dz_junglety",
			"maps": {
				"dz_junglety": ""
			}
		},
		"mg_dz_sirocco": {
			"imagename": "map-blacksite-overall",
			"nameID": "#SFUI_Map_dz_sirocco",
			"name": "mg_dz_sirocco",
			"icon_image_path": "map_icons/map_icon_dz_sirocco",
			"maps": {
				"dz_sirocco": ""
			}
		},
		"mg_dz_blacksite": {
			"imagename": "map-blacksite-overall",
			"nameID": "#SFUI_Map_dz_blacksite",
			"name": "mg_dz_blacksite",
			"icon_image_path": "map_icons/map_icon_dz_blacksite",
			"maps": {
				"dz_blacksite": ""
			}
		},
		"mg_cs_apollo": {
			"imagename": "map-apollo-overall",
			"nameID": "#SFUI_Map_cs_apollo",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Hostage",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_apollo",
			"authorID": "Vaya, Vorontsov, Andi, and Sad Ones",
			"name": "mg_cs_apollo",
			"icontag": "hostage",
			"grouptype": "reserves",
			"icon_image_path": "map_icons/map_icon_cs_apollo",
			"maps": {
				"cs_apollo": ""
			}
		},
		"mg_de_engage": {
			"imagename": "map-engage-overall",
			"nameID": "#SFUI_Map_de_engage",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Reserves",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_engage",
			"authorID": "catfood, BubkeZ, and RZL",
			"name": "mg_de_engage",
			"icontag": "bomb",
			"grouptype": "reserves",
			"icon_image_path": "map_icons/map_icon_de_engage",
			"maps": {
				"de_engage": ""
			}
		},
		"mg_de_guard": {
			"imagename": "map-guard-overall",
			"nameID": "#SFUI_Map_de_guard",
			"authorID": "poLemin",
			"name": "mg_de_guard",
			"icontag": "bomb",
			"icon_image_path": "map_icons/map_icon_de_guard",
			"maps": {
				"de_guard": ""
			}
		},
		"mg_de_elysion": {
			"imagename": "map-elysion-overall",
			"nameID": "#SFUI_Map_de_elysion",
			"authorID": "Big_SG21",
			"name": "mg_de_elysion",
			"icontag": "bomb",
			"icon_image_path": "map_icons/map_icon_de_elysion",
			"maps": {
				"de_elysion": ""
			}
		},
		"mg_dz_county": {
			"imagename": "map-blacksite-overall",
			"nameID": "#SFUI_Map_dz_county",
			"name": "mg_dz_county",
			"icon_image_path": "map_icons/map_icon_dz_county",
			"maps": {
				"dz_county": ""
			}
		},
		"mg_dz_vineyard": {
			"imagename": "map-vineyard-overall",
			"nameID": "#SFUI_Map_dz_vineyard",
			"name": "mg_dz_vineyard",
			"icon_image_path": "map_icons/map_icon_dz_vineyard",
			"maps": {
				"dz_vineyard": ""
			}
		},
		"mg_dz_ember": {
			"imagename": "map-ember-overall",
			"nameID": "#SFUI_Map_dz_ember",
			"name": "mg_dz_ember",
			"icon_image_path": "map_icons/map_icon_dz_ember",
			"maps": {
				"dz_ember": ""
			}
		},
		"mg_de_ravine": {
			"imagename": "map-ravine-overall",
			"nameID": "#SFUI_Map_de_ravine",
			"authorID": "jd40, Quoting, and Quadratic",
			"name": "mg_de_ravine",
			"icontag": "bomb",
			"icon_image_path": "map_icons/map_icon_de_ravine",
			"maps": {
				"de_ravine": ""
			}
		},
		"mg_de_extraction": {
			"imagename": "map-extraction-overall",
			"nameID": "#SFUI_Map_de_extraction",
			"authorID": "Fnugz, Andi, and MadsenFK",
			"name": "mg_de_extraction",
			"icontag": "bomb",
			"icon_image_path": "map_icons/map_icon_de_extraction",
			"maps": {
				"de_extraction": ""
			}
		},
		"mg_de_crete": {
			"imagename": "map-crete-overall",
			"nameID": "#SFUI_Map_de_crete",
			"authorID": "Quoting",
			"name": "mg_de_crete",
			"icontag": "bomb",
			"icon_image_path": "map_icons/map_icon_de_crete",
			"maps": {
				"de_crete": ""
			}
		},
		"mg_de_hive": {
			"imagename": "map-hive-overall",
			"nameID": "#SFUI_Map_de_hive",
			"authorID": "jakuza, Lizard, and celery",
			"name": "mg_de_hive",
			"icontag": "bomb",
			"icon_image_path": "map_icons/map_icon_de_hive",
			"maps": {
				"de_hive": ""
			}
		},
		"mg_de_prime": {
			"imagename": "map-prime-overall",
			"nameID": "#SFUI_Map_de_prime",
			"authorID": "-",
			"name": "mg_de_prime",
			"icontag": "bomb",
			"icon_image_path": "map_icons/map_icon_de_prime",
			"maps": {
				"de_prime": ""
			}
		},
		"mg_de_blagai": {
			"imagename": "map-blagai-overall",
			"nameID": "#SFUI_Map_de_blagai",
			"authorID": "-",
			"name": "mg_de_blagai",
			"icontag": "bomb",
			"icon_image_path": "map_icons/map_icon_de_blagai",
			"maps": {
				"de_blagai": ""
			}
		},
		"mg_de_boyard": {
			"imagename": "map-boyard-overall",
			"nameID": "#SFUI_Map_de_boyard",
			"authorID": "-",
			"name": "mg_de_boyard",
			"icontag": "bomb",
			"icon_image_path": "map_icons/map_icon_de_boyard",
			"maps": {
				"de_boyard": ""
			}
		},
		"mg_de_chalice": {
			"imagename": "map-chalice-overall",
			"nameID": "#SFUI_Map_de_chalice",
			"authorID": "-",
			"name": "mg_de_chalice",
			"icontag": "bomb",
			"icon_image_path": "map_icons/map_icon_de_chalice",
			"maps": {
				"de_chalice": ""
			}
		},
		"mg_cs_insertion2": {
			"imagename": "map-insertion2-overall",
			"nameID": "#SFUI_Map_cs_insertion2",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Hostage",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_insertion2",
			"authorID": "Oskmos",
			"name": "mg_cs_insertion2",
			"icontag": "hostage",
			"grouptype": "reserves",
			"icon_image_path": "map_icons/map_icon_cs_insertion2",
			"maps": {
				"cs_insertion2": ""
			}
		},
		"mg_cs_climb": {
			"imagename": "map-climb-overall",
			"nameID": "#SFUI_Map_cs_climb",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Hostage",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_climb",
			"authorID": "'RZL, Squidski, and Coachi",
			"name": "mg_cs_climb",
			"icontag": "hostage",
			"grouptype": "reserves",
			"icon_image_path": "map_icons/map_icon_cs_climb",
			"maps": {
				"cs_climb": ""
			}
		},
		"mg_de_basalt": {
			"imagename": "map-basalt-overall",
			"nameID": "#SFUI_Map_de_basalt",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Reserves",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_basalt",
			"authorID": "'RZL, Yanzl, and Oliver",
			"name": "mg_de_basalt",
			"icontag": "bomb",
			"grouptype": "reserves",
			"icon_image_path": "map_icons/map_icon_de_basalt",
			"maps": {
				"de_basalt": ""
			}
		},
		"mg_de_iris": {
			"imagename": "map-iris-overall",
			"nameID": "#SFUI_Map_de_iris",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Reserves",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_iris",
			"authorID": "BubkeZ and Oliver",
			"name": "mg_de_iris",
			"icontag": "bomb",
			"grouptype": "reserves",
			"icon_image_path": "map_icons/map_icon_de_iris",
			"maps": {
				"de_iris": ""
			}
		},
		"mg_de_calavera": {
			"imagename": "map-calavera-overall",
			"nameID": "#SFUI_Map_de_calavera",
			"authorID": "Squink and T-R3x3r",
			"name": "mg_de_calavera",
			"icontag": "bomb",
			"icon_image_path": "map_icons/map_icon_de_calavera",
			"maps": {
				"de_calavera": ""
			}
		},
		"mg_de_pitstop": {
			"imagename": "map-pitstop-overall",
			"nameID": "#SFUI_Map_de_pitstop",
			"authorID": "Quadratic and Quoting",
			"name": "mg_de_pitstop",
			"icontag": "bomb",
			"icon_image_path": "map_icons/map_icon_de_pitstop",
			"maps": {
				"de_pitstop": ""
			}
		},
		"mg_de_grind": {
			"imagename": "map-grind-overall",
			"nameID": "#SFUI_Map_de_grind",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Reserves",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_grind",
			"authorID": "The Horse Strangler, RZL, and MaanMan",
			"name": "mg_de_grind",
			"icontag": "bomb",
			"grouptype": "reserves",
			"icon_image_path": "map_icons/map_icon_de_grind",
			"maps": {
				"de_grind": ""
			}
		},
		"mg_de_mocha": {
			"imagename": "map-mocha-overall",
			"nameID": "#SFUI_Map_de_mocha",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Reserves",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_mocha",
			"authorID": "tr0nic and Bevster",
			"name": "mg_de_mocha",
			"icontag": "bomb",
			"grouptype": "reserves",
			"icon_image_path": "map_icons/map_icon_de_mocha",
			"maps": {
				"de_mocha": ""
			}
		},
		"mg_de_breach": {
			"imagename": "map-breach-overall",
			"nameID": "#SFUI_Map_de_breach",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Reserves",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_breach",
			"authorID": "Yanzl and Puddy",
			"name": "mg_de_breach",
			"icontag": "bomb",
			"grouptype": "reserves",
			"icon_image_path": "map_icons/map_icon_de_breach",
			"maps": {
				"de_breach": ""
			}
		},
		"mg_de_seaside": {
			"imagename": "map-seaside-overall",
			"nameID": "#SFUI_Map_de_seaside",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Reserves",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_seaside",
			"authorID": "Tanuki",
			"name": "mg_de_seaside",
			"icontag": "bomb",
			"grouptype": "reserves",
			"icon_image_path": "map_icons/map_icon_de_seaside",
			"maps": {
				"de_seaside": ""
			}
		},
		"mg_de_mutiny": {
			"imagename": "map-mutiny-overall",
			"nameID": "#SFUI_Map_de_mutiny",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Reserves",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_mutiny",
			"authorID": "-",
			"name": "mg_de_mutiny",
			"icontag": "bomb",
			"grouptype": "reserves",
			"icon_image_path": "map_icons/map_icon_de_mutiny",
			"maps": {
				"de_mutiny": ""
			}
		},
		"mg_de_swamp": {
			"imagename": "map-swamp-overall",
			"nameID": "#SFUI_Map_de_swamp",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Reserves",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_swamp",
			"authorID": "-",
			"name": "mg_de_swamp",
			"icontag": "bomb",
			"grouptype": "reserves",
			"icon_image_path": "map_icons/map_icon_de_swamp",
			"maps": {
				"de_swamp": ""
			}
		},
		"mg_de_ancient": {
			"imagename": "map-ancient-overall",
			"nameID": "#SFUI_Map_de_ancient",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Active_Op_over",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_ancient",
			"authorID": "-",
			"name": "mg_de_ancient",
			"icontag": "bomb",
			"grouptype": "active",
			"icon_image_path": "map_icons/map_icon_de_ancient",
			"maps": {
				"de_ancient": ""
			}
		},
		"mg_de_tuscan": {
			"imagename": "map-tuscan-overall",
			"nameID": "#SFUI_Map_de_tuscan",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Reserves",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_tuscan",
			"authorID": "-",
			"name": "mg_de_tuscan",
			"icontag": "bomb",
			"grouptype": "reserves",
			"icon_image_path": "map_icons/map_icon_de_tuscan",
			"maps": {
				"de_tuscan": ""
			}
		},
		"mg_de_anubis": {
			"imagename": "map-anubis-overall",
			"nameID": "#SFUI_Map_de_anubis",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Active_Op_over",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_anubis",
			"authorID": "-",
			"name": "mg_de_anubis",
			"showtagui": "new",
			"icontag": "bomb",
			"grouptype": "active",
			"icon_image_path": "map_icons/map_icon_de_anubis",
			"maps": {
				"de_anubis": ""
			}
		},
		"mg_de_chlorine": {
			"imagename": "map-chlorine-overall",
			"nameID": "#SFUI_Map_de_chlorine",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Reserves",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_chlorine",
			"authorID": "-",
			"name": "mg_de_chlorine",
			"icontag": "bomb",
			"grouptype": "reserves",
			"icon_image_path": "map_icons/map_icon_de_chlorine",
			"maps": {
				"de_chlorine": ""
			}
		},
		"mg_lobby_mapveto": {
			"imagename": "map-lobby-mapveto-overall",
			"nameID": "#SFUI_Map_lobby_mapveto",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_lobby_mapveto",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_lobby_mapveto",
			"authorID": "-",
			"name": "mg_lobby_mapveto",
			"icontag": "bomb",
			"competitivemod": "lobby",
			"grouptype": "active",
			"icon_image_path": "map_icons/map_icon_lobby_mapveto",
			"maps": {
				"lobby_mapveto": ""
			}
		},
		"mg_de_studio": {
			"imagename": "map-studio-overall",
			"nameID": "#SFUI_Map_de_studio",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Reserves",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_seaside",
			"authorID": "Tanuki",
			"name": "mg_de_studio",
			"icontag": "bomb",
			"grouptype": "reserves",
			"icon_image_path": "map_icons/map_icon_de_studio",
			"maps": {
				"de_studio": ""
			}
		},
		"mg_de_ruby": {
			"imagename": "map-ruby-overall",
			"nameID": "#SFUI_Map_de_ruby",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Reserves",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_ruby",
			"authorID": "catfood",
			"name": "mg_de_ruby",
			"icontag": "bomb",
			"grouptype": "reserves",
			"icon_image_path": "map_icons/map_icon_de_ruby",
			"maps": {
				"de_ruby": ""
			}
		},
		"mg_de_biome": {
			"imagename": "map-biome-overall",
			"nameID": "#SFUI_Map_de_biome",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Reserves",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_biome",
			"authorID": "JD40",
			"name": "mg_de_biome",
			"icontag": "bomb",
			"grouptype": "reserves",
			"icon_image_path": "map_icons/map_icon_de_biome",
			"maps": {
				"de_biome": ""
			}
		},
		"mg_de_subzero": {
			"imagename": "map-subzero-overall",
			"nameID": "#SFUI_Map_de_subzero",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Reserves",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_subzero",
			"authorID": "FMPONE, Tanuki, Connor",
			"name": "mg_de_subzero",
			"icontag": "bomb",
			"grouptype": "reserves",
			"icon_image_path": "map_icons/map_icon_de_subzero",
			"maps": {
				"de_subzero": ""
			}
		},
		"mg_de_abbey": {
			"imagename": "map-abbey-overall",
			"nameID": "#SFUI_Map_de_abbey",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Reserves",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_abbey",
			"authorID": "Lizard and thewhaleman",
			"name": "mg_de_abbey",
			"icontag": "bomb",
			"grouptype": "reserves",
			"icon_image_path": "map_icons/map_icon_de_abbey",
			"maps": {
				"de_abbey": ""
			}
		},
		"mg_cs_agency": {
			"imagename": "map-agency-overall",
			"nameID": "#SFUI_Map_cs_agency",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Hostage",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_agency",
			"authorID": "Puddy and Rick",
			"name": "mg_cs_agency",
			"icontag": "hostage",
			"grouptype": "reserves",
			"icon_image_path": "map_icons/map_icon_cs_agency",
			"maps": {
				"cs_agency": ""
			}
		},
		"mg_cs_insertion": {
			"imagename": "map-insertion-overall",
			"nameID": "#SFUI_Map_cs_insertion",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Hostage",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_insertion",
			"authorID": "Oskmos",
			"name": "mg_cs_insertion",
			"icontag": "hostage",
			"grouptype": "reserves",
			"icon_image_path": "map_icons/map_icon_cs_insertion",
			"maps": {
				"cs_insertion": ""
			}
		},
		"mg_de_blackgold": {
			"imagename": "map-blackgold-overall",
			"nameID": "#SFUI_Map_de_blackgold",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Operation",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_blackgold",
			"authorID": "The Horse Strangler, Az, HoliestCow",
			"name": "mg_de_blackgold",
			"icontag": "bomb",
			"show_medal_icon": "8Operation$OperationCoin",
			"show_season_icon": "season_7",
			"show_rich_presence": "_op08",
			"grouptype": "op_op08",
			"icon_image_path": "map_icons/map_icon_de_blackgold",
			"maps": {
				"de_blackgold": ""
			}
		},
		"mg_de_austria": {
			"imagename": "map-austria-overall",
			"nameID": "#SFUI_Map_de_austria",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Reserves",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_austria",
			"authorID": "Radix",
			"name": "mg_de_austria",
			"icontag": "bomb",
			"grouptype": "reserves",
			"icon_image_path": "map_icons/map_icon_de_austria",
			"maps": {
				"de_austria": ""
			}
		},
		"mg_de_lite": {
			"imagename": "map-lite-overall",
			"nameID": "#SFUI_Map_de_lite",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Operation",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_lite",
			"authorID": "ted",
			"name": "mg_de_lite",
			"icontag": "bomb",
			"show_medal_icon": "8Operation$OperationCoin",
			"show_season_icon": "season_7",
			"show_rich_presence": "_op08",
			"grouptype": "op_op08",
			"icon_image_path": "map_icons/map_icon_de_lite",
			"maps": {
				"de_lite": ""
			}
		},
		"mg_de_shipped": {
			"imagename": "map-shipped-overall",
			"nameID": "#SFUI_Map_de_shipped",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Reserves",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_shipped",
			"authorID": "catfood",
			"name": "mg_de_shipped",
			"icontag": "bomb",
			"grouptype": "reserves",
			"icon_image_path": "map_icons/map_icon_de_shipped",
			"maps": {
				"de_shipped": ""
			}
		},
		"mg_de_thrill": {
			"imagename": "map-thrill-overall",
			"nameID": "#SFUI_Map_de_thrill",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Operation",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_thrill",
			"authorID": "Yanzl, BubkeZ, Squad",
			"name": "mg_de_thrill",
			"icontag": "bomb",
			"show_medal_icon": "8Operation$OperationCoin",
			"show_season_icon": "season_7",
			"show_rich_presence": "_op08",
			"grouptype": "op_op08",
			"icon_image_path": "map_icons/map_icon_de_thrill",
			"maps": {
				"de_thrill": ""
			}
		},
		"mg_de_canals": {
			"imagename": "map-canals-overall",
			"nameID": "#SFUI_Map_de_canals",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Reserves",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_canals",
			"name": "mg_de_canals",
			"icontag": "bomb",
			"grouptype": "reserves",
			"icon_image_path": "map_icons/map_icon_de_canals",
			"maps": {
				"de_canals": ""
			}
		},
		"mg_cs_cruise": {
			"imagename": "map-cruise-overall",
			"nameID": "#SFUI_Map_cs_cruise",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Operation",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_cruise",
			"authorID": "Skybex and Yanzl",
			"name": "mg_cs_cruise",
			"icontag": "hostage",
			"show_medal_icon": "7Operation$OperationCoin",
			"show_season_icon": "season_6",
			"show_rich_presence": "_op07",
			"grouptype": "op_op07",
			"icon_image_path": "map_icons/map_icon_cs_cruise",
			"maps": {
				"cs_cruise": ""
			}
		},
		"mg_de_coast": {
			"imagename": "map-coast-overall",
			"nameID": "#SFUI_Map_de_coast",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Operation",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_coast",
			"authorID": "OrnateBaboon",
			"name": "mg_de_coast",
			"icontag": "bomb",
			"show_medal_icon": "7Operation$OperationCoin",
			"show_season_icon": "season_6",
			"show_rich_presence": "_op07",
			"grouptype": "op_op07",
			"icon_image_path": "map_icons/map_icon_de_coast",
			"maps": {
				"de_coast": ""
			}
		},
		"mg_de_empire": {
			"imagename": "map-empire-overall",
			"nameID": "#SFUI_Map_de_empire",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Operation",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_empire",
			"authorID": "Andre Valera, Hordeau, waLtz and Lt.Dan",
			"name": "mg_de_empire",
			"icontag": "bomb",
			"show_medal_icon": "7Operation$OperationCoin",
			"show_season_icon": "season_6",
			"show_rich_presence": "_op07",
			"grouptype": "op_op07",
			"icon_image_path": "map_icons/map_icon_de_empire",
			"maps": {
				"de_empire": ""
			}
		},
		"mg_de_mikla": {
			"imagename": "map-mikla-overall",
			"nameID": "#SFUI_Map_de_mikla",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Operation",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_mikla",
			"authorID": "dr_pretzel and Rick",
			"name": "mg_de_mikla",
			"icontag": "bomb",
			"show_medal_icon": "7Operation$OperationCoin",
			"show_season_icon": "season_6",
			"show_rich_presence": "_op07",
			"grouptype": "op_op07",
			"icon_image_path": "map_icons/map_icon_de_mikla",
			"maps": {
				"de_mikla": ""
			}
		},
		"mg_de_royal": {
			"imagename": "map-royal-overall",
			"nameID": "#SFUI_Map_de_royal",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Operation",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_royal",
			"authorID": "jakuza",
			"name": "mg_de_royal",
			"icontag": "bomb",
			"show_medal_icon": "7Operation$OperationCoin",
			"show_season_icon": "season_6",
			"show_rich_presence": "_op07",
			"grouptype": "op_op07",
			"icon_image_path": "map_icons/map_icon_de_royal",
			"maps": {
				"de_royal": ""
			}
		},
		"mg_de_santorini": {
			"imagename": "map-santorini-overall",
			"nameID": "#SFUI_Map_de_santorini",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Operation",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_santorini",
			"authorID": "FMPONE, Hordeau, Dreamsane and Rf",
			"name": "mg_de_santorini",
			"icontag": "bomb",
			"show_medal_icon": "7Operation$OperationCoin",
			"show_season_icon": "season_6",
			"show_rich_presence": "_op07",
			"grouptype": "op_op07",
			"icon_image_path": "map_icons/map_icon_de_santorini",
			"maps": {
				"de_santorini": ""
			}
		},
		"mg_de_tulip": {
			"imagename": "map-tulip-overall",
			"nameID": "#SFUI_Map_de_tulip",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Operation",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_tulip",
			"authorID": "catfood",
			"name": "mg_de_tulip",
			"icontag": "bomb",
			"show_medal_icon": "7Operation$OperationCoin",
			"show_season_icon": "season_6",
			"show_rich_presence": "_op07",
			"grouptype": "op_op07",
			"icon_image_path": "map_icons/map_icon_de_tulip",
			"maps": {
				"de_tulip": ""
			}
		},
		"mg_de_rails": {
			"imagename": "map-rails-overall",
			"nameID": "#SFUI_Map_de_rails",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Operation",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_rails",
			"authorID": "Deh0lise",
			"name": "mg_de_rails",
			"icontag": "bomb",
			"show_medal_icon": "6Operation$OperationCoin",
			"show_season_icon": "season_5",
			"show_rich_presence": "_op06",
			"grouptype": "op_op06",
			"icon_image_path": "map_icons/map_icon_de_rails",
			"maps": {
				"de_rails": ""
			}
		},
		"mg_de_resort": {
			"imagename": "map-resort-overall",
			"nameID": "#SFUI_Map_de_resort",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Operation",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_resort",
			"authorID": "'RZL and Yanzl",
			"name": "mg_de_resort",
			"icontag": "bomb",
			"show_medal_icon": "6Operation$OperationCoin",
			"show_season_icon": "season_5",
			"show_rich_presence": "_op06",
			"grouptype": "op_op06",
			"icon_image_path": "map_icons/map_icon_de_resort",
			"maps": {
				"de_resort": ""
			}
		},
		"mg_de_zoo": {
			"imagename": "map-zoo-overall",
			"nameID": "#SFUI_Map_de_zoo",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Reserves",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_zoo",
			"authorID": "Squad and Yanzl",
			"name": "mg_de_zoo",
			"icontag": "bomb",
			"grouptype": "reserves",
			"icon_image_path": "map_icons/map_icon_de_zoo",
			"maps": {
				"de_zoo": ""
			}
		},
		"mg_de_log": {
			"imagename": "map-log-overall",
			"nameID": "#SFUI_Map_de_log",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Operation",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_log",
			"authorID": "catfood",
			"name": "mg_de_log",
			"icontag": "bomb",
			"show_medal_icon": "6Operation$OperationCoin",
			"show_season_icon": "season_5",
			"show_rich_presence": "_op06",
			"grouptype": "op_op06",
			"icon_image_path": "map_icons/map_icon_de_log",
			"maps": {
				"de_log": ""
			}
		},
		"mg_de_season": {
			"imagename": "map-season-overall",
			"nameID": "#SFUI_Map_de_season",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Operation",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_season",
			"authorID": "ted and FMPONE",
			"name": "mg_de_season",
			"icontag": "bomb",
			"show_medal_icon": "6Operation$OperationCoin",
			"show_season_icon": "season_5",
			"show_rich_presence": "_op06",
			"grouptype": "op_op06",
			"icon_image_path": "map_icons/map_icon_de_season",
			"maps": {
				"de_season": ""
			}
		},
		"mg_cs_workout": {
			"imagename": "map-workout-overall",
			"nameID": "#SFUI_Map_cs_workout",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Hostage",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_workout",
			"authorID": "Skybex",
			"name": "mg_cs_workout",
			"icontag": "hostage",
			"grouptype": "reserves",
			"icon_image_path": "map_icons/map_icon_cs_workout",
			"maps": {
				"cs_workout": ""
			}
		},
		"mg_cs_backalley": {
			"imagename": "map-backalley-overall",
			"nameID": "#SFUI_Map_cs_backalley",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Operation",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_backalley",
			"authorID": "H.Grunt",
			"name": "mg_cs_backalley",
			"icontag": "hostage",
			"show_medal_icon": "5Operation$Community Season Five Summer 2014",
			"show_season_icon": "season_4",
			"show_rich_presence": "_vanguard",
			"grouptype": "op_op05",
			"icon_image_path": "map_icons/map_icon_cs_backalley",
			"maps": {
				"cs_backalley": ""
			}
		},
		"mg_de_marquis": {
			"imagename": "map-marquis-overall",
			"nameID": "#SFUI_Map_de_marquis",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Operation",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_marquis",
			"authorID": "Kane and DamDam",
			"name": "mg_de_marquis",
			"icontag": "bomb",
			"show_medal_icon": "5Operation$Community Season Five Summer 2014",
			"show_season_icon": "season_4",
			"show_rich_presence": "_vanguard",
			"grouptype": "op_op05",
			"icon_image_path": "map_icons/map_icon_de_marquis",
			"maps": {
				"de_marquis": ""
			}
		},
		"mg_de_facade": {
			"imagename": "map-facade-overall",
			"nameID": "#SFUI_Map_de_facade",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Operation",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_facade",
			"authorID": "TopHATTwaffle and maxgiddens",
			"name": "mg_de_facade",
			"icontag": "bomb",
			"show_medal_icon": "5Operation$Community Season Five Summer 2014",
			"show_season_icon": "season_4",
			"show_rich_presence": "_vanguard",
			"grouptype": "op_op05",
			"icon_image_path": "map_icons/map_icon_de_facade",
			"maps": {
				"de_facade": ""
			}
		},
		"mg_de_bazaar": {
			"imagename": "map-bazaar-overall",
			"nameID": "#SFUI_Map_de_bazaar",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Operation",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_bazaar",
			"authorID": "Skybex",
			"name": "mg_de_bazaar",
			"icontag": "bomb",
			"show_medal_icon": "5Operation$Community Season Five Summer 2014",
			"show_season_icon": "season_4",
			"show_rich_presence": "_vanguard",
			"grouptype": "op_op05",
			"icon_image_path": "map_icons/map_icon_de_bazaar",
			"maps": {
				"de_bazaar": ""
			}
		},
		"mg_de_castle": {
			"imagename": "map-castle-overall",
			"nameID": "#SFUI_Map_de_castle",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Operation",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_castle",
			"authorID": "Yanzl",
			"name": "mg_de_castle",
			"icontag": "bomb",
			"show_medal_icon": "4OpBreakout$Community Season Four Summer 2014",
			"show_season_icon": "season_3",
			"grouptype": "op_breakout",
			"icon_image_path": "map_icons/map_icon_",
			"maps": {
				"de_castle": ""
			}
		},
		"mg_de_overgrown": {
			"imagename": "map-overgrown-overall",
			"nameID": "#SFUI_Map_de_overgrown",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Operation",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_overgrown",
			"authorID": "Psy",
			"name": "mg_de_overgrown",
			"icontag": "bomb",
			"show_medal_icon": "4OpBreakout$Community Season Four Summer 2014",
			"show_season_icon": "season_3",
			"grouptype": "op_breakout",
			"icon_image_path": "map_icons/map_icon_de_overgrown",
			"maps": {
				"de_overgrown": ""
			}
		},
		"mg_cs_rush": {
			"imagename": "map-rush-overall",
			"nameID": "#SFUI_Map_cs_rush",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Operation",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_rush",
			"authorID": "Invalid nick",
			"name": "mg_cs_rush",
			"icontag": "hostage",
			"show_medal_icon": "4OpBreakout$Community Season Four Summer 2014",
			"show_season_icon": "season_3",
			"grouptype": "op_breakout",
			"icon_image_path": "map_icons/map_icon_cs_rush",
			"maps": {
				"cs_rush": ""
			}
		},
		"mg_de_mist": {
			"imagename": "map-mist-overall",
			"nameID": "#SFUI_Map_de_mist",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Operation",
			"descriptionID": "#SFUI_Map_Tooltip_Desc_mist",
			"authorID": "Invalid nick",
			"name": "mg_de_mist",
			"icontag": "bomb",
			"show_medal_icon": "4OpBreakout$Community Season Four Summer 2014",
			"show_season_icon": "season_3",
			"grouptype": "op_breakout",
			"icon_image_path": "map_icons/map_icon_de_mist",
			"maps": {
				"de_mist": ""
			}
		},
		"mg_de_cache": {
			"imagename": "map-cache-overall",
			"nameID": "#SFUI_Map_de_cache",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Reserves",
			"name": "mg_de_cache",
			"icontag": "bomb",
			"grouptype": "reserves",
			"icon_image_path": "map_icons/map_icon_de_cache",
			"maps": {
				"de_cache": ""
			}
		},
		"mg_de_cache_scrimmagemap": {
			"imagename": "map-cache-overall",
			"nameID": "#SFUI_Map_de_cache_scrimmagemap",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Reserves",
			"name": "mg_de_cache_scrimmagemap",
			"icontag": "bomb",
			"grouptype": "reserves",
			"icon_image_path": "map_icons/map_icon_de_cache",
			"maps": {
				"de_cache_scrimmagemap": ""
			}
		},
		"mg_de_ali": {
			"imagename": "map-ali-overall",
			"nameID": "#SFUI_Map_de_ali",
			"name": "mg_de_ali",
			"icontag": "bomb",
			"show_season_icon": "season_2",
			"icon_image_path": "map_icons/map_icon_de_ali",
			"maps": {
				"de_ali": ""
			}
		},
		"mg_cs_thunder": {
			"imagename": "map-thunder-overall",
			"nameID": "#SFUI_Map_cs_thunder",
			"name": "mg_cs_thunder",
			"icontag": "hostage",
			"show_season_icon": "season_2",
			"icon_image_path": "map_icons/map_icon_cs_thunder",
			"maps": {
				"cs_thunder": ""
			}
		},
		"mg_de_favela": {
			"imagename": "map-favela-overall",
			"nameID": "#SFUI_Map_de_favela",
			"name": "mg_de_favela",
			"icontag": "bomb",
			"show_season_icon": "season_2",
			"icon_image_path": "map_icons/map_icon_de_favela",
			"maps": {
				"de_favela": ""
			}
		},
		"mg_cs_downtown": {
			"imagename": "map-downtown-overall",
			"nameID": "#SFUI_Map_cs_downtown",
			"name": "mg_cs_downtown",
			"icontag": "hostage",
			"show_season_icon": "season_2",
			"icon_image_path": "map_icons/map_icon_cs_downtown",
			"maps": {
				"cs_downtown": ""
			}
		},
		"mg_cs_motel": {
			"imagename": "map-motel-overall",
			"nameID": "#SFUI_Map_cs_motel",
			"name": "mg_cs_motel",
			"icontag": "hostage",
			"show_season_icon": "season_2",
			"icon_image_path": "map_icons/map_icon_cs_motel",
			"maps": {
				"cs_motel": ""
			}
		},
		"mg_de_gwalior": {
			"imagename": "map-gwalior-overall",
			"nameID": "#SFUI_Map_de_gwalior",
			"name": "mg_de_gwalior",
			"icontag": "bomb",
			"show_season_icon": "season_1",
			"icon_image_path": "map_icons/map_icon_de_gwalior",
			"maps": {
				"de_gwalior": ""
			}
		},
		"mg_de_chinatown": {
			"imagename": "map-chinatown-overall",
			"nameID": "#SFUI_Map_de_chinatown",
			"name": "mg_de_chinatown",
			"icontag": "bomb",
			"show_season_icon": "season_1",
			"icon_image_path": "map_icons/map_icon_de_chinatown",
			"maps": {
				"de_chinatown": ""
			}
		},
		"mg_cs_siege": {
			"imagename": "map-siege-overall",
			"nameID": "#SFUI_Map_cs_siege",
			"name": "mg_cs_siege",
			"icontag": "hostage",
			"show_season_icon": "season_1",
			"icon_image_path": "map_icons/map_icon_cs_siege",
			"maps": {
				"cs_siege": ""
			}
		},
		"mg_cs_museum": {
			"imagename": "map-museum-overall",
			"nameID": "#SFUI_Map_cs_museum",
			"name": "mg_cs_museum",
			"icontag": "hostage",
			"show_season_icon": "season_1",
			"icon_image_path": "map_icons/map_icon_cs_museum",
			"maps": {
				"cs_museum": ""
			}
		},
		"mg_de_library": {
			"imagename": "map-library-overall",
			"nameID": "#SFUI_Map_de_library",
			"name": "mg_de_library",
			"icontag": "bomb",
			"show_season_icon": "season_1",
			"icon_image_path": "map_icons/map_icon_de_library",
			"maps": {
				"de_library": ""
			}
		},
		"mg_de_ruins": {
			"imagename": "map-ruins-overall",
			"nameID": "#SFUI_Map_de_ruins",
			"name": "mg_de_ruins",
			"icontag": "bomb",
			"show_season_icon": "season_1",
			"icon_image_path": "map_icons/map_icon_de_ruins",
			"maps": {
				"de_ruins": ""
			}
		},
		"mg_ar_baggage": {
			"imagename": "map-baggage-overall",
			"nameID": "#SFUI_Map_ar_baggage",
			"name": "mg_ar_baggage",
			"icon_image_path": "map_icons/map_icon_ar_baggage",
			"maps": {
				"ar_baggage": ""
			}
		},
		"mg_ar_shoots": {
			"imagename": "map-vietnam-overall",
			"nameID": "#SFUI_Map_ar_shoots",
			"name": "mg_ar_shoots",
			"icon_image_path": "map_icons/map_icon_ar_shoots",
			"maps": {
				"ar_shoots": ""
			}
		},
		"mg_ar_lunacy": {
			"imagename": "map-lunacy-overall",
			"nameID": "#SFUI_Map_ar_lunacy",
			"name": "mg_ar_lunacy",
			"icon_image_path": "map_icons/map_icon_ar_lunacy",
			"maps": {
				"ar_lunacy": ""
			}
		},
		"mg_gd_lunacy": {
			"imagename": "map-lunacy-overall",
			"nameID": "#SFUI_Map_gd_lunacy",
			"name": "mg_gd_lunacy",
			"icon_image_path": "map_icons/map_icon_ar_lunacy",
			"maps": {
				"gd_lunacy": ""
			}
		},
		"mg_ar_dizzy": {
			"imagename": "map-dizzy-overall",
			"nameID": "#SFUI_Map_ar_dizzy",
			"name": "mg_ar_dizzy",
			"icon_image_path": "map_icons/map_icon_ar_dizzy",
			"maps": {
				"ar_dizzy": ""
			}
		},
		"mg_ar_monastery": {
			"imagename": "map-monastery-overall",
			"nameID": "#SFUI_Map_ar_monastery",
			"name": "mg_ar_monastery",
			"icon_image_path": "map_icons/map_icon_ar_monastery",
			"maps": {
				"ar_monastery": ""
			}
		},
		"mg_ar_lake": {
			"imagename": "map-boathouse-overall",
			"nameID": "#SFUI_Map_de_lake",
			"name": "mg_ar_lake",
			"icon_image_path": "map_icons/map_icon_ar_lake",
			"maps": {
				"de_lake": ""
			}
		},
		"mg_ar_stmarc": {
			"imagename": "map-shacks-overall",
			"nameID": "#SFUI_Map_de_stmarc",
			"name": "mg_ar_stmarc",
			"icon_image_path": "map_icons/map_icon_ar_stmarc",
			"maps": {
				"de_stmarc": ""
			}
		},
		"mg_ar_safehouse": {
			"imagename": "map-house-overall",
			"nameID": "#SFUI_Map_de_safehouse",
			"name": "mg_ar_safehouse",
			"icon_image_path": "map_icons/map_icon_ar_safehouse",
			"maps": {
				"de_safehouse": ""
			}
		},
		"mg_de_bank": {
			"imagename": "map-bank-overall",
			"nameID": "#SFUI_Map_de_bank",
			"name": "mg_de_bank",
			"icontag": "bomb",
			"icon_image_path": "map_icons/map_icon_de_bank",
			"maps": {
				"de_bank": ""
			}
		},
		"mg_de_lake": {
			"imagename": "map-boathouse-overall",
			"nameID": "#SFUI_Map_de_lake",
			"name": "mg_de_lake",
			"icontag": "bomb",
			"icon_image_path": "map_icons/map_icon_de_lake",
			"maps": {
				"de_lake": ""
			}
		},
		"mg_de_safehouse": {
			"imagename": "map-house-overall",
			"nameID": "#SFUI_Map_de_safehouse",
			"name": "mg_de_safehouse",
			"icontag": "bomb",
			"icon_image_path": "map_icons/map_icon_de_safehouse",
			"maps": {
				"de_safehouse": ""
			}
		},
		"mg_de_sugarcane": {
			"imagename": "map-mill-overall",
			"nameID": "#SFUI_Map_de_sugarcane",
			"name": "mg_de_sugarcane",
			"icon_image_path": "map_icons/map_icon_de_sugarcane",
			"icontag": "bomb",
			"maps": {
				"de_sugarcane": ""
			}
		},
		"mg_de_stmarc": {
			"imagename": "map-shacks-overall",
			"nameID": "#SFUI_Map_de_stmarc",
			"name": "mg_de_stmarc",
			"icontag": "bomb",
			"icon_image_path": "map_icons/map_icon_de_stmarc",
			"maps": {
				"de_stmarc": ""
			}
		},
		"mg_de_shorttrain": {
			"imagename": "map-train-overall",
			"nameID": "#SFUI_Map_de_shorttrain",
			"name": "mg_de_shorttrain",
			"icontag": "bomb",
			"icon_image_path": "map_icons/map_icon_de_shorttrain",
			"maps": {
				"de_shorttrain": ""
			}
		},
		"mg_de_shortdust": {
			"imagename": "map-dust-overall",
			"nameID": "#SFUI_Map_de_shortdust",
			"name": "mg_de_shortdust",
			"icontag": "bomb",
			"icon_image_path": "map_icons/map_icon_de_shortdust",
			"maps": {
				"de_shortdust": ""
			}
		},
		"mg_de_shortnuke": {
			"imagename": "map-nuke-overall",
			"nameID": "#SFUI_Map_de_nuke",
			"tooltipID": "#SFUI_Map_Tooltip_Desc_Active_Op_over",
			"name": "mg_de_shortnuke",
			"icontag": "bomb",
			"grouptype": "active",
			"icon_image_path": "map_icons/map_icon_de_nuke",
			"maps": {
				"de_shortnuke": ""
			}
		},
		"mg_training1": {
			"imagename": "map-alleyway-overall",
			"nameID": "#SFUI_Map_training1",
			"name": "mg_training1",
			"maps": {
				"training1": ""
			}
		},
		"random_ar": {
			"nameID": "#SFUI_Map_random",
			"imagename": "map-random-ar",
			"name": "random",
			"icon_image_path": "map_icons/mapgroup_icon_random",
			"maps": {
			}
		},
		"random_demo": {
			"nameID": "#SFUI_Map_random",
			"imagename": "map-random-demo",
			"name": "random",
			"icon_image_path": "map_icons/mapgroup_icon_random",
			"maps": {
			}
		},
		"random_classic": {
			"nameID": "#SFUI_Map_random",
			"imagename": "map-random-overall",
			"name": "random",
			"icon_image_path": "map_icons/mapgroup_icon_random",
			"maps": {
			}
		}
	};

	g.SE_GAME_TYPES_CONFIG = { gameTypes: types, mapgroups: mapgroups };
})();
