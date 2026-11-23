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
// flashlight.cpp
//
// implementation of CHudFlashlight class
//

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "../public/game_controls/iviewport.h"

DECLARE_MESSAGE(m_Flash, Flashlight)
DECLARE_MESSAGE(m_Flash, FlashBat)

int CHudFlashlight::Init()
{
	m_fFade = 0;
	m_fOn = 0;
	HOOK_MESSAGE(Flashlight);
	HOOK_MESSAGE(FlashBat);
	m_iFlags |= HUD_ACTIVE;
	gHUD.AddHudElem(this);
	return 1;
}

void CHudFlashlight::Reset()
{
	m_fFade = 0;
	m_fOn = 0;
}

int CHudFlashlight::VidInit()
{
	int empty = gHUD.GetSpriteIndex("flash_empty");
	int full = gHUD.GetSpriteIndex("flash_full");
	int beam = gHUD.GetSpriteIndex("flash_beam");
	m_hSprite1 = gHUD.GetSprite(empty);
	m_hSprite2 = gHUD.GetSprite(full);
	m_hBeam = gHUD.GetSprite(beam);
	m_rc1 = gHUD.GetSpriteRect(empty);
	m_rc2 = gHUD.GetSpriteRect(full);
	m_rcBeam = gHUD.GetSpriteRect(beam);
	m_iWidth = m_rc2.right - m_rc2.left;
	return 1;
}

int CHudFlashlight::MsgFunc_FlashBat(const char *, int size, void *buffer)
{
	BEGIN_READ(buffer, size);
	m_iBat = READ_BYTE();
	m_flBat = (float)(m_iBat / 100.0);
	return 1;
}

int CHudFlashlight::MsgFunc_Flashlight(const char *, int size, void *buffer)
{
	BEGIN_READ(buffer, size);
	m_fOn = READ_BYTE();
	m_iBat = READ_BYTE();
	m_flBat = (float)(m_iBat / 100.0);
	return 1;
}

int CHudFlashlight::Draw(float)
{
	if (gHUD.m_iHideHUDDisplay & (HIDEHUD_FLASHLIGHT | HIDEHUD_ALL))
		return 1;
	if (!gViewPortInterface->IsTutorTextWindowOpen() && (gHUD.m_iWeaponBits & (1U << WEAPON_SUIT)) && !g_iUser1)
	{
		int r, g, b;
		int alpha = m_fOn ? 225 : MIN_ALPHA;
		if (m_flBat < 0.2)
			UnpackRGB(r, g, b, RGB_REDISH);
		else
			HudColor(r, g, b);
		ScaleColors(r, g, b, alpha);
		int y = (m_rc1.bottom - m_rc1.top) / 2;	// stock mixed the empty and full rects (same top at 640)
		int x = ScreenWidth - m_iWidth - m_iWidth / 2;

		// Draw the flashlight casing
		if (m_hSprite1)
		{
			SPR_Set(m_hSprite1, r, g, b);
			SPR_DrawAdditive(0, x, y, &m_rc1);
		}
		if (m_fOn && m_hBeam)
		{  // draw the flashlight beam
			SPR_Set(m_hBeam, r, g, b);
			SPR_DrawAdditive(0, ScreenWidth - m_iWidth / 2, y, &m_rcBeam);
		}
		int offset = (int)((1.0 - m_flBat) * m_iWidth);
		if (offset < m_iWidth && m_hSprite2)
		{
			wrect_t rect = m_rc2;
			rect.left += offset;
			SPR_Set(m_hSprite2, r, g, b);
			SPR_DrawAdditive(0, ScreenWidth - m_iWidth - m_iWidth / 2 + offset, y, &rect);
		}
	}
	return 1;
}
