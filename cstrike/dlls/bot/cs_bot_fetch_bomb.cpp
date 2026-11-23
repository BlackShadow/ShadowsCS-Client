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
 * Move to the bomb on the floor and pick it up
 */
void FetchBombState::OnEnter( CCSBot *me )
{
	me->DestroyPath();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Move to the bomb on the floor and pick it up
 */
void FetchBombState::OnUpdate( CCSBot *me )
{
	CBaseEntity *bomb = TheCSBots()->GetLooseBomb();
	if ( bomb )
	{
		if ( !me->m_pathLength )
		{
			// build a path to the bomb
			const Vector *position = &bomb->pev->origin;
			CNavArea *area = TheNavAreaGrid.GetNavArea( position );
			if ( me->ComputePath( area, position, SAFEST_ROUTE ) == false )
			{
				me->PrintIfWatched( "Fetch bomb pathfind failed\n" );
				me->Idle();
			}
		}
	}
	else
	{
		// someone picked up the bomb
		me->PrintIfWatched( "Bomb not loose\n" );
		me->Idle();
		return;
	}

	// look around
	me->UpdateLookAround( false );

	if ( me->UpdatePathMovement() != CCSBot::PROGRESSING )
		me->Idle();
}

const char *FetchBombState::GetName() const
{
	return "FetchBomb";
}
