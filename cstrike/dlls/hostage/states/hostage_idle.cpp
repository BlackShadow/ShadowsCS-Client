#include "hostage/hostage.h"
#include "cs_bot_manager.h"

void HostageIdleState::OnEnter( CHostageImprov *improv )
{
	m_moveState = MoveDone;
	m_fleeTimer.Invalidate();
	m_mustFlee = false;
}


void HostageIdleState::OnUpdate( CHostageImprov *improv )
{
	if ( !UTIL_ActivePlayersInGame() )
		return;

	CHostage *hostage = improv->GetEntity();

	if ( m_mustFlee || ( improv->IsScared() && !improv->IsTerroristNearby() && m_moveState != Moving ) )
	{
		m_mustFlee |= improv->GetScareIntensity() == CHostageImprov::TERRIFIED;

		if ( m_mustFlee || ( m_fleeTimer.IsElapsed() && improv->GetScareIntensity() > CHostageImprov::NERVOUS ) )
		{
			m_fleeTimer.Start( RANDOM_FLOAT( 10, 20 ) );

			if ( !m_mustFlee )
			{
				const float terroristRecentTime = 5.0f;
				if ( improv->GetTimeSinceLastSawPlayer( TERRORIST ) > terroristRecentTime )
				{
					const float fleeChance = 33.3f;
					if ( RANDOM_FLOAT( 0, 100 ) < fleeChance )
						m_mustFlee = true;
				}
			}

			if ( m_mustFlee )
			{
				m_mustFlee = false;

				const Vector *spot = FindNearbyRetreatSpot( NULL, &improv->GetFeet(), improv->GetLastKnownArea(), 500, TERRORIST, false );
				if ( spot )
				{
					improv->MoveTo( *spot );
					improv->Run();
					m_moveState = Moving;

					if ( improv->GetScareIntensity() == CHostageImprov::TERRIFIED )
						improv->Frighten( CHostageImprov::SCARED );

					return;
				}
			}
		}
	}

	if ( m_moveState != NotMoving && improv->IsAtMoveGoal( 20 ) )
	{
		m_moveState = NotMoving;

		improv->Stop();
		improv->FaceOutwards();

		if ( improv->IsScared() && !improv->IsAtHome() )
		{
			const float crouchChance = 33.3f;
			if ( RANDOM_FLOAT( 0, 100 ) <= crouchChance )
				improv->Crouch();
		}

		return;
	}

	if ( m_moveState == Moving )
	{
		improv->Run();
		return;
	}

	const float pushbackRange = 75.0f;
	if ( !improv->IsAtMoveGoal( pushbackRange ) )
	{
		improv->Walk();
		m_moveState = Moving;
		return;
	}

	CBasePlayer *rescuer = improv->GetClosestVisiblePlayer( CT );
	CBasePlayer *captor = improv->GetClosestVisiblePlayer( TERRORIST );

	if ( rescuer )
	{
		improv->LookAt( rescuer->EyePosition() );
		improv->Stop();

		if ( captor )
		{
			float rangeT = ( improv->GetCentroid() - captor->pev->origin ).Length();

			const float attentionRange = 700.0f;
			if ( rangeT < attentionRange )
			{
				const float cosTolerance = 0.95f;
				if ( improv->IsAnyPlayerLookingAtMe( TERRORIST, cosTolerance ) )
				{
					improv->LookAt( captor->EyePosition() );
				}
				else
				{
					TraceResult result;
					UTIL_TraceLine( rescuer->pev->origin, captor->pev->origin, ignore_monsters, ignore_glass, captor->edict(), &result );
					if ( result.flFraction != 1 )
					{
						if ( m_disagreeTimer.IsElapsed() )
						{
							improv->Disagree();
							m_disagreeTimer.Start( RANDOM_FLOAT( 2, 4 ) );
						}
					}
				}

				return;
			}
		}
		else
		{
			if ( !TheCSBots()->IsRoundOver() && m_askTimer.IsElapsed() )
			{
				const float closeRange = 200.0f;
				if ( ( rescuer->pev->origin - improv->GetCentroid() ).IsLengthLessThan( closeRange ) )
				{
					if ( improv->IsPlayerLookingAtMe( rescuer, 0.99f ) )
					{
						HostageChatterType say = HOSTAGE_CHATTER_PLEASE_RESCUE_ME;

						if ( improv->IsTerroristNearby() )
						{
							say = HOSTAGE_CHATTER_WARN_NEARBY;
						}

						improv->Chatter( say, false );
						m_askTimer.Start( RANDOM_FLOAT( 3, 10 ) );
					}
				}
			}
		}

		const float waveRange = 250.0f;
		if ( m_waveTimer.IsElapsed() && ( rescuer->pev->origin - hostage->pev->origin ).IsLengthGreaterThan( waveRange ) )
		{
			improv->Stop();
			improv->Wave();
			improv->LookAt( rescuer->EyePosition() );
			improv->Chatter( HOSTAGE_CHATTER_CALL_TO_RESCUER, false );
			m_moveState = NotMoving;
			m_waveTimer.Start( RANDOM_FLOAT( 10, 20 ) );
		}
	}
	else if ( captor )
	{
		improv->LookAt( captor->EyePosition() );
		improv->Stop();

		const float closeRange = 200.0f;
		if ( ( captor->pev->origin - improv->GetCentroid() ).IsLengthLessThan( closeRange ) &&
			improv->IsPlayerLookingAtMe( captor, 0.99f ) )
		{
			if ( !m_intimidatedTimer.HasStarted() )
			{
				m_intimidatedTimer.Start();
			}

			if ( !improv->IsScared() )
			{
				improv->Frighten( CHostageImprov::NERVOUS );
			}

			const float minThreatenTime = 1.0f;
			if ( m_intimidatedTimer.GetElapsedTime() > minThreatenTime && m_pleadTimer.IsElapsed() )
			{
				improv->Chatter( HOSTAGE_CHATTER_INTIMIDATED, true );
				m_pleadTimer.Start( RANDOM_FLOAT( 10, 20 ) );
			}

			if ( !improv->IsAtHome() )
			{
				improv->Chatter( HOSTAGE_CHATTER_RETREAT, true );
				improv->Retreat();
			}
		}
		else
		{
			m_intimidatedTimer.Invalidate();
		}
	}
	else
	{
		improv->ClearLookAt();

		const float stayHomeDuration = 60.0f - 5.0f * improv->GetAggression();
		if ( TheCSBots()->GetElapsedRoundTime() > stayHomeDuration )
		{
			if ( m_escapeTimer.IsElapsed() )
			{
				m_escapeTimer.Start( 5 );

				float sightTimeT = improv->GetTimeSinceLastSawPlayer( TERRORIST );
				float sightTimeCT = improv->GetTimeSinceLastSawPlayer( CT );
				const float waitTime = 15.0f - 3.0f * improv->GetAggression();

				if ( sightTimeT > waitTime && sightTimeCT > waitTime )
				{
					if ( improv->IsTerroristNearby() )
					{
						if ( cv_hostage_debug.value > 0 )
							CONSOLE_ECHO( "Hostage: I want to escape, but a T is nearby\n" );

						m_escapeTimer.Start( waitTime );
					}
					else
					{
						improv->Escape();

						if ( cv_hostage_debug.value > 0 )
							CONSOLE_ECHO( "Hostage: I'm escaping!\n" );
					}
				}
			}
		}
	}
}


void HostageIdleState::OnExit( CHostageImprov *improv )
{
	improv->StandUp();
	improv->ClearFaceTo();
}


void HostageIdleState::UpdateStationaryAnimation( CHostageImprov *improv )
{
	if ( improv->IsScared() )
	{
		if ( improv->GetScareIntensity() == CHostageImprov::TERRIFIED )
			improv->Afraid();
		else
			improv->UpdateIdleActivity( ACT_IDLE_SCARED, ACT_RESET );
	}
	else if ( improv->IsAtHome() )
	{
		improv->UpdateIdleActivity( ACT_IDLE, ACT_IDLE_FIDGET );
	}
	else
	{
		improv->UpdateIdleActivity( ACT_IDLE_SNEAKY, ACT_IDLE_SNEAKY_FIDGET );
	}
}
