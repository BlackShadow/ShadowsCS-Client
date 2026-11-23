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
#include "gamerules.h"
#pragma warning( pop )
#include "cs_bot_manager.h"
#include "client.h"
#include "debug.h"

//--------------------------------------------------------------------------------------------------------------
void CCSBot::Panic( CBasePlayer *enemy )
{
	if ( gpGlobals->time - m_lastPanicTime < 5 )
		return;
	float sine = BotSIN( pev->v_angle.y );
	float cosine = BotCOS( pev->v_angle.y );
	Vector position;
	if ( GetProfile()->GetSkill() >= 0.5f )
	{
		Vector direction = enemy->pev->origin - pev->origin;
		float length = direction.Length2D();
		if ( length <= 0 )
		{
			direction.x = 1;
			direction.y = 0;
		}
		else
		{
			direction.x /= length;
			direction.y /= length;
		}
		float ahead = cosine * direction.x + sine * direction.y;
		float offset = RANDOM_FLOAT( -75, 75 );
		if ( ahead > 0.7071f )
		{
			position.x = pev->origin.x + cosine * 100 - sine * offset;
			position.y = pev->origin.y + sine * 100 + cosine * offset;
		}
		else if ( ahead < -0.7071f )
		{
			position.x = pev->origin.x - cosine * 100 - sine * offset;
			position.y = pev->origin.y - sine * 100 + cosine * offset;
		}
		else if ( -sine * direction.x + cosine * direction.y <= 0 )
		{
			position.x = pev->origin.x + sine * 100 + cosine * offset;
			position.y = pev->origin.y - cosine * 100 + sine * offset;
		}
		else
		{
			position.x = pev->origin.x - sine * 100 + cosine * offset;
			position.y = pev->origin.y + cosine * 100 + sine * offset;
		}
	}
	else
	{
		float offset = RANDOM_FLOAT( -200, 200 ) * 2;
		position.x = pev->origin.x - cosine * 200 - sine * offset;
		position.y = pev->origin.y - sine * 200 + cosine * offset;
	}
	float height = pev->origin.z;
	position.z = RANDOM_FLOAT( -50, 50 ) + height;
	m_surpriseDelay = RANDOM_FLOAT( 0.1f, 0.2f );
	m_lastPanicTime = gpGlobals->time;
	SetLookAt( "Panic", &position, PRIORITY_HIGH, 0, false, 5 );
	PrintIfWatched( "Aaaah!\n" );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Invoked when injured by something
 * NOTE: We dont want to directly call Attack() here, or the bots will have super-human reaction times when injured
 */
int CCSBot::TakeDamage( entvars_t *inflictor, entvars_t *attacker, float damage, int damageType )
{
	CBaseEntity *entity = GetClassPtr( (CBaseEntity *)inflictor );

	// if we were attacked by a teammate, rebuke
	if ( entity->IsPlayer() )
	{
		CBasePlayer *player = (CBasePlayer *)entity;

		if ( player->m_iTeam == m_iTeam && !player->IsBot() )
			GetChatter()->FriendlyFire();
	}

	if ( entity->IsPlayer() && IsEnemy( entity ) )
	{
		// Track previous attacker so we don't try to panic multiple times for a shotgun blast
		CBasePlayer *lastAttacker = m_attacker;
		float lastAttackedTimestamp = m_attackedTimestamp;

		// keep track of our last attacker
		m_attacker = (CBasePlayer *)entity;
		m_attackedTimestamp = gpGlobals->time;

		// no longer safe
		AdjustSafeTime();

		if ( gpGlobals->time - m_lastPanicTime >= 5 && (m_attackedTimestamp != lastAttackedTimestamp || m_attacker != lastAttacker) &&
			!IsVisible( (CBasePlayer *)entity, true, NULL ) )
		{
			float surpriseChance = 1 - GetProfile()->GetSkill();
			surpriseChance *= 50 * surpriseChance;
			if ( !IsAttacking() || !m_isEnemyVisible || surpriseChance > RANDOM_FLOAT( 0, 100 ) )
				Panic( m_attacker );
		}
	}

	// extend
	return CBasePlayer::TakeDamage( inflictor, attacker, damage, damageType );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Invoked when killed
 */
void CCSBot::Killed( entvars_t *attacker, int gib )
{
	PrintIfWatched( "Killed( attacker = %s )\n", STRING( attacker->netname ) );

	GetChatter()->OnDeath();

	// increase the danger where we died
	const float deathDanger = 1.0f;
	const float deathDangerRadius = 500.0f;
	IncreaseDangerNearby( m_iTeam - 1, deathDanger, m_lastKnownArea, &pev->origin, deathDangerRadius );

	// end voice feedback
	EndVoiceFeedback( true );

	// extend
	CBasePlayer::Killed( attacker, gib );
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::BotTouch( CBaseEntity *entity )
{
	// if we have touched a higher-priority player, make way
	/// @todo Need to account for reaction time, etc.
	if ( entity->IsPlayer() )
	{
		// if we are defusing a bomb, don't move
		if ( IsDefusingBomb() )
			return;

		// get priority of other player
		unsigned int otherPri = TheCSBots()->GetPlayerPriority( (CBasePlayer *)entity );

		// get our priority
		unsigned int myPri = TheCSBots()->GetPlayerPriority( this );

		// if our priority is better, don't budge
		if ( myPri < otherPri )
			return;

		// they are higher priority - make way, unless we're already making way for someone more important
		if ( m_avoid != NULL )
		{
			unsigned int avoidPri = TheCSBots()->GetPlayerPriority( (CBasePlayer *)(CBaseEntity *)m_avoid );
			if ( avoidPri < otherPri )
			{
				// ignore 'other' because we're already avoiding someone better
				return;
			}
		}

		m_avoid = entity;
		m_avoidTimestamp = gpGlobals->time;
		return;
	}
	if ( entity->pev->takedamage != DAMAGE_YES || IsAttacking() || !FClassnameIs( entity->pev, "func_breakable" ) )
		return;
	Vector center = (entity->pev->absmin + entity->pev->absmax) * 0.5f;
	if ( m_pathLength )
	{
		const Vector &lo = entity->pev->absmin;
		const Vector &hi = entity->pev->absmax;
		unsigned int startSide = 0;
		unsigned int endSide = 0;
		if ( pev->origin.x > hi.x )
			startSide |= 1;
		if ( pev->origin.x < lo.x )
			startSide |= 2;
		if ( pev->origin.y > hi.y )
			startSide |= 4;
		if ( pev->origin.y < lo.y )
			startSide |= 8;
		if ( pev->origin.z > hi.z )
			startSide |= 16;
		if ( pev->origin.z < lo.z )
			startSide |= 32;
		if ( m_goalPosition.x > hi.x )
			endSide |= 1;
		if ( m_goalPosition.x < lo.x )
			endSide |= 2;
		if ( m_goalPosition.y > hi.y )
			endSide |= 4;
		if ( m_goalPosition.y < lo.y )
			endSide |= 8;
		if ( m_goalPosition.z + 36 > hi.z )
			endSide |= 16;
		if ( m_goalPosition.z + 36 < lo.z )
			endSide |= 32;
		if ( startSide & endSide )
			return;
	}
	SetLookAt( "Breakable", &center, PRIORITY_HIGH, 0.2f, false, 5 );
	if ( IsUsingGrenade() )
		EquipBestWeapon( false );
	else
		PrimaryAttack();
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnEvent( GameEventType event, CBaseEntity *entity, CBaseEntity *other )
{
	m_gameState.OnEvent( event, entity, other );
	m_chatter.OnEvent( event, entity, other );

	// Morale adjustments happen even for dead players
	if ( event == EVENT_TERRORISTS_WIN )
	{
		if ( m_iTeam == CT )
			DecreaseMorale();
		else
			IncreaseMorale();
	}
	else if ( event == EVENT_CTS_WIN )
	{
		if ( m_iTeam == CT )
			IncreaseMorale();
		else
			DecreaseMorale();
	}

	if ( !IsAlive() )
		return;

	CCSBotManager *manager = TheCSBots();
	CBasePlayer *player = (entity && entity->IsPlayer()) ? (CBasePlayer *)entity : NULL;
	if ( event == EVENT_PLAYER_DIED )
	{
		//
		// If we just saw a nearby friend die, and we haven't yet acquired an enemy
		// automatically acquire our dead friend's killer
		//
		if ( !IsAttacking() && (GetDisposition() == ENGAGE_AND_INVESTIGATE || GetDisposition() == OPPORTUNITY_FIRE) &&
			player->m_iTeam == m_iTeam && other && other->IsPlayer() && ((CBasePlayer *)other)->m_iTeam != m_iTeam &&
			IsVisible( &player->pev->origin, false ) )
		{
			// people are dying - we should hurry
			m_hurryTimer.Start( RANDOM_FLOAT( 10, 15 ) );

			// if we're hiding with only our knife, be a little more cautious
			const float knifeAmbushChance = 50.0f;
			if ( !IsHiding() || !IsUsingKnife() || RANDOM_FLOAT( 0, 100 ) < knifeAmbushChance )
			{
				PrintIfWatched( "Attacking our friend's killer!\n" );
				Attack( (CBasePlayer *)other );
				return;
			}
		}

		CBasePlayer *killer = (other && other->IsPlayer()) ? (CBasePlayer *)other : NULL;

		// if the human player died in the single player game, tell the team
		if ( ((CHalfLifeMultiplay *)g_pGameRules)->IsCareer() && !player->IsBot() && player->m_iTeam == m_iTeam )
		{
			GetChatter()->Say( "CommanderDown", 20.0f );
		}

		// keep track of the last player we killed
		if ( killer == this )
			m_lastVictimID = player->entindex();

		// react to teammate death
		if ( player->m_iTeam == m_iTeam )
		{
			// chastise friendly fire from humans
			if ( killer && !killer->IsBot() && killer->m_iTeam == m_iTeam && killer != this )
				GetChatter()->KilledFriend();

			if ( IsHunting() )
			{
				PrintIfWatched( "Rethinking hunt due to teammate death\n" );
				Idle();
			}
			else if ( IsAttacking() && gpGlobals->time - m_lastSawEnemyTimestamp > 0.4f )
			{
				PrintIfWatched( "Rethinking my attack due to teammate death\n" );

				// allow us to sneak past windows, doors, etc
				IgnoreEnemies( 1 );

				// move to last known position of enemy - this could cause us to flank if 
				// the danger has changed due to our teammate's recent death
				SetTask( MOVE_TO_LAST_KNOWN_ENEMY_POSITION, (CBaseEntity *)m_enemy );
				MoveTo( &m_lastEnemyPosition, SAFEST_ROUTE );
			}
		}
		else if ( killer && killer->m_iTeam == m_iTeam )
		{
			if ( GetNearbyEnemyCount() <= 1 )
			{
				// report if number of enemies left is few and we killed the last one we saw locally
				GetChatter()->EnemiesRemaining();

				bool visible = IsVisible( &player->pev->origin, true );

				// congratulate teammates on their kills
				if ( killer != this && visible )
				{
					float delay = RANDOM_FLOAT( 2, 3 );
					if ( killer->IsBot() )
					{
						if ( RANDOM_FLOAT( 0, 100 ) < 40 )
							GetChatter()->Say( "NiceShot", 3, delay );
					}
					else
					{
						// humans get the honorific
						if ( ((CHalfLifeMultiplay *)g_pGameRules)->IsCareer() )
							GetChatter()->Say( "NiceShotCommander", 3, delay );
						else
							GetChatter()->Say( "NiceShotSir", 3, delay );
					}
				}
			}
		}
		return;
	}

	switch ( event )
	{
	case EVENT_WEAPON_FIRED:
	case EVENT_WEAPON_FIRED_ON_EMPTY:
	case EVENT_WEAPON_RELOADED:
		// for knife fighting - if our victim is attacking or reloading, rush him
		/// @todo Propagate events into active state
		if ( player == (CBaseEntity *)m_enemy && IsUsingKnife() )
		{
			Run();
			m_mustRunTimer.Start( 5 );
		}
		break;
	case EVENT_BOMB_PLANTED:
		if ( m_iTeam == CT )
			Idle();

		// if we are following someone, stop following
		if ( m_isFollowing )
		{
			StopFollowing();
			Idle();
		}

		OnEvent( EVENT_BOMB_BEEP, other, NULL );
		return;
	case EVENT_BOMB_PICKED_UP:
		if ( m_iTeam == CT )
		{
			// check if we're close enough to hear it
			const float bombPickupHearRangeSq = 1000.0f * 1000.0f;
			if ( (pev->origin - entity->pev->origin).LengthSquared() < bombPickupHearRangeSq )
			{
				GetChatter()->TheyPickedUpTheBomb();
			}
		}
		return;
	case EVENT_BOMB_BEEP:
		// if we don't know where the bomb is, but heard it beep, we've discovered it
		if ( !GetGameState()->IsPlantedBombLocationKnown() )
		{
			// check if we're close enough to hear it
			if ( (pev->origin - entity->pev->origin).LengthSquared() < 1000 * 1000 )
			{
				// radio the news to our team
				if ( m_iTeam == CT && GetGameState()->GetPlantedBombsite() == CSGameState::UNKNOWN )
				{
					const CCSBotManager::Zone *zone = manager->GetZone( &entity->pev->origin );
					if ( zone )
						GetChatter()->FoundPlantedBomb( zone->m_index );
				}

				// remember where the bomb is
				GetGameState()->UpdatePlantedBomb( &entity->pev->origin );
			}
		}
		return;
	case EVENT_BOMB_DEFUSE_ABORTED:
		PrintIfWatched( "BOMB DEFUSE ABORTED\n" );
		return;
	case EVENT_BOMB_DEFUSED:
		if ( m_iTeam == CT && manager->GetBombTimeLeft() < 2 )
		{
			GetChatter()->Say( "BarelyDefused" );
		}
		return;
	case EVENT_TERRORISTS_WIN:
		if ( m_iTeam == TERRORIST )
			GetChatter()->CelebrateWin();
		return;
	case EVENT_CTS_WIN:
		if ( m_iTeam == CT )
			GetChatter()->CelebrateWin();
		return;
	default:
		break;
	}

	//
	// Process radio events from our team
	//
	if ( player && player->m_iTeam == m_iTeam && event > EVENT_START_RADIO_1 && event < EVENT_END_RADIO )
	{
		/// @todo Distinguish between radio commands and responses
		if ( event != EVENT_RADIO_AFFIRMATIVE && event != EVENT_RADIO_NEGATIVE && event != EVENT_RADIO_REPORTING_IN )
		{
			m_lastRadioCommand = event;
			m_lastRadioRecievedTimestamp = gpGlobals->time;
			m_radioSubject = player;
			m_radioPosition = player->pev->origin;
		}
		return;
	}

	if ( !entity )
		return;
	if ( !IsRogue() && event == EVENT_HOSTAGE_CALLED_FOR_HELP && m_iTeam == CT && IsHunting() )
	{
		if ( (entity->pev->origin - pev->origin).LengthSquared() > 1000 * 1000 )
			return;
		Vector center = entity->Center();
		if ( IsVisible( &center, false ) )
		{
			SetTask( COLLECT_HOSTAGES );
			Run();
			m_goalEntity = entity;
			MoveTo( &entity->pev->origin, m_hostageEscortCount ? FASTEST_ROUTE : SAFEST_ROUTE );
			PrintIfWatched( "I'm fetching a hostage that called out to me\n" );
			return;
		}
	}

	/// @todo Listen to non-player sounds
	if ( player == NULL )
		return;

	// don't pay attention to noise that friends make
	if ( !IsEnemy( player ) )
		return;

	float range;
	PriorityType priority;
	bool isHostile;
	if ( !IsGameEventAudible( event, entity, other, &range, &priority, &isHostile ) )
		return;

	if ( event == EVENT_HOSTAGE_USED )
	{
		// this is here so T's not only act on the noise, but look at it, too
		if ( m_iTeam == TERRORIST )
		{
			// make sure we can hear the noise
			if ( (player->pev->origin - pev->origin).IsLengthGreaterThan( range ) )
				return;

			// tell our teammates that the hostages are being taken
			GetChatter()->HostagesBeingTaken();

			// only move if we hear them being rescued and can't see any hostages
			if ( GetGameState()->GetNearestVisibleFreeHostage() == NULL )
			{
				if ( GetTask() != GUARD_HOSTAGE_RESCUE_ZONE )
				{
					// head them off at a rescue zone
					if ( GuardRandomZone( 500 ) )
					{
						SetTask( GUARD_HOSTAGE_RESCUE_ZONE );
						SetDisposition( OPPORTUNITY_FIRE );
						PrintIfWatched( "Trying to beat them to an escape zone!\n" );
					}
				}
			}
		}
		else
		{
			// CT's don't care about this noise
			return;
		}
	}

	// check if noise is close enough for us to hear
	float newNoiseDist = (pev->origin - player->pev->origin).Length();
	if ( range <= newNoiseDist )
		return;

	// we heard the sound
	if ( (IsLocalPlayerWatchingMe() && cv_bot_debug.value == 3) || cv_bot_debug.value == 4 )
	{
		PrintIfWatched( "Heard noise (%sfrom %s, pri %s, time %3.1f)\n",
			(event == EVENT_WEAPON_FIRED) ? "Weapon fire " : "",
			STRING( player->pev->netname ),
			(priority == PRIORITY_HIGH) ? "HIGH" : ((priority == PRIORITY_MEDIUM) ? "MEDIUM" : "LOW"),
			gpGlobals->time );
	}

	if ( event == EVENT_PLAYER_FOOTSTEP && IsUsingSniperRifle() && newNoiseDist < 300 )
		EquipPistol();

	// should we pay attention to it
	// if noise timestamp is zero, there is no prior noise
	if ( m_noiseTimestamp <= 0 || gpGlobals->time - m_noiseTimestamp >= 3 ||
		(m_noisePriority <= priority && newNoiseDist < (pev->origin - m_noisePosition).Length()) )
	{
		// find the area in which the noise occured
		CNavArea *noiseArea = TheNavAreaGrid.GetNavArea( &player->pev->origin );
		if ( !noiseArea )
			noiseArea = TheNavAreaGrid.GetNearestNavArea( &player->pev->origin );
		if ( noiseArea )
		{
			m_noiseArea = noiseArea;

			// remember noise priority
			m_noisePriority = priority;

			// randomize noise position in the area a bit - hearing isn't very accurate
			// the closer the noise is, the more accurate our placement
			/// @todo Make sure not to pick a position on the opposite side of ourselves.
			const float maxErrorRadius = 400.0f;
			const float maxHearingRange = 2000.0f;
			float errorRadius = maxErrorRadius * newNoiseDist/maxHearingRange;

			m_noisePosition.x = player->pev->origin.x + RANDOM_FLOAT( -errorRadius, errorRadius );
			m_noisePosition.y = player->pev->origin.y + RANDOM_FLOAT( -errorRadius, errorRadius );

			// make sure noise position remains in the same area
			m_noiseArea->GetClosestPointOnArea( &m_noisePosition, &m_noisePosition );

			m_noiseTravelDistanceChecked = false;

			// note when we heard the noise
			m_noiseTimestamp = gpGlobals->time;
		}
		else
			PrintIfWatched( "  *** Noise occurred off the nav mesh - ignoring!\n" );
	}
}
