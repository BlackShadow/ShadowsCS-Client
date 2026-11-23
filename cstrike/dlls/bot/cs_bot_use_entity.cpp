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

/**
 * Face the entity and "use" it
 * NOTE: This state assumes we are standing in range of the entity to be used, with no obstructions.
 */
void UseEntityState::OnEnter( CCSBot * )
{
}

void UseEntityState::OnUpdate( CCSBot *me )
{
	// in the very rare situation where two or more bots "used" a hostage at the same time,
	// one bot will fail and needs to time out of this state
	const float useTimeout = 5.0f;
	if ( me->m_stateTimestamp - gpGlobals->time > useTimeout )
	{
		me->Idle();
		return;
	}

	// look at the entity
	Vector pos = m_entity->pev->origin + Vector( 0.0f, 0.0f, 36.0f );
	me->SetLookAt( "Use entity", &pos, PRIORITY_HIGH, -1.0f, false, 5.0f );

	// if we are looking at the entity, "use" it and exit
	if ( me->IsLookingAtPosition( &pos, 20.0f ) )
	{
		if ( TheCSBots()->GetScenario() == CCSBotManager::SCENARIO_RESCUE_HOSTAGES &&
			me->m_iTeam == CT &&
			me->GetTask() == CCSBot::COLLECT_HOSTAGES )
		{
			// we are collecting a hostage, assume we were successful - the update check will correct us if we weren't
			++me->m_hostageEscortCount;
		}

		me->UseEnvironment();
		me->Idle();
	}
}

void UseEntityState::OnExit( CCSBot *me )
{
	me->m_lookAtSpotState = 0;
	me->m_lookAtDesc = NULL;
	me->ResetStuckMonitor();
}

const char *UseEntityState::GetName() const
{
	return "UseEntity";
}
