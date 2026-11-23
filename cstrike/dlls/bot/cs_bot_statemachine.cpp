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

//--------------------------------------------------------------------------------------------------------------
/**
 * This method is the ONLY legal way to change a bot's current state
 */
void CCSBot::SetState( BotState *state )
{
	PrintIfWatched( "SetState: %s -> %s\n", (m_state) ? m_state->GetName() : "NULL", state->GetName() );

	// if we changed state from within the special Attack state, we are no longer attacking
	if ( m_isAttacking )
		StopAttacking();

	if ( m_state )
		m_state->OnExit( this );

	state->OnEnter( this );

	m_state = state;
	m_stateTimestamp = gpGlobals->time;
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::Idle( void )
{
	SetTask( SEEK_AND_DESTROY );
	SetState( &m_idleState );
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::EscapeFromBomb( void )
{
	SetTask( ESCAPE_FROM_BOMB );
	SetState( &m_escapeFromBombState );
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::Follow( CBasePlayer *player )
{
	if ( player == NULL )
		return;

	// note when we began following
	if ( !m_isFollowing || m_leader != player )
		m_followTimestamp = gpGlobals->time;

	m_isFollowing = true;
	m_leader = player;

	SetTask( FOLLOW );
	m_followState.m_leader = player;
	SetState( &m_followState );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Continue following our leader after finishing what we were doing
 */
void CCSBot::ContinueFollowing( void )
{
	SetTask( FOLLOW );

	m_followState.m_leader = (CBaseEntity *)m_leader;

	SetState( &m_followState );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Stop following
 */
void CCSBot::StopFollowing( void )
{
	m_isFollowing = false;
	m_leader = NULL;
	m_allowAutoFollowTime = gpGlobals->time + 10.0f;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Begin process of rescuing hostages
 */
void CCSBot::RescueHostages( void )
{
	SetTask( RESCUE_HOSTAGES );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Use the entity
 */
void CCSBot::UseEntity( CBaseEntity *entity )
{
	m_useEntityState.SetEntity( entity );
	SetState( &m_useEntityState );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * DEPRECATED: Use TryToHide() instead.
 * Move to a hiding place.
 * If 'searchFromArea' is non-NULL, hiding spots are looked for from that area first.
 */
void CCSBot::Hide( CNavArea *searchFromArea, float duration, float hideRange, bool holdPosition )
{
	DestroyPath();

	CNavArea *source;
	Vector sourcePos;
	if ( searchFromArea )
	{
		source = searchFromArea;
		sourcePos = *searchFromArea->GetCenter();
	}
	else
	{
		source = m_lastKnownArea;
		sourcePos = pev->origin;
	}

	if ( source == NULL )
	{
		PrintIfWatched( "Hide from area is NULL.\n" );
		Idle();
		return;
	}

	m_hideState.m_range = hideRange;
	m_hideState.m_searchFromArea = source;
	m_hideState.m_isHoldingPosition = holdPosition;
	m_hideState.m_duration = duration;

	// search around source area for a good hiding spot
	const Vector *pos = FindNearbyHidingSpot( this, &sourcePos, source, hideRange, IsSniper(), false );
	if ( pos == NULL )
	{
		PrintIfWatched( "No available hiding spots.\n" );
		// hide at our current position
		pos = &pev->origin;
	}

	m_hideState.m_hidingSpot = *pos;

	// build a path to our new hiding spot
	if ( ComputePath( TheNavAreaGrid.GetNavArea( pos ), pos, FASTEST_ROUTE ) == false )
	{
		PrintIfWatched( "Can't pathfind to hiding spot\n" );
		Idle();
		return;
	}

	SetState( &m_hideState );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Move to the given hiding place
 */
void CCSBot::Hide( const Vector *hidingSpot, float duration, bool holdPosition )
{
	CNavArea *hideArea = TheNavAreaGrid.GetNearestNavArea( hidingSpot );
	if ( hideArea == NULL )
	{
		PrintIfWatched( "Hiding spot off nav mesh\n" );
		Idle();
		return;
	}

	DestroyPath();

	m_hideState.m_searchFromArea = hideArea;
	m_hideState.m_range = 750.0f;
	m_hideState.m_duration = duration;
	m_hideState.m_isHoldingPosition = holdPosition;
	m_hideState.m_hidingSpot = *hidingSpot;

	// build a path to our new hiding spot
	if ( ComputePath( hideArea, hidingSpot, FASTEST_ROUTE ) == false )
	{
		PrintIfWatched( "Can't pathfind to hiding spot\n" );
		Idle();
		return;
	}

	SetState( &m_hideState );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Try to hide nearby.  Return true if hiding, false if can't hide here.
 * If 'searchFromArea' is non-NULL, hiding spots are looked for from that area first.
 */
bool CCSBot::TryToHide( CNavArea *searchFromArea, float duration, float hideRange, bool holdPosition, bool useNearest )
{
	CNavArea *source;
	Vector sourcePos;
	if ( searchFromArea )
	{
		source = searchFromArea;
		sourcePos = *searchFromArea->GetCenter();
	}
	else
	{
		source = m_lastKnownArea;
		sourcePos = pev->origin;
	}

	if ( source == NULL )
	{
		PrintIfWatched( "Hide from area is NULL.\n" );
		return false;
	}

	m_hideState.m_range = hideRange;
	m_hideState.m_searchFromArea = source;
	m_hideState.m_isHoldingPosition = holdPosition;
	m_hideState.m_duration = duration;

	// search around source area for a good hiding spot
	const Vector *pos = FindNearbyHidingSpot( this, &sourcePos, source, hideRange, IsSniper(), useNearest );
	if ( pos == NULL )
	{
		PrintIfWatched( "No available hiding spots.\n" );
		return false;
	}

	m_hideState.m_hidingSpot = *pos;

	// build a path to our new hiding spot
	if ( ComputePath( TheNavAreaGrid.GetNavArea( pos ), pos, FASTEST_ROUTE ) == false )
	{
		PrintIfWatched( "Can't pathfind to hiding spot\n" );
		return false;
	}

	SetState( &m_hideState );
	return true;
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::Hunt( void )
{
	SetState( &m_huntState );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Attack our the given victim
 * NOTE: Attacking does not change our task.
 */
void CCSBot::Attack( CBasePlayer *victim )
{
	if ( victim == NULL )
		return;

	// zombies never attack
	if ( cv_bot_zombie.value > 0.0f )
		return;

	// cannot attack if we are reloading
	if ( IsActiveWeaponReloading() )
		return;

	// change enemy
	SetEnemy( victim );

	//
	// Do not "re-enter" the attack state if we are already attacking
	//
	if ( IsAttacking() )
		return;

	// if we are currently hiding, increase our chances of crouching and holding position
	if ( IsAtHidingSpot() )
		m_attackState.m_crouchAndHold = (RANDOM_FLOAT( 0.0f, 100.0f ) < 60.0f) ? true : false;
	else
		m_attackState.m_crouchAndHold = false;

	PrintIfWatched( "ATTACK BEGIN (reaction time = %g (+ update time), surprise time = %g, attack delay = %g)\n",
		GetProfile()->GetReactionTime(), m_surpriseDelay, GetProfile()->GetAttackDelay() );
	m_isAttacking = true;
	m_attackState.OnEnter( this );

	// cheat a bit and give the bot the initial location of its victim
	m_lastEnemyPosition = victim->pev->origin;
	m_lastSawEnemyTimestamp = gpGlobals->time;
	m_aimSpreadTimestamp = gpGlobals->time;

	// compute the angle difference between where are looking, and where we need to look
	Vector toEnemy = victim->pev->origin - pev->origin;

	Vector idealAngle = UTIL_VecToAngles( toEnemy );

	float deltaYaw = (float)abs( (int)(m_lookYaw - idealAngle.y) );

	while ( deltaYaw > 180.0f )
		deltaYaw -= 360.0f;

	if ( deltaYaw < 0.0f )
		deltaYaw = -deltaYaw;

	// immediately aim at enemy - accuracy penalty depending on how far we must turn to aim
	// accuracy is halved if we have to turn 180 degrees
	float turn = deltaYaw / 180.0f;
	float accuracy = GetProfile()->GetSkill() / (1.0f + turn);

	SetAimOffset( accuracy );

	// define time when aim offset will automatically be updated
	// longer time the more we had to turn (surprise)
	m_aimOffsetTimestamp = gpGlobals->time + RANDOM_FLOAT( 0.25f + turn, 1.5f );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Exit the Attack state
 */
void CCSBot::StopAttacking( void )
{
	PrintIfWatched( "ATTACK END\n" );
	m_attackState.OnExit( this );
	m_isAttacking = false;

	// if we are following someone, go to the Idle state after the attack to decide whether we still want to follow
	if ( m_isFollowing )
	{
		Idle();
	}
}

//--------------------------------------------------------------------------------------------------------------
bool CCSBot::IsAttacking( void ) const
{
	return m_isAttacking;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we are escaping from the bomb
 */
bool CCSBot::IsEscapingFromBomb( void ) const
{
	if ( m_state == static_cast<const BotState *>( &m_escapeFromBombState ) )
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we are defusing the bomb
 */
bool CCSBot::IsDefusingBomb( void ) const
{
	if ( m_state == static_cast<const BotState *>( &m_defuseBombState ) )
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we are hiding
 */
bool CCSBot::IsHiding( void ) const
{
	if ( m_state == static_cast<const BotState *>( &m_hideState ) )
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we are hiding and at our hiding spot
 */
bool CCSBot::IsAtHidingSpot( void ) const
{
	if ( !IsHiding() )
		return false;

	return m_hideState.m_isAtSpot;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we are huting
 */
bool CCSBot::IsHunting( void ) const
{
	if ( m_state == static_cast<const BotState *>( &m_huntState ) )
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we are in the MoveTo state
 */
bool CCSBot::IsMovingTo( void ) const
{
	if ( m_state == static_cast<const BotState *>( &m_moveToState ) )
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we are buying
 */
bool CCSBot::IsBuying( void ) const
{
	if ( m_state == static_cast<const BotState *>( &m_buyState ) )
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Move to potentially distant position
 */
void CCSBot::MoveTo( const Vector *pos, RouteType route )
{
	m_moveToState.m_goalPosition = *pos;
	m_moveToState.m_routeType = route;
	SetState( &m_moveToState );
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::PlantBomb( void )
{
	SetState( &m_plantBombState );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Bomb has been dropped - go get it
 */
void CCSBot::FetchBomb( void )
{
	SetState( &m_fetchBombState );
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::DefuseBomb( void )
{
	SetState( &m_defuseBombState );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Investigate recent enemy noise
 */
void CCSBot::InvestigateNoise( void )
{
	SetState( &m_investigateNoiseState );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we are busy doing something important
 */
bool CCSBot::IsBusy( void ) const
{
	if ( IsAttacking() ||
		IsBuying() ||
		IsDefusingBomb() ||
		GetTask() == PLANT_BOMB ||
		GetTask() == RESCUE_HOSTAGES ||
		IsSniping() )
	{
		return true;
	}

	return false;
}
