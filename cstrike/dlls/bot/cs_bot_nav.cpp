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
#include "cs_bot.h"
#include "bot_profile.h"
#include <algorithm>
#include <cstdlib>

namespace
{
	enum LadderState
	{
		APPROACH_ASCENDING_LADDER = 0,
		APPROACH_DESCENDING_LADDER = 1,
		FACE_ASCENDING_LADDER = 2,
		FACE_DESCENDING_LADDER = 3,
		MOUNT_ASCENDING_LADDER = 4,
		MOUNT_DESCENDING_LADDER = 5,
		ASCEND_LADDER = 6,
		DESCEND_LADDER = 7,
		DISMOUNT_ASCENDING_LADDER = 8,
		MOVE_TO_DESTINATION = 10
	};
}

//--------------------------------------------------------------------------------------------------------------
/**
 * If we are not on the navigation mesh (m_currentArea == NULL),
 * move towards last known area.
 * Return false if off mesh.
 */
bool CCSBot::StayOnNavMesh( void )
{
	if ( m_currentArea == NULL )
	{
		// move back onto the area map

		// if we have no lastKnownArea, we probably started off
		// of the nav mesh - find the closest nav area and use it
		CNavArea *goalArea;
		if ( !m_lastKnownArea )
		{
			goalArea = TheNavAreaGrid.GetNearestNavArea( &pev->origin, false );
			PrintIfWatched( "Started off the nav mesh - moving to closest nav area...\n" );
		}
		else
		{
			goalArea = m_lastKnownArea;
			PrintIfWatched( "Getting out of NULL area...\n" );
		}

		if ( goalArea )
		{
			Vector pos;
			goalArea->GetClosestPointOnArea( &pev->origin, &pos );

			// move point into area
			Vector to = pos - pev->origin;
			to.NormalizeInPlace();

			const float stepInDist = 5.0f;		// how far to "step into" an area - must be less than min area size
			pos = pos + (stepInDist * to);

			MoveTowardsPosition( &pos );
		}

		// if we're stuck, try to get un-stuck
		// do stuck movements last, so they override normal movement
		if ( m_isStuck )
			Wiggle();

		return false;
	}

	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Find "simple" ground height, treating current nav area as part of the floor
 */
bool CCSBot::GetSimpleGroundHeightWithFloor( const Vector *pos, float *height, Vector *normal )
{
	if ( GetSimpleGroundHeight( pos, height, normal ) )
	{
		// our current nav area also serves as a ground polygon
		if ( m_lastKnownArea && m_lastKnownArea->IsOverlapping( pos ) )
			*height = std::max( m_lastKnownArea->GetZ( pos ), *height );

		return true;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return approximately how much damage will will take from the given fall height
 */
float CCSBot::GetApproximateFallDamage( float height ) const
{
	// empirically discovered height values
	const float slope = 0.2178f;
	const float intercept = 26.0f;

	float damage = slope * height - intercept;

	return damage >= 0.0f ? damage : 0.0f;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Test for un-jumpable height change, or unrecoverable fall
 */
bool CCSBot::IsStraightLinePathWalkable( const Vector * ) const
{
	// this is causing hang-up problems when crawling thru ducts/windows that drop off into rooms (they fail the "falling" check)
	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Check if we need to jump due to height change
 */
bool CCSBot::DiscontinuityJump( float ground, bool onlyJumpDown, bool mustJump )
{
	if ( m_isJumpCrouching )
		return false;
	float feetHeight = pev->origin.z - (IsCrouching() ? 18.0f : 36.0f);
	float dz = ground - feetHeight;
	bool jumped;
	if ( !onlyJumpDown && dz > StepHeight )
	{
		// dont restrict jump time when going up
		jumped = Jump( true );
	}
	else
	{
		if ( m_pathLadder || dz >= -JumpHeight )
			return false;
		jumped = Jump( mustJump );
	}
	if ( !jumped )
		return false;
	m_isJumpCrouching = true;
	m_hasJumpCrouched = false;
	StandUp();
	m_jumpCrouchTimestamp = gpGlobals->time;
	return jumped;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Move towards position, independant of view angle
 */
void CCSBot::MoveTowardsPosition( const Vector *pos )
{
	//
	// Jump up on ledges
	// Because we may not be able to get to our goal position and enter the next
	// area because our extent collides with a nearby vertical ledge, make sure
	// we look far enough ahead to avoid this situation.
	// Can't look too far ahead, or bots will try to jump up slopes.
	//
	// NOTE: We need to do this frequently to catch edges at the right time
	// @todo Look ahead *along path* instead of straight line 
	//
	if ( (m_lastKnownArea == NULL || !(m_lastKnownArea->GetAttributes() & NAV_NO_JUMP)) && !IsOnLadder() && !m_isJumpCrouching )
	{
		Vector aheadRay = *pos - pev->origin;
		aheadRay.z = 0.0f;
		aheadRay.NormalizeInPlace();
		Vector stepAhead;
		Vector normal;
		float ground;

		// look far ahead to allow us to smoothly jump over gaps, ledges, etc
		// only jump if ground is flat at lookahead spot to avoid jumping up slopes
		bool jumped = false;
		if ( IsRunning() )
		{
			stepAhead = pev->origin + aheadRay * 80.0f + Vector( 0, 0, 36 );
			if ( GetSimpleGroundHeightWithFloor( &stepAhead, &ground, &normal ) && normal.z > 0.9f && !m_isJumpCrouching )
				jumped = DiscontinuityJump( ground, true, false );
		}
		if ( !jumped )
		{
			// close up jumping
			stepAhead = pev->origin + aheadRay * 30.0f + Vector( 0, 0, 36 );
			if ( GetSimpleGroundHeightWithFloor( &stepAhead, &ground, NULL ) && !m_isJumpCrouching )
				jumped = DiscontinuityJump( ground, false, false );
			if ( !jumped )
			{
				// about to fall gap-jumping
				stepAhead = pev->origin + aheadRay * 10.0f + Vector( 0, 0, 36 );
				if ( GetSimpleGroundHeightWithFloor( &stepAhead, &ground, NULL ) && !m_isJumpCrouching )
					DiscontinuityJump( ground, true, true );
			}
		}
	}

	// compute our current forward and lateral vectors
	float sine = BotSIN( pev->v_angle.y );
	float cosine = BotCOS( pev->v_angle.y );

	// compute unit vector to goal position
	Vector2D to = (*pos - pev->origin).Make2D();
	to.NormalizeInPlace();

	// move towards the position independant of our view direction
	float toProj = cosine * to.x + sine * to.y;
	if ( toProj > 0.25f )
		MoveForward();
	else if ( toProj < -0.25f )
		MoveBackward();

	// if we are avoiding someone via strafing, don't override
	if ( !m_avoid )
	{
		float latProj = cosine * to.y - sine * to.x;
		if ( latProj >= 0.25f )
			StrafeLeft();
		else if ( latProj <= -0.25f )
			StrafeRight();
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Move away from position, independant of view angle
 */
void CCSBot::MoveAwayFromPosition( const Vector *pos )
{
	// compute our current forward and lateral vectors
	float sine = BotSIN( pev->v_angle.y );
	float cosine = BotCOS( pev->v_angle.y );

	// compute unit vector to goal position
	Vector2D to = (*pos - pev->origin).Make2D();
	to.NormalizeInPlace();

	// move away from the position independant of our view direction
	float toProj = cosine * to.x + sine * to.y;
	float latProj = cosine * to.y - sine * to.x;
	if ( toProj > 0.5f )
		MoveBackward();
	else if ( toProj < -0.5f )
		MoveForward();

	if ( latProj >= 0.5f )
		StrafeRight();
	else if ( latProj <= -0.5f )
		StrafeLeft();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Strafe (sidestep) away from position, independant of view angle
 */
void CCSBot::StrafeAwayFromPosition( const Vector *pos )
{
	// compute our current forward and lateral vectors
	float sine = BotSIN( pev->v_angle.y );
	float cosine = BotCOS( pev->v_angle.y );

	// compute unit vector to goal position
	Vector2D to = (*pos - pev->origin).Make2D();
	to.NormalizeInPlace();

	if ( cosine * to.y - sine * to.x < 0.0f )
		StrafeLeft();
	else
		StrafeRight();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Determine approach points from eye position and approach areas of current area
 */
void CCSBot::ComputeApproachPoints( void )
{
	m_approachPointCount = 0;
	if ( m_lastKnownArea == NULL )
		return;

	// assume we're crouching for now
	Vector eye = pev->origin; // + pev->view_ofs;	// eye position

	for ( int i = 0; i < m_lastKnownArea->GetApproachInfoCount() && m_approachPointCount <= 15; ++i )
	{
		const CNavArea::ApproachInfo *info = m_lastKnownArea->GetApproachInfo( i );
		if ( info->here.area == NULL || info->prev.area == NULL )
			continue;

		// compute approach point (approach area is "info->here")
		Vector ap;
		if ( info->prevToHereHow <= GO_WEST )
		{
			float halfWidth;
			info->prev.area->ComputePortal( info->here.area, (NavDirType)info->prevToHereHow, &ap, &halfWidth );
			ap.z = info->here.area->GetZ( &ap );
		}
		else
		{
			// use the area's center as an approach point
			ap = *info->here.area->GetCenter();
		}

		// "bend" our line of sight around corners until we can see the approach point
		Vector bendPoint;
		if ( BendLineOfSight( &eye, &ap, &bendPoint ) )
			m_approachPoint[m_approachPointCount++] = bendPoint;
	}
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::DrawApproachPoints( void )
{
	for ( int i = 0; i < m_approachPointCount; ++i )
		UTIL_DrawBeamPoints( m_approachPoint[i], m_approachPoint[i] + Vector( 0, 0, 50 ), 3, 0, 255, 255 );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Find the approach point that is nearest to our current path, ahead of us
 */
bool CCSBot::FindApproachPointNearestPath( Vector *pos )
{
	if ( m_pathLength == 0 )
		return false;

	// make sure approach points are accurate
	ComputeApproachPoints();

	float targetRangeSq = 0.0f;
	bool found = false;
	Vector target;

	//
	// We dont want the strictly closest point, but the farthest approach point
	// from us that is near our path
	//
	const float nearPathSq = 10000.0f;	// (100)

	for ( int i = 0; i < m_approachPointCount; ++i )
	{
		Vector close;
		if ( FindClosestPointOnPath( &m_approachPoint[i], m_pathIndex, m_pathLength, &close ) )
		{
			float rangeSq = (m_approachPoint[i] - close).LengthSquared();
			if ( rangeSq <= nearPathSq && rangeSq > targetRangeSq )
			{
				target = close;
				found = true;
				targetRangeSq = rangeSq;
			}
		}
	}

	if ( found )
	{
		*pos = target + Vector( 0, 0, 36 );
		return true;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if nearing a jump in the path
 */
bool CCSBot::IsNearJump( void ) const
{
	if ( m_pathIndex == 0 || m_pathIndex >= m_pathLength )
		return false;

	for ( int i = m_pathIndex - 1; i < m_pathIndex; ++i )
	{
		if ( m_path[i].area->GetAttributes() & NAV_JUMP )
		{
			float dz = m_path[i + 1].pos.z - m_path[i].pos.z;

			if ( dz > 0.0f )
				return true;
		}
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if a friend is between us and the given position
 */
bool CCSBot::IsFriendInTheWay( const Vector *goalPos ) const
{
	// do this check less often to ease CPU burden
	if ( !m_avoidFriendTimer.IsElapsed() )
	{
		return m_isFriendInTheWay;
	}

	const float avoidFriendInterval = 0.5f;
	m_avoidFriendTimer.Start( avoidFriendInterval );

	// compute ray along intended path
	Vector moveDir = *goalPos - pev->origin;

	// make it a unit vector 
	float length = moveDir.NormalizeInPlace();

	m_isFriendInTheWay = false;

	// check if any friends are overlapping this linear path
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *player = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( player == NULL )
			continue;

		if ( FNullEnt( player->pev ) )
			continue;

		if ( !player->IsAlive() )
			continue;

		if ( player->m_iTeam != m_iTeam )
			continue;

		if ( player == this )
			continue;

		// compute vector from us to our friend
		Vector toFriend = player->pev->origin - pev->origin;

		// check if friend is in our "personal space"
		const float personalSpace = 100.0f;
		if ( toFriend.IsLengthGreaterThan( personalSpace ) )
			continue;

		// find distance of friend along our movement path
		float friendDistAlong = DotProduct( toFriend, moveDir );

		// if friend is behind us, ignore him
		if ( friendDistAlong <= 0.0f )
			continue;

		// constrain point to be on path segment
		Vector pos;
		if ( friendDistAlong >= length )
			pos = *goalPos;
		else
			pos = pev->origin + moveDir * friendDistAlong;

		// check if friend overlaps our intended line of movement
		const float friendRadius = 30.0f;
		if ( (pos - player->pev->origin).IsLengthLessThan( friendRadius ) )
		{
			// friend is in our personal space and overlaps our intended line of movement
			m_isFriendInTheWay = true;
			break;
		}
	}

	return m_isFriendInTheWay;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Do reflex avoidance movements if our "feelers" are touched
 */
void CCSBot::FeelerReflexAdjustment( Vector *goalPosition )
{
	// if we are in a "precise" area, do not do feeler adjustments
	if ( m_lastKnownArea && (m_lastKnownArea->GetAttributes() & NAV_PRECISE) )
		return;
	float sine = BotSIN( m_forwardAngle );
	float cosine = BotCOS( m_forwardAngle );
	float feelerOffset = IsCrouching() ? 15.0f : 20.0f;
	float feelerLength = IsCrouching() ? 20.0f : (IsRunning() ? 50.0f : 30.0f);

	//
	// Feelers must follow floor slope
	//
	m_eyePosition = pev->origin + pev->view_ofs;
	float ground;
	Vector normal;
	if ( !GetSimpleGroundHeightWithFloor( &m_eyePosition, &ground, &normal ) )
		return;

	// get forward vector along floor
	Vector dir = CrossProduct( Vector( -sine, cosine, 0 ), normal );

	// correct the sideways vector
	Vector lat = CrossProduct( dir, normal );

	Vector feet = pev->origin;
	feet.z -= IsCrouching() ? 18.0f : 36.0f;
	Vector center = feet + Vector( 0, 0, 18.1f );
	Vector leftStart = center + lat * feelerOffset;
	Vector leftEnd = leftStart + dir * feelerLength;
	bool leftClear = IsWalkableTraceLineClear( leftStart, leftEnd, WALK_THRU_DOORS | WALK_THRU_BREAKABLES );
	if ( (cv_bot_traceview.value == 1.0f && IsLocalPlayerWatchingMe()) || cv_bot_traceview.value == 10.0f )
		UTIL_DrawBeamPoints( leftStart, leftEnd, 1, leftClear ? 0 : 255, leftClear ? 255 : 0, 0 );
	Vector rightStart = center - lat * feelerOffset;
	Vector rightEnd = rightStart + dir * feelerLength;
	bool rightClear = IsWalkableTraceLineClear( rightStart, rightEnd, WALK_THRU_DOORS | WALK_THRU_BREAKABLES );
	if ( (cv_bot_traceview.value == 1.0f && IsLocalPlayerWatchingMe()) || cv_bot_traceview.value == 10.0f )
		UTIL_DrawBeamPoints( rightStart, rightEnd, 1, rightClear ? 0 : 255, rightClear ? 255 : 0, 0 );

	float avoidRange = IsCrouching() ? 150.0f : 300.0f;
	if ( rightClear )
	{
		if ( !leftClear )
		{
			// right clear, left hit - veer right
			*goalPosition = *goalPosition - lat * avoidRange;
		}
	}
	else if ( leftClear )
	{
		// right hit, left clear - veer left
		*goalPosition = *goalPosition + lat * avoidRange;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * If next step of path uses a ladder, prepare to traverse it
 */
void CCSBot::SetupLadderMovement( void )
{
	if ( m_pathIndex <= 0 || m_pathLength == 0 || m_path[m_pathIndex].ladder == NULL )
		return;
	m_spotEncounter = NULL;
	m_pathLadder = m_path[m_pathIndex].ladder;
	m_areaEnteredTimestamp = gpGlobals->time;
	m_pathLadderTimestamp = gpGlobals->time;

	// to get to next area, we must traverse a ladder
	if ( m_path[m_pathIndex].how == GO_LADDER_UP )
	{
		m_pathLadderState = APPROACH_ASCENDING_LADDER;
		m_pathLadderFaceIn = true;
		PrintIfWatched( "APPROACH_ASCENDING_LADDER\n" );
		m_goalPosition = m_pathLadder->m_bottom;
		AddDirectionVector( &m_goalPosition, m_pathLadder->m_dir, 32.0f );
		m_lookAheadAngle = DirectionToAngle( OppositeDirection( m_pathLadder->m_dir ) );
		return;
	}

	// try to mount ladder "face out" first
	m_goalPosition = m_pathLadder->m_top;
	AddDirectionVector( &m_goalPosition, OppositeDirection( m_pathLadder->m_dir ), 32.0f );
	TraceResult trace;
	UTIL_TraceLine( m_pathLadder->m_top, m_goalPosition, ignore_monsters, m_pathLadder->m_entity->edict(), &trace );
	m_pathLadderState = APPROACH_DESCENDING_LADDER;
	if ( trace.flFraction == 1.0f )
	{
		PrintIfWatched( "APPROACH_DESCENDING_LADDER (face out)\n" );
		m_pathLadderFaceIn = false;
		m_lookAheadAngle = DirectionToAngle( m_pathLadder->m_dir );
	}
	else
	{
		PrintIfWatched( "APPROACH_DESCENDING_LADDER (face in)\n" );
		m_pathLadderFaceIn = true;
		m_lookAheadAngle = DirectionToAngle( OppositeDirection( m_pathLadder->m_dir ) );
		m_goalPosition = m_pathLadder->m_top;
		AddDirectionVector( &m_goalPosition, m_pathLadder->m_dir, 16.0f );
	}
}

//--------------------------------------------------------------------------------------------------------------
/// @todo What about ladders whose top AND bottom are messed up?
void CCSBot::ComputeLadderEndpoint( bool isAscending )
{
	Vector from = isAscending ? m_pathLadder->m_bottom : m_pathLadder->m_top;
	const Vector &to = isAscending ? m_pathLadder->m_top : m_pathLadder->m_bottom;
	from.z = pev->origin.z;
	TraceResult result;
	UTIL_TraceLine( from, to, ignore_monsters, m_pathLadder->m_entity->edict(), &result );
	m_pathLadderEnd = result.flFraction == 1.0f ? to.z : from.z + result.flFraction * (to.z - from.z);
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Navigate our current ladder. Return true if we are doing ladder navigation.
 * @todo Need Push() and Pop() for run/walk context to keep ladder speed contained.
 */
bool CCSBot::UpdateLadderMovement( void )
{
	if ( m_pathLadder == NULL )
		return false;

	// check for timeout
	if ( gpGlobals->time - m_pathLadderTimestamp > 10.0f )
	{
		PrintIfWatched( "Ladder timeout!\n" );

		// jump off ladder and give up
		Jump( true );
		Wiggle();
		ResetStuckMonitor();
		DestroyPath();
		Run();
		return false;
	}
	if ( (m_pathLadderState == APPROACH_ASCENDING_LADDER || m_pathLadderState == APPROACH_DESCENDING_LADDER || m_pathLadderState == ASCEND_LADDER || m_pathLadderState == DESCEND_LADDER || m_pathLadderState == DISMOUNT_ASCENDING_LADDER || m_pathLadderState == MOVE_TO_DESTINATION) && m_isStuck )
	{
		PrintIfWatched( "Giving up ladder - stuck\n" );

		// jump off ladder and give up
		Jump( true );
		Wiggle();
		ResetStuckMonitor();
		DestroyPath();
		Run();
		return false;
	}
	ResetStuckMonitor();

	// check if somehow we totally missed the ladder
	if ( m_pathLadderState >= MOUNT_ASCENDING_LADDER && m_pathLadderState <= DESCEND_LADDER && (m_pathLadder->m_top - pev->origin).Make2D().IsLengthGreaterThan( 200.0f ) )
	{
		PrintIfWatched( "Missed ladder\n" );
		Jump( true );
		DestroyPath();
		Run();
		return false;
	}
	m_areaEnteredTimestamp = gpGlobals->time;
	float feetHeight = pev->origin.z - (IsCrouching() ? 18.0f : 36.0f);
	switch ( m_pathLadderState )
	{
	case APPROACH_ASCENDING_LADDER:
	{
		Vector2D delta = (pev->origin - m_goalPosition).Make2D();
		bool aligned = DotProduct( delta, m_pathLadder->m_dirVector ) < 0.0f && std::abs( (int)(m_pathLadder->m_dirVector.x * delta.y - m_pathLadder->m_dirVector.y * delta.x) ) < 10 && delta.IsLengthLessThan( 25.0f );

		// small radius will just slow them down a little for more accuracy in hitting their spot
		if ( delta.IsLengthLessThan( 50.0f ) )
		{
			Walk();
			StandUp();
		}

		/// @todo Check that we are on the ladder we think we are
		if ( IsOnLadder() )
		{
			m_pathLadderState = ASCEND_LADDER;
			PrintIfWatched( "ASCEND_LADDER\n" );

			// find actual top in case m_pathLadder penetrates the ceiling
			ComputeLadderEndpoint( true );
		}
		else if ( aligned )
		{
			// face the m_pathLadder
			m_pathLadderState = FACE_ASCENDING_LADDER;
			PrintIfWatched( "FACE_ASCENDING_LADDER\n" );
		}
		else
		{
			// move toward ladder mount point
			MoveTowardsPosition( &m_goalPosition );
		}
		return true;
	}
	case APPROACH_DESCENDING_LADDER:
	{
		// fall check
		if ( m_pathLadder->m_bottom.z + 36.0f >= feetHeight )
			break;
		Vector2D delta = (pev->origin - m_goalPosition).Make2D();
		bool aligned = DotProduct( delta, m_pathLadder->m_dirVector ) > 0.0f && std::abs( (int)(m_pathLadder->m_dirVector.x * delta.y - m_pathLadder->m_dirVector.y * delta.x) ) < 10 && delta.IsLengthLessThan( 25.0f );

		// if approaching ladder from the side or "ahead", walk
		if ( m_pathLadder->m_topBehindArea != m_lastKnownArea && !IsCrouching() && delta.IsLengthLessThan( 150.0f ) )
			Walk();

		/// @todo Check that we are on the ladder we think we are
		if ( IsOnLadder() )
		{
			// we slipped onto the ladder - climb it
			m_pathLadderState = DESCEND_LADDER;
			Run();
			PrintIfWatched( "DESCEND_LADDER\n" );

			// find actual bottom in case m_pathLadder penetrates the floor
			ComputeLadderEndpoint( false );
		}
		else if ( aligned )
		{
			// face the ladder
			m_pathLadderState = FACE_DESCENDING_LADDER;
			PrintIfWatched( "FACE_DESCENDING_LADDER\n" );
		}
		else
		{
			// move toward ladder mount point
			MoveTowardsPosition( &m_goalPosition );
		}
		return true;
	}
	case FACE_ASCENDING_LADDER:
	case FACE_DESCENDING_LADDER:
	{
		// find yaw to directly aim at ladder
		Vector target = m_pathLadderState == FACE_ASCENDING_LADDER ? m_pathLadder->m_bottom : m_pathLadder->m_top;
		Vector angles = UTIL_VecToAngles( target - pev->origin );
		float difference = pev->v_angle.y - angles.y;
		while ( difference > 180.0f )
			difference -= 360.0f;
		while ( difference < -180.0f )
			difference += 360.0f;
		if ( std::abs( (int)difference ) < 5 )
		{
			// move toward ladder until we become "on" it
			if ( m_pathLadderState == FACE_ASCENDING_LADDER )
			{
				Run();
				ResetStuckMonitor();
				m_pathLadderState = MOUNT_ASCENDING_LADDER;
				PrintIfWatched( "MOUNT_ASCENDING_LADDER\n" );
			}
			else
			{
				m_pathLadderState = MOUNT_DESCENDING_LADDER;
				ResetStuckMonitor();
				PrintIfWatched( "MOUNT_DESCENDING_LADDER\n" );
			}
		}
		return true;
	}
	case MOUNT_ASCENDING_LADDER:
		if ( IsOnLadder() )
		{
			m_pathLadderState = ASCEND_LADDER;
			PrintIfWatched( "ASCEND_LADDER\n" );

			// find actual top in case m_pathLadder penetrates the ceiling
			ComputeLadderEndpoint( true );
		}

		// move toward ladder mount point
		MoveForward();
		return true;
	case MOUNT_DESCENDING_LADDER:
		// fall check
		if ( m_pathLadder->m_bottom.z + 36.0f >= feetHeight )
			break;
		if ( IsOnLadder() )
		{
			m_pathLadderState = DESCEND_LADDER;
			PrintIfWatched( "DESCEND_LADDER\n" );

			// find actual bottom in case m_pathLadder penetrates the floor
			ComputeLadderEndpoint( false );
		}

		// move toward ladder mount point
		MoveForward();
		return true;
	case ASCEND_LADDER:
		// run, so we can make our dismount jump to the side, if necessary
		Run();

		// if our destination area requires us to crouch, do it
		if ( m_path[m_pathIndex].area->GetAttributes() & NAV_CROUCH )
			Crouch();

		// did we reach the top?
		feetHeight = pev->origin.z - (IsCrouching() ? 18.0f : 36.0f);
		if ( feetHeight < m_pathLadderEnd )
		{
			if ( !IsOnLadder() )
			{
				// we fall off the ladder, repath
				DestroyPath();
				return false;
			}
		}
		else
		{
			// we reached the top - dismount
			m_pathLadderState = DISMOUNT_ASCENDING_LADDER;
			PrintIfWatched( "DISMOUNT_ASCENDING_LADDER\n" );
			if ( m_path[m_pathIndex].area == m_pathLadder->m_topForwardArea )
				m_pathLadderDismountDir = FORWARD;
			else if ( m_path[m_pathIndex].area == m_pathLadder->m_topLeftArea )
				m_pathLadderDismountDir = LEFT;
			else if ( m_path[m_pathIndex].area == m_pathLadder->m_topRightArea )
				m_pathLadderDismountDir = RIGHT;
			m_pathLadderDismountTimestamp = gpGlobals->time;
		}

		// move up ladder
		MoveForward();
		return true;
	case DESCEND_LADDER:
		Run();
		if ( !IsOnLadder() || m_pathLadderEnd + 36.0f >= feetHeight )
		{
			// we reached the bottom, or we fell off - dismount
			m_pathLadderState = MOVE_TO_DESTINATION;
			m_path[m_pathIndex].area->GetClosestPointOnArea( &m_pathLadder->m_bottom, &m_goalPosition );
			AddDirectionVector( &m_goalPosition, m_pathLadder->m_dir, 16.0f );
			PrintIfWatched( "MOVE_TO_DESTINATION\n" );
		}

		// Move down ladder
		MoveForward();
		return true;
	case DISMOUNT_ASCENDING_LADDER:
		if ( gpGlobals->time - m_pathLadderDismountTimestamp >= 0.4f )
		{
			m_pathLadderState = MOVE_TO_DESTINATION;
			m_path[m_pathIndex].area->GetClosestPointOnArea( &pev->origin, &m_goalPosition );
			PrintIfWatched( "MOVE_TO_DESTINATION\n" );
		}

		// We should already be facing the dismount point
		if ( m_pathLadderFaceIn )
		{
			if ( m_pathLadderDismountDir == RIGHT )
				StrafeRight();
			else if ( m_pathLadderDismountDir == LEFT )
				StrafeLeft();
			else if ( m_pathLadderDismountDir == FORWARD )
				MoveForward();
			return m_pathLadderFaceIn;
		}
		if ( m_pathLadderDismountDir == FORWARD )
			MoveBackward();
		else if ( m_pathLadderDismountDir == RIGHT )
			StrafeLeft();
		else if ( m_pathLadderDismountDir == LEFT )
			StrafeRight();
		return true;
	case MOVE_TO_DESTINATION:
		if ( m_path[m_pathIndex].area->Contains( &pev->origin ) )
		{
			// successfully traversed ladder and reached destination area
			// exit ladder state machine
			PrintIfWatched( "Ladder traversed.\n" );
			m_pathLadder = NULL;

			// incrememnt path index to next step beyond this ladder
			SetPathIndex( m_pathIndex + 1 );
			return false;
		}
		MoveTowardsPosition( &m_goalPosition );
		return true;
	default:
		return true;
	}
	PrintIfWatched( "Fell from ladder.\n" );
	m_pathLadderState = MOVE_TO_DESTINATION;
	m_path[m_pathIndex].area->GetClosestPointOnArea( &m_pathLadder->m_bottom, &m_goalPosition );
	AddDirectionVector( &m_goalPosition, m_pathLadder->m_dir, 16.0f );
	PrintIfWatched( "MOVE_TO_DESTINATION\n" );
	return true;
}
