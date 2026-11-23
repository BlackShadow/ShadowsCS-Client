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

enum flashbang_e
{
	FLASHBANG_IDLE = 0,
	FLASHBANG_PINPULL,
	FLASHBANG_THROW,
	FLASHBANG_DRAW,
};

LINK_ENTITY_TO_CLASS( weapon_flashbang, CFlashbang );

void CFlashbang::Spawn( void )
{
	Precache( );
	m_iId = WEAPON_FLASHBANG;
	SET_MODEL( ENT( pev ), "models/w_flashbang.mdl" );

	pev->dmg = 4;

	m_iDefaultAmmo = 1;

	m_flStartThrow = 0;
	m_flReleaseThrow = -1;

	m_iWeaponState &= ~WPNSTATE_SHIELD_DRAWN;

	FallInit();// get ready to fall down.
}


void CFlashbang::Precache( void )
{
	PRECACHE_MODEL( "models/v_flashbang.mdl" );
	PRECACHE_MODEL( "models/shield/v_shield_flashbang.mdl" );

	PRECACHE_SOUND( "weapons/flashbang-1.wav" );
	PRECACHE_SOUND( "weapons/flashbang-2.wav" );
	PRECACHE_SOUND( "weapons/pinpull.wav" );
}

int CFlashbang::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "Flashbang";
	p->iMaxAmmo1 = 2;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 3;
	p->iPosition = 2;
	p->iId = m_iId = WEAPON_FLASHBANG;
	p->iWeight = 1;
	p->iFlags = ITEM_FLAG_LIMITINWORLD | ITEM_FLAG_EXHAUSTIBLE;

	return 1;
}


BOOL CFlashbang::Deploy( void )
{
	m_flReleaseThrow = -1;
	m_fMaxSpeed = 250;

	m_iWeaponState &= ~WPNSTATE_SHIELD_DRAWN;

	m_pPlayer->m_bShieldDrawn = false;

	if ( m_pPlayer->HasShield() )
		return DefaultDeploy( "models/shield/v_shield_flashbang.mdl", "models/shield/p_shield_flashbang.mdl", FLASHBANG_DRAW, "shieldgren", UseDecrement() ? 1 : 0 );
	else
		return DefaultDeploy( "models/v_flashbang.mdl", "models/p_flashbang.mdl", FLASHBANG_DRAW, "grenade", UseDecrement() ? 1 : 0 );
}

void CFlashbang::Holster( int skiplocal /* = 0 */ )
{
	m_pPlayer->m_flNextAttack = 0.5f;
	if ( !m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
	{
		m_pPlayer->pev->weapons &= ~( 1 << WEAPON_FLASHBANG );
		DestroyItem();
	}

	m_flStartThrow = 0;
	m_flReleaseThrow = -1;
}


void CFlashbang::PrimaryAttack( void )
{
	if ( m_iWeaponState & WPNSTATE_SHIELD_DRAWN )
		return;

	if ( !m_flStartThrow && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] > 0 )
	{
		m_flStartThrow = gpGlobals->time;
		m_flReleaseThrow = 0;

		SendWeaponAnim( FLASHBANG_PINPULL, UseDecrement() ? 1 : 0 );
		m_flTimeWeaponIdle = 0.5f;
	}
}

bool CFlashbang::ShieldSecondaryFire( int iUpAnim, int iDownAnim )
{
	if ( m_pPlayer->HasShield() == false )
		return false;

	if ( m_flStartThrow > 0 )
		return false;

	if ( !( m_iWeaponState & WPNSTATE_SHIELD_DRAWN ) )
	{
		m_iWeaponState |= WPNSTATE_SHIELD_DRAWN;

		SendWeaponAnim( iUpAnim, UseDecrement() ? 1 : 0 );
		strcpy( m_pPlayer->m_szAnimExtention, "shielded" );
		m_fMaxSpeed = 180;
		m_pPlayer->m_bShieldDrawn = true;
	}
	else
	{
		m_iWeaponState &= ~WPNSTATE_SHIELD_DRAWN;

		SendWeaponAnim( iDownAnim, UseDecrement() ? 1 : 0 );
		strcpy( m_pPlayer->m_szAnimExtention, "shieldgren" );
		m_fMaxSpeed = 250;
		m_pPlayer->m_bShieldDrawn = false;
	}

	m_pPlayer->UpdateShieldCrosshair( !( m_iWeaponState & WPNSTATE_SHIELD_DRAWN ) );
	m_pPlayer->ResetMaxSpeed();
	m_flNextSecondaryAttack = 0.4f;
	m_flNextPrimaryAttack = GetNextAttackDelay( 0.4f );
	m_flTimeWeaponIdle = 0.6f;

	return true;
}

void CFlashbang::SecondaryAttack( void )
{
	ShieldSecondaryFire( GRENADE_SHIELD_UP, GRENADE_SHIELD_DOWN );
}

void CFlashbang::SetPlayerShieldAnim( void )
{
	if ( m_pPlayer->HasShield() )
	{
		if ( m_iWeaponState & WPNSTATE_SHIELD_DRAWN )
			strcpy( m_pPlayer->m_szAnimExtention, "shield" );
		else
			strcpy( m_pPlayer->m_szAnimExtention, "shieldgren" );
	}
}

void CFlashbang::ResetPlayerShieldAnim( void )
{
	if ( m_pPlayer->HasShield() )
	{
		if ( m_iWeaponState & WPNSTATE_SHIELD_DRAWN )
			strcpy( m_pPlayer->m_szAnimExtention, "shieldgren" );
	}
}

void CFlashbang::WeaponIdle( void )
{
	if ( m_flReleaseThrow == 0 && m_flStartThrow )
		m_flReleaseThrow = gpGlobals->time;

	if ( m_flTimeWeaponIdle > 0 )
		return;

	if ( m_flStartThrow )
	{
		m_pPlayer->Radio( "%!MRAD_FIREINHOLE", "#Fire_in_the_hole", 100, true );

		Vector angThrow = m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle;

		if ( angThrow.x < 0 )
			angThrow.x = angThrow.x * ( ( 90.0 - 10.0 ) / 90.0 ) - 10.0;
		else
			angThrow.x = angThrow.x * ( ( 90.0 + 10.0 ) / 90.0 ) - 10.0;

		float flVel = ( 90 - angThrow.x ) * 6;
		if ( flVel > 750 )
			flVel = 750;

		UTIL_MakeVectors( angThrow );

		Vector vecSrc = m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs + gpGlobals->v_forward * 16;

		Vector vecThrow = gpGlobals->v_forward * flVel + m_pPlayer->pev->velocity;

		CGrenade::ShootTimed( m_pPlayer->pev, vecSrc, vecThrow, 1.5f );

		SendWeaponAnim( FLASHBANG_THROW, UseDecrement() ? 1 : 0 );

		SetPlayerShieldAnim();

		// player "shoot" animation
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

		m_flStartThrow = 0;
		m_flNextPrimaryAttack = GetNextAttackDelay( 0.5f );
		m_flTimeWeaponIdle = 0.75f;

		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;

		if ( !m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
		{
			m_flTimeWeaponIdle = m_flNextSecondaryAttack = m_flNextPrimaryAttack = GetNextAttackDelay( 0.5f );
		}

		ResetPlayerShieldAnim();
		return;
	}
	else if ( m_flReleaseThrow > 0 )
	{
		m_flStartThrow = 0;

		RetireWeapon();
		return;
	}

	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
	{
		int iAnim;
		float flRand = RANDOM_FLOAT( 0, 1 );

		if ( m_pPlayer->HasShield() )
		{
			m_flTimeWeaponIdle = 20;

			if ( m_iWeaponState & WPNSTATE_SHIELD_DRAWN )
				SendWeaponAnim( GRENADE_SHIELD_IDLE, UseDecrement() ? 1 : 0 );
		}
		else
		{
			if ( flRand <= 0.75 )
			{
				iAnim = FLASHBANG_IDLE;
				m_flTimeWeaponIdle = RANDOM_FLOAT( 10, 15 );
			}
			else
			{
				iAnim = FLASHBANG_IDLE;
				m_flTimeWeaponIdle = 75.0 / 30.0;
			}

			SendWeaponAnim( iAnim, UseDecrement() ? 1 : 0 );
		}
	}
}


BOOL CFlashbang::CanDeploy( void )
{
	return m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] != 0;
}
