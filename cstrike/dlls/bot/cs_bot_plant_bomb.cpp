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

//--------------------------------------------------------------------------------------------------------------
/**
 * Plant the bomb.
 */
void PlantBombState::OnEnter( CCSBot *me )
{
	me->Crouch();
	me->SetDisposition( CCSBot::SELF_DEFENSE );

	// look at the floor
	float yaw = me->pev->v_angle.y;
	float sine = BotSIN( yaw );
	float cosine = BotCOS( yaw );

	Vector down = me->pev->origin;
	down.x += cosine * 10;
	down.y += sine * 10;
	down.z -= me->IsCrouching() ? 18.0f : 36.0f;
	me->SetLookAt( "Plant bomb on floor", &down, PRIORITY_HIGH, -1, false, 5 );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Plant the bomb.
 */
void PlantBombState::OnUpdate( CCSBot *me )
{
	// if we aren't holding the C4, grab it, otherwise plant it
	if ( me->m_pActiveItem && FStrEq( STRING( me->m_pActiveItem->pev->classname ), "weapon_c4" ) )
		me->PrimaryAttack();
	else
		me->SelectItem( "weapon_c4" );

	// if we no longer have the C4, we've successfully planted
	if ( !me->m_bHasC4 )
	{
		// move to a hiding spot and watch the bomb
		me->SetTask( CCSBot::GUARD_TICKING_BOMB );
		me->Hide( (CNavArea *)NULL, -1, 750, false );
	}

	// if we time out, it's because we slipped into a non-plantable area
	const float timeout = 5.0f;
	if ( gpGlobals->time - me->m_stateTimestamp > timeout )
		me->Idle();
}

//--------------------------------------------------------------------------------------------------------------
void PlantBombState::OnExit( CCSBot *me )
{
	// equip our rifle (in case we were interrupted while holding C4)
	me->EquipBestWeapon( false );
	me->StandUp();
	me->ResetStuckMonitor();
	me->SetDisposition( CCSBot::ENGAGE_AND_INVESTIGATE );
	me->m_lookAtSpotState = 0;
	me->m_lookAtDesc = NULL;
}

const char *PlantBombState::GetName() const
{
	return "PlantBomb";
}
