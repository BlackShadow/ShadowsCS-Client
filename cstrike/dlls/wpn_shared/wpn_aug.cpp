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

enum aug_e
{
	AUG_IDLE1 = 0,
	AUG_RELOAD,
	AUG_DRAW,
	AUG_SHOOT1,
	AUG_SHOOT2,
	AUG_SHOOT3,
};

LINK_ENTITY_TO_CLASS( weapon_aug, CAUG );

void CAUG::Spawn( void )
{
	Precache( );
	m_iId = WEAPON_AUG;
	SET_MODEL( ENT( pev ), "models/w_aug.mdl" );

	m_iDefaultAmmo = 30;
	m_flAccuracy = 0.2f;
	m_iShotsFired = 0;

	FallInit();// get ready to fall down.
}


void CAUG::Precache( void )
{
	PRECACHE_MODEL( "models/v_aug.mdl" );
	PRECACHE_MODEL( "models/w_aug.mdl" );

	PRECACHE_SOUND( "weapons/aug-1.wav" );
	PRECACHE_SOUND( "weapons/aug_clipout.wav" );
	PRECACHE_SOUND( "weapons/aug_clipin.wav" );
	PRECACHE_SOUND( "weapons/aug_boltpull.wav" );
	PRECACHE_SOUND( "weapons/aug_boltslap.wav" );
	PRECACHE_SOUND( "weapons/aug_forearm.wav" );

	m_iShell = PRECACHE_MODEL( "models/rshell.mdl" );// brass shell

	m_usFireAug = PRECACHE_EVENT( 1, "events/aug.sc" );
}

int CAUG::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "556Nato";
	p->iMaxAmmo1 = 90;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 30;
	p->iSlot = 0;
	p->iPosition = 14;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_AUG;
	p->iWeight = 25;

	return 1;
}

BOOL CAUG::Deploy( void )
{
	m_flAccuracy = 0.2f;
	m_iShotsFired = 0;
	iShellOn = 1;

	return DefaultDeploy( "models/v_aug.mdl", "models/p_aug.mdl", AUG_DRAW, "carbine", UseDecrement() ? 1 : 0 );
}

void CAUG::SecondaryAttack( void )
{
	if ( m_pPlayer->m_iFOV == 90 )
	{
		m_pPlayer->m_iFOV = 55;
		m_pPlayer->pev->fov = 55;
	}
	else if ( m_pPlayer->m_iFOV == 55 )
	{
		m_pPlayer->m_iFOV = 90;
		m_pPlayer->pev->fov = 90;
	}
	else
	{
		m_pPlayer->m_iFOV = 90;
		m_pPlayer->pev->fov = 90;
	}

	m_flNextSecondaryAttack = 0.3f;
}

void CAUG::PrimaryAttack( void )
{
	if ( !FBitSet( m_pPlayer->pev->flags, FL_ONGROUND ) )
		AUGFire( 0.4 * m_flAccuracy + 0.035, 0.0825f, FALSE );
	else if ( m_pPlayer->pev->velocity.Length2D() > 140 )
		AUGFire( 0.07 * m_flAccuracy + 0.035, 0.0825f, FALSE );
	else if ( m_pPlayer->pev->fov == 90 )
		AUGFire( 0.02 * m_flAccuracy, 0.0825f, FALSE );
	else
		AUGFire( 0.02 * m_flAccuracy, 0.135f, FALSE );
}


void CAUG::AUGFire( float flSpread, float flCycleTime, BOOL fUseAutoAim )
{
	m_bDelayFire = true;

	m_iShotsFired++;
	m_flAccuracy = ( m_iShotsFired * m_iShotsFired * m_iShotsFired / 215 ) + 0.3;
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

	// non-silenced
	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	Vector vecAiming;
	vecAiming = gpGlobals->v_forward;
	Vector vecSrc = m_pPlayer->GetGunPosition( );

	Vector vecDir;
	vecDir = m_pPlayer->FireBullets3( vecSrc, vecAiming, flSpread, 8192, 2, BULLET_PLAYER_556MM, 32, 0.96f, m_pPlayer->pev, false, m_pPlayer->random_seed );

	int flag;
#if defined( CLIENT_WEAPONS )
	flag = FEV_NOTHOST;
#else
	flag = 0;
#endif

	PLAYBACK_EVENT_FULL( flag, m_pPlayer->edict(), m_usFireAug,
		0, (float *)&g_vecZero, (float *)&g_vecZero,
		vecDir.x,
		vecDir.y,
		(int)( m_pPlayer->pev->punchangle.x * 100 ),
		(int)( m_pPlayer->pev->punchangle.y * 100 ),
		0, 0 );

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay( flCycleTime );

	if ( !m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 );

	m_flTimeWeaponIdle = 1.9f;

	if ( m_pPlayer->pev->velocity.Length2D() > 0 )
		KickBack( 1, 0.45f, 0.275f, 0.05f, 4, 2.5f, 7 );
	else if ( !FBitSet( m_pPlayer->pev->flags, FL_ONGROUND ) )
		KickBack( 1.25f, 0.45f, 0.22f, 0.18f, 5.5f, 4, 5 );
	else if ( FBitSet( m_pPlayer->pev->flags, FL_DUCKING ) )
		KickBack( 0.575f, 0.325f, 0.2f, 0.011f, 3.25f, 2, 8 );
	else
		KickBack( 0.625f, 0.375f, 0.25f, 0.0125f, 3.5f, 2.25f, 8 );
}


void CAUG::Reload( void )
{
	if ( m_pPlayer->ammo_556nato <= 0 )
		return;

	int iResult = DefaultReload( 30, AUG_RELOAD, 3.3f );

	if ( iResult )
	{
		m_pPlayer->SetAnimation( PLAYER_RELOAD );

		if ( m_pPlayer->m_iFOV != 90 )
			SecondaryAttack();

		m_flAccuracy = 0;
		m_iShotsFired = 0;
		m_bDelayFire = false;
	}
}

void CAUG::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	if ( m_flTimeWeaponIdle > 0 )
		return;

	m_flTimeWeaponIdle = 20;

	SendWeaponAnim( AUG_IDLE1, UseDecrement() ? 1 : 0 );
}
