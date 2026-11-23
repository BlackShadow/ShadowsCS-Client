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
#include "player.h"
#include "bot_util.h"
#include "cs_bot_chatter.h"
#include "cs_bot.h"
#include "cs_bot_manager.h"
#include "nav_area.h"
#include <algorithm>

extern char *MP_COM_Parse( char *data );
extern char *MP_COM_GetToken( void );

namespace
{
	const CountCriteria UNDEFINED_COUNT = 0xFFFF;
	const CountCriteria COUNT_MANY = 4;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * @todo Fix this
 */
const Vector *GetRandomSpotAtPlace( Place place )
{
	int count = 0;
	for ( NavAreaList::const_iterator it = TheNavAreaList.begin(); it != TheNavAreaList.end(); ++it )
	{
		if ( (*it)->GetPlace() == place )
			++count;
	}
	if ( count )
	{
		int selection = RANDOM_LONG( 0, count - 1 );
		for ( NavAreaList::const_iterator it = TheNavAreaList.begin(); it != TheNavAreaList.end(); ++it )
		{
			if ( (*it)->GetPlace() == place && selection == 0 )
				return (*it)->GetCenter();
		}
	}
	return NULL;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Transmit meme to other bots
 */
void BotMeme::Transmit( CCSBot *sender ) const
{
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *player = static_cast<CBasePlayer *>(UTIL_PlayerByIndex( i ));

		if ( player == NULL )
			continue;

		if ( FNullEnt( player->pev ) )
			continue;

		if ( STRING( player->pev->netname )[0] == '\0' )
			continue;

		// skip self
		if ( player == sender )
			continue;

		// ignore dead humans
		if ( !player->IsBot() && !player->IsAlive() )
			continue;

		// ignore enemies, since we can't hear them talk
		if ( player->m_iTeam != sender->m_iTeam )
			continue;

		// if not a bot, fail the test
		if ( !player->IsBot() )
			continue;

		// allow bot to interpret our meme
		Interpret( sender, static_cast<CCSBot *>(player) );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * A teammate called for help - respond
 */
void BotHelpMeme::Interpret( CCSBot *sender, CCSBot *receiver ) const
{
	const float maxHelpRange = 3000.0f;		// 2000
	receiver->RespondToHelpRequest( sender, m_place, maxHelpRange );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * A teammate reported information about a bombsite
 */
void BotBombsiteStatusMeme::Interpret( CCSBot *, CCSBot *receiver ) const
{
	// remember this bombsite's status
	if ( m_status == 0 )
		receiver->GetGameState()->ClearBombsite( m_zoneIndex );
	else
		receiver->GetGameState()->MarkBombsiteAsPlanted( m_zoneIndex );

	// if we were heading to the just-cleared bombsite, pick another one to search
	// if our target bombsite wasn't cleared, will will continue going to it,
	// because GetNextBombsiteToSearch() will return the same zone (since its not cleared)
	// if the bomb was planted, we will head to that bombsite
	if ( receiver->GetTask() == CCSBot::FIND_TICKING_BOMB )
	{
		receiver->Idle();
		receiver->GetChatter()->Affirmative();
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * A teammate reported information about the bomb
 */
void BotBombStatusMeme::Interpret( CCSBot *sender, CCSBot *receiver ) const
{
	// update our gamestate based on teammate's report
	if ( m_state == CSGameState::LOOSE )
	{
		receiver->GetGameState()->UpdateLooseBomb( &m_pos );
		if ( receiver->GetTask() == CCSBot::GUARD_BOMB_ZONE )
		{
			receiver->Idle();
			receiver->GetChatter()->Affirmative();
		}
	}
	else if ( m_state == CSGameState::MOVING )
	{
		receiver->GetGameState()->UpdateBomber( &m_pos );
		// if we are hunting and see no enemies, respond
		if ( !receiver->IsRogue() && receiver->IsHunting() &&
			receiver->GetNearbyEnemyCount() == 0 )
			receiver->RespondToHelpRequest( sender, TheNavAreaGrid.GetPlace( &m_pos ), -1.0f );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * A teammate has asked that we follow him
 */
void BotFollowMeme::Interpret( CCSBot *sender, CCSBot *receiver ) const
{
	if ( receiver->IsRogue() )
		return;

	// if we're busy, ignore
	if ( receiver->IsBusy() )
		return;

	// if we are too far away, ignore
	// compute actual travel distance
	PathCost cost( receiver );
	CNavArea *area = TheNavAreaGrid.GetNearestNavArea( &sender->pev->origin );
	float travelDistance = NavAreaTravelDistance( receiver->GetLastKnownArea(), area, cost );
	if ( travelDistance >= 0.0f && travelDistance <= 1000.0f )
	{
		// begin following
		receiver->Follow( sender );

		// acknowledge
		receiver->GetChatter()->Say( "CoveringFriend" );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * A teammate has asked us to defend a place
 */
void BotDefendHereMeme::Interpret( CCSBot *, CCSBot *receiver ) const
{
	if ( receiver->IsRogue() )
		return;

	// if we're busy, ignore
	if ( receiver->IsBusy() )
		return;

	Place place = TheNavAreaGrid.GetPlace( &m_pos );
	const Vector *spot = NULL;
	if ( place )
		spot = FindRandomHidingSpot( receiver, place, receiver->IsSniper() );
	if ( spot )
	{
		receiver->SetTask( CCSBot::HOLD_POSITION );
		receiver->Hide( spot, -1.0f, false );
	}
	else
	{
		// hide nearby
		receiver->SetTask( CCSBot::HOLD_POSITION );
		CNavArea *area = TheNavAreaGrid.GetNearestNavArea( &m_pos );
		receiver->Hide( area, -1.0f, 750.0f, false );

		// acknowledge
		receiver->GetChatter()->Say( "Affirmative" );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * A teammate has asked where the bomb is planted
 */
void BotWhereBombMeme::Interpret( CCSBot *, CCSBot *receiver ) const
{
	int zone = receiver->GetGameState()->GetPlantedBombsite();
	if ( zone != CSGameState::UNKNOWN )
		receiver->GetChatter()->FoundPlantedBomb( zone );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * A teammate has asked us to report in
 */
void BotRequestReportMeme::Interpret( CCSBot *, CCSBot *receiver ) const
{
	receiver->GetChatter()->ReportingIn();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * A teammate told us all the hostages are gone
 */
void BotAllHostagesGoneMeme::Interpret( CCSBot *, CCSBot *receiver ) const
{
	receiver->GetGameState()->AllHostagesGone();

	// acknowledge
	receiver->GetChatter()->Say( "Affirmative" );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * A teammate told us a CT is talking to a hostage
 */
void BotHostageBeingTakenMeme::Interpret( CCSBot *, CCSBot *receiver ) const
{
	receiver->GetGameState()->SetHostageTaken( true );

	// if we're busy, ignore
	if ( receiver->IsBusy() )
		return;

	receiver->Idle();

	// acknowledge
	receiver->GetChatter()->Say( "Affirmative" );
}

//--------------------------------------------------------------------------------------------------------------
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
BotSpeakable::BotSpeakable( void )
{
	m_phrase = NULL;
}

//--------------------------------------------------------------------------------------------------------------
BotSpeakable::~BotSpeakable()
{
	delete [] m_phrase;
}

//--------------------------------------------------------------------------------------------------------------
BotPhrase::BotPhrase( unsigned int id, bool isPlace )
{
	m_id = id;
	m_radioEvent = EVENT_INVALID;
	m_isImportant = false;
	m_placeCriteria = ANY_PLACE;
	m_countCriteria = UNDEFINED_COUNT;
	m_numVoiceBanks = 0;
	m_isPlace = isPlace;
	InitVoiceBank( 0 );
}

//--------------------------------------------------------------------------------------------------------------
BotPhrase::~BotPhrase()
{
	for ( size_t bank = 0; bank < m_voiceBank.size(); ++bank )
	{
		BotVoiceBank *voiceBank = m_voiceBank[bank];
		for ( size_t i = 0; i < voiceBank->size(); ++i )
			delete (*voiceBank)[i];
		delete voiceBank;
	}
}

//--------------------------------------------------------------------------------------------------------------
void BotPhrase::InitVoiceBank( int bankIndex )
{
	while ( bankIndex >= m_numVoiceBanks )
	{
		m_count.push_back( 0 );
		m_index.push_back( 0 );
		m_voiceBank.push_back( new BotVoiceBank );
		++m_numVoiceBanks;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return a random speakable - avoid repeating
 */
const char *BotPhrase::GetSpeakable( int bankIndex, float *duration ) const
{
	if ( bankIndex >= 0 && bankIndex < m_numVoiceBanks && m_count[bankIndex] != 0 )
	{
		// find phrase that meets the current criteria
		int first = m_index[bankIndex];
		do
		{
			const BotSpeakable *speakable = (*m_voiceBank[bankIndex])[m_index[bankIndex]++];
			if ( m_index[bankIndex] >= m_count[bankIndex] )
				m_index[bankIndex] = 0;

			// check place criteria
			// if this speakable has a place criteria, it must match to be used
			// speakables with Place of ANY will match any place
			// speakables with a specific Place will only be used if Place matches
			// speakables with Place of UNDEFINED only match Place of UNDEFINED
			if ( speakable->m_place != ANY_PLACE && speakable->m_place != m_placeCriteria )
				continue;

			// check count criteria
			// if this speakable has a count criteria, it must match to be used
			// if this speakable does not have a count criteria, we dont care what the count is set to
			if ( speakable->m_count != UNDEFINED_COUNT && speakable->m_count != std::min( m_countCriteria, COUNT_MANY ) )
				continue;

			if ( duration )
				*duration = speakable->m_duration;
			return speakable->m_phrase;
		}
		while ( m_index[bankIndex] != first );
	}

	if ( duration )
		*duration = 0.0f;
	return NULL;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Randomly shuffle the speakable order
 */
void BotPhrase::Randomize( void )
{
	for ( size_t bank = 0; bank < m_voiceBank.size(); ++bank )
	{
		BotVoiceBank &voiceBank = *m_voiceBank[bank];
		for ( size_t i = 1; i < voiceBank.size(); ++i )
		{
			size_t other = static_cast<size_t>(rand()) % (i + 1);
			if ( other != i )
				std::swap( voiceBank[i], voiceBank[other] );
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
BotPhraseManager *TheBotPhrases = NULL;

//--------------------------------------------------------------------------------------------------------------
BotPhraseManager::BotPhraseManager( void )
{
	m_placeCount = 0;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Invoked when map changes
 */
void BotPhraseManager::OnMapChange( void )
{
	m_placeCount = 0;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Invoked when the round resets
 */
void BotPhraseManager::OnRoundRestart( void )
{
	// effectively reset all interval timers
	m_placeCount = 0;

	// shuffle all the speakables
	for ( BotPhraseList::iterator it = m_placeList.begin(); it != m_placeList.end(); ++it )
		(*it)->Randomize();
	for ( BotPhraseList::iterator it = m_list.begin(); it != m_list.end(); ++it )
		(*it)->Randomize();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Initialize phrase system from database file
 */
bool BotPhraseManager::Initialize( const char *filename, int bankIndex )
{
	int length;
	char *file = reinterpret_cast<char *>(g_engfuncs.pfnLoadFileForMe( const_cast<char *>(filename), &length ));
	if ( file == NULL )
	{
		if ( UTIL_IsGame( "czero" ) )
			CONSOLE_ECHO( "WARNING: Cannot access bot phrase database '%s'\n", filename );
		return false;
	}

	char baseDirectory[128] = {};
	unsigned int nextID = 1;
	char *data = file;
	while ( (data = MP_COM_Parse( data )) != NULL )
	{
		const char *token = MP_COM_GetToken();
		if ( stricmp( token, "BaseDir" ) == 0 )
		{
			data = MP_COM_Parse( data );
			if ( data == NULL )
			{
				CONSOLE_ECHO( "Error parsing '%s' - expected identifier\n", filename );
				g_engfuncs.pfnFreeFile( file );
				return false;
			}
			strncpy( baseDirectory, MP_COM_GetToken(), sizeof(baseDirectory) );
			baseDirectory[sizeof(baseDirectory) - 1] = '\0';
			continue;
		}

		bool isPlace = stricmp( token, "Place" ) == 0;
		if ( !isPlace && stricmp( token, "Chatter" ) != 0 )
			continue;

		// encountered a new phrase collection
		BotPhrase *phrase;
		if ( bankIndex == 0 )
			phrase = new BotPhrase( nextID++, isPlace );
		else
			phrase = NULL;

		// get name of this phrase
		data = MP_COM_Parse( data );
		if ( data == NULL )
		{
			CONSOLE_ECHO( "Error parsing '%s' - expected identifier\n", filename );
			g_engfuncs.pfnFreeFile( file );
			return false;
		}
		token = MP_COM_GetToken();
		if ( bankIndex != 0 )
		{
			phrase = const_cast<BotPhrase *>(isPlace ? GetPlace( token ) : GetPhrase( token ));
			if ( phrase == NULL )
			{
				CONSOLE_ECHO( "Error parsing '%s' - phrase '%s' is invalid\n", filename, token );
				g_engfuncs.pfnFreeFile( file );
				return false;
			}
		}
		else
		{
			phrase->m_name = new char[token ? strlen( token ) + 1 : 1];
			if ( token )
				strcpy( phrase->m_name, token );
			else
				phrase->m_name[0] = '\0';
		}

		phrase->InitVoiceBank( bankIndex );
		PlaceCriteria place = ANY_PLACE;
		CountCriteria count = UNDEFINED_COUNT;
		GameEventType radioEvent = EVENT_INVALID;
		bool isImportant = false;

		// read attributes of this phrase
		for (;;)
		{
			// get next token
			data = MP_COM_Parse( data );
			if ( data == NULL )
			{
				CONSOLE_ECHO( "Error parsing %s - expected 'End'\n", filename );
				g_engfuncs.pfnFreeFile( file );
				return false;
			}
			token = MP_COM_GetToken();
			if ( token == NULL )
			{
				g_engfuncs.pfnFreeFile( file );
				return false;
			}

			// check for End delimiter
			if ( stricmp( token, "End" ) == 0 )
				break;

			// check for "important" flag
			if ( stricmp( token, "Important" ) == 0 )
			{
				isImportant = true;
				continue;
			}

			if ( stricmp( token, "Place" ) == 0 || stricmp( token, "Count" ) == 0 || stricmp( token, "Radio" ) == 0 )
			{
				bool isPlaceCriterion = stricmp( token, "Place" ) == 0;
				bool isCountCriterion = stricmp( token, "Count" ) == 0;
				data = MP_COM_Parse( data );
				if ( data == NULL )
				{
					if ( isPlaceCriterion )
						CONSOLE_ECHO( "Error parsing %s - expected Place name\n", filename );
					else if ( isCountCriterion )
						CONSOLE_ECHO( "Error parsing %s - expected Count value\n", filename );
					else
						CONSOLE_ECHO( "Error parsing %s - expected radio event\n", filename );
					g_engfuncs.pfnFreeFile( file );
					return false;
				}
				token = MP_COM_GetToken();
				if ( isPlaceCriterion )
				{
					if ( stricmp( token, "ANY" ) == 0 )
						place = ANY_PLACE;
					else if ( stricmp( token, "UNDEFINED" ) == 0 )
						place = UNDEFINED_PLACE;
					else
						place = TheBotPhrases->NameToID( token );
				}
				else if ( isCountCriterion )
					count = stricmp( token, "Many" ) == 0 ? COUNT_MANY : atoi( token );
				else
				{
					radioEvent = NameToGameEvent( token );
					if ( radioEvent < EVENT_RADIO_COVER_ME || radioEvent > EVENT_RADIO_ENEMY_DOWN )
					{
						CONSOLE_ECHO( "Error parsing %s - invalid radio event '%s'\n", filename, token );
						g_engfuncs.pfnFreeFile( file );
						return false;
					}
				}
				continue;
			}

			// found a phrase - add it to the collection
			BotSpeakable *speakable = new BotSpeakable;
			char path[128];
			const char *name = token;
			if ( baseDirectory[0] )
			{
				snprintf( path, sizeof(path), "%s%s", baseDirectory, token );
				name = path;
			}
			speakable->m_phrase = new char[name ? strlen( name ) + 1 : 1];
			if ( name )
				strcpy( speakable->m_phrase, name );
			else
				speakable->m_phrase[0] = '\0';
			speakable->m_place = place;
			speakable->m_count = count;
			snprintf( path, sizeof(path), "sound\\%s", speakable->m_phrase );
			speakable->m_duration = static_cast<float>(g_engfuncs.pfnGetApproxWavePlayLen( path ) / 1000.0);
			if ( speakable->m_duration <= 0.0f )
			{
				CONSOLE_ECHO( "Warning: Couldn't get duration of phrase '%s'\n", path );
				speakable->m_duration = 1.0f;
			}
			phrase->m_voiceBank[bankIndex]->push_back( speakable );
			++phrase->m_count[bankIndex];
		}
		if ( bankIndex == 0 )
		{
			phrase->m_radioEvent = radioEvent;
			phrase->m_isImportant = isImportant;
		}

		// add phrase collection to the appropriate master list
		if ( isPlace )
			m_placeList.push_back( phrase );
		else
			m_list.push_back( phrase );
	}
	g_engfuncs.pfnFreeFile( file );
	return true;
}

//--------------------------------------------------------------------------------------------------------------
BotPhraseManager::~BotPhraseManager()
{
	// free phrase resources
	for ( BotPhraseList::iterator it = m_list.begin(); it != m_list.end(); ++it )
		delete *it;
	for ( BotPhraseList::iterator it = m_placeList.begin(); it != m_placeList.end(); ++it )
		delete *it;
	m_list.clear();
	m_placeList.clear();
}

//--------------------------------------------------------------------------------------------------------------
unsigned int BotPhraseManager::NameToID( const char *name ) const
{
	for ( BotPhraseList::const_iterator it = m_placeList.begin(); it != m_placeList.end(); ++it )
	{
		if ( stricmp( (*it)->GetName(), name ) == 0 )
			return (*it)->GetID();
	}
	for ( BotPhraseList::const_iterator it = m_list.begin(); it != m_list.end(); ++it )
	{
		if ( stricmp( (*it)->GetName(), name ) == 0 )
			return (*it)->GetID();
	}
	return 0;
}

//--------------------------------------------------------------------------------------------------------------
const char *BotPhraseManager::IDToName( unsigned int id ) const
{
	for ( BotPhraseList::const_iterator it = m_placeList.begin(); it != m_placeList.end(); ++it )
	{
		if ( (*it)->GetID() == id )
			return (*it)->GetName();
	}
	for ( BotPhraseList::const_iterator it = m_list.begin(); it != m_list.end(); ++it )
	{
		if ( (*it)->GetID() == id )
			return (*it)->GetName();
	}
	return NULL;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Given a name, return the associated phrase collection
 */
const BotPhrase *BotPhraseManager::GetPhrase( const char *name ) const
{
	for ( BotPhraseList::const_iterator it = m_list.begin(); it != m_list.end(); ++it )
	{
		if ( stricmp( (*it)->GetName(), name ) == 0 )
			return *it;
	}
	return NULL;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Given a name, return the associated Place phrase collection
 */
const BotPhrase *BotPhraseManager::GetPlace( const char *name ) const
{
	if ( name == NULL )
		return NULL;
	for ( BotPhraseList::const_iterator it = m_placeList.begin(); it != m_placeList.end(); ++it )
	{
		if ( stricmp( (*it)->GetName(), name ) == 0 )
			return *it;
	}
	return NULL;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Given a name, return the associated Place phrase collection
 */
const BotPhrase *BotPhraseManager::GetPlace( unsigned int id ) const
{
	if ( id == 0 )
		return NULL;
	for ( BotPhraseList::const_iterator it = m_placeList.begin(); it != m_placeList.end(); ++it )
	{
		if ( (*it)->GetID() == id )
			return *it;
	}
	return NULL;
}

//--------------------------------------------------------------------------------------------------------------
int BotPhraseManager::FindPlaceIndex( Place where ) const
{
	for ( int i = 0; i < m_placeCount; ++i )
	{
		if ( m_placeStatementHistory[i].placeID == where )
			return i;
	}
	if ( m_placeCount >= 64 )
		return -1;

	int index = m_placeCount;
	m_placeStatementHistory[index].placeID = where;
	m_placeStatementHistory[index].timer.Invalidate();
	++m_placeCount;
	return index;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return time last statement of given type was emitted by a teammate for the given place
 */
float BotPhraseManager::GetPlaceStatementInterval( Place where ) const
{
	int index = FindPlaceIndex( where );
	if ( index < 0 || index >= m_placeCount )
		return 99999.9f;
	return m_placeStatementHistory[index].timer.GetElapsedTime();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Set time of last statement of given type was emitted by a teammate for the given place
 */
void BotPhraseManager::ResetPlaceStatementInterval( Place where )
{
	int index = FindPlaceIndex( where );
	if ( index >= 0 && index < m_placeCount )
		m_placeStatementHistory[index].timer.Start();
}

//--------------------------------------------------------------------------------------------------------------
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
BotStatement::BotStatement( BotChatterInterface *chatter, BotStatementType type, float expireDuration )
{
	m_chatter = chatter;
	m_next = NULL;
	m_prev = NULL;
	m_type = type;
	m_subject = -1;
	m_place = UNDEFINED_PLACE;
	m_meme = NULL;
	m_timestamp = gpGlobals->time;
	m_startTime = gpGlobals->time;
	m_expireTime = gpGlobals->time + expireDuration;
	m_speakTimestamp = 0.0f;
	m_isSpeaking = false;
	m_nextTime = 0.0f;
	m_index = -1;
	m_count = 0;
	m_conditionCount = 0;
}

//--------------------------------------------------------------------------------------------------------------
BotStatement::~BotStatement()
{
	if ( m_meme )
		::operator delete( m_meme );
}

//--------------------------------------------------------------------------------------------------------------
CCSBot *BotStatement::GetOwner( void ) const
{
	return m_chatter->GetOwner();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Attach a meme to this statement, to be transmitted to other friendly bots when spoken
 */
void BotStatement::AttachMeme( BotMeme *meme )
{
	m_meme = meme;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Add a conditions that must be true for the statement to be spoken
 */
void BotStatement::AddCondition( ConditionType condition )
{
	if ( m_conditionCount < 4 )
		m_condition[m_conditionCount++] = condition;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if this statement is "important" and not personality chatter
 */
bool BotStatement::IsImportant( void ) const
{
	// if a statement contains any important phrases, it is important
	for ( int i = 0; i < m_count; ++i )
	{
		if ( m_statement[i].isPhrase && m_statement[i].phrase->IsImportant() )
			return true;

		// hack for now - phrases with enemy counts are important
		if ( !m_statement[i].isPhrase && m_statement[i].context == CURRENT_ENEMY_COUNT )
			return true;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Verify all attached conditions
 */
bool BotStatement::IsValid( void ) const
{
	for ( int i = 0; i < m_conditionCount; ++i )
	{
		if ( m_condition[i] == IS_IN_COMBAT && !GetOwner()->IsAttacking() )
			return false;
		if ( m_condition[i] == ENEMIES_REMAINING && GetOwner()->GetEnemiesRemaining() == 0 )
			return false;
	}
	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if this statement is essentially the same as the given one
 */
bool BotStatement::IsRedundant( const BotStatement *say ) const
{
	// special cases
	if ( m_type == REPORT_CRITICAL_EVENT || m_type == REPORT_REQUEST_HELP ||
		m_type == REPORT_MY_PLAN || m_type == REPORT_ACKNOWLEDGE )
		return false;

	// check if topics are different
	if ( m_type != say->m_type )
		return false;

	Place place = GetPlace();
	Place otherPlace = say->GetPlace();
	if ( place && otherPlace && place == otherPlace )
	{
		// talking about the same place
		return true;
	}

	if ( !place && !otherPlace && m_subject == -1 && say->m_subject == -1 )
	{
		// neither has place or subject, so they are the same
		return true;
	}

	return m_subject != -1 && say->m_subject != -1 && m_subject == say->m_subject;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if this statement is no longer appropriate to say
 */
bool BotStatement::IsObsolete( void ) const
{
	// if the round is over, the only things we should say are emotes
	if ( GetOwner()->GetGameState()->IsRoundOver() )
	{
		if ( m_type != REPORT_EMOTE )
			return true;
	}

	// check if statement lifetime has expired
	return gpGlobals->time > m_expireTime;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Possibly change what were going to say base on what teammate is saying
 */
void BotStatement::Convert( const BotStatement *say )
{
	if ( m_type == REPORT_MY_PLAN && say->m_type == REPORT_MY_PLAN )
	{
		static const BotPhrase *meToo = TheBotPhrases->GetPhrase( "AgreeWithPlan" );

		// don't reconvert
		if ( m_statement[0].phrase == meToo )
			return;

		// if our plans are the same, change our statement to "me too"
		if ( m_statement[0].phrase == say->m_statement[0].phrase )
		{
			float time = gpGlobals->time;
			if ( m_place == say->m_place )
			{
				// same plan at the same place - convert to "me too"
				m_statement[0].phrase = meToo;
				m_startTime = time + RANDOM_FLOAT( 0.5f, 1.0f );
			}
			else
			{
				// same plan at different place - wait a bit to allow others to respond "me too"
				m_startTime = time + RANDOM_FLOAT( 3.0f, 4.0f );
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
void BotStatement::AppendPhrase( const BotPhrase *phrase )
{
	if ( phrase && m_count < 4 )
	{
		m_statement[m_count].isPhrase = true;
		m_statement[m_count].phrase = phrase;
		++m_count;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Special phrases that depend on the context
 */
void BotStatement::AppendPhrase( ContextType contextPhrase )
{
	if ( m_count < 4 )
	{
		m_statement[m_count].isPhrase = false;
		m_statement[m_count].context = contextPhrase;
		++m_count;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Say our statement
 */
bool BotStatement::Update( void )
{
	CCSBot *bot = GetOwner();

	// if all of our teammates are dead, the only non-redundant statements are emotes
	if ( bot->GetFriendsRemaining() == 0 && m_type != REPORT_EMOTE )
		return false;
	if ( !m_isSpeaking )
	{
		m_isSpeaking = true;
		m_speakTimestamp = gpGlobals->time;
	}

	// special case - context dependent delay
	if ( m_index >= 0 && m_statement[m_index].context == ACCUMULATE_ENEMIES_DELAY )
	{
		// report if we see a lot of enemies, or if enough time has passed
		int count = bot->GetNearbyEnemyCount();
		if ( count > 3 || gpGlobals->time - m_speakTimestamp > 2.0f )
		{
			// enough enemies have accumulated to expire this delay
			m_nextTime = 0.0f;
		}
	}
	if ( gpGlobals->time <= m_nextTime )
		return true;

	// check for end of statement
	if ( ++m_index == m_count )
	{
		// transmit any memes carried in this statement to our teammates
		if ( m_meme )
			m_meme->Transmit( bot );
		return false;
	}

	// start next part of statement
	const BotPhrase *phrase = NULL;
	if ( m_statement[m_index].isPhrase )
	{
		// normal phrase
		phrase = m_statement[m_index].phrase;
	}
	else
	{
		// context-dependant phrase
		switch ( m_statement[m_index].context )
		{
		case CURRENT_ENEMY_COUNT:
		{
			int enemyCount = bot->GetNearbyEnemyCount();
			int friends = bot->GetNearbyFriendCount();

			// if we are outnumbered, ask for help
			if ( enemyCount - 1 > friends )
			{
				phrase = TheBotPhrases->GetPhrase( "Help" );
				m_meme = new BotHelpMeme( UNDEFINED_PLACE );
			}
			else if ( enemyCount > 1 )
			{
				phrase = TheBotPhrases->GetPhrase( "EnemySpotted" );
				phrase->SetCountCriteria( enemyCount );
			}
			break;
		}
		case REMAINING_ENEMY_COUNT:
		{
			unsigned int remaining = bot->GetEnemiesRemaining();
			if ( remaining <= 3 )
			{
				static const char *speak[] = { "NoEnemiesLeft", "OneEnemyLeft", "TwoEnemiesLeft", "ThreeEnemiesLeft" };
				phrase = TheBotPhrases->GetPhrase( speak[remaining] );
			}
			break;
		}
		case SHORT_DELAY:
		{
			float time = gpGlobals->time;
			m_nextTime = time + RANDOM_FLOAT( 0.1f, 0.5f );
			return true;
		}
		case LONG_DELAY:
		{
			float time = gpGlobals->time;
			m_nextTime = time + RANDOM_FLOAT( 1.0f, 2.0f );
			return true;
		}
		case ACCUMULATE_ENEMIES_DELAY:
			// wait until test becomes true
			m_nextTime = 100000000.0f;
			return true;
		}
	}
	if ( phrase == NULL )
	{
		// skip directly to the next phrase
		m_nextTime = 0.0f;
		return true;
	}

	float duration;

	// if chatter system is in "standard radio" mode, send the equivalent radio command
	if ( bot->GetChatter()->GetVerbosity() == BotChatterInterface::RADIO )
	{
		if ( phrase->GetRadioEquivalent() != EVENT_INVALID )
		{
			// use the standard radio
			bot->GetChatter()->ResetRadioSilenceDuration();
			bot->SendRadioMessage( phrase->GetRadioEquivalent() );
			duration = 2.0f;
		}
		else
		{
			// skip directly to the next phrase
			m_nextTime = 0.0f;
			return true;
		}
	}
	else
	{
		// set place criteria
		phrase->SetPlaceCriteria( m_place );
		const BotProfile *profile = bot->GetProfile();
		const char *speakable = phrase->GetSpeakable( profile->GetVoiceBank(), &duration );
		if ( phrase->IsPlace() )
		{
			// don't repeat the place if someone just mentioned it not too long ago
			if ( TheBotPhrases->GetPlaceStatementInterval( phrase->GetID() ) < 20.0f )
			{
				m_nextTime = gpGlobals->time + duration + 0.1f;
				return true;
			}
			TheBotPhrases->ResetPlaceStatementInterval( phrase->GetID() );
		}
		if ( speakable )
		{
			bot->Radio( speakable, NULL, static_cast<short>(profile->GetVoicePitch()), false );
			bot->GetChatter()->ResetRadioSilenceDuration();
			bot->StartVoiceFeedback( duration + 1.0f );
		}
		else if ( phrase->GetRadioEquivalent() != EVENT_INVALID )
		{
			// use the standard radio
			bot->SendRadioMessage( phrase->GetRadioEquivalent() );
			bot->GetChatter()->ResetRadioSilenceDuration();
			duration = 2.0f;
		}
		else
		{
			// skip directly to the next phrase
			m_nextTime = 0.0f;
		}
	}
	m_nextTime = gpGlobals->time + duration + 0.1f;
	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * If this statement refers to a specific place, return that place
 * Places can be implicit in the statement, or explicitly defined
 */
Place BotStatement::GetPlace( void ) const
{
	// return any explicitly set place if we have one
	if ( m_place )
		return m_place;
	// look for an implicit place in our statement
	for ( int i = 0; i < m_count; ++i )
	{
		if ( m_statement[i].isPhrase && m_statement[i].phrase->IsPlace() )
			return m_statement[i].phrase->GetID();
	}
	return UNDEFINED_PLACE;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if this statement has an associated count
 */
bool BotStatement::HasCount( void ) const
{
	for ( int i = 0; i < m_count; ++i )
	{
		if ( !m_statement[i].isPhrase && m_statement[i].context == CURRENT_ENEMY_COUNT )
			return true;
	}
	return false;
}

//--------------------------------------------------------------------------------------------------------------
CountdownTimer BotChatterInterface::m_encourageTimer;
IntervalTimer BotChatterInterface::m_radioSilenceInterval[2];

enum PitchHack
{
	P_HI,
	P_NORMAL,
	P_LOW
};

static int nextPitch = P_HI;

//--------------------------------------------------------------------------------------------------------------
BotChatterInterface::BotChatterInterface( CCSBot *me )
{
	m_me = me;
	m_statementList = NULL;
	switch ( nextPitch )
	{
	case P_HI:
		m_pitch = RANDOM_LONG( 105, 110 );
		break;
	case P_NORMAL:
		m_pitch = RANDOM_LONG( 95, 105 );
		break;
	case P_LOW:
		m_pitch = RANDOM_LONG( 85, 95 );
		break;
	}
	nextPitch = (nextPitch + 1) % 3;
	Reset();
}

//--------------------------------------------------------------------------------------------------------------
BotChatterInterface::~BotChatterInterface()
{
	// free pending statements
	BotStatement *statement = m_statementList;
	while ( statement )
	{
		BotStatement *next = statement->m_next;
		delete statement;
		statement = next;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Reset to initial state
 */
void BotChatterInterface::Reset( void )
{
	// removing pending statements - except for those about the round results
	BotStatement *statement = m_statementList;
	while ( statement )
	{
		BotStatement *next = statement->m_next;
		if ( statement->m_type != REPORT_ROUND_END )
			RemoveStatement( statement );
		statement = next;
	}
	m_seeAtLeastOneEnemy = false;
	m_timeWhenSawFirstEnemy = 0.0f;
	m_reportedEnemies = false;
	m_requestedBombLocation = false;
	m_encourageTimer.Invalidate();
	m_needBackupInterval.Invalidate();
	m_spottedBomberInterval.Invalidate();
	m_spottedLooseBombTimer.Invalidate();
	m_heardNoiseTimer.Invalidate();
	m_scaredInterval.Invalidate();
	m_planInterval.Invalidate();
	m_escortingHostageTimer.Invalidate();
	ResetRadioSilenceDuration();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Register a statement for speaking
 */
void BotChatterInterface::AddStatement( BotStatement *statement, bool mustAdd )
{
	// don't add statements if bot chatter is shut off
	if ( GetVerbosity() == OFF )
		return;

	// if we only want mission-critical radio chatter, ignore non-important phrases
	if ( GetVerbosity() == MINIMAL && !statement->IsImportant() )
		return;

	// don't add statements if we're dead
	if ( !m_me->IsAlive() && !mustAdd )
		return;

	// don't add empty statements
	if ( statement->m_count == 0 )
	{
		delete statement;
		return;
	}

	// don't add statements that are redundant with something we're already waiting to say
	for ( BotStatement *other = m_statementList; other; other = other->m_next )
	{
		if ( statement->IsRedundant( other ) )
		{
			m_me->PrintIfWatched( "I tried to say something I'm already saying.\n" );
			delete statement;
			return;
		}
	}

	BotStatement *prev = NULL;
	BotStatement *next = m_statementList;
	while ( next && next->m_startTime <= statement->m_startTime )
	{
		prev = next;
		next = next->m_next;
	}
	statement->m_next = next;
	statement->m_prev = prev;
	if ( next )
		next->m_prev = statement;
	if ( prev )
		prev->m_next = statement;
	else
		m_statementList = statement;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Remove a statement
 */
void BotChatterInterface::RemoveStatement( BotStatement *statement )
{
	if ( statement->m_next )
		statement->m_next->m_prev = statement->m_prev;
	if ( statement->m_prev )
		statement->m_prev->m_next = statement->m_next;
	else
		m_statementList = statement->m_next;
	delete statement;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Track nearby enemy count and report enemy activity
 */
void BotChatterInterface::ReportEnemies( void )
{
	if ( !m_me->IsAlive() )
		return;
	int count = m_me->GetNearbyEnemyCount();
	if ( count )
	{
		if ( !m_seeAtLeastOneEnemy )
		{
			m_seeAtLeastOneEnemy = true;
			m_timeWhenSawFirstEnemy = gpGlobals->time;
		}
		if ( !m_reportedEnemies )
		{
			if ( !m_me->IsOutnumbered() || !NeedBackup() )
				m_me->GetChatter()->EnemySpotted();
			m_reportedEnemies = true;
		}
	}
	else
	{
		m_seeAtLeastOneEnemy = false;
		m_reportedEnemies = false;
	}
}

//--------------------------------------------------------------------------------------------------------------
void BotChatterInterface::OnEvent( GameEventType, CBaseEntity *, CBaseEntity * )
{
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Invoked when we die
 */
void BotChatterInterface::OnDeath( void )
{
	if ( m_statementList && m_statementList->m_isSpeaking )
	{
		if ( m_me->GetChatter()->GetVerbosity() == BotChatterInterface::MINIMAL ||
			m_me->GetChatter()->GetVerbosity() == BotChatterInterface::NORMAL )
		{
			// we've died mid-sentance - emit a gargle of pain
			static const BotPhrase *pain = TheBotPhrases->GetPhrase( "pain" );
			if ( pain )
			{
				const BotProfile *profile = m_me->GetProfile();
				m_me->Radio( pain->GetSpeakable( profile->GetVoiceBank(), NULL ), NULL, static_cast<short>(profile->GetVoicePitch()), true );
				m_me->GetChatter()->ResetRadioSilenceDuration();
			}
		}
	}

	// remove all of our statements
	Reset();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Process ongoing chatter for this bot
 */
void BotChatterInterface::Update( void )
{
	// report enemy activity
	ReportEnemies();

	// ask team to report in if we havent heard anything in awhile
	if ( ShouldSpeak() && m_me->GetEnemiesRemaining() > 0 && GetRadioSilenceDuration() > 30.0f )
		ReportIn();

	// speak if it is our turn
	BotStatement *active = GetActiveStatement();
	if ( active && active->GetOwner() == m_me && !active->Update() )
		RemoveStatement( active );

	//
	// Process active statements.
	// Removed expired statements, re-order statements according to their relavence and importance
	// Remove redundant statements (ie: our teammates already said them)
	//
	active = GetActiveStatement();
	if ( active && active->GetOwner() == m_me )
		active = NULL;
	BotStatement *statement = m_statementList;
	while ( statement )
	{
		BotStatement *next = statement->m_next;

		// check statement conditions
		if ( !statement->IsValid() )
			RemoveStatement( statement );
		else if ( !statement->m_isSpeaking )
		{
			// check for obsolete statements
			if ( statement->IsObsolete() )
			{
				m_me->PrintIfWatched( "Statement obsolete - removing.\n" );
				RemoveStatement( statement );
			}
			else if ( active )
			{
				// convert what we're about to say based on what our teammate is currently saying
				statement->Convert( active );

				// don't say things our teammates have just said
				if ( statement->IsRedundant( active ) )
				{
					// thie statement is redundant - destroy it
					m_me->PrintIfWatched( "Teammate said what I was going to say - shutting up.\n" );
					RemoveStatement( statement );
				}
			}
		}
		statement = next;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Returns the statement that is being spoken, or is next to be spoken if no-one is speaking now
 */
BotStatement *BotChatterInterface::GetActiveStatement( void )
{
	// keep track of statement waiting longest to be spoken - it is next
	BotStatement *earliest = NULL;
	float earlyTime = 1000000000.0f;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *player = static_cast<CBasePlayer *>(UTIL_PlayerByIndex( i ));
		if ( player == NULL || FNullEnt( player->pev ) || STRING( player->pev->netname )[0] == '\0' )
			continue;

		// ignore dead humans
		if ( !player->IsBot() && !player->IsAlive() )
			continue;

		// ignore enemies, since we can't hear them talk
		if ( player->m_iTeam != m_me->m_iTeam )
			continue;

		// if not a bot, fail the test
		/// @todo Check if human is currently talking
		if ( !player->IsBot() )
			continue;
		CCSBot *bot = static_cast<CCSBot *>(player);
		for ( BotStatement *statement = bot->GetChatter()->m_statementList; statement; statement = statement->m_next )
		{
			// if this statement is currently being spoken, return it
			if ( statement->m_isSpeaking )
				return statement;

			// keep track of statement that has been waiting longest to be spoken of anyone on our team
			if ( statement->m_startTime < earlyTime )
			{
				earliest = statement;
				earlyTime = statement->m_timestamp;
			}
		}
	}
	if ( earliest && earliest->m_startTime <= gpGlobals->time )
		return earliest;
	return NULL;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we speaking makes sense now
 */
bool BotChatterInterface::ShouldSpeak( void ) const
{
	// don't talk to non-existent friends
	if ( m_me->GetFriendsRemaining() == 0 )
		return false;
	return m_me->GetNearbyFriendCount() != m_me->GetFriendsRemaining();
}

//--------------------------------------------------------------------------------------------------------------
float BotChatterInterface::GetRadioSilenceDuration( void )
{
	if ( m_me->m_iTeam != TERRORIST && m_me->m_iTeam != CT )
		return 99999.9f;

	return m_radioSilenceInterval[m_me->m_iTeam - TERRORIST].GetElapsedTime();
}

//--------------------------------------------------------------------------------------------------------------
void BotChatterInterface::ResetRadioSilenceDuration( void )
{
	if ( m_me->m_iTeam != TERRORIST && m_me->m_iTeam != CT )
		return;

	m_radioSilenceInterval[m_me->m_iTeam - TERRORIST].Start();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Report enemy sightings
 */
void BotChatterInterface::EnemySpotted( void )
{
	BotStatement *say = new BotStatement( this, REPORT_VISIBLE_ENEMIES, 10.0f );

	// where are the enemies
	say->AppendPhrase( TheBotPhrases->GetPlace( m_me->GetLastEnemyPlace() ) );

	// how many are there
	say->AppendPhrase( BotStatement::ACCUMULATE_ENEMIES_DELAY );
	say->AppendPhrase( BotStatement::CURRENT_ENEMY_COUNT );
	say->AddCondition( BotStatement::IS_IN_COMBAT );
	AddStatement( say );
}

//--------------------------------------------------------------------------------------------------------------
void BotChatterInterface::Clear( Place where )
{
	BotStatement *say = new BotStatement( this, REPORT_INFORMATION, 10.0f );
	say->AppendPhrase( TheBotPhrases->GetPlace( where ) );
	say->AppendPhrase( TheBotPhrases->GetPhrase( "Clear" ) );
	AddStatement( say );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Request enemy activity report
 */
void BotChatterInterface::ReportIn( void )
{
	BotStatement *say = new BotStatement( this, REPORT_REQUEST_INFORMATION, 10.0f );
	say->AppendPhrase( TheBotPhrases->GetPhrase( "RequestReport" ) );
	say->AddCondition( BotStatement::RADIO_SILENCE );
	say->AttachMeme( new BotRequestReportMeme );
	AddStatement( say );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Report our situtation
 */
void BotChatterInterface::ReportingIn( void )
{
	BotStatement *say = new BotStatement( this, REPORT_INFORMATION, 10.0f );

	// where are we
	Place place = m_me->GetPlace();
	say->AppendPhrase( TheBotPhrases->GetPlace( place ) );

	// what are we doing
	switch ( m_me->GetTask() )
	{
	case CCSBot::PLANT_BOMB:
		m_me->GetChatter()->GoingToPlantTheBomb( UNDEFINED_PLACE );
		break;
	case CCSBot::DEFUSE_BOMB:
		m_me->GetChatter()->Say( "DefusingBomb" );
		break;
	case CCSBot::GUARD_LOOSE_BOMB:
	{
		CBaseEntity *bomb = TheCSBots()->GetLooseBomb();
		if ( bomb )
		{
			say->AppendPhrase( TheBotPhrases->GetPhrase( "GuardingLooseBomb" ) );
			say->AttachMeme( new BotBombStatusMeme( CSGameState::LOOSE, &bomb->pev->origin ) );
		}
		break;
	}
	case CCSBot::RESCUE_HOSTAGES:
		m_me->GetChatter()->EscortingHostages();
		break;
	case CCSBot::GUARD_HOSTAGES:
		m_me->GetChatter()->GuardingHostages( UNDEFINED_PLACE, !m_me->IsAtHidingSpot() );
		break;
	case CCSBot::GUARD_HOSTAGE_RESCUE_ZONE:
		m_me->GetChatter()->GuardingHostageEscapeZone( !m_me->IsAtHidingSpot() );
		break;
	}

	// what do we see
	if ( m_me->IsAttacking() )
	{
		if ( m_me->IsOutnumbered() )
		{
			// in trouble in a firefight
			say->AppendPhrase( TheBotPhrases->GetPhrase( "Help" ) );
			say->AttachMeme( new BotHelpMeme( place ) );
		}
		else
		{
			// battling enemies
			say->AppendPhrase( TheBotPhrases->GetPhrase( "InCombat" ) );
		}
	}
	else
	{
		// not in combat, start our report a little later
		say->SetStartTime( gpGlobals->time + 2.0f );
		float elapsed = gpGlobals->time - m_me->GetEnemyLastSeenTimestamp();
		float deathTimestamp = m_me->GetLastEnemyDeathTimestamp();
		if ( deathTimestamp < 10.0f && deathTimestamp >= elapsed + 0.5f )
		{
			// recently saw an enemy die
			say->AppendPhrase( TheBotPhrases->GetPhrase( "EnemyDown" ) );
		}
		else if ( elapsed < 10.0f )
		{
			// recently saw an enemy
			say->AppendPhrase( TheBotPhrases->GetPhrase( "EnemySpotted" ) );
		}
		else
		{
			// haven't seen enemies
			say->AppendPhrase( TheBotPhrases->GetPhrase( "Clear" ) );
		}
	}
	AddStatement( say );
}

//--------------------------------------------------------------------------------------------------------------
bool BotChatterInterface::NeedBackup( void )
{
	if ( m_needBackupInterval.IsLessThen( 10.0f ) )
		return false;
	m_needBackupInterval.Start();
	if ( m_me->GetFriendsRemaining() )
	{
		// ask friends for help
		BotStatement *say = new BotStatement( this, REPORT_REQUEST_HELP, 10.0f );

		// where are we
		Place place = m_me->GetPlace();
		say->AppendPhrase( TheBotPhrases->GetPlace( place ) );
		say->AppendPhrase( TheBotPhrases->GetPhrase( "Help" ) );
		say->AttachMeme( new BotHelpMeme( place ) );
		AddStatement( say );
	}
	else
	{
		// we're all alone...
		Scared();
	}
	return true;
}

//--------------------------------------------------------------------------------------------------------------
void BotChatterInterface::PinnedDown( void )
{
	// this is a form of "need backup"
	if ( m_needBackupInterval.IsLessThen( 10.0f ) )
		return;
	m_needBackupInterval.Start();
	BotStatement *say = new BotStatement( this, REPORT_REQUEST_HELP, 10.0f );

	// where are we
	Place place = m_me->GetPlace();
	say->AppendPhrase( TheBotPhrases->GetPlace( place ) );
	say->AppendPhrase( TheBotPhrases->GetPhrase( "PinnedDown" ) );
	say->AttachMeme( new BotHelpMeme( place ) );
	say->AddCondition( BotStatement::IS_IN_COMBAT );
	AddStatement( say );
}

//--------------------------------------------------------------------------------------------------------------
void BotChatterInterface::HeardNoise( const Vector *pos )
{
	if ( !TheCSBots()->IsRoundOver() && m_heardNoiseTimer.IsElapsed() )
	{
		// throttle frequency
		m_heardNoiseTimer.Start( 20.0f );

		// make rare, since many teammates may try to say this
		if ( RANDOM_FLOAT( 0.0f, 100.0f ) < 33.0f )
		{
			BotStatement *say = new BotStatement( this, REPORT_INFORMATION, 5.0f );
			say->AppendPhrase( TheBotPhrases->GetPhrase( "HeardNoise" ) );
			say->SetPlace( TheNavAreaGrid.GetPlace( pos ) );
			AddStatement( say );
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
void BotChatterInterface::KilledMyEnemy( int victimID )
{
	// only report if we killed the last enemy in the area
	if ( m_me->GetNearbyEnemyCount() > 1 )
	{
		BotStatement *say = new BotStatement( this, REPORT_ENEMY_ACTION, 3.0f );
		say->AppendPhrase( TheBotPhrases->GetPhrase( "KilledMyEnemy" ) );
		say->SetSubject( victimID );
		AddStatement( say );
	}
}

//--------------------------------------------------------------------------------------------------------------
void BotChatterInterface::EnemiesRemaining( void )
{
	// only report if we killed the last enemy in the area
	if ( m_me->GetNearbyEnemyCount() <= 1 )
	{
		BotStatement *say = new BotStatement( this, REPORT_ENEMIES_REMAINING, 5.0f );
		say->AppendPhrase( BotStatement::REMAINING_ENEMY_COUNT );
		float time = gpGlobals->time;
		say->SetStartTime( time + RANDOM_FLOAT( 2.0f, 4.0f ) );
		AddStatement( say );
	}
}

//--------------------------------------------------------------------------------------------------------------
void BotChatterInterface::Affirmative( void )
{
	BotStatement *say = new BotStatement( this, REPORT_ACKNOWLEDGE, 3.0f );
	say->AppendPhrase( TheBotPhrases->GetPhrase( "Affirmative" ) );
	AddStatement( say );
}

//--------------------------------------------------------------------------------------------------------------
void BotChatterInterface::Negative( void )
{
	BotStatement *say = new BotStatement( this, REPORT_ACKNOWLEDGE, 3.0f );
	say->AppendPhrase( TheBotPhrases->GetPhrase( "Negative" ) );
	AddStatement( say );
}

//--------------------------------------------------------------------------------------------------------------
void BotChatterInterface::GoingToPlantTheBomb( Place where )
{
	if ( TheCSBots()->IsRoundOver() || m_planInterval.IsLessThen( 10.0f ) )
		return;
	m_planInterval.Start();
	BotStatement *say = new BotStatement( this, REPORT_CRITICAL_EVENT, 10.0f );
	say->AppendPhrase( TheBotPhrases->GetPhrase( "GoingToPlantBomb" ) );
	say->SetPlace( where );
	say->AttachMeme( new BotFollowMeme );
	AddStatement( say );
}

//--------------------------------------------------------------------------------------------------------------
void BotChatterInterface::PlantingTheBomb( Place where )
{
	if ( !TheCSBots()->IsRoundOver() )
	{
		BotStatement *say = new BotStatement( this, REPORT_CRITICAL_EVENT, 10.0f );
		say->AppendPhrase( TheBotPhrases->GetPhrase( "PlantingBomb" ) );
		say->SetPlace( where );
		say->AttachMeme( new BotDefendHereMeme( &m_me->pev->origin ) );
		AddStatement( say );
	}
}

//--------------------------------------------------------------------------------------------------------------
void BotChatterInterface::TheyPickedUpTheBomb( void )
{
	if ( !TheCSBots()->IsRoundOver() && m_me->GetGameState()->GetBombState() == CSGameState::LOOSE )
	{
		// update our gamestate - use our own position for now
		m_me->GetGameState()->UpdateBomber( &m_me->pev->origin );

		// tell our teammates
		BotStatement *say = new BotStatement( this, REPORT_INFORMATION, 10.0f );
		say->AppendPhrase( TheBotPhrases->GetPhrase( "TheyPickedUpTheBomb" ) );
		say->AttachMeme( new BotBombStatusMeme( CSGameState::MOVING, &m_me->pev->origin ) );
		AddStatement( say );
	}
}

//--------------------------------------------------------------------------------------------------------------
void BotChatterInterface::SpottedBomber( CBasePlayer *bomber )
{
	if ( m_me->GetGameState()->GetBombState() == CSGameState::MOVING )
	{
		// if we knew where the bomber was, this is old news
		const Vector *bomberPos = m_me->GetGameState()->GetBombPosition();
		if ( bomberPos && (bomber->pev->origin - *bomberPos).LengthSquared() < 1000000.0f )
			return;
	}

	// update our gamestate
	m_me->GetGameState()->UpdateBomber( &bomber->pev->origin );

	// tell our teammates
	BotStatement *say = new BotStatement( this, REPORT_INFORMATION, 10.0f );

	// where is the bomber
	Place place = TheNavAreaGrid.GetPlace( &bomber->pev->origin );
	say->AppendPhrase( TheBotPhrases->GetPlace( place ) );
	say->AppendPhrase( TheBotPhrases->GetPhrase( "SpottedBomber" ) );
	say->SetSubject( bomber->entindex() );
	say->AttachMeme( new BotBombStatusMeme( CSGameState::MOVING, &bomber->pev->origin ) );
	AddStatement( say );
}

//--------------------------------------------------------------------------------------------------------------
void BotChatterInterface::SpottedLooseBomb( CBaseEntity *bomb )
{
	if ( TheCSBots()->IsRoundOver() )
		return;

	// if we already know the bomb is loose, this is old news
	if ( m_me->GetGameState()->GetBombState() == CSGameState::LOOSE )
		return;

	// update our gamestate
	m_me->GetGameState()->UpdateLooseBomb( &bomb->pev->origin );
	if ( !m_spottedLooseBombTimer.IsElapsed() )
		return;

	// throttle frequency
	m_spottedLooseBombTimer.Start( 10.0f );

	// tell our teammates
	BotStatement *say = new BotStatement( this, REPORT_INFORMATION, 10.0f );

	// where is the bomb
	say->AppendPhrase( TheBotPhrases->GetPlace( TheNavAreaGrid.GetPlace( &bomb->pev->origin ) ) );
	say->AppendPhrase( TheBotPhrases->GetPhrase( "SpottedLooseBomb" ) );
	if ( TheCSBots()->GetLooseBomb() )
		say->AttachMeme( new BotBombStatusMeme( CSGameState::LOOSE, &bomb->pev->origin ) );
	AddStatement( say );
}

//--------------------------------------------------------------------------------------------------------------
void BotChatterInterface::GuardingLooseBomb( CBaseEntity *bomb )
{
	if ( TheCSBots()->IsRoundOver() )
		return;

	// update our gamestate
	m_me->GetGameState()->UpdateLooseBomb( &bomb->pev->origin );

	// tell our teammates
	BotStatement *say = new BotStatement( this, REPORT_INFORMATION, 10.0f );

	// where is the bomb
	say->AppendPhrase( TheBotPhrases->GetPlace( TheNavAreaGrid.GetPlace( &bomb->pev->origin ) ) );
	say->AppendPhrase( TheBotPhrases->GetPhrase( "GuardingLooseBomb" ) );
	if ( TheCSBots()->GetLooseBomb() )
		say->AttachMeme( new BotBombStatusMeme( CSGameState::LOOSE, &bomb->pev->origin ) );
	AddStatement( say );
}

//--------------------------------------------------------------------------------------------------------------
void BotChatterInterface::RequestBombLocation( void )
{
	// only ask once per round
	if ( m_requestedBombLocation )
		return;
	m_requestedBombLocation = true;

	// tell our teammates
	BotStatement *say = new BotStatement( this, REPORT_REQUEST_INFORMATION, 10.0f );
	say->AppendPhrase( TheBotPhrases->GetPhrase( "WhereIsTheBomb" ) );
	say->AttachMeme( new BotWhereBombMeme );
	AddStatement( say );
}

//--------------------------------------------------------------------------------------------------------------
void BotChatterInterface::BombsiteClear( int zoneIndex )
{
	const CCSBotManager::Zone *zone = TheCSBots()->GetZone( zoneIndex );
	if ( zone )
	{
		BotStatement *say = new BotStatement( this, REPORT_INFORMATION, 10.0f );
		say->AppendPhrase( TheBotPhrases->GetPlace( TheNavAreaGrid.GetPlace( &zone->m_center ) ) );
		say->AppendPhrase( TheBotPhrases->GetPhrase( "BombsiteClear" ) );
		say->AttachMeme( new BotBombsiteStatusMeme( zoneIndex, 0 ) );
		AddStatement( say );
	}
}

//--------------------------------------------------------------------------------------------------------------
void BotChatterInterface::FoundPlantedBomb( int zoneIndex )
{
	const CCSBotManager::Zone *zone = TheCSBots()->GetZone( zoneIndex );
	if ( zone )
	{
		BotStatement *say = new BotStatement( this, REPORT_INFORMATION, 3.0f );
		say->AppendPhrase( TheBotPhrases->GetPhrase( "PlantedBombPlace" ) );
		say->SetPlace( TheNavAreaGrid.GetPlace( &zone->m_center ) );
		say->AttachMeme( new BotBombsiteStatusMeme( zoneIndex, 1 ) );
		AddStatement( say );
	}
}

//--------------------------------------------------------------------------------------------------------------
void BotChatterInterface::Scared( void )
{
	if ( m_scaredInterval.IsLessThen( 10.0f ) )
		return;
	m_scaredInterval.Start();
	BotStatement *say = new BotStatement( this, REPORT_EMOTE, 1.0f );
	say->AppendPhrase( TheBotPhrases->GetPhrase( "ScaredEmote" ) );
	say->AddCondition( BotStatement::IS_IN_COMBAT );
	AddStatement( say );
}

//--------------------------------------------------------------------------------------------------------------
void BotChatterInterface::CelebrateWin( void )
{
	BotStatement *say = new BotStatement( this, REPORT_EMOTE, 15.0f );

	// wait a bit before speaking
	float time = gpGlobals->time;
	say->SetStartTime( time + RANDOM_FLOAT( 2.0f, 5.0f ) );
	float roundTime = TheCSBots()->GetElapsedRoundTime();
	const char *phrase = NULL;
	if ( m_me->GetFriendsRemaining() )
	{
		if ( roundTime >= 45.0f )
		{
			if ( RANDOM_FLOAT( 0.0f, 100.0f ) < 10.0f )
				phrase = "WonRound";
		}
		else if ( RANDOM_FLOAT( 0.0f, 100.0f ) < 33.3f )
			phrase = "WonRoundQuickly";
	}
	else if ( roundTime >= 45.0f )
	{
		if ( RANDOM_FLOAT( 0.0f, 100.0f ) < 33.3f )
			phrase = "LastManStanding";
	}
	else
		phrase = "WonRoundQuickly";
	if ( phrase )
		say->AppendPhrase( TheBotPhrases->GetPhrase( phrase ) );
	AddStatement( say );
}

//--------------------------------------------------------------------------------------------------------------
void BotChatterInterface::AnnouncePlan( const char *phraseName, Place where )
{
	if ( !TheCSBots()->IsRoundOver() )
	{
		BotStatement *say = new BotStatement( this, REPORT_MY_PLAN, 10.0f );
		say->AppendPhrase( TheBotPhrases->GetPhrase( phraseName ) );
		say->SetPlace( where );

		// wait at least a short time after round start
		float roundStart = TheCSBots()->GetRoundStartTimestamp();
		say->SetStartTime( roundStart + RANDOM_FLOAT( 2.0f, 3.0f ) );
		AddStatement( say );
	}
}

//--------------------------------------------------------------------------------------------------------------
void BotChatterInterface::GuardingHostages( Place where, bool isPlan )
{
	if ( TheCSBots()->IsRoundOver() || m_planInterval.IsLessThen( 20.0f ) )
		return;
	if ( isPlan )
		AnnouncePlan( "GoingToGuardHostages", where );
	else
		Say( "GuardingHostages" );
}

//--------------------------------------------------------------------------------------------------------------
void BotChatterInterface::GuardingHostageEscapeZone( bool isPlan )
{
	if ( TheCSBots()->IsRoundOver() || m_planInterval.IsLessThen( 20.0f ) )
		return;
	if ( isPlan )
		AnnouncePlan( "GoingToGuardHostageEscapeZone", UNDEFINED_PLACE );
	else
		Say( "GuardingHostageEscapeZone" );
}

//--------------------------------------------------------------------------------------------------------------
void BotChatterInterface::HostagesBeingTaken( void )
{
	if ( !TheCSBots()->IsRoundOver() )
	{
		BotStatement *say = new BotStatement( this, REPORT_INFORMATION, 3.0f );
		say->AppendPhrase( TheBotPhrases->GetPhrase( "HostagesBeingTaken" ) );
		say->AttachMeme( new BotHostageBeingTakenMeme );
		AddStatement( say );
	}
}

//--------------------------------------------------------------------------------------------------------------
void BotChatterInterface::HostagesTaken( void )
{
	if ( !TheCSBots()->IsRoundOver() )
	{
		BotStatement *say = new BotStatement( this, REPORT_INFORMATION, 3.0f );
		say->AppendPhrase( TheBotPhrases->GetPhrase( "HostagesTaken" ) );
		AddStatement( say );
	}
}

//--------------------------------------------------------------------------------------------------------------
void BotChatterInterface::TalkingToHostages( void )
{
}

//--------------------------------------------------------------------------------------------------------------
void BotChatterInterface::EscortingHostages( void )
{
	if ( TheCSBots()->IsRoundOver() || !m_escortingHostageTimer.IsElapsed() )
		return;

	// throttle frequency
	m_escortingHostageTimer.Start( 10.0f );
	BotStatement *say = new BotStatement( this, REPORT_MY_PLAN, 5.0f );
	say->AppendPhrase( TheBotPhrases->GetPhrase( "EscortingHostages" ) );
	AddStatement( say );
}

//--------------------------------------------------------------------------------------------------------------
void BotChatterInterface::HostageDown( void )
{
	if ( !TheCSBots()->IsRoundOver() )
	{
		BotStatement *say = new BotStatement( this, REPORT_INFORMATION, 3.0f );
		say->AppendPhrase( TheBotPhrases->GetPhrase( "HostageDown" ) );
		AddStatement( say );
	}
}

//--------------------------------------------------------------------------------------------------------------
void BotChatterInterface::Encourage( const char *phraseName, float repeatInterval, float lifetime )
{
	if ( m_encourageTimer.IsElapsed() )
	{
		Say( phraseName, lifetime );
		m_encourageTimer.Start( repeatInterval );
	}
}

//--------------------------------------------------------------------------------------------------------------
void BotChatterInterface::KilledFriend( void )
{
	BotStatement *say = new BotStatement( this, REPORT_KILLED_FRIEND, 2.0f );
	say->AppendPhrase( TheBotPhrases->GetPhrase( "KilledFriend" ) );

	// give them time to react
	float time = gpGlobals->time;
	say->SetStartTime( time + RANDOM_FLOAT( 0.5f, 1.0f ) );
	AddStatement( say );
}

//--------------------------------------------------------------------------------------------------------------
void BotChatterInterface::FriendlyFire( void )
{
	BotStatement *say = new BotStatement( this, REPORT_FRIENDLY_FIRE, 1.0f );
	say->AppendPhrase( TheBotPhrases->GetPhrase( "FriendlyFire" ) );

	// give them time to react
	float time = gpGlobals->time;
	say->SetStartTime( time + RANDOM_FLOAT( 0.3f, 0.5f ) );
	AddStatement( say );
}
