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
//  ammohistory.cpp
//


#include "hud.h"
#include "cl_util.h"
#include "ammohistory.h"
#include <string.h>

// pixel constants of the 640 layout, times the HUD tier's scale
#define AMMO_PICKUP_GAP (gHR.iHistoryGap + 5 * gHUD.m_iHudScale)
#define AMMO_PICKUP_PICK_HEIGHT (72 * gHUD.m_iHudScale + gHR.iHistoryGap * 2)
#define AMMO_PICKUP_HEIGHT_MAX (ScreenHeight - 100 * gHUD.m_iHudScale)

HistoryResource gHR;
int HISTORY_DRAW_TIME = 5;

void HistoryResource::AddToHistory(int type, int id, int count)
{
	if (type == HISTSLOT_AMMO && !count)
		return;
	if (AMMO_PICKUP_GAP * iCurrentHistorySlot + AMMO_PICKUP_PICK_HEIGHT > AMMO_PICKUP_HEIGHT_MAX
		|| iCurrentHistorySlot >= MAX_HISTORY)
		iCurrentHistorySlot = 0;
	HIST_ITEM *item = &rgAmmoHistory[iCurrentHistorySlot++];
	HISTORY_DRAW_TIME = (int)CVAR_GET_FLOAT("hud_drawhistory_time");
	item->type = type;
	item->iId = id;
	item->iCount = count;
	item->DisplayTime = gHUD.m_flTime + (float)HISTORY_DRAW_TIME;
}

void HistoryResource::AddToHistory(int type, const char *name, int count)
{
	if (type != HISTSLOT_ITEM)
		return;
	if (AMMO_PICKUP_GAP * iCurrentHistorySlot + AMMO_PICKUP_PICK_HEIGHT > AMMO_PICKUP_HEIGHT_MAX
		|| iCurrentHistorySlot >= MAX_HISTORY)
		iCurrentHistorySlot = 0;
	HIST_ITEM *item = &rgAmmoHistory[iCurrentHistorySlot++];
	int id = gHUD.GetSpriteIndex(name);
	if (id == -1)
		return;
	item->iId = id;
	item->type = type;
	item->iCount = count;
	HISTORY_DRAW_TIME = (int)CVAR_GET_FLOAT("hud_drawhistory_time");
	item->DisplayTime = gHUD.m_flTime + (float)HISTORY_DRAW_TIME;
}

void HistoryResource::CheckClearHistory()
{
	for (int i = 0; i < MAX_HISTORY; i++)
	{
		if (rgAmmoHistory[i].type)
			return;
	}
	iCurrentHistorySlot = 0;
}

int HistoryResource::DrawAmmoHistory(float time)
{
	if ( !iCurrentHistorySlot )
		return 1;
	for (int i = 0; i < MAX_HISTORY; i++)
	{
		HIST_ITEM &item = rgAmmoHistory[i];
		if (!item.type)
			continue;
		if (item.DisplayTime > gHUD.m_flTime + (float)HISTORY_DRAW_TIME)
			item.DisplayTime = gHUD.m_flTime + (float)HISTORY_DRAW_TIME;
		if (!(time < item.DisplayTime))
		{
			memset(&item, 0, sizeof(item));
			CheckClearHistory();
			continue;
		}
		int y = ScreenHeight - (AMMO_PICKUP_PICK_HEIGHT + AMMO_PICKUP_GAP * i);
		if (item.type == HISTSLOT_AMMO)
		{
			wrect_t rect;
			int iconScale = 1;
			HSPRITE *sprite = gWR.GetAmmoPicFromWeapon(item.iId, rect, &iconScale);
			int r, g, b;
			HudColor(r, g, b);
			int alpha = (int)(float)((item.DisplayTime - time) * 80.0);
			ScaleColors(r, g, b, min(alpha, 255));
			int x = ScreenWidth - 24 * gHUD.m_iHudScale;	// the 640 ammo icon width
			if (sprite && *sprite)
				HUD_DrawIconAdditive(*sprite, x, y, &rect, r, g, b, iconScale);
			gHUD.DrawHudNumberString(x, y, ScreenWidth - 124, item.iCount, r, g, b);
		}
		else if (item.type == HISTSLOT_WEAP)
		{
			WEAPON *weapon = gWR.GetWeapon(item.iId);
			if (!weapon)
				return 1;
			int r, g, b;
			if (gWR.HasAmmo(weapon))
				HudColor(r, g, b);
			else
				UnpackRGB(r, g, b, RGB_REDISH);
			int alpha = (int)(float)((item.DisplayTime - time) * 80.0);
			ScaleColors(r, g, b, min(alpha, 255));
			const int iconScale = WeaponIconScale(weapon);
			int x = ScreenWidth - (weapon->rcInactive.right - weapon->rcInactive.left) * iconScale;
			if (weapon->hInactive)
				HUD_DrawIconAdditive(weapon->hInactive, x, y, &weapon->rcInactive, r, g, b, iconScale);
		}
		else if (item.type == HISTSLOT_ITEM && item.iId)
		{
			wrect_t rect = gHUD.GetSpriteRect(item.iId);
			int r, g, b;
			HudColor(r, g, b);
			int alpha = (int)(float)((item.DisplayTime - time) * 80.0);
			ScaleColors(r, g, b, min(alpha, 255));
			int x = ScreenWidth - (rect.right - rect.left) - 10 * gHUD.m_iHudScale;
			if (gHUD.GetSprite(item.iId))
			{
				SPR_Set(gHUD.GetSprite(item.iId), r, g, b);
				SPR_DrawAdditive(0, x, y, &rect);
			}
		}
	}
	return 1;
}
