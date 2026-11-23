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

// range for snipers to select a hiding spot
const float sniperHideRange = 2000.0f;

//--------------------------------------------------------------------------------------------------------------
/**
 * The Idle state.
 * We never stay in the Idle state - it is a "home base" for the state machine that
 * does various checks to determine what we should do next.
 */
void IdleState::OnEnter( CCSBot *me )
{
	me->DestroyPath();
	me->SetEnemy( NULL );

	// lurking death
	if ( me->IsUsingKnife() && me->IsWellPastSafe() && !me->IsHurrying() )
		me->Walk();

	//
	// Since Idle assigns tasks, we assume that coming back to Idle means our task is complete
	//
	me->SetTask( CCSBot::SEEK_AND_DESTROY );
	me->SetDisposition( CCSBot::ENGAGE_AND_INVESTIGATE );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Determine what we should do next
 */
void IdleState::OnUpdate( CCSBot *me )
{
	// all other states assume GetLastKnownArea() is valid, ensure that it is
	if ( me->GetLastKnownArea() == NULL && me->StayOnNavMesh() == false )
		return;

	// zombies never leave the Idle state
	if ( cv_bot_zombie.value > 0.0f )
	{
		me->ResetStuckMonitor();
		return;
	}

	CCSBotManager *manager = TheCSBots();

	// if we are in the early "safe" time, grab a knife or grenade
	if ( me->IsSafe() )
	{
		// if we have a grenade, use it
		if ( !me->EquipGrenade( false ) )
		{
			if ( me->GetProfile()->GetSkill() > 0.33f )
			{
				me->EquipKnife();
			}
		}
	}

	// if round is over, hunt
	if ( me->GetGameState()->IsRoundOver() )
	{
		// if we are escorting hostages, try to get to the rescue zone
		if ( me->GetHostageEscortCount() )
		{
			const CCSBotManager::Zone *zone = manager->GetClosestZone( me->GetLastKnownArea(), PathCost( me, FASTEST_ROUTE ) );

			me->SetTask( CCSBot::RESCUE_HOSTAGES );
			me->Run();
			me->SetDisposition( CCSBot::SELF_DEFENSE );
			me->MoveTo( manager->GetRandomPositionInZone( zone ), FASTEST_ROUTE );
			me->PrintIfWatched( "Trying to rescue hostages at the end of the round\n" );
			return;
		}

		me->Hunt();
		return;
	}

	const float defenseSniperCampChance = 75.0f;
	const float offenseSniperCampChance = 10.0f;

	// if we were following someone, continue following them
	if ( me->m_isFollowing )
	{
		me->ContinueFollowing();
		return;
	}

	//
	// Scenario logic
	//
	switch ( manager->GetScenario() )
	{
		//======================================================================================================
		case CCSBotManager::SCENARIO_DEFUSE_BOMB:
		{
			// if this is a bomb game and we have the bomb, go plant it
			if ( me->m_iTeam == TERRORIST )
			{
				if ( me->GetGameState()->GetBombState() == CSGameState::PLANTED )
				{
					if ( me->GetGameState()->GetPlantedBombsite() != CSGameState::UNKNOWN )
					{
						// T's always know where the bomb is - go defend it
						const CCSBotManager::Zone *zone = manager->GetZone( me->GetGameState()->GetPlantedBombsite() );
						if ( zone )
						{
							me->SetTask( CCSBot::GUARD_TICKING_BOMB );

							Place place = TheNavAreaGrid.GetPlace( &zone->m_center );
							if ( place != UNDEFINED_PLACE )
							{
								// pick a random hiding spot in this place
								const Vector *spot = FindRandomHidingSpot( me, place, me->IsSniper() );
								if ( spot )
								{
									me->Hide( spot, -1, false );
									return;
								}
							}

							// hide nearby
							me->Hide( TheNavAreaGrid.GetNearestNavArea( &zone->m_center ), -1, 750, false );
							return;
						}
					}
					else
					{
						// ask our teammates where the bomb is
						me->GetChatter()->RequestBombLocation();

						// we dont know where the bomb is - we must search the bombsites
						int zoneIndex = me->GetGameState()->GetNextBombsiteToSearch();

						// move to bombsite - if we reach it, we'll update its cleared status, causing us to select another
						const Vector *pos = manager->GetRandomPositionInZone( manager->GetZone( zoneIndex ) );
						if ( pos )
						{
							me->SetTask( CCSBot::FIND_TICKING_BOMB );
							me->MoveTo( pos, SAFEST_ROUTE );
							return;
						}
					}
				}
				else if ( me->m_bHasC4 )
				{
					// if we're at a bomb site, plant the bomb
					if ( me->m_signals.GetState() & SIGNAL_BOMB )
					{
						// plant it
						me->SetTask( CCSBot::PLANT_BOMB );
						me->PlantBomb();

						// radio to the team
						me->GetChatter()->PlantingTheBomb( me->GetPlace() );

						return;
					}
					else if ( gpGlobals->time >= manager->m_earliestBombPlantTimestamp )
					{
						// move to the closest bomb site
						const CCSBotManager::Zone *zone = manager->GetClosestZone( me->GetLastKnownArea(), PathCost( me ) );

						if ( zone )
						{
							// pick a random spot within the bomb zone
							const Vector *pos = manager->GetRandomPositionInZone( zone );
							if ( pos )
							{
								// move to bombsite
								me->SetTask( CCSBot::PLANT_BOMB );
								me->Run();
								me->MoveTo( pos, SAFEST_ROUTE );

								return;
							}
						}
					}
				}
				else
				{
					// small chance of sniper camping on offense, if we aren't carrying the bomb
					if ( me->GetFriendsRemaining() && me->IsSniper() && RANDOM_FLOAT( 0, 100 ) < offenseSniperCampChance )
					{
						me->SetTask( CCSBot::MOVE_TO_SNIPER_SPOT );
						me->Hide( me->GetLastKnownArea(), RANDOM_FLOAT( 10, 30 ), sniperHideRange, false );
						me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
						me->PrintIfWatched( "Sniping!\n" );
						return;
					}

					// if the bomb is loose (on the ground), go get it
					if ( me->NoticeLooseBomb() )
					{
						me->FetchBomb();
						return;
					}

					// if bomb has been planted, and we hear it, move to a hiding spot near the bomb and guard it
					if ( !me->IsRogue() && me->GetGameState()->GetBombState() == CSGameState::PLANTED && me->GetGameState()->GetBombPosition() )
					{
						me->SetTask( CCSBot::GUARD_TICKING_BOMB );
						me->Hide( TheNavAreaGrid.GetNavArea( me->GetGameState()->GetBombPosition() ), -1, 750, false );
						return;
					}
				}
			}
			else	// CT ------------------------------------------------------------------------------------------
			{
				if ( me->GetGameState()->GetBombState() == CSGameState::PLANTED )
				{
					// if the bomb has been planted, attempt to defuse it
					const Vector *bombPos = me->GetGameState()->GetBombPosition();
					if ( bombPos )
					{
						// if someone is defusing the bomb, guard them
						if ( manager->GetBombDefuser() )
						{
							if ( !me->IsRogue() )
							{
								me->SetTask( CCSBot::GUARD_BOMB_DEFUSER );
								me->Hide( TheNavAreaGrid.GetNavArea( bombPos ), -1, 750, false );
								return;
							}
						}
						else if ( me->IsDoingScenario() )
						{
							// move to the bomb and defuse it
							me->SetTask( CCSBot::DEFUSE_BOMB );
							me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
							me->MoveTo( bombPos, SAFEST_ROUTE );
							return;
						}
						else
						{
							// we're not allowed to defuse, guard the bomb zone
							me->SetTask( CCSBot::GUARD_BOMB_ZONE );
							me->Hide( TheNavAreaGrid.GetNavArea( bombPos ), -1, 750, false );
							me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
							return;
						}
					}
					else if ( me->GetGameState()->GetPlantedBombsite() != CSGameState::UNKNOWN )
					{
						// we know which bombsite, but not exactly where the bomb is, go there
						const CCSBotManager::Zone *zone = manager->GetZone( me->GetGameState()->GetPlantedBombsite() );
						if ( zone )
						{
							if ( me->IsDoingScenario() )
							{
								me->SetTask( CCSBot::DEFUSE_BOMB );
								me->MoveTo( &zone->m_center, SAFEST_ROUTE );
								me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
								return;
							}
							else
							{
								// we're not allowed to defuse, guard the bomb zone
								me->SetTask( CCSBot::GUARD_BOMB_ZONE );
								me->Hide( TheNavAreaGrid.GetNavArea( &zone->m_center ), -1, 750, false );
								me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
								return;
							}
						}
					}
					else
					{
						// we dont know where the bomb is - we must search the bombsites

						// find closest un-cleared bombsite
						const CCSBotManager::Zone *zone = NULL;
						float travelDistance = 10000000;

						for ( int z = 0; z < manager->GetZoneCount(); ++z )
						{
							if ( manager->GetZone( z )->m_areaCount == 0 )
								continue;

							// don't check bombsites that have been cleared
							if ( me->GetGameState()->IsBombsiteClear( z ) )
								continue;

							// just use the first overlapping nav area as a reasonable approximation
							ShortestPathCost cost = ShortestPathCost();
							float dist = NavAreaTravelDistance( me->GetLastKnownArea(),
																TheNavAreaGrid.GetNearestNavArea( &manager->GetZone( z )->m_center ),
																cost );

							if ( dist < travelDistance )
							{
								zone = manager->GetZone( z );
								travelDistance = dist;
							}
						}

						if ( zone )
						{
							const float farAwayRange = 2000.0f;
							if ( travelDistance > farAwayRange )
							{
								zone = NULL;
							}
						}

						// if closest bombsite is "far away", pick one at random
						if ( zone == NULL )
						{
							int zoneIndex = me->GetGameState()->GetNextBombsiteToSearch();
							zone = manager->GetZone( zoneIndex );
						}

						// move to bombsite - if we reach it, we'll update its cleared status, causing us to select another
						if ( zone )
						{
							const Vector *pos = manager->GetRandomPositionInZone( zone );
							if ( pos )
							{
								me->SetTask( CCSBot::FIND_TICKING_BOMB );
								me->MoveTo( pos, SAFEST_ROUTE );
								return;
							}
						}
					}
				}

				// if we have a sniper rifle, we like to camp, whether rogue or not
				if ( me->IsSniper() )
				{
					if ( RANDOM_FLOAT( 0, 100 ) <= defenseSniperCampChance )
					{
						CNavArea *snipingArea = NULL;

						// if the bomb is loose, snipe near it
						if ( me->GetGameState()->IsLooseBombLocationKnown() )
						{
							snipingArea = TheNavAreaGrid.GetNearestNavArea( me->GetGameState()->GetBombPosition() );
							me->PrintIfWatched( "Sniping near loose bomb\n" );
						}
						else
						{
							// snipe bomb zone(s)
							const CCSBotManager::Zone *zone = manager->GetRandomZone();
							if ( zone )
							{
								snipingArea = manager->GetRandomAreaInZone( zone );
								me->PrintIfWatched( "Sniping near bombsite\n" );
							}
						}

						if ( snipingArea )
						{
							me->SetTask( CCSBot::MOVE_TO_SNIPER_SPOT );
							me->Hide( snipingArea, -1, sniperHideRange, false );
							me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
							return;
						}
					}
				}

				// rogues just hunt, unless they want to snipe
				// if the whole team has decided to rush, hunt
				// if we know the bomb is dropped, hunt for enemies and the loose bomb
				if ( me->IsRogue() || manager->IsDefenseRushing() || me->GetGameState()->IsLooseBombLocationKnown() )
				{
					me->Hunt();
					return;
				}

				// the lower our morale gets, the more we want to camp the bomb zone(s)
				// only decide to camp at the start of the round, or if we haven't seen anything for a long time
				if ( me->IsSafe() || me->HasNotSeenEnemyForLongTime() )
				{
					float guardBombsiteChance = me->m_morale * -34.0f;

					if ( RANDOM_FLOAT( 0, 100 ) < guardBombsiteChance )
					{
						float guardRange = 500.0f + 100.0f * (me->m_morale + 3);

						// guard bomb zone(s)
						const CCSBotManager::Zone *zone = manager->GetRandomZone();
						if ( zone )
						{
							CNavArea *area = manager->GetRandomAreaInZone( zone );
							if ( area )
							{
								me->PrintIfWatched( "I'm guarding a bombsite\n" );
								me->GetChatter()->AnnouncePlan( "GoingToDefendBombsite", area->GetPlace() );
								me->SetTask( CCSBot::GUARD_BOMB_ZONE );
								me->Hide( area, -1, guardRange, false );
								me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
								return;
							}
						}
					}
				}
			}

			break;
		}

		//======================================================================================================
		case CCSBotManager::SCENARIO_ESCORT_VIP:
		{
			if ( me->m_iTeam == TERRORIST )
			{
				// if we have a sniper rifle, we like to camp, whether rogue or not
				if ( me->IsSniper() )
				{
					if ( RANDOM_FLOAT( 0, 100 ) <= defenseSniperCampChance )
					{
						// snipe escape zone(s)
						const CCSBotManager::Zone *zone = manager->GetRandomZone();
						if ( zone )
						{
							CNavArea *area = manager->GetRandomAreaInZone( zone );
							if ( area )
							{
								me->SetTask( CCSBot::MOVE_TO_SNIPER_SPOT );
								me->Hide( area, -1, sniperHideRange, false );
								me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
								me->PrintIfWatched( "Sniping near escape zone\n" );
								return;
							}
						}
					}
				}

				// rogues just hunt, unless they want to snipe
				// if the whole team has decided to rush, hunt
				if ( me->IsRogue() || manager->IsDefenseRushing() )
					break;

				// the lower our morale gets, the more we want to camp the escape zone(s)
				float guardEscapeZoneChance = me->m_morale * -34.0f;

				if ( RANDOM_FLOAT( 0, 100 ) < guardEscapeZoneChance )
				{
					// guard escape zone(s)
					const CCSBotManager::Zone *zone = manager->GetRandomZone();
					if ( zone )
					{
						CNavArea *area = manager->GetRandomAreaInZone( zone );
						if ( area )
						{
							// guard the escape zone - stay closer if our morale is low
							me->SetTask( CCSBot::GUARD_VIP_ESCAPE_ZONE );
							me->PrintIfWatched( "I'm guarding an escape zone\n" );

							float escapeGuardRange = 750.0f + 250.0f * (me->m_morale + 3);
							me->Hide( area, -1, escapeGuardRange, false );

							me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
							return;
						}
					}
				}
			}
			else	// CT
			{
				if ( me->m_bIsVIP )
				{
					// if early in round, pick a random zone, otherwise pick closest zone
					const float earlyTime = 20.0f;
					const CCSBotManager::Zone *zone = NULL;

					if ( manager->GetElapsedRoundTime() < earlyTime )
					{
						// pick random zone
						zone = manager->GetRandomZone();
					}
					else
					{
						// pick closest zone
						zone = manager->GetClosestZone( me->GetLastKnownArea(), PathCost( me ) );
					}

					if ( zone )
					{
						// pick a random spot within the escape zone
						const Vector *pos = manager->GetRandomPositionInZone( zone );
						if ( pos )
						{
							// move to escape zone
							me->SetTask( CCSBot::VIP_ESCAPE );
							me->Run();
							me->MoveTo( pos, SAFEST_ROUTE );

							// tell team to follow
							const float repeatTime = 30.0f;
							if ( me->GetFriendsRemaining() &&
									manager->GetRadioMessageInterval( EVENT_RADIO_FOLLOW_ME, me->m_iTeam ) > repeatTime )
								me->SendRadioMessage( EVENT_RADIO_FOLLOW_ME );
							return;
						}
					}
				}
				else
				{
					// small chance of sniper camping on offense, if we aren't VIP
					if ( me->GetFriendsRemaining() && me->IsSniper() && RANDOM_FLOAT( 0, 100 ) < offenseSniperCampChance )
					{
						me->SetTask( CCSBot::MOVE_TO_SNIPER_SPOT );
						me->Hide( me->GetLastKnownArea(), RANDOM_FLOAT( 10, 30 ), sniperHideRange, false );
						me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
						me->PrintIfWatched( "Sniping!\n" );
						return;
					}
				}
			}
			break;
		}

		//======================================================================================================
		case CCSBotManager::SCENARIO_RESCUE_HOSTAGES:
		{
			if ( me->m_iTeam == TERRORIST )
			{
				bool campHostages;

				// if we are in early game, camp the hostages
				if ( me->IsSafe() )
				{
					campHostages = true;
				}
				else if ( me->GetGameState()->m_haveSomeHostagesBeenTaken || me->GetGameState()->AreAllHostagesBeingRescued() )
				{
					campHostages = false;
				}
				else
				{
					// later in the game, camp either hostages or escape zone
					const float campZoneChance = (manager->GetElapsedRoundTime() - me->m_safeTime) * 100 / 120;

					campHostages = (RANDOM_FLOAT( 0, 100 ) > campZoneChance) ? true : false;
				}

				// if we have a sniper rifle, we like to camp, whether rogue or not
				if ( me->IsSniper() )
				{
					if ( RANDOM_FLOAT( 0, 100 ) <= defenseSniperCampChance )
					{
						const Vector *hostagePos = me->GetGameState()->GetRandomFreeHostagePosition();
						if ( hostagePos && campHostages )
						{
							me->SetTask( CCSBot::MOVE_TO_SNIPER_SPOT );
							me->PrintIfWatched( "Sniping near hostages\n" );
							me->Hide( TheNavAreaGrid.GetNearestNavArea( hostagePos ), -1, sniperHideRange, false );
							me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
							return;
						}
						else
						{
							// camp the escape zone(s)
							if ( me->GuardRandomZone( sniperHideRange ) )
							{
								me->SetTask( CCSBot::MOVE_TO_SNIPER_SPOT );
								me->PrintIfWatched( "Sniping near a rescue zone\n" );
								me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
								return;
							}
						}
					}
				}

				// if safe time is up, and we stumble across a hostage, guard it
				if ( !me->IsSafe() && !me->IsRogue() )
				{
					CHostage *hostage = me->GetGameState()->GetNearestVisibleFreeHostage();
					if ( hostage )
					{
						// we see a free hostage, guard it
						CNavArea *area = TheNavAreaGrid.GetNearestNavArea( &hostage->pev->origin );
						if ( area )
						{
							me->SetTask( CCSBot::GUARD_HOSTAGES );
							me->Hide( area, -1, 750, false );
							me->PrintIfWatched( "I'm guarding hostages I found\n" );
							// don't chatter here - he'll tell us when he's in his hiding spot
							return;
						}
					}
				}

				// decide if we want to hunt, or guard
				const float huntChance = 70.0f + 25.0f * me->m_morale;

				// rogues just hunt, unless they want to snipe
				// if the whole team has decided to rush, hunt
				if ( me->GetFriendsRemaining() )
				{
					if ( me->IsRogue() || manager->IsDefenseRushing() || RANDOM_FLOAT( 0, 100 ) < huntChance )
					{
						me->Hunt();
						return;
					}
				}

				// decide whether to camp the hostages or the escape zones
				const Vector *hostagePos = me->GetGameState()->GetRandomFreeHostagePosition();
				if ( hostagePos && campHostages )
				{
					CNavArea *area = TheNavAreaGrid.GetNearestNavArea( hostagePos );
					if ( area )
					{
						// guard the hostages - stay closer to hostages if our morale is low
						me->SetTask( CCSBot::GUARD_HOSTAGES );
						me->PrintIfWatched( "I'm guarding hostages\n" );

						float hostageGuardRange = 750.0f + 250.0f * (me->m_morale + 3);			// 2000
						me->Hide( area, -1, hostageGuardRange, false );
						me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );

						if ( RANDOM_FLOAT( 0, 100 ) < 50 )
							me->GetChatter()->GuardingHostages( area->GetPlace(), true );

						return;
					}
				}

				// guard rescue zone(s)
				if ( me->GuardRandomZone( 500 ) )
				{
					me->SetTask( CCSBot::GUARD_HOSTAGE_RESCUE_ZONE );
					me->PrintIfWatched( "I'm guarding a rescue zone\n" );
					me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
					me->GetChatter()->GuardingHostageEscapeZone( true );
					return;
				}
			}
			else	// CT ---------------------------------------------------------------------------------
			{
				// only decide to do something else if we aren't already rescuing hostages
				if ( !me->GetHostageEscortCount() )
				{
					// small chance of sniper camping on offense
					if ( me->GetFriendsRemaining() && me->IsSniper() && RANDOM_FLOAT( 0, 100 ) < offenseSniperCampChance )
					{
						me->SetTask( CCSBot::MOVE_TO_SNIPER_SPOT );
						me->Hide( me->GetLastKnownArea(), RANDOM_FLOAT( 10, 30 ), sniperHideRange, false );
						me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
						me->PrintIfWatched( "Sniping!\n" );
						return;
					}

					if ( me->GetFriendsRemaining() && !me->GetHostageEscortCount() )
					{
						// rogues just hunt, unless all friends are dead
						// if we have friends left, we might go hunting instead of hostage rescuing
						const float huntChance = 33.3f;
						if ( me->IsRogue() || RANDOM_FLOAT( 0, 100 ) < huntChance )
						{
							me->Hunt();
							return;
						}
					}
				}

				// look for free hostages - CT's have radar so they know where hostages are at all times
				CHostage *hostage = me->GetGameState()->GetNearestFreeHostage( NULL );

				// if we are not allowed to do the scenario, guard the hostages to clear the area for the human(s)
				if ( !me->IsDoingScenario() )
				{
					if ( hostage )
					{
						CNavArea *area = TheNavAreaGrid.GetNearestNavArea( &hostage->pev->origin );
						if ( area )
						{
							me->SetTask( CCSBot::GUARD_HOSTAGES );
							me->Hide( area, -1, 750, false );
							me->PrintIfWatched( "I'm securing the hostages for a human to rescue\n" );
							return;
						}
					}

					me->Hunt();
					return;
				}

				bool fetchHostages = false;
				bool rescueHostages = false;
				const CCSBotManager::Zone *zone = NULL;
				me->m_goalEntity = NULL;

				// if we are escorting hostages, determine where to take them
				if ( me->GetHostageEscortCount() )
					zone = manager->GetClosestZone( me->GetLastKnownArea(), PathCost( me, FASTEST_ROUTE ) );

				// if we are escorting hostages and there are more hostages to rescue,
				// determine whether it's faster to rescue the ones we have, or go get the remaining ones
				if ( hostage )
				{
					if ( zone )
					{
						PathCost cost( me, FASTEST_ROUTE );
						float toZone = NavAreaTravelDistance( me->GetLastKnownArea(), zone->m_area[0], cost );
						float toHostage = NavAreaTravelDistance( me->GetLastKnownArea(), TheNavAreaGrid.GetNearestNavArea( &hostage->pev->origin ), cost );

						if ( toHostage < 0.0f )
						{
							rescueHostages = true;
						}
						else
						{
							if ( toZone < toHostage )
								rescueHostages = true;
							else
								fetchHostages = true;
						}
					}
					else
					{
						fetchHostages = true;
					}
				}
				else if ( zone )
				{
					rescueHostages = true;
				}

				if ( fetchHostages )
				{
					// go get hostages
					me->SetTask( CCSBot::COLLECT_HOSTAGES );
					me->Run();
					me->m_goalEntity = hostage;
					me->m_isWaitingForHostage = false;
					me->m_inhibitWaitingForHostageTimer.Invalidate();

					// if we already have some hostages, move to the others by the quickest route
					RouteType route = (me->GetHostageEscortCount()) ? FASTEST_ROUTE : SAFEST_ROUTE;
					me->MoveTo( &hostage->pev->origin, route );

					me->PrintIfWatched( "I'm collecting hostages\n" );
					return;
				}

				if ( rescueHostages )
				{
					me->SetTask( CCSBot::RESCUE_HOSTAGES );
					me->Run();
					me->SetDisposition( CCSBot::SELF_DEFENSE );
					me->MoveTo( manager->GetRandomPositionInZone( zone ), FASTEST_ROUTE );
					me->PrintIfWatched( "I'm rescuing hostages\n" );
					me->GetChatter()->EscortingHostages();
					return;
				}
			}
			break;
		}

		default:	// deathmatch
		{
			// sniping check
			if ( me->GetFriendsRemaining() && me->IsSniper() && RANDOM_FLOAT( 0, 100 ) < offenseSniperCampChance )
			{
				me->SetTask( CCSBot::MOVE_TO_SNIPER_SPOT );
				me->Hide( me->GetLastKnownArea(), RANDOM_FLOAT( 10, 30 ), sniperHideRange, false );
				me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
				me->PrintIfWatched( "Sniping!\n" );
				return;
			}
			break;
		}
	}

	// if we have nothing special to do, go hunting for enemies
	me->Hunt();
}

const char *IdleState::GetName() const
{
	return "Idle";
}
