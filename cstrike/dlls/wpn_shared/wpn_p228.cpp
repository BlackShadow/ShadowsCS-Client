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

enum p228_e
{
	P228_IDLE = 0,
	P228_SHOOT1,
	P228_SHOOT2,
	P228_SHOOT3,
	P228_SHOOT_EMPTY,
	P228_RELOAD,
	P228_DRAW,
};

enum p228_shield_e
{
	P228_SHIELD_IDLE = 0,
	P228_SHIELD_SHOOT1,
	P228_SHIELD_SHOOT2,
	P228_SHIELD_SHOOT_EMPTY,
	P228_SHIELD_RELOAD,
	P228_SHIELD_DRAW,
	P228_SHIELD_IDLE_UP,
	P228_SHIELD_UP,
	P228_SHIELD_DOWN,
};

LINK_ENTITY_TO_CLASS( weapon_p228, CP228 );

void CP228::Spawn( void )
{
	Precache( );
	m_iId = WEAPON_P228;
	SET_MODEL( ENT( pev ), "models/w_p228.mdl" );

	m_iDefaultAmmo = 13;
	m_flAccuracy = 0.9f;

	ClearBits( m_iWeaponState, WPNSTATE_SHIELD_DRAWN );

	FallInit();// get ready to fall down.
}


void CP228::Precache( void )
{
	PRECACHE_MODEL( "models/v_p228.mdl" );
	PRECACHE_MODEL( "models/w_p228.mdl" );
	PRECACHE_MODEL( "models/shield/v_shield_p228.mdl" );

	PRECACHE_SOUND( "weapons/p228-1.wav" );
	PRECACHE_SOUND( "weapons/p228_clipout.wav" );
	PRECACHE_SOUND( "weapons/p228_clipin.wav" );
	PRECACHE_SOUND( "weapons/p228_sliderelease.wav" );
	PRECACHE_SOUND( "weapons/p228_slidepull.wav" );

	m_iShell = PRECACHE_MODEL( "models/pshell.mdl" );// brass shell

	m_usFireP228 = PRECACHE_EVENT( 1, "events/p228.sc" );
}

int CP228::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "357SIG";
	p->iMaxAmmo1 = 52;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 13;
	p->iSlot = 1;
	p->iPosition = 3;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_P228;
	p->iWeight = 5;

	return 1;
}

BOOL CP228::Deploy( void )
{
	m_flAccuracy = 0.9f;
	m_fMaxSpeed = 250;

	m_iWeaponState &= ~WPNSTATE_SHIELD_DRAWN;

	m_pPlayer->m_bShieldDrawn = false;

	if ( m_pPlayer->HasShield() )
		return DefaultDeploy( "models/shield/v_shield_p228.mdl", "models/shield/p_shield_p228.mdl", P228_SHIELD_DRAW, "shieldgun", UseDecrement() ? 1 : 0 );
	else
		return DefaultDeploy( "models/v_p228.mdl", "models/p_p228.mdl", P228_DRAW, "onehanded", UseDecrement() ? 1 : 0 );
}

void CP228::PrimaryAttack( void )
{
	if ( !FBitSet( m_pPlayer->pev->flags, FL_ONGROUND ) )
		P228Fire( (float)( 1.5 * ( 1 - m_flAccuracy ) ), 0.2f, FALSE );
	else if ( m_pPlayer->pev->velocity.Length2D() > 0 )
		P228Fire( (float)( 0.255 * ( 1 - m_flAccuracy ) ), 0.2f, FALSE );
	else if ( FBitSet( m_pPlayer->pev->flags, FL_DUCKING ) )
		P228Fire( (float)( 0.075 * ( 1 - m_flAccuracy ) ), 0.2f, FALSE );
	else
		P228Fire( (float)( 0.15 * ( 1 - m_flAccuracy ) ), 0.2f, FALSE );
}

void CP228::SecondaryAttack( void )
{
	ShieldSecondaryFire( P228_SHIELD_UP, P228_SHIELD_DOWN );
}

void CP228::P228Fire( float flSpread, float flCycleTime, BOOL fUseSemi )
{
	flCycleTime -= 0.05;
	m_iShotsFired++;

	if ( m_iShotsFired > 1 )
		return;

	// Mark the time of this shot and determine the accuracy modifier based on the last shot fired...
	if ( m_flLastFire == 0 )
		m_flLastFire = gpGlobals->time;
	else
	{
		m_flAccuracy -= ( 0.3 ) * ( 0.325 - ( gpGlobals->time - m_flLastFire ) );

		if ( m_flAccuracy > 0.9 )
			m_flAccuracy = 0.9f;
		else if ( m_flAccuracy < 0.6 )
			m_flAccuracy = 0.6f;

		m_flLastFire = gpGlobals->time;
	}

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
			TheBots->OnEvent( EVENT_WEAPON_FIRED_ON_EMPTY, m_pPlayer );
		}
#endif

		return;
	}

	m_iClip--;
	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;

	SetPlayerShieldAnim();

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

	// Special f/x
	m_pPlayer->m_iWeaponVolume = BIG_EXPLOSION_VOLUME;
	m_pPlayer->m_iWeaponFlash = DIM_GUN_FLASH;

	// Aiming
	Vector vecAiming;
	vecAiming = gpGlobals->v_forward;
	Vector vecSrc = m_pPlayer->GetGunPosition( );
	Vector vecDir;
	vecDir = m_pPlayer->FireBullets3( vecSrc, vecAiming, flSpread, 4096, 1, BULLET_PLAYER_357SIG, 32, 0.8f, m_pPlayer->pev, true, m_pPlayer->random_seed );

	int flag;
#if defined( CLIENT_WEAPONS )
	flag = FEV_NOTHOST;
#else
	flag = 0;
#endif

	PLAYBACK_EVENT_FULL( flag, m_pPlayer->edict(), m_usFireP228,
		0, (float *)&g_vecZero, (float *)&g_vecZero,
		vecDir.x,
		vecDir.y,
		(int)( m_pPlayer->pev->punchangle.x * 100 ), (int)( m_pPlayer->pev->punchangle.y * 100 ),
		m_iClip ? 0 : 1, 0 );

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay( flCycleTime );

	if ( !m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 );

	m_flTimeWeaponIdle = 2;

	ResetPlayerShieldAnim();

#ifndef CLIENT_DLL
	m_pPlayer->pev->punchangle.x -= 2;
#endif
}


void CP228::Reload( void )
{
	if ( m_pPlayer->ammo_357sig <= 0 )
		return;

	int iResult;

	if ( m_pPlayer->HasShield() )
		iResult = DefaultReload( 13, P228_SHIELD_RELOAD, 2.7f );
	else
		iResult = DefaultReload( 13, P228_RELOAD, 2.7f );

	if ( iResult )
	{
		m_pPlayer->SetAnimation( PLAYER_RELOAD );

		m_flAccuracy = 0.9f;
	}
}

void CP228::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	if ( m_flTimeWeaponIdle > 0 )
		return;

	if ( m_pPlayer->HasShield() )
	{
		m_flTimeWeaponIdle = 20;

		if ( FBitSet( m_iWeaponState, WPNSTATE_SHIELD_DRAWN ) )
			SendWeaponAnim( P228_SHIELD_IDLE_UP, UseDecrement() ? 1 : 0 );
	}
	else
	{
		// only idle if the slid isn't back
		if ( m_iClip != 0 )
		{
			m_flTimeWeaponIdle = 49.0 / 16;
			SendWeaponAnim( P228_IDLE, UseDecrement() ? 1 : 0 );
		}
	}
}
