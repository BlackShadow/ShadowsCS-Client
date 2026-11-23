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

#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"cstrike\\dlls\\bot\\cs_bot_pathfind.cpp" )

//--------------------------------------------------------------------------------------------------------------
/**
 * Compute shortest path to goal position via A* algorithm
 * If 'goalArea' is NULL, path will get as close as it can.
 */
bool CCSBot::ComputePath( CNavArea *goalArea, const Vector *goal, RouteType route )
{
	//
	// Throttle re-pathing
	//
	if ( !m_repathTimer.IsElapsed() )
		return false;

	// randomize to distribute CPU load
	m_repathTimer.Start( RANDOM_FLOAT( 0.4f, 0.6f ) );

	DestroyPath();
	if ( m_lastKnownArea == NULL || (goalArea == NULL && goal == NULL) )
		return false;

	// note final specific position
	Vector pathEndPosition = goal ? *goal : *goalArea->GetCenter();

	// make sure path end position is on the ground
	if ( goalArea )
	{
		pathEndPosition.z = goalArea->GetZ( &pathEndPosition );

		// if we are already in the goal area, build trivial path
		if ( m_lastKnownArea == goalArea )
		{
			BuildTrivialPath( &pathEndPosition );
			return true;
		}
	}
	else
		GetGroundHeight( &pathEndPosition, &pathEndPosition.z, NULL );

	//
	// Compute shortest path to goal
	//
	CNavArea *closestArea;
	PathCost cost( this, route );
	if ( !NavAreaBuildPath( m_lastKnownArea, goalArea, goal, cost, &closestArea ) )
	{
		goalArea = closestArea;
		if ( goalArea == NULL )
			return false;
	}
	else if ( goalArea == NULL )
		return false;

	//
	// Build path by following parent links
	//

	// get count
	int count = 0;
	for ( CNavArea *area = goalArea; area; area = area->GetParent() )
		++count;

	// save room for endpoint
	if ( count > 255 )
		count = 255;
	else if ( count == 1 )
	{
		BuildTrivialPath( &pathEndPosition );
		return true;
	}

	// build path
	m_pathLength = count;
	for ( CNavArea *area = goalArea; count && area; area = area->GetParent() )
	{
		--count;
		m_path[count].area = area;
		m_path[count].how = area->GetParentHow();
	}

	// compute path positions
	if ( !ComputePathPositions() )
	{
		PrintIfWatched( "Error building path\n" );
		DestroyPath();
		return false;
	}
	if ( goal == NULL )
	{
		const ConnectInfo &last = m_path[m_pathLength - 1];
		switch ( last.how )
		{
		case GO_NORTH:
		case GO_SOUTH:
			pathEndPosition.x = last.pos.x;
			pathEndPosition.y = goalArea->GetCenter()->y;
			break;
		case GO_EAST:
		case GO_WEST:
			pathEndPosition.x = goalArea->GetCenter()->x;
			pathEndPosition.y = last.pos.y;
			break;
		default:
			break;
		}
		GetGroundHeight( &pathEndPosition, &pathEndPosition.z, NULL );
	}

	// append path end position
	ConnectInfo &end = m_path[m_pathLength++];
	end.area = goalArea;
	end.how = NUM_TRAVERSE_TYPES;
	end.pos = pathEndPosition;
	end.ladder = NULL;

	// do movement setup
	m_pathIndex = 1;
	m_areaEnteredTimestamp = gpGlobals->time;
	m_spotEncounter = NULL;
	m_goalPosition = m_path[1].pos;
	if ( m_path[1].ladder )
		SetupLadderMovement();
	else
		m_pathLadder = NULL;
	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Build trivial path to goal, assuming we are already in the same area
 */
void CCSBot::BuildTrivialPath( const Vector *goal )
{
	m_pathIndex = 1;
	m_pathLength = 2;

	m_path[0].area = m_lastKnownArea;
	m_path[0].pos = pev->origin;
	m_path[0].pos.z = m_lastKnownArea->GetZ( &pev->origin );
	m_path[0].ladder = NULL;
	m_path[0].how = NUM_TRAVERSE_TYPES;

	m_path[1].area = m_lastKnownArea;
	m_path[1].pos = *goal;
	m_path[1].pos.z = m_lastKnownArea->GetZ( goal );
	m_path[1].ladder = NULL;
	m_path[1].how = NUM_TRAVERSE_TYPES;

	m_spotEncounter = NULL;
	m_pathLadder = NULL;
	m_areaEnteredTimestamp = gpGlobals->time;

	m_goalPosition = *goal;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Set the current index along the path
 */
void CCSBot::SetPathIndex( int newIndex )
{
	m_pathIndex = std::min( m_pathLength - 1, newIndex );
	m_areaEnteredTimestamp = gpGlobals->time;

	if ( m_path[m_pathIndex].ladder )
	{
		SetupLadderMovement();
	}
	else
	{
		// get our "encounter spots" for this leg of the path
		if ( m_pathIndex > 1 && m_pathIndex < m_pathLength )
			m_spotEncounter = m_path[m_pathIndex - 1].area->GetSpotEncounter( m_path[m_pathIndex - 2].area, m_path[m_pathIndex].area );
		else
			m_spotEncounter = NULL;

		m_pathLadder = NULL;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Determine actual path positions bot will move between along the path
 */
bool CCSBot::ComputePathPositions( void )
{
	if ( m_pathLength == 0 )
		return false;

	// start in first area's center
	m_path[0].pos = *m_path[0].area->GetCenter();
	m_path[0].ladder = NULL;
	m_path[0].how = NUM_TRAVERSE_TYPES;

	for ( int i = 1; i < m_pathLength; ++i )
	{
		ConnectInfo &to = m_path[i];
		CNavArea *from = m_path[i - 1].area;
		if ( to.how <= GO_WEST )		// walk along the floor to the next area
		{
			to.ladder = NULL;

			// compute next point, keeping path as straight as possible
			NavDirType direction = (NavDirType)to.how;
			from->ComputeClosestPointInPortal( to.area, direction, &m_path[i - 1].pos, &to.pos );

			// move goal position into the goal area a bit
			AddDirectionVector( &to.pos, direction, 5.0f );

			// we need to walk out of "from" area, so keep Z where we can reach it
			to.pos.z = from->GetZ( &to.pos );

			// if this is a "jump down" connection, we must insert an additional point on the path
			if ( !to.area->IsConnected( from, NUM_DIRECTIONS ) )
			{
				// this is a "jump down" link

				// shift top of "jump down" out a bit to "get over the ledge"
				Vector step( 0, 0, 0 );
				AddDirectionVector( &step, direction, 25.0f );
				to.pos = to.pos + step;

				// insert a duplicate node to represent the bottom of the fall
				if ( m_pathLength <= 254 )
				{
					// copy nodes down
					for ( int j = m_pathLength - 1; j >= i; --j )
						m_path[j + 1] = m_path[j];

					// path is one node longer
					++m_pathLength;

					m_path[i + 1].pos = to.pos + step;

					// put this one at the bottom of the fall
					m_path[i + 1].pos.z = to.area->GetZ( &m_path[i + 1].pos );

					// move index ahead into the new node we just duplicated
					++i;
				}
			}
		}
		else if ( to.how == GO_LADDER_UP || to.how == GO_LADDER_DOWN )
		{
			// find our ladder
			const NavLadderList *list = from->GetLadderList( to.how == GO_LADDER_UP ? LADDER_UP : LADDER_DOWN );
			const CNavLadder *ladder = NULL;
			for ( NavLadderList::const_iterator iter = list->begin(); iter != list->end(); ++iter )
			{
				const CNavLadder *candidate = *iter;

				// can't use "behind" area when ascending...
				if ( to.how == GO_LADDER_UP ? (candidate->m_topForwardArea == to.area || candidate->m_topLeftArea == to.area || candidate->m_topRightArea == to.area) : candidate->m_bottomArea == to.area )
				{
					ladder = candidate;
					break;
				}
			}
			if ( ladder == NULL )
			{
				PrintIfWatched( "ERROR: Can't find ladder in path\n" );
				return false;
			}
			to.ladder = ladder;
			to.pos = to.how == GO_LADDER_UP ? ladder->m_bottom : ladder->m_top;
			NavDirType direction = to.how == GO_LADDER_UP ? ladder->m_dir : OppositeDirection( ladder->m_dir );
			AddDirectionVector( &to.pos, direction, 32.0f );
		}
	}

	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return estimated distance left to travel along path
 */
float CCSBot::GetPathDistanceRemaining( void ) const
{
	if ( m_pathLength == 0 )
		return -1.0f;

	int idx = std::min( m_pathLength - 1, m_pathIndex );

	const Vector *prevCenter = m_path[m_pathIndex].area->GetCenter();
	float dist = 0.0f;
	for ( int i = idx + 1; i < m_pathLength; ++i )
	{
		const Vector *center = m_path[i].area->GetCenter();
		dist += (*center - *prevCenter).Length();
		prevCenter = center;
	}

	return dist;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Compute closest point on path to given point
 * NOTE: This does not do line-of-sight tests, so closest point may be thru the floor, etc
 */
int CCSBot::FindClosestPointOnPath( const Vector *worldPos, int startIndex, int endIndex, Vector *close ) const
{
	if ( m_pathLength == 0 || close == NULL )
		return 0;

	float closeDistSq = 10000000000.0f;

	for ( int i = startIndex; i <= endIndex; ++i )
	{
		const Vector &from = m_path[i - 1].pos;
		const Vector &to = m_path[i].pos;

		// compute ray along this path segment
		Vector along = to - from;

		// make it a unit vector along the path
		float length = along.NormalizeInPlace();

		// find distance of closest point on ray
		float closeLength = DotProduct( *worldPos - from, along );

		// constrain point to be on path segment
		Vector pos = closeLength <= 0.0f ? from : (closeLength < length ? from + along * closeLength : to);

		float distSq = (pos - *worldPos).LengthSquared();

		// keep the closest point so far
		if ( closeDistSq > distSq )
		{
			*close = pos;
			closeDistSq = distSq;
		}
	}

	return 1;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return the closest point to our current position on our current path
 * If "local" is true, only check the portion of the path surrounding m_pathIndex.
 */
int CCSBot::FindOurPositionOnPath( Vector *close, bool local ) const
{
	if ( m_pathLength == 0 )
		return -1;

	Vector feet = pev->origin;
	feet.z -= IsCrouching() ? 18.0f : 36.0f;

	int start = 1;
	int end = m_pathLength;
	if ( local )
	{
		start = std::max( 1, m_pathIndex - 3 );
		end = std::min( end, m_pathIndex + 3 );
	}

	float closeDistSq = 10000000000.0f;
	int closeIndex = -1;
	for ( int i = start; i < end; ++i )
	{
		const Vector &from = m_path[i - 1].pos;
		const Vector &to = m_path[i].pos;

		// compute ray along this path segment
		Vector along = to - from;

		// make it a unit vector along the path
		float length = along.NormalizeInPlace();

		// find distance of closest point on ray
		float closeLength = DotProduct( feet - from, along );

		// constrain point to be on path segment
		Vector pos = closeLength <= 0.0f ? from : (closeLength < length ? from + along * closeLength : to);

		float distSq = (pos - feet).LengthSquared();

		// keep the closest point so far
		if ( closeDistSq > distSq )
		{
			Vector traceStart = feet + Vector( 0, 0, 36 );
			Vector traceEnd = pos + Vector( 0, 0, 36 );
			edict_t *ignore = NULL;
			TraceResult result;
			for ( ;; )
			{
				UTIL_TraceLine( traceStart, traceEnd, ignore_monsters, ignore, &result );
				if ( result.flFraction == 1.0f )
					break;
				ignore = result.pHit;
				entvars_t *hit = VARS( ignore );
				if ( !FClassnameIs( hit, "func_door" ) && !FClassnameIs( hit, "func_door_rotating" ) && (!FClassnameIs( hit, "func_breakable" ) || hit->takedamage != DAMAGE_YES) )
					break;
				Vector step = traceEnd - (feet + Vector( 0, 0, 36 ));
				step.NormalizeInPlace();
				traceStart = result.vecEndPos + step * 5.0f;
			}
			if ( result.flFraction == 1.0f )
			{
				if ( close )
					*close = pos;
				closeDistSq = distSq;
				closeIndex = i - 1;
			}
		}
	}

	return closeIndex;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Compute a point a fixed distance ahead along our path.
 * Returns path index just after point.
 */
int CCSBot::FindPathPoint( float aheadRange, Vector *point, int *prevIndex )
{
	// finds the closest point on local area of path, and returns the path index just prior to it
	Vector close;
	int startIndex = FindOurPositionOnPath( &close, true );

	if ( prevIndex )
		*prevIndex = startIndex;

	if ( startIndex <= 0 )
	{
		// went off the end of the path
		// or next point in path is unwalkable (ie: jump-down)
		// keep same point
		return m_pathIndex;
	}

	// if we are crouching, just follow the path exactly
	if ( IsCrouching() )
	{
		// we want to move to the immediately next point along the path from where we are now
		int index = std::min( startIndex + 1, m_pathLength - 1 );

		*point = m_path[index].pos;

		// if we are very close to the next point in the path, skip ahead to the next one to avoid wiggling
		// we must do a 2D check here, in case the goal point is floating in space due to jump down, etc
		while ( (*point - close).Make2D().IsLengthLessThan( 20.0f ) )
		{
			if ( ++index >= m_pathLength )
				return m_pathLength - 1;

			*point = m_path[index].pos;
		}

		return index;
	}

	// make sure we use a node a minimum distance ahead of us, to avoid wiggling 
	while ( startIndex < m_pathLength - 1 && (m_path[startIndex + 1].pos - close).Make2D().IsLengthLessThan( 20.0f ) )
		++startIndex;

	if ( startIndex > m_pathIndex && startIndex < m_pathLength && (m_path[startIndex].ladder || (m_path[startIndex].area->GetAttributes() & NAV_JUMP)) )
	{
		*point = m_path[startIndex].pos;
		return startIndex;
	}

	// we need the point just *ahead* of us
	startIndex = std::min( startIndex + 1, m_pathLength - 1 );

	if ( m_path[startIndex].ladder || (m_path[startIndex].area->GetAttributes() & NAV_JUMP) )
	{
		*point = m_path[startIndex].pos;
		return startIndex;
	}

	// note direction of path segment we are standing on
	Vector initDir = m_path[startIndex].pos - m_path[startIndex - 1].pos;
	initDir.NormalizeInPlace();

	Vector feet = pev->origin;
	feet.z -= IsCrouching() ? 18.0f : 36.0f;
	Vector eyes = feet + Vector( 0, 0, 36 );
	Vector prevDir = initDir;
	float rangeSoFar = 0.0f;

	// this flag is true if our ahead point is visible
	bool visible = true;

	// step along the path until we pass aheadRange
	bool isCorner = false;
	int i;
	for ( i = startIndex; i < m_pathLength; ++i )
	{
		const Vector &pos = m_path[i].pos;
		Vector dir = (pos - m_path[i - 1].pos).Normalize();

		// don't allow path to double-back from our starting direction (going upstairs, down curved passages, etc)
		if ( DotProduct( dir, initDir ) < 0.0f )
		{
			--i;
			break;
		}

		// if the path turns a corner, we want to move towards the corner, not into the wall/stairs/etc
		if ( DotProduct( dir, prevDir ) < 0.5f )
		{
			isCorner = true;
			--i;
			break;
		}

		// don't use points we cant see
		Vector probe = pos + Vector( 0, 0, 36 );
		if ( !IsWalkableTraceLineClear( eyes, probe, WALK_THRU_BREAKABLES ) )
		{
			// presumably, the previous point is visible, so we will interpolate
			visible = false;
			break;
		}

		// if we encounter a ladder or jump area, we must stop
		if ( m_path[i].ladder || (m_path[i].area->GetAttributes() & NAV_JUMP) )
			break;

		rangeSoFar += (pos - (i == startIndex ? feet : m_path[i - 1].pos)).Length2D();

		// stop if we have gone farther than aheadRange
		if ( rangeSoFar >= aheadRange )
			break;

		prevDir = dir;
	}

	int afterIndex = std::max( startIndex, std::min( i, m_pathLength - 1 ) );

	// compute point on the path at aheadRange
	if ( afterIndex == 0 )
		*point = m_path[0].pos;
	else
	{
		// interpolate point along path segment
		const Vector &beforePoint = m_path[afterIndex - 1].pos;
		Vector to = m_path[afterIndex].pos - beforePoint;
		float length = to.Length2D();

		float t = 1.0f - (rangeSoFar - aheadRange) / length;

		if ( t < 0.0f )
			t = 0.0f;
		else if ( t > 1.0f )
			t = 1.0f;

		*point = beforePoint + to * t;

		// if afterPoint wasn't visible, slide point backwards towards beforePoint until it is
		if ( !visible )
		{
			Vector probe = *point + Vector( 0, 0, 36 );
			while ( t > 0.0f && !IsWalkableTraceLineClear( eyes, probe, WALK_THRU_BREAKABLES ) )
			{
				t -= 25.0f / length;
				*point = beforePoint + to * t;
			}

			if ( t <= 0.0f )
				*point = beforePoint;
		}
	}

	// if position found is too close to us, or behind us, force it farther down the path so we don't stop and wiggle
	if ( !isCorner )
	{
		Vector2D toPoint = (*point - pev->origin).Make2D();
		if ( DotProduct( toPoint, initDir.Make2D() ) < 0.0f || toPoint.IsLengthLessThan( 50.0f ) )
		{
			for ( i = startIndex; i < m_pathLength; ++i )
			{
				toPoint = (m_path[i].pos - pev->origin).Make2D();
				if ( m_path[i].ladder || (m_path[i].area->GetAttributes() & NAV_JUMP) || toPoint.IsLengthGreaterThan( 50.0f ) )
				{
					*point = m_path[i].pos;
					startIndex = i;
					break;
				}
			}

			if ( i == m_pathLength )
			{
				*point = m_path[m_pathLength - 1].pos;
				startIndex = m_pathLength - 1;
			}
		}
	}

	// m_pathIndex should always be the next point on the path, even if we're not moving directly towards it
	return std::min( startIndex, m_pathLength - 1 );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Move along the path. Return false if end of path reached.
 */
CCSBot::PathResult CCSBot::UpdatePathMovement( bool allowSpeedChange )
{
	if ( m_pathLength == 0 )
		return PATH_FAILURE;

	if ( cv_bot_walk.value != 0.0f )
		Walk();

	//
	// If we are navigating a ladder, it overrides all other path movement until complete
	//
	if ( UpdateLadderMovement() )
		return PROGRESSING;

	// ladder failure can destroy the path
	if ( m_pathLength == 0 )
		return PATH_FAILURE;

	// we are not supposed to be on a ladder - if we are, jump off
	if ( IsOnLadder() )
		Jump( true );

	ASSERT_AT( m_pathIndex < m_pathLength, 1324 );

	//
	// Check if reached the end of the path
	//
	bool nearEndOfPath = false;
	if ( m_pathIndex >= m_pathLength - 1 )
	{
		Vector feet = pev->origin;
		feet.z -= IsCrouching() ? 18.0f : 36.0f;
		float distance = (m_path[m_pathLength - 1].pos - feet).LengthSquared();

		// walk as we get close to the goal position to ensure we hit it
		if ( distance < 40000.0f )
		{
			// don't walk if crouching - too slow
			if ( !IsCrouching() && allowSpeedChange )
				Walk();

			if ( distance < 400.0f )
			{
				// reached goal position - path complete
				DestroyPath();

				/// @todo We should push and pop walk state here, in case we want to continue walking after reaching goal
				if ( allowSpeedChange )
					Run();

				return END_OF_PATH;
			}

			// note if we are near the end of the path
			nearEndOfPath = distance < 2500.0f;
		}
	}

	//
	// To keep us moving smoothly, we will move towards
	// a point farther ahead of us down our path.
	//
	int prevIndex = 0;	// closest index on path just prior to where we are now
	const float aheadRange = 300.0f;
	int newIndex = FindPathPoint( aheadRange, &m_goalPosition, &prevIndex );

	// if goal position is near to us, we must be about to go around a corner - so look ahead!
	const float nearCornerRange = 100.0f;
	if ( m_pathIndex < m_pathLength - 1 && (m_goalPosition - pev->origin).IsLengthLessThan( nearCornerRange ) )
	{
		m_lookAtSpotState = 0;
		m_lookAtDesc = NULL;
		InhibitLookAround( 0.5f );
	}

	// if we moved to a new node on the path, setup movement
	if ( newIndex > m_pathIndex )
	{
		SetPathIndex( newIndex );
	}

	//
	// Crouching
	//
	if ( m_pathLadder == NULL )
	{
		// if we are approaching a crouch area, crouch
		// if there are no crouch areas coming up, stand
		const float crouchRange = 50.0f;
		bool didCrouch = false;
		for ( int i = prevIndex; i < m_pathLength; ++i )
		{
			CNavArea *to = m_path[i].area;
			if ( to->GetAttributes() & NAV_JUMP )
				break;

			Vector close;
			to->GetClosestPointOnArea( &pev->origin, &close );

			if ( (close - pev->origin).Make2D().IsLengthGreaterThan( crouchRange ) )
				break;

			if ( to->GetAttributes() & NAV_CROUCH )
			{
				Crouch();
				didCrouch = true;
				break;
			}
		}

		if ( !didCrouch && !IsJumping() )
		{
			// no crouch areas coming up
			StandUp();
		}
	}

	// compute our forward facing angle
	Vector goalDelta = m_goalPosition - pev->origin;
	m_forwardAngle = UTIL_VecToYaw( goalDelta );

	//
	// Look farther down the path to "lead" our view around corners
	//
	Vector toGoal;
	if ( m_pathIndex == 0 )
		toGoal = m_path[1].pos;
	else if ( m_pathIndex >= m_pathLength )
		toGoal = m_path[m_pathLength - 1].pos - pev->origin;
	else
	{
		toGoal = m_path[m_pathIndex].pos - pev->origin;

		// actually aim our view farther down the path
		const float lookAheadRange = 500.0f;
		if ( m_path[m_pathIndex].ladder == NULL && !IsNearJump() && toGoal.Make2D().IsLengthLessThan( lookAheadRange ) )
		{
			float along = toGoal.Length2D();
			int i;
			for ( i = m_pathIndex + 1; i < m_pathLength; ++i )
			{
				Vector delta = m_path[i].pos - m_path[i - 1].pos;
				float total = along + delta.Length2D();
				if ( total >= lookAheadRange )
				{
					// interpolate between points to keep look ahead point at fixed distance
					float t = (lookAheadRange - along) / total;
					Vector target;
					if ( t <= 0.0f )
						target = m_path[i - 1].pos;
					else if ( t >= 1.0f )
						target = m_path[i].pos;
					else
						target = m_path[i - 1].pos + delta * t;
					toGoal = target - pev->origin;
					break;
				}
				along = total;

				// if we are coming up to a ladder or a jump, look at it
				if ( m_path[i].ladder || (m_path[i].area->GetAttributes() & NAV_JUMP) )
				{
					toGoal = m_path[i].pos - pev->origin;
					break;
				}
			}
			if ( i == m_pathLength )
				toGoal = m_path[m_pathLength - 1].pos - pev->origin;
		}
	}
	m_lookAheadAngle = UTIL_VecToYaw( toGoal );

	// initialize "adjusted" goal to current goal
	Vector adjustedGoal = m_goalPosition;

	//
	// Use short "feelers" to veer away from close-range obstacles
	// Feelers come from our ankles, just above StepHeight, so we avoid short walls, too
	// Don't use feelers if very near the end of the path, or about to jump
	//
	/// @todo Consider having feelers at several heights to deal with overhangs, etc.
	if ( !nearEndOfPath && !IsNearJump() && !IsJumping() )
	{
		FeelerReflexAdjustment( &adjustedGoal );
	}

	// draw debug visualization
	if ( (cv_bot_traceview.value == 1.0f && IsLocalPlayerWatchingMe()) || cv_bot_traceview.value == 10.0f )
	{
		if ( m_pathLength )
			DrawPath();

		const Vector &pos = m_path[m_pathIndex].pos;
		UTIL_DrawBeamPoints( pos, pos + Vector( 0, 0, 50 ), 1, 255, 255, 0 );

		UTIL_DrawBeamPoints( adjustedGoal, adjustedGoal + Vector( 0, 0, 50 ), 1, 255, 0, 255 );
		UTIL_DrawBeamPoints( pev->origin, adjustedGoal + Vector( 0, 0, 50 ), 1, 255, 0, 255 );
	}

	// dont use adjustedGoal, as it can vary wildly from the feeler adjustment
	if ( !IsAttacking() && IsFriendInTheWay( &m_goalPosition ) )
	{
		if ( !m_isWaitingBehindFriend )
		{
			m_isWaitingBehindFriend = true;

			const float politeDuration = 5.0f - 3.0f * GetProfile()->GetAggression();
			m_politeTimer.Start( politeDuration );
		}
		else if ( m_politeTimer.IsElapsed() )
		{
			// we have run out of patience
			m_isWaitingBehindFriend = false;
			ResetStuckMonitor();

			// repath to avoid clump of friends in the way
			DestroyPath();
		}
	}
	else if ( m_isWaitingBehindFriend )
	{
		// we're done waiting for our friend to move
		m_isWaitingBehindFriend = false;
		ResetStuckMonitor();
	}

	//
	// Move along our path if there are no friends blocking our way,
	// or we have run out of patience
	//
	if ( !m_isWaitingBehindFriend || m_politeTimer.IsElapsed() )
	{
		//
		// Move along path
		//
		MoveTowardsPosition( &adjustedGoal );

		//
		// Stuck check
		//
		if ( m_isStuck && !IsJumping() )
			Wiggle();
	}

	// if our goal is high above us, we must have fallen
	float feetHeight = pev->origin.z - (IsCrouching() ? 18.0f : 36.0f);
	bool didFall = false;
	if ( m_goalPosition.z - feetHeight > JumpCrouchHeight && (pev->origin - m_goalPosition).Make2D().IsLengthLessThan( 75.0f ) )
	{
		// we can't reach the goal position
		// check if we can reach the next node, in case this was a "jump down" situation
		if ( m_pathIndex >= m_pathLength - 1 || m_path[m_pathIndex + 1].pos.z - feetHeight > JumpCrouchHeight )
		{
			PrintIfWatched( "I fell off!\n" );
			didFall = true;
		}
	}

	//
	// This timeout check is needed if the bot somehow slips way off 
	// of its path and cannot progress, but also moves around
	// enough that it never becomes "stuck"
	//
	if ( !didFall && gpGlobals->time - m_areaEnteredTimestamp <= 5.0f )
		return PROGRESSING;

	// if we havent made any progress in a long time, give up
	if ( m_pathIndex >= m_pathLength - 1 )
		PrintIfWatched( "Giving up trying to get to end of path\n" );
	else
		PrintIfWatched( "Giving up trying to get to area #%d\n", m_path[m_pathIndex].area->GetID() );
	Run();
	StandUp();
	DestroyPath();
	return PATH_FAILURE;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Draw a portion of our current path for debugging.
 */
void CCSBot::DrawPath( void )
{
	if ( m_pathLength == 0 )
		return;

	for ( int i = 1; i < m_pathLength; ++i )
		UTIL_DrawBeamPoints( m_path[i - 1].pos + Vector( 0, 0, 36 ), m_path[i].pos + Vector( 0, 0, 36 ), 2, 255, 75, 0 );

	Vector close;
	if ( FindOurPositionOnPath( &close, true ) >= 0 )
	{
		close.z += cv_bot_nav_zdraw.value;
		UTIL_DrawBeamPoints( close + Vector( 0, 0, 25 ), close, 1, 0, 255, 0 );
		UTIL_DrawBeamPoints( close + Vector( 25, 0, 0 ), close - Vector( 25, 0, 0 ), 1, 0, 255, 0 );
		UTIL_DrawBeamPoints( close + Vector( 0, 25, 0 ), close - Vector( 0, 25, 0 ), 1, 0, 255, 0 );
	}
}
