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

enum usp_e
{
	USP_IDLE = 0,
	USP_SHOOT1,
	USP_SHOOT2,
	USP_SHOOT3,
	USP_SHOOT_EMPTY,
	USP_RELOAD,
	USP_DRAW,
	USP_ATTACH_SILENCER,
	USP_UNSIL_IDLE,
	USP_UNSIL_SHOOT1,
	USP_UNSIL_SHOOT2,
	USP_UNSIL_SHOOT3,
	USP_UNSIL_SHOOT_EMPTY,
	USP_UNSIL_RELOAD,
	USP_UNSIL_DRAW,
	USP_DETACH_SILENCER,
};

enum usp_shield_e
{
	USP_SHIELD_IDLE = 0,
	USP_SHIELD_SHOOT1,
	USP_SHIELD_SHOOT2,
	USP_SHIELD_SHOOT_EMPTY,
	USP_SHIELD_RELOAD,
	USP_SHIELD_DRAW,
	USP_SHIELD_UP_IDLE,
	USP_SHIELD_UP,
	USP_SHIELD_DOWN,
};

LINK_ENTITY_TO_CLASS( weapon_usp, CUSP );

void CUSP::Spawn( void )
{
	Precache( );
	m_iId = WEAPON_USP;
	SET_MODEL( ENT( pev ), "models/w_usp.mdl" );

	m_iDefaultAmmo = 12;
	m_flAccuracy = 0.92f;

	m_iWeaponState &= ~WPNSTATE_SHIELD_DRAWN;

	FallInit();// get ready to fall down.
}


void CUSP::Precache( void )
{
	PRECACHE_MODEL( "models/v_usp.mdl" );
	PRECACHE_MODEL( "models/w_usp.mdl" );
	PRECACHE_MODEL( "models/shield/v_shield_usp.mdl" );

	PRECACHE_SOUND( "weapons/usp1.wav" );
	PRECACHE_SOUND( "weapons/usp2.wav" );
	PRECACHE_SOUND( "weapons/usp_unsil-1.wav" );
	PRECACHE_SOUND( "weapons/usp_clipout.wav" );
	PRECACHE_SOUND( "weapons/usp_clipin.wav" );
	PRECACHE_SOUND( "weapons/usp_silencer_on.wav" );
	PRECACHE_SOUND( "weapons/usp_silencer_off.wav" );
	PRECACHE_SOUND( "weapons/usp_sliderelease.wav" );
	PRECACHE_SOUND( "weapons/usp_slideback.wav" );

	m_iShell = PRECACHE_MODEL( "models/pshell.mdl" );// brass shell

	m_usFireUSP = PRECACHE_EVENT( 1, "events/usp.sc" );
}

int CUSP::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "45ACP";
	p->iMaxAmmo1 = 100;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 12;
	p->iSlot = 1;
	p->iPosition = 4;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_USP;
	p->iWeight = 5;

	return 1;
}

BOOL CUSP::Deploy( void )
{
	m_flAccuracy = 0.92f;
	m_fMaxSpeed = 250;

	m_iWeaponState &= ~WPNSTATE_SHIELD_DRAWN;

	m_pPlayer->m_bShieldDrawn = false;

	if ( m_pPlayer->HasShield() )
	{
		m_iWeaponState &= ~WPNSTATE_USP_SILENCED;
		return DefaultDeploy( "models/shield/v_shield_usp.mdl", "models/shield/p_shield_usp.mdl", USP_SHIELD_DRAW, "shieldgun", UseDecrement() ? 1 : 0 );
	}
	else
	{
		if ( m_iWeaponState & WPNSTATE_USP_SILENCED )
			return DefaultDeploy( "models/v_usp.mdl", "models/p_usp.mdl", USP_DRAW, "onehanded", UseDecrement() ? 1 : 0 );
		else
			return DefaultDeploy( "models/v_usp.mdl", "models/p_usp.mdl", USP_UNSIL_DRAW, "onehanded", UseDecrement() ? 1 : 0 );
	}
}

void CUSP::SecondaryAttack( void )
{
	if ( ShieldSecondaryFire( USP_SHIELD_UP, USP_SHIELD_DOWN ) == true )
		return;

	if ( !( m_iWeaponState & WPNSTATE_USP_SILENCED ) )
	{
		m_iWeaponState |= WPNSTATE_USP_SILENCED;

		SendWeaponAnim( USP_ATTACH_SILENCER, UseDecrement() ? 1 : 0 );
		strcpy( m_pPlayer->m_szAnimExtention, "onehanded" );
	}
	else
	{
		m_iWeaponState &= ~WPNSTATE_USP_SILENCED;

		SendWeaponAnim( USP_DETACH_SILENCER, UseDecrement() ? 1 : 0 );
		strcpy( m_pPlayer->m_szAnimExtention, "onehanded" );
	}

	m_flNextSecondaryAttack = 3;
	m_flNextPrimaryAttack = GetNextAttackDelay( 3 );
	m_flTimeWeaponIdle = 3;
}


void CUSP::PrimaryAttack( void )
{
	if ( m_iWeaponState & WPNSTATE_USP_SILENCED )
	{
		if ( !FBitSet( m_pPlayer->pev->flags, FL_ONGROUND ) )
			USPFire( (float)( 1.3 * ( 1 - m_flAccuracy ) ), 0.225f, FALSE );
		else if ( m_pPlayer->pev->velocity.Length2D() > 0 )
			USPFire( (float)( 0.25 * ( 1 - m_flAccuracy ) ), 0.225f, FALSE );
		else if ( FBitSet( m_pPlayer->pev->flags, FL_DUCKING ) )
			USPFire( (float)( 0.125 * ( 1 - m_flAccuracy ) ), 0.225f, FALSE );
		else
			USPFire( (float)( 0.15 * ( 1 - m_flAccuracy ) ), 0.225f, FALSE );
	}
	else
	{
		if ( !FBitSet( m_pPlayer->pev->flags, FL_ONGROUND ) )
			USPFire( (float)( 1.2 * ( 1 - m_flAccuracy ) ), 0.225f, FALSE );
		else if ( m_pPlayer->pev->velocity.Length2D() > 0 )
			USPFire( (float)( 0.225 * ( 1 - m_flAccuracy ) ), 0.225f, FALSE );
		else if ( FBitSet( m_pPlayer->pev->flags, FL_DUCKING ) )
			USPFire( (float)( 0.08 * ( 1 - m_flAccuracy ) ), 0.225f, FALSE );
		else
			USPFire( (float)( 0.1 * ( 1 - m_flAccuracy ) ), 0.225f, FALSE );
	}
}

void CUSP::USPFire( float flSpread, float flCycleTime, BOOL fUseSemi )
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
		m_flAccuracy -= ( 0.275 ) * ( 0.3 - ( gpGlobals->time - m_flLastFire ) );

		if ( m_flAccuracy > 0.92 )
			m_flAccuracy = 0.92f;
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
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay( flCycleTime );

	m_iClip--;

	SetPlayerShieldAnim();

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	m_pPlayer->m_iWeaponVolume = BIG_EXPLOSION_VOLUME;
	m_pPlayer->m_iWeaponFlash = DIM_GUN_FLASH;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

	if ( !( m_iWeaponState & WPNSTATE_USP_SILENCED ) )
		m_pPlayer->pev->effects |= EF_MUZZLEFLASH;

	int flag;
#if defined( CLIENT_WEAPONS )
	flag = FEV_NOTHOST;
#else
	flag = 0;
#endif

	Vector vecAiming;
	vecAiming = gpGlobals->v_forward;
	Vector vecSrc = m_pPlayer->GetGunPosition( );

	Vector vecDir;
	if ( m_iWeaponState & WPNSTATE_USP_SILENCED )
		vecDir = m_pPlayer->FireBullets3( vecSrc, vecAiming, flSpread, 4096, 1, BULLET_PLAYER_45ACP, 30, 0.79f, m_pPlayer->pev, true, m_pPlayer->random_seed );
	else
		vecDir = m_pPlayer->FireBullets3( vecSrc, vecAiming, flSpread, 4096, 1, BULLET_PLAYER_45ACP, 34, 0.79f, m_pPlayer->pev, true, m_pPlayer->random_seed );

	PLAYBACK_EVENT_FULL( flag, m_pPlayer->edict(), m_usFireUSP,
		0, (float *)&g_vecZero, (float *)&g_vecZero,
		vecDir.x,
		vecDir.y,
		(int)( m_pPlayer->pev->punchangle.x * 100 ), 0, m_iClip ? 0 : 1, m_iWeaponState & WPNSTATE_USP_SILENCED );

	if ( !m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 );

	m_flTimeWeaponIdle = 2;

#ifndef CLIENT_DLL
	m_pPlayer->pev->punchangle.x -= 2;
#endif

	ResetPlayerShieldAnim();
}


void CUSP::Reload( void )
{
	if ( m_pPlayer->ammo_45acp <= 0 )
		return;

	int iResult;

	if ( m_pPlayer->HasShield() )
		iResult = DefaultReload( 12, USP_SHIELD_RELOAD, 2.7f );
	else if ( m_iWeaponState & WPNSTATE_USP_SILENCED )
		iResult = DefaultReload( 12, USP_RELOAD, 2.7f );
	else
		iResult = DefaultReload( 12, USP_UNSIL_RELOAD, 2.7f );

	if ( iResult )
	{
		m_pPlayer->SetAnimation( PLAYER_RELOAD );
		m_flAccuracy = 0.92f;
	}
}

void CUSP::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	if ( m_flTimeWeaponIdle > 0 )
		return;

	if ( m_pPlayer->HasShield() )
	{
		m_flTimeWeaponIdle = 20;

		if ( FBitSet( m_iWeaponState, WPNSTATE_SHIELD_DRAWN ) )
			SendWeaponAnim( USP_SHIELD_UP_IDLE, UseDecrement() ? 1 : 0 );
	}
	else
	{
		// only idle if the slid isn't back
		if ( m_iClip != 0 )
		{
			int iAnim;

			if ( m_iWeaponState & WPNSTATE_USP_SILENCED )
				iAnim = USP_IDLE;
			else
				iAnim = USP_UNSIL_IDLE;
			m_flTimeWeaponIdle = 60;

			SendWeaponAnim( iAnim, UseDecrement() ? 1 : 0 );
		}
	}
}
