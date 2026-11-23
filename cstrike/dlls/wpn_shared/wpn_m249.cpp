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

enum m249_e
{
	M249_IDLE1 = 0,
	M249_SHOOT1,
	M249_SHOOT2,
	M249_RELOAD,
	M249_DRAW,
};

LINK_ENTITY_TO_CLASS( weapon_m249, CM249 );

void CM249::Spawn( void )
{
	Precache( );
	m_iId = WEAPON_M249;
	SET_MODEL( ENT( pev ), "models/w_m249.mdl" );

	m_iDefaultAmmo = 100;
	m_flAccuracy = 0.2f;
	m_iShotsFired = 0;

	FallInit();// get ready to fall down.
}


void CM249::Precache( void )
{
	PRECACHE_MODEL( "models/v_m249.mdl" );
	PRECACHE_MODEL( "models/w_m249.mdl" );

	PRECACHE_SOUND( "weapons/m249-1.wav" );
	PRECACHE_SOUND( "weapons/m249-2.wav" );
	PRECACHE_SOUND( "weapons/m249_boxout.wav" );
	PRECACHE_SOUND( "weapons/m249_boxin.wav" );
	PRECACHE_SOUND( "weapons/m249_chain.wav" );
	PRECACHE_SOUND( "weapons/m249_coverup.wav" );
	PRECACHE_SOUND( "weapons/m249_coverdown.wav" );

	m_iShell = PRECACHE_MODEL( "models/rshell.mdl" );// brass shell

	m_usFireM249 = PRECACHE_EVENT( 1, "events/m249.sc" );
}

int CM249::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "556NatoBox";
	p->iMaxAmmo1 = 200;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 100;
	p->iSlot = 0;
	p->iPosition = 4;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_M249;
	p->iWeight = 25;

	return 1;
}

BOOL CM249::Deploy( void )
{
	m_flAccuracy = 0.2f;
	m_iShotsFired = 0;
	iShellOn = 1;

	return DefaultDeploy( "models/v_m249.mdl", "models/p_m249.mdl", M249_DRAW, "m249", UseDecrement() ? 1 : 0 );
}

void CM249::PrimaryAttack( void )
{
	if ( !FBitSet( m_pPlayer->pev->flags, FL_ONGROUND ) )
		M249Fire( 0.5 * m_flAccuracy + 0.045, 0.1f, FALSE );
	else if ( m_pPlayer->pev->velocity.Length2D() > 140 )
		M249Fire( 0.095 * m_flAccuracy + 0.045, 0.1f, FALSE );
	else
		M249Fire( 0.03 * m_flAccuracy, 0.1f, FALSE );
}

void CM249::M249Fire( float flSpread, float flCycleTime, BOOL fUseAutoAim )
{
	m_bDelayFire = true;

	m_iShotsFired++;

	m_flAccuracy = ( m_iShotsFired * m_iShotsFired * m_iShotsFired / 175 ) + 0.4;
	if ( m_flAccuracy > 0.9 )
		m_flAccuracy = 0.9f;

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
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

	Vector vecAiming;
	vecAiming = gpGlobals->v_forward;
	Vector vecSrc = m_pPlayer->GetGunPosition( );
	Vector vecDir;
	vecDir = m_pPlayer->FireBullets3( vecSrc, vecAiming, flSpread, 8192, 2, BULLET_PLAYER_556MM, 32, 0.97f, m_pPlayer->pev, false, m_pPlayer->random_seed );

	int flag;
#if defined( CLIENT_WEAPONS )
	flag = FEV_NOTHOST;
#else
	flag = 0;
#endif

	PLAYBACK_EVENT_FULL( flag, m_pPlayer->edict(), m_usFireM249,
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

	m_flTimeWeaponIdle = 1.6f;

	if ( !FBitSet( m_pPlayer->pev->flags, FL_ONGROUND ) )
		KickBack( 1.8f, 0.65f, 0.45f, 0.125f, 5, 3.5f, 8 );
	else if ( m_pPlayer->pev->velocity.Length2D() > 0 )
		KickBack( 1.1f, 0.5f, 0.3f, 0.06f, 4, 3, 8 );
	else if ( FBitSet( m_pPlayer->pev->flags, FL_DUCKING ) )
		KickBack( 0.75f, 0.325f, 0.25f, 0.025f, 3.5f, 2.5f, 9 );
	else
		KickBack( 0.8f, 0.35f, 0.3f, 0.03f, 3.75f, 3, 9 );
}


void CM249::Reload( void )
{
	if ( m_pPlayer->ammo_556natobox <= 0 )
		return;

	int iResult;

	iResult = DefaultReload( 100, M249_RELOAD, 4.7f );

	if ( iResult )
	{
		m_pPlayer->SetAnimation( PLAYER_RELOAD );

		m_flAccuracy = 0.2f;
		m_bDelayFire = false;
		m_iShotsFired = 0;
	}
}

void CM249::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	if ( m_flTimeWeaponIdle > 0 )
		return;

	m_flTimeWeaponIdle = 20;

	SendWeaponAnim( M249_IDLE1, UseDecrement() ? 1 : 0 );
}
