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

enum mac10_e
{
	MAC10_IDLE1 = 0,
	MAC10_RELOAD,
	MAC10_DRAW,
	MAC10_SHOOT1,
	MAC10_SHOOT2,
	MAC10_SHOOT3,
};

LINK_ENTITY_TO_CLASS( weapon_mac10, CMAC10 );

void CMAC10::Spawn( void )
{
	Precache( );
	m_iId = WEAPON_MAC10;
	SET_MODEL( ENT( pev ), "models/w_mac10.mdl" );

	m_iDefaultAmmo = 30;
	m_flAccuracy = 0.15f;
	m_bDelayFire = false;

	FallInit();// get ready to fall down.
}


void CMAC10::Precache( void )
{
	PRECACHE_MODEL( "models/v_mac10.mdl" );
	PRECACHE_MODEL( "models/w_mac10.mdl" );

	PRECACHE_SOUND( "weapons/mac10-1.wav" );
	PRECACHE_SOUND( "weapons/mac10_clipout.wav" );
	PRECACHE_SOUND( "weapons/mac10_clipin.wav" );
	PRECACHE_SOUND( "weapons/mac10_boltpull.wav" );

	m_iShell = PRECACHE_MODEL( "models/pshell.mdl" );// brass shell

	m_usFireMAC10 = PRECACHE_EVENT( 1, "events/mac10.sc" );
}

int CMAC10::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "45acp";
	p->iMaxAmmo1 = 100;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 30;
	p->iSlot = 0;
	p->iPosition = 13;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_MAC10;
	p->iWeight = 25;

	return 1;
}

BOOL CMAC10::Deploy( void )
{
	m_flAccuracy = 0.15f;
	iShellOn = 1;
	m_bDelayFire = false;

	return DefaultDeploy( "models/v_mac10.mdl", "models/p_mac10.mdl", MAC10_DRAW, "onehanded", UseDecrement() ? 1 : 0 );
}

void CMAC10::PrimaryAttack( void )
{
	if ( !FBitSet( m_pPlayer->pev->flags, FL_ONGROUND ) )
		MAC10Fire( 0.375 * m_flAccuracy, 0.07f, FALSE );
	else
		MAC10Fire( 0.03 * m_flAccuracy, 0.07f, FALSE );
}

void CMAC10::MAC10Fire( float flSpread, float flCycleTime, BOOL fUseAutoAim )
{
	m_iShotsFired++;

	m_bDelayFire = true;

	m_flAccuracy = ( m_iShotsFired * m_iShotsFired * m_iShotsFired / 200 ) + 0.6;
	if ( m_flAccuracy > 1.65 )
		m_flAccuracy = 1.65f;

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

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

	// non-silenced
	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = DIM_GUN_FLASH;

	Vector vecAiming;
	vecAiming = gpGlobals->v_forward;
	Vector vecSrc = m_pPlayer->GetGunPosition( );
	Vector vecDir;
	vecDir = m_pPlayer->FireBullets3( vecSrc, vecAiming, flSpread, 8192, 1, BULLET_PLAYER_45ACP, 29, 0.82f, m_pPlayer->pev, false, m_pPlayer->random_seed );

	int flag;
#if defined( CLIENT_WEAPONS )
	flag = FEV_NOTHOST;
#else
	flag = 0;
#endif

	PLAYBACK_EVENT_FULL( flag, m_pPlayer->edict(), m_usFireMAC10,
		0, (float *)&g_vecZero, (float *)&g_vecZero,
		vecDir.x,
		vecDir.y,
		(int)( m_pPlayer->pev->punchangle.x * 100 ),
		(int)( m_pPlayer->pev->punchangle.y * 100 ),
		0, 0 );

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay( flCycleTime );

	if ( !m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 );

	m_flTimeWeaponIdle = 2;

	if ( !FBitSet( m_pPlayer->pev->flags, FL_ONGROUND ) )	// jumping
		KickBack( 1.3f, 0.55f, 0.4f, 0.05f, 4.75f, 3.75f, 5 );
	else if ( m_pPlayer->pev->velocity.Length2D() > 0 )				// running
		KickBack( 0.9f, 0.45f, 0.25f, 0.035f, 3.5f, 2.75f, 7 );
	else if ( FBitSet( m_pPlayer->pev->flags, FL_DUCKING ) )	// ducking
		KickBack( 0.75f, 0.4f, 0.175f, 0.03f, 2.75f, 2.5f, 10 );
	else														// standing
		KickBack( 0.775f, 0.425f, 0.2f, 0.03f, 3, 2.75f, 9 );
}


void CMAC10::Reload( void )
{
	if ( m_pPlayer->ammo_45acp <= 0 )
		return;

	if ( DefaultReload( 30, MAC10_RELOAD, 3.15f ) )
	{
		m_pPlayer->SetAnimation( PLAYER_RELOAD );

		m_flAccuracy = 0;
		m_iShotsFired = 0;
	}
}

void CMAC10::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	if ( m_flTimeWeaponIdle > 0 )
		return;

	m_flTimeWeaponIdle = 20;

	SendWeaponAnim( MAC10_IDLE1, UseDecrement() ? 1 : 0 );
}
