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
#include "cs_bot_manager.h"

//--------------------------------------------------------------------------------------------------------------
/**
 * Begin defusing the bomb
 */
void DefuseBombState::OnEnter( CCSBot *me )
{
	me->Crouch();
	me->SetDisposition( CCSBot::SELF_DEFENSE );
	me->GetChatter()->Say( "DefusingBomb" );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Defuse the bomb
 */
void DefuseBombState::OnUpdate( CCSBot *me )
{
	const Vector *bombPos = me->GetGameState()->GetBombPosition();

	if ( bombPos == NULL )
	{
		me->PrintIfWatched( "In Defuse state, but don't know where the bomb is!\n" );
		me->Idle();
		return;
	}

	// look at the bomb
	me->SetLookAt( "Defuse bomb", bombPos, PRIORITY_HIGH, -1, false, 5 );

	// defuse...
	me->UseEnvironment();

	if ( gpGlobals->time - me->m_stateTimestamp > 1 )
	{
		// if we missed starting the defuse, give up
		if ( TheCSBots()->GetBombDefuser() == NULL )
		{
			me->PrintIfWatched( "Failed to start defuse, giving up\n" );
			me->Idle();
			return;
		}
		else if ( TheCSBots()->GetBombDefuser() != me )
		{
			// if someone else got the defuse, give up
			me->PrintIfWatched( "Someone else started defusing, givin up\n" );
			me->Idle();
			return;
		}
	}

	// if bomb has been defused, give up
	if ( !TheCSBots()->IsBombPlanted() )
	{
		me->Idle();
		return;
	}
}

//--------------------------------------------------------------------------------------------------------------
void DefuseBombState::OnExit( CCSBot *me )
{
	me->StandUp();
	me->ResetStuckMonitor();
	me->SetTask( CCSBot::SEEK_AND_DESTROY );
	me->SetDisposition( CCSBot::ENGAGE_AND_INVESTIGATE );
	me->m_lookAtSpotState = 0;
	me->m_lookAtDesc = NULL;
}

const char *DefuseBombState::GetName() const
{
	return "DefuseBomb";
}
