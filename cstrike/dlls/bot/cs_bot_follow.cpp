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
#include "cs_bot_manager.h"

//--------------------------------------------------------------------------------------------------------------
/**
 * Follow our leader
 */
void FollowState::OnEnter( CCSBot *me )
{
	me->StandUp();
	me->Run();
	me->DestroyPath();

	m_isStopped = false;
	m_stoppedTimestamp = 0.0f;

	// to force immediate repath
	m_lastLeaderPos = Vector( -100000000.0f, -100000000.0f, -100000000.0f );

	m_lastSawLeaderTime = 0.0f;

	// set re-pathing frequency
	m_repathInterval.Invalidate();

	m_isSneaking = false;

	m_walkTime.Invalidate();
	m_isAtWalkSpeed = false;

	m_leaderMotionState = INVALID;
	m_idleTimer.Start( RANDOM_FLOAT( 2.0f, 5.0f ) );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Determine the leader's motion state by tracking his speed
 */
void FollowState::ComputeLeaderMotionState( float leaderSpeed )
{
	// walk = 130, run = 250
	const float runWalkThreshold = 140.0f;
	const float walkStopThreshold = 10.0f; // 120.0f;
	LeaderMotionStateType prevState = m_leaderMotionState;
	if ( leaderSpeed > runWalkThreshold )
	{
		m_leaderMotionState = RUNNING;
		m_isAtWalkSpeed = false;
	}
	else if ( leaderSpeed > walkStopThreshold )
	{
		// track when began to walk
		if ( !m_isAtWalkSpeed )
		{
			m_walkTime.Start();
			m_isAtWalkSpeed = true;
		}

		const float minWalkTime = 0.25f;
		if ( m_walkTime.GetElapsedTime() > minWalkTime )
		{
			m_leaderMotionState = WALKING;
		}
	}
	else
	{
		m_leaderMotionState = STOPPED;
		m_isAtWalkSpeed = false;
	}

	// track time spent in this motion state
	if ( prevState != m_leaderMotionState )
	{
		m_leaderMotionStateTime.Start();
		m_waitTime = RANDOM_FLOAT( 1.0f, 3.0f );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Functor to collect all areas in the forward direction of the given player within a radius
 */
class FollowTargetCollector
{
public:
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
	FollowTargetCollector( CBaseEntity *player )
	{
		m_player = player;

		m_forward = player->pev->velocity.Make2D();
		float speed = m_forward.NormalizeInPlace();

		const float walkSpeed = 100.0f;
		if ( speed < walkSpeed )
		{
			m_forward = Vector2D( 0.0f, 0.0f );
			m_cutoff = player->pev->origin.Make2D();
		}
		else
		{
			float distance = speed > 200.0f ? 300.0f : speed * 1.5f;
			m_cutoff = player->pev->origin.Make2D() + m_forward * distance;
		}

		m_targetAreaCount = 0;
	}

	enum { MAX_TARGET_AREAS = 128 };

	bool operator()( CNavArea *area )
	{
		if ( m_targetAreaCount >= MAX_TARGET_AREAS )
			return false;

		// only use two-way connections
		if ( !area->GetParent() || area->IsConnected( area->GetParent(), NUM_DIRECTIONS ) )
		{
			if ( m_forward.x > -0.01f && m_forward.x < 0.01f && m_forward.y > -0.01f && m_forward.y < 0.01f )
			{
				m_targetArea[ m_targetAreaCount++ ] = area;
			}
			else
			{
				// collect areas in the direction of the player's forward motion
				Vector2D to = area->GetCenter()->Make2D() - m_cutoff;
				to.NormalizeInPlace();

				if ( DotProduct( to, m_forward ) > 0.7071f )
					m_targetArea[ m_targetAreaCount++ ] = area;
			}
		}

		return (m_targetAreaCount < MAX_TARGET_AREAS);
	}

	CBaseEntity *m_player;
	Vector2D m_forward;
	Vector2D m_cutoff;

	CNavArea *m_targetArea[ MAX_TARGET_AREAS ];
	int m_targetAreaCount;
};

//--------------------------------------------------------------------------------------------------------------
/**
 * Follow our leader
 * @todo Clean up this nasty mess
 */
void FollowState::OnUpdate( CCSBot *me )
{
	// if we lost our leader, give up
	if ( !m_leader || !m_leader->IsAlive() )
	{
		me->Idle();
		return;
	}

	// if we are carrying the bomb and at a bombsite, plant
	if ( me->m_bHasC4 && (me->m_signals.GetState() & SIGNAL_BOMB) )
	{
		// plant it
		me->SetTask( CCSBot::PLANT_BOMB );
		me->PlantBomb();

		// radio to the team
		me->GetChatter()->PlantingTheBomb( me->GetPlace() );

		return;
	}

	// look around
	me->UpdateLookAround( false );

	// if we are moving, we are not idle
	if ( me->pev->velocity.LengthSquared() >= 100.0f )
		m_idleTimer.Start( RANDOM_FLOAT( 2.0f, 5.0f ) );

	// compute the leader's speed
	float leaderSpeed = Vector2D( m_leader->pev->velocity.x, m_leader->pev->velocity.y ).Length();

	// determine our leader's movement state
	ComputeLeaderMotionState( leaderSpeed );

	// track whether we can see the leader
	bool isLeaderVisible;
	if ( me->IsVisible( &m_leader->pev->origin, false ) )
	{
		m_lastSawLeaderTime = gpGlobals->time;
		isLeaderVisible = true;
	}
	else
	{
		isLeaderVisible = false;
	}

	// determine whether we should sneak or not
	const float farAwayRange = 750.0f;
	if ( (m_leader->pev->origin - me->pev->origin).IsLengthGreaterThan( farAwayRange ) )
	{
		// far away from leader - run to catch up
		m_isSneaking = false;
	}
	else if ( isLeaderVisible )
	{
		// if we see leader walking and we are nearby, walk
		if ( m_leaderMotionState == WALKING )
			m_isSneaking = true;

		// if we are sneaking and our leader starts running, stop sneaking
		if ( m_isSneaking && m_leaderMotionState == RUNNING )
			m_isSneaking = false;
	}

	// if we haven't seen the leader for a long time, run
	const float longTime = 20.0f;
	if ( gpGlobals->time - m_lastSawLeaderTime > longTime )
		m_isSneaking = false;

	if ( m_isSneaking )
		me->Walk();
	else
		me->Run();

	bool repath = false;

	// if the leader has stopped, hide nearby
	const float nearLeaderRange = 250.0f;
	if ( !me->m_pathLength && m_leaderMotionState == STOPPED && m_leaderMotionStateTime.GetElapsedTime() > m_waitTime )
	{
		// throttle how often this check occurs
		m_waitTime += RANDOM_FLOAT( 1.0f, 3.0f );

		// the leader has stopped - if we are close to him, take up a hiding spot
		if ( (m_leader->pev->origin - me->pev->origin).IsLengthLessThan( nearLeaderRange ) )
		{
			const float hideRange = 250.0f;
			if ( me->TryToHide( NULL, -1.0f, hideRange, false, true ) )
			{
				me->ResetStuckMonitor();
				return;
			}
		}
	}

	// if we have been idle for awhile, move
	if ( m_idleTimer.IsElapsed() )
	{
		repath = true;

		// always walk when we move such a short distance
		m_isSneaking = true;
	}

	// if our leader has moved, repath (don't repath if leading is stopping)
	if ( leaderSpeed > 100.0f && m_leaderMotionState != STOPPED )
	{
		repath = true;
	}

	// move along our path
	if ( me->UpdatePathMovement( false ) != CCSBot::PROGRESSING )
	{
		me->DestroyPath();
	}

	// recompute our path if necessary
	if ( repath && m_repathInterval.IsElapsed() )
	{
		// recompute our path to keep us near our leader
		m_lastLeaderPos = m_leader->pev->origin;

		me->ResetStuckMonitor();

		const float runSpeed = 200.0f;

		const float collectRange = (leaderSpeed > runSpeed) ? 600.0f : 400.0f;		// 400, 200
		FollowTargetCollector collector( m_leader );
		SearchSurroundingAreas( TheNavAreaGrid.GetNearestNavArea( &m_lastLeaderPos, false ), &m_lastLeaderPos, collector, collectRange );

		if ( cv_bot_debug.value > 0.0f )
		{
			for ( int i = 0; i < collector.m_targetAreaCount; ++i )
				collector.m_targetArea[i]->Draw( 255, 0, 0, 2 );
		}

		// move to one of the collected areas
		if ( collector.m_targetAreaCount )
		{
			CNavArea *target = NULL;

			// if we are idle, pick a random area
			if ( m_idleTimer.IsElapsed() )
			{
				target = collector.m_targetArea[ RANDOM_LONG( 0, collector.m_targetAreaCount - 1 ) ];
				me->PrintIfWatched( "%4.1f: Bored. Repathing to a new nearby area\n", gpGlobals->time );
			}
			else
			{
				me->PrintIfWatched( "%4.1f: Repathing to stay with leader.\n", gpGlobals->time );

				// find closest area to where we are
				CNavArea *area;
				float closeRangeSq = 10000000000.0f;
				Vector close;

				for ( int a = 0; a < collector.m_targetAreaCount; ++a )
				{
					area = collector.m_targetArea[a];

					area->GetClosestPointOnArea( &me->pev->origin, &close );

					float rangeSq = (me->pev->origin - close).LengthSquared();
					if ( rangeSq < closeRangeSq )
					{
						target = area;
						closeRangeSq = rangeSq;
					}
				}
			}

			if ( !me->ComputePath( target, NULL, FASTEST_ROUTE ) )
				me->PrintIfWatched( "Pathfind to leader failed.\n" );

			// throttle how often we repath
			m_repathInterval.Start( 0.5f );

			m_idleTimer.Reset();
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
void FollowState::OnExit( CCSBot * )
{
}

const char *FollowState::GetName() const
{
	return "Follow";
}
