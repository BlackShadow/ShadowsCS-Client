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
#include "gamerules.h"

//--------------------------------------------------------------------------------------------------------------
/**
 * Heavyweight processing, invoked less often
 */
void CCSBot::Update( void )
{
	CCSBotManager *manager = TheCSBots();
	if ( CCSBotManager::m_isAnalysisRequested && m_process == 0 )
	{
		CCSBotManager::m_isAnalysisRequested = false;
		StartAnalyzeAlphaProcess();
	}

	switch ( m_process )
	{
	case 1:
		UpdateLearnProcess();
		return;
	case 2:
		UpdateAnalyzeAlphaProcess();
		return;
	case 3:
		UpdateAnalyzeBetaProcess();
		return;
	case 4:
		UpdateSaveProcess();
		return;
	}

	// update our radio chatter
	// need to allow bots to finish their chatter even if they are dead
	GetChatter()->Update();

	if ( m_voiceEndTime != 0.0f && (gpGlobals->time >= m_voiceEndTime || m_voiceStartTime > gpGlobals->time) )
		EndVoiceFeedback( false );

	// check if we are dead
	if ( !IsAlive() )
	{
		// remember that we died
		m_diedLastRound = true;

		BotDeathThink();
		return;
	}

	if ( (cv_bot_traceview.value == 100.0f && IsLocalPlayerWatchingMe()) || cv_bot_traceview.value == 101.0f )
	{
		Vector angles = pev->v_angle + pev->punchangle;
		UTIL_MakeVectors( angles );
		Vector forward = gpGlobals->v_forward;
		Vector eye = EyePosition();
		if ( !IsFriendInLineOfFire() )
		{
			Vector end = eye + forward * 2000.0f;
			if ( m_iTeam == TERRORIST )
				UTIL_DrawBeamPoints( eye, end, 1, 255, 0, 0 );
			else
				UTIL_DrawBeamPoints( eye, end, 1, 0, 50, 255 );
		}
	}

	if ( (cv_bot_traceview.value == 2.0f && IsLocalPlayerWatchingMe()) || cv_bot_traceview.value == 3.0f )
		DrawApproachPoints();

	if ( (cv_bot_traceview.value == 4.0f && IsLocalPlayerWatchingMe()) || cv_bot_traceview.value == 5.0f )
	{
		if ( m_spotEncounter )
		{
			UTIL_DrawBeamPoints( m_spotEncounter->path.from, m_spotEncounter->path.to, 3, 0, 0, 255 );
			Vector direction = m_spotEncounter->path.to - m_spotEncounter->path.from;
			float length = direction.NormalizeInPlace();
			for ( SpotOrderList::const_iterator spot = m_spotEncounter->spotList.begin();
				spot != m_spotEncounter->spotList.end(); ++spot )
			{
				Vector position = m_spotEncounter->path.from + direction * (length * spot->t);
				UTIL_DrawBeamPoints( position, *spot->spot->GetPosition(), 3, 0, 255, 255 );
			}
		}
	}

	if ( cv_bot_traceview.value == 1.0f && IsLocalPlayerWatchingMe() )
	{
		m_eyePosition = pev->origin + pev->view_ofs;
		double angle = M_PI * m_lookAheadAngle / 180.0;
		float forwardX = (float)(50.0 * cos( angle ));
		float forwardY = (float)(50.0 * sin( angle ));
		Vector end( m_eyePosition.x + forwardX, m_eyePosition.y + forwardY, m_eyePosition.z );
		UTIL_DrawBeamPoints( m_eyePosition, end, 1, 0, 255, 255 );
	}

	if ( cv_bot_stop.value != 0.0f )
		return;

	// check if we are stuck
	StuckCheck();

	// if our current 'noise' was heard a long time ago, forget it
	const float rememberNoiseDuration = 20.0f;
	if ( m_noiseTimestamp > 0.0f && gpGlobals->time - m_noiseTimestamp > rememberNoiseDuration )
	{
		m_noiseTimestamp = 0.0f;
	}

	// where are we
	if ( !m_currentArea || !m_currentArea->Contains( &pev->origin ) )
	{
		m_currentArea = TheNavAreaGrid.GetNavArea( &pev->origin, 120.0f );
	}

	// track the last known area we were in
	if ( m_currentArea && m_currentArea != m_lastKnownArea )
	{
		m_lastKnownArea = m_currentArea;
		m_currentArea->SetClearedTimestamp( m_iTeam - 1 );
	}

	if ( (m_approachPointViewPosition - pev->origin).LengthSquared() > 2500.0f )
	{
		ComputeApproachPoints();
		m_approachPointViewPosition = pev->origin;
	}
	if ( cv_bot_show_nav.value > 0.0f && m_lastKnownArea )
		m_lastKnownArea->DrawConnectedAreas();

	// if we're blind, retreat!
	if ( m_blindUntilTime > gpGlobals->time )
	{
		if ( !IsAtHidingSpot() )
		{
			switch ( m_blindMoveDir )
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
				Crouch();
				break;
			}
		}
		if ( m_blindFire )
			PrimaryAttack();
		return;
	}

	//
	// Enemy acquisition and attack initiation
	//

	// take a snapshot and update our reaction time queue
	UpdateReactionQueue();

	// "threat" may be the same as our current enemy
	CBasePlayer *threat = GetRecognizedEnemy();
	if ( threat )
	{
		// adjust our personal "safe" time
		AdjustSafeTime();

		if ( IsUsingGrenade() )
		{
			ThrowGrenade( &threat->pev->origin );
		}
		else if ( GetDisposition() != IGNORE_ENEMIES &&
			(GetDisposition() == ENGAGE_AND_INVESTIGATE || GetDisposition() == OPPORTUNITY_FIRE ||
			IsPlayerLookingAtMe( threat ) || (pev->origin - threat->pev->origin).LengthSquared() < 562500.0f) )
		{
			if ( !m_enemy || threat != (CBasePlayer *)(CBaseEntity *)m_enemy || !IsAttacking() )
			{
				if ( !IsUsingKnife() || !IsHiding() ||
					(pev->origin - threat->pev->origin).LengthSquared() < 62500.0f )
				{
					Attack( threat );
				}
			}
		}
		else
		{
			// dont attack, but keep track of nearby enemies
			SetEnemy( threat );
			m_isEnemyVisible = true;
		}

		// if we aren't attacking but we are being attacked, retaliate
		if ( GetDisposition() != IGNORE_ENEMIES && !IsAttacking() && gpGlobals->time - m_attackedTimestamp < 1.0f )
		{
			// we may not be attacking our attacker, but at least we're not just taking it
			// (since m_attacker isn't reaction-time delayed, we can't directly use it)
			Attack( threat );
			PrintIfWatched( "Ouch! Retaliating!\n" );
		}
		manager->m_lastSeenEnemyTimestamp = gpGlobals->time;
	}

	//
	// Validate existing enemy, if any
	//
	if ( m_enemy )
	{
		if ( m_enemyDeathTimestamp == 0.0f ||
			(m_enemy && (m_enemy->IsAlive() || gpGlobals->time - m_enemyDeathTimestamp <= 1.0f - m_profile->GetSkill())) )
		{
			// check LOS to current enemy (chest & head), in case he's dead (GetNearestEnemy() only returns live players)
			// note we're not checking FOV - once we've acquired an enemy (which does check FOV), assume we know roughly where he is
			if ( IsVisible( (CBasePlayer *)(CBaseEntity *)m_enemy, false, &m_visibleEnemyParts ) )
			{
				m_isEnemyVisible = true;
				m_lastSawEnemyTimestamp = gpGlobals->time;
				m_lastEnemyPosition = m_enemy->pev->origin;
			}
			else
			{
				m_isEnemyVisible = false;
			}

			// check if enemy died
			if ( m_enemy->IsAlive() )
			{
				m_enemyDeathTimestamp = 0.0f;
				m_isLastEnemyDead = false;
			}
			else if ( m_enemyDeathTimestamp == 0.0f )
			{
				// note time of death (to allow bots to overshoot for a time)
				m_isLastEnemyDead = true;
				m_enemyDeathTimestamp = gpGlobals->time;
			}
		}
		else
		{
			// we have noticed that our enemy has died
			m_enemy = NULL;
			m_isEnemyVisible = false;
		}
	}
	else
	{
		m_isEnemyVisible = false;
	}

	// if we have seen an enemy recently, keep an eye on him if we can
	const float seenRecentTime = 3.0f;
	m_isAimingAtEnemy = m_enemy && gpGlobals->time - m_lastSawEnemyTimestamp < seenRecentTime;

	//
	// Hack to fire while retreating
	/// @todo Encapsulate aiming and firing on enemies separately from current task
	//
	if ( GetDisposition() == IGNORE_ENEMIES )
		FireWeaponAtEnemy();

	Vector grenadeTarget;
	if ( IsEndOfSafeTime() && IsUsingGrenade() && (IsWellPastSafe() || !IsUsingHEGrenade()) &&
		!m_isWaitingToThrowGrenade && FindGrenadeTossPathTarget( &grenadeTarget ) )
	{
		ThrowGrenade( &grenadeTarget );
	}
	if ( IsUsingGrenade() )
	{
		if ( m_isWaitingToThrowGrenade && (m_tossGrenadeTimer.IsElapsed() || m_lookAtSpotState == 2) )
		{
			ClearPrimaryAttack();
			m_isWaitingToThrowGrenade = false;
		}
		else
		{
			PrimaryAttack();
		}
	}
	else
	{
		m_isWaitingToThrowGrenade = false;
	}
	if ( IsHunting() && IsWellPastSafe() && IsUsingGrenade() )
		EquipBestWeapon( true );

	// check if our weapon is totally out of ammo
	// or if we no longer feel "safe", equip our weapon
	if ( !IsSafe() && !IsUsingGrenade() && IsActiveWeaponOutOfAmmo() )
		EquipBestWeapon( false );

	/// @todo This doesn't work if we are restricted to just knives and sniper rifles because we cant use the rifle at close range
	if ( !IsSafe() && !IsUsingGrenade() && IsUsingKnife() && !IsEscapingFromBomb() )
		EquipBestWeapon( false );

	// if we haven't seen an enemy in awhile, and we switched to our pistol during combat, 
	// switch back to our primary weapon (if it still has ammo left)
	const float safeRearmTime = 5.0f;
	if ( !IsActiveWeaponReloading() && IsUsingPistol() && !IsPrimaryWeaponEmpty() && gpGlobals->time - m_lastSawEnemyTimestamp > safeRearmTime )
	{
		EquipBestWeapon( false );
	}

	// reload our weapon if we must
	ReloadCheck();

	// equip silencer
	SilencerCheck();

	// listen to the radio
	RespondToRadioCommands();

	// make way
	const float avoidTime = 0.33f;
	if ( gpGlobals->time - m_avoidTimestamp < avoidTime && m_avoid )
	{
		StrafeAwayFromPosition( &m_avoid->pev->origin );
	}
	else
	{
		m_avoid = NULL;
	}

	if ( m_isJumpCrouching )
	{
		float elapsed = gpGlobals->time - m_jumpCrouchTimestamp;
		if ( elapsed > 0.05f && elapsed < 0.6f )
			Crouch();
		if ( elapsed >= 0.6f )
			StandUp();
		if ( elapsed > 0.75f )
			m_isJumpCrouching = false;
	}

	// if we're using a sniper rifle and are no longer attacking, stop looking thru scope
	if ( !IsAtHidingSpot() && !IsAttacking() && IsUsingSniperRifle() && IsUsingScope() )
	{
		SecondaryAttack();
	}

	// check encounter spots
	UpdatePeripheralVision();

	//
	// Update gamestate
	//
	if ( m_bomber )
		GetChatter()->SpottedBomber( (CBasePlayer *)(CBaseEntity *)m_bomber );

	if ( CanSeeLooseBomb() )
		GetChatter()->SpottedLooseBomb( manager->m_looseBomb );

	//
	// Scenario interrupts
	//
	if ( manager->m_scenario == CCSBotManager::SCENARIO_DEFUSE_BOMB )
	{
		// flee if the bomb is ready to blow and we aren't defusing it or attacking and we know where the bomb is
		// (aggressive players wait until its almost too late)
		float gonnaBlowTime = 8.0f - (2.0f * m_profile->GetAggression());

		// if we have a defuse kit, can wait longer
		if ( m_bHasDefuser )
			gonnaBlowTime *= 0.66f;

		if ( !IsEscapingFromBomb() &&								// we aren't already escaping the bomb
			manager->m_isBombPlanted &&								// is the bomb planted
			GetGameState()->IsPlantedBombLocationKnown() &&			// we know where the bomb is
			gonnaBlowTime > manager->GetBombTimeLeft() &&			// is the bomb about to explode
			!IsDefusingBomb() &&									// we aren't defusing the bomb
			!IsAttacking() )										// we aren't in the midst of a firefight
		{
			EscapeFromBomb();
		}
	}
	else if ( manager->m_scenario == CCSBotManager::SCENARIO_RESCUE_HOSTAGES )
	{
		if ( m_iTeam == CT )
		{
			UpdateHostageEscortCount();
		}
		else
		{
			// Terrorists have imperfect information on status of hostages
			unsigned int status = GetGameState()->ValidateHostagePositions();

			if ( status & 4 )
			{
				GetChatter()->HostagesTaken();
				Idle();
			}
			else if ( status & 2 )
			{
				GetGameState()->SetHostageTaken( true );
				Idle();
			}
		}
	}

	//
	// Follow nearby humans if our co-op is high and we have nothing else to do
	// If we were just following someone, don't auto-follow again for a short while to 
	// give us a chance to do something else.
	//
	const float earliestAutoFollowTime = 5.0f;
	const float minAutoFollowTeamwork = 0.4f;
	if ( manager->GetElapsedRoundTime() > earliestAutoFollowTime &&
		m_profile->GetTeamwork() > minAutoFollowTeamwork &&
		gpGlobals->time > m_allowAutoFollowTime &&
		!IsBusy() )
	{
		if ( !m_isFollowing && !GetGameState()->IsAtPlantedBombsite() )
		{
			// chance of following is proportional to teamwork attribute
			float teamwork = m_profile->GetTeamwork();
			if ( teamwork > RANDOM_FLOAT( 0.0f, 1.0f ) )
			{
				CBasePlayer *leader = (CBasePlayer *)(CBaseEntity *)m_closestVisibleHumanFriend;
				if ( leader && gpGlobals->time > leader->m_allowAutoFollowTime && GetBotFollowCount( leader ) < 2 &&
					(leader->pev->origin - pev->origin).LengthSquared() < 90000.0f )
				{
					CNavArea *leaderArea = TheNavAreaGrid.GetNavArea( &leader->pev->origin, 120.0f );
					if ( leaderArea )
					{
						PathCost cost( this, FASTEST_ROUTE );
						if ( NavAreaTravelDistance( m_lastKnownArea, leaderArea, cost ) < 300.0f )
						{
							// follow this human
							Follow( leader );
							PrintIfWatched( "Auto-Following %s\n", STRING( leader->pev->netname ) );

							if ( ((CHalfLifeMultiplay *)g_pGameRules)->IsCareer() )
							{
								GetChatter()->Say( "FollowingCommander", 10.0f );
							}
							else
							{
								GetChatter()->Say( "FollowingSir", 10.0f );
							}
						}
					}
				}
			}
			else
			{
				// we decided not to follow, don't re-check for a duration
				m_allowAutoFollowTime = gpGlobals->time + 15.0f + (1.0f - m_profile->GetTeamwork()) * 30.0f;
			}
		}
	}

	if ( m_isFollowing )
	{
		// if we are following someone, make sure they are still alive
		CBaseEntity *leader = m_leader;
		if ( !leader || !leader->IsAlive() )
			StopFollowing();

		// decide whether to continue following them
		float teamwork = m_profile->GetTeamwork();
		if ( teamwork < 0.85f && gpGlobals->time - m_followTimestamp > teamwork * 40.0f + 15.0f )
		{
			// we are bored of following our leader
			StopFollowing();
			PrintIfWatched( "Stopping following - bored\n" );
		}
	}
	if ( !m_isFollowing && m_morale < 0 && IsSafe() && GetSafeTimeRemaining() < 2.0f && IsHunting() )
	{
		float chance = m_morale * -40.0f;
		if ( chance > RANDOM_FLOAT( 0.0f, 100.0f ) && (manager->IsOnOffense( this ) || !manager->m_isDefenseRushing) )
		{
			SetDisposition( OPPORTUNITY_FIRE );
			float duration = RANDOM_FLOAT( 3.0f, 15.0f );
			Hide( m_lastKnownArea, duration, 750.0f, false );
			GetChatter()->Say( "WaitingHere" );
		}
	}

	//
	// Execute state machine
	//
	if ( m_isAttacking )
	{
		m_attackState.OnUpdate( this );
	}
	else
	{
		m_state->OnUpdate( this );
	}

	if ( m_isWaitingToThrowGrenade )
	{
		ResetStuckMonitor();
		ClearMovement();
	}

	// don't move while reloading unless we see an enemy
	CBasePlayerWeapon *weapon = GetActiveWeapon();
	if ( weapon && weapon->m_fInReload && !m_isEnemyVisible )
	{
		ResetStuckMonitor();
		ClearMovement();
	}

	// if we get too far ahead of the hostages we are escorting, wait for them
	if ( !IsAttacking() && m_inhibitWaitingForHostageTimer.IsElapsed() && m_task == RESCUE_HOSTAGES &&
		GetRangeToFarthestEscortedHostage() > 500.0f )
	{
		if ( m_isWaitingForHostage )
		{
			// we've been waiting
			if ( !m_waitForHostageTimer.IsElapsed() )
			{
				// keep waiting
				ResetStuckMonitor();
				ClearMovement();
			}
			else
			{
				// give up waiting for awhile
				m_isWaitingForHostage = false;
				m_inhibitWaitingForHostageTimer.Start( 3.0f );
			}
		}
		else
		{
			// just started waiting
			m_isWaitingForHostage = true;
			m_waitForHostageTimer.Start( 10.0f );
		}
	}

	// remember our prior safe time status
	m_wasSafe = IsSafe();
}
