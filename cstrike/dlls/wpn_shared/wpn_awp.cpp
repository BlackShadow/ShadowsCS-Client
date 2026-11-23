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

enum awp_e
{
	AWP_IDLE = 0,
	AWP_SHOOT,
	AWP_SHOOT2,
	AWP_SHOOT3,
	AWP_RELOAD,
	AWP_DRAW,
};

LINK_ENTITY_TO_CLASS( weapon_awp, CAWP );

void CAWP::Spawn( void )
{
	Precache( );
	m_iId = WEAPON_AWP;
	SET_MODEL( ENT( pev ), "models/w_awp.mdl" );

	m_iDefaultAmmo = 10;

	FallInit();// get ready to fall down.
}


void CAWP::Precache( void )
{
	PRECACHE_MODEL( "models/v_awp.mdl" );
	PRECACHE_MODEL( "models/w_awp.mdl" );

	PRECACHE_SOUND( "weapons/awp1.wav" );
	PRECACHE_SOUND( "weapons/boltpull1.wav" );
	PRECACHE_SOUND( "weapons/boltup.wav" );
	PRECACHE_SOUND( "weapons/boltdown.wav" );
	PRECACHE_SOUND( "weapons/zoom.wav" );

	PRECACHE_SOUND( "weapons/awp_deploy.wav" );
	PRECACHE_SOUND( "weapons/awp_clipin.wav" );
	PRECACHE_SOUND( "weapons/awp_clipout.wav" );

	m_iShellId = m_iShell = PRECACHE_MODEL( "models/rshell_big.mdl" );// brass shell
	m_usFireAWP = PRECACHE_EVENT( 1, "events/awp.sc" );
}

int CAWP::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "338Magnum";
	p->iMaxAmmo1 = 30;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 10;
	p->iSlot = 0;
	p->iPosition = 2;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_AWP;
	p->iWeight = 30;

	return 1;
}

BOOL CAWP::Deploy( void )
{
	if ( DefaultDeploy( "models/v_awp.mdl", "models/p_awp.mdl", AWP_DRAW, "rifle", UseDecrement() ? 1 : 0 ) )
	{
		m_pPlayer->m_flNextAttack = m_flNextPrimaryAttack = GetNextAttackDelay( 1.45f );
		m_flNextSecondaryAttack = 1;

		return TRUE;
	}

	return FALSE;
}

void CAWP::SecondaryAttack( void )
{
	if ( m_pPlayer->m_iFOV == 90 )
	{
		m_pPlayer->m_iFOV = 40;
		m_pPlayer->pev->fov = 40;
	}
	else if ( m_pPlayer->m_iFOV == 40 )
	{
		m_pPlayer->m_iFOV = 10;
		m_pPlayer->pev->fov = 10;
	}
	else
	{
		m_pPlayer->m_iFOV = 90;
		m_pPlayer->pev->fov = 90;
	}

#ifndef CLIENT_DLL
	if ( TheBots )
	{
		TheBots->OnEvent( EVENT_WEAPON_ZOOMED, m_pPlayer );
	}
#endif

	m_pPlayer->ResetMaxSpeed();
	EMIT_SOUND( m_pPlayer->edict(), CHAN_ITEM, "weapons/zoom.wav", 0.2f, 2.4f );
	m_flNextSecondaryAttack = 0.3f;
}

void CAWP::PrimaryAttack( void )
{
	if ( !FBitSet( m_pPlayer->pev->flags, FL_ONGROUND ) )
		AWPFire( 0.85f, 1.45f, FALSE );
	else if ( m_pPlayer->pev->velocity.Length2D() > 140 )
		AWPFire( 0.25f, 1.45f, FALSE );
	else if ( m_pPlayer->pev->velocity.Length2D() > 10 )
		AWPFire( 0.1f, 1.45f, FALSE );
	else if ( FBitSet( m_pPlayer->pev->flags, FL_DUCKING ) )
		AWPFire( 0, 1.45f, FALSE );
	else
		AWPFire( 0.001f, 1.45f, FALSE );
}

void CAWP::AWPFire( float flSpread, float flCycleTime, BOOL fUseAutoAim )
{
	if ( m_pPlayer->pev->fov != 90 )
	{
		m_pPlayer->m_bResumeZoom = true;
		m_pPlayer->m_iLastZoom = m_pPlayer->m_iFOV;
		m_pPlayer->m_iFOV = 90;
		m_pPlayer->pev->fov = 90;
	}
	else
		flSpread += 0.08;

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

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

	m_pPlayer->m_flEjectBrass = (float)( gpGlobals->time + 0.55 );

	m_pPlayer->m_iWeaponVolume = BIG_EXPLOSION_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

	Vector vecAiming;
	vecAiming = gpGlobals->v_forward;
	Vector vecSrc = m_pPlayer->GetGunPosition( );

	Vector vecDir;
	vecDir = m_pPlayer->FireBullets3( vecSrc, vecAiming, flSpread, 8192, 3, BULLET_PLAYER_338MAG, 115, 0.99f, m_pPlayer->pev, true, m_pPlayer->random_seed );

	int flag;
#if defined( CLIENT_WEAPONS )
	flag = FEV_NOTHOST;
#else
	flag = 0;
#endif

	PLAYBACK_EVENT_FULL( flag, m_pPlayer->edict(), m_usFireAWP,
		0, (float *)&g_vecZero, (float *)&g_vecZero,
		vecDir.x,
		vecDir.y,
		(int)( m_pPlayer->pev->punchangle.x * 100 ),
		(int)( m_pPlayer->pev->punchangle.x * 100 ),
		0, 0 );

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay( flCycleTime );

	if ( !m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 );

	m_flTimeWeaponIdle = 2;

	m_pPlayer->pev->punchangle.x -= 2;
}


void CAWP::Reload( void )
{
	if ( m_pPlayer->ammo_338mag <= 0 )
		return;

	int iResult;

	iResult = DefaultReload( 10, AWP_RELOAD, 2.5f );

	if ( iResult )
	{
		m_pPlayer->SetAnimation( PLAYER_RELOAD );

		if ( m_pPlayer->pev->fov != 90 )
		{
			m_pPlayer->m_iFOV = 10;
			m_pPlayer->pev->fov = 10;
			SecondaryAttack();
		}
	}
}


void CAWP::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	if ( m_flTimeWeaponIdle > 0 )
		return;

	if ( m_iClip )
	{
		m_flTimeWeaponIdle = 60;
		SendWeaponAnim( AWP_IDLE, UseDecrement() ? 1 : 0 );
	}
}


float CAWP::GetMaxSpeed( void )
{
	if ( m_pPlayer->m_iFOV == 90 )
		return 210;
	else
		// Slower speed when zoomed in.
		return 150;
}
