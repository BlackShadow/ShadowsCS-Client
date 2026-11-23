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
// Health.cpp
//
// implementation of CHudHealth class
//

#include "stdio.h"
#include "stdlib.h"
#include "math.h"

#include "hud.h"
#include "radar2.h"
#include "vismods.h"
#include "cl_util.h"
#include "parsemsg.h"
#include <string.h>
#include "../public/game_controls/iviewport.h"
#include "ev_hldm.h"
#include "controls.h"
#include "vgui/ILocalize.h"
#include "pm_shared.h"
#include "../common/GameUI/ICareerUI.h"


DECLARE_MESSAGE(m_Health, Health )
DECLARE_MESSAGE(m_Health, Damage )
DECLARE_MESSAGE(m_Health, Radar)
DECLARE_MESSAGE(m_Health, Account)
DECLARE_MESSAGE(m_Health, HealthInfo)
DECLARE_MESSAGE(m_Health, ScoreAttrib)
DECLARE_MESSAGE(m_Health, ClCorpse)
DECLARE_MESSAGE(m_Health, ReceiveW)
DECLARE_MESSAGE(m_roundTimer, RoundTime)
DECLARE_MESSAGE(m_progressBar, BarTime)
DECLARE_MESSAGE(m_progressBar, BarTime2)
DECLARE_MESSAGE(m_careerTaskTimer, TaskTime)
DECLARE_COMMAND(m_Health, TrackPlayer)
DECLARE_COMMAND(m_Health, ClearPlayers)
DECLARE_COMMAND(m_Health, DrawRadar)
DECLARE_COMMAND(m_Health, HideRadar)

#define FADE_TIME 100

cvar_t *cl_radartype;
float g_LocationColor[3] = { 0, 0.8f, 0 };
extern cvar_t *cl_minmodels;
extern cvar_t *cl_min_t;
extern cvar_t *cl_min_ct;
extern char *sPlayerModelFiles[];
extern ICareerUI *g_pCareerUI;
bool IsBombPlanted();
bool BIsValidTModelIndex(int index);
bool BIsValidCTModelIndex(int index);

#define PAIN_NAME "sprites/%d_pain.spr"
#define DAMAGE_NAME "sprites/%d_dmg.spr"

int giDmgHeight, giDmgWidth;

int giDmgFlags[NUM_DMG_TYPES] =
{
	DMG_POISON,
	DMG_ACID,
	DMG_FREEZE|DMG_SLOWFREEZE,
	DMG_DROWN,
	DMG_BURN|DMG_SLOWBURN,
	DMG_NERVEGAS,
	DMG_RADIATION,
	DMG_SHOCK,
	DMG_CALTROP,
	DMG_TRANQ,
	DMG_CONCUSS,
	DMG_HALLUC
};

int CHudHealth::Init(void)
{
	HOOK_MESSAGE(Health);
	HOOK_MESSAGE(Damage);
	HOOK_MESSAGE(Radar);
	HOOK_MESSAGE(Account);
	HOOK_MESSAGE(HealthInfo);
	HOOK_MESSAGE(ScoreAttrib);
	HOOK_MESSAGE(ClCorpse);
	HOOK_COMMAND("trackplayer", TrackPlayer);
	HOOK_COMMAND("clearplayers", ClearPlayers);
	HOOK_COMMAND("drawradar", DrawRadar);
	HOOK_COMMAND("hideradar", HideRadar);
	HOOK_MESSAGE(ReceiveW);
	m_iHealth = 100;
	m_fFade = 0;
	m_iFlags = 0;
	m_bitsDamage = 0;
	m_fAttackFront = m_fAttackRear = m_fAttackRight = m_fAttackLeft = 0;
	giDmgHeight = 0;
	giDmgWidth = 0;

	memset(m_dmg, 0, sizeof(DAMAGE_IMAGE) * NUM_DMG_TYPES);
	memset(m_bTrackArray, 0, sizeof(m_bTrackArray));
	m_iPlayerLastPointedAt = 0;
	m_iPlayerNum = 0;
	m_bDrawRadar = true;


	gHUD.AddHudElem(this);
	return 1;
}

void CHudHealth::Reset( void )
{
	// make sure the pain compass is cleared when the player respawns
	m_fAttackFront = m_fAttackRear = m_fAttackRight = m_fAttackLeft = 0;


	// force all the flashing damage icons to expire
	m_bitsDamage = 0;
	for ( int i = 0; i < NUM_DMG_TYPES; i++ )
	{
		m_dmg[i].fExpire = 0;
	}
}

int CHudHealth::VidInit(void)
{
	m_hSprite = 0;

	m_HUD_dmg_bio = gHUD.GetSpriteIndex( "dmg_bio" ) + 1;
	m_HUD_cross = gHUD.GetSpriteIndex( "cross" );

	giDmgHeight = gHUD.GetSpriteRect(m_HUD_dmg_bio).right - gHUD.GetSpriteRect(m_HUD_dmg_bio).left;
	giDmgWidth = gHUD.GetSpriteRect(m_HUD_dmg_bio).bottom - gHUD.GetSpriteRect(m_HUD_dmg_bio).top;
	int radar = gHUD.GetSpriteIndex("radar");
	int opaque = gHUD.GetSpriteIndex("radaropaque");
	const int radarSize = 128 * gHUD.m_iHudScale;
	m_hrad = &gHUD.GetSpriteRect(radar);	// (an invalid index yields gHUD's scratch zero rect, never out of bounds)
	m_hrad->left = m_hrad->top = 0;
	m_hrad->right = m_hrad->bottom = radarSize;
	m_hradopaque = &gHUD.GetSpriteRect(opaque);
	m_hradopaque->left = m_hradopaque->top = 0;
	m_hradopaque->right = m_hradopaque->bottom = radarSize;
	m_hRadar = gHUD.GetSprite(radar);
	m_hRadaropaque = gHUD.GetSprite(opaque);
	return 1;
}

int CHudHealth:: MsgFunc_Health(const char *,  int iSize, void *pbuf )
{
	// TODO: update local health data
	BEGIN_READ( pbuf, iSize );
	int x = READ_BYTE();

	m_iFlags |= HUD_ACTIVE;

	// Only update the fade if we've changed health
	if (x != m_iHealth)
	{
		m_fFade = FADE_TIME;
		m_iHealth = x;
	}

	return 1;
}


int CHudHealth:: MsgFunc_Damage(const char *,  int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	int armor = READ_BYTE();	// armor
	int damageTaken = READ_BYTE();	// health
	long bitsDamage = READ_LONG(); // damage bits

	vec3_t vecFrom;

	for ( int i = 0 ; i < 3 ; i++)
		vecFrom[i] = READ_COORD();

	UpdateTiles(gHUD.m_flTime, bitsDamage);

	// Actually took damage?
	if ( damageTaken > 0 || armor > 0 )
		CalcDamageDirection(vecFrom);

	return 1;
}


// Returns back a color from the
// Green <-> Yellow <-> Red ramp
void CHudHealth::GetPainColor( int &r, int &g, int &b )
{
	// hud_color "dynamic": green at 100, yellow at 50, red at 25 and below, blended in between
	if (HudColorDynamic())
	{
		UnpackRGB(r, g, b, HudDynamicRGB((float)DisplayHealth(), 25.0f, 50.0f, 100.0f));
		return;
	}
	if (m_iHealth > 25)
	{
		UnpackRGB(r,g,b, RGB_YELLOWISH);
	}
	else
	{
		r = 250;
		g = 0;
		b = 0;
	}
}

// The Health message carries one byte, so servers send at most 255 (some wrap around instead: 300 arrives as 44).
// Two other sources know more:
//  - HealthInfo (the scoreboard health of this game version's servers), which by default also tells each player
//    their own health, as a full number
//  - the server's copy of our health in the client data, exact up to 510 (the network field holds no more)
// Either is used only while it agrees with the Health message (255 = "255 or more", or the same lowest byte), so a
// stale value never shows.
extern int g_iClientDataHealth;

int CHudHealth::DisplayHealth()
{
	cl_entity_t* local = gEngfuncs.GetLocalPlayer();
	int candidate = -1;
	if (local && local->index >= 1 && local->index <= MAX_PLAYERS && g_PlayerExtraInfo[local->index].sb_health > 255)
		candidate = g_PlayerExtraInfo[local->index].sb_health;
	else if (g_iClientDataHealth > 255 && g_iClientDataHealth < 511)
		candidate = g_iClientDataHealth;
	if (candidate > 255 && (m_iHealth == 255 || (candidate & 0xFF) == m_iHealth))
		return candidate;
	return m_iHealth;
}

int CHudHealth::Draw(float flTime)
{
	GetAllPlayersInfo();
	int r, g, b;
	int a = 0, x, y;
	int HealthWidth;

	if ( (gHUD.m_iHideHUDDisplay & HIDEHUD_HEALTH) || g_iUser1 )
		return 1;
	if (!gHUD.m_fPlayerDead && m_bDrawRadar)
		DrawRadar(flTime);

	if ( !m_hSprite )
		m_hSprite = LoadSprite(PAIN_NAME);

	// above 255 the Health message stays at 255: flash on changes of the real value too
	const int shownHealth = DisplayHealth();
	if (shownHealth != m_iShownHealth)
	{
		if (m_iShownHealth > 255 || shownHealth > 255)
			m_fFade = FADE_TIME;
		m_iShownHealth = shownHealth;
	}

	// Has health changed? Flash the health #
	if (m_fFade)
	{
		m_fFade -= (gHUD.m_flTimeDelta * 20);
		if (m_fFade <= 0)
		{
			a = MIN_ALPHA;
			m_fFade = 0;
		}

		// Fade the health number back to dim

		a = MIN_ALPHA +  (m_fFade/FADE_TIME) * 128;

	}
	else
		a = MIN_ALPHA;

	// If health is getting low, make it bright red
	if (m_iHealth <= 15)
		a = 255;

	GetPainColor( r, g, b );
	ScaleColors(r, g, b, a );

	// Only draw health if we have the suit.
	if (gHUD.m_iWeaponBits & (1<<(WEAPON_SUIT)))
	{
		HealthWidth = gHUD.GetSpriteRect(gHUD.m_HUD_number_0).right - gHUD.GetSpriteRect(gHUD.m_HUD_number_0).left;
		int CrossWidth = gHUD.GetSpriteRect(m_HUD_cross).right - gHUD.GetSpriteRect(m_HUD_cross).left;

		y = ScreenHeight - gHUD.m_iFontHeight - gHUD.m_iFontHeight / 2;
		x = CrossWidth /2;

		SPR_Set(gHUD.GetSprite(m_HUD_cross), r, g, b);
		SPR_DrawAdditive(0, x, y, &gHUD.GetSpriteRect(m_HUD_cross));

		x = CrossWidth + HealthWidth / 2;

		x = gHUD.DrawHudNumber(x, y, DHN_3DIGITS | DHN_DRAWZERO, shownHealth, r, g, b);

	}

	DrawDamage(flTime);
	return DrawPain(flTime);
}

void CHudHealth::CalcDamageDirection(vec3_t vecFrom)
{
	vec3_t	forward, right, up;
	float	side, front;
	vec3_t vecOrigin, vecAngles;

	if (!vecFrom[0] && !vecFrom[1] && !vecFrom[2])
	{
		m_fAttackFront = m_fAttackRear = m_fAttackRight = m_fAttackLeft = 0;
		return;
	}


	memcpy(vecOrigin, gHUD.m_vecOrigin, sizeof(vec3_t));
	memcpy(vecAngles, gHUD.m_vecAngles, sizeof(vec3_t));


	VectorSubtract (vecFrom, vecOrigin, vecFrom);

	float flDistToTarget = vecFrom.Length();

	vecFrom = vecFrom.Normalize();
	AngleVectors (vecAngles, forward, right, up);

	front = DotProduct (vecFrom, right);
	side = DotProduct (vecFrom, forward);

	if (flDistToTarget <= 50)
	{
		m_fAttackFront = m_fAttackRear = m_fAttackRight = m_fAttackLeft = 1;
	}
	else
	{
		if (side > 0)
		{
			if (side > 0.3)
				m_fAttackFront = max(m_fAttackFront, side);
		}
		else
		{
			float f = fabs(side);
			if (f > 0.3)
				m_fAttackRear = max(m_fAttackRear, f);
		}

		if (front > 0)
		{
			if (front > 0.3)
				m_fAttackRight = max(m_fAttackRight, front);
		}
		else
		{
			float f = fabs(front);
			if (f > 0.3)
				m_fAttackLeft = max(m_fAttackLeft, f);
		}
	}
}

int CHudHealth::DrawPain(float)
{
	if (!(m_fAttackFront || m_fAttackRear || m_fAttackLeft || m_fAttackRight))
		return 1;

	int r, g, b;
	int x, y, a, shade;

	// TODO:  get the shift value of the health
	a = 255;	// max brightness until then

	float fFade = gHUD.m_flTimeDelta * 2;

	// SPR_Draw top
	if (m_fAttackFront > 0.4)
	{
		GetPainColor(r,g,b);
		shade = a * max( m_fAttackFront, 0.5f );
		ScaleColors(r, g, b, shade);
		SPR_Set(m_hSprite, r, g, b );

		x = ScreenWidth/2 - SPR_Width(m_hSprite, 0)/2;
		y = ScreenHeight/2 - SPR_Height(m_hSprite,0) * 3;
		SPR_DrawAdditive(0, x, y, NULL);
		m_fAttackFront = max( 0.0f, m_fAttackFront - fFade );
	} else
		m_fAttackFront = 0;

	if (!(m_fAttackRight <= 0.4))
	{
		GetPainColor(r,g,b);
		shade = a * max( m_fAttackRight, 0.5f );
		ScaleColors(r, g, b, shade);
		SPR_Set(m_hSprite, r, g, b );

		x = ScreenWidth/2 + SPR_Width(m_hSprite, 1) * 2;
		y = ScreenHeight/2 - SPR_Height(m_hSprite,1)/2;
		SPR_DrawAdditive(1, x, y, NULL);
		m_fAttackRight = max( 0.0f, m_fAttackRight - fFade );
	} else
		m_fAttackRight = 0;

	if (!(m_fAttackRear <= 0.4))
	{
		GetPainColor(r,g,b);
		shade = a * max( m_fAttackRear, 0.5f );
		ScaleColors(r, g, b, shade);
		SPR_Set(m_hSprite, r, g, b );

		x = ScreenWidth/2 - SPR_Width(m_hSprite, 2)/2;
		y = ScreenHeight/2 + SPR_Height(m_hSprite,2) * 2;
		SPR_DrawAdditive(2, x, y, NULL);
		m_fAttackRear = max( 0.0f, m_fAttackRear - fFade );
	} else
		m_fAttackRear = 0;

	if (m_fAttackLeft > 0.4)
	{
		GetPainColor(r,g,b);
		shade = a * max( m_fAttackLeft, 0.5f );
		ScaleColors(r, g, b, shade);
		SPR_Set(m_hSprite, r, g, b );

		x = ScreenWidth/2 - SPR_Width(m_hSprite, 3) * 3;
		y = ScreenHeight/2 - SPR_Height(m_hSprite,3)/2;
		SPR_DrawAdditive(3, x, y, NULL);

		m_fAttackLeft = max( 0.0f, m_fAttackLeft - fFade );
	} else
		m_fAttackLeft = 0;

	return 1;
}

int CHudHealth::DrawDamage(float flTime)
{
	int r, g, b, a;
	DAMAGE_IMAGE *pdmg;

	if (!m_bitsDamage)
		return 1;

	UnpackRGB(r,g,b, RGB_YELLOWISH);

	a = (int)( fabs(sin(flTime*2)) * 256.0);

	ScaleColors(r, g, b, a);

	// Draw all the items
	int i;
	for ( i = 0; i < NUM_DMG_TYPES; i++)
	{
		if (m_bitsDamage & giDmgFlags[i])
		{
			pdmg = &m_dmg[i];
			SPR_Set(gHUD.GetSprite(m_HUD_dmg_bio + i), r, g, b );
			SPR_DrawAdditive(0, pdmg->x, pdmg->y, &gHUD.GetSpriteRect(m_HUD_dmg_bio + i));
		}
	}


	// check for bits that should be expired
	for ( i = 0; i < NUM_DMG_TYPES; i++ )
	{
		pdmg = &m_dmg[i];

		if ( m_bitsDamage & giDmgFlags[i] )
		{
			pdmg->fExpire = flTime + DMG_IMAGE_LIFE > pdmg->fExpire ?
				pdmg->fExpire : flTime + DMG_IMAGE_LIFE;

			if ( !(flTime < pdmg->fExpire)		// when the time has expired
				&& a < 40 )						// and the flash is at the low point of the cycle
			{
				pdmg->fExpire = 0;

				int y = pdmg->y;
				pdmg->x = pdmg->y = 0;

				// move everyone above down
				for (int j = 0; j < NUM_DMG_TYPES; j++)
				{
					pdmg = &m_dmg[j];
					if ((pdmg->y) && (pdmg->y < y))
						pdmg->y += giDmgHeight;

				}

				m_bitsDamage &= ~giDmgFlags[i];  // clear the bits
			}
		}
	}

	return 1;
}


void CHudHealth::UpdateTiles(float flTime, long bitsDamage)
{
	DAMAGE_IMAGE *pdmg;

	// Which types are new?
	long bitsOn = ~m_bitsDamage & bitsDamage;

	for (int i = 0; i < NUM_DMG_TYPES; i++)
	{
		pdmg = &m_dmg[i];

		// Is this one already on?
		if (m_bitsDamage & giDmgFlags[i])
		{
			pdmg->fExpire = flTime + DMG_IMAGE_LIFE; // extend the duration
			if (!pdmg->fBaseline)
				pdmg->fBaseline = flTime;
		}

		// Are we just turning it on?
		if (bitsOn & giDmgFlags[i])
		{
			// put this one at the bottom
			pdmg->x = giDmgWidth/8;
			pdmg->y = ScreenHeight - giDmgHeight * 2;
			pdmg->fExpire=flTime + DMG_IMAGE_LIFE;

			// move everyone else up
			for (int j = 0; j < NUM_DMG_TYPES; j++)
			{
				if (j == i)
					continue;

				pdmg = &m_dmg[j];
				if (pdmg->y)
					pdmg->y -= giDmgHeight;

			}
			pdmg = &m_dmg[i];
		}
	}

	// damage bits are only turned on here;  they are turned off when the draw time has expired (in DrawDamage())
	m_bitsDamage |= bitsDamage;
}

int CHudHealth::MsgFunc_Radar(const char *, int size, void *buffer)
{
	BEGIN_READ(buffer, size);
	int player = READ_BYTE();
	float x = READ_COORD();
	float y = READ_COORD();
	float z = READ_COORD();
	g_PlayerExtraInfo[player].origin.x = x;
	g_PlayerExtraInfo[player].origin.y = y;
	g_PlayerExtraInfo[player].origin.z = z;
	return 1;
}

int CHudHealth::MsgFunc_Account(const char *, int size, void *buffer)
{
	BEGIN_READ(buffer, size);
	int player = READ_BYTE();
	int account = READ_LONG();
	if ((unsigned int)(player - 1) <= 63)
		g_PlayerExtraInfo[player].sb_account = account;
	return 1;
}

int CHudHealth::MsgFunc_HealthInfo(const char *, int size, void *buffer)
{
	BEGIN_READ(buffer, size);
	int player = READ_BYTE();
	int health = READ_LONG();
	if ((unsigned int)(player - 1) <= 63)
		g_PlayerExtraInfo[player].sb_health = health;
	return 1;
}

int CHudHealth::MsgFunc_ScoreAttrib(const char *, int size, void *buffer)
{
	BEGIN_READ(buffer, size);
	short player = (short)READ_BYTE();
	int attributes = READ_BYTE();
	if ((unsigned int)(player - 1) <= 63)
	{
		g_PlayerExtraInfo[player].dead = (attributes & 1) != 0;
		g_PlayerExtraInfo[player].has_c4 = (attributes & 2) != 0;
		g_PlayerExtraInfo[player].vip = (attributes & 4) != 0;
		g_PlayerExtraInfo[player].has_defuse_kit = (attributes & 8) != 0;
	}
	return 1;
}

int CHudHealth::MsgFunc_ClCorpse(const char *, int size, void *buffer)
{
	BEGIN_READ(buffer, size);
	const char *modelName = READ_STRING();
	Vector origin, angles;
	for (int i = 0; i < 3; i++)
		origin[i] = (float)(READ_LONG() * 0.0078125);
	for (int i = 0; i < 3; i++)
		angles[i] = READ_COORD();
	int animationOffset = READ_LONG();
	float animationTime = (float)(gEngfuncs.GetClientTime() + animationOffset / 100.0);
	int sequence = READ_BYTE();
	int body = READ_BYTE();
	int team = READ_BYTE();
	int player = READ_BYTE();
	char model[64];
	if (!cl_minmodels || cl_minmodels->value == 0)
	{
		if (strstr(modelName, "models/"))
			_snprintf(model, sizeof(model), "%s", modelName);
		else
			_snprintf(model, sizeof(model), "models/player/%s/%s.mdl", modelName, modelName);
	}
	else if (team == 1)
	{
		if (cl_min_t && BIsValidTModelIndex((int)cl_min_t->value))
			_snprintf(model, sizeof(model), "%s", sPlayerModelFiles[(int)cl_min_t->value]);
		else
			strcpy(model, "models/player/leet/leet.mdl");
	}
	else if (team == 2)
	{
		if (g_PlayerExtraInfo[player].vip)
			strcpy(model, "models/player/vip/vip.mdl");
		else if (cl_min_ct && BIsValidCTModelIndex((int)cl_min_ct->value))
			_snprintf(model, sizeof(model), "%s", sPlayerModelFiles[(int)cl_min_ct->value]);
		else
			strcpy(model, "models/player/gign/gign.mdl");
	}
	else
		strcpy(model, "models/player/player.mdl");
	model[sizeof(model) - 1] = 0;
	CreateCorpse(origin, angles, model, animationTime, sequence, body, team, player);
	return 1;
}

void CHudHealth::GetAllPlayersInfo()
{
	for (int i = 1; i < 64; i++)
	{
		gEngfuncs.pfnGetPlayerInfo(i, &g_PlayerInfoList[i]);
		if (g_PlayerInfoList[i].thisplayer)
			m_iPlayerNum = i;
	}
}

void CHudHealth::UserCmd_DrawRadar()
{
	m_bDrawRadar = true;
}

void CHudHealth::UserCmd_HideRadar()
{
	m_bDrawRadar = false;
}

void CHudHealth::UserCmd_TrackPlayer()
{
	if ((unsigned int)(m_iPlayerLastPointedAt - 1) <= 31)
		m_bTrackArray[m_iPlayerLastPointedAt] = true;
}

void CHudHealth::UserCmd_ClearPlayers()
{
	memset(m_bTrackArray, 0, sizeof(m_bTrackArray));
}

void CHudHealth::DrawPlayerLocation()
{
	wchar_t *location = vgui2::localize()->Find(g_PlayerExtraInfo[m_iPlayerNum].location);
	if (!location)
		return;
	int width, height;
	gHUD.m_VGUI2Print.GetStringSize(location, &width, &height);
	int x = (gHUD.m_iRes >= 640 ? 64 * gHUD.m_iHudScale : 32) - width / 2;
	if (x < 0)
		x = 0;
	int y = (gHUD.m_iRes >= 640 ? 128 * gHUD.m_iHudScale : 64) + height / 2;
	gHUD.m_VGUI2Print.DrawVGUI2String(location, x, y,
		g_LocationColor[0], g_LocationColor[1], g_LocationColor[2]);
}

void CHudHealth::DrawRadarDot(int x, int y, float z, int size, int flags, int r, int g, int b, int alpha)
{
	const int S = gHUD.m_iHudScale;	// pixel unit of the HUD tier (size already includes it)
	if (flags & 4)
		FillRGBA(x - 3 * S, y - 3 * S, size + 3 * S, size + 3 * S, r, g, b, alpha);
	else if (z < -128)
	{
		int offset = (-z <= 3096 ? (int)(float)(-z / 400.0) : 7) * S;
		FillRGBA(x, y, S, 2 * size + offset, r, g, b, alpha);
		FillRGBA(x - 2 * S, y + 2 * size + offset, 5 * S, S, r, g, b, alpha);
	}
	else if (z <= 128)
	{
		if ((flags & 2) || ((flags & 1) && !(flags & 8)))
			FillRGBA(x - S, y - S, size + S, size + S, r, g, b, alpha);
		else
		{
			FillRGBA(x, y, size, size, r, g, b, alpha);
			if (flags & 1)
			{
				FillRGBA(x - 2 * S, y - 2 * S, size, size, r, g, b, alpha);
				FillRGBA(x - 2 * S, y + 2 * S, size, size, r, g, b, alpha);
				FillRGBA(x + 2 * S, y - 2 * S, size, size, r, g, b, alpha);
				FillRGBA(x + 2 * S, y + 2 * S, size, size, r, g, b, alpha);
			}
		}
	}
	else
	{
		int offset = (z <= 3096 ? (int)(float)(z / 400.0) : 7) * S;
		FillRGBA(x - 2 * S, y, 5 * S, S, r, g, b, alpha);
		FillRGBA(x, y + S, S, offset + 2 * size - S, r, g, b, alpha);
	}
}

int CHudRoundTimer::Init()
{
	HOOK_MESSAGE(RoundTime);
	gHUD.AddHudElem(this);
	Reset();
	return 1;
}

void CHudRoundTimer::Reset()
{
	m_iFlags |= HUD_ACTIVE;
	m_fFade = m_flTimeEnd = m_flNewPeriod = m_flNextToggle = 0;
	m_bPanicColor = 0;
	m_closestRight = ScreenWidth;
}

int CHudRoundTimer::VidInit()
{
	m_HUD_stopwatch = gHUD.GetSpriteIndex("stopwatch");
	return 1;
}

int CHudRoundTimer::MsgFunc_RoundTime(const char *, int size, void *buffer)
{
	BEGIN_READ(buffer, size);
	m_flNewPeriod = (float)READ_SHORT();
	m_bPanicColor = 0;
	m_flNextToggle = 0;
	return 1;
}

int CHudRoundTimer::Draw(float time)
{
	if (m_flNewPeriod != 0)
	{
		m_flTimeEnd = m_flNewPeriod + time;
		m_flNewPeriod = 0;
	}
	if ((gHUD.m_iHideHUDDisplay & HIDEHUD_TIMER) ||
		(gViewPortInterface && gViewPortInterface->IsSpectatorGUIVisible()) || IsBombPlanted())
		return 1;
	int seconds = (int)(float)(m_flTimeEnd - time);
	if (seconds <= 0)
	{
		seconds = 0;
		m_bPanicColor = 1;
	}
	else if (seconds <= 30 && time > m_flNextToggle)
	{
		double interval = seconds <= 2 ? 0.05 : seconds <= 5 ? 0.1 :
			seconds <= 10 ? 0.2 : seconds <= 20 ? 0.4 : 0.8;
		m_flNextToggle = (float)(time + interval);
		m_bPanicColor = 1 - m_bPanicColor;
	}
	int r = 255, g = 16, b = 16;
	if (!m_bPanicColor)
	{
		HudColor(r, g, b);
		// hud_color "dynamic": red at 30 seconds and below (where it starts flashing), yellow at 60, green from 90
		if (HudColorDynamic())
			UnpackRGB(r, g, b, HudDynamicRGB((float)seconds, 30.0f, 60.0f, 90.0f));
		ScaleColors(r, g, b, 100);
	}
	int digitWidth = gHUD.GetSpriteRect(gHUD.m_HUD_number_0).right -
		gHUD.GetSpriteRect(gHUD.m_HUD_number_0).left;
	int x = (ScreenWidth - digitWidth * ((gHUD.m_iRes < 640) + 4)) / 2;
	int y = (gHUD.m_iWeaponBits & (1 << WEAPON_SUIT)) ?
		ScreenHeight - gHUD.m_iFontHeight - gHUD.m_iFontHeight / 2 : 42 * gHUD.m_iHudScale;
	if (ScreenWidth > 399)
	{
		wrect_t &rect = gHUD.GetSpriteRect(m_HUD_stopwatch);
		if (gHUD.GetSprite(m_HUD_stopwatch))
		{
			SPR_Set(gHUD.GetSprite(m_HUD_stopwatch), r, g, b);
			SPR_DrawAdditive(0, x + rect.left - rect.right, y, &rect);
		}
	}
	int textY = y - (gHUD.m_iFontEngineHeight - gHUD.m_iFontHeight) / 2 - 1;
	x = gHUD.DrawHudNumber(x, y, DHN_DRAWZERO | DHN_2DIGITS, seconds / 60, r, g, b);
	x = gHUD.DrawHudString(x, textY, ScreenWidth - 1, ":", r, g, b);
	x = gHUD.DrawHudNumber(x, y, DHN_DRAWZERO | DHN_2DIGITS | DHN_FILLZERO, seconds % 60, r, g, b);
	m_closestRight = (int)(digitWidth * 1.5 + x);
	return 1;
}

int CHudProgressBar::Init()
{
	HOOK_MESSAGE(BarTime);
	HOOK_MESSAGE(BarTime2);
	gHUD.AddHudElem(this);
	Reset();
	return 1;
}

void CHudProgressBar::Reset()
{
	m_iFlags |= HUD_ACTIVE;
	m_tmEnd = m_tmNewPeriod = 0;
	const int S = gHUD.m_iHudScale;
	m_height = 10 * S;
	m_width = (int)(float)(ScreenWidth * 0.5);
	m_x0 = m_width / 2;
	m_x1 = m_width + m_x0 - S;
	m_y0 = (int)(float)(ScreenHeight * 0.5 + 64 * S);
	m_y1 = m_y0 + m_height - S;
}

int CHudProgressBar::VidInit()
{
	Reset();
	return 1;
}

int CHudProgressBar::MsgFunc_BarTime(const char *, int size, void *buffer)
{
	BEGIN_READ(buffer, size);
	int period = READ_SHORT();
	m_tmNewPeriod = period ? (float)period : -1;
	m_tmNewElapsed = 0;
	return 1;
}

int CHudProgressBar::MsgFunc_BarTime2(const char *, int size, void *buffer)
{
	BEGIN_READ(buffer, size);
	int period = READ_SHORT();
	int elapsedPercent = READ_SHORT();
	m_tmNewPeriod = period ? (float)period : -1;
	m_tmNewElapsed = (float)(period * (double)0.01f * elapsedPercent);
	return 1;
}

int CHudProgressBar::Draw(float time)
{
	if (m_tmNewPeriod != 0)
	{
		m_tmEnd = m_tmNewPeriod + time - m_tmNewElapsed;
		m_tmNewPeriod = 0;
		m_tmStart = time - m_tmNewElapsed;
		m_tmNewElapsed = 0;
	}
	if (time > m_tmEnd || m_tmStart > time)
		return 1;
	const int S = gHUD.m_iHudScale;
	int r, g, b;
	HudColor(r, g, b);
	FillRGBA(m_x0, m_y0, S, m_height, r, g, b, 80);
	FillRGBA(m_x1, m_y0, S, m_height, r, g, b, 80);
	FillRGBA(m_x0 + S, m_y0, m_width - 2 * S, S, r, g, b, 80);
	FillRGBA(m_x0 + S, m_y1, m_width - 2 * S, S, r, g, b, 80);
	float progress = (float)((m_width - 4 * S) * (double)(time - m_tmStart) / (m_tmEnd - m_tmStart));
	int filled = (int)progress;
	if (filled >= m_width - 3 * S)
		filled = m_width - 4 * S;
	FillRGBA(m_x0 + 2 * S, m_y0 + 2 * S, filled, m_height - 4 * S, r, g, b, 160);
	return 1;
}

// cl_radar_square: the square map radar (radar2.cpp). Same people/objects as the round radar, same flash timing.
void CHudHealth::DrawSquareRadar(float time)
{
	// bounded: a team name of 16+ letters from the server used to run past this buffer (and crash the game)
	char teamName[16];
	strncpy(teamName, g_PlayerExtraInfo[m_iPlayerNum].teamname, sizeof(teamName) - 1);
	teamName[sizeof(teamName) - 1] = 0;
	static Radar2Blip blips[64];
	static float s_lastPos[33][3];   // last radar position of each living teammate
	static bool s_seenAlive[33];
	static float s_deathTime[33];
	static float s_deathPos[33][3];
	static float s_lastTrack = 0.0f;
	static char s_trackLevel[64] = "";
	{
		const char *level = gEngfuncs.pfnGetLevelName();
		if (!level)
			level = "";
		// nothing is known about deaths from while the radar was not drawn, nor from another map
		if (time < s_lastTrack || time - s_lastTrack > 0.5f || strncmp(level, s_trackLevel, sizeof(s_trackLevel) - 1))
		{
			memset(s_seenAlive, 0, sizeof(s_seenAlive));
			memset(s_deathTime, 0, sizeof(s_deathTime));
			strncpy(s_trackLevel, level, sizeof(s_trackLevel) - 1);
			s_trackLevel[sizeof(s_trackLevel) - 1] = 0;
		}
		s_lastTrack = time;
	}
	int n = 0;
	cl_entity_t *local = gEngfuncs.GetLocalPlayer();
	for (int i = 1; i < 34 && n < 64; i++)
	{
		extra_player_info_t &player = g_PlayerExtraInfo[i];
		if (i == m_iPlayerNum || strcmp(teamName, player.teamname) || player.dead)
			continue;
		if (player.radarflash != -1 && !(time <= player.radarflash) && player.radarflashes > 0)
		{
			if (i == 33)
				player.radarflash = time + (player.playerclass == 1 ? 0.15f : 0.35f);
			else
				player.radarflash = (float)(time + 0.15);
			player.radarflashon = 1 - player.radarflashon;
			player.radarflashes--;
		}
		Radar2Blip &b = blips[n];
		memset(&b, 0, sizeof(b));
		b.flash = player.radarflashon == 1 && player.radarflashes > 0;
		VectorCopy(player.origin, b.origin);
		if (i == 33)
		{
			// the bomb: Terrorists only, while its position is known (dropped or planted)
			if (g_iTeamNumber != 1 || player.radarflashes <= 0)
				continue;
			b.kind = RADAR2_BOMB;
			b.planted = player.playerclass == 1;
			n++;
			continue;
		}
		b.kind = RADAR2_PLAYER;
		b.team = player.teamnumber;
		b.vip = VisMods_IsRoundVIP(i) && (g_iTeamNumber & ~2) == 0;
		b.carrier = player.has_c4 && (unsigned int)g_iTeamNumber <= 1;
		b.tracked = m_bTrackArray[i];
		// a teammate the game is showing you right now: exact position and facing
		cl_entity_t *ent = gEngfuncs.GetEntityByIndex(i);
		if (ent && local && ent->player && ent->curstate.messagenum == local->curstate.messagenum)
		{
			VectorCopy(ent->origin, b.origin);
			b.yaw = ent->angles[1];
			b.hasYaw = true;
		}
		VectorCopy(b.origin, s_lastPos[i]);
		n++;
	}

	// red X where a teammate died: shown for 5 seconds, then it fades out over one (gone sooner if they are alive again)
	for (int i = 1; i < 33; i++)
	{
		extra_player_info_t &player = g_PlayerExtraInfo[i];
		const bool mate = i != m_iPlayerNum && player.teamname[0] && !strcmp(teamName, player.teamname);
		if (!mate || !player.dead || time < s_deathTime[i])
		{
			s_seenAlive[i] = mate && !player.dead;
			s_deathTime[i] = 0.0f;
			continue;
		}
		if (s_seenAlive[i])
		{
			s_seenAlive[i] = false;
			s_deathTime[i] = time;
			VectorCopy(s_lastPos[i], s_deathPos[i]);
		}
		const float age = time - s_deathTime[i];
		if (s_deathTime[i] <= 0.0f || age >= 6.0f || n >= 64)
			continue;
		Radar2Blip &b = blips[n++];
		memset(&b, 0, sizeof(b));
		b.kind = RADAR2_DEAD;
		VectorCopy(s_deathPos[i], b.origin);
		b.fade = age > 5.0f ? age - 5.0f : 0.0f;
	}
	for (int i = 0; i < 24 && n < 64; i++)
	{
		hostage_info_t &hostage = g_HostageInfo[i];
		if (strcmp(teamName, hostage.teamname) || hostage.dead)
		{
			if (!hostage.dead || hostage.radarflash == -1)
				continue;
		}
		if (hostage.radarflash != -1 && !(time <= hostage.radarflash))
		{
			if (hostage.radarflashes <= 0)
				continue;
			hostage.radarflash = time + (hostage.dead ? 0.15f : 0.35f);
			hostage.radarflashon = 1 - hostage.radarflashon;
			hostage.radarflashes--;
		}
		Radar2Blip &b = blips[n++];
		memset(&b, 0, sizeof(b));
		b.kind = RADAR2_HOSTAGE;
		VectorCopy(hostage.origin, b.origin);
		b.dead = hostage.dead;
		b.flash = hostage.radarflashon == 1 && hostage.radarflashes > 0;
	}
	int x, y, size;
	Radar2_GetRect(&x, &y, &size);
	const int bottom = Radar2_Draw(gHUD.m_vecOrigin, gHUD.m_vecAngles, blips, n, time);
	DrawPlayerLocationAt(x + size / 2, bottom + 2 * gHUD.m_iHudScale);
}

void CHudHealth::DrawPlayerLocationAt(int centerX, int top)
{
	wchar_t *location = vgui2::localize()->Find(g_PlayerExtraInfo[m_iPlayerNum].location);
	if (!location)
		return;
	int width, height;
	gHUD.m_VGUI2Print.GetStringSize(location, &width, &height);
	int x = centerX - width / 2;
	if (x < 0)
		x = 0;
	gHUD.m_VGUI2Print.DrawVGUI2String(location, x, top + height / 2,
		g_LocationColor[0], g_LocationColor[1], g_LocationColor[2]);
}

// the same checks as Draw and DrawRadar (the weapon menu is drawn before this element, so it asks)
int CHudHealth::RadarRight()
{
	if (!(m_iFlags & HUD_ACTIVE) || (gHUD.m_iHideHUDDisplay & HIDEHUD_HEALTH) || g_iUser1 || gHUD.m_fPlayerDead
		|| !m_bDrawRadar || g_PlayerExtraInfo[m_iPlayerNum].dead)
		return 0;
	if (Radar2_Enabled())
	{
		int x, y, size;
		Radar2_GetRect(&x, &y, &size);
		return x + size;
	}
	return gHUD.m_iRes >= 640 ? 128 * gHUD.m_iHudScale : 64;	// the round radar's sprite
}

void CHudHealth::DrawRadar(float time)
{
	// bounded: a team name of 16+ letters from the server used to run past this buffer (and crash the game)
	char teamName[16];
	strncpy(teamName, g_PlayerExtraInfo[m_iPlayerNum].teamname, sizeof(teamName) - 1);
	teamName[sizeof(teamName) - 1] = 0;
	Vector origin = gHUD.m_vecOrigin;
	if (g_PlayerExtraInfo[m_iPlayerNum].dead)
		return;
	if (Radar2_Enabled())
	{
		DrawSquareRadar(time);
		return;
	}
	if (cl_radartype && cl_radartype->value != 0)
	{
		if (m_hRadaropaque)
		{
			SPR_Set(m_hRadaropaque, 200, 200, 200);
			SPR_DrawHoles(0, 0, 0, m_hradopaque);
		}
	}
	else if (m_hRadar)
	{
		SPR_Set(m_hRadar, 25, 75, 25);
		SPR_DrawAdditive(0, 0, 0, m_hrad);
	}
	for (int i = 0; i < 34; i++)
	{
		extra_player_info_t &player = g_PlayerExtraInfo[i];
		if (i == m_iPlayerNum || strcmp(teamName, player.teamname) || player.dead)
			continue;
		float xDiff = player.origin.x - origin.x;
		float yDiff = player.origin.y - origin.y;
		float angle = (float)atan((double)yDiff / xDiff);
		angle = (float)(angle * 180.0 / 3.141592654);
		double bearing = angle;
		if (xDiff < 0)
			bearing += 180;
		else if (yDiff < 0)
			bearing += 360;
		float distance = (float)sqrt((double)xDiff * xDiff + (double)yDiff * yDiff);
		float radians = (float)((gHUD.m_vecAngles.y - bearing) * 3.141592654);
		double sine = sin(radians / 180.0), cosine = cos(radians / 180.0);
		float rotatedX = (float)(cosine * 0.0 - sine * -distance);
		float rotatedY = (float)(0.0 * sine + -distance * cosine);
		const bool hiRes = gHUD.m_iRes >= 640;
		int center = hiRes ? 64 * gHUD.m_iHudScale : 32;
		int edge = center * 2;
		int size = hiRes ? 2 * gHUD.m_iHudScale : 1;
		double range = hiRes ? 2048 : 1024;
		double scale = distance <= range ? 1.0 : distance / range;
		int x = center + (int)(float)(rotatedX / scale * 0.03125 * gHUD.m_iHudScale);
		int y = center + (int)(float)(rotatedY / scale * 0.03125 * gHUD.m_iHudScale);
		if (x < 0 || y < 0 || x > edge || y > edge)
			continue;
		int r, g, b;
		if ((player.has_c4 && (unsigned int)g_iTeamNumber <= 1) ||
			(VisMods_IsRoundVIP(i) && (g_iTeamNumber & ~2) == 0))
		{
			r = 250;
			g = b = 0;
		}
		else if (m_bTrackArray[i])
		{
			r = 185;
			g = b = 20;
			size *= 2;
		}
		else
		{
			r = 150;
			g = 75;
			b = 250;
		}
		float z = player.origin.z - origin.z;
		if (i != 33)
			DrawRadarDot(x, y, z, size, 0, r, g, b, 235);
		if (player.radarflash != -1 && !(time <= player.radarflash))
		{
			if (player.radarflashes <= 0)
				continue;
			if (i == 33)
				player.radarflash = time + (player.playerclass == 1 ? 0.15f : 0.35f);
			else
				player.radarflash = (float)(time + 0.15);
			player.radarflashon = 1 - player.radarflashon;
			player.radarflashes--;
		}
		if (player.radarflashon == 1 && player.radarflashes > 0)
		{
			if (i != 33)
				DrawRadarDot(x, y, z, size, 4, 230, 110, 25, 245);
			else if (g_iTeamNumber == 1)
				DrawRadarDot(x, y, z, size, 1 + (player.playerclass == 1 ? 8 : 0), 250, 0, 0, 245);
		}
	}
	for (int i = 0; i < 24; i++)
	{
		hostage_info_t &hostage = g_HostageInfo[i];
		if (strcmp(teamName, hostage.teamname) || hostage.dead)
		{
			if (!hostage.dead || hostage.radarflash == -1)
				continue;
		}
		float xDiff = hostage.origin.x - origin.x;
		float yDiff = hostage.origin.y - origin.y;
		float angle = (float)atan((double)yDiff / xDiff);
		angle = (float)(angle * 180.0 / 3.141592654);
		double bearing = angle;
		if (xDiff < 0)
			bearing += 180;
		else if (yDiff < 0)
			bearing += 360;
		float distance = (float)sqrt((double)xDiff * xDiff + (double)yDiff * yDiff);
		float radians = (float)((gHUD.m_vecAngles.y - bearing) * 3.141592654);
		double sine = sin(radians / 180.0), cosine = cos(radians / 180.0);
		float rotatedX = (float)(cosine * 0.0 - sine * -distance);
		float rotatedY = (float)(0.0 * sine + -distance * cosine);
		const bool hiRes = gHUD.m_iRes >= 640;
		int center = hiRes ? 64 * gHUD.m_iHudScale : 32;
		int edge = center * 2;
		int size = hiRes ? 2 * gHUD.m_iHudScale : 1;
		double range = hiRes ? 2048 : 1024;
		double scale = distance <= range ? 1.0 : distance / range;
		int x = center + (int)(float)(rotatedX / scale * 0.03125 * gHUD.m_iHudScale);
		int y = center + (int)(float)(rotatedY / scale * 0.03125 * gHUD.m_iHudScale);
		if (x < 0 || y < 0 || x > edge || y > edge)
			continue;
		if (hostage.radarflash != -1 && !(time <= hostage.radarflash))
		{
			if (hostage.radarflashes <= 0)
				continue;
			hostage.radarflash = time + (hostage.dead ? 0.15f : 0.35f);
			hostage.radarflashon = 1 - hostage.radarflashon;
			hostage.radarflashes--;
		}
		if (hostage.radarflashon == 1 && hostage.radarflashes > 0)
			DrawRadarDot(x, y, hostage.origin.z - origin.z, size, 2,
				hostage.dead ? 250 : 50, hostage.dead ? 25 : 50, hostage.dead ? 25 : 255, 245);
	}
	DrawPlayerLocation();
}

int CHudCareerTaskTimer::Init()
{
	HOOK_MESSAGE(TaskTime);
	gHUD.AddHudElem(this);
	Reset();
	return 1;
}

void CHudCareerTaskTimer::Reset()
{
	m_iFlags |= HUD_ACTIVE;
	m_fFade = m_flTimeEnd = m_flNextToggle = m_flFadeDelay = m_flFadeStart = 0;
	m_flNewPeriod = -1;
	m_bPanicColor = 0;
	m_bTaskComplete = m_bCountdown = false;
	m_flAlpha = 1;
}

int CHudCareerTaskTimer::VidInit()
{
	m_HUD_stopwatch = gHUD.GetSpriteIndex("stopwatch");
	return 1;
}

int CHudCareerTaskTimer::MsgFunc_TaskTime(const char *, int size, void *buffer)
{
	BEGIN_READ(buffer, size);
	m_flNewPeriod = (float)READ_SHORT();
	m_bCountdown = READ_BYTE() != 0;
	int fadeDelay = READ_BYTE();
	m_bPanicColor = 0;
	m_flAlpha = 1;
	m_flFadeStart = 0;
	m_bTaskComplete = m_flNewPeriod < -1;
	m_flFadeDelay = (float)fadeDelay;
	m_flNextToggle = 0;
	return 1;
}

int CHudCareerTaskTimer::Draw(float time)
{
	if (!g_pCareerUI || !g_pCareerUI->IsPlayingMatch() || !g_pCareerUI->GetWinfastLength() ||
		m_flNewPeriod == -1)
		return 1;
	if (m_bCountdown)
	{
		if (m_flNewPeriod > 0)
		{
			m_flTimeEnd = m_flNewPeriod + time;
			m_flNewPeriod = 0;
		}
	}
	else
		m_flTimeEnd = time - m_flNewPeriod;
	if ((gHUD.m_iHideHUDDisplay & HIDEHUD_TIMER) ||
		(gViewPortInterface && gViewPortInterface->IsSpectatorGUIVisible()))
		return 1;
	int seconds = m_bCountdown ? (int)(float)(m_flTimeEnd - time) : -(int)m_flNewPeriod;
	if (seconds <= 0)
	{
		seconds = 0;
		m_bPanicColor = 1;
	}
	else if (seconds <= 30 && time > m_flNextToggle)
	{
		double interval = seconds <= 2 ? 0.05 : seconds <= 5 ? 0.1 :
			seconds <= 10 ? 0.2 : seconds <= 20 ? 0.4 : 0.8;
		m_flNextToggle = (float)(time + interval);
		m_bPanicColor = 1 - m_bPanicColor;
	}
	int r, g, b;
	if (m_bTaskComplete || !m_bPanicColor)
	{
		if (m_bTaskComplete)
		{
			r = 0;
			g = 160;
			b = 0;
		}
		else
			HudColor(r, g, b);
		ScaleColors(r, g, b, 100);
	}
	else
	{
		r = 255;
		g = b = 16;
	}
	if (m_flFadeDelay != 0)
	{
		m_flFadeStart = m_flFadeDelay + time;
		m_flFadeDelay = 0;
	}
	if (m_flFadeStart != 0 && time > m_flFadeStart)
	{
		double alpha = 1.0 - (time - (double)m_flFadeStart) / 3.0;
		int shade;
		if (alpha < 0)
		{
			m_flAlpha = 0;
			shade = 0;
		}
		else
		{
			m_flAlpha = (float)alpha;
			shade = (int)(float)(alpha * 255.0);
		}
		ScaleColors(r, g, b, shade);
	}
	int digitWidth = gHUD.GetSpriteRect(gHUD.m_HUD_number_0).right -
		gHUD.GetSpriteRect(gHUD.m_HUD_number_0).left;
	int x = (ScreenWidth - digitWidth * ((gHUD.m_iRes < 640) + 4)) / 2;
	int y = (gHUD.m_iWeaponBits & (1 << WEAPON_SUIT)) ?
		ScreenHeight - gHUD.m_iFontHeight - gHUD.m_iFontHeight / 2 : 42 * gHUD.m_iHudScale;
	y -= gHUD.m_iFontHeight + gHUD.m_iFontHeight / 2;
	if (ScreenWidth > 399)
	{
		wrect_t &rect = gHUD.GetSpriteRect(m_HUD_stopwatch);
		if (gHUD.GetSprite(m_HUD_stopwatch))
		{
			SPR_Set(gHUD.GetSprite(m_HUD_stopwatch), r, g, b);
			SPR_DrawAdditive(0, x + rect.left - rect.right, y, &rect);
		}
	}
	int textY = y - (gHUD.m_iFontEngineHeight - gHUD.m_iFontHeight) / 2 - 1;
	x = gHUD.DrawHudNumber(x, y, DHN_DRAWZERO | DHN_2DIGITS, seconds / 60, r, g, b);
	x = gHUD.DrawHudString(x, textY, ScreenWidth - 1, ":", r, g, b);
	gHUD.DrawHudNumber(x, y, DHN_DRAWZERO | DHN_2DIGITS | DHN_FILLZERO, seconds % 60, r, g, b);
	return 1;
}
