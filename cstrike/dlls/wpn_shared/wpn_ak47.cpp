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

enum ak47_e
{
	AK47_IDLE1 = 0,
	AK47_RELOAD,
	AK47_DRAW,
	AK47_SHOOT1,
	AK47_SHOOT2,
	AK47_SHOOT3,
};

LINK_ENTITY_TO_CLASS( weapon_ak47, CAK47 );

void CAK47::Spawn( void )
{
	Precache( );
	m_iId = WEAPON_AK47;
	SET_MODEL( ENT( pev ), "models/w_ak47.mdl" );

	m_iDefaultAmmo = 30;
	m_flAccuracy = 0.2f;
	m_iShotsFired = 0;

	FallInit();// get ready to fall down.
}


void CAK47::Precache( void )
{
	PRECACHE_MODEL( "models/v_ak47.mdl" );
	PRECACHE_MODEL( "models/w_ak47.mdl" );

	PRECACHE_SOUND( "weapons/ak47-1.wav" );
	PRECACHE_SOUND( "weapons/ak47-2.wav" );
	PRECACHE_SOUND( "weapons/ak47_clipout.wav" );
	PRECACHE_SOUND( "weapons/ak47_clipin.wav" );
	PRECACHE_SOUND( "weapons/ak47_boltpull.wav" );

	m_iShell = PRECACHE_MODEL( "models/rshell.mdl" );// brass shell

	m_usFireAK47 = PRECACHE_EVENT( 1, "events/ak47.sc" );
}

int CAK47::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "762Nato";
	p->iMaxAmmo1 = 90;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 30;
	p->iSlot = 0;
	p->iPosition = 1;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_AK47;
	p->iWeight = 25;

	return 1;
}

BOOL CAK47::Deploy( void )
{
	m_flAccuracy = 0.2f;
	m_iShotsFired = 0;
	iShellOn = 1;

	return DefaultDeploy( "models/v_ak47.mdl", "models/p_ak47.mdl", AK47_DRAW, "ak47", UseDecrement() ? 1 : 0 );
}

void CAK47::SecondaryAttack( void )
{
}

void CAK47::PrimaryAttack( void )
{
	if ( !FBitSet( m_pPlayer->pev->flags, FL_ONGROUND ) )
		AK47Fire( 0.4 * m_flAccuracy + 0.04, 0.0955f, FALSE );
	else if ( m_pPlayer->pev->velocity.Length2D() > 140 )
		AK47Fire( 0.07 * m_flAccuracy + 0.04, 0.0955f, FALSE );
	else
		AK47Fire( 0.0275 * m_flAccuracy, 0.0955f, FALSE );
}

void CAK47::AK47Fire( float flSpread, float flCycleTime, BOOL fUseAutoAim )
{
	m_bDelayFire = true;

	m_iShotsFired++;

	m_flAccuracy = ( m_iShotsFired * m_iShotsFired * m_iShotsFired / 200 ) + 0.35;
	if ( m_flAccuracy > 1.25 )
		m_flAccuracy = 1.25f;

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
	Vector vecAiming;
	vecAiming = gpGlobals->v_forward;
	Vector vecSrc = m_pPlayer->GetGunPosition( );

	Vector vecDir;
	vecDir = m_pPlayer->FireBullets3( vecSrc, vecAiming, flSpread, 8192, 2, BULLET_PLAYER_762MM, 36, 0.98f, m_pPlayer->pev, false, m_pPlayer->random_seed );

	int flag;
#if defined( CLIENT_WEAPONS )
	flag = FEV_NOTHOST;
#else
	flag = 0;
#endif

	PLAYBACK_EVENT_FULL( flag, m_pPlayer->edict(), m_usFireAK47,
		0, (float *)&g_vecZero, (float *)&g_vecZero,
		vecDir.x,
		vecDir.y,
		(int)( m_pPlayer->pev->punchangle.x * 100 ), (int)( m_pPlayer->pev->punchangle.y * 100 ),
		0, 0 );

	// non-silenced
	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay( flCycleTime );

	if ( !m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 );

	m_flTimeWeaponIdle = 1.9f;

	if ( m_pPlayer->pev->velocity.Length2D() > 0 )
		KickBack( 1.5f, 0.45f, 0.225f, 0.05f, 6.5f, 2.5f, 7 );
	else if ( !FBitSet( m_pPlayer->pev->flags, FL_ONGROUND ) )
		KickBack( 2, 1, 0.5f, 0.35f, 9, 6, 5 );
	else if ( FBitSet( m_pPlayer->pev->flags, FL_DUCKING ) )
		KickBack( 0.9f, 0.35f, 0.15f, 0.025f, 5.5f, 1.5f, 9 );
	else
		KickBack( 1, 0.375f, 0.175f, 0.0375f, 5.75f, 1.75f, 8 );
}


void CAK47::Reload( void )
{
	if ( m_pPlayer->ammo_762nato <= 0 )
		return;

	if ( DefaultReload( 30, AK47_RELOAD, 2.45f ) )
	{
		m_pPlayer->SetAnimation( PLAYER_RELOAD );

		m_flAccuracy = 0.2f;
		m_iShotsFired = 0;
		m_bDelayFire = false;
	}
}

void CAK47::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	if ( m_flTimeWeaponIdle > 0 )
		return;

	m_flTimeWeaponIdle = 20;

	SendWeaponAnim( AK47_IDLE1, UseDecrement() ? 1 : 0 );
}
