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
#include "hostage.h"

//--------------------------------------------------------------------------------------------------------------
/**
 * Begin moving to a nearby hidey-hole.
 * NOTE: Do not forget this state may include a very long "move-to" time to get to our hidey spot!
 */
void HideState::OnEnter( CCSBot *me )
{
	m_isAtSpot = false;

	// if duration is "infinite", set it to a reasonably long time to prevent infinite camping
	if ( m_duration < 0.0f )
	{
		m_duration = RANDOM_FLOAT( 30.0f, 60.0f );
	}

	// decide whether to "ambush" or not - never set to false so as not to override external setting
	if ( RANDOM_FLOAT( 0.0f, 100.0f ) < 50.0f )
	{
		m_isHoldingPosition = true;
	}

	// if we are holding position, decide for how long
	if ( m_isHoldingPosition )
	{
		m_holdPositionTime = RANDOM_FLOAT( 3.0f, 10.0f );
	}
	else
	{
		m_holdPositionTime = 0.0f;
	}

	m_heardEnemy = false;
	m_firstHeardEnemyTime = 0.0f;
	m_retry = 0;

	if ( me->m_isFollowing )
	{
		m_leaderAnchorPos = ((CBaseEntity *)me->m_leader)->pev->origin;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Move to a nearby hidey-hole.
 * NOTE: Do not forget this state may include a very long "move-to" time to get to our hidey spot!
 */
void HideState::OnUpdate( CCSBot *me )
{
	CCSBotManager *manager = TheCSBots();

	// wait until finished reloading to leave hide state
	CBasePlayerWeapon *weapon = (CBasePlayerWeapon *)me->m_pActiveItem;
	bool reloading = weapon && (weapon->m_fInReload || weapon->m_fInSpecialReload);
	if ( !reloading )
	{
		// check if duration has expired
		if ( gpGlobals->time - me->m_stateTimestamp > m_duration )
		{
			if ( me->m_task == CCSBot::GUARD_LOOSE_BOMB )
			{
				// if we're guarding the loose bomb, continue to guard it but pick a new spot
				me->Hide( manager->GetLooseBombArea(), -1.0f, 750.0f, false );
				return;
			}
			else if ( me->m_task == CCSBot::GUARD_BOMB_ZONE )
			{
				// if we're guarding a bombsite, continue to guard it but pick a new spot
				const CCSBotManager::Zone *zone = manager->GetClosestZone( &me->pev->origin );
				if ( zone )
				{
					CNavArea *area = manager->GetRandomAreaInZone( zone );
					if ( area )
					{
						me->Hide( area, -1.0f, 750.0f, false );
						return;
					}
				}
			}
			else if ( me->m_task == CCSBot::GUARD_HOSTAGE_RESCUE_ZONE )
			{
				// if we're guarding a rescue zone, continue to guard this or another rescue zone
				if ( me->GuardRandomZone( 500.0f ) )
				{
					me->SetTask( CCSBot::GUARD_HOSTAGE_RESCUE_ZONE );
					me->PrintIfWatched( "Continuing to guard hostage rescue zones\n" );
					me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
					me->GetChatter()->GuardingHostageEscapeZone( true );
					return;
				}
			}

			me->Idle();
			return;
		}

		// if we are momentarily hiding while following someone, check to see if he has moved on
		if ( me->m_isFollowing )
		{
			CBasePlayer *leader = (CBasePlayer *)(CBaseEntity *)me->m_leader;

			float runThreshold = 200.0f;
			if ( leader->pev->velocity.IsLengthGreaterThan( runThreshold ) )
			{
				// leader is running, stay with him
				me->Follow( leader );
				return;
			}

			// if leader has moved, stay with him
			const float followRange = 250.0f;
			if ( (m_leaderAnchorPos - leader->pev->origin).IsLengthGreaterThan( followRange ) )
			{
				me->Follow( leader );
				return;
			}
		}

		//
		// Scenario logic
		//
		if ( manager->GetScenario() == CCSBotManager::SCENARIO_DEFUSE_BOMB )
		{
			if ( me->m_iTeam == CT )
			{
				// if we are just holding position (due to a radio order) and the bomb has just planted, go defuse it
				if ( me->m_task == CCSBot::HOLD_POSITION &&
					manager->IsBombPlanted() &&
					manager->m_bombPlantTimestamp > me->m_stateTimestamp )
				{
					me->Idle();
					return;
				}

				// if we are guarding the defuser and he dies/gives up, stop hiding (to choose another defuser)
				if ( me->m_task == CCSBot::GUARD_BOMB_DEFUSER && !manager->GetBombDefuser() )
				{
					me->Idle();
					return;
				}

				// if we are guarding the loose bomb and it is picked up, stop hiding
				if ( me->m_task == CCSBot::GUARD_LOOSE_BOMB && !manager->GetLooseBomb() )
				{
					me->GetChatter()->TheyPickedUpTheBomb();
					me->Idle();
					return;
				}

				// if we are guarding a bombsite and the bomb is dropped and we hear about it, stop guarding
				if ( me->m_task == CCSBot::GUARD_BOMB_ZONE && me->GetGameState()->IsLooseBombLocationKnown() )
				{
					me->Idle();
					return;
				}

				if ( me->IsDoingScenario() && me->m_task == CCSBot::GUARD_BOMB_ZONE && manager->IsBombPlanted() )
				{
					me->Idle();
					return;
				}
			}
			else	// TERRORIST
			{
				// if we are near the ticking bomb and someone starts defusing it, attack!
				if ( manager->GetBombDefuser() )
				{
					Vector toDefuser = manager->GetBombDefuser()->pev->origin - me->pev->origin;

					const float hearDefuseRange = 2000.0f;
					if ( toDefuser.IsLengthLessThan( hearDefuseRange ) )
					{
						// if we are nearby, attack, otherwise move to the bomb (which will cause us to attack when we see defuser)
						if ( me->CanSeePlantedBomb() )
						{
							me->Attack( manager->GetBombDefuser() );
						}
						else
						{
							me->MoveTo( &manager->GetBombDefuser()->pev->origin, FASTEST_ROUTE );
							me->InhibitLookAround( 10.0f );
						}

						return;
					}
				}
			}
		}
		else if ( manager->GetScenario() == CCSBotManager::SCENARIO_RESCUE_HOSTAGES )
		{
			// if we're guarding the hostages and they all die or are taken, do something else
			if ( me->m_task == CCSBot::GUARD_HOSTAGES )
			{
				if ( me->GetGameState()->AreAllHostagesBeingRescued() || me->GetGameState()->AreAllHostagesGone() )
				{
					me->Idle();
					return;
				}
			}
			else if ( me->m_task == CCSBot::GUARD_HOSTAGE_RESCUE_ZONE )
			{
				// if we stumble across a hostage, guard it
				CHostage *hostage = me->GetGameState()->GetNearestVisibleFreeHostage();
				if ( hostage )
				{
					// we see a free hostage, guard it
					CNavArea *area = TheNavAreaGrid.GetNearestNavArea( &hostage->pev->origin, false );
					if ( area )
					{
						me->SetTask( CCSBot::GUARD_HOSTAGES );
						me->Hide( area, -1.0f, 750.0f, false );
						me->PrintIfWatched( "I'm guarding hostages I found\n" );
						// don't chatter here - he'll tell us when he's in his hiding spot
						return;
					}
				}
			}
		}

		bool isSettledInSniper = (me->IsSniper() && m_isAtSpot) ? true : false;

		// only investigate noises if we are initiating attacks, and we aren't a "settled in" sniper
		// dont investigate noises if we are reloading
		weapon = (CBasePlayerWeapon *)me->m_pActiveItem;
		if ( !(weapon && (weapon->m_fInReload || weapon->m_fInSpecialReload)) &&
			!isSettledInSniper &&
			me->GetDisposition() == CCSBot::ENGAGE_AND_INVESTIGATE )
		{
			// if we are holding position, and have heard the enemy nearby, investigate after our hold time is up
			if ( m_isHoldingPosition && m_heardEnemy && (gpGlobals->time - m_firstHeardEnemyTime > m_holdPositionTime) )
			{
				/// @todo We might need to remember specific location of last enemy noise here
				me->InvestigateNoise();
				return;
			}

			// investigate nearby enemy noises
			if ( me->ShouldInvestigateNoise( NULL ) )
			{
				// if we are holding position, check if enough time has elapsed since we first heard the enemy
				if ( m_isAtSpot && m_isHoldingPosition )
				{
					if ( !m_heardEnemy )
					{
						// first time we heard the enemy
						m_heardEnemy = true;
						m_firstHeardEnemyTime = gpGlobals->time;
						me->PrintIfWatched( "Heard enemy, holding position for %f2.1 seconds...\n", m_holdPositionTime );
					}
				}
				else
				{
					// not holding position - investigate enemy noise
					me->InvestigateNoise();
					return;
				}
			}
		}
	}	// end reloading check

	// look around
	me->UpdateLookAround( false );

	// if we are at our hiding spot, crouch and wait
	if ( m_isAtSpot )
	{
		me->Crouch();

		// if we have a shield, hide behind it
		if ( me->HasShield() && (!me->HasShield() || !me->m_bShieldDrawn) )
			me->SecondaryAttack();

		// while sitting at our hiding spot, if we are being attacked but can't see our attacker, move somewhere else
		const float hurtRecentlyTime = 1.0f;
		if ( !me->m_isEnemyVisible && gpGlobals->time - me->m_attackedTimestamp < hurtRecentlyTime )
		{
			me->Idle();
			return;
		}

		// encourage the human player
		if ( !me->IsDoingScenario() )
		{
			if ( me->m_iTeam == CT )
			{
				if ( me->m_task == CCSBot::GUARD_BOMB_ZONE &&
					me->IsAtHidingSpot() &&
					manager->IsBombPlanted() )
				{
					if ( me->GetNearbyEnemyCount() == 0 )
					{
						const float someTime = 30.0f;
						const float littleTime = 11.0;

						if ( manager->GetBombTimeLeft() > someTime )
							me->GetChatter()->Encourage( "BombsiteSecure", RANDOM_FLOAT( 10.0f, 15.0f ), 3.0f );
						else if ( manager->GetBombTimeLeft() > littleTime )
							me->GetChatter()->Encourage( "WaitingForHumanToDefuseBomb", RANDOM_FLOAT( 5.0f, 8.0f ), 3.0f );
						else
							me->GetChatter()->Encourage( "WaitingForHumanToDefuseBombPanic", RANDOM_FLOAT( 3.0f, 4.0f ), 3.0f );
					}
				}

				if ( me->m_task == CCSBot::GUARD_HOSTAGES && me->IsAtHidingSpot() )
				{
					if ( me->GetNearbyEnemyCount() == 0 )
					{
						CHostage *hostage = me->GetGameState()->GetNearestVisibleFreeHostage();
						if ( hostage )
						{
							me->GetChatter()->Encourage( "WaitingForHumanToRescueHostages", RANDOM_FLOAT( 10.0f, 15.0f ), 3.0f );
						}
					}
				}
			}
		}
	}
	else
	{
		// we are moving to our hiding spot

		// if a Player is using this hiding spot, give up
		float range;
		CBasePlayer *camper = UTIL_GetClosestPlayer( &m_hidingSpot, &range );

		const float closeRange = 75.0f;
		if ( camper && camper != me && range < closeRange && me->IsVisible( camper, CHECK_FOV ) )
		{
			// player is in our hiding spot
			me->PrintIfWatched( "Someone's in my hiding spot - picking another...\n" );

			const int maxRetries = 3;
			if ( m_retry++ >= maxRetries )
			{
				me->PrintIfWatched( "Can't find a free hiding spot, giving up.\n" );
				me->Idle();
				return;
			}

			// pick another hiding spot near where we were planning on hiding
			me->Hide( TheNavAreaGrid.GetNavArea( &m_hidingSpot, 120.0f ), -1.0f, 750.0f, false );

			return;
		}

		Vector feet = me->pev->origin;
		feet.z -= me->IsCrouching() ? 18.0f : 36.0f;
		range = (m_hidingSpot - feet).Length();
		if ( range < 200.0f )
			me->Crouch();

		const float atDist = 20.0f;
		if ( range < atDist )
		{
			//-------------------------------------
			// Just reached our hiding spot
			//
			m_isAtSpot = true;

			// make sure our approach points are valid, since we'll be watching them
			me->ComputeApproachPoints();

			// ready our weapon and prepare to attack
			me->EquipBestWeapon( true );
			me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );

			// if we are a sniper, update our task
			if ( me->m_task == CCSBot::MOVE_TO_SNIPER_SPOT )
			{
				me->SetTask( CCSBot::SNIPING );
			}

			// determine which way to look
			TraceResult result;
			float outAngle = 0.0f;
			float outAngleRange = 0.0f;
			for ( float angle = 0.0f; angle < 360.0f; angle += 45.0f )
			{
				float sine = BotSIN( angle );
				float cosine = BotCOS( angle );
				me->m_eyePosition = me->pev->origin + me->pev->view_ofs;
				Vector end = me->m_eyePosition + Vector( cosine * 1000.0f, sine * 1000.0f, 0.0f );
				me->m_eyePosition = me->pev->origin + me->pev->view_ofs;
				UTIL_TraceLine( me->m_eyePosition, end, ignore_monsters, ignore_glass, me->edict(), &result );

				if ( result.flFraction > outAngleRange )
				{
					outAngle = angle;
					outAngleRange = result.flFraction;
				}
			}

			me->m_lookAheadAngle = outAngle;
		}

		// move to hiding spot
		if ( me->UpdatePathMovement() != CCSBot::PROGRESSING && !m_isAtSpot )
		{
			// we couldn't get to our hiding spot - pick another
			me->PrintIfWatched( "Can't get to my hiding spot - finding another...\n" );

			// search from hiding spot, since we know it was valid
			const Vector *pos = FindNearbyHidingSpot( me, &m_hidingSpot, m_searchFromArea, m_range, me->IsSniper(), false );
			if ( pos == NULL )
			{
				// no available hiding spots
				me->PrintIfWatched( "No available hiding spots - hiding where I'm at.\n" );

				// hide where we are
				m_hidingSpot.x = me->pev->origin.x;
				m_hidingSpot.x = me->pev->origin.y;
				m_hidingSpot.z = me->pev->origin.z - (me->IsCrouching() ? 18.0f : 36.0f);
			}
			else
			{
				m_hidingSpot = *pos;
			}

			// build a path to our new hiding spot
			if ( me->ComputePath( TheNavAreaGrid.GetNavArea( &m_hidingSpot, 120.0f ), &m_hidingSpot, FASTEST_ROUTE ) == false )
			{
				me->PrintIfWatched( "Can't pathfind to hiding spot\n" );
				me->Idle();
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
void HideState::OnExit( CCSBot *me )
{
	m_isHoldingPosition = false;

	me->StandUp();
	me->ResetStuckMonitor();
	me->m_lookAtSpotState = 0;
	me->m_lookAtDesc = NULL;
	me->m_approachPointCount = 0;

	// if we have a shield, put it away
	if ( me->HasShield() && me->HasShield() && me->m_bShieldDrawn )
		me->SecondaryAttack();
}

const char *HideState::GetName() const
{
	return "Hide";
}
