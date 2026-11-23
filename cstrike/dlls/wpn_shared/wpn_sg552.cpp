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

enum sg552_e
{
	SG552_IDLE1 = 0,
	SG552_RELOAD,
	SG552_DRAW,
	SG552_SHOOT1,
	SG552_SHOOT2,
	SG552_SHOOT3,
};

LINK_ENTITY_TO_CLASS( weapon_sg552, CSG552 );

void CSG552::Spawn( void )
{
	Precache( );
	m_iId = WEAPON_SG552;
	SET_MODEL( ENT( pev ), "models/w_sg552.mdl" );

	m_iDefaultAmmo = 30;
	m_flAccuracy = 0.2f;
	m_iShotsFired = 0;

	FallInit();// get ready to fall down.
}


void CSG552::Precache( void )
{
	PRECACHE_MODEL( "models/v_sg552.mdl" );
	PRECACHE_MODEL( "models/w_sg552.mdl" );

	PRECACHE_SOUND( "weapons/sg552-1.wav" );
	PRECACHE_SOUND( "weapons/sg552-2.wav" );
	PRECACHE_SOUND( "weapons/sg552_clipout.wav" );
	PRECACHE_SOUND( "weapons/sg552_clipin.wav" );
	PRECACHE_SOUND( "weapons/sg552_boltpull.wav" );

	m_iShell = PRECACHE_MODEL( "models/rshell.mdl" );// brass shell

	m_usFireSG552 = PRECACHE_EVENT( 1, "events/sg552.sc" );
}

int CSG552::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "556Nato";
	p->iMaxAmmo1 = 90;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 30;
	p->iSlot = 0;
	p->iPosition = 10;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_SG552;
	p->iWeight = 25;

	return 1;
}

BOOL CSG552::Deploy( void )
{
	m_flAccuracy = 0.2f;
	m_iShotsFired = 0;
	iShellOn = 1;

	return DefaultDeploy( "models/v_sg552.mdl", "models/p_sg552.mdl", SG552_DRAW, "mp5", UseDecrement() ? 1 : 0 );
}

void CSG552::SecondaryAttack( void )
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

void CSG552::PrimaryAttack( void )
{
	if ( !FBitSet( m_pPlayer->pev->flags, FL_ONGROUND ) )
		SG552Fire( 0.45 * m_flAccuracy + 0.035, 0.0825f, FALSE );
	else if ( m_pPlayer->pev->velocity.Length2D() > 140 )
		SG552Fire( 0.075 * m_flAccuracy + 0.035, 0.0825f, FALSE );
	else if ( m_pPlayer->pev->fov == 90 )
		SG552Fire( 0.02 * m_flAccuracy, 0.0825f, FALSE );
	else
		SG552Fire( 0.02 * m_flAccuracy, 0.135f, FALSE );
}


void CSG552::SG552Fire( float flSpread, float flCycleTime, BOOL fUseAutoAim )
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

	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	// non-silenced
	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

	int flag;
#if defined( CLIENT_WEAPONS )
	flag = FEV_NOTHOST;
#else
	flag = 0;
#endif

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	Vector vecAiming;
	vecAiming = gpGlobals->v_forward;
	Vector vecSrc = m_pPlayer->GetGunPosition( );

	Vector vecDir;
	vecDir = m_pPlayer->FireBullets3( vecSrc, vecAiming, flSpread, 8192, 2, BULLET_PLAYER_556MM, 33, 0.955f, m_pPlayer->pev, false, m_pPlayer->random_seed );

	PLAYBACK_EVENT_FULL( flag, m_pPlayer->edict(), m_usFireSG552,
		0, (float *)&g_vecZero, (float *)&g_vecZero,
		vecDir.x,
		vecDir.y,
		(int)( m_pPlayer->pev->punchangle.x * 100 ), (int)( m_pPlayer->pev->punchangle.y * 100 ),
		5, 0 );

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay( flCycleTime );

	if ( !m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 );

	m_flTimeWeaponIdle = 2;

	if ( m_pPlayer->pev->velocity.Length2D() > 0 )
		KickBack( 1, 0.45f, 0.28f, 0.04f, 4.25f, 2.5f, 7 );
	else if ( !FBitSet( m_pPlayer->pev->flags, FL_ONGROUND ) )
		KickBack( 1.25f, 0.45f, 0.22f, 0.18f, 6, 4, 5 );
	else if ( FBitSet( m_pPlayer->pev->flags, FL_DUCKING ) )
		KickBack( 0.6f, 0.35f, 0.2f, 0.0125f, 3.7f, 2, 10 );
	else
		KickBack( 0.625f, 0.375f, 0.25f, 0.0125f, 4, 2.25f, 9 );
}


void CSG552::Reload( void )
{
	if ( m_pPlayer->ammo_556nato <= 0 )
		return;

	int iResult = DefaultReload( 30, SG552_RELOAD, 3 );

	if ( iResult )
	{
		if ( m_pPlayer->pev->fov != 90 )
			SecondaryAttack();
		m_pPlayer->SetAnimation( PLAYER_RELOAD );

		m_flAccuracy = 0.2f;
		m_iShotsFired = 0;
		m_bDelayFire = false;
	}
}

void CSG552::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	if ( m_flTimeWeaponIdle > 0 )
		return;

	m_flTimeWeaponIdle = 20;

	SendWeaponAnim( SG552_IDLE1, UseDecrement() ? 1 : 0 );
}


float CSG552::GetMaxSpeed( void )
{
	if ( m_pPlayer->m_iFOV == 90 )
		return 235;
	else
		return 200; // zoomed in.
}
