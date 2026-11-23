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
#include "cs_bot.h"
#include "bot_profile.h"

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we directly see where we think the noise came from
 * NOTE: Dont check FOV, since this is used to determine if we should turn our head to look at the noise
 * NOTE: Dont use IsVisible(), because smoke shouldnt cause us to not look toward noises
 */
bool CCSBot::CanSeeNoisePosition( void ) const
{
	TraceResult result;
	m_eyePosition = pev->origin + pev->view_ofs;
	UTIL_TraceLine( m_eyePosition, m_noisePosition + Vector( 0, 0, HalfHumanHeight ), ignore_monsters, ignore_glass, pev->pContainingEntity, &result );
	if ( result.flFraction == 1.0f )
	{
		// we can see the source of the noise
		return true;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we hear nearby threatening enemy gunfire within given range
 * -1 == infinite range
 */
bool CCSBot::CanHearNearbyEnemyGunfire( float range ) const
{
	// only attend to noise if it just happened
	if ( gpGlobals->time - m_noiseTimestamp > 0.5f )
		return false;

	// gunfire is high priority
	if ( m_noisePriority != PRIORITY_HIGH )
		return false;

	// check noise range
	if ( range > 0.0f && (pev->origin - m_noisePosition).IsLengthGreaterThan( range ) )
		return false;

	// if we dont have line of sight, it's not threatening (cant get shot)
	if ( !CanSeeNoisePosition() )
		return false;

	EHANDLE &enemy = const_cast<EHANDLE &>( m_enemy );
	if ( IsAttacking() && static_cast<int>( enemy ) != 0 )
	{
		// gunfire is only threatening if it is closer than our current enemy
		float gunfireDistSq = (m_noisePosition - pev->origin).LengthSquared();
		float enemyDistSq = (enemy->pev->origin - pev->origin).LengthSquared();
		const float muchCloserSq = 100.0f * 100.0f;
		if ( gunfireDistSq > enemyDistSq - muchCloserSq )
			return false;
	}

	return true;
}

//--------------------------------------------------------------------------------------------------------------
bool CCSBot::UpdateLookAtNoise( void )
{
	if ( m_noiseTimestamp <= 0.0f )
		return false;
	float elapsed = gpGlobals->time - m_noiseTimestamp;
	if ( elapsed < m_profile->GetReactionTime() || elapsed > 0.5f )
		return false;
	bool nearbyRecentEnemy = false;
	if ( gpGlobals->time - m_lastSawEnemyTimestamp < 5.0f )
	{
		bool nearby = (pev->origin - m_lastEnemyPosition).LengthSquared() < 750.0f * 750.0f;
		nearbyRecentEnemy = IsAtHidingSpot() < nearby;
	}
	else
		IsAtHidingSpot();
	Vector target;
	if ( nearbyRecentEnemy || CanSeeNoisePosition() )
		target = m_noisePosition;
	else
	{
		if ( m_approachPointCount == 0 )
			return false;
		int closest = -1;
		float closestDistance = 99999997952.0f;
		for ( int i = 0; i < m_approachPointCount; ++i )
		{
			float distance = (m_approachPoint[i] - m_noisePosition).LengthSquared();
			if ( closestDistance > distance )
			{
				closestDistance = distance;
				closest = i;
			}
		}
		if ( closest == -1 || !BendLineOfSight( &pev->origin, &m_approachPoint[closest], &target ) )
			return false;
	}
	PriorityType priority = m_noisePriority == PRIORITY_HIGH ? PRIORITY_HIGH : PRIORITY_MEDIUM;
	target.z += 36.0f;
	float duration;
	if ( IsAtHidingSpot() )
		duration = GetEnemiesRemaining() == 1 ? RANDOM_FLOAT( 5.0f, 15.0f ) : RANDOM_FLOAT( 2.0f, 5.0f );
	else
		duration = RANDOM_FLOAT( 1.0f, 2.0f );
	SetLookAt( "Noise", &target, priority, duration, true, 5.0f );
	return true;
}

//--------------------------------------------------------------------------------------------------------------
bool CCSBot::ShouldInvestigateNoise( float *range )
{
	if ( m_noiseTravelDistanceChecked || !IsWellPastSafe() || GetDisposition() != ENGAGE_AND_INVESTIGATE )
		return false;
	if ( m_noiseTimestamp <= 0.0f || gpGlobals->time - m_noiseTimestamp < m_profile->GetReactionTime() )
		return false;
	if ( gpGlobals->time - m_noiseCheckTimestamp < 0.25f )
		return false;
	m_noiseCheckTimestamp = gpGlobals->time;
	Vector delta = m_noisePosition - pev->origin;
	float distance = delta.Length();
	if ( abs( static_cast<int>( delta.z ) ) > 120 )
	{
		PathCost cost( this );
		float travel = NavAreaTravelDistance( m_lastKnownArea, m_noiseArea, cost );
		m_noiseTravelDistanceChecked = true;
		if ( travel < 0.0f || travel > 1500.0f )
			return false;
		if ( travel > distance )
			distance = travel;
	}
	if ( IsAtHidingSpot() && distance > 400.0f * m_profile->GetAggression() + 100.0f )
		return false;
	float chance = 1.0f - distance / 2000.0f;
	if ( GetFriendsRemaining() > 2 )
	{
		float penalty = GetFriendsRemaining() * 0.05f;
		chance -= penalty <= 0.5f ? penalty : 0.5f;
	}
	if ( chance >= RANDOM_FLOAT( 0.0f, 1.0f ) )
	{
		if ( range )
			*range = distance;
		return true;
	}
	return false;
}
