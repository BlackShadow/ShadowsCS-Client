/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
// Ammo.cpp
//
// implementation of CHudAmmo class
//

#include "hud.h"
#include "vismods.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "pm_shared.h"

#include <string.h>
#include <ctype.h>
#include <stdio.h>

#include "ammohistory.h"
#include "../public/game_controls/iviewport.h"
#include "VGUI/icstrikeviewport.h"
#include "shared_util.h"
#include "weapontype.h"
#include "const.h"
#include "triangleapi.h"
#include "r_studioint.h"
#include "crosshair_draw.h"

extern engine_studio_api_t IEngineStudio;

// (the software renderer's TriAPI works in world space, so icons stay at their own size there)
// a full magazine (the game's iMaxClip, dlls/wpn_shared/wpn_*.cpp); other weapons: the most seen in it so far
static int WeaponClipSize( int id, int clip )
{
	switch ( id )
	{
	case WEAPON_DEAGLE:
	case WEAPON_XM1014:
		return 7;
	case WEAPON_M3:
		return 8;
	case WEAPON_AWP:
	case WEAPON_SCOUT:
		return 10;
	case WEAPON_USP:
		return 12;
	case WEAPON_P228:
		return 13;
	case WEAPON_GLOCK18:
	case WEAPON_FIVESEVEN:
	case WEAPON_G3SG1:
		return 20;
	case WEAPON_FAMAS:
	case WEAPON_UMP45:
		return 25;
	case WEAPON_AK47:
	case WEAPON_AUG:
	case WEAPON_ELITE:
	case WEAPON_M4A1:
	case WEAPON_MAC10:
	case WEAPON_MP5N:
	case WEAPON_SG550:
	case WEAPON_SG552:
	case WEAPON_TMP:
		return 30;
	case WEAPON_GALIL:
		return 35;
	case WEAPON_P90:
		return 50;
	case WEAPON_M249:
		return 100;
	}
	static int s_seen[MAX_WEAPONS];
	if ( id < 0 || id >= MAX_WEAPONS )
		return clip > 0 ? clip : 1;
	if ( clip > s_seen[id] )
		s_seen[id] = clip;
	return s_seen[id] > 0 ? s_seen[id] : 1;
}

int WeaponIconScale( const WEAPON* weapon )
{
	return ( weapon && gHUD.m_iRes > 640 && weapon->iRes == 640 && gHUD.m_iHudScale > 1 && IEngineStudio.IsHardware() )
		? gHUD.m_iHudScale : 1;
}

void HUD_DrawIconAdditive( HSPRITE sprite, int x, int y, const wrect_t* rect, int r, int g, int b, int scale )
{
	if ( !sprite || !rect )
		return;
	const int frameW = SPR_Width( sprite, 0 ), frameH = SPR_Height( sprite, 0 );
	struct model_s* model = (struct model_s*)gEngfuncs.GetSpritePointer( sprite );
	if ( scale <= 1 || !model || frameW <= 0 || frameH <= 0 || !gEngfuncs.pTriAPI || !IEngineStudio.IsHardware() )
	{
		SPR_Set( sprite, r, g, b );
		SPR_DrawAdditive( 0, x, y, rect );
		return;
	}
	// half a texel in from the edges on both the picture and the quad: nothing from the neighbours on the sheet
	// bleeds in, and one texel stays exactly scale x scale pixels
	const float u0 = ( rect->left + 0.5f ) / frameW, u1 = ( rect->right - 0.5f ) / frameW;
	const float v0 = ( rect->top + 0.5f ) / frameH, v1 = ( rect->bottom - 0.5f ) / frameH;
	const float in = 0.5f * scale;
	const float fx = x + in, fy = y + in;
	const float w = ( rect->right - rect->left ) * scale - 2.0f * in, h = ( rect->bottom - rect->top ) * scale - 2.0f * in;
	triangleapi_s* tri = gEngfuncs.pTriAPI;
	tri->SpriteTexture( model, 0 );
	tri->RenderMode( kRenderTransAdd );
	tri->CullFace( TRI_NONE );
	tri->Color4ub( (unsigned char)r, (unsigned char)g, (unsigned char)b, 255 );
	tri->Begin( TRI_QUADS );
	tri->TexCoord2f( u0, v0 );
	tri->Vertex3f( fx, fy, 0.0f );
	tri->TexCoord2f( u1, v0 );
	tri->Vertex3f( fx + w, fy, 0.0f );
	tri->TexCoord2f( u1, v1 );
	tri->Vertex3f( fx + w, fy + h, 0.0f );
	tri->TexCoord2f( u0, v1 );
	tri->Vertex3f( fx, fy + h, 0.0f );
	tri->End();
	tri->RenderMode( kRenderNormal );
}

WEAPON *gpActiveSel;	// NULL means off, 1 means just the menu bar, otherwise
						// this points to the active weapon menu item
WEAPON *gpLastSel;		// Last weapon menu selection

client_sprite_t *GetSpriteList(client_sprite_t *pList, const char *psz, int iRes, int iCount);

WeaponsResource gWR;

int g_weaponselect = 0;

int g_iPlayerFlags;
int g_iWeaponFlags;
float g_flPlayerSpeed;
int g_iShotsFired;
cvar_t *cl_dynamiccrosshair;
extern bool g_bHoldingShield;

void WeaponsResource :: LoadAllWeaponSprites( void )
{
	for (int i = 0; i < MAX_WEAPONS; i++)
	{
		if ( rgWeapons[i].iId )
			LoadWeaponSprites( &rgWeapons[i] );
	}
}

int WeaponsResource :: CountAmmo( int iId )
{
	if ( iId < 0 )
		return 0;

	return riAmmo[iId];
}

int WeaponsResource :: HasAmmo( WEAPON *p )
{
	if ( !p )
		return FALSE;

	// weapons with no max ammo can always be selected
	if ( p->iMax1 == -1 )
		return TRUE;

	return (p->iAmmoType == -1) || p->iClip > 0 || CountAmmo(p->iAmmoType)
		|| CountAmmo(p->iAmmo2Type) || ( p->iFlags & WEAPON_FLAGS_SELECTONEMPTY );
}


// Loads one weapon sprite kind from its list record.
static HSPRITE LoadWeaponSprite( const client_sprite_t *p, wrect_t *rc )
{
	char sz[128];
	snprintf( sz, sizeof( sz ), "sprites/%s.spr", p->szSprite );
	*rc = p->rc;
	return SPR_Load( sz );
}

// The record to use for 'kind': the hi-res overlay record when useTier and it is valid, else the stock record.
static client_sprite_t *PickWeaponSprite( client_sprite_t *pList, int iCount, client_sprite_t *pTierList, int iTierCount,
	const char *kind, int iRes, int iTier, bool useTier )
{
	client_sprite_t *p = GetSpriteList( pList, kind, iRes, iCount );
	if ( useTier && p )
	{
		client_sprite_t *t = GetSpriteList( pTierList, kind, iTier, iTierCount );
		char why[96];
		if ( t && gHUD.CheckTierRecord( t, p, gHUD.m_iHudScale, why, sizeof( why ) ) )
			return t;
	}
	return p;
}

void WeaponsResource :: LoadWeaponSprites( WEAPON *pWeapon )
{
	if ( !pWeapon )
		return;

	// clear all eight sprites first, so no early return leaves stale or uninitialised handles
	pWeapon->hCrosshair = pWeapon->hAutoaim = pWeapon->hZoomedCrosshair = pWeapon->hZoomedAutoaim = 0;
	pWeapon->hInactive = pWeapon->hActive = pWeapon->hAmmo = pWeapon->hAmmo2 = 0;
	memset( &pWeapon->rcCrosshair, 0, sizeof(wrect_t) );
	memset( &pWeapon->rcAutoaim, 0, sizeof(wrect_t) );
	memset( &pWeapon->rcZoomedCrosshair, 0, sizeof(wrect_t) );
	memset( &pWeapon->rcZoomedAutoaim, 0, sizeof(wrect_t) );
	memset( &pWeapon->rcActive, 0, sizeof(wrect_t) );
	memset( &pWeapon->rcInactive, 0, sizeof(wrect_t) );
	memset( &pWeapon->rcAmmo, 0, sizeof(wrect_t) );
	memset( &pWeapon->rcAmmo2, 0, sizeof(wrect_t) );

	const int iTier = gHUD.m_iRes;					// HUD tier chosen at VidInit
	const int iRes = iTier >= 640 ? 640 : 320;		// resolution of the stock list records
	pWeapon->iRes = iRes;

	// the Advanced window's weapon set may bring its own pictures for this weapon (vismods.cpp): its lists then
	const char *folder = VisMods_WeaponIconFolder( pWeapon->szName );
	char sz[128];
	int i = 0;
	snprintf( sz, sizeof( sz ), "sprites/%s%s.txt", folder, pWeapon->szName );
	client_sprite_t *pList = SPR_GetList(sz, &i);

	if (!pList)
		return;

	// hi-res tier: the overlay list sprites/cs<tier>/<weapon>.txt (custom server weapons have none and stay at 640)
	client_sprite_t *pTierList = NULL;
	int iTierCount = 0;
	if ( iTier > 640 )
	{
		snprintf( sz, sizeof( sz ), "sprites/%scs%d/%s.txt", folder, iTier, pWeapon->szName );
		pTierList = gHUD.GetTierSpriteList( sz, &iTierCount );
	}

	// the icons (menu, pickup history, ammo) switch together: all at the tier or all at 640
	static const char *s_IconKinds[] = { "weapon", "weapon_s", "ammo", "ammo2" };
	bool bIconsAtTier = pTierList != NULL;
	for ( int k = 0; bIconsAtTier && k < (int)( sizeof( s_IconKinds ) / sizeof( s_IconKinds[0] ) ); k++ )
	{
		client_sprite_t *p = GetSpriteList( pList, s_IconKinds[k], iRes, i );
		if ( p && PickWeaponSprite( pList, i, pTierList, iTierCount, s_IconKinds[k], iRes, iTier, true ) == p )
			bIconsAtTier = false;
	}
	if ( bIconsAtTier )
		pWeapon->iRes = iTier;

	// crosshairs resolve per kind; zoom reticles have no overlay record and stay at 640
	const bool bCrosshairsAtTier = pTierList != NULL;
	client_sprite_t *p;

	p = PickWeaponSprite( pList, i, pTierList, iTierCount, "crosshair", iRes, iTier, bCrosshairsAtTier );
	if (p)
		pWeapon->hCrosshair = LoadWeaponSprite( p, &pWeapon->rcCrosshair );

	p = PickWeaponSprite( pList, i, pTierList, iTierCount, "autoaim", iRes, iTier, bCrosshairsAtTier );
	if (p)
		pWeapon->hAutoaim = LoadWeaponSprite( p, &pWeapon->rcAutoaim );

	p = PickWeaponSprite( pList, i, pTierList, iTierCount, "zoom", iRes, iTier, bCrosshairsAtTier );
	if (p)
		pWeapon->hZoomedCrosshair = LoadWeaponSprite( p, &pWeapon->rcZoomedCrosshair );
	else
	{
		pWeapon->hZoomedCrosshair = pWeapon->hCrosshair; //default to non-zoomed crosshair
		pWeapon->rcZoomedCrosshair = pWeapon->rcCrosshair;
	}

	p = PickWeaponSprite( pList, i, pTierList, iTierCount, "zoom_autoaim", iRes, iTier, bCrosshairsAtTier );
	if (p)
		pWeapon->hZoomedAutoaim = LoadWeaponSprite( p, &pWeapon->rcZoomedAutoaim );
	else
	{
		pWeapon->hZoomedAutoaim = pWeapon->hZoomedCrosshair;  //default to zoomed crosshair
		pWeapon->rcZoomedAutoaim = pWeapon->rcZoomedCrosshair;
	}

	p = PickWeaponSprite( pList, i, pTierList, iTierCount, "weapon", iRes, iTier, bIconsAtTier );
	if (p)
	{
		pWeapon->hInactive = LoadWeaponSprite( p, &pWeapon->rcInactive );
		gHR.iHistoryGap = max( gHR.iHistoryGap, ( pWeapon->rcActive.bottom - pWeapon->rcActive.top ) * WeaponIconScale( pWeapon ) );
	}

	p = PickWeaponSprite( pList, i, pTierList, iTierCount, "weapon_s", iRes, iTier, bIconsAtTier );
	if (p)
		pWeapon->hActive = LoadWeaponSprite( p, &pWeapon->rcActive );

	p = PickWeaponSprite( pList, i, pTierList, iTierCount, "ammo", iRes, iTier, bIconsAtTier );
	if (p)
	{
		pWeapon->hAmmo = LoadWeaponSprite( p, &pWeapon->rcAmmo );
		gHR.iHistoryGap = max( gHR.iHistoryGap, ( pWeapon->rcActive.bottom - pWeapon->rcActive.top ) * WeaponIconScale( pWeapon ) );
	}

	p = PickWeaponSprite( pList, i, pTierList, iTierCount, "ammo2", iRes, iTier, bIconsAtTier );
	if (p)
	{
		pWeapon->hAmmo2 = LoadWeaponSprite( p, &pWeapon->rcAmmo2 );
		gHR.iHistoryGap = max( gHR.iHistoryGap, ( pWeapon->rcActive.bottom - pWeapon->rcActive.top ) * WeaponIconScale( pWeapon ) );
	}
}

// the weapon set changed (vismods.cpp): every weapon's pictures read again from its lists
void Ammo_ReloadWeaponIcons()
{
	gWR.LoadAllWeaponSprites();
}

void WeaponsResource :: CountIconTiers( int iTier, int *piKnown, int *piAtTier )
{
	*piKnown = *piAtTier = 0;
	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		if ( !rgWeapons[i].iId )
			continue;
		(*piKnown)++;
		if ( rgWeapons[i].iRes == iTier )
			(*piAtTier)++;
	}
}

void CountWeaponIconTiers( int tier, int *known, int *atTier )
{
	gWR.CountIconTiers( tier, known, atTier );
}

// Returns the first weapon for a given slot.
WEAPON *WeaponsResource :: GetFirstPos( int iSlot )
{
	WEAPON *pret = NULL;

	for (int i = 0; i < MAX_WEAPON_POSITIONS; i++)
	{
		if ( rgSlots[iSlot][i] )
		{
			pret = rgSlots[iSlot][i];
			break;
		}
	}

	return pret;
}


WEAPON* WeaponsResource :: GetNextActivePos( int iSlot, int iSlotPos )
{
	if ( iSlotPos >= MAX_WEAPON_POSITIONS || iSlot >= MAX_WEAPON_SLOTS )
		return NULL;

	WEAPON *p = gWR.rgSlots[ iSlot ][ iSlotPos+1 ];

	if ( !p )
		return GetNextActivePos( iSlot, iSlotPos + 1 );

	return p;
}


int giBucketHeight, giBucketWidth, giABHeight, giABWidth; // Ammo Bar width and height

HSPRITE ghsprBuckets;					// Sprite for top row of weapons menu

DECLARE_MESSAGE(m_Ammo, CurWeapon );	// Current weapon and clip
DECLARE_MESSAGE(m_Ammo, WeaponList);	// new weapon type
DECLARE_MESSAGE(m_Ammo, AmmoX);			// update known ammo type's count
DECLARE_MESSAGE(m_Ammo, AmmoPickup);	// flashes an ammo pickup record
DECLARE_MESSAGE(m_Ammo, WeapPickup);    // flashes a weapon pickup record
DECLARE_MESSAGE(m_Ammo, HideWeapon);	// hides the weapon, ammo, and crosshair displays temporarily
DECLARE_MESSAGE(m_Ammo, ItemPickup);
DECLARE_MESSAGE(m_Ammo, Crosshair);
DECLARE_MESSAGE(m_accountBalance, Money);
DECLARE_MESSAGE(m_accountBalance, BlinkAcct);

DECLARE_COMMAND(m_Ammo, Slot1);
DECLARE_COMMAND(m_Ammo, Slot2);
DECLARE_COMMAND(m_Ammo, Slot3);
DECLARE_COMMAND(m_Ammo, Slot4);
DECLARE_COMMAND(m_Ammo, Slot5);
DECLARE_COMMAND(m_Ammo, Slot6);
DECLARE_COMMAND(m_Ammo, Slot7);
DECLARE_COMMAND(m_Ammo, Slot8);
DECLARE_COMMAND(m_Ammo, Slot9);
DECLARE_COMMAND(m_Ammo, Slot10);
DECLARE_COMMAND(m_Ammo, Close);
DECLARE_COMMAND(m_Ammo, NextWeapon);
DECLARE_COMMAND(m_Ammo, PrevWeapon);
DECLARE_COMMAND(m_Ammo, Adjust_Crosshair);

// width of ammo fonts
#define AMMO_SMALL_WIDTH 10
#define AMMO_LARGE_WIDTH 20

#define HISTORY_DRAW_TIME	"5"

// a sniper rifle: the game's four, or a server's own weapon (AMXX plugins add some, such as weapon_scope_awp) named
// after one of them
static bool IsSniperRifle( const WEAPON* weapon )
{
	if ( !weapon )
		return false;
	const int id = weapon->iId;
	if ( id == WEAPON_SCOUT || id == WEAPON_SG550 || id == WEAPON_AWP || id == WEAPON_G3SG1 )
		return true;
	char name[MAX_WEAPON_NAME];
	int i = 0;
	for ( ; i < MAX_WEAPON_NAME - 1 && weapon->szName[i]; i++ )
		name[i] = (char)tolower( (unsigned char)weapon->szName[i] );
	name[i] = 0;
	return strstr( name, "awp" ) || strstr( name, "scout" ) || strstr( name, "sg550" ) || strstr( name, "g3sg1" )
		|| strstr( name, "sniper" );
}

// is the crosshair hidden by the server (HideWeapon's crosshair flag)? Sniper rifle crosshair (cl_crosshair_sniper)
// wins over it on a sniper rifle - a server's own sniper rifle gets no crosshair that way, as the game's get none
// through the client - but never while a shield is held (the game hides the crosshair behind a drawn shield with it)
static bool ServerHidesCrosshair( const WEAPON* weapon )
{
	if ( !( gHUD.m_iHideHUDDisplay & HIDEHUD_CROSSHAIR ) )
		return false;
	return g_bHoldingShield || !IsSniperRifle( weapon ) || gEngfuncs.pfnGetCvarFloat( (char*)"cl_crosshair_sniper" ) == 0.0f;
}

int CHudAmmo::Init(void)
{
	gHUD.AddHudElem(this);

	HOOK_MESSAGE(CurWeapon);
	HOOK_MESSAGE(WeaponList);
	HOOK_MESSAGE(AmmoPickup);
	HOOK_MESSAGE(WeapPickup);
	HOOK_MESSAGE(ItemPickup);
	HOOK_MESSAGE(HideWeapon);
	HOOK_MESSAGE(AmmoX);
	HOOK_MESSAGE(Crosshair);

	HOOK_COMMAND("slot1", Slot1);
	HOOK_COMMAND("slot2", Slot2);
	HOOK_COMMAND("slot3", Slot3);
	HOOK_COMMAND("slot4", Slot4);
	HOOK_COMMAND("slot5", Slot5);
	HOOK_COMMAND("slot6", Slot6);
	HOOK_COMMAND("slot7", Slot7);
	HOOK_COMMAND("slot8", Slot8);
	HOOK_COMMAND("slot9", Slot9);
	HOOK_COMMAND("slot10", Slot10);
	HOOK_COMMAND("cancelselect", Close);
	HOOK_COMMAND("invnext", NextWeapon);
	HOOK_COMMAND("invprev", PrevWeapon);
	HOOK_COMMAND("adjust_crosshair", Adjust_Crosshair);
	gHUD.m_iHideHUDDisplay = 0;

	Reset();

	CVAR_CREATE( "hud_drawhistory_time", HISTORY_DRAW_TIME, 0 );
	CVAR_CREATE( "hud_fastswitch", "0", FCVAR_ARCHIVE );		// controls whether or not weapons can be selected in one keypress
	CVAR_CREATE("cl_observercrosshair", "1", FCVAR_ARCHIVE);	// kept between games (console only)
	CVAR_CREATE("cl_crosshair_color", "50 250 50", FCVAR_ARCHIVE);
	CVAR_CREATE("cl_crosshair_translucent", "1", FCVAR_ARCHIVE);
	// the crosshair's colour and translucency as the HUD and the Advanced window use them: names of their own, because
	// the game's Options window (whose crosshair controls deploy.ps1 hides) still writes cl_crosshair_color and
	// cl_crosshair_translucent on every OK / Apply. "" until they take over the old values (Crosshair_TakeOverCvars).
	CVAR_CREATE("cl_crosshair_rgb", "", FCVAR_ARCHIVE);
	CVAR_CREATE("cl_crosshair_additive", "", FCVAR_ARCHIVE);
	CVAR_CREATE("cl_crosshair_size", "0", FCVAR_ARCHIVE);
	// the Advanced window's crosshair options (crosshair_draw.cpp); these defaults draw the game's own crosshair
	CVAR_CREATE("cl_crosshair_thickness", "1", FCVAR_ARCHIVE);
	CVAR_CREATE("cl_crosshair_length", "0", FCVAR_ARCHIVE);
	CVAR_CREATE("cl_crosshair_gap", "0", FCVAR_ARCHIVE);
	CVAR_CREATE("cl_crosshair_alpha", "0", FCVAR_ARCHIVE);
	CVAR_CREATE("cl_crosshair_outline", "0", FCVAR_ARCHIVE);
	CVAR_CREATE("cl_crosshair_dot", "0", FCVAR_ARCHIVE);
	CVAR_CREATE("cl_crosshair_t", "0", FCVAR_ARCHIVE);
	CVAR_CREATE("cl_crosshair_recoil", "1", FCVAR_ARCHIVE);
	CVAR_CREATE("cl_crosshair_sniper", "0", FCVAR_ARCHIVE);	// crosshair for sniper rifles while not zoomed

	m_iFlags |= HUD_ACTIVE | HUD_INTERMISSION;
	m_bObserverCrosshair = 0;
	m_flCrosshairDistance = 0;
	m_iAmmoLastCheck = 0;
	m_iAlpha = 200;
	m_R = 50;
	m_G = 250;
	m_B = 50;
	m_cvarR = m_cvarG = m_cvarB = -1;
	m_iCurrentCrosshair = 0;
	m_bAdditive = true;
	m_iCrosshairScaleBase = 1024;

	gWR.Init();
	gHR.Init();

	return 1;
};

void CHudAmmo::Reset(void)
{
	m_fFade = 0;
	m_iFlags |= HUD_ACTIVE; //!!!

	gpActiveSel = NULL;
	gHUD.m_iHideHUDDisplay &= HIDEHUD_CROSSHAIR;

	gWR.Reset();
	gHR.Reset();
}

int CHudAmmo::VidInit(void)
{
	// Load sprites for buckets (top row of weapon menu)
	m_HUD_bucket0 = gHUD.GetSpriteIndex( "bucket1" );
	m_HUD_selection = gHUD.GetSpriteIndex( "selection" );

	ghsprBuckets = gHUD.GetSprite(m_HUD_bucket0);
	giBucketWidth = gHUD.GetSpriteRect(m_HUD_bucket0).right - gHUD.GetSpriteRect(m_HUD_bucket0).left;
	giBucketHeight = gHUD.GetSpriteRect(m_HUD_bucket0).bottom - gHUD.GetSpriteRect(m_HUD_bucket0).top;

	gHR.iHistoryGap = 0;	// recomputed for the current tier below
	gHR.iHistoryGap = max( gHR.iHistoryGap, gHUD.GetSpriteRect(m_HUD_bucket0).bottom - gHUD.GetSpriteRect(m_HUD_bucket0).top);

	// If we've already loaded weapons, let's get new sprites
	gWR.LoadAllWeaponSprites();

	int nScale = gHUD.m_iRes >= 640 ? 2 * gHUD.m_iHudScale : 1;
	giABWidth = 10 * nScale;
	giABHeight = 2 * nScale;

	m_hObserverCrosshair = 0;
	memset( &m_rcObserverCrosshair, 0, sizeof( m_rcObserverCrosshair ) );
	int count = 0;
	client_sprite_t *sprites = SPR_GetList("sprites/observer.txt", &count);
	client_sprite_t *crosshair = GetSpriteList(sprites, "crosshair", gHUD.m_iRes >= 640 ? 640 : 320, count);
	if ( crosshair && gHUD.m_iRes > 640 )
	{
		// hi-res observer crosshair from sprites/cs<tier>/observer.txt, if valid
		char path[64];
		snprintf( path, sizeof( path ), "sprites/cs%d/observer.txt", gHUD.m_iRes );
		int tierCount = 0;
		client_sprite_t *tierList = gHUD.GetTierSpriteList( path, &tierCount );
		client_sprite_t *tierCrosshair = GetSpriteList( tierList, "crosshair", gHUD.m_iRes, tierCount );
		char why[96];
		if ( tierCrosshair && gHUD.CheckTierRecord( tierCrosshair, crosshair, gHUD.m_iHudScale, why, sizeof( why ) ) )
			crosshair = tierCrosshair;
	}
	if (crosshair)
	{
		char name[128];
		snprintf( name, sizeof( name ), "sprites/%s.spr", crosshair->szSprite );
		m_hObserverCrosshair = SPR_Load(name);
		m_rcObserverCrosshair = crosshair->rc;
	}

	return 1;
}

//
// Think:
//  Used for selection of weapon menu item.
//
// The weapon menu closes on its own: shown for 7 seconds after the last key that picked in it, it then fades out
// quickly (0.4 s) and is gone, as if closed with nothing chosen
static const float kMenuShowTime = 7.0f;
static const float kMenuFadeTime = 0.4f;
static WEAPON *s_pMenuSel;			// the pick the timer last saw
static float s_flMenuSince;			// when the menu was last used (gHUD.m_flTime)
static float s_flMenuFade = 1.0f;	// the menu's brightness while drawn: 1, then down to 0 as it fades

// the menu was used (a slot key, the next / previous weapon): shown in full again
static void MenuUsed()
{
	s_pMenuSel = gpActiveSel;
	s_flMenuSince = gHUD.m_flTime;
}

// how bright the weapon menu is now: 1 until kMenuShowTime after it was last used (or its pick changed), then down to
// 0 over kMenuFadeTime
static float MenuFade()
{
	if ( gpActiveSel != s_pMenuSel )
		MenuUsed();
	const float shown = gHUD.m_flTime - s_flMenuSince;
	if ( shown < 0.0f )
	{
		MenuUsed();		// the clock started again (a new map)
		return 1.0f;
	}
	if ( shown <= kMenuShowTime )
		return 1.0f;
	const float fade = 1.0f - ( shown - kMenuShowTime ) / kMenuFadeTime;
	return fade > 0.0f ? fade : 0.0f;
}

void CHudAmmo::Think(void)
{
	if ( gHUD.m_fPlayerDead )
		return;

	if ( gHUD.m_iWeaponBits != gWR.iOldWeaponBits )
	{
		gWR.iOldWeaponBits = gHUD.m_iWeaponBits;

		for (int i = MAX_WEAPONS-1; i > 0; i-- )
		{
			WEAPON *p = gWR.GetWeapon(i);

			if ( p )
			{
				if ( gHUD.m_iWeaponBits & ( 1 << p->iId ) )
					gWR.PickupWeapon( p );
				else
					gWR.DropWeapon( p );
			}
		}
	}

	if (!gpActiveSel)
	{
		s_pMenuSel = NULL;	// closed: its next opening starts the timer afresh
		return;
	}

	// faded out: closed, nothing chosen
	if ( MenuFade() <= 0.0f )
	{
		gpLastSel = gpActiveSel;
		gpActiveSel = NULL;
		return;
	}

	// has the player selected one?
	if (gHUD.m_iKeyBits & IN_ATTACK)
	{
		if (gpActiveSel != (WEAPON *)1)
		{
			ServerCmd(gpActiveSel->szName);
			g_weaponselect = gpActiveSel->iId;
		}

		gpLastSel = gpActiveSel;
		gpActiveSel = NULL;
		gHUD.m_iKeyBits &= ~IN_ATTACK;

		PlaySound("common/wpn_select.wav", 1);
	}

}

//
// Helper function to return a Ammo pointer from id
//

HSPRITE* WeaponsResource :: GetAmmoPicFromWeapon( int iAmmoId, wrect_t& rect, int* scale )
{
	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		if ( rgWeapons[i].iAmmoType == iAmmoId )
		{
			rect = rgWeapons[i].rcAmmo;
			if ( scale )
				*scale = WeaponIconScale( &rgWeapons[i] );
			return &rgWeapons[i].hAmmo;
		}
		else if ( rgWeapons[i].iAmmo2Type == iAmmoId )
		{
			rect = rgWeapons[i].rcAmmo2;
			if ( scale )
				*scale = WeaponIconScale( &rgWeapons[i] );
			return &rgWeapons[i].hAmmo2;
		}
	}

	return NULL;
}


// Menu Selection Code

void WeaponsResource :: SelectSlot( int iSlot, int fAdvance, int iDirection )
{
	if ( gHUD.m_Menu.m_fMenuDisplayed && (fAdvance == FALSE) && (iDirection == 1) )
	{ // menu is overriding slot use commands
		gHUD.m_Menu.SelectMenuItem( iSlot + 1 );  // slots are one off the key numbers
		return;
	}

	if ( iSlot > MAX_WEAPON_SLOTS )
		return;

	if ( gHUD.m_fPlayerDead || gHUD.m_iHideHUDDisplay & ( HIDEHUD_WEAPONS | HIDEHUD_ALL ) )
		return;

	if (!(gHUD.m_iWeaponBits & (1<<(WEAPON_SUIT)) ))
		return;

	if ( ! ( gHUD.m_iWeaponBits & ~(1<<(WEAPON_SUIT)) ))
		return;

	WEAPON *p = NULL;
	bool fastSwitch = CVAR_GET_FLOAT( "hud_fastswitch" ) != 0;

	if ( (gpActiveSel == NULL) || (gpActiveSel == (WEAPON *)1) || (iSlot != gpActiveSel->iSlot) )
	{
		PlaySound( "common/wpn_hudon.wav", 1 );
		p = GetFirstPos( iSlot );

		if ( p && fastSwitch ) // check for fast weapon switch mode
		{
			// if fast weapon switch is on, then weapons can be selected in a single keypress
			// but only if there is only one item in the bucket
			WEAPON *p2 = GetNextActivePos( p->iSlot, p->iSlotPos );
			if ( !p2 )
			{	// only one active item in bucket, so change directly to weapon
				ServerCmd( p->szName );
				return;
			}
		}
	}
	else
	{
		PlaySound("common/wpn_moveselect.wav", 1);
		if ( gpActiveSel )
			p = GetNextActivePos( gpActiveSel->iSlot, gpActiveSel->iSlotPos );
		if ( !p )
			p = GetFirstPos( iSlot );
	}


	if ( !p )  // no selection found
	{
		// just display the weapon list, unless fastswitch is on just ignore it
		if ( !fastSwitch )
			gpActiveSel = (WEAPON *)1;
		else
			gpActiveSel = NULL;
	}
	else
		gpActiveSel = p;
	MenuUsed();
}

//------------------------------------------------------------------------
// Message Handlers
//------------------------------------------------------------------------

//
// AmmoX  -- Update the count of a known type of ammo
//
int CHudAmmo::MsgFunc_AmmoX(const char *, int iSize, void *pbuf)
{
	BEGIN_READ( pbuf, iSize );

	int iIndex = READ_BYTE();
	int iCount = READ_BYTE();

	gWR.SetAmmo( iIndex, abs(iCount) );
	gCSViewPortInterface->UpdateBuyPresets();

	return 1;
}

int CHudAmmo::MsgFunc_AmmoPickup( const char *, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	int iIndex = READ_BYTE();
	int iCount = READ_BYTE();

	// Add ammo to the history
	gHR.AddToHistory( HISTSLOT_AMMO, iIndex, abs(iCount) );
	gCSViewPortInterface->UpdateBuyPresets();

	return 1;
}

int CHudAmmo::MsgFunc_WeapPickup( const char *, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	int iIndex = READ_BYTE();

	// Add the weapon to the history
	gHR.AddToHistory( HISTSLOT_WEAP, iIndex );
	gCSViewPortInterface->UpdateBuyPresets();

	return 1;
}

int CHudAmmo::MsgFunc_ItemPickup( const char *, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	const char *szName = READ_STRING();

	// Add the weapon to the history
	gHR.AddToHistory( HISTSLOT_ITEM, szName );
	gCSViewPortInterface->UpdateBuyPresets();

	return 1;
}


int CHudAmmo::MsgFunc_HideWeapon( const char *, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	gHUD.m_iHideHUDDisplay = READ_BYTE();

	if (gEngfuncs.IsSpectateOnly())
		return 1;

	if ((gHUD.m_iHideHUDDisplay & HIDEHUD_CROSSHAIR) || !gHUD.m_iHideHUDDisplay)
	{
		static wrect_t nullrc;
		SetCrosshair( 0, nullrc, 0, 0, 0 );
	}
	else
	{
		if (gHUD.m_iHideHUDDisplay & (HIDEHUD_WEAPONS | HIDEHUD_ALL))
		{
			if (!m_bObserverCrosshair)
			{
				static wrect_t nullrc;
				gpActiveSel = NULL;
				SetCrosshair(0, nullrc, 0, 0, 0);
			}
		}
		else
		{
			// as in CurWeapon: CS draws its own crosshair, the engine one (the weapon list's orange Half-Life
			// crosshair) is only for zoomed weapons without the sniper scope. Servers that hide the money or timer
			// send HideWeapon, which used to bring the orange crosshair up until the next shot.
			if (m_pWeapon && !g_bHoldingShield)
			{
				static wrect_t nullrc;
				if (gHUD.m_iFOV >= 90 || gHUD.m_iFOV <= 44)
					SetCrosshair(0, nullrc, 0, 0, 0);
				else
					SetCrosshair(m_pWeapon->hZoomedCrosshair, m_pWeapon->rcZoomedCrosshair, 255, 255, 255);
			}
			m_bObserverCrosshair = 0;
		}
	}

	return 1;
}

//
//  CurWeapon: Update hud state with the current weapon and clip count. Ammo
//  counts are updated with AmmoX. Server assures that the Weapon ammo type
//  numbers match a real ammo type.
//
int CHudAmmo::MsgFunc_CurWeapon(const char *, int iSize, void *pbuf )
{
	static wrect_t nullrc;
	int fOnTarget = FALSE;

	BEGIN_READ( pbuf, iSize );

	int iState = READ_BYTE();
	int iId = READ_CHAR();
	int iClip = READ_CHAR();

	// detect if we're also on target
	if ( iState > 1 )
	{
		fOnTarget = TRUE;
	}

	if ( iId < 1 )
	{
		if ( !m_bObserverCrosshair )
		{
			SetCrosshair(0, nullrc, 0, 0, 0);
			return 0;
		}
		// free look keeps its observer crosshair (the server sends CurWeapon 0 right after switching to it)
		if ( g_iUser1 != OBS_IN_EYE && iId == -1 && iClip == -1 )
		{
			gHUD.m_fPlayerDead = TRUE;
			gpActiveSel = NULL;
		}
		return 1;
	}

	if ( g_iUser1 != OBS_IN_EYE )
	{
		// Is player dead???
		if ((iId == -1) && (iClip == -1))
		{
			gHUD.m_fPlayerDead = TRUE;
			gpActiveSel = NULL;
			return 1;
		}
		gHUD.m_fPlayerDead = FALSE;
	}

	WEAPON *pWeapon = gWR.GetWeapon( iId );

	if ( !pWeapon )
		return 0;

	if ( iClip < -1 )
		pWeapon->iClip = abs(iClip);
	else
		pWeapon->iClip = iClip;


	if ( iState == 0 )	// we're not the current weapon, so update no more
		return 1;

	m_pWeapon = pWeapon;

	if ( gHUD.m_iFOV >= 90 )
	{ // normal crosshairs
		SetCrosshair(0, nullrc, 0, 0, 0);
	}
	else
	{ // zoomed crosshairs
		if ( gHUD.m_iFOV <= 44 )
			SetCrosshair(0, nullrc, 0, 0, 0);	// sniper scope: CHudSniperScope draws the reticle itself
		else if (fOnTarget && m_pWeapon->hZoomedAutoaim)
			SetCrosshair(m_pWeapon->hZoomedAutoaim, m_pWeapon->rcZoomedAutoaim, 255, 255, 255);
		else
			SetCrosshair(m_pWeapon->hZoomedCrosshair, m_pWeapon->rcZoomedCrosshair, 255, 255, 255);

	}

	m_fFade = 200.0f; //!!!
	m_iFlags |= HUD_ACTIVE;
	m_bObserverCrosshair = 0;
	gCSViewPortInterface->UpdateBuyPresets();

	return 1;
}

//
// WeaponList -- Tells the hud about a new weapon type.
//
int CHudAmmo::MsgFunc_WeaponList(const char *, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	WEAPON Weapon;

	strncpy( Weapon.szName, READ_STRING(), MAX_WEAPON_NAME );
	Weapon.szName[ sizeof(Weapon.szName) - 1 ] = '\0';

	Weapon.iAmmoType = (int)READ_CHAR();

	Weapon.iMax1 = READ_BYTE();
	if (Weapon.iMax1 == 255)
		Weapon.iMax1 = -1;

	Weapon.iAmmo2Type = READ_CHAR();
	Weapon.iMax2 = READ_BYTE();
	if (Weapon.iMax2 == 255)
		Weapon.iMax2 = -1;

	Weapon.iSlot = READ_CHAR();
	Weapon.iSlotPos = READ_CHAR();
	Weapon.iId = READ_CHAR();
	Weapon.iFlags = READ_BYTE();
	Weapon.iClip = 0;

	if (Weapon.iId < 0 || Weapon.iId >= MAX_WEAPONS)
		return 0;

	if (Weapon.iSlot < 0 || Weapon.iSlot >= MAX_WEAPON_SLOTS+1)
		return 0;

	if (Weapon.iSlotPos < 0 || Weapon.iSlotPos >= MAX_WEAPON_POSITIONS+1)
		return 0;

	if (Weapon.iAmmoType < -1 || Weapon.iAmmoType >= MAX_AMMO_TYPES)
		return 0;

	if (Weapon.iAmmo2Type < -1 || Weapon.iAmmo2Type >= MAX_AMMO_TYPES)
		return 0;

	if (Weapon.iAmmoType >= 0 && Weapon.iMax1 == 0)
		return 0;

	if (Weapon.iAmmo2Type >= 0 && Weapon.iMax2 == 0)
		return 0;

	gWR.AddWeapon( &Weapon );

	return 1;

}

//------------------------------------------------------------------------
// Command Handlers
//------------------------------------------------------------------------
// Slot button pressed
void CHudAmmo::SlotInput( int iSlot )
{
	if ( gViewPortInterface && gViewPortInterface->SlotInput( iSlot ) )
		return;

	gWR.SelectSlot(iSlot, FALSE, 1);
}

void CHudAmmo::UserCmd_Slot1(void)
{
	SlotInput( 0 );
}

void CHudAmmo::UserCmd_Slot2(void)
{
	SlotInput( 1 );
}

void CHudAmmo::UserCmd_Slot3(void)
{
	SlotInput( 2 );
}

void CHudAmmo::UserCmd_Slot4(void)
{
	SlotInput( 3 );
}

void CHudAmmo::UserCmd_Slot5(void)
{
	SlotInput( 4 );
}

void CHudAmmo::UserCmd_Slot6(void)
{
	SlotInput( 5 );
}

void CHudAmmo::UserCmd_Slot7(void)
{
	SlotInput( 6 );
}

void CHudAmmo::UserCmd_Slot8(void)
{
	SlotInput( 7 );
}

void CHudAmmo::UserCmd_Slot9(void)
{
	SlotInput( 8 );
}

void CHudAmmo::UserCmd_Slot10(void)
{
	SlotInput( 9 );
}

void CHudAmmo::UserCmd_Close(void)
{
	if (gpActiveSel)
	{
		gpLastSel = gpActiveSel;
		gpActiveSel = NULL;
		PlaySound("common/wpn_hudoff.wav", 1);
	}
	else
		EngineClientCmd("escape");
}


// Selects the next item in the weapon menu
void CHudAmmo::UserCmd_NextWeapon(void)
{
	if ( gHUD.m_fPlayerDead || (gHUD.m_iHideHUDDisplay & (HIDEHUD_WEAPONS | HIDEHUD_ALL)) )
		return;

	if ( !gpActiveSel || gpActiveSel == (WEAPON*)1 )
		gpActiveSel = m_pWeapon;

	int pos = 0;
	int slot = 0;
	if ( gpActiveSel )
	{
		pos = gpActiveSel->iSlotPos + 1;
		slot = gpActiveSel->iSlot;
	}

	for ( int loop = 0; loop <= 1; loop++ )
	{
		for ( ; slot < MAX_WEAPON_SLOTS; slot++ )
		{
			for ( ; pos < MAX_WEAPON_POSITIONS; pos++ )
			{
				WEAPON *wsp = gWR.GetWeaponSlot( slot, pos );

				if ( wsp )
				{
					if (CVAR_GET_FLOAT("hud_fastswitch") == 1.0f)
					{
						PlaySound("common/wpn_hudon.wav", 1);
						ServerCmd(wsp->szName);
						gpActiveSel = NULL;
					}
					else
					{
						gpActiveSel = wsp;
					}
					return;
				}
			}

			pos = 0;
		}

		slot = 0;  // start looking from the first slot again
	}

	gpActiveSel = NULL;
}

// Selects the previous item in the menu
void CHudAmmo::UserCmd_PrevWeapon(void)
{
	if ( gHUD.m_fPlayerDead || (gHUD.m_iHideHUDDisplay & (HIDEHUD_WEAPONS | HIDEHUD_ALL)) )
		return;

	if ( !gpActiveSel || gpActiveSel == (WEAPON*)1 )
		gpActiveSel = m_pWeapon;

	int pos = MAX_WEAPON_POSITIONS-1;
	int slot = MAX_WEAPON_SLOTS-1;
	if ( gpActiveSel )
	{
		pos = gpActiveSel->iSlotPos - 1;
		slot = gpActiveSel->iSlot;
	}

	for ( int loop = 0; loop <= 1; loop++ )
	{
		for ( ; slot >= 0; slot-- )
		{
			for ( ; pos >= 0; pos-- )
			{
				WEAPON *wsp = gWR.GetWeaponSlot( slot, pos );

				if ( wsp )
				{
					if (CVAR_GET_FLOAT("hud_fastswitch") == 1.0f)
					{
						PlaySound("common/wpn_hudon.wav", 1);
						ServerCmd(wsp->szName);
						gpActiveSel = NULL;
					}
					else
					{
						gpActiveSel = wsp;
					}
					return;
				}
			}

			pos = MAX_WEAPON_POSITIONS-1;
		}

		slot = MAX_WEAPON_SLOTS-1;
	}

	gpActiveSel = NULL;
}



//-------------------------------------------------------------------------
// Drawing code
//-------------------------------------------------------------------------

static cvar_t *CrosshairCvar(cvar_t *&cache, const char *name);

int CHudAmmo::Draw(float flTime)
{
	int a, x, y, r, g, b;
	int AmmoWidth;
	static wrect_t nullrc;

	if (gEngfuncs.IsSpectateOnly())
	{
		if (g_iUser1 != OBS_IN_EYE)
		{
			SetCrosshair(0, nullrc, 0, 0, 0);
			return 1;
		}
		WEAPON *sniper = gWR.GetWeapon(WEAPON_AWP);
		if (gHUD.m_iFOV > 44 || !sniper->hZoomedCrosshair)
		{
			SetCrosshair(0, nullrc, 0, 0, 0);
			DrawCrosshair(flTime, WEAPON_NONE);
		}
		else
		{
			SetCrosshair(0, nullrc, 0, 0, 0);	// sniper scope: CHudSniperScope draws the reticle itself
		}
		return 1;
	}
	if (gHUD.m_iIntermission)
	{
		SetCrosshair(0, nullrc, 0, 0, 0);
		return 1;
	}

	if (g_iUser1 != OBS_IN_EYE)
	{
		if (!(gHUD.m_iWeaponBits & (1U << WEAPON_SUIT))
			|| (gHUD.m_iHideHUDDisplay & (HIDEHUD_WEAPONS | HIDEHUD_ALL)))
			return 1;

		// Draw Weapon Menu
		DrawWList(flTime);

		// Draw ammo pickup history
		gHR.DrawAmmoHistory(flTime);
		if (!(m_iFlags & HUD_ACTIVE))
			return 0;
	}

	if (!m_pWeapon)
		return 0;

	WEAPON *pw = m_pWeapon; // shorthand
	// sniper rifles show no crosshair unless zoomed in, as in the game; cl_crosshair_sniper 1 (the Advanced window's
	// "Sniper rifle crosshair") gives them the normal crosshair while not zoomed - servers' own ones too, whose
	// crosshair the server hides (ServerHidesCrosshair)
	static cvar_t *sniperCvar;
	const bool sniperRifle = IsSniperRifle(pw);
	if (gHUD.m_iFOV > 54 && !ServerHidesCrosshair(pw)
		&& (!sniperRifle || (CrosshairCvar(sniperCvar, "cl_crosshair_sniper") && sniperCvar->value != 0)))
		DrawCrosshair(flTime, pw->iId);
	if (g_iUser1 == OBS_IN_EYE)
		return 1;

	// SPR_Draw Ammo
	if ((pw->iAmmoType < 0) && (pw->iAmmo2Type < 0))
		return 0;

	int iFlags = DHN_DRAWZERO; // draw 0 values

	AmmoWidth = gHUD.GetSpriteRect(gHUD.m_HUD_number_0).right - gHUD.GetSpriteRect(gHUD.m_HUD_number_0).left;

	a = max<int>( MIN_ALPHA, m_fFade );

	if (m_fFade > 0)
		m_fFade -= (gHUD.m_flTimeDelta * 20);

	// hud_color "dynamic": the magazine by how full it is, the rest (bar, reserve, icon) by the reserve against the
	// most that can be carried; green full, yellow half, red a quarter and below
	const bool dynamic = HudColorDynamic();
	const unsigned long reserveRGB = dynamic && pw->iMax1 > 0
		? HudDynamicRGB(100.0f * gWR.CountAmmo(pw->iAmmoType) / pw->iMax1, 25.0f, 50.0f, 100.0f) : RGB_YELLOWISH;
	const unsigned long clipRGB = dynamic && pw->iClip >= 0
		? HudDynamicRGB(100.0f * pw->iClip / WeaponClipSize(pw->iId, pw->iClip), 25.0f, 50.0f, 100.0f) : RGB_YELLOWISH;
	UnpackRGB(r,g,b, pw->iClip >= 0 ? clipRGB : reserveRGB);

	ScaleColors(r, g, b, a );

	y = ScreenHeight - gHUD.m_iFontHeight - gHUD.m_iFontHeight/2;

	// Does weapon have any ammo at all?
	if (m_pWeapon->iAmmoType > 0)
	{
		const int iIconScale = WeaponIconScale( m_pWeapon );
		int iIconWidth = ( m_pWeapon->rcAmmo.right - m_pWeapon->rcAmmo.left ) * iIconScale;

		if (pw->iClip >= 0)
		{
			// room for the number and the '|' and the current ammo

			x = ScreenWidth - (8 * AmmoWidth) - iIconWidth;
			x = gHUD.DrawHudNumber(x, y, iFlags | DHN_3DIGITS, pw->iClip, r, g, b);

			wrect_t rc;
			rc.top = 0;
			rc.left = 0;
			rc.right = AmmoWidth;
			rc.bottom = 100;

			int iBarWidth =  AmmoWidth/10;

			x += AmmoWidth/2;

			UnpackRGB(r,g,b, reserveRGB);

			// draw the | bar
			FillRGBA(x, y, iBarWidth, gHUD.m_iFontHeight, r, g, b, a);

			x += iBarWidth + AmmoWidth/2;;

			// GL Seems to need this
			ScaleColors(r, g, b, a );
			x = gHUD.DrawHudNumber(x, y, iFlags | DHN_3DIGITS, gWR.CountAmmo(pw->iAmmoType), r, g, b);


		}
		else
		{
			// SPR_Draw a bullets only line
			x = ScreenWidth - 4 * AmmoWidth - iIconWidth;
			x = gHUD.DrawHudNumber(x, y, iFlags | DHN_3DIGITS, gWR.CountAmmo(pw->iAmmoType), r, g, b);
		}

		// Draw the ammo Icon
		int iOffset = (m_pWeapon->rcAmmo.bottom - m_pWeapon->rcAmmo.top) * iIconScale / 8;
		if (m_pWeapon->hAmmo)	// no icon: skip (a draw after SPR_Set(0) would show the previous sprite)
			HUD_DrawIconAdditive( m_pWeapon->hAmmo, x, y - iOffset, &m_pWeapon->rcAmmo, r, g, b, iIconScale );
	}

	// Does weapon have seconday ammo?
	if (pw->iAmmo2Type > 0)
	{
		const int iIconScale = WeaponIconScale( m_pWeapon );
		int iIconWidth = ( m_pWeapon->rcAmmo2.right - m_pWeapon->rcAmmo2.left ) * iIconScale;

		// Do we have secondary ammo?
		if ((pw->iAmmo2Type != 0) && (gWR.CountAmmo(pw->iAmmo2Type) > 0))
		{
			y -= gHUD.m_iFontHeight + gHUD.m_iFontHeight/4;
			x = ScreenWidth - 4 * AmmoWidth - iIconWidth;
			x = gHUD.DrawHudNumber(x, y, iFlags|DHN_3DIGITS, gWR.CountAmmo(pw->iAmmo2Type), r, g, b);

			// Draw the ammo Icon
			int iOffset = (m_pWeapon->rcAmmo2.bottom - m_pWeapon->rcAmmo2.top) * iIconScale / 8;
			if (m_pWeapon->hAmmo2)
				HUD_DrawIconAdditive( m_pWeapon->hAmmo2, x, y - iOffset, &m_pWeapon->rcAmmo2, r, g, b, iIconScale );
		}
	}
	return 1;
}


//
// Draws the ammo bar on the hud
//
int DrawBar(int x, int y, int width, int height, float f)
{
	int r, g, b;

	if (f >= 0 && f != 0)
	{
		if (f > 1)
			f = 1;
		int w = f * width;

		// Always show at least one pixel (one tier pixel) if we have ammo.
		if (w <= 0)
			w = gHUD.m_iHudScale;
		UnpackRGB(r, g, b, RGB_GREENISH);
		FillRGBA(x, y, w, height, r, g, b, (int)( 255 * s_flMenuFade ));
		x += w;
		width -= w;
	}

	UnpackRGB(r, g, b, RGB_YELLOWISH);

	FillRGBA(x, y, width, height, r, g, b, (int)( 128 * s_flMenuFade ));

	return (x + width);
}



void DrawAmmoBar(WEAPON *p, int x, int y, int width, int height)
{
	if ( !p )
		return;

	if (p->iAmmoType != -1)
	{
		if (!gWR.CountAmmo(p->iAmmoType))
			return;

		float f = (float)gWR.CountAmmo(p->iAmmoType)/(float)p->iMax1;

		x = DrawBar(x, y, width, height, f);


		// Do we have secondary ammo too?

		if (p->iAmmo2Type != -1)
		{
			f = (float)gWR.CountAmmo(p->iAmmo2Type)/(float)p->iMax2;

			x += 5 * gHUD.m_iHudScale; //!!!

			DrawBar(x, y, width, height, f);
		}
	}
}




//
// Draw Weapon Menu
//
int CHudAmmo::DrawWList(float)
{
	int r,g,b,x,y,a,i;

	if ( !gpActiveSel )
		return 0;

	// fading out after a while unused (MenuFade): everything below drawn that much dimmer
	const float fade = s_flMenuFade = MenuFade();
	if ( fade <= 0.0f )
		return 0;

	int iActiveSlot;

	if ( gpActiveSel == (WEAPON *)1 )
		iActiveSlot = -1;	// current slot has no weapons
	else
		iActiveSlot = gpActiveSel->iSlot;

	const int S = gHUD.m_iHudScale;	// layout scale of the HUD tier (1 at 640/320)
	y = 10 * S; //!!!


	// Ensure that there are available choices in the active slot
	if ( iActiveSlot > 0 )
	{
		if ( !gWR.GetFirstPos( iActiveSlot ) )
		{
			gpActiveSel = (WEAPON *)1;
			iActiveSlot = -1;
		}
	}

	// the menu starts right of the radar: the game's place clears its round radar, but the square one can be bigger
	// (it was drawn over the menu); kept on the screen when the radar is very big
	const bool hiRes = gHUD.m_iRes >= 640;
	int left = hiRes ? 138 * S : 70;
	const int radarRight = gHUD.m_Health.RadarRight();
	if ( radarRight > 0 && radarRight + ( hiRes ? 10 * S : 6 ) > left )
	{
		int menuWide = 0;
		for ( i = 0; i < MAX_WEAPON_SLOTS; i++ )
		{
			WEAPON *p = i == iActiveSlot ? gWR.GetFirstPos( i ) : NULL;
			menuWide += ( p ? ( p->rcActive.right - p->rcActive.left ) * WeaponIconScale( p ) : giBucketWidth ) + 5 * S;
		}
		int wanted = radarRight + ( hiRes ? 10 * S : 6 );
		if ( wanted > ScreenWidth - menuWide )
			wanted = ScreenWidth - menuWide;
		if ( wanted > left )
			left = wanted;
	}
	x = left;

	// Draw top line
	for ( i = 0; i < MAX_WEAPON_SLOTS; i++ )
	{
		int iWidth;

		UnpackRGB(r,g,b, RGB_YELLOWISH);

		if ( iActiveSlot == i )
			a = 255;
		else
			a = 192;

		ScaleColors(r, g, b, (int)( 255 * fade ));
		const int iBucket = m_HUD_bucket0 >= 0 ? m_HUD_bucket0 + i : -1;
		const HSPRITE hBucket = gHUD.GetSprite( iBucket );
		if ( hBucket )
			SPR_Set( hBucket, r, g, b );

		// make active slot wide enough to accomodate gun pictures
		if ( i == iActiveSlot )
		{
			WEAPON *p = gWR.GetFirstPos(iActiveSlot);
			if ( p )
				iWidth = ( p->rcActive.right - p->rcActive.left ) * WeaponIconScale( p );
			else
				iWidth = giBucketWidth;
		}
		else
			iWidth = giBucketWidth;

		if ( hBucket )
			SPR_DrawAdditive(0, x, y, &gHUD.GetSpriteRect(iBucket));

		x += iWidth + 5 * S;
	}


	a = 128; //!!!
	x = left;

	// Draw all of the buckets
	for (i = 0; i < MAX_WEAPON_SLOTS; i++)
	{
		y = giBucketHeight + 10 * S;

		// If this is the active slot, draw the bigger pictures,
		// otherwise just draw boxes
		if ( i == iActiveSlot )
		{
			WEAPON *p = gWR.GetFirstPos( i );
			int iWidth = giBucketWidth;
			if ( p )
				iWidth = ( p->rcActive.right - p->rcActive.left ) * WeaponIconScale( p );

			for ( int iPos = 0; iPos < MAX_WEAPON_POSITIONS; iPos++ )
			{
				p = gWR.GetWeaponSlot( i, iPos );

				if ( !p || !p->iId )
					continue;

				UnpackRGB( r,g,b, RGB_YELLOWISH );

				// if active, then we must have ammo.

				if ( gpActiveSel == p )
				{
					if (gWR.HasAmmo(p))
						ScaleColors(r, g, b, (int)( 192 * fade ));
					else
					{
						UnpackRGB(r, g, b, RGB_REDISH);
						ScaleColors(r, g, b, (int)( 128 * fade ));
					}
					if ( p->hActive )
						HUD_DrawIconAdditive( p->hActive, x, y, &p->rcActive, r, g, b, WeaponIconScale( p ) );

					// a weapon whose icons stayed at 640 and are not enlarged gets the 640 selection frame
					if ( p->iRes != gHUD.m_iRes && WeaponIconScale( p ) == 1 && gHUD.m_hsprSelection640 )
					{
						SPR_Set( gHUD.m_hsprSelection640, r, g, b );
						SPR_DrawAdditive( 0, x, y, &gHUD.m_rcSelection640 );
					}
					else if ( gHUD.GetSprite(m_HUD_selection) )
					{
						SPR_Set(gHUD.GetSprite(m_HUD_selection), r, g, b );
						SPR_DrawAdditive(0, x, y, &gHUD.GetSpriteRect(m_HUD_selection));
					}
				}
				else
				{
					// Draw Weapon if Red if no ammo

					if ( gWR.HasAmmo(p) )
						ScaleColors(r, g, b, (int)( 192 * fade ));
					else
					{
						UnpackRGB(r,g,b, RGB_REDISH);
						ScaleColors(r, g, b, (int)( 128 * fade ));
					}

					if ( p->hInactive )
						HUD_DrawIconAdditive( p->hInactive, x, y, &p->rcInactive, r, g, b, WeaponIconScale( p ) );
				}

				// Draw Ammo Bar

				DrawAmmoBar(p, x + giABWidth/2, y + 3 * S, giABWidth, giABHeight);

				y += ( p->rcActive.bottom - p->rcActive.top ) * WeaponIconScale( p ) + 5 * S;
			}

			x += iWidth + 5 * S;

		}
		else
		{
			// Draw Row of weapons.

			UnpackRGB(r,g,b, RGB_YELLOWISH);

			for ( int iPos = 0; iPos < MAX_WEAPON_POSITIONS; iPos++ )
			{
				WEAPON *p = gWR.GetWeaponSlot( i, iPos );

				if ( !p || !p->iId )
					continue;

				if ( gWR.HasAmmo(p) )
				{
					UnpackRGB(r,g,b, RGB_YELLOWISH);
					a = 128;
				}
				else
				{
					UnpackRGB(r,g,b, RGB_REDISH);
					a = 96;
				}

				FillRGBA( x, y, giBucketWidth, giBucketHeight, r, g, b, (int)( a * fade ) );

				y += giBucketHeight + 5 * S;
			}

			x += giBucketWidth + 5 * S;
		}
	}

	return 1;

}


int CHudAmmo::MsgFunc_Crosshair(const char *, int size, void *buffer)
{
	BEGIN_READ(buffer, size);
	int show = READ_BYTE();
	if (gEngfuncs.IsSpectateOnly() || CVAR_GET_FLOAT("cl_observercrosshair") == 0)
		return 1;
	if (show)
	{
		SetCrosshair(m_hObserverCrosshair, m_rcObserverCrosshair, 255, 255, 255);
	}
	else
	{
		static wrect_t nullrc;
		SetCrosshair(0, nullrc, 0, 0, 0);
	}
	m_bObserverCrosshair = 1;
	return 1;
}

int CHudAccountBalance::Init()
{
	HOOK_MESSAGE(Money);
	HOOK_MESSAGE(BlinkAcct);
	gHUD.AddHudElem(this);
	Reset();
	return 1;
}

void CHudAccountBalance::Reset()
{
	m_fFade = 0;
	m_fFadeFast = 0;
	m_iFlags |= HUD_ACTIVE;
	m_fBlinkTime = 0;
	m_iAccount = 0;
	m_iAccountDelta = 0;
	m_iBlinkCount = 0;
}

int CHudAccountBalance::VidInit()
{
	m_HUD_dollar = gHUD.GetSpriteIndex("dollar");
	m_HUD_minus = gHUD.GetSpriteIndex("minus");
	m_HUD_plus = gHUD.GetSpriteIndex("plus");
	return 1;
}

int CHudAccountBalance::GetBalance()
{
	return m_iAccount;
}

int CHudAccountBalance::MsgFunc_Money(const char *, int size, void *buffer)
{
	BEGIN_READ(buffer, size);
	int account = READ_LONG();
	int show = READ_BYTE();
	m_iAccountDelta = account - m_iAccount;
	m_iAccount = account;
	m_bShowDelta = show;
	if (m_iAccountDelta)
	{
		m_fFade = 5;
		m_fFadeFast = 2.5f;
	}
	else if (show)
	{
		m_bShowDelta = 0;
	}
	gCSViewPortInterface->UpdateBuyPresets();
	return 1;
}

int CHudAccountBalance::MsgFunc_BlinkAcct(const char *, int size, void *buffer)
{
	BEGIN_READ(buffer, size);
	m_iBlinkCount = 2 * READ_BYTE();
	m_fBlinkTime = 0;
	return 1;
}

int CHudAccountBalance::Draw(float time)
{
	if (gEngfuncs.IsSpectateOnly())
		return 1;
	int hr, hg, hb;
	HudColor(hr, hg, hb);
	// hud_color "dynamic": red at $1000 and below, yellow at $4000 (a full buy), green from $8000, blended in between
	if (HudColorDynamic())
		UnpackRGB(hr, hg, hb, HudDynamicRGB((float)m_iAccount, 1000.0f, 4000.0f, 8000.0f));
	int r = hr, g = hg, b = hb;
	int alpha = MIN_ALPHA;
	if (m_fFade != 0)
	{
		m_fFade = (float)(m_fFade - gHUD.m_flTimeDelta);
		if (m_fFade <= 0)
		{
			m_fFade = 0;
			m_iAccountDelta = 0;
		}
		else
		{
			double fade = m_fFade / 5.0;
			alpha = (int)(float)(128.0 * fade + MIN_ALPHA);
			double remaining = (5.0 - m_fFade) / 5.0;
			if (m_iAccountDelta > 0)
			{
				r = (int)(float)(hr * remaining + 0.0 * fade);
				g = (int)(float)(hg * remaining + fade * 255.0);
				b = (int)(float)(0.0 * fade + remaining * hb);
			}
			else if (m_iAccountDelta)
			{
				r = (int)(float)(hr * remaining + 255.0 * fade);
				g = (int)(float)(hg * remaining + fade * 0.0);
				b = (int)(float)(fade * 0.0 + remaining * hb);
			}
		}
	}
	ScaleColors(r, g, b, alpha);
	int deltaR = r, deltaG = g, deltaB = b;
	if (m_bShowDelta && m_fFadeFast != 0)
	{
		m_fFadeFast = (float)(m_fFadeFast - gHUD.m_flTimeDelta);
		if (m_fFadeFast <= 0)
		{
			m_fFadeFast = 0;
			m_bShowDelta = 0;
		}
		else
		{
			deltaR = r;
			deltaG = g;
			deltaB = b;
			ScaleColors(deltaR, deltaG, deltaB, (int)(m_fFadeFast / 2.5 * 256.0));
		}
	}
	if (m_iBlinkCount > 0)
	{
		if (time > m_fBlinkTime)
		{
			m_iBlinkCount--;
			m_fBlinkTime = (float)(time + 0.1);
		}
		if (m_iBlinkCount & 1)
		{
			r = hr;
			g = hg;
			b = hb;
			ScaleColors(r, g, b, 255);
		}
	}
	if ((gHUD.m_iHideHUDDisplay & HIDEHUD_MONEY) || g_iUser1)
		return 1;
	wrect_t &number = gHUD.GetSpriteRect(gHUD.m_HUD_number_0);
	int x = ScreenWidth - (gHUD.m_iRes >= 640 ? 16 * gHUD.m_iHudScale : 12) - 5 * (number.right - number.left);
	int y = ScreenHeight - ((gHUD.m_iWeaponBits & (1U << WEAPON_SUIT)) ? 3 : 5) * gHUD.m_iFontHeight;
	wrect_t &dollar = gHUD.GetSpriteRect(m_HUD_dollar);
	if ( gHUD.GetSprite(m_HUD_dollar) )
	{
		SPR_Set(gHUD.GetSprite(m_HUD_dollar), r, g, b);
		SPR_DrawAdditive(0, x + dollar.left - dollar.right, y, &dollar);
	}
	gHUD.DrawHudNumber(x, y, m_iAccount, r, g, b);
	if (m_bShowDelta)
	{
		int sprite = m_iAccountDelta < 0 ? m_HUD_minus : m_HUD_plus;
		int deltaY = y - (gHUD.m_iFontHeight / 2 + gHUD.m_iFontHeight);
		wrect_t &rect = gHUD.GetSpriteRect(sprite);
		if ( gHUD.GetSprite(sprite) )
		{
			SPR_Set(gHUD.GetSprite(sprite), deltaR, deltaG, deltaB);
			SPR_DrawAdditive(0, x + rect.left - rect.right, deltaY, &rect);
		}
		gHUD.DrawHudNumber(x, deltaY, abs(m_iAccountDelta), deltaR, deltaG, deltaB);
	}
	return 1;
}

// The crosshair settings were looked up by name (a walk through every console variable) every frame. The variables
// are found once now; their values are still read every frame.
static cvar_t *CrosshairCvar(cvar_t *&cache, const char *name)
{
	if (!cache)
		cache = gEngfuncs.pfnGetCvarPointer(name);
	return cache;
}

// once, after config.cfg has run (in a map, or when the Advanced window opens): cl_crosshair_rgb / cl_crosshair_additive
// still "" take the values of cl_crosshair_color / cl_crosshair_translucent
void Crosshair_TakeOverCvars()
{
	static bool s_done;
	if (s_done)
		return;
	s_done = true;
	static const char *const kPairs[2][3] = {
		{ "cl_crosshair_color", "cl_crosshair_rgb", "50 250 50" },
		{ "cl_crosshair_translucent", "cl_crosshair_additive", "1" },
	};
	for (int i = 0; i < 2; i++)
	{
		const char *current = gEngfuncs.pfnGetCvarString((char *)kPairs[i][1]);
		if (current && current[0])
			continue;
		const char *old = gEngfuncs.pfnGetCvarString((char *)kPairs[i][0]);
		gEngfuncs.Cvar_Set((char *)kPairs[i][1], (char *)(old && old[0] ? old : kPairs[i][2]));
	}
}

void CHudAmmo::CalculateCrosshairColor()
{
	static cvar_t *cvar;
	static char parsed[64] = "\x01";
	Crosshair_TakeOverCvars();
	CrosshairCvar(cvar, "cl_crosshair_rgb");
	const char *text = cvar ? cvar->string : CVAR_GET_STRING("cl_crosshair_rgb");
	if (!text)
		return;
	// parsing the same text gives the same colour: only parse it again when it changed
	if (!strcmp(text, parsed))
		return;
	strncpy(parsed, text, sizeof(parsed) - 1);
	parsed[sizeof(parsed) - 1] = 0;
	text = SharedParse(text);
	const char *token = SharedGetToken();
	if (!token)
		return;
	int r = atoi(token);
	text = SharedParse(text);
	token = SharedGetToken();
	if (!token)
		return;
	int g = atoi(token);
	SharedParse(text);
	token = SharedGetToken();
	if (!token)
		return;
	int b = atoi(token);
	if (r != m_cvarR || g != m_cvarG || b != m_cvarB)
	{
		m_R = max(0, min(255, r));
		m_G = max(0, min(255, g));
		m_B = max(0, min(255, b));
		m_cvarR = r;
		m_cvarG = g;
		m_cvarB = b;
	}
}

void CHudAmmo::CalculateCrosshairDrawMode()
{
	static cvar_t *cvar;
	CrosshairCvar(cvar, "cl_crosshair_additive");
	float translucent = cvar ? cvar->value : CVAR_GET_FLOAT("cl_crosshair_additive");
	if (translucent == 0)
		m_bAdditive = false;
	else if (translucent == 1)
		m_bAdditive = true;
	else
		gEngfuncs.Con_Printf("usage: cl_crosshair_additive <1|0>\n");
}

// always the "auto" size (the biggest above 1023 wide): the Advanced window sizes the crosshair with its length, so
// cl_crosshair_size (and the game's Multiplayer options' crosshair size) no longer changes it
void CHudAmmo::CalculateCrosshairSize()
{
	m_iCrosshairScaleBase = Crosshair_ScaleBase(0, ScreenWidth);
}

void CHudAmmo::UserCmd_Adjust_Crosshair()
{
	int newCrosshair = (m_iCurrentCrosshair + 1) % 5;
	if (gEngfuncs.Cmd_Argc() > 1)
		newCrosshair = atoi(gEngfuncs.Cmd_Argv(1)) % 10;
	m_iCurrentCrosshair = newCrosshair;

	switch (m_iCurrentCrosshair)
	{
	case 1:
		m_R = 250;
		m_G = 50;
		m_B = 50;
		m_bAdditive = true;
		break;
	case 2:
		m_R = 50;
		m_G = 50;
		m_B = 250;
		m_bAdditive = true;
		break;
	case 3:
		m_R = 250;
		m_G = 250;
		m_B = 50;
		m_bAdditive = true;
		break;
	case 4:
		m_R = 50;
		m_G = 250;
		m_B = 250;
		m_bAdditive = true;
		break;
	case 5:
		m_R = 50;
		m_G = 250;
		m_B = 50;
		m_bAdditive = false;
		break;
	case 6:
		m_R = 250;
		m_G = 50;
		m_B = 50;
		m_bAdditive = false;
		break;
	case 7:
		m_R = 50;
		m_G = 50;
		m_B = 250;
		m_bAdditive = false;
		break;
	case 8:
		m_R = 250;
		m_G = 250;
		m_B = 50;
		m_bAdditive = false;
		break;
	case 9:
		m_R = 50;
		m_G = 250;
		m_B = 250;
		m_bAdditive = false;
		break;
	default:
		m_R = 50;
		m_G = 250;
		m_B = 50;
		m_bAdditive = true;
		break;
	}

	char command[255];
	command[0] = 0;
	sprintf(command, "%d %d %d", m_R, m_G, m_B);
	gEngfuncs.Cvar_Set("cl_crosshair_rgb", command);
	if (m_bAdditive)
		gEngfuncs.Cvar_Set("cl_crosshair_additive", "1");
	else
		gEngfuncs.Cvar_Set("cl_crosshair_additive", "0");
}

int CHudAmmo::DrawCrosshair(float, int)
{
	// the same crosshair on every gun: the one the Advanced window shows (crosshair_draw.h; the game gives each gun its
	// own resting distance, opening per shot and widening)
	const CrosshairProfile &profile = kCrosshairProfile;
	int distance = profile.distance;
	const int delta = profile.delta;
	// wider while moving (crosshair_draw.cpp, the game's sums)
	if (profile.accuracy && cl_dynamiccrosshair && cl_dynamiccrosshair->value != 0
		&& !(gHUD.m_iHideHUDDisplay & HIDEHUD_WEAPONS))
		distance = Crosshair_RestingDistance(distance, profile.accuracy, (g_iPlayerFlags & FL_ONGROUND) != 0,
			(g_iPlayerFlags & FL_DUCKING) != 0, (int)g_flPlayerSpeed > profile.fastSpeed);
	// the Advanced window's options (cvars found once, read every frame)
	static cvar_t *thicknessCvar, *lengthCvar, *gapCvar, *alphaCvar, *outlineCvar, *dotCvar, *tshapeCvar, *recoilCvar;
	CrosshairSettings settings;
	memset(&settings, 0, sizeof(settings));
	settings.thickness = CrosshairCvar(thicknessCvar, "cl_crosshair_thickness") ? (int)thicknessCvar->value : 1;
	settings.length = CrosshairCvar(lengthCvar, "cl_crosshair_length") ? (int)lengthCvar->value : 0;
	settings.gap = CrosshairCvar(gapCvar, "cl_crosshair_gap") ? (int)gapCvar->value : 0;
	settings.alpha = CrosshairCvar(alphaCvar, "cl_crosshair_alpha") ? (int)alphaCvar->value : 0;
	settings.outline = CrosshairCvar(outlineCvar, "cl_crosshair_outline") ? (int)outlineCvar->value : 0;
	settings.dot = CrosshairCvar(dotCvar, "cl_crosshair_dot") ? (int)dotCvar->value : 0;
	settings.tshape = CrosshairCvar(tshapeCvar, "cl_crosshair_t") && tshapeCvar->value != 0;
	settings.recoil = !CrosshairCvar(recoilCvar, "cl_crosshair_recoil") || recoilCvar->value != 0;

	// the spread: a shot this frame opens it up, otherwise it closes back (crosshair_draw.cpp, the game's sums)
	const bool shot = m_iAmmoLastCheck < g_iShotsFired;
	if (g_iShotsFired > 600)
		g_iShotsFired = 1;
	m_iAmmoLastCheck = g_iShotsFired;
	Crosshair_Step(m_flCrosshairDistance, m_iAlpha, distance, delta, shot, settings.recoil);
	CalculateCrosshairColor();
	CalculateCrosshairDrawMode();
	CalculateCrosshairSize();
	int gap, length;
	Crosshair_Measure(m_flCrosshairDistance, distance, settings.gap, settings.length, ScreenWidth, m_iCrosshairScaleBase, &gap, &length);
	int r = m_R, g = m_G, b = m_B;
	if (gHUD.m_NightVision.m_fOn)
	{
		r = 250;
		g = 50;
		b = 50;
	}
	const bool additive = !gHUD.m_NightVision.m_fOn && m_bAdditive;
	CrosshairRect rects[CROSSHAIR_MAX_RECTS];
	const int count = Crosshair_Build(settings, additive, r, g, b, m_iAlpha, ScreenWidth / 2, ScreenHeight / 2, gap, length, rects);
	for (int i = 0; i < count; i++)
	{
		const CrosshairRect &rect = rects[i];
		(rect.additive ? gEngfuncs.pfnFillRGBA : gEngfuncs.pfnFillRGBABlend)(rect.x, rect.y, rect.w, rect.h, rect.r, rect.g, rect.b, rect.a);
	}
	return 1;
}


/* =================================
	GetSpriteList

Finds and returns the matching 
sprite name 'psz' and resolution 'iRes'
in the given sprite list 'pList'
iCount is the number of items in the pList
================================= */
client_sprite_t *GetSpriteList(client_sprite_t *pList, const char *psz, int iRes, int iCount)
{
	if (!pList)
		return NULL;

	int i = iCount;
	client_sprite_t *p = pList;

	while(i--)
	{
		if ((p->iRes == iRes) && (!strcmp(psz, p->szName)))
			return p;
		p++;
	}

	return NULL;
}
