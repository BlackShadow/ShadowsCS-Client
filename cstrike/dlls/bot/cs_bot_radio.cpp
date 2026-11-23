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

extern int gmsgBotVoice;

//--------------------------------------------------------------------------------------------------------------
void CCSBot::StartVoiceFeedback( float duration )
{
	m_voiceStartTime = gpGlobals->time;
	m_voiceEndTime = gpGlobals->time + duration;
	CBasePlayer *recipient = NULL;
	while ( (recipient = GetNextRadioRecipient( recipient )) != NULL )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgBotVoice, NULL, recipient->edict() );
		WRITE_BYTE( 1 );
		WRITE_BYTE( entindex() );
		MESSAGE_END();
	}
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::EndVoiceFeedback( bool force )
{
	if ( force || m_voiceEndTime != 0 )
	{
		m_voiceEndTime = 0;
		MESSAGE_BEGIN( MSG_ALL, gmsgBotVoice );
		WRITE_BYTE( 0 );
		WRITE_BYTE( entindex() );
		MESSAGE_END();
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Send a radio message
 */
void CCSBot::SendRadioMessage( GameEventType event )
{
	// make sure this is a radio event
	if ( event <= EVENT_START_RADIO_1 || event >= EVENT_END_RADIO )
		return;

	PrintIfWatched( "%3.1f: SendRadioMessage( %s )\n", gpGlobals->time, GameEventName[event] );

	// note the time the message was sent
	TheCSBots()->SetRadioMessageTimestamp( event, m_iTeam );

	m_lastRadioSentTimestamp = gpGlobals->time;

	char slot[2];
	slot[1] = '\000';

	if ( event > EVENT_START_RADIO_1 && event < EVENT_START_RADIO_2 )
	{
		slot[0] = char( '1' + event - EVENT_RADIO_COVER_ME );
		ClientCommand( "radio1" );
	}
	else if ( event > EVENT_START_RADIO_2 && event < EVENT_START_RADIO_3 )
	{
		slot[0] = char( '1' + event - EVENT_RADIO_GO_GO_GO );
		ClientCommand( "radio2" );
	}
	else
	{
		slot[0] = char( '1' + event - EVENT_RADIO_AFFIRMATIVE );
		ClientCommand( "radio3" );
	}
	ClientCommand( "menuselect", slot );
	ClientCommand( "menuselect", "10" );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Returns true if the radio message is an order to do something
 * NOTE: "Report in" is not considered a "command" because it doesnt ask the bot to go somewhere, or change its mind
 */
bool CCSBot::IsRadioCommand( GameEventType event ) const
{
	if ( event == EVENT_RADIO_AFFIRMATIVE ||
		event == EVENT_RADIO_NEGATIVE ||
		event == EVENT_RADIO_ENEMY_SPOTTED ||
		event == EVENT_RADIO_SECTOR_CLEAR ||
		event == EVENT_RADIO_REPORTING_IN ||
		event == EVENT_RADIO_REPORT_IN_TEAM ||
		event == EVENT_RADIO_ENEMY_DOWN )
		return false;

	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Decide if we should move to help the player, return true if we will
 */
bool CCSBot::RespondToHelpRequest( CBasePlayer *them, Place place, float maxRange )
{
	if ( IsRogue() )
		return false;

	// if we're busy, ignore
	if ( IsBusy() )
		return false;

	// if we are too far away, ignore
	if ( maxRange > 0.0f )
	{
		// compute actual travel distance
		PathCost cost( this );
		float travelDistance = NavAreaTravelDistance( m_lastKnownArea, TheNavAreaGrid.GetNearestNavArea( &them->pev->origin ), cost );
		if ( travelDistance < 0.0f )
			return false;

		if ( travelDistance > maxRange )
			return false;
	}

	if ( place == UNDEFINED_PLACE )
	{
		// if we have no "place" identifier, go directly to them

		// if we are already there, ignore
		float rangeSq = (them->pev->origin - pev->origin).LengthSquared();
		const float close = 750.0f * 750.0f;
		if ( rangeSq < close )
			return true;

		MoveTo( &them->pev->origin, FASTEST_ROUTE );
	}
	else
	{
		// if we are already there, ignore
		if ( GetPlace() == place )
			return true;

		// go to where help is needed
		const Vector *pos = GetRandomSpotAtPlace( place );
		if ( pos )
		{
			MoveTo( pos, FASTEST_ROUTE );
		}
		else
		{
			MoveTo( &them->pev->origin, FASTEST_ROUTE );
		}
	}

	// acknowledge
	GetChatter()->Say( "OnMyWay" );

	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Respond to radio commands from HUMAN players
 */
void CCSBot::RespondToRadioCommands( void )
{
	// bots use the chatter system to respond to each other
	if ( m_radioSubject != NULL && m_radioSubject->IsPlayer() )
	{
		CBasePlayer *player = (CBasePlayer *)(CBaseEntity *)m_radioSubject;
		if ( player->IsBot() )
		{
			m_lastRadioCommand = EVENT_INVALID;
			return;
		}
	}

	if ( m_lastRadioCommand == EVENT_INVALID )
		return;

	// a human player has issued a radio command
	GetChatter()->ResetRadioSilenceDuration();

	// if we are doing something important, ignore the radio
	// unless it is a "report in" request - we can do that while we continue to do other things
	/// @todo Create "uninterruptable" flag
	if ( m_lastRadioCommand != EVENT_RADIO_REPORT_IN_TEAM )
	{
		if ( IsBusy() )
		{
			// consume command
			m_lastRadioCommand = EVENT_INVALID;
			return;
		}
	}

	// wait for reaction time before responding
	// delay needs to be long enough for the radio message we're responding to to finish
	float respondTime = 1.0f + 2.0f * GetProfile()->GetReactionTime();
	if ( IsRogue() )
		respondTime += 2.0f;

	if ( gpGlobals->time - m_lastRadioRecievedTimestamp < respondTime )
		return;

	// rogues won't follow commands, unless already following the player
	if ( !m_isFollowing && IsRogue() )
	{
		if ( IsRadioCommand( m_lastRadioCommand ) )
		{
			GetChatter()->Negative();
		}

		// consume command
		m_lastRadioCommand = EVENT_INVALID;
		return;
	}

	CBasePlayer *player = (CBasePlayer *)(CBaseEntity *)m_radioSubject;
	if ( player == NULL )
		return;

	// respond to command
	bool canDo = false;
	const float inhibitAutoFollowDuration = 60.0f;
	switch ( m_lastRadioCommand )
	{
		case EVENT_RADIO_REPORT_IN_TEAM:
		{
			GetChatter()->ReportingIn();
			break;
		}

		case EVENT_RADIO_FOLLOW_ME:
		case EVENT_RADIO_COVER_ME:
		case EVENT_RADIO_STICK_TOGETHER_TEAM:
		case EVENT_RADIO_REGROUP_TEAM:
		{
			if ( !m_isFollowing )
			{
				Follow( player );
				player->m_allowAutoFollowTime = 0;
				canDo = true;
			}
			break;
		}

		case EVENT_RADIO_ENEMY_SPOTTED:
		case EVENT_RADIO_NEED_BACKUP:
		case EVENT_RADIO_TAKING_FIRE:
			if ( !m_isFollowing )
			{
				Follow( player );
				GetChatter()->Say( "OnMyWay" );
				player->m_allowAutoFollowTime = 0;
			}
			break;

		case EVENT_RADIO_TEAM_FALL_BACK:
		{
			if ( TryToRetreat() )
				canDo = true;
			break;
		}

		case EVENT_RADIO_HOLD_THIS_POSITION:
		{
			// find the leader's area 
			SetTask( HOLD_POSITION );
			StopFollowing();
			player->m_allowAutoFollowTime = gpGlobals->time + inhibitAutoFollowDuration;
			Hide( TheNavAreaGrid.GetNearestNavArea( &m_radioPosition ), -1, 750, false );
			canDo = true;
			break;
		}

		case EVENT_RADIO_GO_GO_GO:
		case EVENT_RADIO_STORM_THE_FRONT:
			StopFollowing();
			Hunt();
			canDo = true;
			player->m_allowAutoFollowTime = gpGlobals->time + inhibitAutoFollowDuration;
			break;

		case EVENT_RADIO_GET_OUT_OF_THERE:
			if ( TheCSBots()->IsBombPlanted() )
			{
				EscapeFromBomb();
				player->m_allowAutoFollowTime = gpGlobals->time + inhibitAutoFollowDuration;
				canDo = true;
			}
			break;

		case EVENT_RADIO_SECTOR_CLEAR:
		{
			// if this is a defusal scenario, and the bomb is planted, 
			// and a human player cleared a bombsite, check it off our list too
			if ( TheCSBots()->GetScenario() == CCSBotManager::SCENARIO_DEFUSE_BOMB )
			{
				if ( m_iTeam == CT && TheCSBots()->IsBombPlanted() )
				{
					const CCSBotManager::Zone *zone = TheCSBots()->GetClosestZone( &player->pev->origin );

					if ( zone )
					{
						GetGameState()->ClearBombsite( zone->m_index );

						// if we are huting for the planted bomb, re-select bombsite
						if ( GetTask() == FIND_TICKING_BOMB )
							Idle();

						canDo = true;
					}
				}
			}
			break;
		}

		default:
			// ignore all other radio commands for now
			return;
	}

	if ( canDo )
	{
		// affirmative
		GetChatter()->Affirmative();

		// if we agreed to follow a new command, put away our grenade
		if ( IsRadioCommand( m_lastRadioCommand ) && IsUsingGrenade() )
		{
			EquipBestWeapon( false );
		}
	}

	// consume command
	m_lastRadioCommand = EVENT_INVALID;
}
