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

extern int gmsgBotProgress;

//--------------------------------------------------------------------------------------------------------------
/**
 * Constructor
 */
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
CCSBot::CCSBot( void ) : m_gameState( this ), m_chatter( this )
{
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Prepare bot for action
 */
bool CCSBot::Initialize( const BotProfile *profile )
{
	// extend
	CBot::Initialize( profile );

	// CS bot initialization
	m_diedLastRound = false;
	m_morale = 1;			// starting a new round makes everyone a little happy
	m_navigationState = 0;
	m_currentNode = NULL;
	m_combatRange = RANDOM_FLOAT( 325.0f, 425.0f );
	m_name[0] = '\0';

	// set initial safe time guess for this map
	m_safeTime = 5.0f * m_profile->GetAggression() + 15.0f;
	ResetValues();
	StartNormalProcess();
	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Reset internal data to initial state
 */
void CCSBot::ResetValues( void )
{
	m_chatter.Reset();
	m_gameState.Reset();
	m_avoid = NULL;
	m_avoidTimestamp = 0.0f;
	m_hurryTimer.Invalidate();
	m_isStuck = false;
	m_stuckTimestamp = 0.0f;
	m_nextWiggleTime = 0.0f;
	m_nextJumpTime = 0.0f;
	m_pathLength = 0;
	m_pathIndex = 0;
	m_areaEnteredTimestamp = 0.0f;
	m_currentArea = NULL;
	m_lastKnownArea = NULL;
	m_avoidFriendTimer.Invalidate();
	m_isFriendInTheWay = false;
	m_isWaitingBehindFriend = false;
	m_disposition = ENGAGE_AND_INVESTIGATE;
	m_enemy = NULL;
	m_isWaitingToThrowGrenade = false;
	m_wasSafe = true;
	m_nearbyEnemyCount = 0;
	m_enemyPlace = UNDEFINED_PLACE;
	m_nearbyFriendCount = 0;
	m_closestVisibleFriend = NULL;
	m_closestVisibleHumanFriend = NULL;
	for ( int i = 0; i < BOT_MAX_CLIENTS; ++i )
	{
		m_watchInfo[i].timestamp = 0.0f;
		m_watchInfo[i].isEnemy = false;
	}
	m_isEnemyVisible = false;
	m_visibleEnemyParts = 0;
	m_lastSawEnemyTimestamp = 0.0f;
	m_firstSawEnemyTimestamp = 0.0f;
	m_currentEnemyAcquireTimestamp = 0.0f;
	m_isLastEnemyDead = true;
	m_attacker = NULL;
	m_attackedTimestamp = 0.0f;
	m_enemyDeathTimestamp = 0.0f;
	m_lastVictimID = 0;
	m_isAimingAtEnemy = false;
	m_fireWeaponTimestamp = 0.0f;
	m_equipTimer.Invalidate();
	m_isFollowing = false;
	m_leader = NULL;
	m_followTimestamp = 0.0f;
	m_allowAutoFollowTime = 0.0f;
	m_enemyQueueIndex = 0;
	m_enemyQueueCount = 0;
	m_enemyQueueAttendIndex = 0;
	m_bomber = NULL;
	m_lookAroundStateTimestamp = 0.0f;
	m_inhibitLookAroundTimestamp = 0.0f;
	m_lookPitch = 0.0f;
	m_lookPitchVel = 0.0f;
	m_lookYaw = 0.0f;
	m_lookYawVel = 0.0f;
	m_aimOffsetTimestamp = 0.0f;
	m_aimSpreadTimestamp = 0.0f;
	m_lookAtSpotState = 0;
	m_spotEncounter = NULL;
	m_spotCheckTimestamp = 0.0f;
	m_peripheralTimestamp = 0.0f;
	m_avgVelIndex = 0;
	m_avgVelCount = 0;
	m_lastOrigin = pev ? pev->origin : Vector( 0.0f, 0.0f, 0.0f );
	m_lastRadioCommand = EVENT_INVALID;
	m_lastRadioRecievedTimestamp = 0.0f;
	m_lastRadioSentTimestamp = 0.0f;
	m_radioSubject = NULL;
	m_voiceEndTime = 0.0f;
	m_hostageEscortCount = 0;
	m_hostageEscortCountTimestamp = 0.0f;
	m_noisePosition = Vector( 0.0f, 0.0f, 0.0f );
	m_noiseTimestamp = 0.0f;
	m_noiseCheckTimestamp = 0.0f;
	m_noiseTravelDistanceChecked = false;
	m_stateTimestamp = 0.0f;
	m_task = SEEK_AND_DESTROY;
	m_taskEntity = NULL;
	m_approachPointCount = 0;
	m_approachPointViewPosition = Vector( 0.0f, 0.0f, 0.0f );
	m_checkedHidingSpotCount = 0;
	m_isJumpCrouching = false;
	StandUp();
	Run();
	m_mustRunTimer.Invalidate();
	m_pathLadder = NULL;
	m_repathTimer.Invalidate();
	m_huntState.m_huntArea = NULL;

	// adjust morale - if we died, our morale decreased, 
	// but if we live, no adjustement (round win/loss also adjusts morale)
	if ( m_diedLastRound )
		DecreaseMorale();

	m_diedLastRound = false;

	// IsRogue() randomly changes this
	m_isRogue = false;

	m_surpriseDelay = 0.0f;
	m_lastPanicTime = 0.0f;

	// even though these are EHANDLEs, they need to be NULL-ed
	m_goalEntity = NULL;
	m_avoid = NULL;
	m_enemy = NULL;

	// start in idle state
	StopAttacking();
	Idle();
}

//--------------------------------------------------------------------------------------------------------------
void BotState::OnEnter( CCSBot * )
{
}

//--------------------------------------------------------------------------------------------------------------
void BotState::OnUpdate( CCSBot * )
{
}

//--------------------------------------------------------------------------------------------------------------
void BotState::OnExit( CCSBot * )
{
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Increase morale one step
 */
void CCSBot::IncreaseMorale( void )
{
	if ( m_morale <= 2 )
		++m_morale;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Decrease morale one step
 */
void CCSBot::DecreaseMorale( void )
{
	if ( m_morale >= -2 )
		--m_morale;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Sets our current disposition
 */
void CCSBot::SetDisposition( DispositionType disposition )
{
	m_disposition = disposition;
	if ( disposition != IGNORE_ENEMIES )
		m_ignoreEnemiesTimer.Invalidate();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return our current disposition
 */
CCSBot::DispositionType CCSBot::GetDisposition( void ) const
{
	return m_ignoreEnemiesTimer.IsElapsed() ? m_disposition : IGNORE_ENEMIES;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Ignore enemies for a short durationy
 */
void CCSBot::IgnoreEnemies( float duration )
{
	m_ignoreEnemiesTimer.Start( duration );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we are acting like a rogue (not listening to teammates, not doing scenario goals)
 * @todo Account for morale
 */
bool CCSBot::IsRogue( void ) const
{
	if ( cv_bot_allow_rogues.value <= 0.0f )
		return false;

	// periodically re-evaluate our rogue status
	if ( m_rogueTimer.IsElapsed() )
	{
		m_rogueTimer.Start( RANDOM_FLOAT( 10.0f, 30.0f ) );

		// our chance of going rogue is inversely proportional to our teamwork attribute
		const float rogueChance = (1.0f - m_profile->GetTeamwork()) * 100.0f;

		m_isRogue = rogueChance > RANDOM_FLOAT( 0.0f, 100.0f );
	}

	return m_isRogue;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if it is the early, "safe", part of the round
 */
bool CCSBot::IsSafe( void ) const
{
	return m_safeTime > TheCSBots()->GetElapsedRoundTime();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if it is well past the early, "safe", part of the round
 */
bool CCSBot::IsWellPastSafe( void ) const
{
	return TheCSBots()->GetElapsedRoundTime() > 1.25f * m_safeTime;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we were in the safe time last update, but not now
 */
bool CCSBot::IsEndOfSafeTime( void ) const
{
	return m_wasSafe && m_safeTime <= TheCSBots()->GetElapsedRoundTime();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return the amount of "safe time" we have left
 */
float CCSBot::GetSafeTimeRemaining( void ) const
{
	return m_safeTime - TheCSBots()->GetElapsedRoundTime();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Called when enemy seen to adjust safe time for this round
 */
void CCSBot::AdjustSafeTime( void )
{
	float elapsed = TheCSBots()->GetElapsedRoundTime();
	if ( m_safeTime > elapsed )
		m_safeTime = elapsed - 2.0f;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we haven't seen an enemy for "a long time"
 */
bool CCSBot::HasNotSeenEnemyForLongTime( void ) const
{
	return gpGlobals->time - m_lastSawEnemyTimestamp > 30.0f;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Get our current radio chatter place
 */
Place CCSBot::GetPlace( void ) const
{
	return m_lastKnownArea ? m_lastKnownArea->GetPlace() : UNDEFINED_PLACE;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return last enemy that hurt us
 */
CBasePlayer *CCSBot::GetAttacker( void ) const
{
	return m_attacker && m_attacker->IsAlive() ? m_attacker : NULL;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we are outnumbered by enemies
 */
bool CCSBot::IsOutnumbered( void ) const
{
	return (GetNearbyFriendCount() < GetNearbyEnemyCount() - 1) ? true : false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return number of enemies we are outnumbered by
 */
int CCSBot::OutnumberedCount( void ) const
{
	if ( IsOutnumbered() )
		return (GetNearbyEnemyCount() - 1) - GetNearbyFriendCount();

	return 0;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we will do scenario-related tasks
 */
bool CCSBot::IsDoingScenario( void ) const
{
	return cv_bot_defer_to_human.value <= 0.0f || UTIL_HumansOnTeam( m_iTeam, true ) == 0;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we noticed the bomb on the ground or on the radar (for T's only)
 */
bool CCSBot::NoticeLooseBomb( void ) const
{
	return TheCSBots()->GetScenario() == CCSBotManager::SCENARIO_DEFUSE_BOMB && TheCSBots()->GetLooseBomb() != NULL;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if can see the bomb lying on the ground
 */
bool CCSBot::CanSeeLooseBomb( void ) const
{
	if ( TheCSBots()->GetScenario() != CCSBotManager::SCENARIO_DEFUSE_BOMB )
		return false;
	CBaseEntity *bomb = TheCSBots()->GetLooseBomb();
	return bomb && IsVisible( &bomb->pev->origin, true );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if can see the planted bomb 
 */
bool CCSBot::CanSeePlantedBomb( void ) const
{
	if ( TheCSBots()->GetScenario() != CCSBotManager::SCENARIO_DEFUSE_BOMB || m_gameState.GetBombState() != CSGameState::PLANTED )
		return false;
	const Vector *position = m_gameState.GetBombPosition();
	return position && IsVisible( position, true );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return time when given spot was last checked
 */
float CCSBot::GetHidingSpotCheckTimestamp( HidingSpot *spot ) const
{
	for ( int i = 0; i < m_checkedHidingSpotCount; ++i )
	{
		if ( m_checkedHidingSpot[i].spot->GetID() == spot->GetID() )
			return m_checkedHidingSpot[i].timestamp;
	}
	return -999999.9f;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Set the timestamp of the given spot to now.
 * If the spot is not in the set, overwrite the least recently checked spot.
 */
void CCSBot::SetHidingSpotCheckTimestamp( HidingSpot *spot )
{
	int leastRecent = 0;
	float leastRecentTime = gpGlobals->time + 1.0f;

	for ( int i = 0; i < m_checkedHidingSpotCount; ++i )
	{
		// if spot is in the set, just update its timestamp
		if ( m_checkedHidingSpot[i].spot->GetID() == spot->GetID() )
		{
			m_checkedHidingSpot[i].timestamp = gpGlobals->time;
			return;
		}

		// keep track of least recent spot
		if ( leastRecentTime > m_checkedHidingSpot[i].timestamp )
		{
			leastRecentTime = m_checkedHidingSpot[i].timestamp;
			leastRecent = i;
		}
	}

	int index = leastRecent;
	if ( m_checkedHidingSpotCount < 64 )
		index = m_checkedHidingSpotCount++;
	m_checkedHidingSpot[index].spot = spot;
	m_checkedHidingSpot[index].timestamp = gpGlobals->time;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Assign given player as our current enemy to attack
 */
void CCSBot::SetEnemy( CBasePlayer *enemy )
{
	if ( (CBaseEntity *)m_enemy != enemy )
	{
		m_enemy = enemy;
		m_currentEnemyAcquireTimestamp = gpGlobals->time;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we are in a hurry 
 */
bool CCSBot::IsHurrying( void ) const
{
	if ( !m_hurryTimer.IsElapsed() )
		return true;
	if ( TheCSBots()->GetScenario() == CCSBotManager::SCENARIO_DEFUSE_BOMB )
		return TheCSBots()->IsBombPlanted();
	return TheCSBots()->GetScenario() == CCSBotManager::SCENARIO_RESCUE_HOSTAGES && m_iTeam == TERRORIST && m_gameState.AreAllHostagesBeingRescued();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Change movement speed to walking
 */
void CCSBot::Walk( void )
{
	if ( m_mustRunTimer.IsElapsed() )
	{
		CBot::Walk();
	}
	else
	{
		// must run
		Run();
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if jump was started.
 * This is extended from the base jump to disallow jumping when in a crouch area.
 */
bool CCSBot::Jump( bool mustJump )
{
	if ( m_lastKnownArea && (m_lastKnownArea->GetAttributes() & NAV_CROUCH) && !(m_lastKnownArea->GetAttributes() & NAV_JUMP) )
		return false;
	return CBot::Jump( mustJump );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Immediately jump off of our ladder, if we're on one
 */
void CCSBot::GetOffLadder( void )
{
	if ( m_pathLadder )
	{
		Jump( true );
		DestroyPath();
	}
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::Disconnect( void )
{
	EndVoiceFeedback( true );
	if ( m_process != 0 )
	{
		MESSAGE_BEGIN( MSG_ALL, gmsgBotProgress );
		WRITE_BYTE( 2 );
		MESSAGE_END();
	}
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::RoundRespawn( void )
{
	CBasePlayer::RoundRespawn();
	EndVoiceFeedback( true );
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::BotDeathThink( void )
{
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::SpawnBot( void )
{
	TheCSBots()->ValidateMapData();
	ResetValues();
	strcpy( m_name, STRING( pev->netname ) );
	SetState( &m_buyState );
	SetTouch( &CCSBot::BotTouch );
	if ( TheNavAreaList.empty() && !CCSBotManager::IsLearningMap() )
	{
		CCSBotManager::m_isLearningMap = true;
		StartLearnProcess();
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Lightweight maintenance, invoked frequently
 */
void CCSBot::Upkeep( void )
{
	if ( CCSBotManager::IsLearningMap() || !IsAlive() )
		return;
	if ( m_isRapidFiring )
		TogglePrimaryAttack();
	if ( !m_isAimingAtEnemy )
	{
		if ( m_lookAtSpotClearIfClose && (m_lookAtSpot - pev->origin).LengthSquared() < 10000.0f )
		{
			m_lookAtSpotState = 0;
			m_lookPitch = 0.0f;
			m_lookYaw = m_lookAheadAngle;
		}
		else if ( m_lookAtSpotState == 1 )
		{
			UpdateLookAt();
			if ( IsLookingAtPosition( &m_lookAtSpot, m_lookAtSpotAngleTolerance ) )
			{
				m_lookAtSpotState = 2;
				m_lookAtSpotTimestamp = gpGlobals->time;
			}
		}
		else if ( m_lookAtSpotState == 2 )
		{
			UpdateLookAt();
			if ( m_lookAtSpotDuration >= 0.0f && gpGlobals->time - m_lookAtSpotTimestamp > m_lookAtSpotDuration )
				m_lookAtSpotState = 0;
		}
		else if ( m_lookAtSpotState == 0 )
		{
			m_lookPitch = 0.0f;
			m_lookYaw = m_lookAheadAngle;
		}
		float amplitude = IsUsingSniperRifle() && IsUsingScope() ? 0.5f : 2.0f;
		m_lookYaw += BotCOS( 33.0f * gpGlobals->time ) * amplitude;
		m_lookPitch += BotSIN( 13.0f * gpGlobals->time ) * amplitude;
		UpdateLookAngles();
		return;
	}
	UpdateAimOffset();
	if ( (int)m_enemy == 0 )
	{
		UpdateLookAngles();
		return;
	}
	float halfHeight = IsCrouching() ? 18.0f : 36.0f;
	if ( !m_isEnemyVisible )
		m_aimSpot = m_lastEnemyPosition;
	else
	{
		if ( m_profile->GetSkill() > 0.5f )
		{
			Vector lead = (m_enemy->pev->velocity - pev->velocity) * (3.0f * g_flBotCommandInterval);
			m_aimSpot = m_enemy->pev->origin + lead;
		}
		else
			m_aimSpot = m_enemy->pev->origin;
		CBasePlayerWeapon *weapon = GetActiveWeapon();
		bool preferChest = IsUsingAWP() || IsUsingShotgun() || IsUsingMachinegun() || m_profile->GetSkill() < 0.8f ||
			(weapon && weapon->m_flAccuracy > 0.4f && !IsUsingPistol() && !IsUsingSniperRifle());
		if ( preferChest && IsEnemyPartVisible( CHEST ) )
			m_aimSpot.z -= halfHeight * 0.25f;
		else if ( !preferChest && IsEnemyPartVisible( HEAD ) )
		{
		}
		else if ( IsEnemyPartVisible( CHEST ) )
			m_aimSpot.z -= halfHeight * 0.5f;
		else if ( IsEnemyPartVisible( HEAD ) )
		{
		}
		else if ( IsEnemyPartVisible( LEFT_SIDE ) )
		{
			Vector2D direction = (m_enemy->pev->origin - pev->origin).Make2D();
			direction.NormalizeInPlace();
			m_aimSpot.x -= direction.y * 16.0f;
			m_aimSpot.y += direction.x * 16.0f;
			m_aimSpot.z -= halfHeight * 0.5f;
		}
		else if ( IsEnemyPartVisible( RIGHT_SIDE ) )
		{
			Vector2D direction = (m_enemy->pev->origin - pev->origin).Make2D();
			direction.NormalizeInPlace();
			m_aimSpot.x += direction.y * 16.0f;
			m_aimSpot.y -= direction.x * 16.0f;
			m_aimSpot.z -= halfHeight * 0.5f;
		}
		else
			m_aimSpot.z -= halfHeight + halfHeight;
	}
	m_aimSpot = m_aimSpot + m_aimOffset;
	Vector angles = UTIL_VecToAngles( m_aimSpot - pev->origin );
	m_lookPitch = 360.0f - angles.x;
	m_lookYaw = angles.y;
	UpdateLookAngles();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Reset the stuck-checker.
 */
void CCSBot::ResetStuckMonitor( void )
{
	if ( m_isStuck && IsLocalPlayerWatchingMe() && cv_bot_debug.value > 0.0f )
		EMIT_SOUND_DYN( pev->pContainingEntity, CHAN_VOICE, "buttons/bell1.wav", 1.0f, ATTN_NORM, 0, PITCH_NORM );
	m_isStuck = false;
	m_stuckTimestamp = 0.0f;
	m_nextJumpTime = 0.0f;
	m_avgVelIndex = 0;
	m_avgVelCount = 0;
	m_areaEnteredTimestamp = gpGlobals->time;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Test if we have become stuck
 */
void CCSBot::StuckCheck( void )
{
	if ( m_isStuck )
	{
		// we are stuck - see if we have moved far enough to be considered unstuck
		Vector delta = pev->origin - m_stuckSpot;

		const float unstuckRange = 75.0f;
		if ( delta.IsLengthGreaterThan( unstuckRange ) )
		{
			// we are no longer stuck
			ResetStuckMonitor();
			PrintIfWatched( "UN-STUCK\n" );
		}
	}
	else
	{
		// check if we are stuck

		// compute average velocity over a short period (for stuck check)
		Vector vel = pev->origin - m_lastOrigin;

		// if we are jumping, ignore Z
		if ( IsJumping() )
			vel.z = 0.0f;

		// cannot be Length2D, or will break ladder movement (they are only Z)
		m_avgVel[m_avgVelIndex++] = vel.Length() / g_flBotFullThinkInterval;

		if ( m_avgVelIndex == 5 )
			m_avgVelIndex = 0;

		if ( m_avgVelCount <= 4 )
		{
			++m_avgVelCount;
		}
		else
		{
			// we have enough samples to know if we're stuck

			float avgVel = 0.0f;
			for ( int t = 0; t < m_avgVelCount; ++t )
				avgVel += m_avgVel[t];

			avgVel /= 5.0f;

			// cannot make this velocity too high, or bots will get "stuck" when going down ladders
			float stuckVel = m_pathLadder ? 10.0f : 20.0f;

			if ( avgVel < stuckVel )
			{
				// we are stuck - note when and where we initially become stuck
				m_stuckTimestamp = gpGlobals->time;
				m_stuckSpot = pev->origin;
				float now = gpGlobals->time;
				m_nextJumpTime = RANDOM_FLOAT( 0.0f, 0.5f ) + now;
				PrintIfWatched( "STUCK\n" );
				if ( IsLocalPlayerWatchingMe() && cv_bot_debug.value > 0.0f )
					EMIT_SOUND_DYN( pev->pContainingEntity, CHAN_VOICE, "buttons/button11.wav", 1.0f, ATTN_NORM, 0, PITCH_NORM );
				m_isStuck = true;
			}
		}
	}
	m_lastOrigin = pev->origin;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * For getting un-stuck
 */
void CCSBot::Wiggle( void )
{
	if ( IsCrouching() )
	{
		ResetStuckMonitor();
		return;
	}

	// for wiggling
	if ( gpGlobals->time >= m_nextWiggleTime )
	{
		m_wiggleDirection = RANDOM_LONG( 0, 3 );
		float now = gpGlobals->time;
		m_nextWiggleTime = RANDOM_FLOAT( 0.5f, 1.5f ) + now;
	}

	switch ( m_wiggleDirection )
	{
	case 0:
		MoveForward();
		break;
	case 1:
		StrafeRight();
		break;
	case 2:
		MoveBackward();
		break;
	case 3:
		StrafeLeft();
		break;
	default:
		break;
	}

	if ( gpGlobals->time >= m_nextJumpTime && Jump( false ) )
	{
		float now = gpGlobals->time;
		m_nextJumpTime = RANDOM_FLOAT( 1.0f, 2.0f ) + now;
	}
}

//--------------------------------------------------------------------------------------------------------------
bool CCSBot::IsEnemyPartVisible( VisiblePartType part ) const
{
	return m_isEnemyVisible && (m_visibleEnemyParts & part) != 0;
}

//--------------------------------------------------------------------------------------------------------------
CBasePlayer *CCSBot::FindNearbyPlayer( void )
{
	CBaseEntity *entity = NULL;
	Vector center = pev->origin;
	while ( (entity = UTIL_FindEntityInSphere( entity, center, 800.0f )) != NULL )
	{
		if ( entity->IsPlayer() && !(entity->pev->flags & FL_FAKECLIENT) )
			return (CBasePlayer *)entity;
	}
	return NULL;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return the closest "important" enemy for the given scenario (bomb carrier, VIP, hostage escorter)
 */
CBasePlayer *CCSBot::GetImportantEnemy( bool checkVisibility ) const
{
	CBasePlayer *closest = NULL;
	float closestDistance = 1000000000.0f;
	CCSBotManager *manager = TheCSBots();
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBaseEntity *entity = UTIL_PlayerByIndex( i );
		if ( entity == NULL || entity->pev == NULL || FNullEnt( entity->pev ) || STRING( entity->pev->netname )[0] == '\0' )
			continue;
		if ( !entity->IsPlayer() || !entity->IsAlive() )
			continue;
		CBasePlayer *player = (CBasePlayer *)entity;
		if ( player->m_iTeam == m_iTeam || !manager->IsImportantPlayer( player ) )
			continue;
		float distance = (pev->origin - player->pev->origin).LengthSquared();
		if ( closestDistance > distance && (!checkVisibility || IsVisible( player, true, NULL )) )
		{
			closestDistance = distance;
			closest = player;
		}
	}
	return closest;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Pick a random zone and hide near it
 */
bool CCSBot::GuardRandomZone( float range )
{
	CCSBotManager *manager = TheCSBots();
	if ( manager->GetZoneCount() == 0 )
		return false;
	const CCSBotManager::Zone *zone = manager->GetZone( RANDOM_LONG( 0, manager->GetZoneCount() - 1 ) );
	if ( zone == NULL )
		return false;
	CNavArea *area = manager->GetRandomAreaInZone( zone );
	if ( area == NULL )
		return false;
	Hide( area, -1.0f, range, false );
	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Retreat to a nearby hiding spot, away from enemies
 */
bool CCSBot::TryToRetreat( void )
{
	const Vector *spot = FindNearbyRetreatSpot( this, 1000.0f );
	if ( spot )
	{
		// ignore enemies for a second to give us time to hide
		// reaching our hiding spot clears our disposition
		IgnoreEnemies( 10.0f );

		float duration = RANDOM_FLOAT( 3.0f, 15.0f );

		StandUp();
		Run();
		Hide( spot, duration, false );

		PrintIfWatched( "Retreating to a safe spot!\n" );

		return true;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Periodic check of hostage count in case we lost some
 */
void CCSBot::UpdateHostageEscortCount( void )
{
	const float updateInterval = 1.0f;
	if ( m_hostageEscortCount == 0 || gpGlobals->time - m_hostageEscortCountTimestamp < updateInterval )
		return;

	m_hostageEscortCountTimestamp = gpGlobals->time;

	// recount the hostages in case we lost some
	m_hostageEscortCount = 0;

	CBaseEntity *entity = NULL;
	while ( (entity = UTIL_FindEntityByClassname( entity, "hostage_entity" )) != NULL )
	{
		if ( FNullEnt( entity->pev ) )
			return;
		CHostage *hostage = (CHostage *)entity;
		if ( hostage->pev->takedamage == DAMAGE_YES && hostage->IsFollowing( this ) )
			++m_hostageEscortCount;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return euclidean distance to farthest escorted hostage.
 * Return -1 if no hostage is following us.
 */
float CCSBot::GetRangeToFarthestEscortedHostage( void ) const
{
	float farthest = -1.0f;
	for ( int i = 0; i < g_pHostages->m_hostageCount; ++i )
	{
		CHostage *hostage = g_pHostages->m_hostage[i];
		if ( hostage->pev->deadflag != DEAD_DEAD && hostage->pev->takedamage == DAMAGE_YES && hostage->IsFollowing( this ) )
		{
			float distance = (hostage->Center() - pev->origin).Length();
			if ( distance > farthest )
				farthest = distance;
		}
	}
	return farthest;
}

//--------------------------------------------------------------------------------------------------------------
class CollectRetreatSpotsFunctor
{
public:
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
	CollectRetreatSpotsFunctor( CCSBot *me, float range ) : m_me( me ), m_range( range ), m_count( 0 )
	{
	}

	enum { MAX_SPOTS = 256 };

	bool operator()( CNavArea *area )
	{
		// collect all the hiding spots in this area
		const HidingSpotList *list = area->GetHidingSpotList();

		for ( HidingSpotList::const_iterator it = list->begin(); it != list->end() && m_count < MAX_SPOTS; ++it )
		{
			const Vector *position = (*it)->GetPosition();

			// make sure hiding spot is in range
			if ( m_range > 0.0f && (*position - m_me->pev->origin).LengthSquared() > m_range * m_range )
				continue;

			// if a Player is using this hiding spot, don't consider it
			if ( IsSpotOccupied( m_me, position ) )
				continue;

			// don't select spot if an enemy can see it
			Vector eye = *position + Vector( 0.0f, 0.0f, 36.0f );
			int enemyTeam = m_me->m_iTeam == TERRORIST ? CT : TERRORIST;
			if ( UTIL_IsVisibleToTeam( eye, enemyTeam, -1.0f ) )
				continue;

			// don't select spot if it is closest to an enemy
			CBasePlayer *owner = UTIL_GetClosestPlayer( position, NULL );
			if ( owner && owner->m_iTeam != m_me->m_iTeam )
				continue;

			m_spot[m_count++] = position;
		}

		// if we've filled up, stop searching
		return m_count != MAX_SPOTS;
	}

	CCSBot *m_me;
	float m_range;

	const Vector *m_spot[ MAX_SPOTS ];
	int m_count;
};

//--------------------------------------------------------------------------------------------------------------
/**
 * Do a breadth-first search to find a good retreat spot.
 * Don't pick a spot that a Player is currently occupying.
 */
const Vector *FindNearbyRetreatSpot( CCSBot *me, float maxRange )
{
	CNavArea *area = me->GetLastKnownArea();
	if ( area == NULL )
		return NULL;

	// collect spots that enemies cannot see
	CollectRetreatSpotsFunctor collector( me, maxRange );
	SearchSurroundingAreas( area, &me->pev->origin, collector, maxRange );

	if ( collector.m_count == 0 )
		return NULL;

	// select a hiding spot at random
	int which = RANDOM_LONG( 0, collector.m_count - 1 );
	return collector.m_spot[ which ];
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return the number of bots following the given player
 */
int GetBotFollowCount( CBasePlayer *leader )
{
	int count = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBaseEntity *entity = UTIL_PlayerByIndex( i );
		if ( entity == NULL || entity->pev == NULL || FNullEnt( entity->pev ) || STRING( entity->pev->netname )[0] == '\0' )
			continue;
		CBasePlayer *player = (CBasePlayer *)entity;
		if ( !player->IsBot() || !player->IsAlive() )
			continue;
		CCSBot *bot = (CCSBot *)player;
		if ( (CBaseEntity *)bot->m_leader == leader )
			++count;
	}
	return count;
}
