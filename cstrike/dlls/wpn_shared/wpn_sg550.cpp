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

enum sg550_e
{
	SG550_IDLE = 0,
	SG550_SHOOT,
	SG550_SHOOT2,
	SG550_RELOAD,
	SG550_DRAW,
};

LINK_ENTITY_TO_CLASS( weapon_sg550, CSG550 );

void CSG550::Spawn( void )
{
	Precache( );
	m_iId = WEAPON_SG550;
	SET_MODEL( ENT( pev ), "models/w_sg550.mdl" );

	m_iDefaultAmmo = 30;
	m_flLastFire = 0;

	FallInit();// get ready to fall down.
}


void CSG550::Precache( void )
{
	PRECACHE_MODEL( "models/v_sg550.mdl" );
	PRECACHE_MODEL( "models/w_sg550.mdl" );

	PRECACHE_SOUND( "weapons/sg550-1.wav" );
	PRECACHE_SOUND( "weapons/sg550_boltpull.wav" );
	PRECACHE_SOUND( "weapons/sg550_clipin.wav" );
	PRECACHE_SOUND( "weapons/sg550_clipout.wav" );
	PRECACHE_SOUND( "weapons/zoom.wav" );

	m_iShellId = m_iShell = PRECACHE_MODEL( "models/rshell.mdl" );// brass shell

	m_usFireSG550 = PRECACHE_EVENT( 1, "events/sg550.sc" );
}

int CSG550::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "556Nato";
	p->iMaxAmmo1 = 90;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 30;
	p->iSlot = 0;
	p->iPosition = 16;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_SG550;
	p->iWeight = 20;

	return 1;
}

BOOL CSG550::Deploy( void )
{
	return DefaultDeploy( "models/v_sg550.mdl", "models/p_sg550.mdl", SG550_DRAW, "rifle", UseDecrement() ? 1 : 0 );
}

void CSG550::SecondaryAttack( void )
{
	if ( m_pPlayer->m_iFOV == 90 )
	{
		m_pPlayer->m_iFOV = 40;
		m_pPlayer->pev->fov = 40;
	}
	else if ( m_pPlayer->m_iFOV == 40 )
	{
		m_pPlayer->m_iFOV = 15;
		m_pPlayer->pev->fov = 15;
	}
	else if ( m_pPlayer->m_iFOV == 15 )
	{
		m_pPlayer->m_iFOV = 90;
		m_pPlayer->pev->fov = 90;
	}

	m_pPlayer->ResetMaxSpeed();

#ifndef CLIENT_DLL
	if ( TheBots )
	{
		TheBots->OnEvent( EVENT_WEAPON_ZOOMED, m_pPlayer );
	}
#endif

	EMIT_SOUND( m_pPlayer->edict(), CHAN_ITEM, "weapons/zoom.wav", 0.2f, 2.4f );
	m_flNextSecondaryAttack = 0.3f;
}

void CSG550::PrimaryAttack( void )
{
	if ( !FBitSet( m_pPlayer->pev->flags, FL_ONGROUND ) )
		SG550Fire( (float)( 0.45 * ( 1 - m_flAccuracy ) ), 0.25f, FALSE );
	else if ( m_pPlayer->pev->velocity.Length2D() > 0 )
		SG550Fire( 0.15f, 0.25f, FALSE );
	else if ( FBitSet( m_pPlayer->pev->flags, FL_DUCKING ) )
		SG550Fire( (float)( 0.04 * ( 1 - m_flAccuracy ) ), 0.25f, FALSE );
	else
		SG550Fire( (float)( 0.05 * ( 1 - m_flAccuracy ) ), 0.25f, FALSE );
}

void CSG550::SG550Fire( float flSpread, float flCycleTime, BOOL fUseAutoAim )
{
	if ( m_pPlayer->pev->fov == 90 )
		flSpread += 0.025;

	// Mark the time of this shot and determine the accuracy modifier based on the last shot fired...
	if ( m_flLastFire == 0 )
		m_flLastFire = gpGlobals->time;
	else
	{
		m_flAccuracy = (float)( ( gpGlobals->time - m_flLastFire ) * 0.35 + 0.65 );

		if ( m_flAccuracy > 0.98 )
			m_flAccuracy = 0.98f;

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

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

	m_pPlayer->m_iWeaponVolume = BIG_EXPLOSION_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

	Vector vecAiming;
	vecAiming = gpGlobals->v_forward;
	Vector vecSrc = m_pPlayer->GetGunPosition( );
	Vector vecDir;
	vecDir = m_pPlayer->FireBullets3( vecSrc, vecAiming, flSpread, 8192, 2, BULLET_PLAYER_556MM, 70, 0.98f, m_pPlayer->pev, true, m_pPlayer->random_seed );

	int flag;
#if defined( CLIENT_WEAPONS )
	flag = FEV_NOTHOST;
#else
	flag = 0;
#endif

	PLAYBACK_EVENT_FULL( flag, m_pPlayer->edict(), m_usFireSG550,
		0, (float *)&g_vecZero, (float *)&g_vecZero,
		vecDir.x,
		vecDir.y,
		(int)( m_pPlayer->pev->punchangle.x * 100 ),
		(int)( m_pPlayer->pev->punchangle.y * 100 ),
		5, 0 );

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay( flCycleTime );

	if ( !m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 );

	m_flTimeWeaponIdle = 1.8f;

	m_pPlayer->pev->punchangle.x -= UTIL_SharedRandomFloat( m_pPlayer->random_seed + 4, 0.75f, 1.25f ) + m_pPlayer->pev->punchangle.x / 4;
	m_pPlayer->pev->punchangle.y += UTIL_SharedRandomFloat( m_pPlayer->random_seed + 5, -0.75f, 0.75f );
}


void CSG550::Reload( void )
{
	if ( m_pPlayer->ammo_556nato <= 0 )
		return;

	int iResult;

	iResult = DefaultReload( 30, SG550_RELOAD, 3.35f );

	if ( iResult )
	{
		m_pPlayer->SetAnimation( PLAYER_RELOAD );
		if ( m_pPlayer->pev->fov != 90 )
		{
			m_pPlayer->pev->fov = 15;
			m_pPlayer->m_iFOV = 15;
			SecondaryAttack();
		}
	}
}

void CSG550::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	if ( m_flTimeWeaponIdle > 0 )
		return;

	if ( m_iClip )
	{
		m_flTimeWeaponIdle = 60;
		SendWeaponAnim( SG550_IDLE, UseDecrement() ? 1 : 0 );
	}
}


float CSG550::GetMaxSpeed( void )
{
	if ( m_pPlayer->m_iFOV == 90 )
		return 210;
	else
		return 150; // zoomed in
}
