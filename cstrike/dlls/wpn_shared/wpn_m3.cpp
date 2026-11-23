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

enum m3_e
{
	M3_IDLE = 0,
	M3_FIRE1,
	M3_FIRE2,
	M3_RELOAD,
	M3_PUMP,
	M3_START_RELOAD,
	M3_DRAW,
	M3_HOLSTER,
};

LINK_ENTITY_TO_CLASS( weapon_m3, CM3 );

void CM3::Spawn( void )
{
	Precache( );
	m_iId = WEAPON_M3;
	SET_MODEL( ENT( pev ), "models/w_m3.mdl" );

	m_iDefaultAmmo = 8;

	FallInit();// get ready to fall
}


void CM3::Precache( void )
{
	PRECACHE_MODEL( "models/v_m3.mdl" );
	PRECACHE_MODEL( "models/w_m3.mdl" );

	m_iShellId = m_iShell = PRECACHE_MODEL( "models/shotgunshell.mdl" );// m3 shell

	PRECACHE_SOUND( "weapons/m3-1.wav" );
	PRECACHE_SOUND( "weapons/m3_insertshell.wav" );
	PRECACHE_SOUND( "weapons/m3_pump.wav" );

	PRECACHE_SOUND( "weapons/reload1.wav" );
	PRECACHE_SOUND( "weapons/reload3.wav" );

	m_usFireM3 = PRECACHE_EVENT( 1, "events/m3.sc" );
}

int CM3::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "buckshot";
	p->iMaxAmmo1 = 32;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 8;
	p->iSlot = 0;
	p->iPosition = 5;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_M3;
	p->iWeight = 20;

	return 1;
}


BOOL CM3::Deploy( void )
{
	return DefaultDeploy( "models/v_m3.mdl", "models/p_m3.mdl", M3_DRAW, "shotgun", UseDecrement() ? 1 : 0 );
}


void CM3::PrimaryAttack( void )
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
	m_pPlayer->FireBullets( 9, vecSrc, vecAiming, Vector( 0.0675f, 0.0675f, 0 ), 3000, BULLET_PLAYER_BUCKSHOT, 0 );

	int flag;
#if defined( CLIENT_WEAPONS )
	flag = FEV_NOTHOST;
#else
	flag = 0;
#endif

	PLAYBACK_EVENT( flag, m_pPlayer->edict(), m_usFireM3 );

	if ( !m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 );

	if ( m_iClip != 0 )
		m_flPumpTime = 0.5;

	m_flNextPrimaryAttack = GetNextAttackDelay( 0.875f );
	m_flNextSecondaryAttack = 0.875;
	if ( m_iClip != 0 )
		m_flTimeWeaponIdle = 2.5f;
	else
		m_flTimeWeaponIdle = 0.875f;
	m_fInSpecialReload = 0;

	// Update punch angles.
	m_pPlayer->pev->punchangle.x -= (float)( !( m_pPlayer->pev->flags & FL_ONGROUND ) ? UTIL_SharedRandomLong( m_pPlayer->random_seed + 1, 8, 11 ) : UTIL_SharedRandomLong( m_pPlayer->random_seed + 1, 4, 6 ) );

	m_pPlayer->m_flEjectBrass = (float)( gpGlobals->time + 0.45 );
}


void CM3::Reload( void )
{
	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 || m_iClip == 8 )
		return;

	// don't reload until recoil is done
	if ( m_flNextPrimaryAttack > 0 )
		return;

	ResetEmptySound( );

	// check to see if we're ready to reload
	if ( m_fInSpecialReload == 0 )
	{
		m_pPlayer->SetAnimation( PLAYER_RELOAD );

		m_fInSpecialReload = 1;

		SendWeaponAnim( M3_START_RELOAD, UseDecrement() ? 1 : 0 );

		m_flNextPrimaryAttack = GetNextAttackDelay( 0.55f );
		m_flNextSecondaryAttack = 0.55f;
		m_flNextReload = 0.55f;
		m_flTimeWeaponIdle = 0.55f;
	}
	else if ( m_fInSpecialReload == 1 )
	{
		if ( m_flTimeWeaponIdle > 0 )
			return;
		// was waiting for gun to move to side

		m_fInSpecialReload = 2;

		SendWeaponAnim( M3_RELOAD, UseDecrement() ? 1 : 0 );

		m_flNextReload = 0.45f;
		m_flTimeWeaponIdle = 0.45f;
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


void CM3::WeaponIdle( void )
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
			if ( m_iClip != 8 && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
			{
				Reload( );
			}
			else
			{
				// reload debounce has timed out
				SendWeaponAnim( M3_PUMP, UseDecrement() ? 1 : 0 );

				// play cocking sound
				m_fInSpecialReload = 0;
				m_flTimeWeaponIdle = 1.5;
			}
		}
		else
		{
			SendWeaponAnim( M3_IDLE, UseDecrement() ? 1 : 0 );
			m_flTimeWeaponIdle = 20;
		}
	}
}

BOOL CM3::PlayEmptySound( void )
{
	BOOL result = CBasePlayerWeapon::PlayEmptySound();
	m_iPlayEmptySound = 0;
	return result;
}
