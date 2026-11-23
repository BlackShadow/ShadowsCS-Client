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
#include "player.h"
#include "weapons.h"

#ifndef CLIENT_DLL
#include "bot/bot.h"
#endif

enum m4a1_e
{
	M4A1_IDLE = 0,
	M4A1_SHOOT1,
	M4A1_SHOOT2,
	M4A1_SHOOT3,
	M4A1_RELOAD,
	M4A1_DRAW,
	M4A1_ATTACH_SILENCER,
	M4A1_UNSIL_IDLE,
	M4A1_UNSIL_SHOOT1,
	M4A1_UNSIL_SHOOT2,
	M4A1_UNSIL_SHOOT3,
	M4A1_UNSIL_RELOAD,
	M4A1_UNSIL_DRAW,
	M4A1_DETACH_SILENCER,
};

LINK_ENTITY_TO_CLASS( weapon_m4a1, CM4A1 );

void CM4A1::Spawn( void )
{
	Precache( );
	m_iId = WEAPON_M4A1;
	SET_MODEL( ENT( pev ), "models/w_m4a1.mdl" );

	m_iDefaultAmmo = 30;
	m_flAccuracy = 0.2f;
	m_iShotsFired = 0;
	m_bDelayFire = true;

	FallInit();// get ready to fall down.
}


void CM4A1::Precache( void )
{
	PRECACHE_MODEL( "models/v_m4a1.mdl" );
	PRECACHE_MODEL( "models/w_m4a1.mdl" );

	PRECACHE_SOUND( "weapons/m4a1-1.wav" );
	PRECACHE_SOUND( "weapons/m4a1_unsil-1.wav" );
	PRECACHE_SOUND( "weapons/m4a1_unsil-2.wav" );
	PRECACHE_SOUND( "weapons/m4a1_clipin.wav" );
	PRECACHE_SOUND( "weapons/m4a1_clipout.wav" );
	PRECACHE_SOUND( "weapons/m4a1_boltpull.wav" );
	PRECACHE_SOUND( "weapons/m4a1_deploy.wav" );
	PRECACHE_SOUND( "weapons/m4a1_silencer_on.wav" );
	PRECACHE_SOUND( "weapons/m4a1_silencer_off.wav" );

	m_iShell = PRECACHE_MODEL( "models/rshell.mdl" );// brass shell
	m_usFireM4A1 = PRECACHE_EVENT( 1, "events/m4a1.sc" );
}

int CM4A1::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "556Nato";
	p->iMaxAmmo1 = 90;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 30;
	p->iSlot = 0;
	p->iPosition = 6;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_M4A1;
	p->iWeight = 25;

	return 1;
}

BOOL CM4A1::Deploy( void )
{
	m_flAccuracy = 0.2f;
	m_iShotsFired = 0;
	m_bDelayFire = true;
	iShellOn = 1;

	if ( m_iWeaponState & WPNSTATE_M4A1_SILENCED )
		return DefaultDeploy( "models/v_m4a1.mdl", "models/p_m4a1.mdl", M4A1_DRAW, "rifle", UseDecrement() ? 1 : 0 );
	else
		return DefaultDeploy( "models/v_m4a1.mdl", "models/p_m4a1.mdl", M4A1_UNSIL_DRAW, "rifle", UseDecrement() ? 1 : 0 );
}

void CM4A1::SecondaryAttack( void )
{
	if ( !( m_iWeaponState & WPNSTATE_M4A1_SILENCED ) )
	{
		m_iWeaponState |= WPNSTATE_M4A1_SILENCED;

		SendWeaponAnim( M4A1_ATTACH_SILENCER, UseDecrement() ? 1 : 0 );
		strcpy( m_pPlayer->m_szAnimExtention, "rifle" );
	}
	else
	{
		m_iWeaponState &= ~WPNSTATE_M4A1_SILENCED;

		SendWeaponAnim( M4A1_DETACH_SILENCER, UseDecrement() ? 1 : 0 );
		strcpy( m_pPlayer->m_szAnimExtention, "rifle" );
	}

	m_flNextSecondaryAttack = 2;
	m_flNextPrimaryAttack = GetNextAttackDelay( 2 );
	m_flTimeWeaponIdle = 2;
}

void CM4A1::PrimaryAttack( void )
{
	if ( !( m_iWeaponState & WPNSTATE_M4A1_SILENCED ) )
	{
		if ( !FBitSet( m_pPlayer->pev->flags, FL_ONGROUND ) )
			M4A1Fire( 0.4 * m_flAccuracy + 0.035, 0.0875f, FALSE );
		else if ( m_pPlayer->pev->velocity.Length2D() > 140 )
			M4A1Fire( 0.07 * m_flAccuracy + 0.035, 0.0875f, FALSE );
		else
			M4A1Fire( 0.02 * m_flAccuracy, 0.0875f, FALSE );
	}
	else
	{
		if ( !FBitSet( m_pPlayer->pev->flags, FL_ONGROUND ) )
			M4A1Fire( 0.4 * m_flAccuracy + 0.035, 0.0875f, FALSE );
		else if ( m_pPlayer->pev->velocity.Length2D() > 140 )
			M4A1Fire( 0.07 * m_flAccuracy + 0.035, 0.0875f, FALSE );
		else
			M4A1Fire( 0.025 * m_flAccuracy, 0.0875f, FALSE );
	}
}

void CM4A1::M4A1Fire( float flSpread, float flCycleTime, BOOL fUseAutoAim )
{
	m_bDelayFire = true;

	m_iShotsFired++;
	m_flAccuracy = ( m_iShotsFired * m_iShotsFired * m_iShotsFired / 220 ) + 0.3;
	if ( m_flAccuracy > 1 )
		m_flAccuracy = 1;

	if ( m_iClip <= 0 )
	{
		if ( m_fFireOnEmpty )
		{
			PlayEmptySound();
			m_flNextPrimaryAttack = GetNextAttackDelay( 0.2f );
		}

#ifndef CLIENT_DLL
		if ( TheBots )
		{
			TheBots->OnEvent( EVENT_WEAPON_FIRED_ON_EMPTY, m_pPlayer, NULL );
		}
#endif

		return;
	}
	m_iClip--;

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

	// non-silenced
	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

	Vector vecAiming;
	vecAiming = gpGlobals->v_forward;
	Vector vecSrc = m_pPlayer->GetGunPosition( );
	Vector vecDir;
	if ( !( m_iWeaponState & WPNSTATE_M4A1_SILENCED ) )
	{
		vecDir = m_pPlayer->FireBullets3( vecSrc, vecAiming, flSpread, 8192, 2, BULLET_PLAYER_556MM, 32, 0.97f, m_pPlayer->pev, false, m_pPlayer->random_seed );
		m_pPlayer->pev->effects |= EF_MUZZLEFLASH;
	}
	else
		vecDir = m_pPlayer->FireBullets3( vecSrc, vecAiming, flSpread, 8192, 2, BULLET_PLAYER_556MM, 33, 0.95f, m_pPlayer->pev, false, m_pPlayer->random_seed );
	m_pPlayer->ammo_556nato--;

	int flag;
#if defined( CLIENT_WEAPONS )
	flag = FEV_NOTHOST;
#else
	flag = 0;
#endif

	PLAYBACK_EVENT_FULL( flag, m_pPlayer->edict(), m_usFireM4A1,
		0, (float *)&g_vecZero, (float *)&g_vecZero,
		vecDir.x,
		vecDir.y,
		(int)( m_pPlayer->pev->punchangle.x * 100 ),
		(int)( m_pPlayer->pev->punchangle.y * 100 ),
		( m_iWeaponState & WPNSTATE_M4A1_SILENCED ) != 0, 0 );

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay( flCycleTime );

	if ( !m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 );

	m_flTimeWeaponIdle = 1.5f;

	if ( m_pPlayer->pev->velocity.Length2D() > 0 )
		KickBack( 1, 0.45f, 0.28f, 0.045f, 3.75f, 3, 7 );
	else if ( !FBitSet( m_pPlayer->pev->flags, FL_ONGROUND ) )
		KickBack( 1.2f, 0.5f, 0.23f, 0.15f, 5.5f, 3.5f, 6 );
	else if ( FBitSet( m_pPlayer->pev->flags, FL_DUCKING ) )
		KickBack( 0.6f, 0.3f, 0.2f, 0.0125f, 3.25f, 2, 7 );
	else
		KickBack( 0.65f, 0.35f, 0.25f, 0.015f, 3.5f, 2.25f, 7 );
}


void CM4A1::Reload( void )
{
	if ( m_pPlayer->ammo_556nato <= 0 )
		return;

	int iResult;

	if ( !( m_iWeaponState & WPNSTATE_M4A1_SILENCED ) )
		iResult = DefaultReload( 30, M4A1_UNSIL_RELOAD, 3.05f );
	else
		iResult = DefaultReload( 30, M4A1_RELOAD, 3.05f );

	if ( iResult )
	{
		m_pPlayer->SetAnimation( PLAYER_RELOAD );

		m_flAccuracy = 0.2f;
		m_iShotsFired = 0;
		m_bDelayFire = false;
	}
}

void CM4A1::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	if ( m_flTimeWeaponIdle > 0 )
		return;

	m_flTimeWeaponIdle = 20;

	if ( !( m_iWeaponState & WPNSTATE_M4A1_SILENCED ) )
		SendWeaponAnim( M4A1_UNSIL_IDLE, UseDecrement() ? 1 : 0 );
	else
		SendWeaponAnim( M4A1_IDLE, UseDecrement() ? 1 : 0 );
}

float CM4A1::GetMaxSpeed( void )
{
	return 230;
}
