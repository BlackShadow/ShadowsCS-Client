// player_preview.cpp - see player_preview.h.
//
// The backdrop is gfx/shadows/preview_assault.png (912 x 1102): the middle of it, as many pixels square as the box, at
// its own size (the menu's own file loader only takes pictures up to 512 x 512, so it is decoded here and handed over as
// RGBA). The player preview stands on it (the crosshair's has its own banner). The players are the game's own
// models, read and posed by studio_preview.cpp the way the game poses them (upper body aiming the weapon, legs in the
// idle animation, the p_ weapon model on the hand bones) and drawn by studio_preview_gl.cpp over the backdrop (the
// engine's model renderer only works while a map is drawn, and this also shows in the main menu). Each team's models are
// read the first time they are shown. The radar and crosshair previews draw with the HUD's own code (radar2_draw.cpp,
// crosshair_draw.cpp). The crosshair's is a banner across the top of its tab: gfx/shadows/crosshair_banner.png (a
// de_aztec screenshot, tools/banner/crosshair_banner.py) at its own pixel size, the middle of the box on the spot the
// screenshot's own crosshair was (kBannerAim), both ends fading into the window.
#include "hud.h"
#include "cl_util.h"
#include "vgui_controls/Controls.h"
#include "FileSystem.h"
#include "vgui/ISurface.h"
#include "vgui/IScheme.h"
#include "vgui/IPanel.h"
#include "vgui/IInput.h"
#include "vgui/MouseCode.h"
#include "player_preview.h"
#include "../studio_preview.h"
#include "../studio_preview_gl.h"
#include "../player_fit.h"
#include "../vismods.h"
#include "../radar2.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include <string>
#include <vector>

unsigned char* VisMods_LoadPicture( const char* path, int* w, int* h );	// vismods.cpp: RGBA, free() it

namespace
{

const char kPicturePath[] = "gfx/shadows/preview_assault.png";
// the players' start angles, degrees about their vertical axis (positive: turned to the right): the CT on the left
// looking left, the Terrorist on the right looking right
const float kLeftYaw = -45.0f;
const float kRightYaw = 45.0f;
const float kDragSpeed = 0.5f;		// degrees per pixel dragged
const float kBoxAspect = 1.0f;		// the player preview is square
// the two players share the whole box: each centred a quarter of the box's width from the middle (the box shows
// 72 / 0.86 world units from bottom to top, as wide as tall), so neither is cut off where the other's side begins
const float kSideOffset = 72.0f / 0.86f * 0.25f;
// the crosshair banner
const char kBannerPath[] = "gfx/shadows/crosshair_banner.png";
const int kBannerAim[2] = { 1161, 352 };	// where the screenshot's crosshair was: the box's middle shows it
const float kBannerFade = 0.18f;			// each end fades out over this much of the banner's width
const int kBannerPictureTall = 643;		// its height until it is read (tools/banner/crosshair_banner.py's)

// ---------------------------------------------------------------------------------------------------------------
// the backdrop

std::vector<unsigned char> g_picture;	// the whole picture, RGBA (kept: the box changes size while the window does)
int g_pictureWide, g_pictureTall;
bool g_bPictureTried;
int g_iBackdrop = -1;					// surface texture of the piece cut for the box
int g_iBackdropSide;					// the box size it was cut for

// the backdrop's texture for a box 'side' pixels square, -1 when the picture is missing
int BackdropTexture( int side )
{
	if ( side <= 0 )
		return -1;
	if ( !g_bPictureTried )
	{
		g_bPictureTried = true;
		int w = 0, h = 0;
		unsigned char* rgba = VisMods_LoadPicture( kPicturePath, &w, &h );
		if ( rgba && w > 0 && h > 0 )
		{
			g_picture.assign( rgba, rgba + (size_t)w * h * 4 );
			g_pictureWide = w;
			g_pictureTall = h;
		}
		free( rgba );
	}
	if ( g_picture.empty() )
		return -1;
	if ( g_iBackdrop >= 0 && side == g_iBackdropSide )
		return g_iBackdrop;
	// the middle, at its own size (all of the width when the box is bigger than the picture)
	int piece = side < g_pictureWide ? side : g_pictureWide;
	piece = piece < g_pictureTall ? piece : g_pictureTall;
	const int left = ( g_pictureWide - piece ) / 2, top = ( g_pictureTall - piece ) / 2;
	std::vector<unsigned char> cut( (size_t)piece * piece * 4 );
	for ( int y = 0; y < piece; y++ )
		memcpy( &cut[(size_t)y * piece * 4], &g_picture[( (size_t)( top + y ) * g_pictureWide + left ) * 4], (size_t)piece * 4 );
	if ( g_iBackdrop < 0 )
		g_iBackdrop = vgui2::surface()->CreateNewTextureID();
	vgui2::surface()->DrawSetTextureRGBA( g_iBackdrop, cut.data(), piece, piece, 1, true );
	g_iBackdropSide = side;
	return g_iBackdrop;
}

// the banner picture, decoded once (kept: the box changes size with the window)
std::vector<unsigned char> g_banner;
int g_bannerWide, g_bannerTall;
bool g_bBannerTried;
int g_iBannerTexture = -1;
int g_iBannerCutWide, g_iBannerCutTall;		// the box size its texture was cut for
int g_iBannerPieceWide, g_iBannerPieceTall;	// the piece cut (smaller than the box when the picture is)
int g_iBannerPieceX, g_iBannerPieceY;		// where the piece goes in the box, putting kBannerAim in the middle

// the piece of the banner picture for a wide x tall box: at its own pixel size round kBannerAim, both ends fading out;
// -1 when the picture is missing
int BannerTexture( int wide, int tall )
{
	if ( wide <= 0 || tall <= 0 )
		return -1;
	if ( !g_bBannerTried )
	{
		g_bBannerTried = true;
		int w = 0, h = 0;
		unsigned char* rgba = VisMods_LoadPicture( kBannerPath, &w, &h );
		if ( rgba && w > 0 && h > 0 )
		{
			g_banner.assign( rgba, rgba + (size_t)w * h * 4 );
			g_bannerWide = w;
			g_bannerTall = h;
		}
		free( rgba );
	}
	if ( g_banner.empty() )
		return -1;
	if ( g_iBannerTexture >= 0 && wide == g_iBannerCutWide && tall == g_iBannerCutTall )
		return g_iBannerTexture;
	const int pieceWide = wide < g_bannerWide ? wide : g_bannerWide;
	const int pieceTall = tall < g_bannerTall ? tall : g_bannerTall;
	int left = kBannerAim[0] - wide / 2, top = kBannerAim[1] - tall / 2;
	left = left < 0 ? 0 : left > g_bannerWide - pieceWide ? g_bannerWide - pieceWide : left;
	top = top < 0 ? 0 : top > g_bannerTall - pieceTall ? g_bannerTall - pieceTall : top;
	std::vector<unsigned char> cut( (size_t)pieceWide * pieceTall * 4 );
	const float fade = pieceWide * kBannerFade;
	for ( int y = 0; y < pieceTall; y++ )
	{
		memcpy( &cut[(size_t)y * pieceWide * 4], &g_banner[( (size_t)( top + y ) * g_bannerWide + left ) * 4], (size_t)pieceWide * 4 );
		for ( int x = 0; x < pieceWide; x++ )
		{
			float t = ( x + 0.5f ) / fade;
			const float u = ( pieceWide - x - 0.5f ) / fade;
			t = t < u ? t : u;
			if ( t >= 1.0f )
				continue;
			const float s = t * t * ( 3.0f - 2.0f * t );	// smooth both ways
			cut[( (size_t)y * pieceWide + x ) * 4 + 3] = (unsigned char)( 255.0f * s );
		}
	}
	if ( g_iBannerTexture < 0 )
		g_iBannerTexture = vgui2::surface()->CreateNewTextureID();
	vgui2::surface()->DrawSetTextureRGBA( g_iBannerTexture, cut.data(), pieceWide, pieceTall, 1, true );
	g_iBannerCutWide = wide;
	g_iBannerCutTall = tall;
	g_iBannerPieceWide = pieceWide;
	g_iBannerPieceTall = pieceTall;
	g_iBannerPieceX = wide / 2 - ( kBannerAim[0] - left );
	g_iBannerPieceY = tall / 2 - ( kBannerAim[1] - top );
	return g_iBannerTexture;
}

void PaintBackdrop( int side )
{
	const int texture = BackdropTexture( side );
	if ( texture >= 0 )
	{
		vgui2::surface()->DrawSetColor( Color( 255, 255, 255, 255 ) );
		vgui2::surface()->DrawSetTexture( texture );
		vgui2::surface()->DrawTexturedRect( 0, 0, side, side );
	}
	else
	{
		vgui2::surface()->DrawSetColor( Color( 28, 32, 26, 255 ) );
		vgui2::surface()->DrawFilledRect( 0, 0, side, side );
	}
}

// the thin frame round the player preview's box, like the boxes around the other controls
void PaintFrame( int side )
{
	vgui2::surface()->DrawSetColor( Color( 0, 0, 0, 170 ) );
	vgui2::surface()->DrawOutlinedRect( 0, 0, side, side );
}

int BoxSide( vgui2::Panel* panel )
{
	int wide, tall;
	panel->GetSize( wide, tall );
	return wide < tall ? wide : tall;
}

// ---------------------------------------------------------------------------------------------------------------
// the players

// per team (CPlayerPreview::Team): the player, the weapon in hand (a weapon model's file name) and the animation of the
// upper body aiming it
struct TeamModels
{
	const char* player;
	const char* weapon;
	const char* aim;
};
const TeamModels kTeams[] = {
	{ "models/player/leet/leet.mdl", "p_ak47.mdl", "ref_aim_ak47" },
	{ "models/player/gign/gign.mdl", "p_m4a1.mdl", "ref_aim_rifle" },
	{ "models/player/vip/vip.mdl", "p_usp.mdl", "ref_aim_onehanded" },
};
const int kNumTeams = (int)( sizeof( kTeams ) / sizeof( kTeams[0] ) );
const char kLegs[] = "idle1";		// standing still

// browsing (CPlayerPreview::SetBrowsing): every player model, in the order the team menus list them, then the VIP;
// each holds its team's weapon (kTeams), in its own skins
struct BrowsedPlayer
{
	const char* name;
	const char* player;
	int team;	// CPlayerPreview::Team
};
const BrowsedPlayer kBrowsed[] = {
	{ "Seal Team 6", "models/player/urban/urban.mdl", CPlayerPreview::TEAM_CT },
	{ "GSG-9", "models/player/gsg9/gsg9.mdl", CPlayerPreview::TEAM_CT },
	{ "SAS", "models/player/sas/sas.mdl", CPlayerPreview::TEAM_CT },
	{ "GIGN", "models/player/gign/gign.mdl", CPlayerPreview::TEAM_CT },
	{ "Phoenix Connexion", "models/player/terror/terror.mdl", CPlayerPreview::TEAM_T },
	{ "Elite Crew", "models/player/leet/leet.mdl", CPlayerPreview::TEAM_T },
	{ "Arctic Avengers", "models/player/arctic/arctic.mdl", CPlayerPreview::TEAM_T },
	{ "Guerilla Warfare", "models/player/guerilla/guerilla.mdl", CPlayerPreview::TEAM_T },
	{ "VIP", "models/player/vip/vip.mdl", CPlayerPreview::TEAM_VIP },
};
const int kNumBrowsed = (int)( sizeof( kBrowsed ) / sizeof( kBrowsed[0] ) );
const float kBrowseYaw = -30.0f;	// the one player browsed to, turned a little to its left

// the weapon view: the M4A1 firing (without the silencer), over and over
const char kViewWeapon[] = "v_m4a1.mdl";
const char kViewAnimation[] = "shoot1_unsil";

// the weapon (and player) models' sets by number (CPlayerPreview::SetWeapons / SetPlayers; vismods.cpp swaps the same
// files in game)
const char* const kWeaponSets[] = { NULL, "retail", "beta" };

// player models by the file read (a map: the GL skin cache keys on their addresses, which stay put), with the sideways
// move centring player and weapon at the start angle
struct PlayerModel
{
	CStudioPreviewModel model;
	float shift = 0.0f;
	bool tried = false;
};
std::map<std::string, PlayerModel> g_playerModels;
// weapon models by the file read and the animation (a map: the GL skin cache keys on their addresses, which stay put)
std::map<std::string, CStudioPreviewModel> g_weaponModels;

// is the data a studio model whose own length does not reach past what was read?
bool WholeModel( const byte* file, int length )
{
	return file && length >= 76 && *(const int*)( file + 72 ) <= length;
}

// gamePath (if given): a player model - the game's model it is drawn for (itself, or the one a set's model stands in
// for). One of the beta set is drawn bigger, as in game (vismods.cpp): its growth is planned against that model's hit
// boxes (player_fit.cpp)
void LoadModel( const char* path, const char* sequence, const char* legs, CStudioPreviewModel& model,
	const char* gamePath = NULL )
{
	int length = 0;
	byte* file = gEngfuncs.COM_LoadFile( (char*)path, 5, &length );
	if ( !file )
		return;
	if ( model.Load( file, length, sequence, legs ) && gamePath && !strnicmp( path, "models/shadows/beta/", 20 )
		 && WholeModel( file, length ) )
	{
		int gameLength = 0;
		byte* game = stricmp( path, gamePath ) ? gEngfuncs.COM_LoadFile( (char*)gamePath, 5, &gameLength ) : NULL;
		if ( !game || WholeModel( game, gameLength ) )
		{
			PlayerFit fit;
			PlayerFit_Plan( file, game ? game : file, &fit );
			model.SetFit( fit );
		}
		if ( game )
			gEngfuncs.COM_FreeFile( game );
	}
	gEngfuncs.COM_FreeFile( file );
}

// a file the game has, asked of its file system as the class and buy menus do (COM_ExpandFilename can say yes to a file
// that is not there)
bool FileExists( const std::string& path )
{
	return vgui2::filesystem()->FileExists( path.c_str() );
}

// a weapon model of a set: models/shadows/<set>/<file> when the set has it (the beta set: else the retail set's, as
// vismods.cpp does), else the game's models/<file>
std::string WeaponPath( int set, const char* file )
{
	const int tries[2] = { set, set == 2 ? 1 : 0 };
	for ( int t = 0; t < 2; t++ )
	{
		if ( tries[t] <= 0 || tries[t] >= (int)( sizeof( kWeaponSets ) / sizeof( kWeaponSets[0] ) ) )
			continue;
		const std::string own = std::string( "models/shadows/" ) + kWeaponSets[tries[t]] + "/" + file;
		if ( FileExists( own ) )
			return own;
	}
	return std::string( "models/" ) + file;
}

// a model read once per file and animation (may not be loaded when the file does not read)
const CStudioPreviewModel& ModelAt( const std::string& path, const char* sequence )
{
	const std::string key = path + "|" + ( sequence ? sequence : "" );
	std::map<std::string, CStudioPreviewModel>::iterator known = g_weaponModels.find( key );
	if ( known != g_weaponModels.end() )
		return known->second;
	CStudioPreviewModel& model = g_weaponModels[key];
	LoadModel( path.c_str(), sequence, NULL, model );
	return model;
}

// a weapon model of a set (WeaponPath). rightHanded (if given): a view model made for the right hand - CS 1.5's and
// the game's knife; the game's other weapons are made for the left one (the rule vismods.cpp gives the game)
const CStudioPreviewModel& WeaponModel( int set, const char* file, const char* sequence, bool* rightHanded = NULL )
{
	const std::string path = WeaponPath( set, file );
	if ( rightHanded )
		*rightHanded = !strnicmp( path.c_str(), "models/shadows/retail/", 22 ) || !stricmp( path.c_str(), "models/v_knife.mdl" );
	return ModelAt( path, sequence );
}

// the set a model file comes from: retail, beta or game (the game's own models)
const char* SetOfPath( const std::string& path )
{
	if ( !strnicmp( path.c_str(), "models/shadows/retail/", 22 ) )
		return "retail";
	if ( !strnicmp( path.c_str(), "models/shadows/beta/", 20 ) )
		return "beta";
	return "game";
}

// a player model of a set: models/shadows/<set>/player/... when the set has it (the beta set: else the retail set's,
// as vismods.cpp does), else the game's models/player/...
std::string PlayerPath( int set, const char* path )
{
	const int tries[2] = { set, set == 2 ? 1 : 0 };
	for ( int t = 0; t < 2; t++ )
	{
		if ( tries[t] <= 0 || tries[t] >= (int)( sizeof( kWeaponSets ) / sizeof( kWeaponSets[0] ) ) )
			continue;
		const std::string own = std::string( "models/shadows/" ) + kWeaponSets[tries[t]] + "/" + ( path + 7 );
		if ( FileExists( own ) )
			return own;
	}
	return path;
}

// the weapon of a set in the hand of the player read from playerPath: a gun made for the other skeleton's hold is
// moved in the hand as the game moves it (vismods.cpp, models/shadows/beta/hold.txt) - a model of its own per hold
const CStudioPreviewModel& HeldWeapon( int set, const char* file, const std::string& playerPath )
{
	const std::string path = WeaponPath( set, file );
	const float* move = VisMods_HeldMoveByPath( path.c_str(), playerPath.c_str() );
	if ( !move )
		return ModelAt( path, NULL );
	const std::string key = path + "|held by " + SetOfPath( playerPath );
	std::map<std::string, CStudioPreviewModel>::iterator known = g_weaponModels.find( key );
	if ( known != g_weaponModels.end() )
		return known->second;
	CStudioPreviewModel& model = g_weaponModels[key];
	LoadModel( path.c_str(), NULL, NULL, model );
	if ( model.IsLoaded() )
		model.SetHandMove( move );
	return model;
}

// a player model (of the player set) aiming the team's weapon of the weapon set, and the sideways move centring them at
// that angle (either may not be loaded)
const CStudioPreviewModel& PosedPlayer( const char* file, int team, int weaponSet, int playerSet, float yaw,
	const CStudioPreviewModel** weapon, float* shift )
{
	const std::string path = PlayerPath( playerSet, file );
	char angle[16];
	snprintf( angle, sizeof( angle ), "|%g", yaw );
	PlayerModel& player = g_playerModels[path + angle];
	if ( !player.tried )
	{
		player.tried = true;
		LoadModel( path.c_str(), kTeams[team].aim, kLegs, player.model, file );
		player.shift = StudioPreview_CentreShift( player.model, &WeaponModel( 0, kTeams[team].weapon, NULL ), yaw,
			kBoxAspect );
	}
	if ( !player.model.IsLoaded() && playerSet != 0 )
		return PosedPlayer( file, team, weaponSet, 0, yaw, weapon, shift );	// the set's did not read: the game's
	*weapon = &HeldWeapon( weaponSet, kTeams[team].weapon, path );
	*shift = player.shift;
	return player.model;
}

// the team's player (of the player set), the weapon set's weapon in its hand and the sideways move (the weapon may not
// be loaded; the player falls back to the Terrorist)
const CStudioPreviewModel& TeamPlayer( int team, int weaponSet, int playerSet, float yaw, const CStudioPreviewModel** weapon,
	float* shift )
{
	if ( team < 0 || team >= kNumTeams )
		team = 0;
	const CStudioPreviewModel& player = PosedPlayer( kTeams[team].player, team, weaponSet, playerSet, yaw, weapon, shift );
	if ( !player.IsLoaded() && team != 0 )
		return TeamPlayer( 0, weaponSet, playerSet, yaw, weapon, shift );
	return player;
}

} // namespace

CPlayerPreview::CPlayerPreview( vgui2::Panel* parent, const char* name ) : vgui2::Panel( parent, name )
{
	m_bSolid = false;
	for ( int team = 0; team < 3; team++ )
		m_Colors[team][0] = m_Colors[team][1] = m_Colors[team][2] = 1.0f;
	m_bHead = false;
	m_HeadColor[0] = m_HeadColor[1] = m_HeadColor[2] = 1.0f;
	m_iWeaponSet = 0;
	m_iPlayerSet = 0;
	m_bWeaponView = false;
	m_bBrowsing = false;
	m_iBrowsed = 0;
	m_flYaw = 0.0f;
	m_bDragging = false;
	m_iDragX = 0;
	SetPaintBackgroundEnabled( true );
	SetMouseInputEnabled( true );
	SetKeyBoardInputEnabled( false );
}

void CPlayerPreview::SetLook( bool solid, const int colors[3][3] )
{
	m_bSolid = solid;
	for ( int team = 0; team < 3; team++ )
	{
		for ( int i = 0; i < 3; i++ )
		{
			const int v = colors[team][i];
			m_Colors[team][i] = ( v < 0 ? 0 : v > 255 ? 255 : v ) / 255.0f;
		}
	}
}

void CPlayerPreview::SetHeadColor( bool on, int r, int g, int b )
{
	m_bHead = on;
	m_HeadColor[0] = ( r < 0 ? 0 : r > 255 ? 255 : r ) / 255.0f;
	m_HeadColor[1] = ( g < 0 ? 0 : g > 255 ? 255 : g ) / 255.0f;
	m_HeadColor[2] = ( b < 0 ? 0 : b > 255 ? 255 : b ) / 255.0f;
}

void CPlayerPreview::SetWeapons( int set, bool showWeapon )
{
	m_iWeaponSet = set;
	m_bWeaponView = showWeapon;
}

void CPlayerPreview::SetPlayers( int set )
{
	if ( set != m_iPlayerSet )
		m_flYaw = 0.0f;	// other models: shown at their start angles, centred
	m_iPlayerSet = set;
}

void CPlayerPreview::SetBrowsing( bool on )
{
	if ( on != m_bBrowsing )
		m_flYaw = 0.0f;	// the other view: shown at its start angles
	m_bBrowsing = on;
}

void CPlayerPreview::StepModel( int step )
{
	m_iBrowsed = ( ( m_iBrowsed + step ) % kNumBrowsed + kNumBrowsed ) % kNumBrowsed;
	m_flYaw = 0.0f;
}

void CPlayerPreview::GetBrowsedName( char* buffer, int size )
{
	const BrowsedPlayer& shown = kBrowsed[m_iBrowsed];
	const char* set = SetOfPath( PlayerPath( m_iPlayerSet, shown.player ) );
	snprintf( buffer, size, "%s (%s)", shown.name, !strcmp( set, "beta" ) ? "Beta" : !strcmp( set, "retail" ) ? "Retail" : "Original" );
}

void CPlayerPreview::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	BaseClass::ApplySchemeSettings( scheme );
	SetBgColor( Color( 0, 0, 0, 0 ) );
	SetBorder( NULL );
}

void CPlayerPreview::PaintBackground()
{
	PaintBackdrop( BoxSide( this ) );
}

void CPlayerPreview::Paint()
{
	const int side = BoxSide( this );
	int x = 0, y = 0, screenWide = 0, screenTall = 0;
	vgui2::ipanel()->GetAbsPos( GetVPanel(), x, y );
	vgui2::surface()->GetScreenSize( screenWide, screenTall );
	vgui2::surface()->DrawFlushText();
	if ( m_bWeaponView )
	{
		// the set's M4A1 firing in first person, with its normal skins (the team and head colours are for players), in
		// the hand cl_righthand asks for: mirrored when the model is made for the other one, as the game does
		bool rightHanded = false;
		const CStudioPreviewModel& view = WeaponModel( m_iWeaponSet, kViewWeapon, kViewAnimation, &rightHanded );
		const bool wantRight = gEngfuncs.pfnGetCvarFloat( (char*)"cl_righthand" ) > 0.0f;
		StudioPreview_Draw( view, NULL, gEngfuncs.GetAbsoluteTime(), x, y, side, side, screenWide, screenTall, 0.0f, 0.0f,
			false, m_Colors[TEAM_T], NULL, true, rightHanded != wantRight, !wantRight );
		PaintFrame( side );
		return;
	}

	if ( m_bBrowsing )
	{
		// the one player browsed to, in the middle (dragging turns it), always with its own skins: this view is for
		// looking at the models, so solid team colours (and the head colour) are not put on them here
		const BrowsedPlayer& shown = kBrowsed[m_iBrowsed];
		const CStudioPreviewModel* weapon = NULL;
		float shift = 0.0f;
		const CStudioPreviewModel& player = PosedPlayer( shown.player, shown.team, m_iWeaponSet, m_iPlayerSet, kBrowseYaw,
			&weapon, &shift );
		StudioPreview_Draw( player, weapon, gEngfuncs.GetAbsoluteTime(), x, y, side, side, screenWide, screenTall,
			kBrowseYaw + m_flYaw, shift, false, m_Colors[shown.team], NULL );
		PaintFrame( side );
		return;
	}

	// the players over the backdrop (the menu draws immediately, in order): the CT on the left looking left, the
	// Terrorist on the right looking right, both over the whole box, a quarter of it either side of the middle
	// (dragging turns both)
	const Team teams[2] = { TEAM_CT, TEAM_T };
	const float yaws[2] = { kLeftYaw, kRightYaw };
	const float sides[2] = { -kSideOffset, kSideOffset };
	for ( int i = 0; i < 2; i++ )
	{
		const CStudioPreviewModel* weapon = NULL;
		float shift = 0.0f;
		const CStudioPreviewModel& player = TeamPlayer( teams[i], m_iWeaponSet, m_iPlayerSet, yaws[i], &weapon, &shift );
		StudioPreview_Draw( player, weapon, gEngfuncs.GetAbsoluteTime(), x, y, side, side, screenWide, screenTall,
			yaws[i] + m_flYaw, shift + sides[i], m_bSolid, m_Colors[teams[i]], m_bHead ? m_HeadColor : NULL );
	}
	PaintFrame( side );
}

void CPlayerPreview::OnMousePressed( vgui2::MouseCode code )
{
	if ( code != vgui2::MOUSE_LEFT || m_bWeaponView )	// the weapon view does not turn
	{
		BaseClass::OnMousePressed( code );
		return;
	}
	int cursorY = 0;
	vgui2::input()->GetCursorPos( m_iDragX, cursorY );
	m_bDragging = true;
	vgui2::input()->SetMouseCapture( GetVPanel() );
}

void CPlayerPreview::OnMouseReleased( vgui2::MouseCode code )
{
	if ( code != vgui2::MOUSE_LEFT || !m_bDragging )
	{
		BaseClass::OnMouseReleased( code );
		return;
	}
	m_bDragging = false;
	vgui2::input()->SetMouseCapture( 0 );
}

void CPlayerPreview::OnCursorMoved( int x, int y )
{
	if ( !m_bDragging )
	{
		BaseClass::OnCursorMoved( x, y );
		return;
	}
	int cursorX = 0, cursorY = 0;
	vgui2::input()->GetCursorPos( cursorX, cursorY );
	m_flYaw = fmodf( m_flYaw + ( cursorX - m_iDragX ) * kDragSpeed, 360.0f );
	m_iDragX = cursorX;
}

// ---------------------------------------------------------------------------------------------------------------
// radar preview
// ---------------------------------------------------------------------------------------------------------------

CRadarPreview::CRadarPreview( vgui2::Panel* parent, const char* name ) : vgui2::Panel( parent, name )
{
	m_bSquare = true;
	m_flSize = 150.0f;
	m_flRange = 1000.0f;
	m_flAlpha = RADAR_MAP_OPACITY_DEFAULT_VALUE;
	m_bRotate = true;
	SetPaintBackgroundEnabled( false );
	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );
}

void CRadarPreview::SetSettings( bool square, float size, float range, float mapAlpha, bool rotate )
{
	m_bSquare = square;
	m_flSize = size;
	m_flRange = range;
	m_flAlpha = mapAlpha;
	m_bRotate = rotate;
}

void CRadarPreview::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	BaseClass::ApplySchemeSettings( scheme );
	SetBgColor( Color( 0, 0, 0, 0 ) );
	SetBorder( NULL );
}

void CRadarPreview::Paint()
{
	// just the radar, filling the box (the square radar brings its own frame)
	const int box = BoxSide( this );
	int x = 0, y = 0, screenWide = 0, screenTall = 0;
	vgui2::ipanel()->GetAbsPos( GetVPanel(), x, y );
	vgui2::surface()->GetScreenSize( screenWide, screenTall );
	vgui2::surface()->DrawFlushText();
	Radar2_DrawPreview( x, y, box, screenWide, screenTall, m_bSquare, m_flSize, m_flRange, m_flAlpha, m_bRotate,
		(float)gEngfuncs.GetAbsoluteTime() );
}

// ---------------------------------------------------------------------------------------------------------------
// crosshair preview
// ---------------------------------------------------------------------------------------------------------------

CCrosshairPreview::CCrosshairPreview( vgui2::Panel* parent, const char* name ) : vgui2::Panel( parent, name )
{
	memset( &m_settings, 0, sizeof( m_settings ) );
	m_settings.r = 50;
	m_settings.g = 250;
	m_settings.b = 50;
	m_settings.translucent = true;
	m_settings.thickness = 1;
	m_settings.dynamic = true;
	m_settings.recoil = true;
	SetPaintBackgroundEnabled( true );
	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );
}

void CCrosshairPreview::SetSettings( const CrosshairSettings& settings )
{
	m_settings = settings;
}

void CCrosshairPreview::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	BaseClass::ApplySchemeSettings( scheme );
	SetBgColor( Color( 0, 0, 0, 0 ) );
	SetBorder( NULL );
}

int CCrosshairPreview::MostTall()
{
	const int tall = g_bannerTall > 0 ? g_bannerTall : kBannerPictureTall;
	const int above = kBannerAim[1], below = tall - kBannerAim[1];
	return 2 * ( above < below ? above : below );
}

void CCrosshairPreview::PaintBackground()
{
	// the banner picture, its ends fading into the window (no frame); nothing when the picture is missing
	int wide, tall;
	GetSize( wide, tall );
	const int texture = BannerTexture( wide, tall );
	if ( texture < 0 )
		return;
	vgui2::surface()->DrawSetColor( Color( 255, 255, 255, 255 ) );
	vgui2::surface()->DrawSetTexture( texture );
	vgui2::surface()->DrawTexturedRect( g_iBannerPieceX, g_iBannerPieceY, g_iBannerPieceX + g_iBannerPieceWide,
		g_iBannerPieceY + g_iBannerPieceTall );
}

void CCrosshairPreview::Paint()
{
	// the crosshair at its real size in the middle, where the screenshot's own one was
	int wide, tall;
	GetSize( wide, tall );
	m_routine.Advance( gEngfuncs.GetAbsoluteTime(), m_settings );
	int x = 0, y = 0, screenWide = 0, screenTall = 0;
	vgui2::ipanel()->GetAbsPos( GetVPanel(), x, y );
	vgui2::surface()->GetScreenSize( screenWide, screenTall );
	vgui2::surface()->DrawFlushText();
	Crosshair_DrawPreview( m_settings, m_routine, x, y, wide, tall, screenWide, screenTall );
}
