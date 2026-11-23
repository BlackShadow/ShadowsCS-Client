//========= Copyright (c) 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "game.h"
#include "client.h"
#include "cs_bot.h"
#include "bot_profile.h"
#include <algorithm>

//--------------------------------------------------------------------------------------------------------------
/**
 * Fire our active weapon towards our current enemy
 * NOTE: Aiming our weapon is handled in RunBotUpkeep()
 */
void CCSBot::FireWeaponAtEnemy( void )
{
	CBasePlayer *enemy = static_cast<CBasePlayer *>(static_cast<CBaseEntity *>(m_enemy));
	if ( enemy == NULL )
	{
		m_isRapidFiring = false;
		return;
	}

	if ( IsUsingSniperRifle() && pev->velocity.LengthSquared() >= 100.0f )
		return;

	if ( gpGlobals->time <= m_fireWeaponTimestamp )
		return;

	float elapsed = gpGlobals->time - m_currentEnemyAcquireTimestamp;
	if ( elapsed < GetProfile()->GetAttackDelay() )
		return;

	float surpriseDelay = gpGlobals->time - m_lastPanicTime >= 5.0f ? 0.0f : m_surpriseDelay;
	if ( elapsed < surpriseDelay )
		return;

	m_surpriseDelay = 0.0f;
	m_lastPanicTime = 0.0f;

	if ( IsRecognizedEnemyProtectedByShield() && IsPlayerFacingMe( enemy ) )
		return;

	if ( IsActiveWeaponReloading() || IsActiveWeaponClipEmpty() || !m_isEnemyVisible )
		return;

	// we have a clear shot - pull trigger if we are aiming at enemy
	Vector2D toAimSpot = (m_aimSpot - pev->origin).Make2D();
	float rangeToEnemy = toAimSpot.NormalizeInPlace();

	// get actual view direction vector
	double yaw = pev->v_angle.y * (M_PI / 180.0);
	Vector2D aimDir( static_cast<float>(cos( yaw )), static_cast<float>(sin( yaw )) );

	// aim more precisely with a sniper rifle
	// because rifles' bullets spray, dont have to be very precise
	const float halfSize = (IsUsingSniperRifle()) ? HalfHumanWidth : 2.0f * HalfHumanWidth;

	// aiming tolerance depends on how close the target is - closer targets subtend larger angles
	float aimTolerance = static_cast<float>(cos( atan( static_cast<double>(halfSize / rangeToEnemy) ) ));

	if ( DotProduct( aimDir, toAimSpot ) <= aimTolerance )
		return;

	// if friendly fire is on, don't fire if a teammate is blocking our line of fire
	if ( friendlyfire.value <= 0.0f || !IsFriendInLineOfFire() )
	{
		// if we are using a knife, only swing it if we're close
		if ( IsUsingKnife() )
		{
			const float knifeRange = 75.0f;		// 50
			if ( rangeToEnemy < knifeRange )
			{
				// since we've given ourselves away - run!
				Run();
				m_mustRunTimer.Start( 5.0f );

				// if our prey is facing away, backstab him!
				if ( !IsPlayerFacingMe( enemy ) )
				{
					SecondaryAttack();
				}
				else
				{
					// randomly choose primary and secondary attacks with knife
					const float knifeStabChance = 33.3f;
					if ( RANDOM_FLOAT( 0.0f, 100.0f ) < knifeStabChance )
						SecondaryAttack();
					else
						PrimaryAttack();
				}
			}
		}
		else
		{
			PrimaryAttack();
		}
	}

	if ( IsUsingPistol() )
	{
		// high-skill bots fire their pistols quickly at close range
		const float closePistolRange = 999999.9f;
		if ( GetProfile()->GetSkill() > 0.75f && rangeToEnemy < closePistolRange )
		{
			// fire as fast as possible
			m_fireWeaponTimestamp = 0.0f;
			m_isRapidFiring = true;
		}
		else
		{
			// fire somewhat quickly
			m_fireWeaponTimestamp = RANDOM_FLOAT( 0.15f, 0.4f );
		}
	}
	else	// not using a pistol
	{
		const float sprayRange = 400.0f;
		if ( GetProfile()->GetSkill() < 0.5f || rangeToEnemy < sprayRange || IsUsingMachinegun() )
		{
			// spray 'n pray if enemy is close, or we're not that good, or we're using the big machinegun
			m_fireWeaponTimestamp = 0.0f;
		}
		else
		{
			const float distantTargetRange = 800.0f;
			if ( !IsUsingSniperRifle() && rangeToEnemy > distantTargetRange )
			{
				// if very far away, fire slowly for better accuracy
				m_fireWeaponTimestamp = RANDOM_FLOAT( 0.3f, 0.7f );
			}
			else
			{
				// fire short bursts for accuracy
				m_fireWeaponTimestamp = RANDOM_FLOAT( 0.15f, 0.5f );
			}
		}
	}

	// subtract system latency
	m_fireWeaponTimestamp -= g_flBotFullThinkInterval;

	m_fireWeaponTimestamp += gpGlobals->time;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Set the current aim offset using given accuracy (1.0 = perfect aim, 0.0f = terrible aim)
 */
void CCSBot::SetAimOffset( float accuracy )
{
	// if our accuracy is less than perfect, it will improve as we "focus in" while not rotating our view
	if ( accuracy < 1.0f )
	{
		// if we moved our view, reset our "focus" mechanism
		if ( m_lookYawVel >= 100.0f || m_lookYawVel <= -100.0f ||
			m_lookPitchVel >= 100.0f || m_lookPitchVel <= -100.0f )
			m_aimSpreadTimestamp = gpGlobals->time;

		// focusTime is the time it takes for a bot to "focus in" for very good aim, from 2 to 5 seconds
		const float focusTime = std::max( 2.0f, (1.0f - accuracy) * 5.0f );
		float focusInterval = gpGlobals->time - m_aimSpreadTimestamp;

		float focusAccuracy = focusInterval / focusTime;

		// limit how much "focus" will help
		const float maxFocusAccuracy = 0.75f;
		if ( focusAccuracy > maxFocusAccuracy )
			focusAccuracy = maxFocusAccuracy;

		accuracy = std::max( accuracy, focusAccuracy );
	}

	PrintIfWatched( "Accuracy = %4.3f\n", accuracy );

	// aim error increases with distance, such that actual crosshair error stays about the same
	float range = (m_lastEnemyPosition - pev->origin).Length();
	float error = range * (static_cast<float>(m_iFOV) / 90.0f * 0.1f) * (1.0f - accuracy);

	m_aimOffsetGoal.x = RANDOM_FLOAT( -error, error );
	m_aimOffsetGoal.y = RANDOM_FLOAT( -error, error );
	m_aimOffsetGoal.z = RANDOM_FLOAT( -error, error );

	// define time when aim offset will automatically be updated
	m_aimOffsetTimestamp = gpGlobals->time + RANDOM_FLOAT( 0.25f, 1.0f );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Wiggle aim error based on GetProfile()->GetSkill()
 */
void CCSBot::UpdateAimOffset( void )
{
	if ( gpGlobals->time >= m_aimOffsetTimestamp )
	{
		SetAimOffset( GetProfile()->GetSkill() );
	}

	// move current offset towards goal offset
	Vector d = m_aimOffsetGoal - m_aimOffset;
	const float stiffness = 0.1f;
	m_aimOffset.x += stiffness * d.x;
	m_aimOffset.y += stiffness * d.y;
	m_aimOffset.z += stiffness * d.z;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Change our zoom level to be appropriate for the given range.
 * Return true if the zoom level changed.
 */
bool CCSBot::AdjustZoom( float range )
{
	bool adjustZoom = false;

	if ( IsUsingSniperRifle() )
	{
		const float sniperFarZoomRange = 1500.0f;

		// if range is too close, don't zoom
		if ( range <= 300.0f )
		{
			// zoom out
			if ( m_iFOV <= 60 )
			{
				adjustZoom = true;
			}
		}
		else if ( range < sniperFarZoomRange )
		{
			// maintain low zoom
			if ( m_iFOV > 60 || m_iFOV <= 25 )
			{
				adjustZoom = true;
			}
		}
		else
		{
			// maintain high zoom
			if ( m_iFOV > 25 )
			{
				adjustZoom = true;
			}
		}
	}
	else
	{
		// zoom out
		if ( m_iFOV <= 60 )
		{
			adjustZoom = true;
		}
	}

	if ( adjustZoom )
	{
		SecondaryAttack();
	}

	return adjustZoom;
}

//--------------------------------------------------------------------------------------------------------------
bool CCSBot::IsUsingAWP( void ) const
{
	return m_pActiveItem && m_pActiveItem->m_iId == WEAPON_AWP;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Returns true if we are using a weapon with a removable silencer
 */
bool CCSBot::DoesActiveWeaponHaveSilencer( void ) const
{
	return m_pActiveItem && (m_pActiveItem->m_iId == WEAPON_M4A1 || m_pActiveItem->m_iId == WEAPON_USP);
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we are using a sniper rifle
 */
bool CCSBot::IsUsingSniperRifle( void ) const
{
	if ( m_pActiveItem == NULL )
		return false;

	switch ( m_pActiveItem->m_iId )
	{
	case WEAPON_SCOUT:
	case WEAPON_SG550:
	case WEAPON_AWP:
	case WEAPON_G3SG1:
		return true;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we have a sniper rifle in our inventory
 */
bool CCSBot::IsSniper( void ) const
{
	for ( int slot = 0; slot < MAX_ITEM_TYPES; ++slot )
	{
		for ( CBasePlayerItem *item = m_rgpPlayerItems[slot]; item; item = item->m_pNext )
		{
			switch ( item->m_iId )
			{
			case WEAPON_SCOUT:
			case WEAPON_SG550:
			case WEAPON_AWP:
			case WEAPON_G3SG1:
				return true;
			}
		}
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we are actively sniping (moving to sniper spot or settled in)
 */
bool CCSBot::IsSniping( void ) const
{
	if ( GetTask() == MOVE_TO_SNIPER_SPOT || GetTask() == SNIPING )
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we are using a shotgun
 */
bool CCSBot::IsUsingShotgun( void ) const
{
	return m_pActiveItem && (m_pActiveItem->m_iId == WEAPON_XM1014 || m_pActiveItem->m_iId == WEAPON_M3);
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Returns true if using the big 'ol machinegun
 */
bool CCSBot::IsUsingMachinegun( void ) const
{
	return m_pActiveItem && m_pActiveItem->m_iId == WEAPON_M249;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if primary weapon doesn't exist or is totally out of ammo
 */
bool CCSBot::IsPrimaryWeaponEmpty( void ) const
{
	CBasePlayerWeapon *weapon = static_cast<CBasePlayerWeapon *>(m_rgpPlayerItems[PRIMARY_WEAPON_SLOT]);

	if ( weapon == NULL )
		return true;

	// check if gun has any ammo left
	if ( weapon->m_iClip != 0 || m_rgAmmo[weapon->m_iPrimaryAmmoType] > 0 )
		return false;

	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if pistol doesn't exist or is totally out of ammo
 */
bool CCSBot::IsPistolEmpty( void ) const
{
	CBasePlayerWeapon *weapon = static_cast<CBasePlayerWeapon *>(m_rgpPlayerItems[PISTOL_SLOT]);

	if ( weapon == NULL )
		return true;

	// check if gun has any ammo left
	if ( weapon->m_iClip != 0 || m_rgAmmo[weapon->m_iPrimaryAmmoType] > 0 )
		return false;

	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Equip the given item
 */
bool CCSBot::DoEquip( CBasePlayerWeapon *weapon )
{
	if ( weapon == NULL )
		return false;

	// check if weapon has any ammo left
	if ( weapon->m_iClip == 0 && m_rgAmmo[weapon->m_iPrimaryAmmoType] <= 0 )
		return false;

	// equip it
	SelectItem( STRING( weapon->pev->classname ) );
	m_equipTimer.Start();

	return true;
}

// throttle how often equipping is allowed
const float minEquipInterval = 5.0f;

//--------------------------------------------------------------------------------------------------------------
/**
 * Equip the best weapon we are carrying that has ammo
 */
void CCSBot::EquipBestWeapon( bool mustEquip )
{
	// throttle how often equipping is allowed
	if ( !mustEquip && m_equipTimer.GetElapsedTime() < minEquipInterval )
		return;

	CBasePlayerWeapon *primary = static_cast<CBasePlayerWeapon *>(m_rgpPlayerItems[PRIMARY_WEAPON_SLOT]);
	if ( primary )
	{
		WeaponClassType weaponClass = WeaponIDToWeaponClass( primary->m_iId );

		if ( (cv_bot_allow_shotguns.value > 0.0f && weaponClass == WEAPONCLASS_SHOTGUN) ||
			(cv_bot_allow_machine_guns.value > 0.0f && weaponClass == WEAPONCLASS_MACHINEGUN) ||
			(cv_bot_allow_rifles.value > 0.0f && weaponClass == WEAPONCLASS_RIFLE) ||
			(cv_bot_allow_snipers.value > 0.0f && weaponClass == WEAPONCLASS_SNIPERRIFLE) ||
			(cv_bot_allow_sub_machine_guns.value > 0.0f && weaponClass == WEAPONCLASS_SUBMACHINEGUN) ||
			(cv_bot_allow_shield.value > 0.0f && primary->m_iId == WEAPON_SHIELDGUN) )
		{
			if ( DoEquip( primary ) )
				return;
		}
	}

	if ( cv_bot_allow_pistols.value > 0.0f )
	{
		if ( DoEquip( static_cast<CBasePlayerWeapon *>(m_rgpPlayerItems[PISTOL_SLOT]) ) )
			return;
	}

	// always have a knife
	EquipKnife();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Equip our pistol
 */
void CCSBot::EquipPistol( void )
{
	// throttle how often equipping is allowed
	if ( m_equipTimer.GetElapsedTime() < minEquipInterval )
		return;

	if ( cv_bot_allow_pistols.value > 0.0f && !IsUsingPistol() )
	{
		CBasePlayerWeapon *pistol = static_cast<CBasePlayerWeapon *>(m_rgpPlayerItems[PISTOL_SLOT]);
		DoEquip( pistol );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Equip the knife
 */
void CCSBot::EquipKnife( void )
{
	if ( !IsUsingKnife() )
	{
		if ( m_rgpPlayerItems[KNIFE_SLOT] )
			SelectItem( STRING( m_rgpPlayerItems[KNIFE_SLOT]->pev->classname ) );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we have a grenade in our inventory
 */
bool CCSBot::HasGrenade( void ) const
{
	return m_rgpPlayerItems[GRENADE_SLOT] != NULL;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Equip a grenade, return false if we cant
 */
bool CCSBot::EquipGrenade( bool noSmoke )
{
	// snipers don't use grenades
	if ( IsSniper() )
		return false;

	if ( IsUsingGrenade() )
		return true;

	if ( HasGrenade() )
	{
		CBasePlayerItem *grenade = m_rgpPlayerItems[GRENADE_SLOT];

		if ( noSmoke && grenade->m_iId == WEAPON_SMOKEGRENADE )
			return false;

		SelectItem( STRING( grenade->pev->classname ) );
		return true;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Returns true if we have knife equipped
 */
bool CCSBot::IsUsingKnife( void ) const
{
	return m_pActiveItem && m_pActiveItem->m_iId == WEAPON_KNIFE;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Returns true if we have pistol equipped
 */
bool CCSBot::IsUsingPistol( void ) const
{
	if ( m_pActiveItem == NULL )
		return false;

	switch ( m_pActiveItem->m_iId )
	{
	case WEAPON_P228:
	case WEAPON_ELITE:
	case WEAPON_FIVESEVEN:
	case WEAPON_USP:
	case WEAPON_GLOCK18:
	case WEAPON_DEAGLE:
		return true;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Returns true if we have a grenade equipped
 */
bool CCSBot::IsUsingGrenade( void ) const
{
	return m_pActiveItem && (m_pActiveItem->m_iId == WEAPON_HEGRENADE ||
		m_pActiveItem->m_iId == WEAPON_SMOKEGRENADE || m_pActiveItem->m_iId == WEAPON_FLASHBANG);
}

//--------------------------------------------------------------------------------------------------------------
bool CCSBot::IsUsingHEGrenade( void ) const
{
	return m_pActiveItem && m_pActiveItem->m_iId == WEAPON_HEGRENADE;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Begin the process of throwing the grenade
 */
void CCSBot::ThrowGrenade( const Vector *target )
{
	if ( IsUsingGrenade() && !m_isWaitingToThrowGrenade )
	{
		SetLookAt( "GrenadeThrow", target, PRIORITY_UNINTERRUPTABLE, 3.0f, false, 1.0f );
		m_isWaitingToThrowGrenade = true;
		m_tossGrenadeTimer.Start( 3.0f );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Find spot to throw grenade ahead of us and "around the corner" along our path
 */
bool CCSBot::FindGrenadeTossPathTarget( Vector *pos )
{
	if ( m_pathLength == 0 || m_pathIndex >= m_pathLength )
		return false;

	// find farthest point we can see on the path
	int i = m_pathIndex;
	while ( i < m_pathLength )
	{
		Vector position = m_path[i].pos + Vector( 0.0f, 0.0f, HalfHumanHeight );
		if ( !FVisible( position ) )
			break;
		++i;
	}

	if ( i == m_pathIndex )
		return false;

	// find exact spot where we lose sight
	Vector visibleSpot = m_path[i - 1].pos;
	Vector dir = m_path[i].pos - visibleSpot;
	float length = dir.Length();
	if ( length > 0.0f )
	{
		dir = dir / length;
		for ( float t = 0.0f; ; t += 25.0f )
		{
			Vector p = m_path[i - 1].pos + dir * t + Vector( 0.0f, 0.0f, HalfHumanHeight );
			if ( !FVisible( p ) )
				break;
			visibleSpot = p;
			if ( t + 25.0f >= length )
				break;
		}
	}

	// massage the location a bit
	visibleSpot.z += 10.0f;

	TraceResult result;
	Vector check;

	// check +X
	check = visibleSpot + Vector( 999.9f, 0.0f, 0.0f );
	UTIL_TraceLine( visibleSpot, check, dont_ignore_monsters, ignore_glass, ENT( pev ), &result );
	if ( result.flFraction < 1.0f && result.vecEndPos.x - visibleSpot.x < 50.0f )
		visibleSpot.x = result.vecEndPos.x - 50.0f;

	// check -X
	check = visibleSpot + Vector( -999.9f, 0.0f, 0.0f );
	UTIL_TraceLine( visibleSpot, check, dont_ignore_monsters, ignore_glass, ENT( pev ), &result );
	if ( result.flFraction < 1.0f && visibleSpot.x - result.vecEndPos.x < 50.0f )
		visibleSpot.x = result.vecEndPos.x + 50.0f;

	// check +Y
	check = visibleSpot + Vector( 0.0f, 999.9f, 0.0f );
	UTIL_TraceLine( visibleSpot, check, dont_ignore_monsters, ignore_glass, ENT( pev ), &result );
	if ( result.flFraction < 1.0f && result.vecEndPos.y - visibleSpot.y < 50.0f )
		visibleSpot.y = result.vecEndPos.y - 50.0f;

	// check -Y
	check = visibleSpot + Vector( 0.0f, -999.9f, 0.0f );
	UTIL_TraceLine( visibleSpot, check, dont_ignore_monsters, ignore_glass, ENT( pev ), &result );
	if ( result.flFraction < 1.0f && visibleSpot.y - result.vecEndPos.y < 50.0f )
		visibleSpot.y = result.vecEndPos.y + 50.0f;

	*pos = visibleSpot;
	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Reload our weapon if we must
 */
void CCSBot::ReloadCheck( void )
{
	// don't bother to reload if there are no enemies left
	if ( GetEnemiesRemaining() == 0 )
		return;

	if ( IsDefusingBomb() || IsActiveWeaponReloading() )
		return;

	if ( IsActiveWeaponClipEmpty() )
	{
		// high-skill players switch to pistol instead of reloading during combat
		if ( GetProfile()->GetSkill() > 0.5f && IsAttacking() )
		{
			if ( !IsUsingPistol() && !IsPistolEmpty() )
			{
				// switch to pistol instead of reloading
				EquipPistol();
				return;
			}
		}
	}
	else if ( gpGlobals->time - m_lastSawEnemyTimestamp > 3.0f && GetActiveWeaponAmmoRatio() <= 0.6f )
	{
		// high-skill players use all their ammo and switch to pistol instead of reloading during combat
		if ( GetProfile()->GetSkill() > 0.5f && IsAttacking() )
			return;
	}
	else
	{
		// do not need to reload
		return;
	}

	// don't reload the AWP until it is totally out of ammo
	if ( IsUsingAWP() && !IsActiveWeaponClipEmpty() )
		return;

	Reload();

	// move to cover to reload if there are enemies nearby
	if ( GetNearbyEnemyCount() )
	{
		// avoid enemies while reloading (above 0.75 skill always hide to reload)
		const float hideChance = 100.0f * GetProfile()->GetSkill() + 25.0f;

		if ( !IsHiding() && RANDOM_FLOAT( 0.0f, 100.0f ) < hideChance )
		{
			const float safeTime = 5.0f;
			if ( gpGlobals->time - m_lastSawEnemyTimestamp < safeTime )
			{
				PrintIfWatched( "Retreating to a safe spot to reload!\n" );
				const Vector *spot = FindNearbyRetreatSpot( this, 1000.0f );
				if ( spot )
				{
					// ignore enemies for a second to give us time to hide
					// reaching our hiding spot clears our disposition
					IgnoreEnemies( 10.0f );

					Run();
					StandUp();
					Hide( spot, 0.0f, false );
				}
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Silence/unsilence our weapon if we must
 */
void CCSBot::SilencerCheck( void )
{
	if ( IsDefusingBomb() || IsActiveWeaponReloading() || IsAttacking() )
		return;

	// M4A1 and USP are the only weapons with removable silencers
	if ( !DoesActiveWeaponHaveSilencer() )
		return;

	// don't touch the silencer if there are enemies nearby
	if ( GetNearbyEnemyCount() == 0 )
	{
		CBasePlayerWeapon *weapon = GetActiveWeapon();
		if ( weapon == NULL )
			return;

		bool isSilencerOn = (weapon->m_iWeaponState & (WPNSTATE_USP_SILENCED | WPNSTATE_M4A1_SILENCED)) != 0;

		// equip silencer if we want to and we don't have a shield.
		if ( isSilencerOn != GetProfile()->PrefersSilencer() && !m_bOwnsShield )
		{
			PrintIfWatched( "%s silencer!\n", (isSilencerOn) ? "Unequipping" : "Equipping" );
			weapon->SecondaryAttack();
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnTouchingWeapon( CWeaponBox *box )
{
	CBasePlayerItem *weapon = box->m_rgpPlayerItems[PRIMARY_WEAPON_SLOT];
	CBasePlayerItem *current = m_rgpPlayerItems[PRIMARY_WEAPON_SLOT];
	if ( !weapon || !current || weapon->m_iId == current->m_iId || !GetProfile()->HasPrimaryPreference() ||
		gpGlobals->time - m_lastSawEnemyTimestamp < 2.5f )
		return;
	for ( int i = 0; GetProfile()->GetWeaponPreferenceCount() != 0; ++i )
	{
		int preference = GetProfile()->GetWeaponPreference( i );
		if ( IsPrimaryWeapon( preference ) )
		{
			if ( current->m_iId == preference )
				return;
			if ( weapon->m_iId == preference )
			{
				DropPrimary( this );
				return;
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if a friend is in our weapon's way
 * @todo Check more rays for safety.
 */
bool CCSBot::IsFriendInLineOfFire( void )
{
	// compute the unit vector along our view
	UTIL_MakeVectors( pev->v_angle + pev->punchangle );

	// trace the bullet's path
	Vector end = GetGunPosition() + gpGlobals->v_forward * 10000.0f;
	TraceResult result;
	UTIL_TraceLine( GetGunPosition(), end, dont_ignore_monsters, ignore_glass, ENT( pev ), &result );

	CBaseEntity *victim = CBaseEntity::Instance( result.pHit );
	return victim && victim->IsPlayer() && victim->IsAlive() && static_cast<CBasePlayer *>(victim)->m_iTeam == m_iTeam;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return line-of-sight distance to obstacle along weapon fire ray
 * @todo Re-use this computation with IsFriendInLineOfFire()
 */
float CCSBot::ComputeWeaponSightRange( void )
{
	// compute the unit vector along our view
	UTIL_MakeVectors( pev->v_angle + pev->punchangle );

	// trace the bullet's path
	Vector end = GetGunPosition() + gpGlobals->v_forward * 10000.0f;
	TraceResult result;
	UTIL_TraceLine( GetGunPosition(), end, dont_ignore_monsters, ignore_glass, ENT( pev ), &result );

	return (GetGunPosition() - result.vecEndPos).Length();
}
