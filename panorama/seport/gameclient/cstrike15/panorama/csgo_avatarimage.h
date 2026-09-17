//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: SE port of game/client/cstrike15/panorama/csgo_avatarimage.h - the player avatar image
//          panel (type "CSGOAvatarImage") that the CS:GO layouts put in the player card, the friend
//          tiles and the end-of-match podium.
//
//          CS:GO fills this panel from Steam (ISteamFriends::GetLarge/Medium/SmallFriendAvatar ->
//          ISteamUtils::GetImageRGBA -> IImageSource).  This build has no Steam at all (stub_steam,
//          -insecure), so the image comes from a file on disk instead, looked up under the mod's
//          image tree:
//
//              materials/panorama/images/avatars/<steam id 64>.png
//              materials/panorama/images/avatars/<account id>.png
//              materials/panorama/images/avatars/local.png
//              ... and if none of those exist, the panel's own "defaultsrc" attribute, which is what
//              CS:GO shows while it waits for Steam.
//
//          ".jpg" and ".svg" are accepted as well as ".png".  See SE_PortAvatarFileExists() in the
//          .cpp - that function is the whole difference from CS:GO's data path.
//
//          What is kept from CS:GO: the panel class and its JS surface (steamid/accountid accessors,
//          Clear/SetNotifiyAvatarLoaded/SetDefaultImage methods), the "steamid"/"accountid"/
//          "notifyavatarloaded"/"defaultsrc"/"scaling" properties, the inner CImagePanel that does the
//          actual drawing, the ReloadAllAvatarImages / CSGOAvatarImageLoaded events, and the
//          IAvatarImageMgr that groups the panels for the GOTV "bits provider" path.
//
//=============================================================================//

#pragma once

#include "panorama/iavatarimagemgr.h"

#include "panorama/controls/image.h"

// SE port: CS:GO gets CSteamID through clientsteamcontext.h -> steam/steam_api.h.  The layouts and
// the JS still speak in SteamIDs (avatar.js assigns `elImage.steamid = xuid`), so the type is needed
// even though nothing here talks to Steam.
#include "steam/steamclientpublic.h"

DECLARE_PANEL_EVENT0( CSGOAvatarImageLoaded );

//-----------------------------------------------------------------------------
// Purpose: Avatar image for a single CS:GO player, based on the DOTA equivalent
//-----------------------------------------------------------------------------
class CCSGO_AvatarImage : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CCSGO_AvatarImage, panorama::CPanel2D );

public:
	CCSGO_AvatarImage( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_AvatarImage();

	virtual bool SetSteamID( const CSteamID &steamID );
	const CSteamID &GetSteamID() const { return m_steamID; }

	// Convenience methods for using just an account id
	bool SetAccountID( uint32 unAccountID );
	uint32 GetAccountID() const { return m_steamID.GetAccountID(); }

	// Set a default image from a URL (file:///, http:///).
	// The default image is used when there is no avatar image for the given steam id.
	void SetDefaultImage( const char *pchImageURL );

	// Property handling
	virtual bool BSetProperty( panorama::CPanoramaSymbol symName, const char *pchValue ) OVERRIDE;

	// JS Bindings
	virtual void SetupJavascriptObjectTemplate() OVERRIDE;
	CUtlString JSGetSteamID() const;
	void JSSetSteamID( CUtlString steamID );
	CUtlString JSGetAccountID() const;
	void JSSetAccountID( CUtlString accountID );

	void SetNotifyAvatarLoaded( bool bNotify = true ) { m_bNotifyAvatarLoaded = bNotify; }

	// Override for painting
	virtual void Paint() OVERRIDE;

	// Handle styles changing
	virtual void OnStylesChanged() OVERRIDE;

	// Handle layout
	virtual void OnLayoutTraverse( float flFinalWidth, float flFinalHeight ) OVERRIDE;

	bool EventReloadImages();
	void ReloadImages();
	void UnloadImages();

private:
	void ClearAvatar();

	// SE port: the one place that decides what this panel shows (CS:GO: GetSteamImage() asking Steam
	// for the small/medium/large avatar handle).
	// pstrBestGuess (optional) receives the most specific file name even when nothing could be
	// confirmed on disk, so the caller can still hand it to the image panel.
	CUtlString GetAvatarImageURL( CUtlString *pstrBestGuess = NULL ) const;

	bool OnImageLoaded( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::IImageSource *pImage );
	bool OnReadyForDisplay( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel );
	bool OnUnreadyForDisplay( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel );

	// SE port: CS:GO's in-memory avatar bits provider (GOTV demo playback attaches one through
	// IAvatarImageMgr::SetImageBitsProvider).  Kept, because it is the only way to feed real pixels
	// into this panel in a build without Steam.
	struct ImageDataInMemory_t
	{
		explicit ImageDataInMemory_t( const CSteamID &steamID );
		~ImageDataInMemory_t();
		unsigned char *pRGBA;
		uint32 nWidth, nHeight;
	};
	void LoadAvatarInternal( ImageDataInMemory_t *pImageDataInMemory );

	CSteamID m_steamID;
	bool m_bNotifyAvatarLoaded;
	panorama::CImagePanel *m_pAvatarImage;
	CUtlString m_defaultImageURL;

	// SE port: set whenever the id / default image / visibility changed, so Paint() resolves the avatar
	// file once instead of once per frame.
	bool m_bAvatarDirty;

	// SE port: "a real SteamID had no avatar file" is reported once per panel (see GetAvatarImageURL,
	// which is const - hence mutable).
	mutable bool m_bWarnedNoAvatarFile;

	// The URL we last handed to the inner image panel, so a repaint does not restart the load.
	CUtlString m_strAvatarURL;
};

//-----------------------------------------------------------------------------
// Purpose: Avatar image manager. Functionality to access all CCSGO_AvatarImage
//          instances for a given steam id.
//
// SE port: CS:GO keys this by SteamID and hangs ISteamFriends/ISteamUtils callbacks
// (AvatarImageLoaded_t / PersonaStateChange_t) off it to reload the panels when Steam
// delivers a new avatar.  Neither the per-SteamID cache nor the Steam callbacks exist
// here - the image is a file that only changes when the file changes - so this keeps
// the parts the rest of the port uses: the bits provider map (GOTV) and the panel list
// that ReloadAllAvatarImages() walks.
//-----------------------------------------------------------------------------
class CCSGO_AvatarImageMgr : public IAvatarImageMgr
{
public:
	CCSGO_AvatarImageMgr();
	~CCSGO_AvatarImageMgr() { Cleanup(); }

	virtual void SetImageBitsProvider( uint32 accountID, AvatarImageBitsProvider pfnBitsProvider ) OVERRIDE;
	virtual void Cleanup() OVERRIDE;

	AvatarImageBitsProvider GetImageBitsProvider( CSteamID steamID );

	void AddAvatarImagePanel( CCSGO_AvatarImage *pImage );
	void RemoveAvatarImagePanel( CCSGO_AvatarImage *pImage );

	void ReloadAvatarImagePanels() const;

private:
	CUtlMap< uint32, AvatarImageBitsProvider, unsigned short > m_mapBitsProviders;
	CUtlVector< CCSGO_AvatarImage* > m_vecImages;
};

extern CCSGO_AvatarImageMgr g_AvatarImageMgr;
