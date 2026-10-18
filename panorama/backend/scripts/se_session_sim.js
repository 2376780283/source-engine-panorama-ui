"use strict";
// SE port - 大厅 (lobby) / 匹配 (matchmaking) / 排位 (rank) 数据层.
//
// WHY THIS FILE EXISTS
// --------------------
// CS:GO's panorama scripts talk to a cstrike15 *game client DLL* for everything session related:
// it owns the lobby (Steam lobby equivalent), the matchmaking queue, the skill group / rank data
// and the game mode table.  This port has no Steam, no GC and no cstrike15 DLL, so those objects
// were answered by the auto-stub in se_api_shim.js: every call returned a *truthy object*, which
// is not a neutral value (see docs/csgo_panorama_port_pitfalls.md P66) - mainmenu_play.js then
// built its dialogs from placeholders and the queue/lobby UI never lit up.
//
// This file replaces those placeholders with a small, *stateful* simulation:
//
//     no session ──CreateSession()/邀请好友──▶ 大厅里（本地玩家是房主）
//           ▲                                        │ StartMatchmaking()
//           └────────CloseSession()/停止/离开────────┤
//                                                    ▼
//                                          搜索中（计时 + 状态字符串）
//
// Data sources:
//   * game modes / map groups: the game's own gamemodes.txt, converted to SE_GAME_TYPES_CONFIG by
//     build/_gen_gametypes.ps1 (se_gametypes.js) - so the mode list is the real one.
//   * rank / level / friends / lobby members: simulated (there is no Steam profile in this build),
//     but answered with *self consistent* numbers so the rank UI ('排位') has something to draw.
//
// Everything is read by the content through the API objects below, and every change dispatches the
// events the scripts listen for (PanoramaComponent_Lobby_MatchmakingSessionUpdate, ...) - the
// scripts then re-read the APIs, so no other notification path is needed.
//
// Debug hook (console / test scripts): SE_SESSION_SIM on the global object.

(function () {
	var g = Function("return this")();

	// Feature switches - flip one to false to fall back to the shim's "no data" answer for that area.
	var OPT = {
		lobby: true,        // 大厅: sessions, members, invites
		matchmaking: true,  // 匹配: StartMatchmaking / status string / elapsed time
		rank: true,         // 排位: skill group + wins + level
		friends: true       // simulated friend profiles (name / rank / prime)
	};
	g.SE_SESSION_SIM_OPTIONS = OPT;

	var cfg = g.SE_GAME_TYPES_CONFIG || { gameTypes: {}, mapgroups: {} };

	// ---------------------------------------------------------------------------------------------
	// 玩家数据 (simulated)
	// ---------------------------------------------------------------------------------------------
	var LOCAL_XUID = "76561198000000001";
	var LOCAL_ACCOUNT_ID = 12345678;

	// skillGroup is the CS:GO competitive rank 1..18 (1 = Silver I, 18 = Global Elite); wins/level are
	// the numbers the player card prints next to it.
	var PLAYERS = {};
	PLAYERS[LOCAL_XUID] = { xuid: LOCAL_XUID, name: "本地玩家", level: 12, xp: 3200,
		skillGroup: 11, wins: 42, prime: true, relationship: "self", talking: false };
	PLAYERS["76561198000000002"] = { xuid: "76561198000000002", name: "队友·雷达", level: 8, xp: 900,
		/* 已让好友状态带玩游戏标记 */
		skillGroup: 13, wins: 87, prime: true, relationship: "friend", talking: false, ingame: true };
	PLAYERS["76561198000000003"] = { xuid: "76561198000000003", name: "队友·闪光", level: 5, xp: 400,
		skillGroup: 8, wins: 31, prime: false, relationship: "friend", talking: false, ingame: false };
	PLAYERS["76561198000000004"] = { xuid: "76561198000000004", name: "队友·烟雾", level: 21, xp: 5100,
		skillGroup: 16, wins: 120, prime: true, relationship: "friend", talking: false, ingame: true };
	// 两个陌生人: 侧边栏“广播”页签（looking to play）上不全是好友才像真的
	PLAYERS["76561198000000005"] = { xuid: "76561198000000005", name: "路人·老枪", level: 27, xp: 6800,
		skillGroup: 14, wins: 260, prime: true, relationship: "friend", talking: false, ingame: true };
	PLAYERS["76561198000000006"] = { xuid: "76561198000000006", name: "路人·萌新", level: 3, xp: 150,
		skillGroup: 3, wins: 4, prime: false, relationship: "friend", talking: false, ingame: false };
	var FRIEND_XUIDS = ["76561198000000002", "76561198000000003", "76561198000000004"];

	// 大厅列表 (the friends panel's 广播 tab, PartyBrowserAPI): friendslist.js builds one
	// friend_advertise_tile per GetXuidByIndex(i), and each tile's 邀请 button calls
	// FriendsListAPI.ActionInviteFriend - this list is one of the entries into the lobby UI
	// (the other one is the friends tab's player-card context menu, and a third is an incoming
	// invite - see seedIncomingInvite() at the end of this file).
	var ADVERTISED = [
		{ xuid: "76561198000000002", mode: "competitive", prime: "1", loc: "cn", rank: 13 },
		{ xuid: "76561198000000005", mode: "competitive", prime: "1", loc: "cn", rank: 14 },
		{ xuid: "76561198000000003", mode: "scrimcomp2v2", prime: "0", loc: "se", rank: 8 },
		{ xuid: "76561198000000004", mode: "competitive", prime: "1", loc: "br", rank: 16 }
	];

	function player(xuid) { return PLAYERS[xuid] || null; }
	function playerName(xuid) {
		var p = player(xuid);
		if (p) { return p.name; }
		if (!xuid || xuid === "0" || xuid === 0) { return ""; }
		return "玩家 " + String(xuid).substring(Math.max(0, String(xuid).length - 4));
	}

	// ---------------------------------------------------------------------------------------------
	// 游戏模式表 (real data, from gamemodes.txt)
	// ---------------------------------------------------------------------------------------------
	function modeConfig(mode) {
		for (var type in cfg.gameTypes) {
			var modes = cfg.gameTypes[type].gameModes;
			if (modes && Object.prototype.hasOwnProperty.call(modes, mode)) { return modes[mode]; }
		}
		return null;
	}

	function modeType(mode) {
		for (var type in cfg.gameTypes) {
			var modes = cfg.gameTypes[type].gameModes;
			if (modes && Object.prototype.hasOwnProperty.call(modes, mode)) { return type; }
		}
		return "";
	}

	function mapGroupKeys(mode, bOfficial) {
		var mcfg = modeConfig(mode);
		var groups = mcfg ? (bOfficial ? mcfg.mapgroupsMP : mcfg.mapgroupsSP) : null;
		return groups ? Object.keys(groups) : [];
	}

	function modeMaxPlayers(mode) {
		var mcfg = modeConfig(mode);
		var n = mcfg && mcfg.maxplayers ? parseInt(mcfg.maxplayers) : 10;
		return (isNaN(n) || n <= 0) ? 10 : n;
	}

	// CS:GO fills a lobby up to half the mode's player count (5 for competitive: 5v5),
	// except for the small modes where maxplayers is already the team based number.
	function lobbySlots(mode) {
		var n = modeMaxPlayers(mode);
		return Math.max(1, Math.floor(n / 2));
	}

	// ---------------------------------------------------------------------------------------------
	// 会话状态 (the lobby)
	// ---------------------------------------------------------------------------------------------
	var DEFAULT_MODE = "competitive";

	var S = {
		active: false,
		host: true,
		hostXuid: LOCAL_XUID,
		settings: null,        // { game:{mode,mapgroupname,prime,gamemodeflags,questid}, system:{access}, members:[] }
		members: [],           // [ { xuid, name, level, skillGroup, wins, prime, ready, color } ]
		queue: {
			searching: false,
			startedAt: 0,
			status: "",
			lastStart: null
		},
		invites: [],           // 收到的邀请: xuids of friends who invited the local player into theirs
		invited: {},           // 发出的邀请: xuid -> true (drives the friend tile's "invited" state)
		inviteSeededAt: 0,     // throttle for the demo invite, see seedIncomingInvite() at the end
		searchFilter: "all",
		forHire: false,
		forHireMode: ""
	};

	function nowSeconds() { return Date.now() / 1000.0; }

	// ---------------------------------------------------------------------------------------------
	// 跨布局共享状态
	//
	// Panorama gives every layout its own JavaScript context, so the lobby state has to live outside
	// of this file: it is kept in an engine ConVar (GameInterfaceAPI.SetSettingString creates it on
	// first write, see uicomponent_gameinterface.cpp) as JSON.  The play page, the matchmaking status
	// panel and the party sidebar then all read the same state - without this, pressing 开始 in the
	// play page left the status panel (a different layout) believing nothing was running.
	// ---------------------------------------------------------------------------------------------
	var STORE_KEY = "se_session_state";
	var m_strLastRaw = null;

	function readStore() {
		try { return g.GameInterfaceAPI.GetSettingString(STORE_KEY) || ""; } catch (e) { return ""; }
	}

	function syncFromStore() {
		var raw = readStore();
		if (!raw || raw === m_strLastRaw) { return false; }

		m_strLastRaw = raw;
		try {
			var o = JSON.parse(raw);
			S.active = !!o.active;
			// "host" is part of the shared state since a lobby can be joined from an invite:
			// the play page locks its mode / map dialogs on BIsHost() and the sidebar only offers
			// 取消搜索 to the host.
			S.host = (o.host === undefined) ? true : !!o.host;
			if (o.hostXuid) { S.hostXuid = o.hostXuid; }
			if (o.settings) { S.settings = o.settings; }
			S.members = o.members || [];
			S.queue.searching = !!o.searching;
			S.queue.startedAt = o.startedAt || 0;
			S.queue.status = o.status || "";
			S.invites = o.invites || [];
			S.invited = o.invited || {};
			S.inviteSeededAt = o.inviteSeededAt || 0;
			return true;
		} catch (e) {
			return false;
		}
	}

	function saveStore() {
		var raw = JSON.stringify({
			active: S.active,
			host: S.host,
			hostXuid: S.hostXuid,
			settings: S.settings,
			members: S.members,
			searching: S.queue.searching,
			startedAt: S.queue.startedAt,
			status: S.queue.status,
			invites: S.invites,
			invited: S.invited,
			inviteSeededAt: S.inviteSeededAt
		});
		m_strLastRaw = raw;
		try { g.GameInterfaceAPI.SetSettingString(STORE_KEY, raw); } catch (e) { }
	}

	function defaultSettings(mode, bOfficial) {
		mode = mode || DEFAULT_MODE;
		var groups = mapGroupKeys(mode, bOfficial !== false);
		return {
			game: {
				mode: mode,
				mapgroupname: groups.length ? groups.slice(0, Math.min(groups.length, 2)).join(",") : "",
				prime: "1",
				gamemodeflags: "0",
				questid: "",
				clanid: ""
			},
			system: {
				access: "public"
			},
			// mainmenu_play.js::_SyncDialogsFromSessionSettings reads options.server ("official" |
			// "listen" | ...) - without it the play page aborts with '"server" of undefined'.
			options: {
				server: "official",
				challengekey: ""
			},
			members: []
		};
	}

	function makeMember(xuid) {
		var p = player(xuid) || { xuid: xuid, name: playerName(xuid), level: 1, skillGroup: 0, wins: 0, prime: false };
		return {
			xuid: xuid,
			name: p.name,
			level: p.level,
			skillGroup: p.skillGroup,
			wins: p.wins,
			prime: p.prime,
			ready: false,
			color: 1 + (S.members.length % 4)
		};
	}

	function syncMemberList() {
		if (!S.settings) { S.settings = defaultSettings(); }
		S.settings.members = S.members.slice(0);
	}

	function ensureSession(mode) {
		if (S.active) { return; }
		S.active = true;
		S.host = true;
		S.hostXuid = LOCAL_XUID;
		S.settings = S.settings || defaultSettings(mode);
		S.members = [makeMember(LOCAL_XUID)];
		syncMemberList();
	}

	var dispatch = function (sessionState) {
		// Publish the state first: the handlers below read the APIs, and they may live in another
		// layout (its own JavaScript context) - that context only sees what is in the ConVar.
		saveStore();

		// The first argument is the session state: mainmenu_play.js::_SessionSettingsUpdate() only
		// re-syncs its dialogs on "updated" (that is what moves the checked game mode / map selection),
		// re-inits on "ready" and hides the content panel on "closed".  The other handlers on this
		// event (party.js, matchmaking_status.js, mainmenu.js) ignore it.
		var state = sessionState || "updated";
		try { $.DispatchEvent("PanoramaComponent_Lobby_MatchmakingSessionUpdate", state); } catch (e) { }
		try { $.DispatchEvent("PanoramaComponent_Lobby_PlayerUpdated", LOCAL_XUID); } catch (e) { }
		try { $.DispatchEvent("PanoramaComponent_PartyList_RebuildPartyList"); } catch (e) { }
	};

	// The port has no console, so state changes are also written to the game log (engine.log with
	// -condebug): that is how a test run can tell whether a click actually reached this layer.
	function log(msg) {
		try { $.Msg("[se_session_sim] " + msg); } catch (e) { }
		// Reachable from a normal run too (no -condebug): the C++ setting probe appends every
		// "se_probe*" write to D:\cstrike\se_ui_probe.txt as a SIMPROBE line - see
		// uicomponent_gameinterface.cpp::SetSettingString.
		try { g.GameInterfaceAPI.SetSettingString("se_probe", msg); } catch (e) { }
	}
	function memberSummary() {
		return S.members.length + "/" + lobbySlots(S.settings && S.settings.game ? S.settings.game.mode : DEFAULT_MODE) +
			" [" + S.members.map(function (m) { return m.name; }).join(", ") + "]";
	}

	function addMember(xuid) {
		if (!xuid || xuid === "0") { return false; }
		if (S.members.some(function (m) { return m.xuid === xuid; })) { return false; }
		var slots = lobbySlots(S.settings && S.settings.game ? S.settings.game.mode : DEFAULT_MODE);
		if (S.members.length >= slots) { return false; }
		S.members.push(makeMember(xuid));
		syncMemberList();
		return true;
	}

	function removeMember(xuid) {
		var before = S.members.length;
		S.members = S.members.filter(function (m) { return m.xuid !== xuid; });
		syncMemberList();
		return S.members.length !== before;
	}

	// =============================================================================================
	// LobbyAPI - 大厅
	// =============================================================================================
	function pickSettings() { return S.settings ? S.settings : defaultSettings(); }

	g.LobbyAPI.IsSessionActive = function () { syncFromStore(); return OPT.lobby ? S.active : false; };
	// True only while the local player *owns* the session (CreateSession / inviting somebody).  A
	// lobby joined from an invite belongs to somebody else: the play page then disables the mode and
	// map buttons (that is the CS:GO behaviour for guests) and the sidebar hides 取消搜索.
	g.LobbyAPI.BIsHost = function () { syncFromStore(); return OPT.lobby ? (S.active && S.host) : false; };
	g.LobbyAPI.GetHostSteamID = function () { syncFromStore(); return S.hostXuid; };

	g.LobbyAPI.GetSessionSettings = function () {
		// Never null/undefined: matchmaking_status.js reads .game before it checks IsSessionActive().
		syncFromStore();
		var settings = pickSettings();
		if (!settings.game) { settings.game = defaultSettings().game; }
		if (!settings.system) { settings.system = defaultSettings().system; }
		if (!settings.options) { settings.options = defaultSettings().options; }
		if (!settings.members) { settings.members = S.members.slice(0); }
		return settings;
	};

	g.LobbyAPI.UpdateSessionSettings = function (settings) {
		if (!settings) { return; }

		// CS:GO always has a lobby session at the main menu (the GC creates one).  Without one the play
		// page computes isHost = LobbyAPI.BIsHost() = false and disables every game mode / map button
		// (mainmenu_play.js::_SyncDialogsFromSessionSettings), which is exactly what "can't select
		// competitive, can't select maps" was: so a settings push also makes sure the session exists.
		ensureSession();

		// Two shapes arrive here:
		//   * the play page's KeyValues style patch - _ApplySessionSettings() sends
		//     { update: { Options: {...}, Game: { mode, mapgroupname, gamemodeflags, prime, map } },
		//       delete: { Options: { challengekey: 1 } } }   (play settings, prime toggle, permissions)
		//   * a whole settings object (game/system/options/members) - kept for compatibility
		var changed = [];
		if (settings.update) {
			changed = mergeTable(S.settings.game, "game", settings.update.Game, changed);
			changed = mergeTable(S.settings.options, "options", settings.update.Options, changed);
			changed = mergeTable(S.settings.system, "system", settings.update.System, changed);
		}
		if (settings.delete) {
			changed = deleteKeys(S.settings.options, "options", settings.delete.Options, changed);
			changed = deleteKeys(S.settings.game, "game", settings.delete.Game, changed);
		}
		if (settings.game || settings.system || settings.options) {
			changed = mergeTable(S.settings.game, "game", settings.game, changed);
			changed = mergeTable(S.settings.system, "system", settings.system, changed);
			changed = mergeTable(S.settings.options, "options", settings.options, changed);
		}

		// An empty mapgroupname means "the player has no saved map selection for this mode yet" (the
		// play page reads it from a ConVar that starts out empty).  CS:GO's session answers that with
		// every map group of the mode selected - without it the map list stays empty and nothing can
		// be picked.  (\"random_*\" values and explicit selections pass through untouched.)
		if (!S.settings.game.mapgroupname) {
			var groups = mapGroupKeys(S.settings.game.mode, true);
			if (groups.length) {
				S.settings.game.mapgroupname = groups.join(",");
				changed.push("mapgroupname=(all " + groups.length + " groups of " + S.settings.game.mode + ")");
			}
		}

		syncMemberList();
		log("UpdateSessionSettings " + (changed.length ? changed.join(" ") : "(no change)") +
			" -> mode=" + S.settings.game.mode + " mapgroup=" + S.settings.game.mapgroupname +
			" access=" + S.settings.system.access);

		// Keep the "last used mode" setting in step: _ValidateSessionSettings() falls back to it when a
		// mode is not selectable (and _Init() of any play page reads it), so a stale value there makes
		// the UI look like it ignored the click.
		try {
			if (S.settings.game.mode) {
				g.GameInterfaceAPI.SetSettingString("ui_playsettings_mode_" + (S.settings.options.server || "official"), S.settings.game.mode);
			}
		} catch (e) { }

		// "updated" is what makes mainmenu_play.js re-check the mode and map buttons.
		dispatch("updated");
	};

	function mergeTable(target, name, src, changed) {
		if (!src) { return changed; }
		for (var k in src) {
			if (typeof src[k] === "function") { continue; }
			if (target[k] !== src[k]) { changed.push(name + "." + k + "=" + src[k]); }
			target[k] = src[k];
		}
		return changed;
	}

	function deleteKeys(target, name, src, changed) {
		if (!src) { return changed; }
		for (var k in src) {
			if (Object.prototype.hasOwnProperty.call(target, k)) {
				changed.push("-" + name + "." + k);
				delete target[k];
			}
		}
		return changed;
	}

	g.LobbyAPI.CreateSession = function (settings) {
		// Called by mainmenu.js and the operation popups; the argument may be missing entirely.
		if (settings && settings.game && settings.system) { S.settings = settings; }
		else { S.settings = defaultSettings(); }
		S.active = true;
		S.host = true;
		S.hostXuid = LOCAL_XUID;
		S.members = [makeMember(LOCAL_XUID)];
		syncMemberList();
		log("CreateSession mode=" + S.settings.game.mode + " -> " + memberSummary());
		dispatch("ready");
	};

	g.LobbyAPI.CloseSession = function () {
		S.active = false;
		S.host = true;              // back to "own lobby" defaults; the next CreateSession is ours
		S.hostXuid = LOCAL_XUID;
		S.members = [];
		S.queue.searching = false;
		S.queue.status = "";
		log("CloseSession");
		dispatch("closed");
	};

	g.LobbyAPI.StartMatchmaking = function (questId, tournamentTeam, opponent, stage) {
		// 匹配: enter the searching state.  CS:GO would hand this to the GC; here the queue only
		// changes state, which is exactly what the search UI (matchmaking_status.js) renders.
		if (!OPT.matchmaking) { return; }
		ensureSession();
		S.queue.searching = true;
		S.queue.startedAt = nowSeconds();
		S.queue.status = "#SFUI_LobbyPrompt_QueueSearchTitle";
		S.queue.lastStart = { questId: questId || "", tournamentTeam: tournamentTeam || "", opponent: opponent || "", stage: stage || "" };
		log("StartMatchmaking mode=" + S.settings.game.mode + " mapgroup=" + S.settings.game.mapgroupname +
			" party=" + memberSummary() + " args=[" + (questId || "") + "," + (tournamentTeam || "") + "," + (opponent || "") + "," + (stage || "") + "]");
		dispatch();
	};

	g.LobbyAPI.StopMatchmaking = function () {
		S.queue.searching = false;
		S.queue.status = "";
		log("StopMatchmaking");
		dispatch();
	};

	g.LobbyAPI.GetMatchmakingStatusString = function () {
		// "" == not searching (matchmaking_status.js::_IsSeaching); a token otherwise.
		syncFromStore();
		return S.queue.searching ? S.queue.status : "";
	};

	g.LobbyAPI.GetTimeSpentMatchmaking = function () {
		syncFromStore();
		return S.queue.searching ? Math.floor(nowSeconds() - S.queue.startedAt) : 0;
	};

	g.LobbyAPI.GetMapWaitTimeInSeconds = function () { return 120; };
	g.LobbyAPI.GetMatchmakingStatistics = function () { return ""; };
	g.LobbyAPI.GetReadyTimeRemainingSeconds = function () { return 0; };
	g.LobbyAPI.GetConfirmedMatchPlayerCount = function () { return 0; };
	g.LobbyAPI.GetConfirmedMatchPlayerByIdx = function (i) { return "0"; };
	g.LobbyAPI.SetLocalPlayerReady = function (bReady) {
		var me = S.members[0];
		if (me) { me.ready = !!bReady; }
		try { $.DispatchEvent("PanoramaComponent_Lobby_ReadyUpForMatch"); } catch (e) { }
	};
	g.LobbyAPI.KickPlayer = function (xuid) {
		if (!S.host || xuid === LOCAL_XUID) { return; }
		if (removeMember(xuid)) { dispatch(); }
	};
	g.LobbyAPI.IsPartyMember = function (xuid) {
		return S.members.some(function (m) { return m.xuid === xuid; });
	};
	g.LobbyAPI.ChangeTeammateColor = function (xuid, color) {
		S.members.forEach(function (m) { if (m.xuid === xuid) { m.color = color; } });
	};
	g.LobbyAPI.LaunchTrainingMap = function () { /* the port has no training map loader */ };

	// =============================================================================================
	// PartyListAPI / PartyBrowserAPI / SessionUtil - 大厅成员与好友
	// =============================================================================================
	g.PartyListAPI.GetCount = function () {
		syncFromStore();
		return OPT.lobby && S.active ? S.members.length : 1;   // 1 = just the local player
	};

	g.PartyListAPI.GetXuidByIndex = function (i) {
		syncFromStore();
		if (S.active && i < S.members.length) { return S.members[i].xuid; }
		return (i === 0 || i === undefined) ? LOCAL_XUID : "0";
	};

	// party.js (sidebar) swaps the local player card for the party member list once the party is at
	// least this full - or while a search is running.  2 = "somebody joined you", which is what makes
	// the lobby UI reachable: invite a friend (friends tab / 广播 tab) or accept an invite, and the
	// member list with the leave / cancel buttons shows up right away.
	g.PartyListAPI.GetPartySessionUiThreshold = function () { return 2; };

	g.PartyListAPI.GetPartyMemberSetting = function (xuid, key) {
		// avatar.js asks for 'game/teamcolor' and builds a wash colour from 1..4.
		if (key && String(key).indexOf("color") !== -1) {
			var m = null;
			S.members.forEach(function (mm) { if (mm.xuid === xuid) { m = mm; } });
			return m ? m.color : 1;
		}
		return "";
	};

	g.PartyListAPI.GetPartySessionSetting = function (key) {
		var settings = pickSettings();
		if (!key) { return ""; }
		var parts = String(key).split("/");
		var root = settings[parts[0]];
		if (!root) { return ""; }
		if (parts.length === 1) { return root; }
		var v = root[parts[1]];
		return (v === undefined || v === null) ? "" : v;
	};

	g.PartyListAPI.GetPartySystemSetting = function (key) {
		var settings = pickSettings();
		if (!key) { return ""; }
		var parts = String(key).split("/");
		if (parts[0] === "system") { return settings.system[parts[1]] === undefined ? "" : settings.system[parts[1]]; }
		return settings.system[parts[1]] === undefined ? "" : settings.system[parts[1]];
	};

	g.PartyListAPI.GetPartyClanTag = function () { return ""; };
	g.PartyListAPI.SessionCommand = function (cmd, arg) { /* 'MakeOnline', 'Game::ChatReport*' - nothing to do */ };

	g.PartyListAPI.GetFriendCompetitiveRank = function (xuid, type) {
		var p = player(xuid);
		return (OPT.rank && p) ? p.skillGroup : -1;
	};
	g.PartyListAPI.GetFriendCompetitiveRankType = function (xuid) {
		return (OPT.rank && player(xuid)) ? "competitive" : "";
	};
	g.PartyListAPI.GetFriendCompetitiveWins = function (xuid, type) {
		var p = player(xuid);
		return (OPT.rank && p) ? p.wins : 0;
	};
	g.PartyListAPI.GetFriendPrimeEligible = function (xuid) {
		var p = player(xuid);
		return (OPT.rank && p) ? p.prime : true;
	};
	g.PartyListAPI.GetFriendIsTalking = function (xuid) { return false; };
	// advertising_toggle.js reads this as a *string* ("<mode>-<hex>", split('-') on activation), so
	// the empty string is the "not advertising" answer - a boolean made it throw every frame.
	g.PartyListAPI.GetLocalPlayerForHireAdvertising = function () { return S.forHireMode || ""; };
	g.PartyListAPI.SetLocalPlayerForHireAdvertising = function (gameMode) {
		S.forHireMode = gameMode || "";
		S.forHire = !!S.forHireMode;
		try { $.DispatchEvent("PanoramaComponent_PartyBrowser_LocalPlayerForHireAdvertisingChanged"); } catch (e) { }
	};
	g.PartyListAPI.SetLocalPlayerVanityPresence = function (xuid, presence) { };
	g.PartyListAPI.IsPlayerForHireAdvertisingEnabledForGameMode = function (mode) { return false; };

	// PartyBrowserAPI browses *other* players' parties.  Two consumers in the friends panel:
	//   * the 广播 ("looking to play") tab: one friend_advertise_tile per GetXuidByIndex(i); the
	//     tile's 邀请 button calls FriendsListAPI.ActionInviteFriend (they join *my* lobby);
	//   * the incoming invite row (friendlobby tile): reads the same "party" keys plus the member
	//     slots, and its 加入 button calls ActionJoinParty (I join *theirs*).
	// The party type picks the flavour: "nearby" = advertised player, "invited" = they invited me.

	function advertFor(xuid) {
		xuid = String(xuid || "");
		for (var i = 0; i < ADVERTISED.length; i++) {
			if (ADVERTISED[i].xuid === xuid) { return ADVERTISED[i]; }
		}
		return null;
	}

	function isInvited(xuid) {
		syncFromStore();
		var s = String(xuid || "");
		return S.invites.some(function (x) { return x === s; });
	}

	// Members of the lobby "party" (an advertising player or an inviter) is in; index 0 = leader.
	// The invite flavour shows a second member so the roster / slots in the tile look real.
	function partyMemberXuids(party) {
		var xuid = String(party || "");
		if (!player(xuid)) { return []; }
		if (isInvited(xuid)) {
			var partner = (xuid === FRIEND_XUIDS[0]) ? FRIEND_XUIDS[2] : FRIEND_XUIDS[0];
			return [xuid, partner];
		}
		return advertFor(xuid) ? [xuid] : [];
	}

	// Incoming invites.  The friends panel only rebuilds the invite row on these two events
	// (PanoramaComponent_PartyBrowser_InviteReceived / _InviteConsumed), so both paths fire them.
	function addInvite(xuid) {
		xuid = String(xuid || "");
		if (!xuid || xuid === "0" || !player(xuid)) { return; }
		syncFromStore();
		if (S.invites.some(function (x) { return x === xuid; })) { return; }
		S.invites.push(xuid);
		saveStore();
		try { $.DispatchEvent("PanoramaComponent_PartyBrowser_InviteReceived"); } catch (e) { }
		log("InviteReceived from " + playerName(xuid));
	}

	function removeInvite(xuid, bNotify) {
		xuid = String(xuid || "");
		syncFromStore();
		var before = S.invites.length;
		S.invites = S.invites.filter(function (x) { return x !== xuid; });
		if (S.invites.length === before) { return; }
		saveStore();
		if (bNotify) {
			try { $.DispatchEvent("PanoramaComponent_PartyBrowser_InviteConsumed"); } catch (e) { }
		}
		log("InviteConsumed " + playerName(xuid));
	}

	g.PartyBrowserAPI.GetPartyType = function (party) {
		var xuid = String(party || "");
		if (isInvited(xuid)) { return "invited"; }
		return advertFor(xuid) ? "nearby" : "";
	};
	g.PartyBrowserAPI.GetPartyMembersCount = function (party) { return partyMemberXuids(party).length; };
	g.PartyBrowserAPI.GetPartyMemberXuid = function (party, i) {
		var members = partyMemberXuids(party);
		var idx = Number(i);
		return (idx >= 0 && idx < members.length) ? members[idx] : 0;
	};
	// What the tiles render: mode (game/mode), prime (game/apr, "1"/"0"), region (game/loc, ISO
	// country code for CommonUtil.SetRegionOnLabel), rank (game/ark = skill group * 10 - the tile
	// divides it back down) and, for the invite flavour, the lobby's map groups (game/mapgroupname).
	g.PartyBrowserAPI.GetPartySessionSetting = function (party, key) {
		var a = advertFor(party);
		if (!a || !key) { return ""; }
		switch (String(key)) {
			case "game/mode": return a.mode;
			case "game/apr": return a.prime;
			case "game/loc": return a.loc;
			case "game/ark": return String((a.rank || 0) * 10);
			case "game/mapgroupname": return mapGroupKeys(a.mode, true).slice(0, 2).join(",");
			case "game/questid": return "";
			case "game/clanid": return "";
			case "game/clantag": return "";
		}
		return "";
	};
	g.PartyBrowserAPI.GetResultsCount = function () { return OPT.lobby ? ADVERTISED.length : 0; };
	g.PartyBrowserAPI.GetXuidByIndex = function (i) {
		if (!OPT.lobby) { return "0"; }
		var idx = Number(i);
		return (idx >= 0 && idx < ADVERTISED.length) ? ADVERTISED[idx].xuid : "0";
	};
	// The friends panel shows the loading bar only for 0 < progress < 100, so 100 = "search done".
	g.PartyBrowserAPI.GetProgress = function () { return 100; };
	// The list would already be there (this is simulated), but friendslist.js only rebuilds it on
	// this event - it is what the GC sends when a search finishes - so 刷新 has to fire it.
	g.PartyBrowserAPI.Refresh = function () {
		try { $.DispatchEvent("PanoramaComponent_PartyBrowser_Refresh"); } catch (e) { }
	};
	g.PartyBrowserAPI.SetSearchFilter = function (filter) { S.searchFilter = filter || "all"; };
	g.PartyBrowserAPI.GetInvitesCount = function () { syncFromStore(); return S.invites.length; };
	g.PartyBrowserAPI.GetInviteXuidByIndex = function (i) { syncFromStore(); return S.invites[Number(i) || 0] || "0"; };
	g.PartyBrowserAPI.ClearInvite = function (xuid) { removeInvite(xuid, true); };
	// 加入大厅 (the invite row's button): become a *guest* of that lobby - the sidebar then shows its
	// members (party.js reads PartyListAPI) and the play page shows the lobby state.
	g.PartyBrowserAPI.ActionJoinParty = function (party, bInvited) {
		if (!OPT.lobby) { return; }
		var leader = String(party || "");
		if (!leader || leader === "0" || leader === LOCAL_XUID || !player(leader)) { return; }

		var a = advertFor(leader);
		var mode = a ? a.mode : (S.settings && S.settings.game ? S.settings.game.mode : DEFAULT_MODE);
		S.active = true;
		S.host = false;
		S.hostXuid = leader;
		S.settings = defaultSettings(mode);
		S.members = partyMemberXuids(leader).map(makeMember);
		if (!S.members.some(function (m) { return m.xuid === LOCAL_XUID; })) { S.members.push(makeMember(LOCAL_XUID)); }
		removeInvite(leader, true);
		syncMemberList();
		log("ActionJoinParty leader=" + playerName(leader) + " -> " + memberSummary());
		dispatch("ready");
	};
	// private queues belong to the direct challenge flow (only drawn while searching)
	g.PartyBrowserAPI.GetPrivateQueuesCount = function () { return 0; };
	g.PartyBrowserAPI.GetPrivateQueuesPlayerCount = function () { return 0; };
	g.PartyBrowserAPI.GetPrivateQueuesMoreParties = function () { return false; };
	g.PartyBrowserAPI.GetPrivateQueuePartyXuidByIndex = function (i) { return "0"; };

	g.SessionUtil.GetMaxLobbySlotsForGameMode = function (mode) { return lobbySlots(mode); };
	g.SessionUtil.GetNumWinsNeededForRank = function (type) { return 10; };
	g.SessionUtil.AreLobbyPlayersPrime = function () { return true; };
	g.SessionUtil.DoesGameModeHavePrimeQueue = function (mode) {
		var type = modeType(mode || DEFAULT_MODE);
		return (type === "classic" || type === "freeforall");
	};

	// =============================================================================================
	// 好友资料 (FriendsListAPI) - 排位显示与邀请入口
	// =============================================================================================
	g.FriendsListAPI.GetFriendName = function (xuid) {
		var p = player(xuid);
		return p ? p.name : playerName(xuid);
	};
	g.FriendsListAPI.GetFriendLevel = function (xuid) {
		var p = player(xuid);
		return p ? p.level : 1;
	};
	g.FriendsListAPI.GetFriendXp = function (xuid) {
		var p = player(xuid);
		return p ? p.xp : 0;
	};
	g.FriendsListAPI.GetFriendCompetitiveRank = function (xuid, type) {
		var p = player(xuid);
		return (OPT.rank && p) ? p.skillGroup : -1;
	};
	g.FriendsListAPI.GetFriendCompetitiveWins = function (xuid, type) {
		var p = player(xuid);
		return (OPT.rank && p) ? p.wins : 0;
	};
	g.FriendsListAPI.GetFriendPrimeEligible = function (xuid) {
		var p = player(xuid);
		return (OPT.rank && p) ? p.prime : true;
	};
	g.FriendsListAPI.GetFriendIsTalking = function (xuid) { return false; };
	g.FriendsListAPI.GetFriendRelationship = function (xuid) {
		var p = player(xuid);
		return p ? p.relationship : "friend";
	};

	// -----------------------------------------------------------------------------------------
	// 好友列表 (the sidebar's friends tab): friendslist.js builds one friendtile per
	// FriendsListAPI.GetXuidByIndex(i); friendtile.js paints the status dot from
	// GetFriendStatusBucket and writes GetFriendStatus() into the status line - that value goes
	// straight into $.Localize(), a *native* call, so it has to be a string (a stub would throw
	// "expected string type" and take the whole tile down with it).
	// -----------------------------------------------------------------------------------------
	function friendStatusBucket(xuid) {
		var p = player(xuid);
		if (!p) { return "Offline"; }
		return p.ingame ? "PlayingCSGO" : "Online";
	}

	g.FriendsListAPI.GetCount = function () { return OPT.friends ? FRIEND_XUIDS.length : 0; };
	g.FriendsListAPI.GetXuidByIndex = function (i) {
		var idx = Number(i);
		return (OPT.friends && idx >= 0 && idx < FRIEND_XUIDS.length) ? FRIEND_XUIDS[idx] : "0";
	};
	g.FriendsListAPI.GetFriendStatusBucket = function (xuid) { return friendStatusBucket(xuid); };
	g.FriendsListAPI.GetFriendStatus = function (xuid) {
		var bucket = friendStatusBucket(xuid);
		if (bucket === "PlayingCSGO") { return "#FriendsList_Ingame_Label"; }
		if (bucket === "Online") { return "#FriendsList_Online_Label"; }
		return "#FriendsList_Offline_Label";
	};
	// "已邀请" state of the friend tile, and the two session buttons the player-card context menu
	// asks about (nothing out of a *lobby* is joinable/watchable in this build).
	g.FriendsListAPI.IsFriendInvited = function (xuid) { syncFromStore(); return S.invited[String(xuid)] === true; };
	g.FriendsListAPI.IsFriendJoinable = function (xuid) { return false; };
	g.FriendsListAPI.IsFriendWatchable = function (xuid) { return false; };
	g.FriendsListAPI.GetFriendRequestsCount = function () { return 0; };
	g.FriendsListAPI.GetFriendRequestsXuidByIdx = function (i) { return "0"; };
	g.FriendsListAPI.GetFriendRequestsNotificationNumber = function () { return 0; };

	// 邀请好友进大厅: the entry point the player card context menu and the advertise tiles use
	// (context_menu_playercard.js::ActionInviteFriend).
	g.FriendsListAPI.ActionInviteFriend = function (xuid, arg) {
		if (!OPT.lobby || !xuid) { return; }
		syncFromStore();
		ensureSession();
		var added = addMember(xuid);
		if (added) { S.invited[String(xuid)] = true; }
		try { $.DispatchEvent("FriendInvitedFromContextMenu", xuid); } catch (e) { }
		try {
			$.Msg("[se_session_sim] invite ", playerName(xuid), added ? " -> joined the lobby (" + S.members.length + "/" + lobbySlots(S.settings.game.mode) + ")" : " -> lobby full");
		} catch (e) { }
		dispatch();
	};

	// The recents tab and friendtile.js both query TeammatesAPI (co-players of past matches).  There
	// is no history in this build, so the answers are real zeros / "": the tab stays in its "nodata"
	// state and GetSecondsAgoFinished() >= 0 keeps the recents loading bar hidden.
	g.TeammatesAPI.GetCount = function () { return 0; };
	g.TeammatesAPI.GetXuidByIndex = function (i) { return "0"; };
	g.TeammatesAPI.GetCoPlayerInCSGO = function (xuid) { return false; };
	g.TeammatesAPI.GetCoPlayerTime = function (xuid) { return ""; };
	g.TeammatesAPI.GetSecondsAgoFinished = function () { return 0; };
	g.TeammatesAPI.Refresh = function () { };

	// =============================================================================================
	// 段位 (排位)
	// =============================================================================================
	// playercard.js opens the skill group block only when this is >= 0, and prints the wins from
	// FriendsListAPI.GetFriendCompetitiveWins.
	g.MyPersonaAPI.GetPipRankWins = function (type) {
		return OPT.rank ? PLAYERS[LOCAL_XUID].wins : -1;
	};
	g.MyPersonaAPI.GetCompetitiveWins = function (type) { return OPT.rank ? PLAYERS[LOCAL_XUID].wins : 0; };
	g.MyPersonaAPI.GetCompetitiveRank = function (type) { return OPT.rank ? PLAYERS[LOCAL_XUID].skillGroup : -1; };
	g.MyPersonaAPI.GetCurrentLevel = function () { return PLAYERS[LOCAL_XUID].level; };
	g.MyPersonaAPI.HasPrestige = function () { return false; };
	g.MyPersonaAPI.GetPlayerLevel = function () { return PLAYERS[LOCAL_XUID].level; };
	g.MyPersonaAPI.IsInventoryValid = function () { return true; };

	// =============================================================================================
	// GameTypesAPI - 模式与地图表 (real data)
	// =============================================================================================
	g.GameTypesAPI.GetConfig = function () { return cfg; };

	g.GameTypesAPI.GetGameModeAttribute = function (modeTypeName, mode, attr) {
		var type = cfg.gameTypes[modeTypeName];
		var m = type && type.gameModes ? type.gameModes[mode] : modeConfig(mode);
		if (!m) { m = modeConfig(mode); }
		return (m && m[attr] !== undefined) ? m[attr] : "";
	};

	g.GameTypesAPI.GetMapGroupAttribute = function (mg, attr) {
		var g2 = cfg.mapgroups[mg];
		return (g2 && g2[attr] !== undefined) ? g2[attr] : "";
	};

	// tooltip_lobby_settings.js calls this as
	//   GameTypesAPI.GetMapGroupAttributeSubKeys( mapgroup, 'maps' ).split( ',' )
	// so it wants the *values* of that sub-table as one comma separated string (the map names), not an
	// array of keys - returning an array made .split() throw and aborted the tooltip (JSEXC #5/6/8/9).
	g.GameTypesAPI.GetMapGroupAttributeSubKeys = function (mg, subKey) {
		var group = cfg.mapgroups[mg];
		if (!group || !subKey) { return ""; }
		var sub = group[subKey];
		if (sub === undefined || sub === null) { return ""; }
		if (typeof sub !== "object") { return String(sub); }
		var out = [];
		for (var k in sub) {
			var v = sub[k];
			// in gamemodes.txt a map entry is "de_dust2" "" - the key carries the name
			out.push((v === "" || v === undefined || v === null) ? k : v);
		}
		return out.join(",");
	};

	g.GameTypesAPI.GetGameModeType = function (mode) { return modeType(mode); };
	g.GameTypesAPI.GetFriendlyMapName = function (map) { return map; };
	g.GameTypesAPI.GetSkirmishName = function (id) { return ""; };
	g.GameTypesAPI.GetSkrimishIcon = function (id) { return ""; };
	g.GameTypesAPI.GetSkirmishInternalName = function (id) { return ""; };
	g.GameTypesAPI.GetSkirmishIdFromInternalName = function (name) { return -1; };
	g.GameTypesAPI.SetCustomBotDifficulty = function (diff) { };

	// SE port（2026-09-19）: 大行动活动页默认关闭 —— 用户实测后反馈"太卡了"（引擎 FRAME 探针可见
	// fps 掉到 ~14、avg 76ms），所以整块退回去：赛季号 -1 = "没有大行动"，活动页不再挂载、赛季相关
	// 代码路径（操作上线弹窗 / 任务卡 / 奖励瓦片）全部不进。
	// 想再打开就把下面这行改成 11（激流大行动），活动页挂载见文件末尾 "大行动（Operation）" 一节。
	var SE_OP_SEASON = -1;
	g.GameTypesAPI.GetActiveSeasionIndexValue = function () { return SE_OP_SEASON; };
	g.GameTypesAPI.GetActiveSeasionCodeName = function () { return SE_OP_SEASON > 0 ? "riptide" : ""; };

	// =============================================================================================
	// CompetitiveMatchAPI - 竞技/排位相关的冷却、直连码、锦标赛
	// =============================================================================================
	var directChallengeCode = "";

	g.CompetitiveMatchAPI.GetCooldownSecondsRemaining = function () { return 0; };
	g.CompetitiveMatchAPI.GetCooldownReason = function () { return ""; };
	g.CompetitiveMatchAPI.GetCooldownType = function () { return ""; };
	g.CompetitiveMatchAPI.CooldownIsPermanent = function () { return false; };
	g.CompetitiveMatchAPI.ShowFairPlayGuidelinesForCooldown = function () { };
	g.CompetitiveMatchAPI.HasOngoingMatch = function () { return false; };
	g.CompetitiveMatchAPI.ActionAbandonOngoingMatch = function () { };
	g.CompetitiveMatchAPI.ActionReconnectToOngoingMatch = function () { };
	g.CompetitiveMatchAPI.ActionAcknowledgePenalty = function () { };

	// 直接挑战 (direct challenge) codes: generated locally, then checked against the local value.
	g.CompetitiveMatchAPI.GenerateDirectChallengeCode = function (arg) {
		var chars = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
		var code = "";
		for (var i = 0; i < 5; i++) { code += chars.charAt(Math.floor(Math.random() * chars.length)); }
		directChallengeCode = code;
		try { $.DispatchEvent("PlayMenu_GoTeamMatchmaking_CodeGenerated", code); } catch (e) { }
		return code;
	};
	g.CompetitiveMatchAPI.GetDirectChallengeCode = function () { return directChallengeCode; };
	g.CompetitiveMatchAPI.GetDirectChallengeCodeForClan = function (clan) { return ""; };
	g.CompetitiveMatchAPI.ValidateDirectChallengeCode = function (code) {
		return !!code && String(code).length >= 4;
	};

	// tournaments: none in this build
	g.CompetitiveMatchAPI.GetTournamentStageCount = function () { return 0; };
	g.CompetitiveMatchAPI.GetTournamentStageNameByIndex = function (i) { return ""; };
	g.CompetitiveMatchAPI.GetTournamentTeamCount = function () { return 0; };
	g.CompetitiveMatchAPI.GetTournamentTeamNameByID = function (id) { return ""; };
	g.CompetitiveMatchAPI.GetTournamentTeamNameByIndex = function (i) { return ""; };
	g.CompetitiveMatchAPI.GetTournamentTeamTagByID = function (id) { return ""; };
	g.CompetitiveMatchAPI.GetRotatingOfficialMapGroupCurrentState = function () { return ""; };

	// =============================================================================================
	// misc booleans the lobby UI reads (a truthy stub here reads as "yes" - see P66)
	// =============================================================================================
	g.GameStateAPI.IsLocalPlayerPlayingMatch = function () { return false; };

	// mainmenu.js::_OnShowPauseMenu feeds this straight into a bool sink
	// ( "( bTraining || bQueuedMatchmaking || bGotvSpectating )" -> SetHasClass ), and the truthy stub
	// aborted the rest of the pause-menu navbar update (JSEXC of the 2026-09-17 run).  CS:GO answers
	// true while a matchmaking search is running *in a lobby*; the sim exposes that state through the
	// Lobby component updates instead, and the two in-game readers (context_menu_vote.js,
	// hudmissionpanel.js, endofmatch.js) all describe situations this port never reaches.
	g.GameStateAPI.IsQueuedMatchmaking = function () { return false; };
	g.GameStateAPI.IsQueuedMatchmakingMode_Team = function () { return false; };

	g.NewsAPI.GetCurrentActiveAlertForUser = function () { return ""; };
	g.MyPersonaAPI.IsPrime = function () { return true; };

	// ---------------------------------------------------------------------------------------------
	// Debug / test hook: SE_SESSION_SIM.state(), .create(), .start(), .stop(), .invite(xuid), .leave()
	// ---------------------------------------------------------------------------------------------
	g.SE_SESSION_SIM = {
		options: OPT,
		state: function () { return S; },
		members: function () { return S.members.map(function (m) { return m.name; }); },
		create: function (mode) { S.settings = defaultSettings(mode); g.LobbyAPI.CreateSession(S.settings); return S; },
		start: function () { g.LobbyAPI.StartMatchmaking("", "", "", ""); return S; },
		stop: function () { g.LobbyAPI.StopMatchmaking(); return S; },
		invite: function (xuid) { g.FriendsListAPI.ActionInviteFriend(xuid || FRIEND_XUIDS[0], ""); return S; },
		inviteFrom: function (xuid) { addInvite(xuid || FRIEND_XUIDS[0]); return S; },
		declineInvite: function (xuid) { removeInvite(xuid || FRIEND_XUIDS[0], true); return S; },
		join: function (xuid) { g.PartyBrowserAPI.ActionJoinParty(xuid || FRIEND_XUIDS[0]); return S; },
		leave: function () { g.LobbyAPI.CloseSession(); return S; },
		advertised: function () { return ADVERTISED.map(function (a) { return a.xuid; }); },
		friends: function () { return FRIEND_XUIDS.slice(0); }
	};

	// ---------------------------------------------------------------------------------------------
	// StoreAPI / InventoryAPI (SE port, 商店最小数据层)
	// ---------------------------------------------------------------------------------------------
	// mainmenu_store.js builds the store page from StoreAPI.GetBannerEntry* plus iteminfo.js, and
	// iteminfo asks InventoryAPI for names / capabilities / prices per item.  This build has no GC
	// price sheet and no items_game.txt schema, so:
	//   * the banner list is a small fixed mock; the def indexes are the real Paris 2023 store ids
	//     taken from the shipped generated/items_event_current_generated_store.js,
	//   * InventoryAPI answers the handful of calls the store path makes with *typed* answers
	//     (strings / numbers / bools - a placeholder stub object in one of those slots throws, P72).
	// The market entry carries market:true, which is what makes the tile render the 市场 button
	// (its click calls SteamOverlayAPI.OpenURL).
	var STORE_FauxPrefix = "se_store_";

	// SE port: every string this layer puts on screen goes through the port's own localization file
	// (mods/panorama_test/panorama/localization/se_port_<language>.txt, loaded by
	// panoramauiclient/se_uicomponents.cpp via BLoadLocalizationFile( "se_port" )).  A token resolves
	// to the game's current language; an unknown one falls back to the raw string, so this cannot
	// throw.  The values below are tokens, not text - GetItemName() resolves them before handing them
	// out, which keeps working whatever the content does with the result.
	function seLocalize(token) {
		try {
			if (typeof $.LocalizeSafe === "function") { return $.LocalizeSafe(token); }
			if (typeof $.Localize === "function") { return $.Localize(token); }
		} catch (e) { }
		return token;
	}

	var STORE_NAMES = {
		"4883": "#SEPort_Store_Item_4883",
		"4888": "#SEPort_Store_Item_4888",
		"6732": "#SEPort_Store_Item_6732",
		// 历代大行动(见下面的 STORE_BANNER);名字里的年份方便辨认。
		"9101": "#SEPort_Store_Item_9101",
		"9102": "#SEPort_Store_Item_9102",
		"9103": "#SEPort_Store_Item_9103",
		"9104": "#SEPort_Store_Item_9104",
		"9105": "#SEPort_Store_Item_9105",
		"9106": "#SEPort_Store_Item_9106",
		"9107": "#SEPort_Store_Item_9107",
		"9108": "#SEPort_Store_Item_9108",
		"9109": "#SEPort_Store_Item_9109",
		"9110": "#SEPort_Store_Item_9110",
		"9111": "#SEPort_Store_Item_9111",
		// 大行动任务物品（名字给活动页的任务卡用，见文件末尾的活动页一节）
		"9201": "#SEPort_Op_Q1_Name",
		"9202": "#SEPort_Op_Q2_Name",
		"9203": "#SEPort_Op_Q3_Name",
		// 奖励物品（定义号 = 9300 + 奖励号，见 GetItemDefinitionIndexFromDefinitionName）
		"9301": "#SEPort_Op_Reward_1",
		"9302": "#SEPort_Op_Reward_2",
		"9303": "#SEPort_Op_Reward_3",
		"9304": "#SEPort_Op_Reward_4",
		"9305": "#SEPort_Op_Reward_5",
		"9306": "#SEPort_Op_Reward_6"
	};
	var STORE_BANNER = [
		{ def: 4883, market: false, format: "", coupon: "" },
		{ def: 4888, market: false, format: "", coupon: "" },
		{ def: 6732, market: true, format: "", coupon: "" },
		// 历代大行动卡(点击 = 打开市场搜索;图标由 installStoreItemImages() 设置,
		// 素材放到 {images}/icons/ui/operation_*.svg 即自动生效)。
		{ def: 9101, market: true, format: "", coupon: "" },
		{ def: 9102, market: true, format: "", coupon: "" },
		{ def: 9103, market: true, format: "", coupon: "" },
		{ def: 9104, market: true, format: "", coupon: "" },
		{ def: 9105, market: true, format: "", coupon: "" },
		{ def: 9106, market: true, format: "", coupon: "" },
		{ def: 9107, market: true, format: "", coupon: "" },
		{ def: 9108, market: true, format: "", coupon: "" },
		{ def: 9109, market: true, format: "", coupon: "" },
		{ def: 9110, market: true, format: "", coupon: "" },
		{ def: 9111, market: true, format: "", coupon: "" }
	];
	function storeBanner(i) { return STORE_BANNER[Number(i) || 0] || null; }

	g.StoreAPI.GetBannerEntryCount = function () { return STORE_BANNER.length; };
	g.StoreAPI.GetBannerEntryDefIdx = function (i) { var e = storeBanner(i); return e ? e.def : 0; };
	g.StoreAPI.IsBannerEntryMarketLink = function (i) { var e = storeBanner(i); return e ? (e.market === true) : false; };
	g.StoreAPI.GetBannerEntryCustomFormatString = function (i) { var e = storeBanner(i); return e ? e.format : ""; };
	g.StoreAPI.GetBannerEntryLinkedCoupon = function (i) { var e = storeBanner(i); return e ? e.coupon : ""; };
	g.StoreAPI.GetSecondsUntilTimestamp = function () { return 86400; };
	g.StoreAPI.GetAccountWalletBalance = function () { return ""; };
	g.StoreAPI.GetStoreItemSalePrice = function () { return ""; };
	g.StoreAPI.GetStoreItemOriginalPrice = function () { return ""; };
	g.StoreAPI.GetStoreItemPercentReduction = function () { return 0; };
	g.StoreAPI.StoreItemPurchase = function () { };
	g.StoreAPI.RecordUIEvent = function () { };
	g.StoreAPI.RequestStoreLayout = function () { };
	g.StoreAPI.GetStoreLayoutObject = function () { return {}; };

	g.InventoryAPI.GetFauxItemIDFromDefAndPaintIndex = function (def, paint) {
		return STORE_FauxPrefix + String(def === undefined ? 0 : def) + "_" + String(paint === undefined ? 0 : paint);
	};
	g.InventoryAPI.IsValidItemID = function (id) {
		return (typeof id === "string") && id.indexOf(STORE_FauxPrefix) === 0;
	};
	g.InventoryAPI.GetItemName = function (id) {
		if (typeof id === "string" && id.indexOf(STORE_FauxPrefix) === 0) {
			var def = id.substring(STORE_FauxPrefix.length).split("_")[0];
			if (STORE_NAMES[def]) { return seLocalize(STORE_NAMES[def]); }
		}
		return seLocalize("#SEPort_Store_Item_Fallback");
	};
	g.InventoryAPI.GetItemDefinitionName = function () { return "se_store_item"; };
	g.InventoryAPI.GetItemTypeFromEnum = function () { return ""; };
	g.InventoryAPI.GetRawDefinitionKey = function () { return "0"; };
	g.InventoryAPI.GetItemCapabilitiesCount = function () { return 0; };
	g.InventoryAPI.GetItemCapabilityByIndex = function () { return ""; };
	g.InventoryAPI.IsTool = function () { return false; };
	g.InventoryAPI.IsCouponCrate = function () { return false; };
	g.InventoryAPI.GetDecodeableRestriction = function () { return ""; };
	g.InventoryAPI.GetLootListItemIdByIndex = function () { return ""; };
	g.InventoryAPI.GetLootListItemsCount = function () { return 0; };

	// ---------------------------------------------------------------------------------------------
	// 主菜单"商店"入口(SE port)
	// ---------------------------------------------------------------------------------------------
	// This content build has no button that fires the 'HideMainMenuNewsPanel' event (the cstrike15
	// client did it in the real game), so the store panel that mainmenu.js loads at startup could
	// never be shown.  Add a button to the news panel's title bar that toggles the same class
	// MainMenu.HideMainMenuNewsPanel() sets ('.news-panel--hide-news-panel': hide the news list,
	// show #JsStorePanel).  Runs on every layout; outside the main menu the target is never found.
	// SE port (2026-09-19): restored per user request.  The Operation panel is OFF by default
	// (SE_OP_SEASON = -1 - it made the menu lag), so the news panel goes back to the stock layout:
	// the news list stays, and this "Store" button toggles '.news-panel--hide-news-panel' to reveal
	// the fake store entries in #JsStorePanel (STORE_BANNER / STORE_NAMES above).
	// If SE_OP_SEASON is ever set back to 11 the mounted operation panel claims this same spot
	// (see mountOperationPanel() at the end of this file) - decide which of the two wins then.
	(function installStoreEntryButton() {
		if (!g.$) { return; }
		// SE port (2026-09-18): the injected scripts run in *every* layout context, and outside the
		// main menu g.MainMenu is a se_api_shim placeholder (always truthy), so the old 40-try loop
		// waited 20 seconds and then logged "#JsNewsPanel 未出现" once per context.  Only the news
		// layout's own context can see it: there the context panel *is* #JsNewsPanel (FindChild
		// cannot find "self"), so just compare the id - no retries needed.
		function attempt() {
			try {
				var root = $.GetContextPanel();
				if (!root || root.id !== 'JsNewsPanel') { return; }
				var newsPanel = root;
				if ($.FindChildInContext('#SeStoreToggleButton')) { return; }
				var navbar = newsPanel.Children()[0] || newsPanel;
				var btn = $.CreatePanel('Button', navbar, 'SeStoreToggleButton');
				btn.AddClass('news-panel-navbar-btn');
				btn.text = seLocalize("#SEPort_Store_Button");
				btn.SetPanelEvent('onactivate', function () {
					var container = $.FindChildInContext('#JsNewsContainer');
					if (!container) { return; }
					if (container.BHasClass('news-panel--hide-news-panel')) {
						container.RemoveClass('news-panel--hide-news-panel');
						log("商店按钮: 收起商店页");
					} else {
						container.AddClass('news-panel--hide-news-panel');
						log("商店按钮: 打开商店页");
					}
				});
				log("商店按钮: 已安装");
			} catch (e) { log("商店按钮: 安装异常 " + e); }
		}
		attempt();
	})();

	// ---------------------------------------------------------------------------------------------
	// 会话开始前的一次性准备
	// ---------------------------------------------------------------------------------------------
	// mainmenu_play.js::StartSearch() opens the "game mode flags" popup *instead of* starting the
	// search when the mode uses flags and the saved value is 0 ( util_gamemodeflags.js: competitive
	// accepts 48/32/16, deathmatch 4/32/16 ).  The ConVars that hold them start out empty in this
	// install, so seed the mode's default - otherwise pressing 开始 never reaches the queue.
	(function seedPlaySettings() {
		if (!g.GameInterfaceAPI || !g.GameInterfaceAPI.SetSettingString) { return; }
		var flSeedT0 = Date.now();
		var seeds = {
			"ui_playsettings_mode_official": "competitive",
			"ui_playsettings_flags_official_competitive": "48",
			"ui_playsettings_flags_official_deathmatch": "4"
		};

		// The map selection the play page reads back is a comma list of *map names* (it matches them
		// against the tiles' mapname attribute), so seed each mode with every map of its official map
		// groups - that is the "all maps" default.  Without it a fresh install has no selection at all
		// and StartSearch() stops at its "no map selected" popup instead of queueing.
		var modes = ["competitive", "scrimcomp2v2", "casual", "deathmatch", "skirmish", "survival"];
		for (var mi = 0; mi < modes.length; mi++) {
			var mode = modes[mi];
			var groups = mapGroupKeys(mode, true);
			var maps = [];
			for (var gi = 0; gi < groups.length; gi++) {
				var mg = cfg.mapgroups[groups[gi]];
				if (!mg || !mg.maps) { continue; }
				for (var mapName in mg.maps) {
					if (maps.indexOf(mapName) === -1) { maps.push(mapName); }
				}
			}
			if (maps.length) { seeds["ui_playsettings_maps_official_" + mode] = maps.join(","); }
		}

		for (var key in seeds) {
			try {
				var cur = g.GameInterfaceAPI.GetSettingString(key, "");
				if (cur === "" || cur === undefined || cur === null) {
					g.GameInterfaceAPI.SetSettingString(key, seeds[key]);
				}
			} catch (e) { }
		}
		g.SE_PORT_SEED_MS = Date.now() - flSeedT0;
	})();

	// CS:GO has a lobby session the moment the main menu comes up, and the play page depends on it
	// (BIsHost() gates every mode / map button).  Start with a solo lobby: one member and a lobby size
	// of five keeps the party member list collapsed, so the local player card stays where it is.
	// The state itself lives in a ConVar so that all layouts share it (the first context to load wins
	// the initialisation; every later one reads what is already there).
	var flStoreT0 = Date.now();
	syncFromStore();
	if (!S.active || !S.settings) {
		ensureSession();
		saveStore();
	}
	g.SE_PORT_STORE_MS = Date.now() - flStoreT0;

	// --- 大厅入口 (demo): a friend invites the local player, so the sidebar shows the invite row
	// (friendlobby tile) whose 加入 button lands in *their* lobby (PartyBrowserAPI.ActionJoinParty -
	// the local player becomes a guest).  Throttled so that declining it does not bring it straight
	// back: at most once every two minutes, and never while a party is already in the lobby.  The
	// other two entries - 邀请 from the friends list's context menu and the 广播 tab's tiles - work
	// without this.  Disable with SE_SESSION_SIM.options.friends = false.
	(function seedIncomingInvite() {
		if (!OPT.lobby || !OPT.friends) { return; }
		syncFromStore();
		if (S.members.length > 1) { return; }                       // already in somebody's lobby
		if (S.invites.length) { return; }                           // one is already waiting
		if (nowSeconds() - (S.inviteSeededAt || 0) < 120) { return; }
		S.inviteSeededAt = nowSeconds();
		addInvite(FRIEND_XUIDS[0]);
		saveStore();
	})();

	// Cost of loading the injected scripts into *this* layout's JavaScript context.  A page switch
	// throws the old context away (CUIPanel::BLoadLayout -> DeleteScriptContext), so se_api_shim.js +
	// se_gametypes.js + this file run again each time: "ms" is that total (the shim recorded the start
	// in g.SE_PORT_JS_T0), "seed" the play-settings/map seeding below and "store" the shared read.
	log("JSINIT ms=" + (Date.now() - (g.SE_PORT_JS_T0 || Date.now())) +
		" seed=" + (g.SE_PORT_SEED_MS || 0) + " store=" + (g.SE_PORT_STORE_MS || 0));

	// ---------------------------------------------------------------------------------------------
	// 商店/新闻的链接：接管 SteamOverlayAPI 的两个 URL 入口，交给宿主用默认浏览器打开
	// ---------------------------------------------------------------------------------------------
	// mainmenu_store.js (market link / banner links), mainmenu_news.js (article entries) and several
	// popups all end at SteamOverlayAPI.OpenURL / OpenUrlInOverlayOrExternalBrowser.  The port has no
	// Steam overlay, and unlike the other members a *no-op* here is what the user notices: the store's
	// 市场 tile (and every news entry) looks "dead".  Route both to the host, which opens the URL with
	// the OS default handler (GameInterfaceAPI.OpenURLInBrowser, see uicomponent_gameinterface.cpp).
	(function installOverlayUrlOpeners() {
		if (!g.SteamOverlayAPI) { return; }

		// 拼接市场/社区链接的伙伴 API:CS:GO 里由 cstrike15 客户端提供。缺失时脚本会拼出
		// "[]/market/search?appid=[]..."(桩对象字符串化成 "[]"),ShellExecute 自然打不开。
		g.SteamOverlayAPI.GetAppID = function () { return "730"; };
		g.SteamOverlayAPI.GetSteamCommunityURL = function () { return "steamcommunity.com"; };
		if (g.InventoryAPI) { g.InventoryAPI.GetItemSet = function () { return "Paris2023"; }; }

		var openURL = function (url) {
			try {
				url = (url === undefined || url === null) ? "" : String(url);
				if (!url) { return; }
				// 内容里两种拼接风格:有的自带 "https://",有的直接用 GetSteamCommunityURL()(不带协议)。
				if (!/^[a-zA-Z][a-zA-Z0-9+.\-]*:/.test(url)) { url = "https://" + url; }
				// 残留的空桩值("[]")说明还有别的伙伴 API 没补,别把垃圾地址丢给浏览器。
				if (url.indexOf("[]") >= 0) { log("打开链接(含空值,跳过): " + url); return; }
				log("打开链接: " + url);
				g.GameInterfaceAPI.OpenURLInBrowser(url);
			} catch (e) { log("打开链接异常: " + e); }
		};
		g.SteamOverlayAPI.OpenURL = openURL;
		g.SteamOverlayAPI.OpenUrlInOverlayOrExternalBrowser = openURL;
	})();

	// ---------------------------------------------------------------------------------------------
	// "Legacy version of CS:GO" 弹窗去重 + 弹窗栈探针 (SE port, 2026-09-18)
	// ---------------------------------------------------------------------------------------------
	// mainmenu.js 会被不止一个 JavaScript 上下文执行(嵌套的 <CSGOMainMenu> 那份也会把布局脚本
	// 跑一遍——探针里 "商店按钮: 已安装" ×2 即证据),而它每个上下文都注册
	// $.RegisterForUnhandledEvent('CSGOShowMainMenu', MainMenu.OnShowMainMenu);
	// 于是 _ShowLegacyVersionWarning() 会在同一个弹窗管理器里放下 *两个一模一样的* 双按钮弹窗:
	// 点"确定"只关掉最上面那个,底下那个纹丝不动 —— 看上去就是"点了没反应"。
	// 用(现在已可靠的)设置层做进程级去重: 第一次调用放行,其余丢弃。
	var LEGACY_POPUP_TITLE = '#legacy_support_text_title';
	(function dedupeLegacyPopup() {
		if (!g.UiToolkitAPI || !g.UiToolkitAPI.ShowGenericPopupTwoOptions) { return; }
		if (g.__seLegacyDedupeInstalled) { return; }
		g.__seLegacyDedupeInstalled = true;
		var orig = g.UiToolkitAPI.ShowGenericPopupTwoOptions;
		try {
			g.UiToolkitAPI.ShowGenericPopupTwoOptions = function (title, message, style, opt1, fn1, opt2, fn2) {
				if (title !== LEGACY_POPUP_TITLE) { return orig.apply(this, arguments); }
				// cfg 开关: se_popup_legacy = 1(默认) 允许弹一次 / 0 完全不弹
				try {
					var sw = "1";
					try { sw = g.GameInterfaceAPI.GetSettingString("se_popup_legacy"); } catch (e) { }
					if (sw === "0") {
						log("弹窗: Legacy 弹窗已被 se_popup_legacy=0 关闭");
						return null;
					}
				} catch (e) { }
				// 进程级去重: 抢到标记的那次调用负责把它弹出来, 其余(多上下文注册的重复处理器)丢弃。
				var bOwner = false;
				try {
					var seen = "";
					try { seen = g.GameInterfaceAPI.GetSettingString("se_legacy_popup_shown"); } catch (e) { }
					if (seen !== "1") {
						bOwner = true;
						try { g.GameInterfaceAPI.SetSettingString("se_legacy_popup_shown", "1"); } catch (e) { }
					}
				} catch (e) { bOwner = true; }
				if (!bOwner) {
					log("弹窗: 重复的 Legacy 弹窗已拦截");
					return null;
				}
				// 主菜单刚加载时 CUI_Root 可能还没给窗口注册好, ShowGenericPopupTwoOptions 会静默
				// 返回空(什么也不创建) —— 表现就是"创建了但没弹"。看到空返回就每隔 0.25 秒重试,
				// 直到真正拿到弹窗面板(返回非空)为止。
				var args = [title, message, style, opt1, fn1, opt2, fn2];
				try {
					var r = orig.apply(this, args);
					if (r) {
						log("弹窗: Legacy 弹窗已弹出");
						return r;
					}
					log("弹窗: Legacy 弹窗创建返回空(管理器未就绪), 开始重试");
					(function retry(n) {
						if (n > 40) { log("弹窗: Legacy 弹窗重试放弃"); return; }
						$.Schedule(0.25, function () {
							try {
								var rr = orig.apply(g.UiToolkitAPI, args);
								if (rr) { log("弹窗: Legacy 弹窗已弹出(第 " + n + " 次重试)"); }
								else { retry(n + 1); }
							} catch (e) { retry(n + 1); }
						});
					})(0);
					return null;
				} catch (e) {
					log("弹窗: Legacy 弹窗创建异常 " + e);
					return null;
				}
			};
		} catch (e) { log("弹窗: Legacy 去重挂钩失败 " + e); }
	})();

	// 弹窗栈探针: 在拥有弹窗管理器(#PopupManager)的上下文里每 2 秒数一次可见弹窗,数量变化时记一行。
	(function popupStackProbe() {
		if (!g.$) { return; }
		var last = -1;
		function tick() {
			try {
				var root = $.GetContextPanel();
				var pm = (root && root.FindChildTraverse) ? root.FindChildTraverse('PopupManager') : null;
				if (pm) {
					var kids = pm.Children() || [];
					var visible = 0;
					for (var i = 0; i < kids.length; i++) {
						var id = kids[i].id || "";
						if (id === 'DimBackground' || id === 'BlurBackground') { continue; }
						if (!kids[i].BHasClass('Hidden')) { visible++; }
					}
					if (visible !== last) {
						last = visible;
						log("弹窗栈[" + (root.id || "?") + "]: 可见=" + visible + "/" + kids.length);
					}
				}
			} catch (e) { }
			$.Schedule(2.0, tick);
		}
		$.Schedule(1.0, tick);
	})();

	// ---------------------------------------------------------------------------------------------
	// 历代大行动的卡片图标(商店面板)
	// ---------------------------------------------------------------------------------------------
	// The store tiles (mainmenu_store_tile.xml) draw their icon through an <ItemImage itemid=...>,
	// which this port stubs (no inventory), so the tiles would be text-only.  The banner entries
	// above use the def numbers 9101..9111 for the classic operations; this scanner walks the panel
	// tree and sets the matching icon on each tile (SetImage is the same JS call mainmenu_news.js
	// uses for its article thumbnails).  A tile whose asset has not been dropped in yet just stays
	// empty until the file appears (see the missing-asset list in the test notes).
	var SE_OPS_ICONS = {
		"9101": "file://{images}/icons/ui/operation_payback.svg",
		"9102": "file://{images}/icons/ui/operation_bravo.svg",
		"9103": "file://{images}/icons/ui/operation_phoenix.svg",
		"9104": "file://{images}/icons/ui/operation_breakout.svg",
		"9105": "file://{images}/icons/ui/operation_vanguard.svg",
		"9106": "file://{images}/icons/ui/operation_bloodhound.svg",
		"9107": "file://{images}/icons/ui/operation_wildfire.svg",
		"9108": "file://{images}/test_images/blog_hydra.png",
		"9109": "file://{images}/icons/ui/shattered_web.svg",
		"9110": "file://{images}/icons/ui/broken_fang.svg",
		"9111": "file://{images}/icons/ui/operation_11.svg"
	};

	(function installStoreItemImages() {
		if (!g.$) { return; }
		var tries = 0;
		var lastSet = 0;
		function walk(panel) {
			var set = 0;
			function rec(p) {
				if (!p) { return; }
				var id = p.id || "";
				if (id.indexOf("se_store_") === 0) {
					var def = id.substring("se_store_".length).split("_")[0];
					var src = SE_OPS_ICONS[def];
					if (src) {
						var img = p.FindChildInLayoutFile("StoreItemImage");
						if (img && img.SetImage && img.Data().seOpsIcon !== src) {
							img.SetImage(src);
							img.Data().seOpsIcon = src;
							set++;
						}
					}
				}
				var children = p.Children();
				if (children) { for (var i = 0; i < children.length; i++) { rec(children[i]); } }
			}
			rec(panel);
			return set;
		}
		function scan() {
			try {
				var n = walk($.GetContextPanel());
				if (n > 0 && n !== lastSet) { lastSet = n; log("大行动图: 已设置 " + n + " 张"); }
			} catch (e) { }
			$.Schedule(2.0, scan);
		}
		$.Schedule(1.0, scan);
	})();

	// ---------------------------------------------------------------------------------------------
	// popup_news(主菜单"未读新闻"弹窗)的关闭兜底
	// ---------------------------------------------------------------------------------------------
	// The popup is created by UiToolkitAPI.ShowCustomLayoutPopupParameters() and its close button runs
	// "$.DispatchEvent('UIPopupButtonClicked')", which the ported C++ popup (CUI_Popup) turns into a
	// close.  If that path ever fails the popup would sit on screen forever (it covers the whole menu
	// and holds mouse/keyboard capture while visible), so the button gets a safety net: 0.9s after the
	// click, if the popup is still there, delete the panel outright - the same thing
	// CUI_PopupManager::ForceClosePopups() does.  When the normal close works the popup (and this
	// callback with it) is torn down at 0.5s, so the net only fires when it is really needed.
	(function hardenNewsPopupClose() {
		if (!g.$) { return; }
		$.Schedule(0.5, function () {
			try {
				if (!g.PopupNews) { return; }              // only the popup_news.xml context defines it
				var root = $.GetContextPanel();
				var btn = root.FindChildTraverse('id-close-button');
				if (!btn) { return; }
				btn.SetPanelEvent('onactivate', function () {
					log("弹窗: 关闭按钮点击");
					var popup = root;
					$.Schedule(0.9, function () {
						try {
							log("弹窗: 引擎关闭未生效, 强制删除");
							popup.AddClass('Hidden');
							popup.DeleteAsync(0);
						} catch (e) { }
					});
					try { $.DispatchEvent('UIPopupButtonClicked', ''); } catch (e) { log("弹窗: 关闭事件异常 " + e); }
					try { $.DispatchEvent('PlaySoundEffect', 'UIPanorama.mainmenu_press_home', 'MOUSE'); } catch (e) { }
				});
			} catch (e) { }
		});
	})();

	// ---------------------------------------------------------------------------------------------
	// 新闻面板演示数据
	// ---------------------------------------------------------------------------------------------
	// mainmenu_news.js requests the RSS feed through BlogAPI (a Steam backend this port does not
	// have), so the news list stayed empty and unclickable.  This runs inside the news layout's
	// JavaScript context (g.NewsPanel only exists there) and hands its own OnRssFeedReceived() a
	// local demo feed - the exact object shape the C++ side used to deliver (items: date / title /
	// description / imageUrl / link / categories).  imageUrl stays empty, which makes the entries
	// fall back to the shipped store/default-news.png.  The first item is pre-marked as read so the
	// "new article" popup does not come up on every launch.
	(function seedNewsFeed() {
		// SE port (2026-09-18): se_api_shim installs a placeholder NewsPanel in *every* context, so the
		// old truthiness check passed everywhere and every context "injected" the feed (probe: the
		// same line x16).  Only the news layout's own context has the real object - and there the
		// context panel's id is JsNewsPanel.
		try { if ($.GetContextPanel().id !== 'JsNewsPanel') { return; } } catch (e) { return; }
		if (!g.NewsPanel || !g.NewsPanel.OnRssFeedReceived) { return; }   // only the news context
		if (g.__seNewsSeeded) { return; }
		g.__seNewsSeeded = true;

		var feed = {
			items: [
				{
					date: "2026-09-18",
					title: seLocalize("#SEPort_News_Title1"),
					description: seLocalize("#SEPort_News_Desc1"),
					imageUrl: "",
					link: "https://blog.counter-strike.net/",
					categories: []
				},
				{
					date: "2026-09-18",
					title: seLocalize("#SEPort_News_Title2"),
					description: seLocalize("#SEPort_News_Desc2"),
					imageUrl: "",
					link: "https://steamcommunity.com/market/",
					categories: []
				},
				{
					date: "2026-09-17",
					title: seLocalize("#SEPort_News_Title3"),
					description: seLocalize("#SEPort_News_Desc3"),
					imageUrl: "",
					link: "https://www.counter-strike.net/",
					categories: []
				}
			]
		};

		$.Schedule(1.2, function () {
			try {
				// cfg 开关: se_popup_news = 1(默认) 按内容脚本的原逻辑走 —— 第一条"未读"会弹一次
				// popup_news.xml; = 0 先把第一条预标记成"已读", 弹窗不出现。
				var bAllowPopup = true;
				try { bAllowPopup = (g.GameInterfaceAPI.GetSettingString("se_popup_news") !== "0"); } catch (e) { }
				if (!bAllowPopup) {
					g.GameInterfaceAPI.SetSettingString("ui_news_last_read_link", feed.items[0].link);
				}
				g.NewsPanel.OnRssFeedReceived(feed);
				log("新闻: 演示条目已注入 x" + feed.items.length + " (弹窗=" + (bAllowPopup ? "开" : "关") + ")");
			} catch (e) { log("新闻: 注入异常 " + e); }
		});
	})();

	// ---------------------------------------------------------------------------------------------
	// 大行动（Operation）活动页 —— CS:GO 原版的 featured 面板
	// ---------------------------------------------------------------------------------------------
	// CS:GO 的 mainmenu.js 里有一句 _AddFeaturedPanel( 'operation/operation_mainmenu.xml',
	// 'JsOperationPanel' )，把这一页塞进 #JsNewsContainer。我们这份内容里它上面的
	// bFeaturedPanelIsActive 是 false（Valve 出厂就是关的），所以这一页从没出现过；这里把它挂上。
	//
	// 页面本身（layout/operation/operation_mainmenu.xml + 它的 scripts/styles + operations/op11/
	// 素材 + videos/riptide_logo_loop.webm）全是 CS:GO 自带内容，缺的只有"后端数据"：MissionsAPI
	// 的 SeasonalOperation* 系列 + InventoryAPI 的 SeasonalOperations 缓存 / Quest* 系列。下面按
	// operation_util.js / operation_mainmenu.js / operation_mission_card.js 真正读到的字段造一套
	// 自洽的假数据：赛季 11 = 激流大行动，3 张周任务卡（前 2 周解锁）、18 星奖励轨道、6 个奖励。
	var OP_CARDS = [
		{ id: 1, name: "#SEPort_Op_Card1", points: 2, quests: [6101, 6102], timer: false },
		{ id: 2, name: "#SEPort_Op_Card2", points: 3, quests: [6103],       timer: false },
		{ id: 3, name: "#SEPort_Op_Card3", points: 3, quests: [],           timer: true  }
	];
	var OP_QUESTS = {
		6101: { item: 9201, goal: 16, remaining: 0, segs: [16],      gtype: 0, mode: "competitive", map: "mg_lobby_mapveto", single: "1" },
		6102: { item: 9202, goal: 5,  remaining: 0, segs: [5],       gtype: 0, mode: "casual",      map: "mg_casualsigma",  single: "1" },
		6103: { item: 9203, goal: 6,  remaining: 3, segs: [2, 4, 6], gtype: 1, mode: "competitive", map: "mg_lobby_mapveto", single: "0" }
	};
	var OP_TRACK_REWARDS = { 2: 1, 5: 2, 8: 3, 11: 4, 14: 5, 17: 6 };   // 星数档 -> 奖励编号
	var OP_TRACK_TIERS = 18;        // GetSeasonalOperationTrackRewardsCount
	var OP_XP_THRESHOLDS = "3,6,10";
	var OP_MISSION_BACKLOG = 2;     // 已解锁 2 周（第 3 张卡走"锁定 + 倒计时"分支）
	var OP_STARS_EARNED = 6;        // = SeasonalOperations 的 tier_unlocked
	var OP_MISSIONS_COMPLETED = 3;

	function opQuest(qid) { return OP_QUESTS[Number(qid)] || null; }
	function opQuestByItem(itemId) {
		var def = parseInt(String(itemId || "").substring(STORE_FauxPrefix.length), 10);
		for (var k in OP_QUESTS) { if (OP_QUESTS[k].item === def) { return OP_QUESTS[k]; } }
		return null;
	}
	function opValid(season) { return Number(season) === SE_OP_SEASON; }

	// --- MissionsAPI: 赛季 / 任务卡 / 任务 / 奖励轨道 ---
	g.MissionsAPI.GetSeasonalOperationMissionCardsCount = function (season) { return opValid(season) ? OP_CARDS.length : 0; };
	g.MissionsAPI.GetSeasonalOperationMissionCardDetails = function (season, idx) {
		var c = OP_CARDS[Number(idx) || 0];
		if (!opValid(season) || !c) { return null; }
		// name 交给内容侧的 $.Localize() 解析（保持跟语言切换）
		return { id: c.id, name: c.name, quests: c.quests.slice(0), operational_points: c.points, showTimer: c.timer };
	};
	g.MissionsAPI.GetSeasonalOperationMissionCardActiveIdx = function (season) { return opValid(season) ? 0 : -1; };
	g.MissionsAPI.GetSeasonalOperationTrackRewardsCount = function (season) {
		if (!g.__seOpCountLogged) { g.__seOpCountLogged = true; log("[SEOP] TrackRewardsCount asked season=" + season + " -> " + (opValid(season) ? OP_TRACK_TIERS : 0)); }
		return opValid(season) ? OP_TRACK_TIERS : 0;
	};
	g.MissionsAPI.GetSeasonalOperationTrackRewardSchema = function (season, idx, field) {
		if (!g.__seOpSchemaLogged) { g.__seOpSchemaLogged = 1; log("[SEOP] TrackRewardSchema asked season=" + season + " idx=" + idx + " field=" + field); }
		else if (g.__seOpSchemaLogged < 20) { g.__seOpSchemaLogged++; log("[SEOP]   schema idx=" + idx + " field=" + field); }
		if (!opValid(season)) { return ""; }
		var i = Number(idx) || 0;
		var n = OP_TRACK_REWARDS[i];
		if (field === "item_name" || field === "item_name_free") { return n ? ("#SEPort_Op_Reward_" + n) : ""; }
		if (field === "none") { return n ? "" : "none"; }          // 空档 ⇒ 内容侧当 gap 跳过
		if (field === "points" || field === "flags") { return String(i + 1); }
		if (field === "ui_order") { return String(Math.floor(i / 5)); }
		return "";
	};
	g.MissionsAPI.GetSeasonalOperationLoopingRewardsCount = function () { return 0; };
	g.MissionsAPI.GetSeasonalOperationRedeemableGoodsCount = function () { return 0; };
	g.MissionsAPI.GetSeasonalOperationRedeemableGoodsSchema = function () { return ""; };
	g.MissionsAPI.GetSeasonalOperationXpRewardsThresholds = function (season) { return opValid(season) ? OP_XP_THRESHOLDS : ""; };
	g.MissionsAPI.GetQuestPoints = function (qid, field) {
		var q = opQuest(qid);
		if (!q) { return 0; }
		if (field === "goal") { return q.goal; }
		if (field === "remaining") { return q.remaining; }
		if (field === "uncommitted") { return 0; }
		if (field === "count") { return q.segs.length; }
		var m = /^goal([0-9]+)$/.exec(String(field === undefined || field === null ? "" : field));
		if (m) { var n = q.segs[Number(m[1])]; return (n === undefined) ? q.goal : n; }
		return 0;
	};
	g.MissionsAPI.GetQuestDefinitionField = function (qid, field) {
		var q = opQuest(qid);
		if (!q) { return ""; }
		var i = q.item - 9200;
		if (field === "loc_description") { return "#SEPort_Op_Q" + i + "_Desc"; }
		if (field === "loc_name") { return "#SEPort_Op_Q" + i + "_Name"; }
		if (field === "operational_points") { return "1"; }
		if (field === "singlematch") { return q.single; }
		return "";
	};
	g.MissionsAPI.GetQuestGraphCount = function () { return 0; };
	g.MissionsAPI.GetQuestGraphType = function (qid) { var q = opQuest(qid); return q ? q.gtype : 0; };
	g.MissionsAPI.GetQuestGraphEntry = function () { return 0; };
	g.MissionsAPI.GetQuestGameElements = function () { return []; };   // 内容侧会 .map()
	g.MissionsAPI.ApplyQuestDialogVarsToPanelJS = function () { };

	// --- InventoryAPI: 任务物品 / SeasonalOperations 进度缓存 ---
	g.InventoryAPI.GetMissionBacklog = function () { return OP_MISSION_BACKLOG; };
	g.InventoryAPI.GetSecondsUntilNextMission = function () { return 0; };
	g.InventoryAPI.GetQuestItemIDFromQuestID = function (qid) { var q = opQuest(qid); return q ? (STORE_FauxPrefix + q.item + "_0") : ""; };
	g.InventoryAPI.GetQuestGameMode = function (itemId) { var q = opQuestByItem(itemId); return q ? q.mode : ""; };
	g.InventoryAPI.GetQuestMapGroup = function (itemId) { var q = opQuestByItem(itemId); return q ? q.map : ""; };
	g.InventoryAPI.GetQuestMap = function () { return ""; };
	g.InventoryAPI.GetCacheTypeElementIndexByKey = function (type, key) {
		return (type === 'SeasonalOperations' && Number(key) === SE_OP_SEASON) ? 0 : -1;
	};
	g.InventoryAPI.GetCacheTypeElementFieldByIndex = function (type, idx, field) {
		if (type !== 'SeasonalOperations' || Number(idx) !== 0) { return 0; }
		if (field === 'season_value') { return SE_OP_SEASON; }
		if (field === 'missions_completed') { return OP_MISSIONS_COMPLETED; }
		if (field === 'tier_unlocked') { return OP_STARS_EARNED; }
		if (field === 'premium_tiers') { return 1; }        // >0 ⇒ 拥有通行证 ⇒ 显示任务面板
		if (field === 'redeemable_balance') { return 0; }
		if (field === 'season_pass_time') { return 0; }
		return 0;
	};
	g.InventoryAPI.GetActiveSeasonPassItemId = function () { return ""; };
	g.InventoryAPI.GetActiveSeasonCoinItemId = function () { return ""; };
	// 奖励/任务名字 -> 物品定义号（内容侧拿它再拼 faux item id；见 operation_util.js::_GetRewardsData）
	g.InventoryAPI.GetItemDefinitionIndexFromDefinitionName = function (name) {
		var m = /_([0-9]+)$/.exec(String(name === undefined || name === null ? "" : name));
		return m ? (9300 + Number(m[1])) : 0;
	};
	g.MyPersonaAPI.GetMyMedalRankByType = function () { return 1; };   // 大行动勋章等级（必须非 -1）
	g.GameStateAPI.GetActiveQuestID = function () { return -1; };
	g.DeepStatsAPI.GetLastCachedMatchJS = function () { return null; };  // 不进"上一场比赛"统计分支

	// --- 挂载：把活动页放进 #JsNewsContainer（新闻下面），并收掉假商店面板 ---
	(function mountOperationPanel() {
		if (!g.$) { return; }
		if (SE_OP_SEASON <= 0) { return; }   // SE port: 用户反馈活动页"太卡" ⇒ 默认不挂（见 SE_OP_SEASON）
		var tries = 0;
		function attempt() {
			tries++;
			try {
				var root = $.GetContextPanel();
				if (!root || root.id !== 'MainMenu') { return; }        // 只有主菜单上下文有 #JsNewsContainer
				var container = $.FindChildInContext('#JsNewsContainer');
				var news = $.FindChildInContext('#JsNewsPanel');
				if (!container || !news) {                               // 等 mainmenu.js 把面板建出来
					if (tries < 80) { $.Schedule(0.25, attempt); }
					return;
				}
				if (!$.FindChildInContext('#JsOperationPanel')) {
					var el = $.CreatePanel('Panel', container, 'JsOperationPanel');
					el.BLoadLayout('file://{resources}/layout/operation/operation_mainmenu.xml', false, false);
					container.MoveChildAfter(el, news);                 // 新闻下面那块
					log("[SEOP] operation panel mounted (season=" + SE_OP_SEASON + ")");
				}
				// CS:GO 在"大行动进行中"会把新闻收起、把 #JsStorePanel 展开成 265px（.news-panel--hide-news-panel
				// 那两条规则）。这里要的是"新闻 + 下面的活动页"，所以把那个 class 去掉、并把假商店面板折起来。
				if (container.BHasClass('news-panel--hide-news-panel')) {
					container.RemoveClass('news-panel--hide-news-panel');
					log("[SEOP] removed news-panel--hide-news-panel (news stays visible)");
				}
				var store = $.FindChildInContext('#JsStorePanel');
				if (store) {
					// 每秒重压：内容侧/样式随时可能再把它拉开，所以不能只压一次。
					if (store.style.height !== '0px') { store.style.height = '0px'; }
					if (store.style.visibility !== 'collapse') { store.style.visibility = 'collapse'; }
					if (!store.__seCollapsed) {
						store.__seCollapsed = true;
						log("[SEOP] fake store panel collapsed");
					}
				}
			} catch (e) { log("[SEOP] mount failed: " + e); }
			$.Schedule(1.0, attempt);                                    // 幂等保持（内容侧可能再改回来）
		}
		attempt();
	})();

	// ---------------------------------------------------------------------------------------------
	// 大行动页的"点进去跳转"：点横幅 → 打开完整大行动页（operation_main.xml 弹窗）
	// ---------------------------------------------------------------------------------------------
	// CS:GO 里这块是照 UiToolkitAPI.ShowCustomLayoutPopupParameters 把 operation/operation_main.xml
	// 当弹窗打开的（OperationUtil.OpenPopupCustomLayoutOperationHub），但内容侧没把它绑到任何按钮上
	// （布局里 "查看奖励" 按钮与横幅背景都没有 onactivate），所以点了没反应。
	// 这里在活动页自己的 JS 上下文里补上：横幅背景 + "查看奖励" 按钮 → 打开大行动 hub 弹窗。
	// （本文件被注入到每个布局；OperationUtil 只存在于活动页那个上下文，所以这段只在
	//  context panel 是 JsOperationPanel 时生效。）
	(function wireOperationClicks() {
		if (!g.$) { return; }
		if (SE_OP_SEASON <= 0) { return; }   // 活动页没挂时不需要接线
		try { if ($.GetContextPanel().id !== 'JsOperationPanel') { return; } } catch (e) { return; }

		function openHub() {
			try {
				if (typeof OperationUtil !== 'undefined' && OperationUtil.OpenPopupCustomLayoutOperationHub) {
					OperationUtil.OpenPopupCustomLayoutOperationHub(-1);
					log("[SEOP] open operation hub popup");
				} else {
					log("[SEOP] OperationUtil not ready, hub not opened");
				}
			} catch (e) { log("[SEOP] hub popup failed: " + e); }
		}

		function bind(sel) {
			var p = $.FindChildInContext(sel);
			if (!p || p.__seOpClick) { return false; }
			p.__seOpClick = true;
			p.SetPanelEvent('onactivate', openHub);
			return true;
		}

		var tries = 0;
		function attempt() {
			tries++;
			var n = 0;
			// 可见区域：任务卡容器 / 任务卡本体（upsell 横幅在"已拥有通行证"时是隐藏的，一并绑上备用）
			if (bind('#id-op-mainmenu-missions')) { n++; }
			if (bind('#id-op-mainmenu-mission-card')) { n++; }
			if (bind('#id-op-mainmenu-upsell-bg')) { n++; }
			if (bind('#id-op-reward-open-operation-hub')) { n++; }
			if (n === 0 && tries < 40) { $.Schedule(0.25, attempt); return; }
			log("[SEOP] operation clicks wired (new=" + n + ")");
			$.Schedule(3.0, dumpRects);
		}

		// 几何探针：把活动页及几个关键子面板的绝对位置/尺寸写到日志（方便自动化点击与验收）
		function absPos(el) {
			var x = 0, y = 0, p = el;
			while (p) {
				try { x += Number(p.actualxoffset) || 0; y += Number(p.actualyoffset) || 0; } catch (e) { }
				try { p = p.GetParent(); } catch (e) { p = null; }
			}
			return { x: x, y: y };
		}
		function dumpRects() {
			var ids = ['JsOperationPanel', 'id-op-mainmenu-upsell', 'id-op-mainmenu-status',
				'id-op-mainmenu-missions', 'id-op-mainmenu-mission-card', 'id-missions-selected-card-btn',
				'id-op-mainmenu-mission-unlock'];
			for (var i = 0; i < ids.length; i++) {
				var el = $.FindChildInContext('#' + ids[i]);
				if (!el) { continue; }
				var a = absPos(el);
				var w = 0, h = 0, vis = '?';
				try { w = Math.round(Number(el.contentwidth) || 0); h = Math.round(Number(el.contentheight) || 0); } catch (e) { }
				try { vis = el.visible ? 1 : 0; } catch (e) { }
				log("[SEOP] rect " + ids[i] + " x=" + a.x + " y=" + a.y + " w=" + w + " h=" + h + " vis=" + vis);
			}
		}
		attempt();
	})();

	// ---------------------------------------------------------------------------------------------
	// 压住"假商店面板"（#JsStorePanel）
	// ---------------------------------------------------------------------------------------------
	// 主菜单左列下面那块（巴黎 2023 促销 + 历代大行动瓦片）是 CS:GO 自己的商店面板，数据是本移植
	// 用 StoreAPI 假数据填的（巴黎 2023 观众通行证/纪念包/印花 + Operation Payback…Riptide）。
	// 用户 2026-09-19 明确表示不要这种"商店市场"块（而且它还在主菜单背景的 blurrects 名单里，
	// 显示着会多出一批模糊矩形），所以这里常驻把它压成 0 高/不显示。
	// 想恢复它：删掉 this 段，或者把下面 RETURN_EARLY 改成 true 以外。
	(function keepStorePanelHidden() {
		// SE port (2026-09-19): store/market panel restored at the user's request.
		// This block used to collapse #JsStorePanel to height:0 / visibility:collapse, which is
		// why the store/market panel under the news had disappeared.  Flip the flag to true
		// to collapse it again.
		var SE_PORT_HIDE_STORE_PANEL = false;
		if (!SE_PORT_HIDE_STORE_PANEL) { return; }
		if (!g.$) { return; }
		var tries = 0;
		var logged = false;
		function attempt() {
			tries++;
			try {
				var root = $.GetContextPanel();
				if (!root || root.id !== 'MainMenu') { return; }
				var store = $.FindChildInContext('#JsStorePanel');
				if (!store) { if (tries < 80) { $.Schedule(0.25, attempt); } return; }
				if (store.style.height !== '0px') { store.style.height = '0px'; }
				if (store.style.visibility !== 'collapse') { store.style.visibility = 'collapse'; }
				if (!logged) { logged = true; log("[SEOP] fake store panel kept collapsed"); }
			} catch (e) { }
			$.Schedule(1.0, attempt);
		}
		attempt();
	})();
	// ==========================================================================
	// SE port (2026-09-19): the inventory page's data layer.
	//
	// The C++ side owns the catalog - panorama/seport/gameclient/cstrike15/panorama/se_faux_econ.cpp
	// (23 items, the four categories, the rarity and the sort order) plus csgo_inventory_item_list.cpp,
	// which builds one tile per item and hands the id over as the tile's "itemid" attribute.  These
	// answers mirror that catalog so the content's own scripts (mainmenu_inventory.js, itemtile.js,
	// common/iteminfo.js) can label and colour the tiles.  Ids are the "se_store_<def>_<paint>" strings
	// that InventoryAPI.GetFauxItemIDFromDefAndPaintIndex() builds and GetItemName() names.
	// ==========================================================================
	(function () {
		var INV_CATEGORIES = "inv_category_any,inv_category_tools,inv_category_container,inv_category_collections";
		var INV_CAT_META = {
			"inv_category_any": "Inv_Category_any",
			"inv_category_tools": "Inv_Category_tools",
			"inv_category_container": "Inv_Category_container",
			"inv_category_collections": "Inv_Category_collections"
		};
		// def index -> rarity (0 consumer .. 5 covert); mirrors k_Catalog in se_faux_econ.cpp
		var INV_RARITY = {
			"4883": 4, "4888": 3, "6732": 3,
			"9101": 4, "9102": 4, "9103": 4, "9104": 4, "9105": 4, "9106": 4,
			"9107": 4, "9108": 4, "9109": 4, "9110": 4, "9111": 4,
			"9201": 3, "9202": 3, "9203": 3,
			"9301": 5, "9302": 5, "9303": 5, "9304": 5, "9305": 5, "9306": 5
		};
		// the CS:GO rarity colours (the tile washes its rarity bar with this)
		var INV_RARITY_COLOR = ["#b0c3d9", "#5e98d9", "#4b69ff", "#8847ff", "#d32ce6", "#eb4b4b", "#e4ae39"];
		// icons, mirroring the "m_pchImage" column of se_faux_econ.cpp
		var INV_ICON = {
			"4883": "file://{images_econ}/econ/operations/op10/logo.png",
			"4888": "file://{images_econ}/econ/store/tournament_items_18.png",
			"6732": "file://{images_econ}/econ/store/get_tournament_stickers.png"
		};

		function invDef(id) {
			var s = String(id === undefined || id === null ? "" : id);
			if (s.indexOf(STORE_FauxPrefix) !== 0) { return ""; }
			return s.substring(STORE_FauxPrefix.length).split("_")[0];
		}
		function invIsKnown(id) { return INV_RARITY.hasOwnProperty(invDef(id)); }
		function invRarity(id) {
			var d = invDef(id);
			return INV_RARITY.hasOwnProperty(d) ? INV_RARITY[d] : 0;
		}

		// --- the category tree ("any" first, same order as the C++ side) -----------------------
		g.InventoryAPI.GetCategories = function () { return INV_CATEGORIES; };
		g.InventoryAPI.GetSubCategories = function (category) {
			// every category exposes the single "any" tier (se_faux_econ.cpp::GetSubCategoryCount)
			return "any";
		};
		g.InventoryAPI.GetInventoryStructureJSON = function (category, subCategory, group) {
			var token = INV_CAT_META.hasOwnProperty(category) ? INV_CAT_META[category] : "Inv_Category_any";
			return JSON.stringify({ nametoken: token, subcategories: "any" });
		};

		// --- item identity / naming ------------------------------------------------------------
		g.InventoryAPI.IsFauxItemID = function (id) { return invIsKnown(id); };
		g.InventoryAPI.IsValidItemID = function (id) { return invIsKnown(id); };
		g.InventoryAPI.IsItemInfoValid = function (id) { return invIsKnown(id); };
		g.InventoryAPI.GetItemRarityColor = function (id) {
			var r = invRarity(id);
			if (r < 0) { r = 0; }
			if (r >= INV_RARITY_COLOR.length) { r = INV_RARITY_COLOR.length - 1; }
			return INV_RARITY_COLOR[r];
		};
		g.InventoryAPI.GetItemInventoryImage = function (id) {
			var d = invDef(id);
			return INV_ICON.hasOwnProperty(d) ? INV_ICON[d] : "";
		};
		g.InventoryAPI.GetItemDefinitionName = function (id) { return invIsKnown(id) ? ("se_inv_item_" + invDef(id)) : ""; };
		g.InventoryAPI.GetItemTypeFromEnum = function () { return ""; };
		g.InventoryAPI.GetRawDefinitionKey = function () { return "0"; };

		// --- per-item state the tiles ask about -----------------------------------------------
		g.InventoryAPI.IsEquipped = function () { return false; };
		g.InventoryAPI.GetSlot = function () { return "noteam"; };
		g.InventoryAPI.GetSlotSubPosition = function () { return ""; };
		g.InventoryAPI.GetItemTeam = function () { return "noteam"; };
		g.InventoryAPI.HasCustomName = function () { return false; };
		g.InventoryAPI.DoesItemMatchDefinitionByName = function () { return false; };
		g.InventoryAPI.GetItemSessionPropertyValue = function () { return ""; };
		g.InventoryAPI.GetItemAttributeValue = function () { return ""; };
		g.InventoryAPI.GetItemStickerSlotCount = function () { return 0; };
		g.InventoryAPI.GetItemStickerCount = function () { return 0; };
		g.InventoryAPI.GetItemStickerImageByIndex = function () { return ""; };
		g.InventoryAPI.GetItemStickerNameByIndex = function () { return ""; };
		g.InventoryAPI.GetNumItemsNeededToTradeUp = function () { return 0; };
		g.InventoryAPI.GetItemGifterXuid = function () { return ""; };
		g.InventoryAPI.GetSet = function () { return ""; };
		g.InventoryAPI.GetToolType = function () { return ""; };
		g.InventoryAPI.BIsRewardPremium = function () { return false; };
		g.InventoryAPI.GetRewardTier = function () { return 0; };
		g.InventoryAPI.GetItemPickupMethod = function () { return ""; };
		g.InventoryAPI.PrecacheCustomMaterials = function () { };
		g.InventoryAPI.GetItemCapabilitiesCount = function () { return 0; };
		g.InventoryAPI.GetItemCapabilityByIndex = function () { return ""; };
		g.InventoryAPI.IsTool = function () { return false; };
		g.InventoryAPI.IsCouponCrate = function () { return false; };

		// --- the flat "query result" API the search panel uses ---------------------------------
		// mainmenu_inventory_search.js calls SetInventorySortAndFilters() and then walks
		// GetInventoryItemIDByIndex(i) / GetInventoryCount(), so this is where the port's search
		// result set is built: the same catalog the C++ list shows, filtered by the search text
		// (against the localized names, which only this side can resolve) and ordered by the sort
		// method the panel's dropdown picked.
		var INV_SORT_FALLBACK = "inv_sort_age";
		var invResult = [];

		function invAllDefs() { return Object.keys(INV_RARITY); }
		function invItemID(def) { return STORE_FauxPrefix + def + "_0"; }
		function invNameFor(def) {
			var id = invItemID(def);
			var name = "";
			try { name = ($.Localize ? $.Localize("#" + (STORE_NAMES[def] ? STORE_NAMES[def].replace("#", "") : "SEPort_Store_Item_Fallback")) : ""); } catch (e) { name = ""; }
			return String(name === undefined || name === null ? "" : name);
		}
		function invRebuild() {
			var defs = invAllDefs();
			var search = String(INV_SORT.search || "").toLowerCase();
			if (search.length > 0) {
				defs = defs.filter(function (d) {
					return invNameFor(d).toLowerCase().indexOf(search) >= 0;
				});
			}
			var sortType = String(INV_SORT.sortType || INV_SORT_FALLBACK);
			if (sortType === "inv_sort_alpha") {
				defs.sort(function (a, b) { return invNameFor(a) < invNameFor(b) ? -1 : (invNameFor(a) > invNameFor(b) ? 1 : 0); });
			} else if (sortType === "inv_sort_rarity" || sortType === "inv_sort_quality") {
				defs.sort(function (a, b) { return (INV_RARITY[b] || 0) - (INV_RARITY[a] || 0); });
			}
			invResult = defs.map(invItemID);
		}
		var INV_SORT = { sortType: INV_SORT_FALLBACK, search: "" };
		g.InventoryAPI.SetInventorySortAndFilters = function (sortType, bForce, searchText) {
			INV_SORT.sortType = sortType || INV_SORT_FALLBACK;
			INV_SORT.search = String(searchText === undefined || searchText === null ? "" : searchText);
			invRebuild();
		};
		g.InventoryAPI.GetInventoryCount = function () { return invResult.length; };
		g.InventoryAPI.GetInventoryItemIDByIndex = function (i) {
			var idx = Number(i) || 0;
			return (idx >= 0 && idx < invResult.length) ? invResult[idx] : "";
		};
		invRebuild();

		// --- the loadout API the item panel asks for (no loadouts in this build) ----------------
		function loadoutNoop() { }
		// mainmenu_inventory.js::_UpdateLoadoutButtonState() does
		// "elInvLoadoutBtn.enabled = LoadoutAPI.IsLoadoutAllowed()", so this must be a real bool and it
		// has to be true once the loadout panel (CCSGO_Loadout) exists - false disables the button and
		// shows #tooltip_loadout_disabled on hover.
		g.LoadoutAPI.IsLoadoutAllowed = function () { return true; };
		g.LoadoutAPI.IsItemInShuffleForTeam = function () { return false; };
		g.LoadoutAPI.IsShuffleEnabled = function () { return false; };
		g.LoadoutAPI.IsShuffleAllowed = function () { return false; };
		g.LoadoutAPI.CountItemsInInventoryForShuffleSlot = function () { return 0; };
		g.LoadoutAPI.GetItemID = function () { return ""; };
		g.LoadoutAPI.GetDefaultItem = function () { return ""; };
		g.LoadoutAPI.GetItemGamePrice = function () { return 0; };
		g.LoadoutAPI.AddItemToShuffle = loadoutNoop;
		g.LoadoutAPI.RemoveItemFromShuffle = loadoutNoop;
		g.LoadoutAPI.ClearShuffle = loadoutNoop;
		g.LoadoutAPI.SetShuffleEnabled = loadoutNoop;
		g.LoadoutAPI.ShuffleEquipmentInSlot = loadoutNoop;
		g.LoadoutAPI.EquipItemInSlot = loadoutNoop;

		if (!g.__seInvLogged) {
			g.__seInvLogged = true;
			log("库存: 数据层已安装 (categories=" + INV_CATEGORIES + ", items=" + Object.keys(INV_RARITY).length + ")");
		}
	})();

	// SE port (2026-09-19): the handful of extra answers the loadout panel's script asks for
	// (loadout.js: GetItemBaseName / shuffle helpers).  The port has no loadout state, so the
	// "which item is equipped / in a shuffle" answers are empty and the helpers are no-ops.
	(function () {
		function invBaseName(id) {
			var s = String(id === undefined || id === null ? "" : id);
			if (s.indexOf(STORE_FauxPrefix) !== 0) { return ""; }
			var def = s.substring(STORE_FauxPrefix.length).split("_")[0];
			// loadout.js does $.Localize( InventoryAPI.GetItemBaseName( id ) ) - it wants a token
			return "#" + (STORE_NAMES[def] ? STORE_NAMES[def].replace("#", "") : "SEPort_Store_Item_Fallback");
		}
		g.InventoryAPI.GetItemBaseName = invBaseName;
		g.LoadoutAPI.ShuffleAllForTeam = function () { };
		g.LoadoutAPI.GetItemCountInShuffle = function () { return 0; };
		g.LoadoutAPI.SetShuffleEnabled = function () { };

		if (!g.__seLoadoutLogged) {
			g.__seLoadoutLogged = true;
			log("配装: loadout 数据层已安装 (无槽位状态, 物品列表复用库存目录)");
		}
	})();

	// SE port (2026-09-19): name the two catalog groups whose def indices are not in the store table
	// (9201-9203 = the operation's quests, 9301-9306 = its rewards).  They use the same SE_port
	// tokens the operation panel already shows, so the tiles read as part of the same operation.
	(function () {
		var EXTRA_NAMES = {
			"9201": "#SEPort_Op_Q1_Name",
			"9202": "#SEPort_Op_Q2_Name",
			"9203": "#SEPort_Op_Q3_Name",
			"9301": "#SEPort_Op_Reward_1",
			"9302": "#SEPort_Op_Reward_2",
			"9303": "#SEPort_Op_Reward_3",
			"9304": "#SEPort_Op_Reward_4",
			"9305": "#SEPort_Op_Reward_5",
			"9306": "#SEPort_Op_Reward_6"
		};
		var prevGetName = g.InventoryAPI.GetItemName;
		g.InventoryAPI.GetItemName = function (id) {
			var s = String(id === undefined || id === null ? "" : id);
			if (s.indexOf(STORE_FauxPrefix) === 0) {
				var def = s.substring(STORE_FauxPrefix.length).split("_")[0];
				if (EXTRA_NAMES.hasOwnProperty(def)) { return seLocalize(EXTRA_NAMES[def]); }
			}
			return prevGetName ? prevGetName(id) : "";
		};
		var prevBase = g.InventoryAPI.GetItemBaseName;
		g.InventoryAPI.GetItemBaseName = function (id) {
			var s = String(id === undefined || id === null ? "" : id);
			if (s.indexOf(STORE_FauxPrefix) === 0) {
				var def = s.substring(STORE_FauxPrefix.length).split("_")[0];
				if (EXTRA_NAMES.hasOwnProperty(def)) { return EXTRA_NAMES[def]; }
			}
			return prevBase ? prevBase(id) : "";
		};
	})();

})();
