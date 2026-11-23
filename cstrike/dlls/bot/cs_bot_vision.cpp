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
#include "cs_bot.h"
#include "bot_profile.h"
#include <algorithm>

//--------------------------------------------------------------------------------------------------------------
/**
 * Move actual view angles towards desired ones.
 * This is the only place v_angle is altered.
 * @todo Make stiffness and turn rate constants timestep invariant.
 */
void CCSBot::UpdateLookAngles( void )
{
	float interval = g_flBotCommandInterval;

	// springs are stiffer when attacking, so we can track and move between targets better
	bool attacking = IsAttacking();
	float damping = attacking ? 30.0f : 25.0f;
	float stiffness = attacking ? 300.0f : 200.0f;

	// these may be overridden by ladder logic
	float yaw = m_lookYaw;
	float pitch = m_lookPitch;

	if ( m_pathLadder )
	{
		Vector direction = m_pathLadder->m_top - pev->origin;
		float ladderYaw = UTIL_VecToYaw( direction );
		NavDirType facing = m_pathLadderFaceIn ? OppositeDirection( m_pathLadder->m_dir ) : m_pathLadder->m_dir;
		switch ( m_pathLadderState )
		{
		case 0:
		case 1:
			yaw = ladderYaw;
			if ( (m_goalPosition - pev->origin).LengthSquared() < 10000.0f )
				pitch = m_pathLadderState == 0 ? -60.0f : 60.0f;
			break;
		case 2:
			yaw = ladderYaw;
			pitch = -60.0f;
			break;
		case 3:
			yaw = ladderYaw;
			pitch = 60.0f;
			break;
		case 4:
		case 5:
		case 6:
		case 7:
		{
			static const float facingYaw[NUM_DIRECTIONS] = { 270.0f, 0.0f, 90.0f, 180.0f };
			float baseYaw = static_cast<unsigned int>(facing) < NUM_DIRECTIONS ? facingYaw[facing] : 0.0f;
			float currentYaw = pev->v_angle.y;
			while ( currentYaw < 0.0f )
				currentYaw += 360.0f;
			while ( currentYaw > 360.0f )
				currentYaw -= 360.0f;
			float error;
			if ( currentYaw < 45.0f || currentYaw > 315.0f )
				error = m_pathLadder->m_top.y - pev->origin.y;
			else if ( currentYaw < 135.0f )
				error = -(m_pathLadder->m_top.x - pev->origin.x);
			else if ( currentYaw < 225.0f )
				error = -(m_pathLadder->m_top.y - pev->origin.y);
			else
				error = m_pathLadder->m_top.x - pev->origin.x;
			yaw = baseYaw + error;
			pitch = m_pathLadderState == 4 || m_pathLadderState == 6 ? -60.0f : 60.0f;
			break;
		}
		case 8:
		case 9:
		{
			static const float facingYaw[NUM_DIRECTIONS] = { 270.0f, 0.0f, 90.0f, 180.0f };
			yaw = static_cast<unsigned int>(facing) < NUM_DIRECTIONS ? facingYaw[facing] : 0.0f;
			break;
		}
		}
	}

	//
	// Yaw
	//
	float angleDiff = yaw - pev->v_angle.y;
	while ( angleDiff < -180.0f )
		angleDiff += 360.0f;
	while ( angleDiff > 180.0f )
		angleDiff -= 360.0f;

	// if almost at target angle, snap to it
	const float onTargetTolerance = 1.0f;		// 3
	if ( angleDiff < onTargetTolerance && angleDiff > -onTargetTolerance )
	{
		m_lookYawVel = 0.0f;
		pev->v_angle.y = yaw;
	}
	else
	{
		// simple angular spring/damper
		float accel = angleDiff * stiffness - damping * m_lookYawVel;

		// limit rate
		accel = std::max( -3000.0f, std::min( 3000.0f, accel ) );

		m_lookYawVel += accel * interval;
		pev->v_angle.y += m_lookYawVel * interval;
	}

	//
	// Pitch
	// Actually, this is negative pitch.
	//
	angleDiff = pitch - pev->v_angle.x;
	while ( angleDiff < -180.0f )
		angleDiff += 360.0f;
	while ( angleDiff > 180.0f )
		angleDiff -= 360.0f;

	// simple angular spring/damper
	// double the stiffness since pitch is only +/- 90 and yaw is +/- 180
	float accel = angleDiff * (stiffness + stiffness) - damping * m_lookPitchVel;

	// limit rate
	accel = std::max( -3000.0f, std::min( 3000.0f, accel ) );

	m_lookPitchVel += accel * interval;
	pev->v_angle.x += interval * m_lookPitchVel;

	// limit range - avoid gimbal lock
	if ( pev->v_angle.x < -89.0f )
		pev->v_angle.x = -89.0f;
	else if ( pev->v_angle.x > 89.0f )
		pev->v_angle.x = 89.0f;

	pev->v_angle.z = 0.0f;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we can see the point
 */
bool CCSBot::IsVisible( const Vector *pos, bool testFOV ) const
{
	// we can't see anything if we're blind
	if ( m_blindUntilTime > gpGlobals->time )
		return false;

	// is it in my general viewcone?
	if ( testFOV && !const_cast<CCSBot *>(this)->FInViewCone( const_cast<Vector *>(pos) ) )
		return false;

	// check line of sight against smoke
	m_eyePosition = pev->origin + pev->view_ofs;
	if ( TheBots->IsLineBlockedBySmoke( &m_eyePosition, pos ) )
		return false;

	// check line of sight
	TraceResult result;
	UTIL_TraceLine( m_eyePosition, *pos, ignore_monsters, ignore_glass, ENT( pev ), &result );
	return result.flFraction == 1.0f;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we can see any part of the player
 * Check parts in order of importance. Return the first part seen in "visPart" if it is non-NULL.
 */
bool CCSBot::IsVisible( CBasePlayer *player, bool testFOV, unsigned char *visParts ) const
{
	unsigned char testVisParts = IsVisible( &player->pev->origin, testFOV ) ? CHEST : NONE;

	// check top of head
	Vector partPos = player->pev->origin;
	partPos.z += 25.0f;
	if ( IsVisible( &partPos, testFOV ) )
		testVisParts |= HEAD;

	// check feet
	partPos.z = player->pev->origin.z + ((player->pev->flags & FL_DUCKING) ? -14.0f : -34.0f);
	if ( IsVisible( &partPos, testFOV ) )
		testVisParts |= FEET;

	// check "edges"
	Vector2D direction = (player->pev->origin - pev->origin).Make2D();
	direction.NormalizeInPlace();
	Vector2D side( -direction.y * 13.0f, direction.x * 13.0f );
	partPos = player->pev->origin + Vector( side.x, side.y, 0.0f );
	if ( IsVisible( &partPos, testFOV ) )
		testVisParts |= LEFT_SIDE;

	partPos.x = player->pev->origin.x - side.x;
	partPos.y = player->pev->origin.y - side.y;
	if ( IsVisible( &partPos, testFOV ) )
		testVisParts |= RIGHT_SIDE;

	if ( visParts )
		*visParts = testVisParts;

	return testVisParts != NONE;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Update desired view angles to point towards m_lookAtSpot
 */
void CCSBot::UpdateLookAt( void )
{
	Vector to = m_lookAtSpot - (pev->origin + pev->view_ofs);
	Vector idealAngle = UTIL_VecToAngles( to );
	m_lookYaw = idealAngle.y;
	m_lookPitch = 360.0f - idealAngle.x;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Look at the given point in space for the given duration (-1 means forever)
 */
void CCSBot::SetLookAt( const char *desc, const Vector *pos, PriorityType pri, float duration, bool clearIfClose, float angleTolerance )
{
	if ( pos == NULL )
		return;

	// if currently looking at a point in space with higher priority, ignore this request
	if ( m_lookAtSpotState != 0 && m_lookAtSpotPriority > pri )
		return;

	// if already looking at this spot, just extend the time
	if ( m_lookAtSpotState != 0 &&
		abs( static_cast<int>(pos->x - m_lookAtSpot.x) ) < 10 &&
		abs( static_cast<int>(pos->y - m_lookAtSpot.y) ) < 10 &&
		abs( static_cast<int>(pos->z - m_lookAtSpot.z) ) < 10 )
	{
		m_lookAtSpotDuration = duration;

		if ( m_lookAtSpotPriority < pri )
			m_lookAtSpotPriority = pri;
	}
	else
	{
		// look at new spot
		m_lookAtSpot = *pos;
		m_lookAtSpotDuration = duration;
		m_lookAtSpotState = 1;
		m_lookAtSpotPriority = pri;
	}

	m_lookAtSpotClearIfClose = clearIfClose;
	m_lookAtSpotAngleTolerance = angleTolerance;
	m_lookAtDesc = desc;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Block all "look at" and "look around" behavior for given duration - just look ahead
 */
void CCSBot::InhibitLookAround( float duration )
{
	m_inhibitLookAroundTimestamp = gpGlobals->time + duration;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Update enounter spot timestamps, etc
 */
void CCSBot::UpdatePeripheralVision( void )
{
	const float peripheralUpdateInterval = 0.29f;		// if we update at 10Hz, this ensures we test once every three
	if ( gpGlobals->time - m_peripheralTimestamp < peripheralUpdateInterval )
		return;

	m_peripheralTimestamp = gpGlobals->time;

	if ( m_spotEncounter )
	{
		// check LOS to all spots in case we see them with our "peripheral vision"
		for ( SpotOrderList::const_iterator it = m_spotEncounter->spotList.begin(); it != m_spotEncounter->spotList.end(); ++it )
		{
			Vector pos = *it->spot->GetPosition();
			pos.z += 36.0f;

			if ( !IsVisible( &pos, CHECK_FOV ) )
				continue;

			// can see hiding spot, remember when we saw it last
			SetHidingSpotCheckTimestamp( it->spot );
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Update the "looking around" behavior.
 */
void CCSBot::UpdateLookAround( bool )
{
	// check if looking around has been inhibited
	if ( m_inhibitLookAroundTimestamp > gpGlobals->time )
		return;

	if ( !m_isEnemyVisible && CanHearNearbyEnemyGunfire( 1000.0f ) )
	{
		Vector spot = m_noisePosition + Vector( 0.0f, 0.0f, HalfHumanHeight );
		SetLookAt( "Check dangerous noise", &spot, PRIORITY_HIGH, 0.25f, false, 5.0f );
		InhibitLookAround( RANDOM_FLOAT( 2.0f, 4.0f ) );
		return;
	}

	if ( !(m_lookAtSpotState != 0 && m_lookAtSpotPriority > PRIORITY_LOW) && gpGlobals->time - m_lastSawEnemyTimestamp < 0.25f )
	{
		m_lookAtSpotState = 0;
		m_lookAtDesc = NULL;

		Vector spot = m_lastEnemyPosition;

		// find enemy position on the ground
		if ( GetSimpleGroundHeight( &m_lastEnemyPosition, &spot.z, NULL ) )
		{
			spot.z += HalfHumanHeight;
			SetLookAt( "Last Enemy Position", &spot, PRIORITY_MEDIUM, RANDOM_FLOAT( 2.0f, 3.0f ), true, 5.0f );
			return;
		}
	}

	//
	// Look at nearby enemy noises
	//
	if ( UpdateLookAtNoise() )
		return;

	//
	// If we are hiding (or otherwise standing still), watch all approach points leading into this region
	//
	if ( pev->velocity.LengthSquared() < 100.0f )
	{
		// if we're sniping, zoom in to watch our approach points
		if ( IsUsingSniperRifle() )
		{
			// low skill bots don't pre-zoom
			if ( GetProfile()->GetSkill() > 0.4f )
			{
				if ( m_lookYawVel >= 1.0f || m_lookYawVel <= -1.0f ||
					m_lookPitchVel >= 1.0f || m_lookPitchVel <= -1.0f )
				{
					// zoom out
					if ( m_iFOV <= 60 )
						SecondaryAttack();
				}
				else
				{
					float range = ComputeWeaponSightRange();
					AdjustZoom( range );
				}
			}
		}

		if ( m_lastKnownArea == NULL )
			return;

		if ( gpGlobals->time < m_lookAroundStateTimestamp )
			return;

		// if we're sniping, switch look-at spots less often
		if ( IsUsingSniperRifle() )
			m_lookAroundStateTimestamp = gpGlobals->time + RANDOM_FLOAT( 5.0f, 10.0f );
		else
			m_lookAroundStateTimestamp = gpGlobals->time + RANDOM_FLOAT( 1.0f, 2.0f );	// 0.5, 1.0

		if ( m_approachPointCount )
		{
			Vector spot = m_approachPoint[ RANDOM_LONG( 0, m_approachPointCount - 1 ) ];

			// don't look at the floor, look roughly at chest level
			/// @todo If this approach point is very near, this will cause us to aim up in the air if were crouching
			spot.z += HalfHumanHeight;

			SetLookAt( "Approach Point (Hiding)", &spot, PRIORITY_LOW, -1.0f, false, 5.0f );
		}
		else
		{
			m_lookAtSpotState = 0;
			m_lookAtDesc = NULL;
		}

		return;
	}

	//
	// Glance at "encouter spots" as we move past them
	//
	if ( m_spotEncounter )
	{
		//
		// Check encounter spots
		//
		if ( !IsSafe() && !(m_lookAtSpotState != 0 && m_lookAtSpotPriority >= PRIORITY_LOW) )
		{
			// allow a short time to look where we're going
			if ( gpGlobals->time < m_spotCheckTimestamp )
				return;

			/// @todo Use skill parameter instead of accuracy

			// lower skills have exponentially longer delays
			float asleep = (1.0f - GetProfile()->GetSkill());
			asleep *= asleep;
			asleep *= asleep;

			m_spotCheckTimestamp = gpGlobals->time + asleep * RANDOM_FLOAT( 10.0f, 30.0f );

			// figure out how far along the path segment we are
			Vector delta = m_spotEncounter->path.to - m_spotEncounter->path.from;
			float length = delta.Length();
			float t;

			if ( abs( static_cast<int>(delta.x) ) > abs( static_cast<int>(delta.y) ) )
				t = (pev->origin.x - m_spotEncounter->path.from.x) / delta.x;
			else
				t = (pev->origin.y - m_spotEncounter->path.from.y) / delta.y;

			// advance parameter a bit so we "lead" our checks
			const float leadCheckRange = 50.0f;
			t = std::max( 0.0f, std::min( 1.0f, t + leadCheckRange / length ) );

			// collect the unchecked spots so far
			HidingSpot *dangerSpot[8];
			int dangerSpotCount = 0;
			int dangerIndex = 0;

			const float checkTime = 10.0f;
			for ( SpotOrderList::const_iterator it = m_spotEncounter->spotList.begin(); it != m_spotEncounter->spotList.end(); ++it )
			{
				// if we have seen this spot recently, we don't need to look at it
				if ( gpGlobals->time - GetHidingSpotCheckTimestamp( it->spot ) <= checkTime )
					continue;

				if ( it->t > t )
					break;

				dangerSpot[ dangerIndex++ ] = it->spot;
				if ( dangerIndex >= 8 )
					dangerIndex = 0;
				if ( dangerSpotCount < 8 )
					++dangerSpotCount;
			}

			if ( dangerSpotCount )
			{
				// pick one of the spots at random
				int which = RANDOM_LONG( 0, dangerSpotCount - 1 );

				// glance at the spot for minimum time
				Vector spot = *dangerSpot[which]->GetPosition() + Vector( 0.0f, 0.0f, HalfHumanHeight );
				SetLookAt( "Encounter Spot", &spot, PRIORITY_LOW, 0.0f, true, 10.0f );

				// immediately mark it as "checked", so we don't check it again
				// if we get distracted before we check it - that's the way it goes
				SetHidingSpotCheckTimestamp( dangerSpot[which] );
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * "Bend" our line of sight around corners until we can "see" the point.
 */
bool CCSBot::BendLineOfSight( const Vector *eye, const Vector *target, Vector *bend ) const
{
	// if we can directly see the point, use it
	Vector raisedTarget = *target + Vector( 0.0f, 0.0f, HalfHumanHeight );
	TraceResult result;
	UTIL_TraceLine( *eye, raisedTarget, ignore_monsters, ENT( pev ), &result );
	if ( result.flFraction == 1.0f && !result.fStartSolid )
	{
		// can directly see point, no bending needed
		*bend = *target;
		return true;
	}

	// "bend" our line of sight until we can see the approach point
	Vector to = *target - *eye;
	float startAngle = UTIL_VecToYaw( to );
	float length = to.Length2D();

	for ( float angle = 10.0f; angle <= 130.0f; angle += 10.0f )
	{
		// check both sides at this angle offset
		for ( int side = 0; side < 2; ++side )
		{
			float actualAngle = side ? startAngle + angle : startAngle - angle;

			// compute rotated point ray endpoint
			Vector rotPoint( eye->x + length * BotCOS( actualAngle ), eye->y + length * BotSIN( actualAngle ), target->z );

			// check LOS to find length to test along ray
			Vector raisedPoint = rotPoint + Vector( 0.0f, 0.0f, HalfHumanHeight );
			UTIL_TraceLine( *eye, raisedPoint, ignore_monsters, ENT( pev ), &result );

			// if this ray started in an obstacle, skip it
			if ( result.fStartSolid )
				continue;

			Vector ray = rotPoint - *eye;
			float rayLength = ray.NormalizeInPlace();
			float visibleLength = rayLength * result.flFraction;

			// step along ray, checking if point is visible from ray point
			for ( float bendLength = 50.0f; bendLength <= visibleLength; bendLength += 50.0f )
			{
				// compute point along ray
				Vector bendPoint = *eye + ray * bendLength;

				// check if we can see approach point from this bend point
				UTIL_TraceLine( bendPoint, raisedTarget, ignore_monsters, ENT( pev ), &result );

				if ( result.flFraction == 1.0f && !result.fStartSolid )
				{
					// target is visible from this bend point on the ray - use this point on the ray as our point
					if ( !GetGroundHeight( &bendPoint, &bendPoint.z, NULL ) )
						bendPoint.z = target->z;

					*bend = bendPoint;
					return true;
				}
			}
		}
	}

	// bending rays didn't help - still can't see the point
	*bend = *target;
	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return most dangerous threat in my field of view (feeds into reaction time queue).
 * @todo Account for lighting levels, cover, and distance to see if we notice enemy
 */
CBasePlayer *CCSBot::FindMostDangerousThreat( void )
{
	enum { MAX_THREATS = 16 };		// maximum number of simulataneously attendable threats
	struct CloseInfo
	{
		CBasePlayer *enemy;
		float range;
	}
	threat[ MAX_THREATS ];
	int threatCount = 0;

	float closeFriendRange = 100000000000.0f;
	float closeHumanFriendRange = 100000000000.0f;

	m_bomber = NULL;
	m_closestVisibleFriend = NULL;
	m_closestVisibleHumanFriend = NULL;

	int i;
	for ( i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *player = static_cast<CBasePlayer *>(UTIL_PlayerByIndex( i ));

		if ( player == NULL )
			continue;

		if ( FNullEnt( player->pev ) )
			continue;

		if ( STRING( player->pev->netname )[0] == '\0' )
			continue;

		// is it a player?
		if ( !player->IsPlayer() )
			continue;

		// ignore self
		if ( player->entindex() == entindex() )
			continue;

		// is it alive?
		if ( !player->IsAlive() )
			continue;

		// is it an enemy?
		if ( player->m_iTeam == m_iTeam )
		{
			// keep track of nearby friends - use less exact visibility check
			m_eyePosition = pev->origin + pev->view_ofs;
			TraceResult result;
			UTIL_TraceLine( m_eyePosition, player->pev->origin, ignore_monsters, ignore_glass, ENT( pev ), &result );
			if ( result.flFraction == 1.0f )
			{
				// update watch timestamp
				WatchInfo &visibility = m_watchInfo[player->entindex() - 1];
				visibility.timestamp = gpGlobals->time;
				visibility.isEnemy = false;

				// keep track of our closest friend
				float rangeSq = (pev->origin - player->pev->origin).LengthSquared();
				if ( closeFriendRange > rangeSq )
				{
					m_closestVisibleFriend = player;
					closeFriendRange = rangeSq;
				}

				// keep track of our closest human friend
				if ( !player->IsBot() && closeHumanFriendRange > rangeSq )
				{
					m_closestVisibleHumanFriend = player;
					closeHumanFriendRange = rangeSq;
				}
			}

			continue;
		}

		// check if this enemy is fully or partially visible
		if ( !IsVisible( player, CHECK_FOV, NULL ) )
			continue;

		// update watch timestamp
		WatchInfo &visibility = m_watchInfo[player->entindex() - 1];
		visibility.timestamp = gpGlobals->time;
		visibility.isEnemy = true;

		// note if we see the bomber
		if ( player->IsBombGuy() )
			m_bomber = player;

		// keep track of all visible threats
		float distSq = (pev->origin - player->pev->origin).LengthSquared();

		// maintain set of visible threats, sorted by increasing distance

		// find insertion point
		int j = 0;
		while ( j < threatCount && threat[j].range <= distSq )
			++j;

		// shift lower half down a notch
		for ( int k = threatCount; k > j; --k )
			threat[k] = threat[k - 1];

		// insert threat into sorted list
		threat[j].enemy = player;
		threat[j].range = distSq;

		if ( threatCount < MAX_THREATS )
			++threatCount;
	}

	// track the maximum enemy and friend counts we've seen recently
	int prevEnemies = m_nearbyEnemyCount;
	int prevFriends = m_nearbyFriendCount;
	m_nearbyEnemyCount = 0;
	m_nearbyFriendCount = 0;
	for ( i = 0; i < 32; ++i )
	{
		if ( m_watchInfo[i].timestamp <= 0.0f )
			continue;

		const float recentTime = 3.0f;
		if ( gpGlobals->time - m_watchInfo[i].timestamp < recentTime )
		{
			if ( m_watchInfo[i].isEnemy )
				++m_nearbyEnemyCount;
			else
				++m_nearbyFriendCount;
		}
	}

	// note when we saw this batch of enemies
	if ( prevEnemies == 0 && m_nearbyEnemyCount != 0 )
		m_firstSawEnemyTimestamp = gpGlobals->time;

	if ( prevEnemies != m_nearbyEnemyCount || prevFriends != m_nearbyFriendCount )
		PrintIfWatched( "Nearby friends = %d, enemies = %d\n", m_nearbyFriendCount, m_nearbyEnemyCount );

	if ( threatCount == 0 )
	{
		m_enemyPlace = UNDEFINED_PLACE;
		return NULL;
	}

	//
	// Track the place where we saw most of our enemies
	//
	struct PlaceRank
	{
		Place place;
		int count;
	};
	static PlaceRank placeRank[64];
	int locCount = 0;

	PlaceRank common;
	common.place = 0;
	common.count = 0;

	for ( i = 0; i < threatCount; ++i )
	{
		// find the area the player/bot is standing on
		CBasePlayer *player = threat[i].enemy;
		CNavArea *area = player->IsBot() ? static_cast<CCSBot *>(player)->GetLastKnownArea() :
			TheNavAreaGrid.GetNearestNavArea( &player->pev->origin );
		if ( area == NULL )
			continue;

		Place threatLoc = area->GetPlace();
		if ( !threatLoc )
			continue;

		// if place is already in set, increment count
		int j = 0;
		while ( j < locCount && placeRank[j].place != threatLoc )
			++j;

		if ( j == locCount )
		{
			// new place
			if ( locCount < 64 )
			{
				placeRank[ locCount ].place = threatLoc;
				placeRank[ locCount ].count = 1;

				if ( common.count == 0 )
					common = placeRank[locCount];

				++locCount;
			}
		}
		else
		{
			// others are in that place, increment
			++placeRank[j].count;

			// keep track of the most common place
			if ( placeRank[j].count > common.count )
				common = placeRank[j];
		}
	}

	// remember most common place
	m_enemyPlace = common.place;

	for ( i = 0; i < threatCount; ++i )
	{
		CBasePlayer *player = threat[i].enemy;
		if ( !player->HasShield() || !player->m_bShieldDrawn )
			return player;
	}

	// return closest threat
	return threat[0].enemy;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Update our reaction time queue
 */
void CCSBot::UpdateReactionQueue( void )
{
	// zombies dont see any threats
	if ( cv_bot_zombie.value > 0.0f )
		return;

	// find biggest threat at this instant
	CBasePlayer *threat = FindMostDangerousThreat();

	int now = m_enemyQueueIndex;

	// store a snapshot of its state at the end of the reaction time queue
	if ( threat )
	{
		m_enemyQueue[ now ].player = threat;
		CBasePlayerWeapon *weapon = static_cast<CBasePlayerWeapon *>(threat->m_pActiveItem);
		m_enemyQueue[ now ].isReloading = weapon && weapon->m_fInReload != 0;
		m_enemyQueue[ now ].isProtectedByShield = threat->HasShield() && threat->m_bShieldDrawn;
	}
	else
	{
		m_enemyQueue[ now ].player = NULL;
		m_enemyQueue[ now ].isReloading = false;
		m_enemyQueue[ now ].isProtectedByShield = false;
	}

	// queue is round-robin
	++m_enemyQueueIndex;
	if ( m_enemyQueueIndex >= 20 )
		m_enemyQueueIndex = 0;

	if ( m_enemyQueueCount < 20 )
		++m_enemyQueueCount;

	// clamp reaction time to enemy queue size
	float reactionTime = GetProfile()->GetReactionTime();
	float maxReactionTime = (20.0f * g_flBotFullThinkInterval) - 0.01f;
	if ( reactionTime > maxReactionTime )
		reactionTime = maxReactionTime;

	// "rewind" time back to our reaction time
	int reactionTimeSteps = static_cast<int>((reactionTime / g_flBotFullThinkInterval) + 0.5f);

	int i = now - reactionTimeSteps;
	if ( i < 0 )
		i += 20;

	m_enemyQueueAttendIndex = static_cast<unsigned char>(i);
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return the most dangerous threat we are "conscious" of
 */
CBasePlayer *CCSBot::GetRecognizedEnemy( void )
{
	if ( m_enemyQueueAttendIndex >= m_enemyQueueCount )
		return NULL;

	return static_cast<CBasePlayer *>(static_cast<CBaseEntity *>(m_enemyQueue[ m_enemyQueueAttendIndex ].player));
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if the enemy we are "conscious" of is reloading
 */
bool CCSBot::IsRecognizedEnemyReloading( void )
{
	if ( m_enemyQueueAttendIndex >= m_enemyQueueCount )
		return false;

	return m_enemyQueue[ m_enemyQueueAttendIndex ].isReloading;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if the enemy we are "conscious" of is hiding behind a shield
 */
bool CCSBot::IsRecognizedEnemyProtectedByShield( void )
{
	if ( m_enemyQueueAttendIndex >= m_enemyQueueCount )
		return false;

	return m_enemyQueue[ m_enemyQueueAttendIndex ].isProtectedByShield;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return distance to closest enemy we are "conscious" of
 */
float CCSBot::GetRangeToNearestRecognizedEnemy( void )
{
	const CBasePlayer *enemy = GetRecognizedEnemy();

	if ( enemy )
		return (pev->origin - enemy->pev->origin).Length();

	return 100000000.0f;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Blind the bot for the given duration
 */
void CCSBot::Blind( float duration, float holdTime, float fadeTime, int alpha )
{
	CBasePlayer::Blind( duration, holdTime, fadeTime, alpha );

	PrintIfWatched( "I'm blind!\n" );

	if ( RANDOM_FLOAT( 0.0f, 100.0f ) < 33.3f )
	{
		GetChatter()->Say( "Blinded", 1.0f );
	}

	// decide which way to move while blind
	m_blindMoveDir = RANDOM_LONG( 1, NUM_RELATIVE_DIRECTIONS - 1 );

	m_blindFire = RANDOM_FLOAT( 0.0f, 100.0f ) < 10.0f;

	// no longer safe
	AdjustSafeTime();
}
