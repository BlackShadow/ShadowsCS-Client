//========= Copyright (c) 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

#pragma warning( push )
#pragma warning( disable: 4100 )
#include "cs_bot.h"
#include "cs_bot_manager.h"
#include "hostage.h"
#pragma warning( pop )

//--------------------------------------------------------------------------------------------------------------
/**
 * Begin the hunt
 */
void HuntState::OnEnter( CCSBot *me )
{
	// lurking death
	if ( me->IsUsingKnife() && me->IsWellPastSafe() && !me->IsHurrying() )
		me->Walk();
	else
		me->Run();

	me->StandUp();
	me->SetDisposition( CCSBot::ENGAGE_AND_INVESTIGATE );
	me->SetTask( CCSBot::SEEK_AND_DESTROY );

	me->DestroyPath();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Hunt down our enemies
 */
void HuntState::OnUpdate( CCSBot *me )
{
	// if we've been hunting for a long time, drop into Idle for a moment to
	// select something else to do
	const float huntingTooLongTime = 30.0f;
	if ( gpGlobals->time - me->m_stateTimestamp > huntingTooLongTime )
	{
		// stop being a rogue and do the scenario, since there must not be many enemies left to hunt
		me->PrintIfWatched( "Giving up hunting, and being a rogue\n" );
		me->m_isRogue = false;
		me->Idle();
		return;
	}

	CCSBotManager *manager = TheCSBots();

	// scenario logic
	if ( manager->GetScenario() == CCSBotManager::SCENARIO_DEFUSE_BOMB )
	{
		if ( me->m_iTeam == TERRORIST )
		{
			// if we have the bomb and it's time to plant, or we happen to be in a bombsite and it seems safe, do it
			if ( me->m_bHasC4 )
			{
				const float safeTime = 3.0f;

				if ( gpGlobals->time >= manager->m_earliestBombPlantTimestamp ||
					((me->m_signals.GetState() & SIGNAL_BOMB) && gpGlobals->time - me->m_lastSawEnemyTimestamp > safeTime) )
				{
					me->Idle();
					return;
				}
			}

			// if we notice the bomb lying on the ground, go get it
			if ( me->NoticeLooseBomb() )
			{
				me->FetchBomb();
				return;
			}

			// if bomb has been planted, and we hear it, move to a hiding spot near the bomb and watch it
			if ( !me->IsRogue() && me->GetGameState()->GetBombState() == CSGameState::PLANTED &&
				me->GetGameState()->GetBombPosition() )
			{
				me->SetTask( CCSBot::GUARD_TICKING_BOMB );
				const Vector *position = me->GetGameState()->GetBombPosition();
				CNavArea *area = TheNavAreaGrid.GetNavArea( position );
				me->Hide( area, -1, 750, false );
				return;
			}
		}
		else		// CT
		{
			if ( !me->IsRogue() && me->CanSeeLooseBomb() )
			{
				// if we are near the loose bomb and can see it, hide nearby and guard it
				me->SetTask( CCSBot::GUARD_LOOSE_BOMB );
				me->Hide( manager->GetLooseBombArea(), -1, 750, false );
				me->GetChatter()->AnnouncePlan( "GoingToGuardLooseBomb", manager->GetLooseBombArea()->GetPlace() );
				return;
			}
			else if ( manager->IsBombPlanted() )
			{
				// rogues will defuse a bomb, but not guard the defuser
				if ( !me->IsRogue() || !manager->GetBombDefuser() )
				{
					// search for the planted bomb to defuse
					me->Idle();
					return;
				}
			}
		}
	}
	else if ( manager->GetScenario() == CCSBotManager::SCENARIO_RESCUE_HOSTAGES )
	{
		if ( me->m_iTeam == TERRORIST )
		{
			if ( me->GetGameState()->AreAllHostagesBeingRescued() )
			{
				// all hostages are being rescued, head them off at the escape zones
				if ( me->GuardRandomZone( 500 ) )
				{
					me->SetTask( CCSBot::GUARD_HOSTAGE_RESCUE_ZONE );
					me->PrintIfWatched( "Trying to beat them to an escape zone!\n" );
					me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
					me->GetChatter()->GuardingHostageEscapeZone( true );
					return;
				}
			}

			// if safe time is up, and we stumble across a hostage, guard it
			if ( !me->IsRogue() && !me->IsSafe() )
			{
				CHostage *hostage = me->GetGameState()->GetNearestVisibleFreeHostage();
				if ( hostage )
				{
					CNavArea *area = TheNavAreaGrid.GetNearestNavArea( &hostage->pev->origin );
					if ( area )
					{
						// we see a free hostage, guard it
						me->SetTask( CCSBot::GUARD_HOSTAGES );
						me->Hide( area, -1, 750, false );
						me->PrintIfWatched( "I'm guarding hostages\n" );
						me->GetChatter()->GuardingHostages( area->GetPlace(), true );
						return;
					}
				}
			}
		}
	}

	// listen for enemy noises
	if ( me->ShouldInvestigateNoise( NULL ) )
	{
		me->InvestigateNoise();
		return;
	}

	// look around
	me->UpdateLookAround( false );

	// if we have reached our destination area, pick a new one
	// if our path fails, pick a new one
	if ( me->GetLastKnownArea() == m_huntArea || me->UpdatePathMovement() != CCSBot::PROGRESSING )
	{
		m_huntArea = NULL;
		double oldest = 0;

		int areaCount = 0;
		const float minSize = 150.0f;

		for ( NavAreaList::iterator it = TheNavAreaList.begin(); it != TheNavAreaList.end(); ++it )
		{
			CNavArea *area = *it;

			++areaCount;

			// skip the small areas
			const Extent *extent = area->GetExtent();
			if ( extent->hi.x - extent->lo.x < minSize || extent->hi.y - extent->lo.y < minSize )
				continue;

			// keep track of the least recently cleared area
			double age = gpGlobals->time - area->GetClearedTimestamp( me->m_iTeam - 1 );
			if ( age > oldest )
			{
				oldest = age;
				m_huntArea = area;
			}
		}

		// if all the areas were too small, pick one at random
		int which = RANDOM_LONG( 0, areaCount - 1 );

		areaCount = 0;
		for ( NavAreaList::iterator it = TheNavAreaList.begin(); it != TheNavAreaList.end(); ++it )
		{
			m_huntArea = *it;

			if ( which == areaCount )
				break;

			--which;
		}

		// create a new path to a far away area of the map
		me->ComputePath( m_huntArea, NULL, SAFEST_ROUTE );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Done hunting
 */
void HuntState::OnExit( CCSBot * )
{
}

const char *HuntState::GetName() const
{
	return "Hunt";
}
