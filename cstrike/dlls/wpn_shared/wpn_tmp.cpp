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

enum tmp_e
{
	TMP_IDLE1 = 0,
	TMP_RELOAD,
	TMP_DRAW,
	TMP_SHOOT1,
	TMP_SHOOT2,
	TMP_SHOOT3,
};

LINK_ENTITY_TO_CLASS( weapon_tmp, CTMP );

void CTMP::Spawn( void )
{
	Precache( );
	m_iId = WEAPON_TMP;
	SET_MODEL( ENT( pev ), "models/w_tmp.mdl" );

	m_iDefaultAmmo = 30;
	m_flAccuracy = 0.2f;
	m_iShotsFired = 0;
	m_bDelayFire = false;

	FallInit();// get ready to fall down.
}


void CTMP::Precache( void )
{
	PRECACHE_MODEL( "models/v_tmp.mdl" );
	PRECACHE_MODEL( "models/w_tmp.mdl" );

	PRECACHE_SOUND( "weapons/tmp-1.wav" );
	PRECACHE_SOUND( "weapons/tmp-2.wav" );

	m_iShell = PRECACHE_MODEL( "models/pshell.mdl" );// brass shell

	m_usFireTMP = PRECACHE_EVENT( 1, "events/tmp.sc" );
}

int CTMP::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "9mm";
	p->iMaxAmmo1 = 120;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 30;
	p->iSlot = 0;
	p->iPosition = 11;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_TMP;
	p->iWeight = 25;

	return 1;
}

BOOL CTMP::Deploy( void )
{
	m_flAccuracy = 0.2f;
	m_iShotsFired = 0;
	iShellOn = 1;
	m_bDelayFire = false;

	return DefaultDeploy( "models/v_tmp.mdl", "models/p_tmp.mdl", TMP_DRAW, "onehanded", UseDecrement() ? 1 : 0 );
}

void CTMP::PrimaryAttack( void )
{
	if ( !FBitSet( m_pPlayer->pev->flags, FL_ONGROUND ) )
		TMPFire( 0.25 * m_flAccuracy, 0.07f, FALSE );
	else
		TMPFire( 0.03 * m_flAccuracy, 0.07f, FALSE );
}

void CTMP::TMPFire( float flSpread, float flCycleTime, BOOL fUseAutoAim )
{
	m_iShotsFired++;

	m_bDelayFire = true;

	m_flAccuracy = ( m_iShotsFired * m_iShotsFired * m_iShotsFired / 200 ) + 0.55;
	if ( m_flAccuracy > 1.4 )
		m_flAccuracy = 1.4f;

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

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;

	Vector vecAiming;
	vecAiming = gpGlobals->v_forward;
	Vector vecSrc = m_pPlayer->GetGunPosition( );

	Vector vecDir;
	vecDir = m_pPlayer->FireBullets3( vecSrc, vecAiming, flSpread, 8192, 1, BULLET_PLAYER_9MM, 20, 0.85f, m_pPlayer->pev, false, m_pPlayer->random_seed );

	int flag;
#if defined( CLIENT_WEAPONS )
	flag = FEV_NOTHOST;
#else
	flag = 0;
#endif
	PLAYBACK_EVENT_FULL( flag, m_pPlayer->edict(), m_usFireTMP,
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

	m_flTimeWeaponIdle = 2;

	if ( !FBitSet( m_pPlayer->pev->flags, FL_ONGROUND ) )
		KickBack( 1.1f, 0.5f, 0.35f, 0.045f, 4.5f, 3.5f, 6 );
	else if ( m_pPlayer->pev->velocity.Length2D() > 0 )
		KickBack( 0.8f, 0.4f, 0.2f, 0.03f, 3, 2.5f, 7 );
	else if ( FBitSet( m_pPlayer->pev->flags, FL_DUCKING ) )
		KickBack( 0.7f, 0.35f, 0.125f, 0.025f, 2.5f, 2, 10 );
	else
		KickBack( 0.725f, 0.375f, 0.15f, 0.025f, 2.75f, 2.25f, 9 );
}


void CTMP::Reload( void )
{
	if ( m_pPlayer->ammo_9mm <= 0 )
		return;

	if ( DefaultReload( 30, TMP_RELOAD, 2.12f ) )
	{
		m_pPlayer->SetAnimation( PLAYER_RELOAD );

		m_flAccuracy = 0.2f;
		m_iShotsFired = 0;
	}
}

void CTMP::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	if ( m_flTimeWeaponIdle > 0 )
		return;

	m_flTimeWeaponIdle = 20;

	SendWeaponAnim( TMP_IDLE1, UseDecrement() ? 1 : 0 );
}
