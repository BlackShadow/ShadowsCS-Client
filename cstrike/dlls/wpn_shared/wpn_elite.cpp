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

enum elite_e
{
	ELITE_IDLE = 0,
	ELITE_IDLE_LEFTEMPTY,
	ELITE_SHOOTLEFT1,
	ELITE_SHOOTLEFT2,
	ELITE_SHOOTLEFT3,
	ELITE_SHOOTLEFT4,
	ELITE_SHOOTLEFT5,
	ELITE_SHOOTLEFTLAST,
	ELITE_SHOOTRIGHT1,
	ELITE_SHOOTRIGHT2,
	ELITE_SHOOTRIGHT3,
	ELITE_SHOOTRIGHT4,
	ELITE_SHOOTRIGHT5,
	ELITE_SHOOTRIGHTLAST,
	ELITE_RELOAD,
	ELITE_DRAW,
};

LINK_ENTITY_TO_CLASS( weapon_elite, CELITE );

void CELITE::Spawn( void )
{
	Precache( );
	m_iId = WEAPON_ELITE;
	SET_MODEL( ENT( pev ), "models/w_elite.mdl" );

	m_iDefaultAmmo = 30;
	m_flAccuracy = 0.88f;

	FallInit();// get ready to fall down.
}


void CELITE::Precache( void )
{
	PRECACHE_MODEL( "models/v_elite.mdl" );
	PRECACHE_MODEL( "models/w_elite.mdl" );

	PRECACHE_SOUND( "weapons/elite_fire.wav" );
	PRECACHE_SOUND( "weapons/elite_reloadstart.wav" );
	PRECACHE_SOUND( "weapons/elite_leftclipin.wav" );
	PRECACHE_SOUND( "weapons/elite_clipout.wav" );
	PRECACHE_SOUND( "weapons/elite_sliderelease.wav" );
	PRECACHE_SOUND( "weapons/elite_rightclipin.wav" );
	PRECACHE_SOUND( "weapons/elite_deploy.wav" );

	m_iShell = PRECACHE_MODEL( "models/pshell.mdl" );// brass shell

	m_usFireELITE_LEFT = PRECACHE_EVENT( 1, "events/elite_left.sc" );
	m_usFireELITE_RIGHT = PRECACHE_EVENT( 1, "events/elite_right.sc" );
}

int CELITE::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "9mm";
	p->iMaxAmmo1 = 120;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 30;
	p->iSlot = 1;
	p->iPosition = 5;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_ELITE;
	p->iWeight = 5;

	return 1;
}

BOOL CELITE::Deploy( void )
{
	m_flAccuracy = 0.88f;

	if ( !( m_iClip & 1 ) )
		m_iWeaponState |= WPNSTATE_ELITE_LEFT;

	return DefaultDeploy( "models/v_elite.mdl", "models/p_elite.mdl", ELITE_DRAW, "dualpistols", UseDecrement() ? 1 : 0 );
}


void CELITE::PrimaryAttack( void )
{
	if ( !FBitSet( m_pPlayer->pev->flags, FL_ONGROUND ) )
		ELITEFire( (float)( 1.3 * ( 1 - m_flAccuracy ) ), 0.2f, FALSE );
	else if ( m_pPlayer->pev->velocity.Length2D() > 0 )
		ELITEFire( (float)( 0.175 * ( 1 - m_flAccuracy ) ), 0.2f, FALSE );
	else if ( FBitSet( m_pPlayer->pev->flags, FL_DUCKING ) )
		ELITEFire( (float)( 0.08 * ( 1 - m_flAccuracy ) ), 0.2f, FALSE );
	else
		ELITEFire( (float)( 0.1 * ( 1 - m_flAccuracy ) ), 0.2f, FALSE );
}

void CELITE::ELITEFire( float flSpread, float flCycleTime, BOOL fUseSemi )
{
	m_iShotsFired++;

	if ( m_iShotsFired > 1 )
		return;

	float flTimeDiff = gpGlobals->time - m_flLastFire;

	// Mark the time of this shot and determine the accuracy modifier based on the last shot fired...
	if ( m_flLastFire == 0 )
		m_flLastFire = gpGlobals->time;
	else
	{
		m_flAccuracy -= ( 0.275 ) * ( 0.325 - flTimeDiff );

		if ( m_flAccuracy > 0.88 )
			m_flAccuracy = 0.88f;
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
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay( (float)( flCycleTime - 0.125 ) );

	m_iClip--;

	m_pPlayer->m_iWeaponVolume = BIG_EXPLOSION_VOLUME;
	m_pPlayer->m_iWeaponFlash = DIM_GUN_FLASH;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

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

	if ( m_iWeaponState & WPNSTATE_ELITE_LEFT )
	{
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

		m_iWeaponState &= ~WPNSTATE_ELITE_LEFT;

		vecSrc = vecSrc - gpGlobals->v_right * 5;
		Vector vecDir;
		vecDir = m_pPlayer->FireBullets3( vecSrc, vecAiming, flSpread, 8192, 1, BULLET_PLAYER_9MM, 36, 0.75f, m_pPlayer->pev, true, m_pPlayer->random_seed );

		PLAYBACK_EVENT_FULL( flag, m_pPlayer->edict(), m_usFireELITE_LEFT,
			0, (float *)&g_vecZero, (float *)&g_vecZero,
			flTimeDiff,
			vecDir.x,
			(int)( vecDir.y * 100 ),
			m_iClip, 0, 0 );
	}
	else
	{
		m_pPlayer->SetAnimation( PLAYER_ATTACK2 );

		m_iWeaponState |= WPNSTATE_ELITE_LEFT;

		vecSrc = vecSrc + gpGlobals->v_right * 5;
		Vector vecDir;
		vecDir = m_pPlayer->FireBullets3( vecSrc, vecAiming, flSpread, 8192, 1, BULLET_PLAYER_9MM, 36, 0.75f, m_pPlayer->pev, true, m_pPlayer->random_seed );

		PLAYBACK_EVENT_FULL( flag, m_pPlayer->edict(), m_usFireELITE_RIGHT,
			0, (float *)&g_vecZero, (float *)&g_vecZero,
			flTimeDiff,
			vecDir.x, (int)( vecDir.y * 100 ),
			m_iClip, 0, 0 );
	}

	if ( !m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 );

	m_flTimeWeaponIdle = 2;

	m_pPlayer->pev->punchangle.x -= 2;
}


void CELITE::Reload( void )
{
	if ( m_pPlayer->ammo_9mm <= 0 )
		return;

	if ( DefaultReload( 30, ELITE_RELOAD, 4.5f ) )
	{
		m_pPlayer->SetAnimation( PLAYER_RELOAD );
		m_flAccuracy = 0.88f;
	}
}

void CELITE::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	if ( m_flTimeWeaponIdle > 0 )
		return;

	// only idle if the slid isn't back
	if ( m_iClip != 0 )
	{
		int iAnim;
		if ( m_iClip == 1 )
			iAnim = ELITE_IDLE_LEFTEMPTY;
		else
			iAnim = ELITE_IDLE;

		m_flTimeWeaponIdle = 60;

		SendWeaponAnim( iAnim, UseDecrement() ? 1 : 0 );
	}
}
