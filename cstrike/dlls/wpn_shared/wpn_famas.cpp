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

enum famas_e
{
	FAMAS_IDLE1 = 0,
	FAMAS_RELOAD,
	FAMAS_DRAW,
	FAMAS_SHOOT1,
	FAMAS_SHOOT2,
	FAMAS_SHOOT3,
};

LINK_ENTITY_TO_CLASS( weapon_famas, CFamas );

void CFamas::Spawn( void )
{
	Precache( );
	m_iId = WEAPON_FAMAS;
	SET_MODEL( ENT( pev ), "models/w_famas.mdl" );

	m_iDefaultAmmo = 25;
	m_iFamasShotsFired = 0;
	m_flFamasShoot = 0;

	FallInit();// get ready to fall down.
}

void CFamas::Precache( void )
{
	PRECACHE_MODEL( "models/v_famas.mdl" );
	PRECACHE_MODEL( "models/w_famas.mdl" );

	PRECACHE_SOUND( "weapons/famas-1.wav" );
	PRECACHE_SOUND( "weapons/famas-2.wav" );
	PRECACHE_SOUND( "weapons/famas_clipout.wav" );
	PRECACHE_SOUND( "weapons/famas_clipin.wav" );
	PRECACHE_SOUND( "weapons/famas_boltpull.wav" );
	PRECACHE_SOUND( "weapons/famas_boltslap.wav" );
	PRECACHE_SOUND( "weapons/famas_forearm.wav" );
	PRECACHE_SOUND( "weapons/famas-burst.wav" );

	m_iShell = PRECACHE_MODEL( "models/rshell.mdl" );// brass shell

	m_usFireFamas = PRECACHE_EVENT( 1, "events/famas.sc" );
}

int CFamas::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "556Nato";
	p->iMaxAmmo1 = 90;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 25;
	p->iSlot = 0;
	p->iPosition = 18;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_FAMAS;
	p->iWeight = 75;

	return 1;
}

BOOL CFamas::Deploy( void )
{
	m_flAccuracy = 0.2f;
	m_iShotsFired = 0;
	iShellOn = 1;
	m_iFamasShotsFired = 0;
	m_flFamasShoot = 0;

	return DefaultDeploy( "models/v_famas.mdl", "models/p_famas.mdl", FAMAS_DRAW, "carbine", UseDecrement() ? 1 : 0 );
}

// Secondary attack could be three-round burst mode
void CFamas::SecondaryAttack( void )
{
	if ( !( m_iWeaponState & WPNSTATE_FAMAS_BURST_MODE ) )
	{
		ClientPrint( m_pPlayer->pev, HUD_PRINTCENTER, "#Switch_To_BurstFire" );
		m_iWeaponState |= WPNSTATE_FAMAS_BURST_MODE;
	}
	else
	{
		ClientPrint( m_pPlayer->pev, HUD_PRINTCENTER, "#Switch_To_FullAuto" );
		m_iWeaponState &= ~WPNSTATE_FAMAS_BURST_MODE;
	}
	m_flNextSecondaryAttack = 0.3f;
}

void CFamas::PrimaryAttack( void )
{
	// don't fire underwater
	if ( m_pPlayer->pev->waterlevel == 3 )
	{
		PlayEmptySound( );
		m_flNextPrimaryAttack = GetNextAttackDelay( 0.15f );
		return;
	}

	bool bFireBurst = false;
	if ( m_iWeaponState & WPNSTATE_FAMAS_BURST_MODE )
		bFireBurst = true;

	if ( !FBitSet( m_pPlayer->pev->flags, FL_ONGROUND ) )	// if player is in air
		FamasFire( 0.3 * m_flAccuracy + 0.03, 0.0825f, FALSE, bFireBurst );
	else if ( m_pPlayer->pev->velocity.Length2D() > 140 )	// if player is moving
		FamasFire( 0.07 * m_flAccuracy + 0.03, 0.0825f, FALSE, bFireBurst );
	/* new code */
	else
		FamasFire( 0.02 * m_flAccuracy, 0.0825f, FALSE, bFireBurst );
}

void CFamas::FamasFire( float flSpread, float flCycleTime, BOOL fUseAutoAim, BOOL bFireBurst )
{
	// change a few things if we're in burst mode
	if ( bFireBurst )
	{
		m_iFamasShotsFired = 0;
		flCycleTime = 0.55f;		// how long it takes to become accurate again
	}
	else
	{
		flSpread += 0.01;
	}
	m_iShotsFired++;

	m_bDelayFire = true;

	m_flAccuracy = ( m_iShotsFired * m_iShotsFired * m_iShotsFired / 215 ) + 0.3;
	if ( m_flAccuracy > 1 )
		m_flAccuracy = 1;

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

	// non-silenced
	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	Vector vecAiming;
	vecAiming = gpGlobals->v_forward;
	Vector vecSrc = m_pPlayer->GetGunPosition( );

	Vector vecDir;
	if ( !bFireBurst )
		vecDir = m_pPlayer->FireBullets3( vecSrc, vecAiming, flSpread, 8192, 2, BULLET_PLAYER_556MM, 30, 0.96f, m_pPlayer->pev, false, m_pPlayer->random_seed );
	else
		vecDir = m_pPlayer->FireBullets3( vecSrc, vecAiming, flSpread, 8192, 2, BULLET_PLAYER_556MM, 34, 0.96f, m_pPlayer->pev, false, m_pPlayer->random_seed );

	int flag;
#if defined( CLIENT_WEAPONS )
	flag = FEV_NOTHOST;
#else
	flag = 0;
#endif

	// This mask holds three custom flags - first bit is burst mode flag, second bit tells us if we have enough bullets for burst mode, last says whether or not this is our first burst mode shot
	int mask = 0;
	//mask |= (m_iWeaponState & WPNSTATE_FAMAS_BURST_MODE) ? 1 : 0;
	//mask |= ((m_iClip < 2) ? 1 : 0) << 4;
	//mask |= ((m_iFamasShotsFired == 0) ? 1 : 0) << 8;

	PLAYBACK_EVENT_FULL( flag, m_pPlayer->edict(), m_usFireFamas,
		0, (float *)&g_vecZero, (float *)&g_vecZero,
		vecDir.x,
		vecDir.y,
		(int)( m_pPlayer->pev->punchangle.x * 10000000 ),
		(int)( m_pPlayer->pev->punchangle.y * 10000000 ),
		mask, 0 );

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay( flCycleTime );

	if ( !m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 );

	m_flTimeWeaponIdle = 1.1f;	// 1.9

	if ( m_pPlayer->pev->velocity.Length2D() > 0 )
		KickBack( 1, 0.45f, 0.275f, 0.05f, 4, 2.5f, 7 );
	else if ( !FBitSet( m_pPlayer->pev->flags, FL_ONGROUND ) )
		KickBack( 1.25f, 0.45f, 0.22f, 0.18f, 5.5f, 4, 5 );
	else if ( FBitSet( m_pPlayer->pev->flags, FL_DUCKING ) )
		KickBack( 0.575f, 0.325f, 0.2f, 0.011f, 3.25f, 2, 8 );
	else
		KickBack( 0.625f, 0.375f, 0.25f, 0.0125f, 3.5f, 2.25f, 8 );

	if ( bFireBurst )
	{
		// Fire off the next two rounds
		m_flFamasShoot = gpGlobals->time + 0.05;	// 0.1
		m_fBurstSpread = flSpread;
		m_iFamasShotsFired++;
	}
}


void CFamas::Reload( void )
{
	if ( m_pPlayer->ammo_556nato <= 0 )
		return;

	int iResult;

	iResult = DefaultReload( 25, FAMAS_RELOAD, 3.3f );

	if ( iResult )
	{
		m_pPlayer->SetAnimation( PLAYER_RELOAD );

		if ( m_pPlayer->m_iFOV != 90 )
			SecondaryAttack();

		m_flAccuracy = 0;
		m_iShotsFired = 0;
		m_bDelayFire = false;
	}
}

void CFamas::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	if ( m_flTimeWeaponIdle > 0 )
		return;

	m_flTimeWeaponIdle = 20;

	SendWeaponAnim( FAMAS_IDLE1, UseDecrement() ? 1 : 0 );
}
