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

enum deagle_e
{
	DEAGLE_IDLE1 = 0,
	DEAGLE_SHOOT1,
	DEAGLE_SHOOT2,
	DEAGLE_SHOOT_EMPTY,
	DEAGLE_RELOAD,
	DEAGLE_DRAW,
};

LINK_ENTITY_TO_CLASS( weapon_deagle, CDEAGLE );

void CDEAGLE::Spawn( void )
{
	Precache( );
	m_iId = WEAPON_DEAGLE;
	SET_MODEL( ENT( pev ), "models/w_deagle.mdl" );

	m_iDefaultAmmo = 7;
	m_flAccuracy = 0.9f;

	m_iWeaponState &= ~WPNSTATE_SHIELD_DRAWN;

	m_fMaxSpeed = 250;

	FallInit();// get ready to fall down.
}


void CDEAGLE::Precache( void )
{
	PRECACHE_MODEL( "models/v_deagle.mdl" );
	PRECACHE_MODEL( "models/shield/v_shield_deagle.mdl" );

	PRECACHE_MODEL( "models/w_deagle.mdl" );

	PRECACHE_SOUND( "weapons/deagle-1.wav" );
	PRECACHE_SOUND( "weapons/deagle-2.wav" );
	PRECACHE_SOUND( "weapons/de_clipout.wav" );
	PRECACHE_SOUND( "weapons/de_clipin.wav" );
	PRECACHE_SOUND( "weapons/de_deploy.wav" );

	m_iShell = PRECACHE_MODEL( "models/pshell.mdl" );// brass shell

	m_usFireDeagle = PRECACHE_EVENT( 1, "events/deagle.sc" );
}

int CDEAGLE::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "50AE";
	p->iMaxAmmo1 = 35;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 7;
	p->iSlot = 1;
	p->iPosition = 1;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_DEAGLE;
	p->iWeight = 7;

	return 1;
}

BOOL CDEAGLE::Deploy( void )
{
	m_flAccuracy = 0.9f;

	m_iWeaponState &= ~WPNSTATE_SHIELD_DRAWN;

	m_pPlayer->m_bShieldDrawn = false;

	m_fMaxSpeed = 250;

	if ( m_pPlayer->HasShield() )
		return DefaultDeploy( "models/shield/v_shield_deagle.mdl", "models/shield/p_shield_deagle.mdl", SHIELDGUN_DRAW, "shieldgun", UseDecrement() ? 1 : 0 );
	else
		return DefaultDeploy( "models/v_deagle.mdl", "models/p_deagle.mdl", DEAGLE_DRAW, "onehanded", UseDecrement() ? 1 : 0 );
}

void CDEAGLE::PrimaryAttack( void )
{
	if ( !FBitSet( m_pPlayer->pev->flags, FL_ONGROUND ) )
		DEAGLEFire( (float)( 1.5 * ( 1 - m_flAccuracy ) ), 0.3f, FALSE );
	else if ( m_pPlayer->pev->velocity.Length2D() > 0 )
		DEAGLEFire( (float)( 0.25 * ( 1 - m_flAccuracy ) ), 0.3f, FALSE );
	else if ( FBitSet( m_pPlayer->pev->flags, FL_DUCKING ) )
		DEAGLEFire( (float)( 0.115 * ( 1 - m_flAccuracy ) ), 0.3f, FALSE );
	else
		DEAGLEFire( (float)( 0.13 * ( 1 - m_flAccuracy ) ), 0.3f, FALSE );
}

void CDEAGLE::SecondaryAttack( void )
{
	ShieldSecondaryFire( SHIELDGUN_UP, SHIELDGUN_DOWN );
}


void CDEAGLE::DEAGLEFire( float flSpread, float flCycleTime, BOOL fUseSemi )
{
	flCycleTime -= 0.075;
	m_iShotsFired++;

	if ( m_iShotsFired > 1 )
		return;

	// Mark the time of this shot and determine the accuracy modifier based on the last shot fired...
	if ( m_flLastFire == 0 )
		m_flLastFire = gpGlobals->time;
	else
	{
		m_flAccuracy -= ( 0.35 ) * ( 0.4 - ( gpGlobals->time - m_flLastFire ) );

		if ( m_flAccuracy > 0.9 )
			m_flAccuracy = 0.9f;
		else if ( m_flAccuracy < 0.55 )
			m_flAccuracy = 0.55f;

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

	m_pPlayer->m_iWeaponVolume = BIG_EXPLOSION_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

	Vector vecAiming;
	vecAiming = gpGlobals->v_forward;

	Vector vecSrc = m_pPlayer->GetGunPosition( );

	Vector vecDir;
	vecDir = m_pPlayer->FireBullets3( vecSrc, vecAiming, flSpread, 4096, 2, BULLET_PLAYER_50AE, 54, 0.81f, m_pPlayer->pev, true, m_pPlayer->random_seed );

#if defined( CLIENT_WEAPONS )
	int flag = FEV_NOTHOST;
#else
	int flag = 0;
#endif

	PLAYBACK_EVENT_FULL( flag, m_pPlayer->edict(), m_usFireDeagle,
		0, (float *)&g_vecZero, (float *)&g_vecZero,
		vecDir.x,
		vecDir.y,
		(int)( m_pPlayer->pev->punchangle.x * 100 ),
		(int)( m_pPlayer->pev->punchangle.y * 100 ),
		m_iClip ? 0 : 1, 0 );

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay( flCycleTime );

	if ( !m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 );

	m_flTimeWeaponIdle = 1.8f;

	m_pPlayer->pev->punchangle.x -= 2;

	ResetPlayerShieldAnim();
}


void CDEAGLE::Reload( void )
{
	if ( m_pPlayer->ammo_50ae <= 0 )
		return;

	if ( DefaultReload( 7, DEAGLE_RELOAD, 2.2f ) )
	{
		m_pPlayer->SetAnimation( PLAYER_RELOAD );
		m_flAccuracy = 0.9f;
	}
}

void CDEAGLE::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	if ( m_flTimeWeaponIdle > 0 )
		return;

	m_flTimeWeaponIdle = 20;

	if ( FBitSet( m_iWeaponState, WPNSTATE_SHIELD_DRAWN ) )
		SendWeaponAnim( SHIELDGUN_DRAWN_IDLE, UseDecrement() ? 1 : 0 );
}
