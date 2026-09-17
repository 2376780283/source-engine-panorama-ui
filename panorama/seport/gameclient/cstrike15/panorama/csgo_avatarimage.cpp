//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: SE port of game/client/cstrike15/panorama/csgo_avatarimage.cpp - the player avatar image
//          panel (type "CSGOAvatarImage").
//
//          The Steam half of CS:GO's version is gone: this build has no Steam (stub_steam, -insecure),
//          so instead of asking ISteamFriends for the small/medium/large avatar handle and turning the
//          RGBA bits from ISteamUtils into an IImageSource, the panel looks for a file:
//
//              <mod>/materials/panorama/images/avatars/<steam id 64>.png     (.jpg / .svg too)
//              <mod>/materials/panorama/images/avatars/<account id>.png
//              <mod>/materials/panorama/images/avatars/local.png
//              ... and if none of them exists, the panel's own "defaultsrc" attribute, which is what
//              CS:GO shows while Steam is still fetching the avatar.
//
//          Everything else - the class shape, the JS surface, the properties, the inner CImagePanel,
//          the two events and the IAvatarImageMgr - is CS:GO's, with "SE port" notes where the Steam
//          callbacks used to be.
//
//=============================================================================//

#include "panorama/se_gameclient_common.h"
#include "panorama/csgo_avatarimage.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

REGISTER_PANEL2D_FACTORY( CCSGO_AvatarImage, CSGOAvatarImage );

DEFINE_PANORAMA_EVENT( CSGOAvatarImageLoaded );

DECLARE_PANORAMA_EVENT0( ReloadAllAvatarImages );
DEFINE_PANORAMA_EVENT( ReloadAllAvatarImages );

CCSGO_AvatarImageMgr g_AvatarImageMgr;

using namespace panorama;

//-----------------------------------------------------------------------------
// Purpose: CS:GO's "hide avatar images" setting.  CS:GO dispatches ReloadAllAvatarImages() here;
// the panels re-resolve their image, which in this port means "fall back to defaultsrc".
//-----------------------------------------------------------------------------
static void SE_PortHideAvatarImagesChanged( IConVar *pVar, const char *pszOldValue, float flOldValue )
{
	g_AvatarImageMgr.ReloadAvatarImagePanels();
}
ConVar cl_hide_avatar_images( "cl_hide_avatar_images", "0", FCVAR_ARCHIVE, "Hide avatar images for other players. \n\t0 - Off.\n\t1 - Block All\n\t2 - Block all but friends", SE_PortHideAvatarImagesChanged );

//-----------------------------------------------------------------------------
// SE port: cl_hide_avatar_images is CS:GO's "hide other players' avatars" setting.  In CS:GO value 2
// means "block everyone who is not on my Steam friend list"; with no Steam there is no friend list, so
// 2 behaves like 1 here (everyone gets the default image).
//-----------------------------------------------------------------------------
static bool SE_PortShouldBlockAvatar( const CSteamID &steamID )
{
	(void)steamID;
	return cl_hide_avatar_images.GetInt() != 0;
}

//-----------------------------------------------------------------------------
// SE port: the avatar file lookup - the only place this differs from CS:GO's data path.
//
// {images} resolves to the mod's "materials/panorama/images" (panorama/panorama.cfg), so the avatar of
// SteamID 76561198000000001 is <mod>/materials/panorama/images/avatars/76561198000000001.png.  The
// files are plain images (png/jpg/svg), i.e. they can simply be dropped into the mod - no VPK, no
// vtex build step, and no Steam depot involved.
//
// NOTE on the existence test: this used to ask GetLocalPathForRelativePath() and hand its result to
// UIFileSystem()->FileExists().  That returned "not found" for files that are definitely there (the
// panel then quietly fell back to the layout's defaultsrc, which is the little white player
// silhouette) - so the test now asks the same question several ways, mirroring what the (working)
// localization lookup does, and accepts any of them:
//   * the named path from panorama.cfg                     ("materials/panorama/images/...", forward slashes)
//   * the named path with backslashes                      ("materials\panorama\images\...")
//   * the engine's own file system, on the "GAME" path     (g_pFullFileSystem, loose file or VPK)
//-----------------------------------------------------------------------------
static const char *k_pchSEPortAvatarDir = "avatars";
static const char *k_pchSEPortAvatarImagesPathFallback = "materials/panorama/images";
static const char *k_pchSEPortAvatarExtensions[] = { "png", "jpg", "svg" };

static bool SE_PortAvatarFileExists( const char *pchBaseName, CUtlString &strURLOut )
{
	if ( !pchBaseName || !pchBaseName[0] )
		return false;

	const char *pchImagesPath = UIEngine()->GetLocalPathForNamedPath( "{images}" );
	if ( !pchImagesPath || !pchImagesPath[0] )
	{
		pchImagesPath = k_pchSEPortAvatarImagesPathFallback;
	}

	for ( int iExt = 0; iExt < V_ARRAYSIZE( k_pchSEPortAvatarExtensions ); ++iExt )
	{
		const char *pchExt = k_pchSEPortAvatarExtensions[ iExt ];

		CUtlString strRelative;
		strRelative.Format( "%s/%s.%s", k_pchSEPortAvatarDir, pchBaseName, pchExt );

		CUtlString strNamedPath;
		strNamedPath.Format( "%s/%s", pchImagesPath, strRelative.String() );

		char szNamedPathSlashed[MAX_PATH];
		V_strncpy( szNamedPathSlashed, strNamedPath.String(), sizeof( szNamedPathSlashed ) );
		V_FixSlashes( szNamedPathSlashed, '\\' );

		bool bFound = UIEngine()->UIFileSystem()->FileExists( strNamedPath.String() )
			|| UIEngine()->UIFileSystem()->FileExists( szNamedPathSlashed )
			|| ( g_pFullFileSystem && g_pFullFileSystem->FileExists( strNamedPath.String(), "GAME" ) );

		if ( !bFound )
		{
			// Last try: whatever the engine's named-path helper builds by itself (it also follows the
			// "overwrite" paths registered from the mod).
			CUtlString strLocalPath;
			UIEngine()->GetLocalPathForRelativePath( "{images}", strRelative.String(), strLocalPath );
			bFound = !strLocalPath.IsEmpty() && UIEngine()->UIFileSystem()->FileExists( strLocalPath.String() );
		}

		if ( bFound )
		{
			strURLOut.Format( "file://{images}/%s/%s.%s", k_pchSEPortAvatarDir, pchBaseName, pchExt );
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CCSGO_AvatarImage::CCSGO_AvatarImage( panorama::CPanel2D *pParent, const char *pchID )
	: panorama::CPanel2D( pParent, pchID )
{
	m_pAvatarImage = new panorama::CImagePanel( this, NULL );
	m_pAvatarImage->SetScaling( k_EImageScalingStretchBothToFitPreserveAspectRatio );

	m_pAvatarImage->AddClass( "AvatarImage" );
	RegisterEventHandlerOnPanel( ImageLoaded(), m_pAvatarImage->UIPanel(), this, &CCSGO_AvatarImage::OnImageLoaded );

	m_bAvatarDirty = false;
	m_bWarnedNoAvatarFile = false;
	m_bNotifyAvatarLoaded = false;

	RegisterForReadyEvents( true );

	if ( !UIEngine()->BHaveEventHandlersRegisteredForType( CCSGO_AvatarImage::GetPanelSymbol() ) )
	{
		RegisterEventHandlerOnPanelType( ReadyForDisplay(), &CCSGO_AvatarImage::OnReadyForDisplay );
		RegisterEventHandlerOnPanelType( UnreadyForDisplay(), &CCSGO_AvatarImage::OnUnreadyForDisplay );
	}
	RegisterForUnhandledEvent( ReloadAllAvatarImages(), this, &CCSGO_AvatarImage::EventReloadImages );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCSGO_AvatarImage::~CCSGO_AvatarImage()
{
	g_AvatarImageMgr.RemoveAvatarImagePanel( this );
}

//
// Initialize from global/external bits provider
//
CCSGO_AvatarImage::ImageDataInMemory_t::ImageDataInMemory_t( const CSteamID &steamID )
{
	V_memset( this, 0, sizeof( *this ) );

	if ( steamID.IsValid() )
	{
		AvatarImageBitsProvider pBitsProvider = g_AvatarImageMgr.GetImageBitsProvider( steamID );
		if ( pBitsProvider )
		{
			if ( !pBitsProvider( steamID.ConvertToUint64(), &pRGBA, &nWidth, &nHeight ) )
			{
				V_memset( this, 0, sizeof( *this ) );
			}
		}
	}
}

CCSGO_AvatarImage::ImageDataInMemory_t::~ImageDataInMemory_t()
{
	if ( pRGBA )
	{
		free( pRGBA );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Triggers loading and displaying a player's avatar
//-----------------------------------------------------------------------------
bool CCSGO_AvatarImage::SetSteamID( const CSteamID &steamID )
{
	if ( m_steamID == steamID )
	{
		if ( m_bNotifyAvatarLoaded )
		{
			DispatchEventAsync( 0.0f, CSGOAvatarImageLoaded(), this );
		}

		return false;
	}

	Assert( steamID == CSteamID() || steamID.IsValid() );

	m_steamID = steamID;

	g_AvatarImageMgr.AddAvatarImagePanel( this );

	// Explicitly clear the avatar image so that you don't see stale avatars
	// for controls that are re-used between different users.
	m_pAvatarImage->Clear();

	m_strAvatarURL = "";
	m_bAvatarDirty = true;

	LoadAvatarInternal( NULL );

	return true;
}

bool CCSGO_AvatarImage::SetAccountID( uint32 unAccountID )
{
	return SetSteamID( CSteamID( unAccountID, k_EUniversePublic, k_EAccountTypeIndividual ) );
}

void CCSGO_AvatarImage::SetDefaultImage( const char *pchImageURL )
{
	m_defaultImageURL = pchImageURL;

	// Force a reload
	ClearAvatar();
}

bool CCSGO_AvatarImage::BSetProperty( CPanoramaSymbol symName, const char *pchValue )
{
	static const CPanoramaSymbol k_symSteamID( "steamid" );
	static const CPanoramaSymbol k_symAccountID( "accountid" );
	static const CPanoramaSymbol k_symNotifyAvatarLoaded( "notifyavatarloaded" );
	static const CPanoramaSymbol k_symDefaultSource( "defaultsrc" );
	static const CPanoramaSymbol k_symScaling( "scaling" );

	if ( symName == k_symSteamID )
	{
		CSteamID steamID;
		// SE port: CS:GO resolves "local" through ClientSteamContext().GetLocalPlayerSteamID(), which
		// this build does not have.  An invalid SteamID makes the lookup fall through to the
		// "local" avatar file, which is exactly what should be shown for the local player here.
		if ( V_stricmp( pchValue, "local" ) )
			steamID = CSteamID( V_atoui64( pchValue ) );

		SetSteamID( steamID );
		return true;
	}
	else if ( symName == k_symAccountID )
	{
		uint32 unAccountID = 0;
		if ( V_stricmp( pchValue, "local" ) )
			unAccountID = (uint32)V_atoui64( pchValue );

		SetAccountID( unAccountID );
		return true;
	}
	else if ( symName == k_symNotifyAvatarLoaded )
	{
		return CSSHelpers::BParseTrueFalse( pchValue, &m_bNotifyAvatarLoaded );
	}
	else if ( symName == k_symDefaultSource )
	{
		if ( pchValue && pchValue[0] != '\0' )
		{
			SetDefaultImage( pchValue );
		}
		return true;
	}
	else if ( symName == k_symScaling )
	{
		m_pAvatarImage->SetScaling( pchValue );
		return true;
	}
	else
	{
		return BaseClass::BSetProperty( symName, pchValue );
	}
}

void CCSGO_AvatarImage::SetupJavascriptObjectTemplate()
{
	BaseClass::SetupJavascriptObjectTemplate();

	panorama::RegisterJSAccessor( "steamid", PANORAMA_DELEGATE( &CCSGO_AvatarImage::JSGetSteamID ), PANORAMA_DELEGATE( &CCSGO_AvatarImage::JSSetSteamID ) );
	panorama::RegisterJSAccessor( "accountid", PANORAMA_DELEGATE( &CCSGO_AvatarImage::JSGetAccountID ), PANORAMA_DELEGATE( &CCSGO_AvatarImage::JSSetAccountID ) );

	panorama::RegisterJSMethod( "Clear", PANORAMA_DELEGATE( &CCSGO_AvatarImage::UnloadImages ) );
	panorama::RegisterJSMethod( "SetNotifiyAvatarLoaded", PANORAMA_DELEGATE( &CCSGO_AvatarImage::SetNotifyAvatarLoaded ) );
	panorama::RegisterJSMethod( "SetDefaultImage", PANORAMA_DELEGATE( &CCSGO_AvatarImage::SetDefaultImage ) );
}

CUtlString CCSGO_AvatarImage::JSGetSteamID() const
{
	CUtlString result;
	result.Format( "%llu", m_steamID.ConvertToUint64() );
	return result;
}

void CCSGO_AvatarImage::JSSetSteamID( CUtlString steamID )
{
	// SE port: CS:GO calls the free function ConvertToUint64() from its steamid helpers; V_atoui64()
	// (tier1) is the same parse and is already available here.
	uint64 unSteamID = V_atoui64( steamID.String() );
	SetSteamID( CSteamID( unSteamID ) );
}

CUtlString CCSGO_AvatarImage::JSGetAccountID() const
{
	CUtlString result;
	result.Format( "%u", m_steamID.GetAccountID() );
	return result;
}

void CCSGO_AvatarImage::JSSetAccountID( CUtlString accountID )
{
	SetAccountID( (uint32)V_atoui64( accountID.String() ) );
}

void CCSGO_AvatarImage::Paint()
{
	// SE port: CS:GO's Paint() lazily starts the Steam request; here it resolves the avatar file.  It
	// only runs when something invalidated the choice, so a repaint does not hit the file system.
	if ( m_bAvatarDirty && BIsVisible() && !BIsTransparent() )
	{
		LoadAvatarInternal( NULL );
	}

	BaseClass::Paint();
}

//-----------------------------------------------------------------------------
// Purpose: Clears image data
//-----------------------------------------------------------------------------
void CCSGO_AvatarImage::ClearAvatar()
{
	m_strAvatarURL = "";
	m_bAvatarDirty = true;
	LoadAvatarInternal( NULL );
}

//-----------------------------------------------------------------------------
// Purpose: Handle style changes (the default image can come from a style rule)
//-----------------------------------------------------------------------------
void CCSGO_AvatarImage::OnStylesChanged()
{
	if ( BIsVisible() )
	{
		m_bAvatarDirty = true;
	}

	BaseClass::OnStylesChanged();
}

//-----------------------------------------------------------------------------
// Purpose: Handle layout
//-----------------------------------------------------------------------------
void CCSGO_AvatarImage::OnLayoutTraverse( float flFinalWidth, float flFinalHeight )
{
	BaseClass::OnLayoutTraverse( flFinalWidth, flFinalHeight );

	// CS:GO picks the small/medium/large Steam avatar by the size it is drawn at.  A file has no size
	// variants, so there is nothing to re-pick here.
}

bool CCSGO_AvatarImage::EventReloadImages()
{
	UnloadImages();
	ReloadImages();
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Panorama is telling us we should reload our image now
//-----------------------------------------------------------------------------
void CCSGO_AvatarImage::ReloadImages()
{
	m_bAvatarDirty = true;

	if ( BIsVisible() )
	{
		LoadAvatarInternal( NULL );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Panorama is telling us it wants us to free images for a while
//-----------------------------------------------------------------------------
void CCSGO_AvatarImage::UnloadImages()
{
	m_pAvatarImage->Clear();
	m_strAvatarURL = "";
	m_bAvatarDirty = true;
}

//-----------------------------------------------------------------------------
// Purpose: The URL this panel should be showing.  CS:GO: GetSteamImage() + the Steam bits provider.
//
// SE port: pstrBestGuess receives the most specific avatar file name whether or not the existence
// tests above could see it.  The avatar panels must never end up with "nothing": the file system the
// existence test asks is not always able to see a loose file in the mod (which is what a modded
// avatar is), and the visible result of a failed lookup is an error marker where the avatar should be.
//-----------------------------------------------------------------------------
CUtlString CCSGO_AvatarImage::GetAvatarImageURL( CUtlString *pstrBestGuess ) const
{
	const char *pchImagesPathForLog = UIEngine()->GetLocalPathForNamedPath( "{images}" );
	if ( !pchImagesPathForLog || !pchImagesPathForLog[0] )
	{
		pchImagesPathForLog = k_pchSEPortAvatarImagesPathFallback;
	}

	CUtlString strURL;

	bool bHaveSteamIDName = false;
	CUtlString strSteamIDName;
	if ( m_steamID.IsValid() )
	{
		uint64 unSteamID = m_steamID.ConvertToUint64();
		if ( unSteamID != 0 )
		{
			strSteamIDName.Format( "%llu", unSteamID );
			bHaveSteamIDName = true;
		}
	}

	// the candidates, most specific first
	const char *rgpchCandidates[3] = { NULL, NULL, "local" };
	CUtlString strAccountIDName;
	if ( m_steamID.IsValid() && m_steamID.GetAccountID() != 0 )
	{
		strAccountIDName.Format( "%u", m_steamID.GetAccountID() );
	}

	if ( bHaveSteamIDName )
		rgpchCandidates[0] = strSteamIDName.String();
	if ( !strAccountIDName.IsEmpty() )
		rgpchCandidates[1] = strAccountIDName.String();

	for ( int i = 0; i < V_ARRAYSIZE( rgpchCandidates ); ++i )
	{
		if ( !rgpchCandidates[i] || !rgpchCandidates[i][0] )
			continue;

		if ( SE_PortAvatarFileExists( rgpchCandidates[i], strURL ) )
			return strURL;
	}

	// Nothing confirmed.  Give the caller the most specific name anyway (see the note above), and warn
	// once when we were handed a real SteamID - that is the local player's own card, the one panel
	// where a missing picture is a mistake rather than the normal case for a placeholder id.
	if ( pstrBestGuess )
	{
		if ( bHaveSteamIDName )
			pstrBestGuess->Format( "file://{images}/%s/%s.png", k_pchSEPortAvatarDir, strSteamIDName.String() );
		else
			pstrBestGuess->Format( "file://{images}/%s/local.png", k_pchSEPortAvatarDir );
	}

	if ( m_steamID.IsValid() && !m_bWarnedNoAvatarFile )
	{
		m_bWarnedNoAvatarFile = true;
		Warning( "CCSGO_AvatarImage: could not confirm an avatar file for steam id %llu under %s/%s - looked at %s/{id,accountid}.{png,jpg,svg}, %s/local.{png,jpg,svg} (using '%s' and falling back to the default image)\n",
			m_steamID.ConvertToUint64(), pchImagesPathForLog, k_pchSEPortAvatarDir, pchImagesPathForLog, pchImagesPathForLog,
			pstrBestGuess->String() );
	}

	// CS:GO's behaviour while Steam is still fetching the avatar.
	return m_defaultImageURL;
}

//-----------------------------------------------------------------------------
// Purpose: Triggers loading and displaying a player's avatar
//-----------------------------------------------------------------------------
void CCSGO_AvatarImage::LoadAvatarInternal( ImageDataInMemory_t *pImageDataInMemory )
{
	if ( SE_PortShouldBlockAvatar( m_steamID ) )
	{
		// Just use a default if we're hiding avatar images
		CUtlString strDefault = m_defaultImageURL.IsEmpty() ? CUtlString( "file://{images}/icons/ui/player.svg" ) : m_defaultImageURL;
		m_pAvatarImage->SetImageJS( strDefault.String() );
		m_strAvatarURL = strDefault;
		m_bAvatarDirty = false;
		return;
	}

	if ( !pImageDataInMemory )
	{
		// recurse with pre-loaded bits (GOTV demo playback)
		ImageDataInMemory_t imgDataInMemory( m_steamID );
		LoadAvatarInternal( &imgDataInMemory );
		return;
	}

	if ( pImageDataInMemory->pRGBA )
	{
		unsigned char *pRGBA = pImageDataInMemory->pRGBA;
		uint32 nWidth = pImageDataInMemory->nWidth, nHeight = pImageDataInMemory->nHeight;

		uint32 unBytes = nWidth * nHeight * 4;
		CUtlBuffer bufRGBA( pRGBA, unBytes );

		// The image loader uses the put position to work out the size
		bufRGBA.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );
		bufRGBA.SeekPut( CUtlBuffer::SEEK_HEAD, unBytes );

		m_pAvatarImage->SetImage( bufRGBA, nWidth, nHeight, m_defaultImageURL.IsEmpty() ? NULL : m_defaultImageURL.String() );

		m_strAvatarURL = "";
		m_bAvatarDirty = false;
		return;
	}

	CUtlString strBestGuess;
	CUtlString strURL = GetAvatarImageURL( &strBestGuess );

	if ( strURL.IsEmpty() )
	{
		// Nothing confirmed on disk.  Hand the most specific name to the inner image panel *with* the
		// layout's defaultsrc as its fallback image: if the file is there after all (the existence
		// check asks a file system that cannot see every path form a mod can use), the picture shows
		// up; if it really is missing, the panel draws the default art instead of an error marker.
		const char *pchDefault = m_defaultImageURL.IsEmpty() ? "file://{images}/icons/ui/player.svg" : m_defaultImageURL.String();

		CUtlString strTry = strBestGuess.IsEmpty() ? CUtlString( "file://{images}/icons/ui/player.svg" ) : strBestGuess;
		if ( strTry != m_strAvatarURL )
		{
			m_pAvatarImage->SetImage( strTry.String(), pchDefault, false );
			m_strAvatarURL = strTry;
		}

		m_bAvatarDirty = false;
		return;
	}

	if ( strURL != m_strAvatarURL )
	{
		m_pAvatarImage->SetImageJS( strURL.String() );
		m_strAvatarURL = strURL;
	}

	m_bAvatarDirty = false;
}

//-----------------------------------------------------------------------------
// Purpose: called when our avatar image is ready to display
//-----------------------------------------------------------------------------
bool CCSGO_AvatarImage::OnImageLoaded( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::IImageSource *pImage )
{
	if ( pPanel.Get() == m_pAvatarImage->UIPanel() )
	{
		InvalidateSizeAndPosition();
		if ( m_bNotifyAvatarLoaded )
		{
			DispatchEventAsync( 0.0f, CSGOAvatarImageLoaded(), this );
		}
	}

	// Always return false so CImagePanel handler gets called next
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: called when we should prepare the avatar for display
//-----------------------------------------------------------------------------
bool CCSGO_AvatarImage::OnReadyForDisplay( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel )
{
	ReloadImages();
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: called when the avatar is no longer needed to display
//-----------------------------------------------------------------------------
bool CCSGO_AvatarImage::OnUnreadyForDisplay( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel )
{
	UnloadImages();
	return true;
}

//-----------------------------------------------------------------------------
//
// CCSGO_AvatarImageMgr
//
//-----------------------------------------------------------------------------

bool SE_PortAvatarAccountIDLessFunc( const uint32 &left, const uint32 &right )
{
	return left < right;
}

CCSGO_AvatarImageMgr::CCSGO_AvatarImageMgr()
	: m_mapBitsProviders( 0, 0, SE_PortAvatarAccountIDLessFunc )
{
}

//-----------------------------------------------------------------------------
// Purpose: Register the provider of in-memory avatar bits (GOTV demos)
//-----------------------------------------------------------------------------
void CCSGO_AvatarImageMgr::SetImageBitsProvider( uint32 accountID, AvatarImageBitsProvider pfnBitsProvider )
{
	m_mapBitsProviders.InsertOrReplace( accountID, pfnBitsProvider );

	// Reload all associated images
	ReloadAvatarImagePanels();
}

//-----------------------------------------------------------------------------
// Purpose: Forget every bits provider (CBaseClientState::Clear in CS:GO)
//-----------------------------------------------------------------------------
void CCSGO_AvatarImageMgr::Cleanup()
{
	m_mapBitsProviders.RemoveAll();
}

AvatarImageBitsProvider CCSGO_AvatarImageMgr::GetImageBitsProvider( CSteamID steamID )
{
	int iEntry = m_mapBitsProviders.Find( steamID.GetAccountID() );
	if ( !m_mapBitsProviders.IsValidIndex( iEntry ) )
		return NULL;

	return m_mapBitsProviders[ iEntry ];
}

void CCSGO_AvatarImageMgr::AddAvatarImagePanel( CCSGO_AvatarImage *pImage )
{
	if ( !pImage || m_vecImages.Find( pImage ) != m_vecImages.InvalidIndex() )
		return;

	m_vecImages.AddToTail( pImage );
}

void CCSGO_AvatarImageMgr::RemoveAvatarImagePanel( CCSGO_AvatarImage *pImage )
{
	int iImage = m_vecImages.Find( pImage );
	if ( iImage != m_vecImages.InvalidIndex() )
	{
		m_vecImages.Remove( iImage );
	}
}

void CCSGO_AvatarImageMgr::ReloadAvatarImagePanels() const
{
	int nImages = m_vecImages.Count();
	for ( int i = 0; i < nImages; ++i )
	{
		m_vecImages[ i ]->UnloadImages();
		m_vecImages[ i ]->ReloadImages();
	}
}
