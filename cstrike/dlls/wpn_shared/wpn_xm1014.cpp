/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
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

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "player.h"

#ifndef CLIENT_DLL
#include "bot.h"
#include "GameEvent.h"
#endif

enum xm1014_e
{
	XM1014_IDLE = 0,
	XM1014_FIRE1,
	XM1014_FIRE2,
	XM1014_RELOAD,
	XM1014_PUMP,
	XM1014_START_RELOAD,
	XM1014_DRAW,
};

LINK_ENTITY_TO_CLASS( weapon_xm1014, CXM1014 );

void CXM1014::Spawn( void )
{
	Precache( );
	m_iId = WEAPON_XM1014;
	SET_MODEL( ENT( pev ), "models/w_xm1014.mdl" );

	m_iDefaultAmmo = 7;

	FallInit();// get ready to fall
}


void CXM1014::Precache( void )
{
	PRECACHE_MODEL( "models/v_xm1014.mdl" );
	PRECACHE_MODEL( "models/w_xm1014.mdl" );

	m_iShellId = m_iShell = PRECACHE_MODEL( "models/shotgunshell.mdl" );

	PRECACHE_SOUND( "weapons/xm1014-1.wav" );

	PRECACHE_SOUND( "weapons/reload1.wav" );
	PRECACHE_SOUND( "weapons/reload3.wav" );

	m_usFireXM1014 = PRECACHE_EVENT( 1, "events/xm1014.sc" );
}

int CXM1014::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "buckshot";
	p->iMaxAmmo1 = 32;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 7;
	p->iSlot = 0;
	p->iPosition = 12;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_XM1014;
	p->iWeight = 20;

	return 1;
}


BOOL CXM1014::Deploy( void )
{
	return DefaultDeploy( "models/v_xm1014.mdl", "models/p_xm1014.mdl", XM1014_DRAW, "m249", UseDecrement() ? 1 : 0 );
}


void CXM1014::PrimaryAttack( void )
{
	// don't fire underwater
	if ( m_pPlayer->pev->waterlevel == 3 )
	{
		PlayEmptySound( );
		m_flNextPrimaryAttack = GetNextAttackDelay( 0.15f );
		return;
	}

	if ( m_iClip <= 0 )
	{
		if ( !m_fInSpecialReload )
		{
			PlayEmptySound( );

#ifndef CLIENT_DLL
			if ( TheBots )
			{
				TheBots->OnEvent( EVENT_WEAPON_FIRED_ON_EMPTY, m_pPlayer );
			}
#endif
		}

		Reload( );
		return;
	}

	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

	m_iClip--;
	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

	Vector vecAiming = gpGlobals->v_forward;
	Vector vecSrc = m_pPlayer->GetGunPosition( );
	m_pPlayer->FireBullets( 6, vecSrc, vecAiming, Vector( 0.0725f, 0.0725f, 0 ), 3048, BULLET_PLAYER_BUCKSHOT, 0 );

	int flag;
#if defined( CLIENT_WEAPONS )
	flag = FEV_NOTHOST;
#else
	flag = 0;
#endif

	PLAYBACK_EVENT_FULL( flag, m_pPlayer->edict(), m_usFireXM1014,
		0, (float *)&g_vecZero, (float *)&g_vecZero,
		m_vVecAiming.x,
		m_vVecAiming.y,
		7, (int)( m_vVecAiming.x * 100 ), m_iClip ? 0 : 1, 0 );

	if ( !m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 );

	if ( m_iClip != 0 )
		m_flPumpTime = 0.125;

	m_flNextPrimaryAttack = GetNextAttackDelay( 0.25f );
	m_flNextSecondaryAttack = 0.25;

	if ( m_iClip != 0 )
		m_flTimeWeaponIdle = 2.25f;
	else
		m_flTimeWeaponIdle = 0.75f;
	m_fInSpecialReload = 0;

	m_pPlayer->pev->punchangle.x -= (float)( !( m_pPlayer->pev->flags & FL_ONGROUND ) ? UTIL_SharedRandomLong( m_pPlayer->random_seed + 1, 7, 10 ) : UTIL_SharedRandomLong( m_pPlayer->random_seed + 1, 3, 5 ) );
}


void CXM1014::Reload( void )
{
	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 || m_iClip == 7 )
		return;

	// don't reload until recoil is done
	if ( m_flNextPrimaryAttack > 0 )
		return;

	ResetEmptySound( );

	// check to see if we're ready to reload
	if ( m_fInSpecialReload == 0 )
	{
		m_fInSpecialReload = 1;

		m_pPlayer->SetAnimation( PLAYER_RELOAD );
		SendWeaponAnim( XM1014_START_RELOAD, UseDecrement() ? 1 : 0 );

		m_pPlayer->m_flNextAttack = 0.55f;
		m_flTimeWeaponIdle = 0.55f;
		m_flNextPrimaryAttack = GetNextAttackDelay( 0.55f );
		m_flNextSecondaryAttack = 0.55f;
	}
	else if ( m_fInSpecialReload == 1 )
	{
		if ( m_flTimeWeaponIdle > 0 )
			return;
		// was waiting for gun to move to side

		m_fInSpecialReload = 2;

		if ( RANDOM_LONG( 0, 1 ) )
			EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_ITEM, "weapons/reload1.wav", 1, ATTN_NORM, 0, 85 + RANDOM_LONG( 0, 0x1f ) );
		else
			EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_ITEM, "weapons/reload3.wav", 1, ATTN_NORM, 0, 85 + RANDOM_LONG( 0, 0x1f ) );

		SendWeaponAnim( XM1014_RELOAD, UseDecrement() ? 1 : 0 );

		m_flNextReload = 0.3f;
		m_flTimeWeaponIdle = 0.3f;
	}
	else
	{
		if ( m_flTimeWeaponIdle > 0 )
			return;

		m_fInSpecialReload = 1;

		// Add them to the clip
		m_iClip += 1;
		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= 1;
		m_pPlayer->ammo_buckshot--;
	}
}


void CXM1014::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	if ( m_flPumpTime && m_flPumpTime < 0 )
	{
		// play pumping sound
		m_flPumpTime = 0;
	}

	if ( m_flTimeWeaponIdle < 0 )
	{
		if ( m_iClip == 0 && m_fInSpecialReload == 0 && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
		{
			Reload( );
		}
		else if ( m_fInSpecialReload != 0 )
		{
			if ( m_iClip != 7 && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
			{
				Reload( );
			}
			else
			{
				// reload debounce has timed out
				SendWeaponAnim( XM1014_PUMP, UseDecrement() ? 1 : 0 );

				// play cocking sound
				m_fInSpecialReload = 0;
				m_flTimeWeaponIdle = 1.5;
			}
		}
		else
		{
			SendWeaponAnim( XM1014_IDLE, UseDecrement() ? 1 : 0 );
			m_flTimeWeaponIdle = 20;
		}
	}
}

BOOL CXM1014::PlayEmptySound( void )
{
	BOOL result = CBasePlayerWeapon::PlayEmptySound();
	m_iPlayEmptySound = 0;
	return result;
}
