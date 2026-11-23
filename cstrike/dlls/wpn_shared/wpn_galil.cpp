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

enum galil_e
{
	GALIL_IDLE1 = 0,
	GALIL_RELOAD,
	GALIL_DRAW,
	GALIL_SHOOT1,
	GALIL_SHOOT2,
	GALIL_SHOOT3,
};

LINK_ENTITY_TO_CLASS( weapon_galil, CGalil );

void CGalil::Spawn( void )
{
	Precache( );
	m_iId = WEAPON_GALIL;
	SET_MODEL( ENT( pev ), "models/w_galil.mdl" );

	m_iDefaultAmmo = 35;

	FallInit();// get ready to fall down.
}

void CGalil::Precache( void )
{
	PRECACHE_MODEL( "models/v_galil.mdl" );
	PRECACHE_MODEL( "models/w_galil.mdl" );

	PRECACHE_SOUND( "weapons/galil-1.wav" );
	PRECACHE_SOUND( "weapons/galil-2.wav" );
	PRECACHE_SOUND( "weapons/galil_clipout.wav" );
	PRECACHE_SOUND( "weapons/galil_clipin.wav" );
	PRECACHE_SOUND( "weapons/galil_boltpull.wav" );

	m_iShell = PRECACHE_MODEL( "models/rshell.mdl" );// brass shell

	m_usFireGalil = PRECACHE_EVENT( 1, "events/galil.sc" );
}

int CGalil::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "556Nato";
	p->iMaxAmmo1 = 90;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 35;
	p->iSlot = 0;
	p->iPosition = 17;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_GALIL;
	p->iWeight = 25;

	return 1;
}

BOOL CGalil::Deploy( void )
{
	m_flAccuracy = 0.2f;
	m_iShotsFired = 0;
	iShellOn = 1;

	return DefaultDeploy( "models/v_galil.mdl", "models/p_galil.mdl", GALIL_DRAW, "ak47", UseDecrement() ? 1 : 0 );
}

void CGalil::SecondaryAttack( void )
{
}

void CGalil::PrimaryAttack( void )
{
	// don't fire underwater
	if ( m_pPlayer->pev->waterlevel == 3 )
	{
		PlayEmptySound( );
		m_flNextPrimaryAttack = GetNextAttackDelay( 0.15f );
		return;
	}

	if ( !FBitSet( m_pPlayer->pev->flags, FL_ONGROUND ) )
		GalilFire( 0.3 * m_flAccuracy + 0.04, 0.0875f, FALSE );
	else if ( m_pPlayer->pev->velocity.Length2D() > 140 )
		GalilFire( 0.07 * m_flAccuracy + 0.04, 0.0875f, FALSE );
	else
		GalilFire( 0.0375 * m_flAccuracy, 0.0875f, FALSE );
}

void CGalil::GalilFire( float flSpread, float flCycleTime, BOOL fUseAutoAim )
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
	vecDir = m_pPlayer->FireBullets3( vecSrc, vecAiming, flSpread, 8192, 2, BULLET_PLAYER_556MM, 30, 0.98f, m_pPlayer->pev, false, m_pPlayer->random_seed );

	int flag;
#if defined( CLIENT_WEAPONS )
	flag = FEV_NOTHOST;
#else
	flag = 0;
#endif

	PLAYBACK_EVENT_FULL( flag, m_pPlayer->edict(), m_usFireGalil,
		0, (float *)&g_vecZero, (float *)&g_vecZero,
		vecDir.x,
		vecDir.y,
		(int)( m_pPlayer->pev->punchangle.x * 10000000 ), (int)( m_pPlayer->pev->punchangle.y * 10000000 ),
		0, 0 );

	// non-silenced
	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay( flCycleTime );

	if ( !m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 );

	m_flTimeWeaponIdle = 1.28f;

	if ( m_pPlayer->pev->velocity.Length2D() > 0 )
		KickBack( 1, 0.45f, 0.28f, 0.045f, 3.75f, 3, 7 );
	else if ( !FBitSet( m_pPlayer->pev->flags, FL_ONGROUND ) )
		KickBack( 1.2f, 0.5f, 0.23f, 0.15f, 5.5f, 3.5f, 6 );
	else if ( FBitSet( m_pPlayer->pev->flags, FL_DUCKING ) )
		KickBack( 0.6f, 0.3f, 0.2f, 0.0125f, 3.25f, 2, 7 );
	else
		KickBack( 0.65f, 0.35f, 0.25f, 0.015f, 3.5f, 2.25f, 7 );
}


void CGalil::Reload( void )
{
	if ( m_pPlayer->ammo_556nato <= 0 )
		return;

	int iResult;

	iResult = DefaultReload( 35, GALIL_RELOAD, 2.45f );

	if ( iResult )
	{
		m_pPlayer->SetAnimation( PLAYER_RELOAD );

		m_flAccuracy = 0.2f;
		m_iShotsFired = 0;
		m_bDelayFire = false;
	}
}

void CGalil::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	if ( m_flTimeWeaponIdle > 0 )
		return;

	m_flTimeWeaponIdle = 20;

	SendWeaponAnim( GALIL_IDLE1, UseDecrement() ? 1 : 0 );
}
