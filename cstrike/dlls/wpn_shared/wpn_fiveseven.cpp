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

enum fiveseven_e
{
	FIVESEVEN_IDLE = 0,
	FIVESEVEN_SHOOT1,
	FIVESEVEN_SHOOT2,
	FIVESEVEN_SHOOT_EMPTY,
	FIVESEVEN_RELOAD,
	FIVESEVEN_DRAW,
};

LINK_ENTITY_TO_CLASS( weapon_fiveseven, CFiveSeven );

void CFiveSeven::Spawn( void )
{
	Precache( );
	m_iId = WEAPON_FIVESEVEN;
	SET_MODEL( ENT( pev ), "models/w_fiveseven.mdl" );

	m_iDefaultAmmo = 20;
	m_flAccuracy = 0.92f;

	m_iWeaponState &= ~WPNSTATE_SHIELD_DRAWN;

	FallInit();// get ready to fall down.
}


void CFiveSeven::Precache( void )
{
	PRECACHE_MODEL( "models/v_fiveseven.mdl" );
	PRECACHE_MODEL( "models/w_fiveseven.mdl" );
	PRECACHE_MODEL( "models/shield/v_shield_fiveseven.mdl" );

	PRECACHE_SOUND( "weapons/fiveseven-1.wav" );
	PRECACHE_SOUND( "weapons/fiveseven_clipout.wav" );
	PRECACHE_SOUND( "weapons/fiveseven_clipin.wav" );
	PRECACHE_SOUND( "weapons/fiveseven_sliderelease.wav" );
	PRECACHE_SOUND( "weapons/fiveseven_slidepull.wav" );

	m_iShell = PRECACHE_MODEL( "models/pshell.mdl" );// brass shell

	m_usFireFiveSeven = PRECACHE_EVENT( 1, "events/fiveseven.sc" );
}

int CFiveSeven::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "57mm";
	p->iMaxAmmo1 = 100;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 20;
	p->iSlot = 1;
	p->iPosition = 6;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_FIVESEVEN;
	p->iWeight = 5;

	return 1;
}

BOOL CFiveSeven::Deploy( void )
{
	m_flAccuracy = 0.92f;

	m_fMaxSpeed = 250;

	m_iWeaponState &= ~WPNSTATE_SHIELD_DRAWN;

	m_pPlayer->m_bShieldDrawn = false;

	if ( m_pPlayer->HasShield() )
		return DefaultDeploy( "models/shield/v_shield_fiveseven.mdl", "models/shield/p_shield_fiveseven.mdl", SHIELDGUN_DRAW, "shieldgun", UseDecrement() ? 1 : 0 );
	else
		return DefaultDeploy( "models/v_fiveseven.mdl", "models/p_fiveseven.mdl", FIVESEVEN_DRAW, "onehanded", UseDecrement() ? 1 : 0 );
}

void CFiveSeven::PrimaryAttack( void )
{
	if ( !FBitSet( m_pPlayer->pev->flags, FL_ONGROUND ) )
		FiveSevenFire( (float)( 1.5 * ( 1 - m_flAccuracy ) ), 0.2f, FALSE );
	else if ( m_pPlayer->pev->velocity.Length2D() > 0 )
		FiveSevenFire( (float)( 0.255 * ( 1 - m_flAccuracy ) ), 0.2f, FALSE );
	else if ( FBitSet( m_pPlayer->pev->flags, FL_DUCKING ) )
		FiveSevenFire( (float)( 0.075 * ( 1 - m_flAccuracy ) ), 0.2f, FALSE );
	else
		FiveSevenFire( (float)( 0.15 * ( 1 - m_flAccuracy ) ), 0.2f, FALSE );
}

void CFiveSeven::SecondaryAttack( void )
{
	ShieldSecondaryFire( SHIELDGUN_UP, SHIELDGUN_DOWN );
}

void CFiveSeven::FiveSevenFire( float flSpread, float flCycleTime, BOOL fUseSemi )
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
		m_flAccuracy -= ( 0.25 ) * ( 0.275 - ( gpGlobals->time - m_flLastFire ) );

		if ( m_flAccuracy > 0.92 )
			m_flAccuracy = 0.92f;
		else if ( m_flAccuracy < 0.725 )
			m_flAccuracy = 0.725f;

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
	vecDir = m_pPlayer->FireBullets3( vecSrc, vecAiming, flSpread, 4096, 1, BULLET_PLAYER_57MM, 20, 0.885f, m_pPlayer->pev, false, m_pPlayer->random_seed );

	int flag;
#if defined( CLIENT_WEAPONS )
	flag = FEV_NOTHOST;
#else
	flag = 0;
#endif

	PLAYBACK_EVENT_FULL( flag, m_pPlayer->edict(), m_usFireFiveSeven,
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

	m_pPlayer->pev->punchangle.x -= 2;

	ResetPlayerShieldAnim();
}


void CFiveSeven::Reload( void )
{
	if ( m_pPlayer->ammo_57mm <= 0 )
		return;

	if ( DefaultReload( 20, FIVESEVEN_RELOAD, 2.7f ) )
	{
		m_pPlayer->SetAnimation( PLAYER_RELOAD );

		m_flAccuracy = 0.92f;
	}
}

void CFiveSeven::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	if ( m_flTimeWeaponIdle > 0 )
		return;

	if ( m_pPlayer->HasShield() )
	{
		m_flTimeWeaponIdle = 20;

		if ( FBitSet( m_iWeaponState, WPNSTATE_SHIELD_DRAWN ) )
			SendWeaponAnim( SHIELDGUN_DRAWN_IDLE, UseDecrement() ? 1 : 0 );
	}
	else
	{
		// only idle if the slid isn't back
		if ( m_iClip != 0 )
		{
			m_flTimeWeaponIdle = 49.0 / 16;
			SendWeaponAnim( FIVESEVEN_IDLE, UseDecrement() ? 1 : 0 );
		}
	}
}
