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

enum p90_e
{
	P90_IDLE1 = 0,
	P90_RELOAD,
	P90_DRAW,
	P90_SHOOT1,
	P90_SHOOT2,
	P90_SHOOT3,
};

LINK_ENTITY_TO_CLASS( weapon_p90, CP90 );

void CP90::Spawn( void )
{
	Precache( );
	m_iId = WEAPON_P90;
	SET_MODEL( ENT( pev ), "models/w_p90.mdl" );

	m_iDefaultAmmo = 50;
	m_flAccuracy = 0.2f;
	m_iShotsFired = 0;
	m_bDelayFire = false;

	FallInit();// get ready to fall down.
}


void CP90::Precache( void )
{
	PRECACHE_MODEL( "models/v_p90.mdl" );
	PRECACHE_MODEL( "models/w_p90.mdl" );

	PRECACHE_SOUND( "weapons/p90-1.wav" );
	PRECACHE_SOUND( "weapons/p90_clipout.wav" );
	PRECACHE_SOUND( "weapons/p90_clipin.wav" );
	PRECACHE_SOUND( "weapons/p90_boltpull.wav" );
	PRECACHE_SOUND( "weapons/p90_cliprelease.wav" );

	m_iShell = PRECACHE_MODEL( "models/rshell.mdl" );// brass shell

	m_usFireP90 = PRECACHE_EVENT( 1, "events/p90.sc" );
}

int CP90::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "57mm";
	p->iMaxAmmo1 = 100;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 50;
	p->iSlot = 0;
	p->iPosition = 8;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_P90;
	p->iWeight = 26;

	return 1;
}

BOOL CP90::Deploy( void )
{
	m_flAccuracy = 0.2f;
	m_iShotsFired = 0;
	iShellOn = 1;
	m_bDelayFire = false;

	return DefaultDeploy( "models/v_p90.mdl", "models/p_p90.mdl", P90_DRAW, "carbine", UseDecrement() ? 1 : 0 );
}

void CP90::PrimaryAttack( void )
{
	if ( !FBitSet( m_pPlayer->pev->flags, FL_ONGROUND ) )
		P90Fire( 0.3 * m_flAccuracy, 0.066f, FALSE );
	else if ( m_pPlayer->pev->velocity.Length2D() > 170 )
		P90Fire( 0.115 * m_flAccuracy, 0.066f, FALSE );
	else
		P90Fire( 0.045 * m_flAccuracy, 0.066f, FALSE );
}

void CP90::P90Fire( float flSpread, float flCycleTime, BOOL fUseAutoAim )
{
	m_bDelayFire = true;

	m_iShotsFired++;
	m_flAccuracy = ( m_iShotsFired * m_iShotsFired / 175 ) + 0.45;
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

	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

	// non-silenced
	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = DIM_GUN_FLASH;

	Vector vecAiming;
	vecAiming = gpGlobals->v_forward;
	Vector vecSrc = m_pPlayer->GetGunPosition( );
	Vector vecDir;
	vecDir = m_pPlayer->FireBullets3( vecSrc, vecAiming, flSpread, 8192, 1, BULLET_PLAYER_57MM, 21, 0.885f, m_pPlayer->pev, false, m_pPlayer->random_seed );

	int flag;
#if defined( CLIENT_WEAPONS )
	flag = FEV_NOTHOST;
#else
	flag = 0;
#endif
	PLAYBACK_EVENT_FULL( flag, m_pPlayer->edict(), m_usFireP90,
		0, (float *)&g_vecZero, (float *)&g_vecZero,
		vecDir.x,
		vecDir.y,
		(int)( m_pPlayer->pev->punchangle.x * 100 ),
		(int)( m_pPlayer->pev->punchangle.y * 100 ),
		5, 0 );

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay( flCycleTime );

	if ( !m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 );

	m_flTimeWeaponIdle = 2;

	// Kick the gun based on the state of the player.
	if ( !FBitSet( m_pPlayer->pev->flags, FL_ONGROUND ) )
		KickBack( 0.9f, 0.45f, 0.35f, 0.04f, 5.25f, 3.5f, 4 );
	else if ( m_pPlayer->pev->velocity.Length2D() > 0 )
		KickBack( 0.45f, 0.3f, 0.2f, 0.0275f, 4, 2.25f, 7 );
	else if ( FBitSet( m_pPlayer->pev->flags, FL_DUCKING ) )
		KickBack( 0.275f, 0.2f, 0.125f, 0.02f, 3, 1, 9 );
	else
		KickBack( 0.3f, 0.225f, 0.125f, 0.02f, 3.25f, 1.25f, 8 );
}


void CP90::Reload( void )
{
	if ( m_pPlayer->ammo_57mm <= 0 )
		return;

	if ( DefaultReload( 50, P90_RELOAD, 3.4f ) )
	{
		m_pPlayer->SetAnimation( PLAYER_RELOAD );

		m_flAccuracy = 0.2f;
		m_iShotsFired = 0;
	}
}

void CP90::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	if ( m_flTimeWeaponIdle > 0 )
		return;

	m_flTimeWeaponIdle = 20;

	SendWeaponAnim( P90_IDLE1, UseDecrement() ? 1 : 0 );
}
