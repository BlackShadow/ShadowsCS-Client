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

enum glock18_e
{
	GLOCK18_IDLE1 = 0,
	GLOCK18_IDLE2,
	GLOCK18_IDLE3,
	GLOCK18_SHOOT,
	GLOCK18_SHOOT2,
	GLOCK18_SHOOT3,
	GLOCK18_SHOOT_EMPTY,
	GLOCK18_RELOAD,
	GLOCK18_DRAW,
	GLOCK18_HOLSTER,
	GLOCK18_ADD_SILENCER,
	GLOCK18_DRAW2,
	GLOCK18_RELOAD2,
};

enum glock18_shield_e
{
	GLOCK18_SHIELD_IDLE1 = 0,
	GLOCK18_SHIELD_SHOOT,
	GLOCK18_SHIELD_SHOOT2,
	GLOCK18_SHIELD_SHOOT_EMPTY,
	GLOCK18_SHIELD_RELOAD,
	GLOCK18_SHIELD_DRAW,
	GLOCK18_SHIELD_IDLE,
	GLOCK18_SHIELD_UP,
	GLOCK18_SHIELD_DOWN,
};

LINK_ENTITY_TO_CLASS( weapon_glock18, CGLOCK18 );

void CGLOCK18::Spawn( void )
{
	Precache( );
	m_iId = WEAPON_GLOCK18;
	SET_MODEL( ENT( pev ), "models/w_glock18.mdl" );

	m_iDefaultAmmo = 20;
	m_bBurstFire = false;
	m_iGlock18ShotsFired = 0;
	m_flGlock18Shoot = 0;
	m_flAccuracy = 0.9f;

	ClearBits( m_iWeaponState, WPNSTATE_SHIELD_DRAWN );

	FallInit();// get ready to fall down.
}


void CGLOCK18::Precache( void )
{
	PRECACHE_MODEL( "models/v_glock18.mdl" );
	PRECACHE_MODEL( "models/w_glock18.mdl" );

	PRECACHE_MODEL( "models/shield/v_shield_glock18.mdl" );

	PRECACHE_SOUND( "weapons/glock18-1.wav" );
	PRECACHE_SOUND( "weapons/glock18-2.wav" );
	PRECACHE_SOUND( "weapons/clipout1.wav" );
	PRECACHE_SOUND( "weapons/clipin1.wav" );
	PRECACHE_SOUND( "weapons/sliderelease1.wav" );
	PRECACHE_SOUND( "weapons/slideback1.wav" );
	PRECACHE_SOUND( "weapons/357_cock1.wav" );

	PRECACHE_SOUND( "weapons/de_clipin.wav" );
	PRECACHE_SOUND( "weapons/de_clipout.wav" );

	m_iShellId = m_iShell = PRECACHE_MODEL( "models/pshell.mdl" );// brass shell

	m_usFireGlock18 = PRECACHE_EVENT( 1, "events/glock18.sc" );
}

int CGLOCK18::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "9mm";
	p->iMaxAmmo1 = 120;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 20;
	p->iSlot = 1;
	p->iPosition = 2;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_GLOCK18;
	p->iWeight = 5;

	return 1;
}

BOOL CGLOCK18::Deploy( void )
{
	// pev->body = 1;
	m_bBurstFire = false;
	m_iGlock18ShotsFired = 0;
	m_flGlock18Shoot = 0;
	m_flAccuracy = 0.9f;
	m_fMaxSpeed = 250;

	ClearBits( m_iWeaponState, WPNSTATE_SHIELD_DRAWN );

	m_pPlayer->m_bShieldDrawn = false;

	if ( m_pPlayer->HasShield() )
	{
		m_iWeaponState &= ~WPNSTATE_GLOCK18_BURST_MODE;
		return DefaultDeploy( "models/shield/v_shield_glock18.mdl", "models/shield/p_shield_glock18.mdl", GLOCK18_SHIELD_DRAW, "shieldgun", UseDecrement() ? 1 : 0 );
	}
	else
	{
		if ( RANDOM_LONG( 0, 1 ) )
			return DefaultDeploy( "models/v_glock18.mdl", "models/p_glock18.mdl", GLOCK18_DRAW, "onehanded", UseDecrement() ? 1 : 0 );
		else
			return DefaultDeploy( "models/v_glock18.mdl", "models/p_glock18.mdl", GLOCK18_DRAW2, "onehanded", UseDecrement() ? 1 : 0 );
	}
}

void CGLOCK18::SecondaryAttack( void )
{
	if ( ShieldSecondaryFire( GLOCK18_SHIELD_UP, GLOCK18_SHIELD_DOWN ) == true )
		return;

	if ( !( m_iWeaponState & WPNSTATE_GLOCK18_BURST_MODE ) )
	{
		ClientPrint( m_pPlayer->pev, HUD_PRINTCENTER, "#Switch_To_BurstFire" );
		m_iWeaponState |= WPNSTATE_GLOCK18_BURST_MODE;
	}
	else
	{
		ClientPrint( m_pPlayer->pev, HUD_PRINTCENTER, "#Switch_To_SemiAuto" );
		m_iWeaponState &= ~WPNSTATE_GLOCK18_BURST_MODE;
	}

	m_flNextSecondaryAttack = 0.3f;
}

void CGLOCK18::PrimaryAttack( void )
{
	if ( m_iWeaponState & WPNSTATE_GLOCK18_BURST_MODE )
	{
		if ( !FBitSet( m_pPlayer->pev->flags, FL_ONGROUND ) )
			GLOCK18Fire( (float)( 1.2 * ( 1 - m_flAccuracy ) ), 0.5f, TRUE );
		else if ( m_pPlayer->pev->velocity.Length2D() > 0 )
			GLOCK18Fire( (float)( 0.185 * ( 1 - m_flAccuracy ) ), 0.5f, TRUE );
		else if ( FBitSet( m_pPlayer->pev->flags, FL_DUCKING ) )
			GLOCK18Fire( (float)( 0.095 * ( 1 - m_flAccuracy ) ), 0.5f, TRUE );
		else
			GLOCK18Fire( (float)( 0.3 * ( 1 - m_flAccuracy ) ), 0.5f, TRUE );
	}
	else
	{
		if ( !FBitSet( m_pPlayer->pev->flags, FL_ONGROUND ) )
			GLOCK18Fire( (float)( 1.0 * ( 1 - m_flAccuracy ) ), 0.2f, FALSE );
		else if ( m_pPlayer->pev->velocity.Length2D() > 0 )
			GLOCK18Fire( (float)( 0.165 * ( 1 - m_flAccuracy ) ), 0.2f, FALSE );
		else if ( FBitSet( m_pPlayer->pev->flags, FL_DUCKING ) )
			GLOCK18Fire( (float)( 0.075 * ( 1 - m_flAccuracy ) ), 0.2f, FALSE );
		else
			GLOCK18Fire( (float)( 0.1 * ( 1 - m_flAccuracy ) ), 0.2f, FALSE );
	}
}

void CGLOCK18::GLOCK18Fire( float flSpread, float flCycleTime, BOOL bFireBurst )
{
	if ( bFireBurst )
	{
		m_iGlock18ShotsFired = 0;
	}
	else
	{
		flCycleTime -= 0.05;
		m_iShotsFired++;

		if ( m_iShotsFired > 1 )
			return;
	}

	// Mark the time of this shot and determine the accuracy modifier based on the last shot fired...
	if ( m_flLastFire == 0 )
		m_flLastFire = gpGlobals->time;
	else
	{
		m_flAccuracy -= ( 0.275 ) * ( 0.325 - ( gpGlobals->time - m_flLastFire ) );

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

	// non-silenced
	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

	Vector vecAiming;
	vecAiming = gpGlobals->v_forward;
	Vector vecSrc = m_pPlayer->GetGunPosition( );

	Vector vecDir;
	vecDir = m_pPlayer->FireBullets3( vecSrc, vecAiming, flSpread, 8192, 1, BULLET_PLAYER_9MM, 25, 0.75f, m_pPlayer->pev, true, m_pPlayer->random_seed );

	int flag;
#if defined( CLIENT_WEAPONS )
	flag = FEV_NOTHOST;
#else
	flag = 0;
#endif

	PLAYBACK_EVENT_FULL( flag, m_pPlayer->edict(), m_usFireGlock18,
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

	m_flTimeWeaponIdle = 2.5;

	if ( bFireBurst )
	{
		// Fire off the next two rounds
		m_flGlock18Shoot = (float)( gpGlobals->time + 0.1 );
		m_iGlock18ShotsFired++;
	}

	ResetPlayerShieldAnim();
}


void CGLOCK18::Reload( void )
{
	if ( m_pPlayer->ammo_9mm <= 0 )
		return;

	int iResult;

	if ( m_pPlayer->HasShield() )
		iResult = DefaultReload( 20, GLOCK18_SHIELD_RELOAD, 2.2f );
	else if ( RANDOM_LONG( 0, 1 ) )
		iResult = DefaultReload( 20, GLOCK18_RELOAD, 2.2f );
	else
		iResult = DefaultReload( 20, GLOCK18_RELOAD2, 2.2f );

	if ( iResult )
	{
		m_pPlayer->SetAnimation( PLAYER_RELOAD );

		m_flAccuracy = 0.9f;
	}
}

void CGLOCK18::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	if ( m_flTimeWeaponIdle > 0 )
		return;

	if ( m_pPlayer->HasShield() )
	{
		m_flTimeWeaponIdle = 20;

		if ( FBitSet( m_iWeaponState, WPNSTATE_SHIELD_DRAWN ) )
			SendWeaponAnim( GLOCK18_SHIELD_IDLE, UseDecrement() ? 1 : 0 );
	}
	else
	{
		// only idle if the slid isn't back
		if ( m_iClip != 0 )
		{
			int iAnim;
			float flRand = RANDOM_FLOAT( 0, 1 );
			if ( flRand <= 0.3 )
			{
				iAnim = GLOCK18_IDLE3;
				m_flTimeWeaponIdle = 49.0 / 16;
			}
			else if ( flRand <= 0.6 )
			{
				iAnim = GLOCK18_IDLE1;
				m_flTimeWeaponIdle = 60.0 / 16.0;
			}
			else
			{
				iAnim = GLOCK18_IDLE2;
				m_flTimeWeaponIdle = 40.0 / 16.0;
			}
			SendWeaponAnim( iAnim, UseDecrement() ? 1 : 0 );
		}
	}
}
