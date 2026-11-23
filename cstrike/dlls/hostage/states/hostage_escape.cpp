#include "hostage/hostage.h"
#include "cs_bot_manager.h"

void HostageEscapeToCoverState::OnEnter( CHostageImprov *improv )
{
	improv->GetPath()->Invalidate();
	m_canEscape = false;

	CNavPath path;
	HostagePathCost pathCost;
	if ( !path.Compute( &improv->GetFeet(), &m_rescueGoal, pathCost ) )
		return;

	const float moveRange = 500.0f;
	int idx = path.GetSegmentIndexAlongPath( moveRange );

	if ( idx < 0 )
		return;

	if ( idx < path.GetSegmentCount() - 1 )
		++idx;

	Vector pathPos = path[idx]->pos;

	const float hidingRange = 450.0f;
	const Vector *spot = FindNearbyHidingSpot( improv->GetEntity(),
		&pathPos,
		TheNavAreaGrid.GetNearestNavArea( &pathPos ),
		hidingRange, false, false );
	if ( !spot )
		spot = &pathPos;
	m_spot = *spot;

	improv->Run();
	improv->MoveTo( m_spot );

	m_canEscape = true;
}


void HostageEscapeToCoverState::OnUpdate( CHostageImprov *improv )
{
	if ( !m_canEscape )
	{
		improv->Idle();
		return;
	}

	if ( IsSpotOccupied( improv->GetEntity(), &m_spot ) )
	{
		const float emergencyHidingRange = 300.0f;
		const Vector *spot = FindNearbyHidingSpot( improv->GetEntity(),
			&improv->GetFeet(),
			improv->GetLastKnownArea(),
			emergencyHidingRange, false, false );
		if ( !spot )
		{
			HostageEscapeState *escape = (HostageEscapeState *)GetParent();
			escape->LookAround();
			return;
		}

		m_spot = *spot;
		improv->MoveTo( m_spot );
	}

	if ( improv->IsAtMoveGoal( 20 ) )
	{
		HostageEscapeState *escape = (HostageEscapeState *)GetParent();
		escape->LookAround();
	}
}


void HostageEscapeToCoverState::OnExit( CHostageImprov *improv )
{
}


void HostageEscapeToCoverState::OnMoveToFailure( const Vector &goal, MoveToFailureType reason )
{
	HostageEscapeState *escape = (HostageEscapeState *)GetParent();
	escape->LookAround();
}


void HostageEscapeLookAroundState::OnEnter( CHostageImprov *improv )
{
	m_timer.Start( RANDOM_FLOAT( 5, 10 ) );

	improv->Stop();
	improv->FaceOutwards();
}


void HostageEscapeLookAroundState::OnUpdate( CHostageImprov *improv )
{
	improv->UpdateIdleActivity( ACT_IDLE_SNEAKY, ACT_IDLE_SNEAKY_FIDGET );

	if ( m_timer.IsElapsed() )
	{
		HostageEscapeState *escape = (HostageEscapeState *)GetParent();
		escape->ToCover();
	}
}


void HostageEscapeLookAroundState::OnExit( CHostageImprov *improv )
{
	improv->ClearFaceTo();
}


void HostageEscapeState::OnEnter( CHostageImprov *improv )
{
	CCSBotManager *ctrl = TheCSBots();
	const CCSBotManager::Zone *zone = ctrl->GetRandomZone();

	if ( zone )
	{
		m_toCoverState.SetRescueGoal( zone->m_center );

		m_behavior.Reset( improv );
		m_behavior.SetState( &m_toCoverState );

		m_canEscape = true;
	}
	else
	{
		m_canEscape = true;
	}
}


void HostageEscapeState::OnUpdate( CHostageImprov *improv )
{
	if ( !m_canEscape )
	{
		improv->Stop();
		improv->Idle();
		return;
	}

	if ( improv->IsScared() && improv->GetScareIntensity() == CHostageImprov::TERRIFIED )
	{
		improv->Stop();
		improv->Idle();
		return;
	}

	if ( m_runTimer.IsElapsed() )
		improv->Walk();
	else
		improv->Run();

	CBasePlayer *player = improv->GetClosestVisiblePlayer( UNASSIGNED );
	if ( player )
	{
		if ( player->m_iTeam == TERRORIST )
		{
			const float farRange = 750.0f;
			if ( ( player->pev->origin - improv->GetCentroid() ).IsLengthGreaterThan( farRange ) )
			{
				improv->Frighten( CHostageImprov::NERVOUS );
				m_runTimer.Start( RANDOM_FLOAT( 3, 6 ) );
				ToCover();
				return;
			}
			else
			{
				improv->Frighten( CHostageImprov::SCARED );
				improv->Stop();
				improv->Idle();
				return;
			}
		}
		else
		{
			improv->Stop();
			improv->Idle();
			return;
		}
	}

	m_behavior.Update();
}


void HostageEscapeState::OnExit( CHostageImprov *improv )
{
	improv->Run();
}
