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
#pragma warning( pop )
#include "bot_profile.h"

//--------------------------------------------------------------------------------------------------------------
/**
 * Move towards currently heard noise
 */
void InvestigateNoiseState::AttendCurrentNoise( CCSBot *me )
{
	if ( me->m_noiseTimestamp <= 0 || gpGlobals->time - me->m_noiseTimestamp < me->GetProfile()->GetReactionTime() )
		return;

	// remember where the noise we heard was
	m_checkNoisePosition = me->m_noisePosition;

	// tell our teammates (unless the noise is obvious, like gunfire)
	if ( me->IsWellPastSafe() && me->HasNotSeenEnemyForLongTime() && me->m_noisePriority != PRIORITY_HIGH )
		me->GetChatter()->HeardNoise( me->m_noiseTimestamp > 0 ? &me->m_noisePosition : NULL );

	// figure out how to get to the noise
	me->PrintIfWatched( "Attending to noise...\n" );
	me->ComputePath( me->m_noiseArea, &m_checkNoisePosition, SAFEST_ROUTE );

	// consume the noise
	me->m_noiseTimestamp = 0;
}

//--------------------------------------------------------------------------------------------------------------
void InvestigateNoiseState::OnEnter( CCSBot *me )
{
	AttendCurrentNoise( me );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * @todo Use TravelDistance instead of distance...
 */
void InvestigateNoiseState::OnUpdate( CCSBot *me )
{
	float newRange;
	if ( me->ShouldInvestigateNoise( &newRange ) &&
		(m_checkNoisePosition - me->pev->origin).LengthSquared() > (newRange + 100.0f) * (newRange + 100.0f) )
	{
		// new sound is closer
		AttendCurrentNoise( me );
	}

	// if the pathfind fails, give up
	if ( !me->m_pathLength )
	{
		me->Idle();
		return;
	}

	// look around
	me->UpdateLookAround( false );

	float range = (m_checkNoisePosition - me->pev->origin).Length();
	if ( (!me->IsUsingKnife() && (range >= 1500 || me->GetFriendsRemaining() > 2)) || me->IsHurrying() )
		me->Run();
	else
		me->Walk();

	// if we can see the noise position and we're close enough to it and looking at it,
	// we don't need to actually move there (it's checked enough)
	if ( range < 200 && me->IsLookingAtPosition( &m_checkNoisePosition, 20 ) && me->IsVisible( &m_checkNoisePosition, false ) )
	{
		// can see noise position
		me->PrintIfWatched( "Noise location is clear.\n" );
		me->Idle();
		return;
	}

	// move towards noise
	if ( me->UpdatePathMovement() != CCSBot::PROGRESSING )
	{
		me->Idle();
	}
}

//--------------------------------------------------------------------------------------------------------------
void InvestigateNoiseState::OnExit( CCSBot *me )
{
	// reset to run mode in case we were sneaking about
	me->Run();
}

const char *InvestigateNoiseState::GetName() const
{
	return "InvestigateNoise";
}
