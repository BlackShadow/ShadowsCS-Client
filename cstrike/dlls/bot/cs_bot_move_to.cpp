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
 * Move to a potentially far away position.
 */
void MoveToState::OnEnter( CCSBot *me )
{
	if ( me->IsUsingKnife() && me->IsWellPastSafe() && !me->IsHurrying() )
	{
		me->Walk();
	}
	else
	{
		me->Run();
	}

	// if we need to find the bomb, get there as quick as we can
	RouteType route;
	switch ( me->GetTask() )
	{
		case CCSBot::FIND_TICKING_BOMB:
		case CCSBot::DEFUSE_BOMB:
		case CCSBot::MOVE_TO_LAST_KNOWN_ENEMY_POSITION:
			route = FASTEST_ROUTE;
			break;

		default:
			route = SAFEST_ROUTE;
			break;
	}

	// build path to, or nearly to, goal position
	me->ComputePath( TheNavAreaGrid.GetNavArea( &m_goalPosition ), &m_goalPosition, route );

	m_radioedPlan = false;
	m_askedForCover = false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Move to a potentially far away position.
 */
void MoveToState::OnUpdate( CCSBot *me )
{
	// assume that we are paying attention and close enough to know our enemy died
	if ( me->m_task == CCSBot::MOVE_TO_LAST_KNOWN_ENEMY_POSITION )
	{
		/// @todo Account for reaction time so we take some time to realized the enemy is dead
		CBasePlayer *victim = (CBasePlayer *)(CBaseEntity *)me->m_taskEntity;
		if ( !victim || !victim->IsAlive() )
		{
			me->PrintIfWatched( "The enemy I was chasing was killed - giving up.\n" );
			me->Idle();
			return;
		}
	}

	// look around
	me->UpdateLookAround( false );

	CCSBotManager *manager = TheCSBots();

	//
	// Scenario logic
	//
	if ( manager->GetScenario() == CCSBotManager::SCENARIO_DEFUSE_BOMB )
	{
		// if the bomb has been planted, find it
		// NOTE: This task is used by both CT and T's to find the bomb
		if ( me->m_task == CCSBot::FIND_TICKING_BOMB )
		{
			if ( me->GetGameState()->GetBombState() != CSGameState::PLANTED )
			{
				// the bomb is not planted - give up this task
				me->Idle();
				return;
			}

			if ( me->GetGameState()->GetPlantedBombsite() != CSGameState::UNKNOWN )
			{
				// we know where the bomb is planted, stop searching
				me->Idle();
				return;
			}

			// check off bombsites that we explore or happen to stumble into
			for ( int z = 0; z < manager->GetZoneCount(); ++z )
			{
				// don't re-check zones
				if ( me->GetGameState()->IsBombsiteClear( z ) )
					continue;

				if ( manager->GetZone( z )->m_extent.Contains( &me->pev->origin ) )
				{
					// note this bombsite is clear
					me->GetGameState()->ClearBombsite( z );

					if ( me->m_iTeam == CT )
					{
						// tell teammates this bombsite is clear
						me->GetChatter()->BombsiteClear( z );
					}

					// find another zone to check
					me->Idle();

					return;
				}
			}
		}
		else if ( me->m_iTeam == CT )
		{
			if ( me->GetGameState()->GetBombState() == CSGameState::PLANTED )
			{
				if ( me->m_task != CCSBot::DEFUSE_BOMB )
				{
					// we need to find the bomb
					me->Idle();
					return;
				}

				// if we are trying to defuse the bomb, and someone has started defusing, guard them instead
				if ( me->CanSeePlantedBomb() && manager->GetBombDefuser() )
				{
					me->GetChatter()->Say( "CoveringFriend" );
					me->Idle();
					return;
				}
			}
		}
		else		// TERRORIST
		{
			if ( me->m_task == CCSBot::PLANT_BOMB )
			{
				if ( me->GetFriendsRemaining() )
				{
					// if we are about to plant, radio for cover
					if ( !m_askedForCover )
					{
						const float nearPlantSite = 50.0f;
						if ( (me->m_signals.GetState() & SIGNAL_BOMB) && me->GetPathDistanceRemaining() < nearPlantSite )
						{
							// radio to the team
							me->GetChatter()->PlantingTheBomb( me->GetPlace() );
							m_askedForCover = true;
						}

						// after we have started to move to the bombsite, tell team we're going to plant, and where
						// don't do this if we have already radioed that we are starting to plant
						if ( !m_radioedPlan )
						{
							const float radioTime = 2.0f;
							if ( gpGlobals->time - me->m_stateTimestamp > radioTime )
							{
								me->GetChatter()->GoingToPlantTheBomb( TheNavAreaGrid.GetPlace( &m_goalPosition ) );
								m_radioedPlan = true;
							}
						}
					}
				}
			}
		}
	}
	else if ( manager->GetScenario() == CCSBotManager::SCENARIO_RESCUE_HOSTAGES )
	{
		if ( me->m_task == CCSBot::COLLECT_HOSTAGES )
		{
			//
			// Since CT's have a radar, they can directly look at the actual hostage state
			//

			// check if someone else collected our hostage, or the hostage died or was rescued
			CHostage *hostage = (CHostage *)(CBaseEntity *)me->m_goalEntity;
			if ( !hostage || !hostage->IsValid() || hostage->IsFollowingSomeone() )
			{
				me->Idle();
				return;
			}

			// if our hostage has moved, repath
			const float repathToleranceSq = 75.0f * 75.0f;
			float error = (hostage->pev->origin - m_goalPosition).LengthSquared();
			if ( error > repathToleranceSq )
			{
				m_goalPosition = hostage->pev->origin;
				me->ComputePath( TheNavAreaGrid.GetNavArea( &m_goalPosition ), &m_goalPosition, SAFEST_ROUTE );
			}

			/// @todo Generalize ladder priorities over other tasks
			if ( !me->m_pathLadder )
			{
				Vector head = hostage->pev->origin + Vector( 0, 0, 54 );
				float rangeSquared = (head - me->pev->origin).LengthSquared();

				// look at the hostage as we approach
				if ( rangeSquared < 100 * 100 )
				{
					me->SetLookAt( "Hostage", &head, PRIORITY_LOW, 0.5f, false, 5 );

					// randomly move just a bit to avoid infinite use loops from bad hostage placement
					NavRelativeDirType dir = (NavRelativeDirType)RANDOM_LONG( 0, 3 );
					switch ( dir )
					{
						case LEFT:		me->StrafeLeft(); break;
						case RIGHT:		me->StrafeRight(); break;
						case FORWARD:	me->MoveForward(); break;
						case BACKWARD:	me->MoveBackward(); break;
					}

					// check if we are close enough to the hostage to talk to him
					if ( rangeSquared < 50 * 50 )
					{
						me->UseEntity( me->m_goalEntity );
						return;
					}
				}
			}
		}
		else if ( me->m_task == CCSBot::RESCUE_HOSTAGES )
		{
			// periodically check if we lost all our hostages
			if ( !me->m_hostageEscortCount )
			{
				// lost our hostages - go get 'em
				me->Idle();
				return;
			}
		}
	}

	if ( me->UpdatePathMovement() != CCSBot::PROGRESSING )
	{
		// reached destination
		if ( me->m_task == CCSBot::DEFUSE_BOMB )
		{
			// if we are near the bomb, defuse it (if we are reloading, don't try to defuse until we finish)
			CBasePlayerWeapon *weapon = (CBasePlayerWeapon *)me->m_pActiveItem;
			if ( weapon && (weapon->m_fInReload || weapon->m_fInSpecialReload) )
				return;

			const Vector *bombPos = me->GetGameState()->GetBombPosition();
			if ( bombPos )
			{
				Vector feet = me->pev->origin;
				feet.z -= me->IsCrouching() ? 18.0f : 36.0f;

				const float defuseRange = 100.0f;		// 50
				if ( (*bombPos - feet).IsLengthLessThan( defuseRange ) )
				{
					me->DefuseBomb();
					return;
				}
			}
		}
		else if ( me->m_task == CCSBot::MOVE_TO_LAST_KNOWN_ENEMY_POSITION )
		{
			CBasePlayer *victim = (CBasePlayer *)(CBaseEntity *)me->m_taskEntity;
			if ( victim && victim->IsAlive() )
			{
				// if we got here and haven't re-acquired the enemy, we lost him
				me->GetChatter()->Say( "LostEnemy" );
			}
		}
		else if ( me->m_task == CCSBot::PLANT_BOMB )
		{
			// if we are at bombsite with the bomb, plant it
			if ( (me->m_signals.GetState() & SIGNAL_BOMB) && me->m_bHasC4 )
			{
				me->PlantBomb();
				return;
			}
		}

		// default behavior when destination is reached
		me->Idle();
		return;
	}
}

//--------------------------------------------------------------------------------------------------------------
void MoveToState::OnExit( CCSBot *me )
{
	// reset to run in case we were walking near our goal position
	me->Run();
	me->SetDisposition( CCSBot::ENGAGE_AND_INVESTIGATE );
}

const char *MoveToState::GetName() const
{
	return "MoveTo";
}
