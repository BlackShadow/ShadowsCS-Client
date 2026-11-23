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

enum scout_e
{
	SCOUT_IDLE = 0,
	SCOUT_SHOOT,
	SCOUT_SHOOT2,
	SCOUT_RELOAD,
	SCOUT_DRAW,
};

LINK_ENTITY_TO_CLASS( weapon_scout, CSCOUT );

void CSCOUT::Spawn( void )
{
	Precache( );
	m_iId = WEAPON_SCOUT;
	SET_MODEL( ENT( pev ), "models/w_scout.mdl" );

	m_iDefaultAmmo = 10;

	FallInit();// get ready to fall down.
}


void CSCOUT::Precache( void )
{
	PRECACHE_MODEL( "models/v_scout.mdl" );
	PRECACHE_MODEL( "models/w_scout.mdl" );

	PRECACHE_SOUND( "weapons/scout_fire-1.wav" );
	PRECACHE_SOUND( "weapons/scout_bolt.wav" );
	PRECACHE_SOUND( "weapons/scout_clipin.wav" );
	PRECACHE_SOUND( "weapons/scout_clipout.wav" );
	PRECACHE_SOUND( "weapons/zoom.wav" );

	m_iShellId = m_iShell = PRECACHE_MODEL( "models/rshell_big.mdl" );// brass shell

	m_usFireScout = PRECACHE_EVENT( 1, "events/scout.sc" );
}

int CSCOUT::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "762Nato";
	p->iMaxAmmo1 = 90;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 10;
	p->iSlot = 0;
	p->iPosition = 9;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_SCOUT;
	p->iWeight = 30;

	return 1;
}

BOOL CSCOUT::Deploy( void )
{
	if ( DefaultDeploy( "models/v_scout.mdl", "models/p_scout.mdl", SCOUT_DRAW, "rifle", UseDecrement() ? 1 : 0 ) )
	{
		m_pPlayer->m_flNextAttack = m_flNextPrimaryAttack = GetNextAttackDelay( 1.25f );
		m_flNextSecondaryAttack = 1;

		return TRUE;
	}

	return FALSE;
}

void CSCOUT::SecondaryAttack( void )
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

void CSCOUT::PrimaryAttack( void )
{
	if ( !FBitSet( m_pPlayer->pev->flags, FL_ONGROUND ) )
		SCOUTFire( 0.2f, 1.25f, FALSE );
	else if ( m_pPlayer->pev->velocity.Length2D() > 170 )
		SCOUTFire( 0.075f, 1.25f, FALSE );
	else if ( FBitSet( m_pPlayer->pev->flags, FL_DUCKING ) )
		SCOUTFire( 0, 1.25f, FALSE );
	else
		SCOUTFire( 0.007f, 1.25f, FALSE );
}


void CSCOUT::SCOUTFire( float flSpread, float flCycleTime, BOOL fUseAutoAim )
{
	if ( m_pPlayer->pev->fov != 90 )
	{
		m_pPlayer->m_bResumeZoom = true;
		m_pPlayer->m_iLastZoom = m_pPlayer->m_iFOV;
		m_pPlayer->m_iFOV = 90;
	}
	else
		flSpread += 0.025;

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

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

	// Delay the shell ejection call because this is a sniper rifle....
	m_pPlayer->m_flEjectBrass = (float)( gpGlobals->time + 0.56 );

	m_pPlayer->m_iWeaponVolume = BIG_EXPLOSION_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

	Vector vecAiming;
	vecAiming = gpGlobals->v_forward;

	Vector vecSrc = m_pPlayer->GetGunPosition( );

	Vector vecDir;
	vecDir = m_pPlayer->FireBullets3( vecSrc, vecAiming, flSpread, 8192, 3, BULLET_PLAYER_762MM, 75, 0.98f, m_pPlayer->pev, true, m_pPlayer->random_seed );

	int flag;
#if defined( CLIENT_WEAPONS )
	flag = FEV_NOTHOST;
#else
	flag = 0;
#endif

	PLAYBACK_EVENT_FULL( flag, m_pPlayer->edict(), m_usFireScout,
		0, (float *)&g_vecZero, m_pPlayer->pev->angles,
		vecDir.x * 1000.0,
		vecDir.y * 1000.0,
		(int)( m_pPlayer->pev->punchangle.x * 100 ), (int)( m_pPlayer->pev->punchangle.y * 100 ),
		0, 0 );

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay( flCycleTime );

	if ( !m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 );

	m_flTimeWeaponIdle = 1.8f;

	m_pPlayer->pev->punchangle.x -= 2;
}


void CSCOUT::Reload( void )
{
	int iResult;

	iResult = DefaultReload( 10, SCOUT_RELOAD, 2 );

	if ( iResult )
	{
		if ( m_pPlayer->m_iFOV != 90 )
		{
			m_pPlayer->m_iFOV = 15;
			m_pPlayer->pev->fov = 15;
			SecondaryAttack();
		}
		m_pPlayer->SetAnimation( PLAYER_RELOAD );
	}
}

void CSCOUT::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	if ( m_flTimeWeaponIdle > 0 )
		return;

	if ( m_iClip )
	{
		m_flTimeWeaponIdle = 60;
		SendWeaponAnim( SCOUT_IDLE, UseDecrement() ? 1 : 0 );
	}
}


float CSCOUT::GetMaxSpeed( void )
{
	if ( m_pPlayer->m_iFOV == 90 )
		return 260;
	else
		return 220;	// zoomed in.
}
