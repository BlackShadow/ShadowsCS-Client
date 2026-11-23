#include "hostage/hostage.h"
#include "cs_bot_manager.h"
#include <stdlib.h>

void HostageFollowState::OnEnter( CHostageImprov *improv )
{
	improv->Chatter( HOSTAGE_CHATTER_START_FOLLOW, true );

	improv->Agree();

	m_isWaiting = false;
	m_isWaitingForFriend = false;

	improv->MoveTo( improv->GetFeet() );
	m_lastLeaderPos.x = 999999.9f;
	m_lastLeaderPos.y = 999999.9f;
	m_lastLeaderPos.z = 999999.9f;

	m_makeWayTimer.Invalidate();

	m_stopRange = RANDOM_FLOAT( 125, 175 );

	if ( improv->IsTerroristNearby() )
	{
		improv->DelayedChatter( 3, HOSTAGE_CHATTER_WARN_NEARBY, false );
	}
}


void HostageFollowState::OnUpdate( CHostageImprov *improv )
{
	if ( !m_leader )
	{
		improv->Idle();
		return;
	}

	if ( m_leader->pev->deadflag != DEAD_NO )
	{
		improv->Frighten( CHostageImprov::TERRIFIED );
		improv->Idle();
		return;
	}

	float range = ( m_leader->pev->origin - improv->GetCentroid() ).Length();

	const float giveUpRange = 1000.0f;
	const float maxPathLength = 3000.0f;
	if ( range > giveUpRange || ( improv->GetPath()->IsValid() && improv->GetPath()->GetLength() > maxPathLength ) )
	{
		improv->Idle();
		return;
	}

	const float walkRange = m_stopRange + 50.0f;
	const float continueRange = m_stopRange + 20.0f;

	float leaderSpeed = m_leader->pev->velocity.Make2D().Length();
	const float runThreshold = 140.0f;
	bool isLeaderRunning = ( leaderSpeed > runThreshold );

	if ( isLeaderRunning )
	{
		m_isWaiting = false;
	}
	else
	{
		if ( m_isWaiting )
		{
			if ( range > continueRange )
				m_isWaiting = false;
		}
		else
		{
			if ( range < m_stopRange )
			{
				m_isWaiting = true;
				m_impatientTimer.Start( RANDOM_FLOAT( 5, 20 ) );
			}
		}
	}
	if ( m_isWaiting )
	{
		if ( m_impatientTimer.IsElapsed() && !TheCSBots()->IsRoundOver() )
		{
			m_impatientTimer.Start( RANDOM_FLOAT( 20, 30 ) );

			if ( improv->CanSeeRescueZone() )
				improv->Chatter( HOSTAGE_CHATTER_SEE_RESCUE_ZONE, false );
			else
				improv->Chatter( HOSTAGE_CHATTER_IMPATIENT_FOR_RESCUE, false );
		}

		const float closeRange = 200.0f;
		if ( ( m_leader->pev->origin - improv->GetCentroid() ).IsLengthLessThan( closeRange ) )
		{
			bool makeWay = false;

			const float cosTolerance = 0.99f;
			if ( improv->IsPlayerLookingAtMe( (CBasePlayer *)(CBaseEntity *)m_leader, cosTolerance ) )
			{
				if ( m_makeWayTimer.HasStarted() )
				{
					makeWay = m_makeWayTimer.IsElapsed();
				}
				else
				{
					m_makeWayTimer.Start( RANDOM_FLOAT( 0.4f, 0.75f ) );
				}
			}
			else
			{
				m_makeWayTimer.Invalidate();
			}

			if ( makeWay )
			{
				m_impatientTimer.Invalidate();

				Vector to = m_leader->pev->origin - improv->GetCentroid();
				to.NormalizeInPlace();

				Vector lat( to.y, -to.x, 0.0f );
				if ( DotProduct( lat, gpGlobals->v_forward ) > 0 )
					lat = -lat;

				const float sideStepSize = 15.0f;
				Vector sideStepPos = improv->GetFeet() + lat * sideStepSize;

				float ground;
				if ( GetGroundHeight( &sideStepPos, &ground, NULL ) )
				{
					if ( fabs( ground - improv->GetFeet().z ) < 18 )
					{
						const float push = 20.0f;
						improv->ApplyForce( push * lat );

						improv->MoveTo( sideStepPos );
						return;
					}
				}
			}
		}

		improv->Stop();

		CBasePlayer *terrorist = improv->GetClosestVisiblePlayer( TERRORIST );
		if ( terrorist )
			improv->LookAt( terrorist->EyePosition() );
		else
			improv->LookAt( m_leader->EyePosition() );

		return;
	}

	if ( improv->IsFriendInTheWay() )
	{
		bool doWait = true;

		if ( m_isWaitingForFriend )
		{
			if ( m_waitForFriendTimer.IsElapsed() )
			{
				doWait = false;
			}
		}
		else
		{
			m_isWaitingForFriend = true;
			m_waitForFriendTimer.Start( 15 );
		}

		if ( doWait )
		{
			improv->Stop();
			return;
		}
	}

	if ( !improv->GetPath()->IsValid() && m_repathTimer.IsElapsed() )
	{
		improv->MoveTo( m_leader->pev->origin );
		m_lastLeaderPos = m_leader->pev->origin;
		m_repathTimer.Start( 1 );
	}

	if ( isLeaderRunning || range > walkRange )
		improv->Run();
	else
		improv->Walk();

	const float movedRange = 35.0f;
	if ( ( m_lastLeaderPos - m_leader->pev->origin ).IsLengthGreaterThan( movedRange ) )
	{
		improv->MoveTo( m_leader->pev->origin );
		m_lastLeaderPos = m_leader->pev->origin;
	}
}


void HostageFollowState::OnExit( CHostageImprov *improv )
{
	improv->Stop();
}


void HostageFollowState::UpdateStationaryAnimation( CHostageImprov *improv )
{
	if ( improv->IsScared() )
		improv->UpdateIdleActivity( ACT_FOLLOW_IDLE_SCARED, ACT_RESET );
	else
		improv->UpdateIdleActivity( ACT_FOLLOW_IDLE, ACT_FOLLOW_IDLE_FIDGET );
}
