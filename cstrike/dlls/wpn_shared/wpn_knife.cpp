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

#define	KNIFE_BODYHIT_VOLUME 128
#define	KNIFE_WALLHIT_VOLUME 512

LINK_ENTITY_TO_CLASS( weapon_knife, CKnife );

enum knife_e
{
	KNIFE_IDLE = 0,
	KNIFE_ATTACK1HIT,
	KNIFE_ATTACK2HIT,
	KNIFE_DRAW,
	KNIFE_STABHIT,
	KNIFE_STABMISS,
	KNIFE_MIDATTACK1HIT,
	KNIFE_MIDATTACK2HIT,
};

enum knife_shield_e
{
	KNIFE_SHIELD_IDLE = 0,
	KNIFE_SHIELD_SLASH,
	KNIFE_SHIELD_ATTACKHIT,
	KNIFE_SHIELD_DRAW,
	KNIFE_SHIELD_UPIDLE,
	KNIFE_SHIELD_UP,
	KNIFE_SHIELD_DOWN,
};


void CKnife::Spawn( void )
{
	Precache( );
	m_iId = WEAPON_KNIFE;
	SET_MODEL( ENT( pev ), "models/w_knife.mdl" );
	m_iClip = WEAPON_NOCLIP;

	m_iWeaponState &= ~WPNSTATE_SHIELD_DRAWN;

	FallInit();// get ready to fall down.
}


void CKnife::Precache( void )
{
	PRECACHE_MODEL( "models/v_knife.mdl" );
	PRECACHE_MODEL( "models/shield/v_shield_knife.mdl" );
	PRECACHE_MODEL( "models/w_knife.mdl" );

	PRECACHE_SOUND( "weapons/knife_deploy1.wav" );
	PRECACHE_SOUND( "weapons/knife_hit1.wav" );
	PRECACHE_SOUND( "weapons/knife_hit2.wav" );
	PRECACHE_SOUND( "weapons/knife_hit3.wav" );
	PRECACHE_SOUND( "weapons/knife_hit4.wav" );
	PRECACHE_SOUND( "weapons/knife_slash1.wav" );
	PRECACHE_SOUND( "weapons/knife_slash2.wav" );
	PRECACHE_SOUND( "weapons/knife_stab.wav" );
	PRECACHE_SOUND( "weapons/knife_hitwall1.wav" );

	m_usKnife = PRECACHE_EVENT( 1, "events/knife.sc" );
}

int CKnife::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 2;
	p->iPosition = 1;
	p->iId = WEAPON_KNIFE;
	p->iWeight = 0;

	return 1;
}


BOOL CKnife::Deploy( void )
{
	EMIT_SOUND( m_pPlayer->edict(), CHAN_ITEM, "weapons/knife_deploy1.wav", 0.3f, 2.4f );

	m_iSwing = 0;
	m_fMaxSpeed = 250;

	m_iWeaponState &= ~WPNSTATE_SHIELD_DRAWN;

	m_pPlayer->m_bShieldDrawn = false;

	if ( m_pPlayer->HasShield() )
		return DefaultDeploy( "models/shield/v_shield_knife.mdl", "models/shield/p_shield_knife.mdl", KNIFE_SHIELD_DRAW, "shieldknife", UseDecrement() ? 1 : 0 );
	else
		return DefaultDeploy( "models/v_knife.mdl", "models/p_knife.mdl", KNIFE_DRAW, "knife", UseDecrement() ? 1 : 0 );
}

void CKnife::Holster( int skiplocal /* = 0 */ )
{
	m_pPlayer->m_flNextAttack = 0.5f;
}

void CKnife::WeaponAnimation( int iAnimation )
{
	int flag;
#if defined( CLIENT_WEAPONS )
	flag = FEV_NOTHOST;
#else
	flag = 0;
#endif

	PLAYBACK_EVENT_FULL( flag, m_pPlayer->edict(), m_usKnife,
		0, (float *)&g_vecZero, (float *)&g_vecZero,
		0,
		0,
		iAnimation, 2, 3, 4 );
}

void FindHullIntersection( const Vector &vecSrc, TraceResult &tr, float *mins, float *maxs, edict_t *pEntity )
{
	int i, j, k;
	float distance;
	float *minmaxs[2] = { mins, maxs };
	TraceResult tmpTrace;
	Vector vecHullEnd = tr.vecEndPos;
	Vector vecEnd;

	distance = 1e6f;

	vecHullEnd = vecSrc + ( ( vecHullEnd - vecSrc ) * 2 );
	UTIL_TraceLine( vecSrc, vecHullEnd, dont_ignore_monsters, pEntity, &tmpTrace );
	if ( tmpTrace.flFraction < 1.0 )
	{
		tr = tmpTrace;
		return;
	}

	for ( i = 0; i < 2; i++ )
	{
		for ( j = 0; j < 2; j++ )
		{
			for ( k = 0; k < 2; k++ )
			{
				vecEnd.x = vecHullEnd.x + minmaxs[i][0];
				vecEnd.y = vecHullEnd.y + minmaxs[j][1];
				vecEnd.z = vecHullEnd.z + minmaxs[k][2];

				UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, pEntity, &tmpTrace );
				if ( tmpTrace.flFraction < 1.0 )
				{
					float thisDistance = ( tmpTrace.vecEndPos - vecSrc ).Length();
					if ( thisDistance < distance )
					{
						tr = tmpTrace;
						distance = thisDistance;
					}
				}
			}
		}
	}
}


void CKnife::PrimaryAttack( void )
{
	Swing( 1 );
}

void CKnife::SetPlayerShieldAnim( void )
{
	if ( m_pPlayer->HasShield() )
	{
		if ( m_iWeaponState & WPNSTATE_SHIELD_DRAWN )
			strcpy( m_pPlayer->m_szAnimExtention, "shield" );
		else
			strcpy( m_pPlayer->m_szAnimExtention, "shieldknife" );
	}
}

void CKnife::ResetPlayerShieldAnim( void )
{
	if ( m_pPlayer->HasShield() )
	{
		if ( m_iWeaponState & WPNSTATE_SHIELD_DRAWN )
			strcpy( m_pPlayer->m_szAnimExtention, "shieldknife" );
	}
}

bool CKnife::ShieldSecondaryFire( int iUpAnim, int iDownAnim )
{
	if ( m_pPlayer->HasShield() == false )
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
		strcpy( m_pPlayer->m_szAnimExtention, "shieldknife" );
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

void CKnife::SecondaryAttack( void )
{
	if ( ShieldSecondaryFire( KNIFE_SHIELD_UP, KNIFE_SHIELD_DOWN ) )
		return;

	Stab( 1 );
	pev->nextthink = 0.35f;
}

void CKnife::Smack( void )
{
	DecalGunshot( &m_trHit, BULLET_PLAYER_CROWBAR, false, m_pPlayer->pev, false );
}


void CKnife::SwingAgain( void )
{
	Swing( 0 );
}

void CKnife::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	if ( m_flTimeWeaponIdle > 0 )
		return;

	if ( m_pPlayer->m_bShieldDrawn )
		return;

	m_flTimeWeaponIdle = 20;

	// only idle if the slid isn't back
	SendWeaponAnim( KNIFE_IDLE, UseDecrement() ? 1 : 0 );
}

int CKnife::Swing( int fFirst )
{
	int fDidHit = FALSE;

	TraceResult tr;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle );
	Vector vecSrc = m_pPlayer->GetGunPosition( );
	Vector vecEnd = vecSrc + gpGlobals->v_forward * 48;

	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, m_pPlayer->edict(), &tr );

	if ( tr.flFraction >= 1.0 )
	{
		UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, head_hull, m_pPlayer->edict(), &tr );
		if ( tr.flFraction < 1.0 )
		{
			// Calculate the point of intersection of the line (or hull) and the object we hit
			// This is and approximation of the "best" intersection
			CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
			if ( !pHit || pHit->IsBSPModel() )
				FindHullIntersection( vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, m_pPlayer->edict() );
			vecEnd = tr.vecEndPos;	// This is the point on the actual surface (the hull could have hit space)
		}
	}

	if ( tr.flFraction >= 1.0 )
	{
		if ( fFirst )
		{
			if ( m_pPlayer->HasShield() == false )
			{
				switch ( ( m_iSwing++ ) % 2 )
				{
				case 0:
					SendWeaponAnim( KNIFE_MIDATTACK1HIT, UseDecrement() ? 1 : 0 );
					break;
				case 1:
					SendWeaponAnim( KNIFE_MIDATTACK2HIT, UseDecrement() ? 1 : 0 );
					break;
				}

				m_flNextPrimaryAttack = GetNextAttackDelay( 0.35f );
				m_flNextSecondaryAttack = 0.5f;
			}
			else
			{
				SendWeaponAnim( KNIFE_SHIELD_ATTACKHIT, UseDecrement() ? 1 : 0 );

				m_flNextPrimaryAttack = GetNextAttackDelay( 1 );
				m_flNextSecondaryAttack = 1.2f;
			}

			m_flTimeWeaponIdle = 2;

			// play wiff or swish sound
			if ( RANDOM_LONG( 0, 1 ) )
				EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_WEAPON, "weapons/knife_slash1.wav", 1, ATTN_NORM, 0, 94 );
			else
				EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_WEAPON, "weapons/knife_slash2.wav", 1, ATTN_NORM, 0, 94 );

			// player "shoot" animation
			m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
		}
	}
	else
	{
		// hit
		fDidHit = TRUE;

		if ( m_pPlayer->HasShield() == false )
		{
			switch ( ( m_iSwing++ ) % 2 )
			{
			case 0:
				SendWeaponAnim( KNIFE_MIDATTACK1HIT, UseDecrement() ? 1 : 0 );
				break;
			case 1:
				SendWeaponAnim( KNIFE_MIDATTACK2HIT, UseDecrement() ? 1 : 0 );
				break;
			}

			m_flNextPrimaryAttack = GetNextAttackDelay( 0.4f );
			m_flNextSecondaryAttack = 0.5f;
		}
		else
		{
			SendWeaponAnim( KNIFE_SHIELD_ATTACKHIT, UseDecrement() ? 1 : 0 );
			m_flNextPrimaryAttack = GetNextAttackDelay( 1 );
			m_flNextSecondaryAttack = 1.2f;
		}

		m_flTimeWeaponIdle = 2;

		// play thwack, smack, or dong sound
		float flVol = 1.0;

#ifndef CLIENT_DLL
		int fHitWorld = TRUE;

		CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );

		SetPlayerShieldAnim();

		// player "shoot" animation
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

		ClearMultiDamage( );
		if ( ( m_flNextPrimaryAttack + 0.4 < 0 ) )
		{
			// first swing does full damage
			pEntity->TraceAttack( m_pPlayer->pev, 20, gpGlobals->v_forward, &tr, DMG_BULLET | DMG_NEVERGIB );
		}
		else
		{
			// subsequent swings do less
			pEntity->TraceAttack( m_pPlayer->pev, 15, gpGlobals->v_forward, &tr, DMG_BULLET | DMG_NEVERGIB );
		}
		ApplyMultiDamage( m_pPlayer->pev, m_pPlayer->pev );

		if ( pEntity )
		{
			if ( pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE )
			{
				switch ( RANDOM_LONG( 0, 3 ) )
				{
				case 0:
					EMIT_SOUND( m_pPlayer->edict(), CHAN_WEAPON, "weapons/knife_hit1.wav", 1, ATTN_NORM ); break;
				case 1:
					EMIT_SOUND( m_pPlayer->edict(), CHAN_WEAPON, "weapons/knife_hit2.wav", 1, ATTN_NORM ); break;
				case 2:
					EMIT_SOUND( m_pPlayer->edict(), CHAN_WEAPON, "weapons/knife_hit3.wav", 1, ATTN_NORM ); break;
				case 3:
					EMIT_SOUND( m_pPlayer->edict(), CHAN_WEAPON, "weapons/knife_hit4.wav", 1, ATTN_NORM ); break;
				}
				m_pPlayer->m_iWeaponVolume = KNIFE_BODYHIT_VOLUME;
				if ( !pEntity->IsAlive() )
					return TRUE;
				else
					flVol = 0.1;

				fHitWorld = FALSE;
			}
		}

		// play texture hit sound
		// UNDONE: Calculate the correct point of intersection when we hit with the hull instead of the line

		if ( fHitWorld )
		{
			float fvolbar = TEXTURETYPE_PlaySound( &tr, vecSrc, vecSrc + ( vecEnd - vecSrc ) * 2, BULLET_PLAYER_CROWBAR );
			fvolbar = 1;

			switch ( RANDOM_LONG( 0, 1 ) )
			{
			case 0:
				EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_ITEM, "weapons/knife_hitwall1.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG( 0, 3 ) );
				break;
			case 1:
				EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_ITEM, "weapons/knife_hitwall1.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG( 0, 3 ) );
				break;
			}
		}
#endif

		// delay the decal a bit
		m_trHit = tr;
		SetThink( &CKnife::Smack );
		pev->nextthink = 0.2f;

		m_pPlayer->m_iWeaponVolume = flVol * KNIFE_WALLHIT_VOLUME;

		ResetPlayerShieldAnim();
	}
	return fDidHit;
}


int CKnife::Stab( int fFirst )
{
	int fDidHit = FALSE;

	TraceResult tr;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle );
	Vector vecSrc = m_pPlayer->GetGunPosition( );
	Vector vecEnd = vecSrc + gpGlobals->v_forward * 32;

	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, m_pPlayer->edict(), &tr );

	if ( tr.flFraction >= 1.0 )
	{
		UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, head_hull, m_pPlayer->edict(), &tr );
		if ( tr.flFraction < 1.0 )
		{
			// Calculate the point of intersection of the line (or hull) and the object we hit
			// This is and approximation of the "best" intersection
			CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
			if ( !pHit || pHit->IsBSPModel() )
				FindHullIntersection( vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, m_pPlayer->edict() );
			vecEnd = tr.vecEndPos;	// This is the point on the actual surface (the hull could have hit space)
		}
	}

	if ( tr.flFraction >= 1.0 )
	{
		if ( fFirst )
		{
			SendWeaponAnim( KNIFE_STABMISS, UseDecrement() ? 1 : 0 );

			m_flNextSecondaryAttack = 1;
			m_flNextPrimaryAttack = GetNextAttackDelay( 1 );
			// play wiff or swish sound
			if ( RANDOM_LONG( 0, 1 ) )
				EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_WEAPON, "weapons/knife_slash1.wav", 1, ATTN_NORM, 0, 94 );
			else
				EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_WEAPON, "weapons/knife_slash2.wav", 1, ATTN_NORM, 0, 94 );

			// player "shoot" animation
			m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
		}
	}
	else
	{
		// hit
		fDidHit = TRUE;
		SendWeaponAnim( KNIFE_STABHIT, UseDecrement() ? 1 : 0 );
		m_flNextSecondaryAttack = 1.1f;
		m_flNextPrimaryAttack = GetNextAttackDelay( 1.1f );

		// play thwack, smack, or dong sound
		float flVol = 1.0;

#ifndef CLIENT_DLL
		int fHitWorld = TRUE;

		CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );

		// player "shoot" animation
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

		float flDamage = 65.0f;

		if ( pEntity && pEntity->IsPlayer() )
		{
			Vector2D vec2LOS;
			float flDot;
			Vector vMyForward = gpGlobals->v_forward;

			UTIL_MakeVectors( pEntity->pev->angles );

			vec2LOS = vMyForward.Make2D();
			vec2LOS = vec2LOS.Normalize();

			flDot = DotProduct( vec2LOS, gpGlobals->v_forward.Make2D() );

			//Triple the damage if we are stabbing them in the back.
			if ( flDot > 0.80f )
				flDamage *= 3;
		}

		UTIL_MakeVectors( m_pPlayer->pev->v_angle );

		ClearMultiDamage( );
		pEntity->TraceAttack( m_pPlayer->pev, flDamage, gpGlobals->v_forward, &tr, DMG_BULLET | DMG_NEVERGIB );
		ApplyMultiDamage( m_pPlayer->pev, m_pPlayer->pev );

		if ( pEntity )
		{
			if ( pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE )
			{
				EMIT_SOUND( m_pPlayer->edict(), CHAN_WEAPON, "weapons/knife_stab.wav", 1, ATTN_NORM );

				m_pPlayer->m_iWeaponVolume = KNIFE_BODYHIT_VOLUME;
				if ( !pEntity->IsAlive() )
					return TRUE;
				else
					flVol = 0.1;

				fHitWorld = FALSE;
			}
		}

		// play texture hit sound
		// UNDONE: Calculate the correct point of intersection when we hit with the hull instead of the line

		if ( fHitWorld )
		{
			float fvolbar = TEXTURETYPE_PlaySound( &tr, vecSrc, vecSrc + ( vecEnd - vecSrc ) * 2, BULLET_PLAYER_CROWBAR );
			fvolbar = 1;

			// also play knife strike

			switch ( RANDOM_LONG( 0, 1 ) )
			{
			case 0:
				EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_ITEM, "weapons/knife_hitwall1.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG( 0, 3 ) );
				break;
			case 1:
				EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_ITEM, "weapons/knife_hitwall1.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG( 0, 3 ) );
				break;
			}
		}
#endif

		// delay the decal a bit
		m_trHit = tr;
		SetThink( &CKnife::Smack );
		pev->nextthink = 0.2f;

		m_pPlayer->m_iWeaponVolume = flVol * KNIFE_WALLHIT_VOLUME;
	}
	return fDidHit;
}
