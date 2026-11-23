#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"
#include "bot_util.h"
#include "tutor.h"
#include "tutor_cs_tutor.h"
#include "tutor_cs_states.h"
#include "career_tasks.h"
#include "hostage/hostage.h"
#include "bot/cs_bot_manager.h"
#include "bot/cs_bot.h"

char *MP_COM_Parse( char *data );
char *MP_COM_GetToken();

char *TutorIdentifierList[TUTOR_NUM_MESSAGES] =
{
	"YOU_FIRED_A_SHOT",
	"YOU_SHOULD_RELOAD",
	"YOU_ARE_OUT_OF_AMMO",
	"YOU_KILLED_A_TEAMMATE",
	"YOU_KILLED_PLAYER",
	"YOU_KILLED_PLAYER_ONE_LEFT",
	"YOU_KILLED_LAST_ENEMY",
	"YOU_KILLED_PLAYER_HEADSHOT",
	"YOU_KILLED_PLAYER_HEADSHOT_ONE_LEFT",
	"YOU_KILLED_LAST_ENEMY_HEADSHOT",
	"YOU_DIED",
	"YOU_DIED_HEADSHOT",
	"YOU_FELL_TO_YOUR_DEATH",
	"YOU_WERE_JUST_HURT",
	"YOU_ARE_BLIND_FROM_FLASHBANG",
	"YOU_ATTACKED_TEAMMATE",
	"BUY_TIME_BEGIN",
	"BOMB_PLANTED_T",
	"BOMB_PLANTED_CT",
	"TEAMMATE_KILLED",
	"TEAMMATE_KILLED_ONE_LEFT",
	"LAST_TEAMMATE_KILLED",
	"ENEMY_KILLED",
	"ENEMY_KILLED_ONE_LEFT",
	"LAST_ENEMY_KILLED",
	"YOU_SPAWNED",
	"YOU_SEE_FRIEND",
	"YOU_SEE_ENEMY",
	"YOU_SEE_FRIEND_CORPSE",
	"YOU_SEE_ENEMY_CORPSE",
	"YOU_SEE_LOOSE_BOMB_T",
	"YOU_SEE_LOOSE_BOMB_CT",
	"YOU_SEE_BOMB_CARRIER_T",
	"YOU_SEE_BOMB_CARRIER_CT",
	"YOU_SEE_PLANTED_BOMB_T",
	"YOU_SEE_PLANTED_BOMB_CT",
	"YOU_ARE_BOMB_CARRIER",
	"YOU_SEE_LOOSE_WEAPON",
	"YOU_SEE_LOOSE_DEFUSER",
	"YOU_SEE_BOMBSITE_T",
	"YOU_SEE_BOMBSITE_CT",
	"YOU_SEE_BOMBSITE_T_BOMB",
	"YOU_SEE_HOSTAGE_T",
	"YOU_SEE_HOSTAGE_CT",
	"YOU_SEE_HOSTAGE_CT_EXAMINE",
	"YOU_USED_HOSTAGE_MORE_LEFT",
	"YOU_USED_HOSTAGE_NO_MORE_LEFT",
	"ALL_HOSTAGES_FOLLOWING_T",
	"ALL_HOSTAGES_FOLLOWING_CT",
	"HOSTAGE_RESCUED_T",
	"HOSTAGE_RESCUED_CT",
	"YOU_RESCUED_HOSTAGE",
	"YOU_ARE_IN_BOMBSITE_T",
	"YOU_ARE_IN_BOMBSITE_CT",
	"YOU_ARE_IN_BOMBSITE_T_BOMB",
	"ALL_HOSTAGES_RESCUED_T",
	"ALL_HOSTAGES_RESCUED_CT",
	"YOU_DAMAGED_HOSTAGE",
	"YOU_KILLED_HOSTAGE",
	"ALL_HOSTAGES_DEAD",
	"YOU_HAVE_BEEN_SHOT_AT",
	"TIME_RUNNING_OUT_DE_T",
	"TIME_RUNNING_OUT_DE_CT",
	"TIME_RUNNING_OUT_CS_T",
	"TIME_RUNNING_OUT_CS_CT",
	"DEFUSING_WITHOUT_KIT",
	"BOMB_DEFUSED_T",
	"BOMB_DEFUSED_CT",
	"YOU_DEFUSED_BOMB",
	"BOMB_EXPLODED_T",
	"BOMB_EXPLODED_CT",
	"ROUND_START_DE_T",
	"ROUND_START_DE_CT",
	"ROUND_START_CS_T",
	"ROUND_START_CS_CT",
	"ROUND_OVER",
	"ROUND_DRAW",
	"CT_WIN",
	"T_WIN",
	"DEATH_CAMERA_START",
	"RADIO_COVER_ME",
	"RADIO_YOU_TAKE_THE_POINT",
	"RADIO_HOLD_THIS_POSITION",
	"RADIO_REGROUP_TEAM",
	"RADIO_FOLLOW_ME",
	"RADIO_TAKING_FIRE",
	"RADIO_GO_GO_GO",
	"RADIO_TEAM_FALL_BACK",
	"RADIO_STICK_TOGETHER_TEAM",
	"RADIO_GET_IN_POSITION_AND_WAIT",
	"RADIO_STORM_THE_FRONT",
	"RADIO_REPORT_IN_TEAM",
	"RADIO_AFFIRMATIVE",
	"RADIO_ENEMY_SPOTTED",
	"RADIO_NEED_BACKUP",
	"RADIO_SECTOR_CLEAR",
	"RADIO_IN_POSITION",
	"RADIO_REPORTING_IN",
	"RADIO_GET_OUT_OF_THERE",
	"RADIO_NEGATIVE",
	"RADIO_ENEMY_DOWN",
	"BUY_NEED_PRIMARY",
	"BUY_NEED_PRIMARY_AMMO",
	"BUY_NEED_SECONDARY_AMMO",
	"BUY_NEED_ARMOR",
	"BUY_NEED_DEFUSE_KIT",
	"BUY_NEED_GRENADE",
	"CAREER_TASK_DONE_MORE_LEFT",
	"CAREER_TASK_DONE_ONE_LEFT",
	"CAREER_TASK_DONE_ALL_DONE",
	"HINT_BEGIN",
	"HINT_1",
	"HINT_2",
	"HINT_3",
	"HINT_4",
	"HINT_5",
	"HINT_10",
	"HINT_11",
	"HINT_12",
	"HINT_13",
	"HINT_14",
	"HINT_15",
	"HINT_20",
	"HINT_21",
	"HINT_22",
	"HINT_23",
	"HINT_24",
	"HINT_25",
	"HINT_26",
	"HINT_30",
	"HINT_31",
	"HINT_32",
	"HINT_33",
	"HINT_34",
	"HINT_40",
	"HINT_50",
	"HINT_51",
	"HINT_52",
	"HINT_53",
	"HINT_60",
	"HINT_61",
	"HINT_70",
	"HINT_71",
	"HINT_72",
	"HINT_73",
	"HINT_END",
	"INGAME_HINT_BEGIN",
	"INGAME_HINT_1",
	"INGAME_HINT_2",
	"INGAME_HINT_END"
};


CCSTutor::CCSTutor( void )
{
	m_stateSystem = new CCSTutorStateSystem;
	m_nextViewableCheckTime = 0;
	m_currentlyShownMessageID = TUTOR_NUM_MESSAGES;
	m_currentMessageEvent = NULL;

	m_messageTypeMask = TUTORMESSAGETYPE_ALL;

	m_lastScenarioEvent = NULL;

	m_haveSpawned = false;

	m_lastHintShown = HINT_BEGIN;

	m_lastInGameHintShown = INGAME_HINT_BEGIN;

	ReadTutorMessageFile();

	ApplyPersistentDecay();

	ResetPlayerDeathInfo();
}

CCSTutor::~CCSTutor( void )
{
	if ( m_stateSystem != NULL )
	{
		delete m_stateSystem;
		m_stateSystem = NULL;
	}

	if ( m_currentMessageEvent != NULL )
	{
		DeleteEvent( m_currentMessageEvent );
		m_currentMessageEvent = NULL;
	}

	if ( m_lastScenarioEvent != NULL )
	{
		DeleteEvent( m_lastScenarioEvent );
		m_lastScenarioEvent = NULL;
	}

	ClearCurrentEvent( true, true );

	ClearEventList();
}

static void ParseMessageParameters( char *&messageData, TutorMessage *ret )
{
	char *token = NULL;

	messageData = MP_COM_Parse( messageData );
	token = MP_COM_GetToken();

	while ( messageData != NULL && stricmp( token, "End" ) != 0 )
	{
		if ( stricmp( token, "String" ) == 0 )
		{
			messageData = MP_COM_Parse( messageData );
			token = MP_COM_GetToken();

			ret->m_text = strdup( token );
		}
		else if ( stricmp( token, "Duration" ) == 0 )
		{
			messageData = MP_COM_Parse( messageData );
			token = MP_COM_GetToken();

			ret->m_duration = (unsigned char)atoi( token );
		}
		else if ( stricmp( token, "Priority" ) == 0 )
		{
			messageData = MP_COM_Parse( messageData );
			token = MP_COM_GetToken();

			ret->m_priority = (unsigned char)atoi( token );
		}
		else if ( stricmp( token, "KeepOld" ) == 0 )
		{
			messageData = MP_COM_Parse( messageData );
			token = MP_COM_GetToken();

			if ( stricmp( token, "true" ) == 0 )
			{
				ret->m_keepOld = TUTORMESSAGEKEEPOLDTYPE_KEEP_OLD;
			}
			else if ( stricmp( token, "updatecontent" ) == 0 )
			{
				ret->m_keepOld = TUTORMESSAGEKEEPOLDTYPE_UPDATE_CONTENT;
			}
			else
			{
				ret->m_keepOld = TUTORMESSAGEKEEPOLDTYPE_DONT_KEEP_OLD;
			}
		}
		else if ( stricmp( token, "Class" ) == 0 )
		{
			messageData = MP_COM_Parse( messageData );
			token = MP_COM_GetToken();

			if ( stricmp( token, "Examine" ) == 0 )
			{
				ret->m_class = TUTORMESSAGECLASS_EXAMINE;
			}
			else
			{
				ret->m_class = TUTORMESSAGECLASS_NORMAL;
			}
		}
		else if ( stricmp( token, "Decay" ) == 0 )
		{
			messageData = MP_COM_Parse( messageData );
			token = MP_COM_GetToken();

			ret->m_decay = (unsigned char)atoi( token );
		}
		else if ( stricmp( token, "Type" ) == 0 )
		{
			messageData = MP_COM_Parse( messageData );
			token = MP_COM_GetToken();

			if ( stricmp( token, "FriendDeath" ) == 0 )
			{
				ret->m_type = TUTORMESSAGETYPE_FRIEND_DEATH;
			}
			else if ( stricmp( token, "EnemyDeath" ) == 0 )
			{
				ret->m_type = TUTORMESSAGETYPE_ENEMY_DEATH;
			}
			else if ( stricmp( token, "Scenario" ) == 0 )
			{
				ret->m_type = TUTORMESSAGETYPE_SCENARIO;
			}
			else if ( stricmp( token, "Buy" ) == 0 )
			{
				ret->m_type = TUTORMESSAGETYPE_BUY;
			}
			else if ( stricmp( token, "Career" ) == 0 )
			{
				ret->m_type = TUTORMESSAGETYPE_CAREER;
			}
			else if ( stricmp( token, "Hint" ) == 0 )
			{
				ret->m_type = TUTORMESSAGETYPE_HINT;
			}
			else if ( stricmp( token, "InGameHint" ) == 0 )
			{
				ret->m_type = TUTORMESSAGETYPE_INGAME_HINT;
			}
			else if ( stricmp( token, "EndGame" ) == 0 )
			{
				ret->m_type = TUTORMESSAGETYPE_END_GAME;
			}
			else
			{
				ret->m_type = TUTORMESSAGETYPE_DEFAULT;
			}
		}
		else if ( stricmp( token, "Lifetime" ) == 0 )
		{
			messageData = MP_COM_Parse( messageData );
			token = MP_COM_GetToken();

			ret->m_lifetime = atoi( token );
		}
		else if ( stricmp( token, "DuplicateID" ) == 0 )
		{
			messageData = MP_COM_Parse( messageData );
			token = MP_COM_GetToken();

			ret->m_duplicateID = atoi( token );
		}
		else if ( stricmp( token, "Interrupt" ) == 0 )
		{
			messageData = MP_COM_Parse( messageData );
			token = MP_COM_GetToken();

			if ( stricmp( token, "Now" ) == 0 )
			{
				ret->m_interruptFlag = TUTORMESSAGEINTERRUPTFLAG_NOW_DAMMIT;
			}
			else
			{
				ret->m_interruptFlag = TUTORMESSAGEINTERRUPTFLAG_DEFAULT;
			}
		}
		else if ( stricmp( token, "MinDisplayTimeOverride" ) == 0 )
		{
			messageData = MP_COM_Parse( messageData );
			token = MP_COM_GetToken();

			ret->m_minDisplayTimeOverride = (float)atof( token );
		}
		else if ( stricmp( token, "MinRepeatInterval" ) == 0 )
		{
			messageData = MP_COM_Parse( messageData );
			token = MP_COM_GetToken();

			ret->m_minRepeatInterval = (float)atof( token );
		}

		messageData = MP_COM_Parse( messageData );
		token = MP_COM_GetToken();
	}
}

static TutorMessage *ConstructTutorMessage( char *&messageData, TutorMessage *defaults )
{
	TutorMessage *ret = new TutorMessage;

	ret->m_text = NULL;

	ret->m_duplicateID = defaults->m_duplicateID;
	ret->m_keepOld = defaults->m_keepOld;
	ret->m_duration = defaults->m_duration;
	ret->m_priority = defaults->m_priority;
	ret->m_class = defaults->m_class;
	ret->m_type = defaults->m_type;
	ret->m_decay = defaults->m_decay;
	ret->m_lifetime = defaults->m_lifetime;
	ret->m_interruptFlag = defaults->m_interruptFlag;
	ret->m_minDisplayTimeOverride = defaults->m_minDisplayTimeOverride;

	ret->m_examineStartTime = -1.0f;
	ret->m_timesShown = 0;
	ret->m_lastCloseTime = 0;

	ParseMessageParameters( messageData, ret );

	return ret;
}

static void ReadDefaultValues( char *&messageData, TutorMessage *defaults )
{
	ParseMessageParameters( messageData, defaults );
}

void CCSTutor::ReadTutorMessageFile( void )
{
	int messageFileLen = 0;
	char *messageFile = (char *)LOAD_FILE_FOR_ME( "tutordata.txt", &messageFileLen );
	char *messageData = messageFile;

	if ( messageFile == NULL )
	{
		if ( UTIL_IsGame( "czero" ) )
		{
			CONSOLE_ECHO( "Warning: Cannot access tutor message file tutordata.txt\n" );
		}
		return;
	}

	TutorMessage defaultMessage;
	defaultMessage.m_duplicateID = 0;
	defaultMessage.m_keepOld = TUTORMESSAGEKEEPOLDTYPE_DONT_KEEP_OLD;
	defaultMessage.m_duration = 1;
	defaultMessage.m_priority = 0;
	defaultMessage.m_class = TUTORMESSAGECLASS_NORMAL;
	defaultMessage.m_type = TUTORMESSAGETYPE_DEFAULT;
	defaultMessage.m_decay = 10;
	defaultMessage.m_lifetime = 10;
	defaultMessage.m_interruptFlag = TUTORMESSAGEINTERRUPTFLAG_DEFAULT;
	defaultMessage.m_minDisplayTimeOverride = 0;
	defaultMessage.m_minRepeatInterval = 0;

	for ( ;; )
	{
		messageData = MP_COM_Parse( messageData );
		if ( messageData == NULL )
		{
			break;
		}

		char *token = MP_COM_GetToken();
		if ( stricmp( token, "TutorMessage" ) == 0 )
		{
			messageData = MP_COM_Parse( messageData );
			std::string identifier;
			identifier = MP_COM_GetToken();

			TutorMessage *tm = ConstructTutorMessage( messageData, &defaultMessage );
			m_messageMap[identifier] = tm;
		}
		else if ( stricmp( token, "Defaults" ) == 0 )
		{
			ReadDefaultValues( messageData, &defaultMessage );
		}
	}

	FREE_FILE( messageFile );
}

void CCSTutor::ApplyPersistentDecay( void )
{
	TutorMessageID mid;

	for ( mid = YOU_FIRED_A_SHOT; mid < TUTOR_NUM_MESSAGES; mid = (TutorMessageID)(mid + 1) )
	{
		TutorMessage *definition = GetTutorMessageDefinition( mid );
		if ( definition != NULL )
		{
			int timesShown = g_engfuncs.pfnGetTimesTutorMessageShown( mid );
			if ( timesShown != -1 )
			{
				definition->m_timesShown = timesShown;
			}
		}
	}
}

bool CCSTutor::HasCurrentWindowBeenActiveLongEnough( float time )
{
	if ( (unsigned int)m_currentlyShownMessageID >= TUTOR_NUM_MESSAGES )
	{
		return true;
	}

	if ( time > m_currentlyShownMessageMinimumCloseTime )
	{
		return true;
	}

	return false;
}

bool CCSTutor::ShouldShowMessageEvent( TutorMessageEvent *event, float time )
{
	if ( event == NULL )
	{
		return false;
	}

	TutorMessage *message = GetTutorMessageDefinition( event->GetID() );
	if ( message == NULL )
	{
		return false;
	}

	if ( message->m_class == TUTORMESSAGECLASS_NORMAL && message->m_decay != 0 && message->m_timesShown >= message->m_decay )
	{
		return false;
	}

	if ( !(message->m_type & m_messageTypeMask) )
	{
		return false;
	}

	if ( message->m_minRepeatInterval > time - message->m_lastCloseTime )
	{
		return false;
	}

	if ( (unsigned int)m_currentlyShownMessageID >= TUTOR_NUM_MESSAGES )
	{
		return true;
	}

	if ( message->m_interruptFlag != TUTORMESSAGEINTERRUPTFLAG_NOW_DAMMIT && time <= m_currentlyShownMessageMinimumCloseTime )
	{
		return false;
	}

	TutorMessage *current = GetTutorMessageDefinition( m_currentlyShownMessageID );

	if ( DoMessagesHaveSameID( event->GetID(), m_currentlyShownMessageID ) && current->m_keepOld != TUTORMESSAGEKEEPOLDTYPE_DONT_KEEP_OLD )
	{
		return false;
	}

	if ( message->m_priority > current->m_priority )
	{
		return true;
	}

	if ( message->m_priority == current->m_priority )
	{
		if ( event->GetTimeActive( time ) > m_currentMessageEvent->GetTimeActive( time ) )
		{
			return true;
		}
	}

	return false;
}


void CCSTutor::CheckForInterruptingMessageEvent( float time )
{
	bool newEvent = false;
	TutorMessageEvent *event = m_eventList;

	TutorMessage *oldMessage = GetTutorMessageDefinition( m_currentlyShownMessageID );

	TutorMessageEvent *oldEvent = m_currentMessageEvent;

	while ( event != NULL )
	{
		if ( ShouldShowMessageEvent( event, time ) )
		{
			newEvent = true;
			ShowTutorMessage( event );
		}

		event = event->GetNext();
	}

	if ( newEvent )
	{
		if ( oldEvent != NULL )
		{
			ProcessShownDeathsForEvent( oldEvent );
			DeleteEvent( oldEvent );
		}

		CloseCurrentWindow();
		if ( oldMessage != NULL )
		{
			oldMessage->m_lastCloseTime = time;
		}

		DeleteEventFromEventList( m_currentMessageEvent );
		ConstructMessageAndDisplay();
	}
}


void CCSTutor::TutorThink( float time )
{
	if ( time >= m_nextViewableCheckTime )
	{
		CheckForBombViewable();
		CheckForLooseWeaponViewable();
		CheckForLooseDefuserViewable();
		CheckForTimeRunningOut();
		CheckForBombsiteViewable();
		CheckForHostageViewable();

		CheckExamineMessages( time );

		CheckHintMessages( time );

		CheckInGameHintMessages( time );

		CheckForNeedToReload( true );

		if ( m_haveSpawned && CanLocalPlayerBuyStuff() )
		{
			m_messageTypeMask = TUTORMESSAGETYPE_BUY;
			CreateAndAddEventToList( BUY_TIME_BEGIN, NULL, NULL );
			m_haveSpawned = false;
		}
		if ( CanLocalPlayerBuyStuff() && m_messageTypeMask == TUTORMESSAGETYPE_BUY )
		{
			CheckBuyZoneMessages();
		}
		else if ( !CanLocalPlayerBuyStuff() && m_messageTypeMask == TUTORMESSAGETYPE_BUY )
		{
			OnEvent( EVENT_PLAYER_LEFT_BUY_ZONE, NULL, NULL );
		}
		m_nextViewableCheckTime = time + cv_tutor_viewable_check_interval.value;
	}

	CheckForInactiveEvents( time );
	CheckForWindowClose( time );
	CheckForContentUpdate();

	CheckForInterruptingMessageEvent( time );
}


void CCSTutor::CheckForWindowClose( float time )
{
	if ( (unsigned int)m_currentlyShownMessageID >= TUTOR_NUM_MESSAGES )
	{
		return;
	}

	if ( time > m_currentlyShownMessageCloseTime )
	{
		TutorMessageEvent *event = GetTutorMessageUpdateEvent();
		if ( event == NULL )
		{
			ClearCurrentEvent( true, true );
		}
		else
		{
			UpdateCurrentMessage( event );
			DeleteEventFromEventList( event );
		}
	}
}


void CCSTutor::CheckForContentUpdate( void )
{
	if ( (unsigned int)m_currentlyShownMessageID >= TUTOR_NUM_MESSAGES )
	{
		return;
	}

	TutorMessage *definition = GetTutorMessageDefinition( m_currentlyShownMessageID );
	if ( definition == NULL )
	{
		return;
	}

	if ( definition->m_keepOld != TUTORMESSAGEKEEPOLDTYPE_UPDATE_CONTENT )
	{
		return;
	}

	TutorMessageEvent *event = GetTutorMessageUpdateEvent();
	if ( event == NULL )
	{
		return;
	}

	UpdateCurrentMessage( event );
	DeleteEventFromEventList( event );
}


void CCSTutor::ClearCurrentEvent( bool closeWindow, bool processDeathsForEvent )
{
	TutorMessage *oldMessage = GetTutorMessageDefinition( m_currentlyShownMessageID );
	if ( oldMessage != NULL )
	{
		oldMessage->m_lastCloseTime = gpGlobals->time;
	}

	if ( m_currentMessageEvent != NULL && processDeathsForEvent )
	{
		ProcessShownDeathsForEvent( m_currentMessageEvent );
	}

	if ( closeWindow )
	{
		CloseCurrentWindow();
	}

	m_currentlyShownMessageID = TUTOR_NUM_MESSAGES;
	m_currentlyShownMessageCloseTime = 0;
	m_currentlyShownMessageMinimumCloseTime = 0;
	if ( m_currentMessageEvent != NULL )
	{
		DeleteEvent( m_currentMessageEvent );
		m_currentMessageEvent = NULL;
	}
}


void CCSTutor::ProcessShownDeathsForEvent( TutorMessageEvent *event )
{
	if ( event == NULL )
	{
		return;
	}

	for ( int i = 0; i < ARRAYSIZE( m_playerDeathInfo ); i++ )
	{
		if ( m_playerDeathInfo[i].m_event == event )
		{
			m_playerDeathInfo[i].m_hasBeenShown = true;
			m_playerDeathInfo[i].m_event = NULL;
		}
	}
}


TutorMessageEvent *CCSTutor::GetTutorMessageUpdateEvent( void )
{
	TutorMessage *definition = GetTutorMessageDefinition( m_currentlyShownMessageID );
	if ( definition == NULL || definition->m_keepOld == TUTORMESSAGEKEEPOLDTYPE_DONT_KEEP_OLD )
	{
		return NULL;
	}

	TutorMessageEvent *event = m_eventList;
	while ( event != NULL )
	{
		if ( DoMessagesHaveSameID( event->GetID(), m_currentlyShownMessageID ) )
		{
			return event;
		}
		event = event->GetNext();
	}

	return NULL;
}


bool CCSTutor::GetDuplicateMessagesFromEventList( TutorMessageEvent *&event1, TutorMessageEvent *&event2 )
{
	event1 = m_eventList;
	while ( event1 != NULL )
	{
		event2 = event1->GetNext();
		while ( event2 != NULL )
		{
			if ( DoMessagesHaveSameID( event1->GetID(), event2->GetID() ) )
			{
				return true;
			}
			event2 = event2->GetNext();
		}
		event1 = event1->GetNext();
	}
	return false;
}


void CCSTutor::CheckForInactiveEvents( float time )
{
	TutorMessageEvent *event = m_eventList;

	while ( event != NULL )
	{
		if ( !event->IsActive( time ) )
		{
			TutorMessageEvent *temp = event;
			event = event->GetNext();
			DeleteEventFromEventList( temp );

			DeleteEvent( temp );
		}
		else
		{
			event = event->GetNext();
		}
	}

	TutorMessageEvent *event1 = NULL;
	TutorMessageEvent *event2 = NULL;
	bool foundDup = false;

	foundDup = GetDuplicateMessagesFromEventList( event1, event2 );
	while ( foundDup )
	{
		if ( event1->GetTimeActive( time ) < event2->GetTimeActive( time ) )
		{
			TransferDeathEvents( event2, event1 );
			DeleteEventFromEventList( event2 );
			DeleteEvent( event2 );
		}
		else
		{
			TransferDeathEvents( event1, event2 );
			DeleteEventFromEventList( event1 );
			DeleteEvent( event1 );
		}

		foundDup = GetDuplicateMessagesFromEventList( event1, event2 );
	}
}


void CCSTutor::CancelEvent( TutorMessageID mid )
{
	if ( m_currentlyShownMessageID == mid )
	{
		ClearCurrentEvent( true, true );
	}

	TutorMessageEvent *event = m_eventList;
	while ( event != NULL )
	{
		if ( event->GetID() == mid )
		{
			TutorMessageEvent *temp = event;
			event = event->GetNext();
			DeleteEventFromEventList( temp );
			DeleteEvent( temp );
		}
		else
		{
			event = event->GetNext();
		}
	}

	if ( m_lastScenarioEvent != NULL && m_lastScenarioEvent->GetID() == mid )
	{
		DeleteEvent( m_lastScenarioEvent );
		m_lastScenarioEvent = NULL;
	}
}


void CCSTutor::LookupHotKey( TutorMessageID mid, int paramNum, wchar_t *buf, int buflen )
{
}


TutorMessageEvent *CCSTutor::CreateTutorMessageEvent( TutorMessageID mid, CBaseEntity *entity, CBaseEntity *other )
{
	CBasePlayer *localPlayer = UTIL_GetLocalPlayer();
	if ( localPlayer == NULL )
	{
		return NULL;
	}

	TutorMessage *message = GetTutorMessageDefinition( mid );
	if ( message == NULL )
	{
		return NULL;
	}

	TutorMessageEvent *event = NULL;
	event = new TutorMessageEvent( mid, message->m_duplicateID, gpGlobals->time, (float)message->m_lifetime, message->m_priority );

	if ( event == NULL )
	{
		return NULL;
	}

	switch ( mid )
	{
	case TEAMMATE_KILLED:
	{
		int numT;
		int numCT;
		GetNumPlayersAliveOnTeams( numT, numCT );

		if ( localPlayer->IsAlive() )
		{
			if ( localPlayer->m_iTeam == TERRORIST )
			{
				numT--;
			}
			else if ( localPlayer->m_iTeam == CT )
			{
				numCT--;
			}
		}

		char numTStr[16];
		snprintf( numTStr, sizeof( numTStr ), "%i", numT );

		char numCTStr[16];
		snprintf( numCTStr, sizeof( numCTStr ), "%i", numCT );

		if ( localPlayer->m_iTeam == CT )
		{
			event->AddParameter( numCTStr );
		}
		else if ( localPlayer->m_iTeam == TERRORIST )
		{
			event->AddParameter( numTStr );
		}
		[[fallthrough]];
	}
	case YOU_KILLED_A_TEAMMATE:
	case TEAMMATE_KILLED_ONE_LEFT:
	case LAST_TEAMMATE_KILLED:
	{
		char teammateList[2048];
		ConstructRecentDeathsList( localPlayer->m_iTeam, teammateList, sizeof( teammateList ), event );
		event->AddParameter( teammateList );

		break;
	}
	case YOU_KILLED_PLAYER:
	case YOU_KILLED_PLAYER_HEADSHOT:
	case ENEMY_KILLED:
	{
		int numT;
		int numCT;
		GetNumPlayersAliveOnTeams( numT, numCT );

		char numTStr[16];
		snprintf( numTStr, sizeof( numTStr ), "%i", numT );

		char numCTStr[16];
		snprintf( numCTStr, sizeof( numCTStr ), "%i", numCT );

		if ( localPlayer->m_iTeam == CT )
		{
			event->AddParameter( numTStr );
		}
		else if ( localPlayer->m_iTeam == TERRORIST )
		{
			event->AddParameter( numCTStr );
		}
		[[fallthrough]];
	}
	case YOU_KILLED_PLAYER_ONE_LEFT:
	case YOU_KILLED_LAST_ENEMY:
	case YOU_KILLED_PLAYER_HEADSHOT_ONE_LEFT:
	case YOU_KILLED_LAST_ENEMY_HEADSHOT:
	case ENEMY_KILLED_ONE_LEFT:
	case LAST_ENEMY_KILLED:
	{
		char enemyList[2048];
		if ( localPlayer->m_iTeam == CT )
		{
			ConstructRecentDeathsList( TERRORIST, enemyList, sizeof( enemyList ), event );
		}
		else if ( localPlayer->m_iTeam == TERRORIST )
		{
			ConstructRecentDeathsList( CT, enemyList, sizeof( enemyList ), event );
		}
		else
		{
			return event;
		}

		event->AddParameter( enemyList );

		break;
	}
	case CAREER_TASK_DONE_MORE_LEFT:
	{
		char numLeftStr[16];

		int numtasks = 0;
		if ( TheCareerTasks != NULL )
		{
			numtasks = TheCareerTasks->GetNumRemainingTasks();
		}
		sprintf( numLeftStr, "%d", numtasks );
		event->AddParameter( numLeftStr );

		break;
	}
	}
	return event;
}


void CCSTutor::AddToEventList( TutorMessageEvent *event )
{
	if ( event == NULL )
	{
		return;
	}

	event->SetNext( m_eventList );
	m_eventList = event;
}


void CCSTutor::CreateAndAddEventToList( TutorMessageID mid, CBaseEntity *entity, CBaseEntity *other )
{
	TutorMessageEvent *event = CreateTutorMessageEvent( mid, entity, other );

	if ( event == NULL )
	{
		return;
	}

	TutorMessage *message = GetTutorMessageDefinition( mid );

	if ( message != NULL && message->m_type == TUTORMESSAGETYPE_SCENARIO )
	{
		if ( m_lastScenarioEvent != NULL )
		{
			DeleteEvent( m_lastScenarioEvent );
			m_lastScenarioEvent = NULL;
		}

		m_lastScenarioEvent = CreateTutorMessageEvent( mid, entity, other );
	}

	AddToEventList( event );
}


void CCSTutor::DeleteEventFromEventList( TutorMessageEvent *event )
{
	if ( event == NULL )
	{
		return;
	}

	TutorMessageEvent *temp = m_eventList;
	if ( temp == event )
	{
		m_eventList = event->GetNext();
	}
	else
	{
		while ( temp != NULL && temp->GetNext() != event )
		{
			temp = temp->GetNext();
		}

		if ( temp != NULL && temp->GetNext() == event )
		{
			temp->SetNext( temp->GetNext()->GetNext() );
		}
	}
}


void CCSTutor::ClearEventList( void )
{
	TutorMessageEvent *temp = m_eventList;
	while ( temp != NULL )
	{
		m_eventList = temp->GetNext();
		DeleteEvent( temp );
		temp = m_eventList;
	}
}


void CCSTutor::DeleteEvent( TutorMessageEvent *event )
{
	for ( int i = 0; i < ARRAYSIZE( m_playerDeathInfo ); i++ )
	{
		if ( m_playerDeathInfo[i].m_event == event )
		{
			m_playerDeathInfo[i].m_event = NULL;
		}
	}
	delete event;
}


void CCSTutor::PurgeMessages( void )
{
	ClearCurrentEvent( true, true );
	ClearEventList();

	if ( m_lastScenarioEvent != NULL )
	{
		DeleteEvent( m_lastScenarioEvent );
		m_lastScenarioEvent = NULL;
	}
}


void CCSTutor::ComputeDisplayTimesForMessage( void )
{
	float now = gpGlobals->time;

	TutorMessage *message = GetTutorMessageDefinition( m_currentlyShownMessageID );
	if ( message == NULL )
	{
		m_currentlyShownMessageCloseTime = now;
		return;
	}

	m_currentlyShownMessageCloseTime = message->m_duration + now;

	m_currentlyShownMessageMinimumCloseTime = cv_tutor_message_minimum_display_time.value;

	int stringLength = g_engfuncs.pfnGetLocalizedStringLength( message->m_text );
	float minShowTime = stringLength * cv_tutor_message_character_display_time_coefficient.value;

	if ( minShowTime > m_currentlyShownMessageMinimumCloseTime )
	{
		m_currentlyShownMessageMinimumCloseTime = minShowTime;
	}

	if ( message->m_minDisplayTimeOverride > m_currentlyShownMessageMinimumCloseTime )
	{
		m_currentlyShownMessageMinimumCloseTime = message->m_minDisplayTimeOverride;
	}

	m_currentlyShownMessageMinimumCloseTime += now;

	if ( m_currentlyShownMessageMinimumCloseTime > m_currentlyShownMessageCloseTime )
	{
		m_currentlyShownMessageCloseTime = m_currentlyShownMessageMinimumCloseTime;
	}
}


bool CCSTutor::ShouldUpdateCurrentMessage( TutorMessageID messageID )
{
	if ( DoMessagesHaveSameID( messageID, m_currentlyShownMessageID ) )
	{
		TutorMessage *definition = GetTutorMessageDefinition( messageID );
		if ( definition->m_keepOld != TUTORMESSAGEKEEPOLDTYPE_DONT_KEEP_OLD )
		{
			return true;
		}
	}
	return false;
}


void CCSTutor::UpdateCurrentMessage( TutorMessageEvent *event )
{
	TransferDeathEvents( m_currentMessageEvent, event );

	if ( m_currentMessageEvent != NULL )
	{
		DeleteEvent( m_currentMessageEvent );
		m_currentMessageEvent = NULL;
	}

	m_currentlyShownMessageID = (TutorMessageID)event->GetID();

	m_currentMessageEvent = event;

	TutorMessage *definition = GetTutorMessageDefinition( event->GetID() );
	if ( definition == NULL )
	{
		return;
	}

	CBasePlayer *localPlayer = UTIL_GetLocalPlayer();
	if ( localPlayer == NULL )
	{
		return;
	}

	m_currentlyShownMessageCloseTime = definition->m_duration + gpGlobals->time;

	if ( definition->m_keepOld == TUTORMESSAGEKEEPOLDTYPE_UPDATE_CONTENT )
	{
		CloseCurrentWindow();
		DisplayMessageToPlayer( localPlayer, event->GetID(), definition->m_text, event );
	}
}


void CCSTutor::ShowTutorMessage( TutorMessageEvent *event )
{
	TutorMessageID mid = (TutorMessageID)event->GetID();
	if ( (unsigned int)mid >= TUTOR_NUM_MESSAGES )
	{
		return;
	}

	TutorMessage *message = GetTutorMessageDefinition( mid );
	if ( message == NULL )
	{
		return;
	}

	m_currentlyShownMessageID = mid;
	m_currentMessageEvent = event;

	m_currentlyShownMessageCloseTime = 0;
	m_currentlyShownMessageMinimumCloseTime = 0;
}


void CCSTutor::ConstructMessageAndDisplay( void )
{
	CBasePlayer *pLocalPlayer = UTIL_GetLocalPlayer();

	if ( pLocalPlayer == NULL )
	{
		return;
	}
	if ( pLocalPlayer->IsBot() == TRUE )
	{
		return;
	}

	TutorMessageID mid = (TutorMessageID)m_currentMessageEvent->GetID();
	if ( (unsigned int)mid >= TUTOR_NUM_MESSAGES )
	{
		return;
	}
	TutorMessage *message = GetTutorMessageDefinition( mid );
	if ( message == NULL )
	{
		return;
	}

	message->m_timesShown++;
	ComputeDisplayTimesForMessage();

	DisplayMessageToPlayer( pLocalPlayer, mid, message->m_text, m_currentMessageEvent );
}


void CCSTutor::CallEventHandler( GameEventType event, CBaseEntity *entity, CBaseEntity *other )
{
	switch ( event )
	{
	case EVENT_WEAPON_FIRED:
		HandleWeaponFired( entity, other );
		break;

	case EVENT_WEAPON_FIRED_ON_EMPTY:
		HandleWeaponFiredOnEmpty( entity, other );
		break;

	case EVENT_WEAPON_RELOADED:
		HandleWeaponReloaded( entity, other );
		break;

	case EVENT_PLAYER_DIED:
		HandlePlayerDied( entity, other );
		break;

	case EVENT_PLAYER_SPAWNED:
		HandlePlayerSpawned( entity, other );
		break;

	case EVENT_PLAYER_LEFT_BUY_ZONE:
		HandlePlayerLeftBuyZone( entity, other );
		break;

	case EVENT_BOMB_PLANTED:
		HandleBombPlanted( entity, other );
		break;

	case EVENT_ROUND_START:
		HandleRoundStart( entity, other );
		break;

	case EVENT_BEING_SHOT_AT:
		HandleBeingShotAt( entity, other );
		break;

	case EVENT_PLAYER_TOOK_DAMAGE:
		HandlePlayerTookDamage( entity, other );
		break;

	case EVENT_PLAYER_BLINDED_BY_FLASHBANG:
		HandlePlayerBlindedByFlashbang( entity, other );
		break;

	case EVENT_BOMB_DEFUSING:
		HandleBombDefusing( entity, other );
		break;

	case EVENT_BOMB_DEFUSED:
		HandleBombDefused( entity, other );
		break;

	case EVENT_BOMB_EXPLODED:
		HandleBombExploded( entity, other );
		break;

	case EVENT_HOSTAGE_USED:
		HandleHostageUsed( entity, other );
		break;

	case EVENT_HOSTAGE_RESCUED:
		HandleHostageRescued( entity, other );
		break;

	case EVENT_ALL_HOSTAGES_RESCUED:
		HandleAllHostagesRescued( entity, other );
		break;

	case EVENT_HOSTAGE_DAMAGED:
		HandleHostageDamaged( entity, other );
		break;

	case EVENT_HOSTAGE_KILLED:
		HandleHostageKilled( entity, other );
		break;

	case EVENT_ROUND_DRAW:
		HandleRoundDraw( entity, other );
		break;

	case EVENT_CTS_WIN:
		HandleCTWin( entity, other );
		break;

	case EVENT_TERRORISTS_WIN:
		HandleTWin( entity, other );
		break;

	case EVENT_DEATH_CAMERA_START:
		HandleDeathCameraStart( entity, other );
		break;

	case EVENT_RADIO_COVER_ME:
		HandleRadioCoverMe( entity, other );
		break;

	case EVENT_RADIO_YOU_TAKE_THE_POINT:
		HandleRadioYouTakeThePoint( entity, other );
		break;

	case EVENT_RADIO_HOLD_THIS_POSITION:
		HandleRadioHoldThisPosition( entity, other );
		break;

	case EVENT_RADIO_REGROUP_TEAM:
		HandleRadioRegroupTeam( entity, other );
		break;

	case EVENT_RADIO_FOLLOW_ME:
		HandleRadioFollowMe( entity, other );
		break;

	case EVENT_RADIO_TAKING_FIRE:
		HandleRadioTakingFire( entity, other );
		break;

	case EVENT_RADIO_GO_GO_GO:
		HandleRadioGoGoGo( entity, other );
		break;

	case EVENT_RADIO_TEAM_FALL_BACK:
		HandleRadioTeamFallBack( entity, other );
		break;

	case EVENT_RADIO_STICK_TOGETHER_TEAM:
		HandleRadioStickTogetherTeam( entity, other );
		break;

	case EVENT_RADIO_GET_IN_POSITION_AND_WAIT:
		HandleRadioGetInPositionAndWait( entity, other );
		break;

	case EVENT_RADIO_STORM_THE_FRONT:
		HandleRadioStormTheFront( entity, other );
		break;

	case EVENT_RADIO_REPORT_IN_TEAM:
		HandleRadioReportInTeam( entity, other );
		break;

	case EVENT_RADIO_AFFIRMATIVE:
		HandleRadioAffirmative( entity, other );
		break;

	case EVENT_RADIO_ENEMY_SPOTTED:
		HandleRadioEnemySpotted( entity, other );
		break;

	case EVENT_RADIO_NEED_BACKUP:
		HandleRadioNeedBackup( entity, other );
		break;

	case EVENT_RADIO_SECTOR_CLEAR:
		HandleRadioSectorClear( entity, other );
		break;

	case EVENT_RADIO_IN_POSITION:
		HandleRadioInPosition( entity, other );
		break;

	case EVENT_RADIO_REPORTING_IN:
		HandleRadioReportingIn( entity, other );
		break;

	case EVENT_RADIO_GET_OUT_OF_THERE:
		HandleRadioGetOutOfThere( entity, other );
		break;

	case EVENT_RADIO_NEGATIVE:
		HandleRadioNegative( entity, other );
		break;

	case EVENT_RADIO_ENEMY_DOWN:
		HandleRadioEnemyDown( entity, other );
		break;

	case EVENT_TUTOR_BUY_MENU_OPENNED:
		HandleBuyMenuOpenned( entity, other );
		break;

	case EVENT_TUTOR_AUTOBUY:
		HandleAutoBuy( entity, other );
		break;

	case EVENT_TUTOR_NOT_BUYING_ANYTHING:
		HandleNotBuyingAnything( entity, other );
		break;

	case EVENT_TUTOR_NEED_TO_BUY_PRIMARY_WEAPON:
		HandleNeedToBuyPrimaryWeapon( entity, other );
		break;

	case EVENT_TUTOR_NEED_TO_BUY_PRIMARY_AMMO:
		HandleNeedToBuyPrimaryAmmo( entity, other );
		break;

	case EVENT_TUTOR_NEED_TO_BUY_SECONDARY_AMMO:
		HandleNeedToBuySecondaryAmmo( entity, other );
		break;

	case EVENT_TUTOR_NEED_TO_BUY_ARMOR:
		HandleNeedToBuyArmor( entity, other );
		break;

	case EVENT_TUTOR_NEED_TO_BUY_DEFUSE_KIT:
		HandleNeedToBuyDefuseKit( entity, other );
		break;

	case EVENT_TUTOR_NEED_TO_BUY_GRENADE:
		HandleNeedToBuyGrenade( entity, other );
		break;

	case EVENT_CAREER_TASK_DONE:
		HandleCareerTaskDone( entity, other );
		break;

	default:
		break;
	}
}


void CCSTutor::HandleWeaponFired( CBaseEntity *entity, CBaseEntity *other )
{
	CBasePlayer *player = (CBasePlayer *)entity;
	CBasePlayer *localPlayer = UTIL_GetLocalPlayer();

	if ( localPlayer == NULL || !localPlayer->IsAlive() )
	{
		return;
	}

	if ( player != localPlayer )
	{
		return;
	}

	CheckForNeedToReload( false );
}


void CCSTutor::HandleWeaponFiredOnEmpty( CBaseEntity *entity, CBaseEntity *other )
{
	CBasePlayer *localPlayer = UTIL_GetLocalPlayer();
	if ( localPlayer == NULL )
	{
		return;
	}

	if ( entity == NULL || !entity->IsPlayer() )
	{
		return;
	}

	CBasePlayer *player = (CBasePlayer *)entity;
	if ( player == localPlayer )
	{
		if ( localPlayer->m_pActiveItem->IsWeapon() == TRUE )
		{
			CBasePlayerWeapon *currentWeapon = (CBasePlayerWeapon *)localPlayer->m_pActiveItem;

			if ( localPlayer->m_rgAmmo[currentWeapon->m_iPrimaryAmmoType] <= 0 )
			{
				TutorMessage *message = GetTutorMessageDefinition( YOU_ARE_OUT_OF_AMMO );
				if ( message != NULL )
				{
					message->m_lastCloseTime = 0;
				}

				CreateAndAddEventToList( YOU_ARE_OUT_OF_AMMO, NULL, NULL );
			}
		}
	}
}


void CCSTutor::HandleWeaponReloaded( CBaseEntity *entity, CBaseEntity *other )
{
	if ( entity == NULL || !entity->IsPlayer() )
	{
		return;
	}

	CBasePlayer *player = (CBasePlayer *)entity;
	if ( player != UTIL_GetLocalPlayer() )
	{
		return;
	}

	CancelEvent( YOU_SHOULD_RELOAD );
}


void CCSTutor::HandlePlayerDied( CBaseEntity *entity, CBaseEntity *other )
{
	CBasePlayer *victim = NULL;
	CBasePlayer *killer = NULL;

	CBasePlayer *localPlayer = UTIL_GetLocalPlayer();
	if ( localPlayer == NULL )
	{
		return;
	}

	if ( entity->IsPlayer() )
	{
		victim = (CBasePlayer *)entity;
	}
	if ( other->IsPlayer() )
	{
		killer = (CBasePlayer *)other;
	}

	if ( victim == localPlayer && killer == NULL )
	{
		if ( localPlayer->m_bKilledByBomb )
		{
			CreateAndAddEventToList( YOU_DIED, entity, other );
		}
		else
		{
			CreateAndAddEventToList( YOU_FELL_TO_YOUR_DEATH, NULL, NULL );
		}
		return;
	}
	if ( victim == NULL || killer == NULL )
	{
		return;
	}

	if ( victim == killer && victim == localPlayer )
	{
		CreateAndAddEventToList( YOU_DIED, entity, other );
		return;
	}

	int numT, numCT;
	GetNumPlayersAliveOnTeams( numT, numCT );

	if ( localPlayer == killer )
	{
		if ( victim->m_iTeam == localPlayer->m_iTeam )
		{
			CreateAndAddEventToList( YOU_KILLED_A_TEAMMATE, entity, other );
			return;
		}

		if ( victim->m_bHeadshotKilled )
		{
			if ( localPlayer->m_iTeam == CT )
			{
				if ( numT == 0 )
				{
					CreateAndAddEventToList( YOU_KILLED_LAST_ENEMY_HEADSHOT, entity, other );
					return;
				}
				else if ( numT == 1 )
				{
					CreateAndAddEventToList( YOU_KILLED_PLAYER_HEADSHOT_ONE_LEFT, entity, other );
					return;
				}
				else
				{
					CreateAndAddEventToList( YOU_KILLED_PLAYER_HEADSHOT, entity, other );
					return;
				}
			}
			else if ( localPlayer->m_iTeam == TERRORIST )
			{
				if ( numCT == 0 )
				{
					CreateAndAddEventToList( YOU_KILLED_LAST_ENEMY_HEADSHOT, entity, other );
					return;
				}
				else if ( numCT == 1 )
				{
					CreateAndAddEventToList( YOU_KILLED_PLAYER_HEADSHOT_ONE_LEFT, entity, other );
					return;
				}
				else
				{
					CreateAndAddEventToList( YOU_KILLED_PLAYER_HEADSHOT, entity, other );
					return;
				}
			}
		}
		else
		{
			if ( localPlayer->m_iTeam == CT )
			{
				if ( numT == 0 )
				{
					CreateAndAddEventToList( YOU_KILLED_LAST_ENEMY, entity, other );
					return;
				}
				else if ( numT == 1 )
				{
					CreateAndAddEventToList( YOU_KILLED_PLAYER_ONE_LEFT, entity, other );
					return;
				}
				else
				{
					CreateAndAddEventToList( YOU_KILLED_PLAYER, entity, other );
					return;
				}
			}
			else if ( localPlayer->m_iTeam == TERRORIST )
			{
				if ( numCT == 0 )
				{
					CreateAndAddEventToList( YOU_KILLED_LAST_ENEMY, entity, other );
					return;
				}
				else if ( numCT == 1 )
				{
					CreateAndAddEventToList( YOU_KILLED_PLAYER_ONE_LEFT, entity, other );
					return;
				}
				else
				{
					CreateAndAddEventToList( YOU_KILLED_PLAYER, entity, other );
					return;
				}
			}
		}
	}

	if ( victim == localPlayer )
	{
		if ( victim->m_bHeadshotKilled )
		{
			CreateAndAddEventToList( YOU_DIED_HEADSHOT, entity, other );
		}
		else
		{
			CreateAndAddEventToList( YOU_DIED, entity, other );
		}

		return;
	}

	if ( localPlayer->m_iTeam == victim->m_iTeam )
	{
		if ( localPlayer->m_iTeam == CT )
		{
			if ( localPlayer->IsAlive() )
			{
				if ( numCT == 1 )
				{
					CreateAndAddEventToList( LAST_TEAMMATE_KILLED, entity, other );
					return;
				}
				else if ( numCT == 2 )
				{
					CreateAndAddEventToList( TEAMMATE_KILLED_ONE_LEFT, entity, other );
					return;
				}
				else
				{
					CreateAndAddEventToList( TEAMMATE_KILLED, entity, other );
					return;
				}
			}
			else
			{
				if ( numCT == 1 )
				{
					CreateAndAddEventToList( TEAMMATE_KILLED_ONE_LEFT, entity, other );
					return;
				}
				else if ( numCT > 1 )
				{
					CreateAndAddEventToList( TEAMMATE_KILLED, entity, other );
					return;
				}
			}
		}
		else if ( localPlayer->m_iTeam == TERRORIST )
		{
			if ( localPlayer->IsAlive() )
			{
				if ( numT == 1 )
				{
					CreateAndAddEventToList( LAST_TEAMMATE_KILLED, entity, other );
					return;
				}
				else if ( numT == 2 )
				{
					CreateAndAddEventToList( TEAMMATE_KILLED_ONE_LEFT, entity, other );
					return;
				}
				else
				{
					CreateAndAddEventToList( TEAMMATE_KILLED, entity, other );
					return;
				}
			}
			else
			{
				if ( numT == 1 )
				{
					CreateAndAddEventToList( TEAMMATE_KILLED_ONE_LEFT, entity, other );
					return;
				}
				else if ( numT > 1 )
				{
					CreateAndAddEventToList( TEAMMATE_KILLED, entity, other );
					return;
				}
			}
		}
	}
	else
	{
		if ( localPlayer->m_iTeam == CT )
		{
			if ( numT == 0 )
			{
				CreateAndAddEventToList( LAST_ENEMY_KILLED, entity, other );
				return;
			}
			else if ( numT == 1 )
			{
				CreateAndAddEventToList( ENEMY_KILLED_ONE_LEFT, entity, other );
				return;
			}
			else
			{
				CreateAndAddEventToList( ENEMY_KILLED, entity, other );
				return;
			}
		}
		else if ( localPlayer->m_iTeam == TERRORIST )
		{
			if ( numCT == 0 )
			{
				CreateAndAddEventToList( LAST_ENEMY_KILLED, entity, other );
				return;
			}
			else if ( numCT == 1 )
			{
				CreateAndAddEventToList( ENEMY_KILLED_ONE_LEFT, entity, other );
				return;
			}
			else
			{
				CreateAndAddEventToList( ENEMY_KILLED, entity, other );
				return;
			}
		}
	}
}


void CCSTutor::HandlePlayerTookDamage( CBaseEntity *entity, CBaseEntity *other )
{
	CBasePlayer *localPlayer = UTIL_GetLocalPlayer();
	if ( localPlayer == NULL )
	{
		return;
	}

	CBasePlayer *victim = NULL;
	CBasePlayer *attacker = NULL;

	if ( entity != NULL && entity->IsPlayer() == TRUE )
	{
		victim = (CBasePlayer *)entity;
	}

	if ( other != NULL && other->IsPlayer() == TRUE )
	{
		attacker = (CBasePlayer *)other;
	}

	if ( victim == localPlayer )
	{
		CreateAndAddEventToList( YOU_WERE_JUST_HURT, NULL, NULL );
	}
	else
	{
		if ( attacker == localPlayer && victim != NULL && localPlayer->m_iTeam == victim->m_iTeam )
		{
			CreateAndAddEventToList( YOU_ATTACKED_TEAMMATE, NULL, NULL );
		}
	}
}


void CCSTutor::HandlePlayerBlindedByFlashbang( CBaseEntity *entity, CBaseEntity *other )
{
	if ( entity == NULL || !entity->IsPlayer() )
	{
		return;
	}

	CBasePlayer *localPlayer = UTIL_GetLocalPlayer();

	CBasePlayer *player = (CBasePlayer *)entity;

	if ( localPlayer != NULL && player == localPlayer )
	{
		CreateAndAddEventToList( YOU_ARE_BLIND_FROM_FLASHBANG, NULL, NULL );
	}
}


void CCSTutor::HandlePlayerSpawned( CBaseEntity *entity, CBaseEntity *other )
{
	if ( entity->IsPlayer() != TRUE )
	{
		return;
	}

	CBasePlayer *player = (CBasePlayer *)entity;

	if ( player != UTIL_GetLocalPlayer() )
	{
		return;
	}

	m_haveSpawned = true;

	m_lastInGameHintShown = INGAME_HINT_BEGIN;

	CreateAndAddEventToList( YOU_SPAWNED, entity, other );
}


void CCSTutor::HandleClientCorpseSpawned( CBaseEntity *entity, CBaseEntity *other )
{
	if ( entity == NULL || !entity->IsPlayer() )
	{
		return;
	}

	CBasePlayer *player = (CBasePlayer *)entity;

	ClientCorpseStruct *corpse = new ClientCorpseStruct;

	corpse->m_position = player->pev->origin;
	corpse->m_team = player->m_iTeam;

	m_clientCorpseList.push_back( corpse );
}

void CCSTutor::HandleBuyMenuOpenned( CBaseEntity *entity, CBaseEntity *other )
{
	if ( m_currentlyShownMessageID == BUY_TIME_BEGIN )
	{
		ClearCurrentEvent( true, true );

		CheckBuyZoneMessages();
	}
}

void CCSTutor::HandleAutoBuy( CBaseEntity *entity, CBaseEntity *other )
{
	if ( m_currentlyShownMessageID == BUY_TIME_BEGIN )
	{
		ClearCurrentEvent( true, true );
	}
}


void CCSTutor::HandleBuyTimeStart( CBaseEntity *entity, CBaseEntity *other )
{
}


void CCSTutor::HandlePlayerLeftBuyZone( CBaseEntity *entity, CBaseEntity *other )
{
	m_messageTypeMask = TUTORMESSAGETYPE_ALL & ~(TUTORMESSAGETYPE_BUY | TUTORMESSAGETYPE_HINT);

	ClearEventList();
	ClearCurrentEvent( true, true );
	if ( m_lastScenarioEvent != NULL )
	{
		m_lastScenarioEvent->SetActivationTime( gpGlobals->time );
		AddToEventList( m_lastScenarioEvent );

		m_lastScenarioEvent = NULL;
	}
}


void CCSTutor::HandleBombPlanted( CBaseEntity *entity, CBaseEntity *other )
{
	CBasePlayer *player = UTIL_GetLocalPlayer();
	if ( player == NULL )
	{
		return;
	}

	if ( player->IsAlive() == TRUE && player->m_iTeam == CT )
	{
		CreateAndAddEventToList( BOMB_PLANTED_CT, entity, other );
	}
	else
	{
		CreateAndAddEventToList( BOMB_PLANTED_T, entity, other );
	}
}


void CCSTutor::HandleBombDefused( CBaseEntity *entity, CBaseEntity *other )
{
	CBasePlayer *localPlayer = UTIL_GetLocalPlayer();
	if ( localPlayer == NULL )
	{
		return;
	}

	CBasePlayer *defuser = NULL;
	if ( entity != NULL )
	{
		if ( entity->IsPlayer() )
		{
			defuser = (CBasePlayer *)entity;
		}
	}

	if ( defuser == localPlayer )
	{
		CreateAndAddEventToList( YOU_DEFUSED_BOMB, NULL, NULL );
	}
	else if ( localPlayer->m_iTeam == CT )
	{
		CreateAndAddEventToList( BOMB_DEFUSED_CT, NULL, NULL );
	}
	else if ( localPlayer->m_iTeam == TERRORIST )
	{
		CreateAndAddEventToList( BOMB_DEFUSED_T, NULL, NULL );
	}
	else
	{
		CreateAndAddEventToList( ROUND_OVER, NULL, NULL );
	}
}

void CCSTutor::HandleBombDefusing( CBaseEntity *entity, CBaseEntity *other )
{
	CBasePlayer *localPlayer = UTIL_GetLocalPlayer();
	if ( localPlayer == NULL )
	{
		return;
	}

	if ( entity == NULL || !entity->IsPlayer() )
	{
		return;
	}

	CBasePlayer *player = (CBasePlayer *)entity;
	if ( player == localPlayer )
	{
		if ( !localPlayer->m_bHasDefuser )
		{
			CreateAndAddEventToList( DEFUSING_WITHOUT_KIT, NULL, NULL );
		}
	}
}


void CCSTutor::HandleBombExploded( CBaseEntity *entity, CBaseEntity *other )
{
	CBasePlayer *localPlayer = UTIL_GetLocalPlayer();
	if ( localPlayer == NULL )
	{
		return;
	}

	if ( localPlayer->m_iTeam == CT )
	{
		CreateAndAddEventToList( BOMB_EXPLODED_CT, NULL, NULL );
	}
	else if ( localPlayer->m_iTeam == TERRORIST )
	{
		CreateAndAddEventToList( BOMB_EXPLODED_T, NULL, NULL );
	}
}


void CCSTutor::HandleRoundStart( CBaseEntity *entity, CBaseEntity *other )
{
	m_roundStartTime = gpGlobals->time;
	CBasePlayer *player = UTIL_GetLocalPlayer();
	if ( player == NULL )
	{
		return;
	}

	if ( IsBombMap() )
	{
		if ( player->m_iTeam == CT )
		{
			CreateAndAddEventToList( ROUND_START_DE_CT, NULL, NULL );
		}
		else if ( player->m_iTeam == TERRORIST )
		{
			if ( player->m_bHasC4 )
			{
				CreateAndAddEventToList( YOU_ARE_BOMB_CARRIER, entity, other );
			}
			else
			{
				CreateAndAddEventToList( ROUND_START_DE_T, NULL, NULL );
			}
		}
	}
	else if ( IsHostageMap() )
	{
		if ( player->m_iTeam == CT )
		{
			CreateAndAddEventToList( ROUND_START_CS_CT, NULL, NULL );
		}
		else if ( player->m_iTeam == TERRORIST )
		{
			CreateAndAddEventToList( ROUND_START_CS_T, NULL, NULL );
		}
	}
}


void CCSTutor::HandleBeingShotAt( CBaseEntity *entity, CBaseEntity *other )
{
	if ( entity == NULL || !entity->IsPlayer() )
	{
		return;
	}

	CBasePlayer *player = (CBasePlayer *)entity;
	CBasePlayer *localPlayer = UTIL_GetLocalPlayer();

	if ( localPlayer == NULL || player != localPlayer )
	{
		return;
	}

	if ( !localPlayer->IsAlive() )
	{
		return;
	}

	CreateAndAddEventToList( YOU_HAVE_BEEN_SHOT_AT, entity, other );
}


void CCSTutor::HandleHostageUsed( CBaseEntity *entity, CBaseEntity *other )
{
	CBasePlayer *localPlayer = UTIL_GetLocalPlayer();
	if ( localPlayer == NULL )
	{
		return;
	}

	if ( entity == NULL || !entity->IsPlayer() )
	{
		return;
	}

	bool unusedHostages = !CheckForAllHostagesFollowingSomeone();

	CBasePlayer *activator = (CBasePlayer *)entity;

	if ( activator == localPlayer )
	{
		if ( unusedHostages )
		{
			CreateAndAddEventToList( YOU_USED_HOSTAGE_MORE_LEFT, NULL, NULL );
		}
		else
		{
			CreateAndAddEventToList( YOU_USED_HOSTAGE_NO_MORE_LEFT, NULL, NULL );
		}
	}
	else if ( !unusedHostages )
	{
		if ( localPlayer->m_iTeam == CT )
		{
			CreateAndAddEventToList( ALL_HOSTAGES_FOLLOWING_CT, NULL, NULL );
		}
		else if ( localPlayer->m_iTeam == TERRORIST )
		{
			CreateAndAddEventToList( ALL_HOSTAGES_FOLLOWING_T, NULL, NULL );
		}
	}
}


void CCSTutor::HandleHostageRescued( CBaseEntity *entity, CBaseEntity *other )
{
	CBasePlayer *localPlayer = UTIL_GetLocalPlayer();
	if ( localPlayer == NULL )
	{
		return;
	}

	if ( entity == NULL || !entity->IsPlayer() )
	{
		return;
	}

	CBasePlayer *rescuer = (CBasePlayer *)entity;

	if ( localPlayer->m_iTeam == TERRORIST )
	{
		CreateAndAddEventToList( HOSTAGE_RESCUED_T, NULL, NULL );
	}
	else if ( localPlayer->m_iTeam == CT )
	{
		if ( rescuer == localPlayer )
		{
			CreateAndAddEventToList( YOU_RESCUED_HOSTAGE, NULL, NULL );
		}
		else
		{
			CreateAndAddEventToList( HOSTAGE_RESCUED_CT, NULL, NULL );
		}
	}
}

void CCSTutor::HandleAllHostagesRescued( CBaseEntity *entity, CBaseEntity *other )
{
	CBasePlayer *localPlayer = UTIL_GetLocalPlayer();
	if ( localPlayer == NULL )
	{
		return;
	}

	if ( localPlayer->m_iTeam == CT )
	{
		CreateAndAddEventToList( ALL_HOSTAGES_RESCUED_CT, NULL, NULL );
	}
	else if ( localPlayer->m_iTeam == TERRORIST )
	{
		CreateAndAddEventToList( ALL_HOSTAGES_RESCUED_T, NULL, NULL );
	}
}


void CCSTutor::HandleHostageDamaged( CBaseEntity *entity, CBaseEntity *other )
{
	CBasePlayer *localPlayer = UTIL_GetLocalPlayer();

	if ( localPlayer == NULL || entity == NULL )
	{
		return;
	}

	if ( other == NULL || !other->IsPlayer() )
	{
		return;
	}

	CBasePlayer *attacker = (CBasePlayer *)other;
	if ( attacker == localPlayer )
	{
		CreateAndAddEventToList( YOU_DAMAGED_HOSTAGE, NULL, NULL );
	}
}


void CCSTutor::HandleHostageKilled( CBaseEntity *entity, CBaseEntity *other )
{
	CBasePlayer *localPlayer = UTIL_GetLocalPlayer();
	if ( localPlayer == NULL )
	{
		return;
	}

	CheckForAllHostagesDead();

	if ( entity == NULL )
	{
		return;
	}

	if ( other == NULL || !other->IsPlayer() )
	{
		return;
	}

	CBasePlayer *attacker = (CBasePlayer *)other;
	if ( attacker == localPlayer )
	{
		CreateAndAddEventToList( YOU_KILLED_HOSTAGE, NULL, NULL );
	}

	bool unusedHostages = !CheckForAllHostagesFollowingSomeone();

	if ( !unusedHostages )
	{
		if ( localPlayer->m_iTeam == CT )
		{
			CreateAndAddEventToList( ALL_HOSTAGES_FOLLOWING_CT, NULL, NULL );
		}
		else if ( localPlayer->m_iTeam == TERRORIST )
		{
			CreateAndAddEventToList( ALL_HOSTAGES_FOLLOWING_T, NULL, NULL );
		}
	}
}


void CCSTutor::HandleRoundDraw( CBaseEntity *entity, CBaseEntity *other )
{
	CHalfLifeMultiplay *mp = (CHalfLifeMultiplay *)g_pGameRules;
	if ( mp->m_iTotalRoundsPlayed != 0 )
	{
		CreateAndAddEventToList( ROUND_DRAW, NULL, NULL );
	}

	ResetPlayerDeathInfo();
}


void CCSTutor::HandleCTWin( CBaseEntity *entith, CBaseEntity *other )
{
	CreateAndAddEventToList( CT_WIN, NULL, NULL );

	ResetPlayerDeathInfo();
}


void CCSTutor::HandleTWin( CBaseEntity *entity, CBaseEntity *other )
{
	CreateAndAddEventToList( T_WIN, NULL, NULL );

	ResetPlayerDeathInfo();
}


void CCSTutor::HandleDeathCameraStart( CBaseEntity *entity, CBaseEntity *other )
{
	CBasePlayer *localPlayer = UTIL_GetLocalPlayer();
	if ( localPlayer == NULL )
	{
		return;
	}

	if ( entity == NULL || !entity->IsPlayer() )
	{
		return;
	}

	CBasePlayer *player = (CBasePlayer *)entity;

	if ( player == localPlayer )
	{
		m_messageTypeMask = TUTORMESSAGETYPE_END_GAME | TUTORMESSAGETYPE_HINT |
			TUTORMESSAGETYPE_ENEMY_DEATH | TUTORMESSAGETYPE_FRIEND_DEATH;

		CreateAndAddEventToList( DEATH_CAMERA_START, NULL, NULL );
	}
}


void CCSTutor::HandleRadioCoverMe( CBaseEntity *entity, CBaseEntity *other )
{
	CreateAndAddEventToList( RADIO_COVER_ME, entity, other );
}

void CCSTutor::HandleRadioYouTakeThePoint( CBaseEntity *entity, CBaseEntity *other )
{
	CreateAndAddEventToList( RADIO_YOU_TAKE_THE_POINT, entity, other );
}

void CCSTutor::HandleRadioHoldThisPosition( CBaseEntity *entity, CBaseEntity *other )
{
	CreateAndAddEventToList( RADIO_HOLD_THIS_POSITION, entity, other );
}

void CCSTutor::HandleRadioRegroupTeam( CBaseEntity *entity, CBaseEntity *other )
{
	CreateAndAddEventToList( RADIO_REGROUP_TEAM, entity, other );
}

void CCSTutor::HandleRadioFollowMe( CBaseEntity *entity, CBaseEntity *other )
{
	CreateAndAddEventToList( RADIO_FOLLOW_ME, entity, other );
}

void CCSTutor::HandleRadioTakingFire( CBaseEntity *entity, CBaseEntity *other )
{
	CreateAndAddEventToList( RADIO_TAKING_FIRE, entity, other );
}

void CCSTutor::HandleRadioGoGoGo( CBaseEntity *entity, CBaseEntity *other )
{
	CreateAndAddEventToList( RADIO_GO_GO_GO, entity, other );
}

void CCSTutor::HandleRadioTeamFallBack( CBaseEntity *entity, CBaseEntity *other )
{
	CreateAndAddEventToList( RADIO_TEAM_FALL_BACK, entity, other );
}

void CCSTutor::HandleRadioStickTogetherTeam( CBaseEntity *entity, CBaseEntity *other )
{
	CreateAndAddEventToList( RADIO_STICK_TOGETHER_TEAM, entity, other );
}

void CCSTutor::HandleRadioGetInPositionAndWait( CBaseEntity *entity, CBaseEntity *other )
{
	CreateAndAddEventToList( RADIO_GET_IN_POSITION_AND_WAIT, entity, other );
}

void CCSTutor::HandleRadioStormTheFront( CBaseEntity *entity, CBaseEntity *other )
{
	CreateAndAddEventToList( RADIO_STORM_THE_FRONT, entity, other );
}

void CCSTutor::HandleRadioReportInTeam( CBaseEntity *entity, CBaseEntity *other )
{
	CreateAndAddEventToList( RADIO_REPORT_IN_TEAM, entity, other );
}

void CCSTutor::HandleRadioAffirmative( CBaseEntity *entity, CBaseEntity *other )
{
	CreateAndAddEventToList( RADIO_AFFIRMATIVE, entity, other );
}

void CCSTutor::HandleRadioEnemySpotted( CBaseEntity *entity, CBaseEntity *other )
{
	CreateAndAddEventToList( RADIO_ENEMY_SPOTTED, entity, other );
}

void CCSTutor::HandleRadioNeedBackup( CBaseEntity *entity, CBaseEntity *other )
{
	CreateAndAddEventToList( RADIO_NEED_BACKUP, entity, other );
}

void CCSTutor::HandleRadioSectorClear( CBaseEntity *entity, CBaseEntity *other )
{
	CreateAndAddEventToList( RADIO_SECTOR_CLEAR, entity, other );
}

void CCSTutor::HandleRadioInPosition( CBaseEntity *entity, CBaseEntity *other )
{
	CreateAndAddEventToList( RADIO_IN_POSITION, entity, other );
}

void CCSTutor::HandleRadioReportingIn( CBaseEntity *entity, CBaseEntity *other )
{
	CreateAndAddEventToList( RADIO_REPORTING_IN, entity, other );
}

void CCSTutor::HandleRadioGetOutOfThere( CBaseEntity *entity, CBaseEntity *other )
{
	CreateAndAddEventToList( RADIO_GET_OUT_OF_THERE, entity, other );
}

void CCSTutor::HandleRadioNegative( CBaseEntity *entity, CBaseEntity *other )
{
	CreateAndAddEventToList( RADIO_NEGATIVE, entity, other );
}

void CCSTutor::HandleRadioEnemyDown( CBaseEntity *entity, CBaseEntity *other )
{
	CreateAndAddEventToList( RADIO_ENEMY_DOWN, entity, other );
}

void CCSTutor::HandleNotBuyingAnything( CBaseEntity *entity, CBaseEntity *other )
{
	CreateAndAddEventToList( BUY_TIME_BEGIN, entity, other );
}

void CCSTutor::HandleNeedToBuyPrimaryWeapon( CBaseEntity *entity, CBaseEntity *other )
{
	CreateAndAddEventToList( BUY_NEED_PRIMARY, entity, other );
}

void CCSTutor::HandleNeedToBuyPrimaryAmmo( CBaseEntity *entity, CBaseEntity *other )
{
	CreateAndAddEventToList( BUY_NEED_PRIMARY_AMMO, entity, other );
}

void CCSTutor::HandleNeedToBuySecondaryAmmo( CBaseEntity *entity, CBaseEntity *other )
{
	CreateAndAddEventToList( BUY_NEED_SECONDARY_AMMO, entity, other );
}

void CCSTutor::HandleNeedToBuyArmor( CBaseEntity *entity, CBaseEntity *other )
{
	CreateAndAddEventToList( BUY_NEED_ARMOR, entity, other );
}

void CCSTutor::HandleNeedToBuyDefuseKit( CBaseEntity *entity, CBaseEntity *other )
{
	CreateAndAddEventToList( BUY_NEED_DEFUSE_KIT, entity, other );
}

void CCSTutor::HandleNeedToBuyGrenade( CBaseEntity *entity, CBaseEntity *other )
{
	CreateAndAddEventToList( BUY_NEED_GRENADE, entity, other );
}

void CCSTutor::HandleCareerTaskDone( CBaseEntity *entity, CBaseEntity *other )
{
	int numTasksRemaining = 0;
	if ( TheCareerTasks != NULL )
	{
		numTasksRemaining = TheCareerTasks->GetNumRemainingTasks();
	}

	if ( numTasksRemaining <= 0 )
	{
		CreateAndAddEventToList( CAREER_TASK_DONE_ALL_DONE, entity, other );
	}
	else if ( numTasksRemaining == 1 )
	{
		CreateAndAddEventToList( CAREER_TASK_DONE_ONE_LEFT, entity, other );
	}
	else
	{
		CreateAndAddEventToList( CAREER_TASK_DONE_MORE_LEFT, entity, other );
	}
}


void CCSTutor::HandleShotFired( Vector source, Vector target )
{
	CBasePlayer *localPlayer = UTIL_GetLocalPlayer();

	if ( localPlayer == NULL )
	{
		return;
	}

	float d1 = ( source - localPlayer->pev->origin ).Length();
	if ( d1 <= 32.0f )
	{
		return;
	}

	float d = 0.0f;
	float angle = 0.0f;
	float FiringLineDistanceToPlayer = 0.0f;

	d = ( target - source ).Length();

	angle = (float)acos( (double)(d1 / d) );

	FiringLineDistanceToPlayer = (float)(sin( (double)angle ) * d1);

	if ( FiringLineDistanceToPlayer <= 300.0f )
	{
		OnEvent( EVENT_BEING_SHOT_AT, localPlayer, NULL );
	}
}


void CCSTutor::GetNumPlayersAliveOnTeams( int &numT, int &numCT )
{
	numT = 0;
	numCT = 0;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *player = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( player == NULL )
		{
			continue;
		}
		if ( !player->IsAlive() )
		{
			continue;
		}
		if ( player->m_iTeam == CT )
		{
			numCT++;
		}
		else if ( player->m_iTeam == TERRORIST )
		{
			numT++;
		}
	}
}


void CCSTutor::CheckForLooseWeaponViewable( void )
{
	CBasePlayer *localPlayer = UTIL_GetLocalPlayer();
	if ( localPlayer == NULL )
	{
		return;
	}

	CBaseEntity *weapon = NULL;
	bool sawFirst = false;

	TutorMessage *message = GetTutorMessageDefinition( YOU_SEE_LOOSE_WEAPON );
	if ( message != NULL && message->m_class == TUTORMESSAGECLASS_EXAMINE )
	{
		return;
	}

	weapon = UTIL_FindEntityByClassname( NULL, "weaponbox" );
	while ( weapon != NULL )
	{
		if ( IsEntityInViewOfPlayer( weapon, localPlayer ) )
		{
			if ( !sawFirst )
			{
				sawFirst = true;
				CreateAndAddEventToList( YOU_SEE_LOOSE_WEAPON, NULL, NULL );
			}
		}

		weapon = UTIL_FindEntityByClassname( weapon, "weaponbox" );
	}
}


void CCSTutor::CheckForLooseDefuserViewable( void )
{
	CBasePlayer *localPlayer = UTIL_GetLocalPlayer();
	if ( localPlayer == NULL || localPlayer->m_iTeam != CT )
	{
		return;
	}

	TutorMessage *message = GetTutorMessageDefinition( YOU_SEE_LOOSE_DEFUSER );
	if ( message != NULL && message->m_class == TUTORMESSAGECLASS_EXAMINE )
	{
		return;
	}

	CBaseEntity *defuser = NULL;
	bool sawFirst = false;

	defuser = UTIL_FindEntityByClassname( NULL, "item_thighpack" );
	while ( defuser != NULL )
	{
		if ( IsEntityInViewOfPlayer( defuser, localPlayer ) )
		{
			if ( !sawFirst )
			{
				sawFirst = true;
				CreateAndAddEventToList( YOU_SEE_LOOSE_DEFUSER, NULL, NULL );
			}
		}

		defuser = UTIL_FindEntityByClassname( defuser, "item_thighpack" );
	}
}


void CCSTutor::CheckForBombViewable( void )
{
	CBasePlayer *localPlayer = UTIL_GetLocalPlayer();
	if ( localPlayer == NULL )
	{
		return;
	}

	CBaseEntity *bomb = UTIL_FindEntityByClassname( NULL, "grenade" );
	if ( bomb != NULL && ((CGrenade *)bomb)->m_bIsC4 )
	{
		if ( IsEntityInViewOfPlayer( bomb, localPlayer ) )
		{
			if ( localPlayer->m_iTeam == CT )
			{
				TutorMessage *message = GetTutorMessageDefinition( YOU_SEE_PLANTED_BOMB_CT );
				if ( message != NULL && message->m_class == TUTORMESSAGECLASS_EXAMINE )
				{
					return;
				}

				CreateAndAddEventToList( YOU_SEE_PLANTED_BOMB_CT, NULL, NULL );
			}
			else if ( localPlayer->m_iTeam == TERRORIST )
			{
				TutorMessage *message = GetTutorMessageDefinition( YOU_SEE_PLANTED_BOMB_T );
				if ( message != NULL && message->m_class == TUTORMESSAGECLASS_EXAMINE )
				{
					return;
				}

				CreateAndAddEventToList( YOU_SEE_PLANTED_BOMB_T, NULL, NULL );
			}

			return;
		}
	}
	CBasePlayer *bombCarrier = NULL;

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *player = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( player != NULL && player->m_bHasC4 )
		{
			bombCarrier = player;
			break;
		}
	}

	bomb = UTIL_FindEntityByClassname( NULL, "weapon_c4" );
	if ( bomb == NULL )
	{
		return;
	}

	if ( !IsEntityInViewOfPlayer( bomb, localPlayer ) )
	{
		return;
	}

	if ( bombCarrier == NULL )
	{
		if ( localPlayer->m_iTeam == CT )
		{
			TutorMessage *message = GetTutorMessageDefinition( YOU_SEE_LOOSE_BOMB_CT );
			if ( message != NULL && message->m_class == TUTORMESSAGECLASS_EXAMINE )
			{
				return;
			}

			CreateAndAddEventToList( YOU_SEE_LOOSE_BOMB_CT, NULL, NULL );
		}
		else if ( localPlayer->m_iTeam == TERRORIST )
		{
			TutorMessage *message = GetTutorMessageDefinition( YOU_SEE_LOOSE_BOMB_T );
			if ( message != NULL && message->m_class == TUTORMESSAGECLASS_EXAMINE )
			{
				return;
			}

			CreateAndAddEventToList( YOU_SEE_LOOSE_BOMB_T, NULL, NULL );
		}
	}
	else
	{
		if ( localPlayer != bombCarrier )
		{
			if ( localPlayer->m_iTeam == CT )
			{
				TutorMessage *message = GetTutorMessageDefinition( YOU_SEE_BOMB_CARRIER_CT );
				if ( message != NULL && message->m_class == TUTORMESSAGECLASS_EXAMINE )
				{
					return;
				}

				CreateAndAddEventToList( YOU_SEE_BOMB_CARRIER_CT, NULL, NULL );
			}
			else if ( localPlayer->m_iTeam == TERRORIST )
			{
				TutorMessage *message = GetTutorMessageDefinition( YOU_SEE_BOMB_CARRIER_T );
				if ( message != NULL && message->m_class == TUTORMESSAGECLASS_EXAMINE )
				{
					return;
				}

				CreateAndAddEventToList( YOU_SEE_BOMB_CARRIER_T, NULL, NULL );
			}
		}
	}
}

void CCSTutor::CheckForBombsiteViewable( void )
{
	CBasePlayer *localPlayer = UTIL_GetLocalPlayer();
	if ( localPlayer == NULL )
	{
		return;
	}

	TutorMessageID mid = CheckForInBombZone();

	if ( mid != TUTOR_NUM_MESSAGES )
	{
		CreateAndAddEventToList( mid, NULL, NULL );
		return;
	}

	if ( localPlayer->m_iTeam == CT )
	{
		mid = YOU_SEE_BOMBSITE_CT;
	}
	else if ( localPlayer->m_iTeam == TERRORIST )
	{
		if ( localPlayer->m_bHasC4 )
		{
			mid = YOU_SEE_BOMBSITE_T_BOMB;
		}
		else if ( TheBots == NULL || !TheCSBots()->IsBombPlanted() )
		{
			mid = YOU_SEE_BOMBSITE_T;
		}
	}

	TutorMessage *definition = GetTutorMessageDefinition( mid );
	if ( definition != NULL && definition->m_class == TUTORMESSAGECLASS_EXAMINE )
	{
		return;
	}

	CBaseEntity *bombTarget = NULL;

	bool sawFirst = false;

	bombTarget = UTIL_FindEntityByClassname( NULL, "func_bomb_target" );
	while ( bombTarget != NULL && !sawFirst )
	{
		if ( IsBombsiteInViewOfPlayer( bombTarget, localPlayer ) )
		{
			sawFirst = true;
			CreateAndAddEventToList( mid, NULL, NULL );
		}

		bombTarget = UTIL_FindEntityByClassname( bombTarget, "func_bomb_target" );
	}

	bombTarget = UTIL_FindEntityByClassname( NULL, "info_bomb_target" );
	while ( bombTarget != NULL && !sawFirst )
	{
		if ( IsBombsiteInViewOfPlayer( bombTarget, localPlayer ) )
		{
			sawFirst = true;
			CreateAndAddEventToList( mid, NULL, NULL );
		}
		bombTarget = UTIL_FindEntityByClassname( bombTarget, "info_bomb_target" );
	}
}

TutorMessageID CCSTutor::CheckForInBombZone( void )
{
	CBasePlayer *localPlayer = UTIL_GetLocalPlayer();
	if ( localPlayer == NULL )
	{
		return TUTOR_NUM_MESSAGES;
	}

	TutorMessageID mid = TUTOR_NUM_MESSAGES;
	if ( localPlayer->m_iTeam == CT )
	{
		mid = YOU_SEE_PLANTED_BOMB_CT;
	}
	else
	{
		return TUTOR_NUM_MESSAGES;
	}

	CBaseEntity *bombTarget = NULL;

	bool sawFirst = false;

	bombTarget = UTIL_FindEntityByClassname( NULL, "func_bomb_target" );
	while ( bombTarget != NULL && !sawFirst )
	{
		if ( IsEntityInBombsite( bombTarget, localPlayer ) )
		{
			if ( IsBombPlantedInBombsite( bombTarget ) )
			{
				return mid;
			}
		}

		bombTarget = UTIL_FindEntityByClassname( bombTarget, "func_bomb_target" );
	}

	bombTarget = UTIL_FindEntityByClassname( NULL, "info_bomb_target" );
	while ( bombTarget != NULL && !sawFirst )
	{
		if ( IsEntityInBombsite( bombTarget, localPlayer ) )
		{
			if ( IsBombPlantedInBombsite( bombTarget ) )
			{
				return mid;
			}
		}
		bombTarget = UTIL_FindEntityByClassname( bombTarget, "info_bomb_target" );
	}

	return TUTOR_NUM_MESSAGES;
}

bool CCSTutor::IsBombPlantedInBombsite( CBaseEntity *bombTarget )
{
	CGrenade *bomb = (CGrenade *)UTIL_FindEntityByClassname( NULL, "grenade" );
	while ( bomb != NULL && !bomb->m_bIsC4 )
	{
		bomb = (CGrenade *)UTIL_FindEntityByClassname( bomb, "grenade" );
	}

	if ( bomb == NULL )
	{
		return false;
	}

	if ( IsEntityInBombsite( bombTarget, bomb ) )
	{
		return true;
	}
	return false;
}


void CCSTutor::CheckForHostageViewable( void )
{
	CBasePlayer *localPlayer = UTIL_GetLocalPlayer();
	if ( localPlayer == NULL )
	{
		return;
	}

	CBaseEntity *hostageEntity = NULL;
	bool sawFirst = false;

	hostageEntity = UTIL_FindEntityByClassname( NULL, "hostage_entity" );
	while ( hostageEntity != NULL )
	{
		bool validHostage = true;
		CHostage *hostage = (CHostage *)hostageEntity;
		if ( hostage->IsFollowingSomeone() )
		{
			validHostage = false;
		}
		else if ( hostage->pev->takedamage != DAMAGE_YES )
		{
			validHostage = false;
		}

		if ( validHostage && IsEntityInViewOfPlayer( hostageEntity, localPlayer ) )
		{
			if ( !sawFirst )
			{
				sawFirst = true;
				if ( localPlayer->m_iTeam == CT )
				{
					TutorMessage *message = GetTutorMessageDefinition( YOU_SEE_HOSTAGE_CT );
					if ( message != NULL && message->m_class == TUTORMESSAGECLASS_EXAMINE )
					{
						return;
					}

					CreateAndAddEventToList( YOU_SEE_HOSTAGE_CT, NULL, NULL );
				}
				else if ( localPlayer->m_iTeam == TERRORIST )
				{
					TutorMessage *message = GetTutorMessageDefinition( YOU_SEE_HOSTAGE_T );
					if ( message != NULL && message->m_class == TUTORMESSAGECLASS_EXAMINE )
					{
						return;
					}

					CreateAndAddEventToList( YOU_SEE_HOSTAGE_T, NULL, NULL );
				}
			}
		}

		hostageEntity = UTIL_FindEntityByClassname( hostageEntity, "hostage_entity" );
	}
}


void CCSTutor::CheckForTimeRunningOut( void )
{
	CBasePlayer *localPlayer = UTIL_GetLocalPlayer();
	if ( localPlayer == NULL )
	{
		return;
	}

	CHalfLifeMultiplay *mpRules = (CHalfLifeMultiplay *)g_pGameRules;

	if ( mpRules->m_bFreezePeriod )
	{
		return;
	}

	if ( mpRules->TimeRemaining() <= 30.0f )
	{
		if ( mpRules->m_bMapHasBombTarget )
		{
			if ( localPlayer->m_iTeam == TERRORIST )
			{
				CreateAndAddEventToList( TIME_RUNNING_OUT_DE_T, NULL, NULL );
			}
			else if ( localPlayer->m_iTeam == CT )
			{
				CreateAndAddEventToList( TIME_RUNNING_OUT_DE_CT, NULL, NULL );
			}
		}
		else if ( IsHostageMap() )
		{
			if ( localPlayer->m_iTeam == TERRORIST )
			{
				CreateAndAddEventToList( TIME_RUNNING_OUT_CS_T, NULL, NULL );
			}
			else if ( localPlayer->m_iTeam == CT )
			{
				CreateAndAddEventToList( TIME_RUNNING_OUT_CS_CT, NULL, NULL );
			}
		}
	}
}

void CCSTutor::CheckForAllHostagesDead( void )
{
	bool foundLiveOne = false;

	CBaseEntity *hostage = NULL;
	hostage = UTIL_FindEntityByClassname( NULL, "hostage_entity" );
	while ( hostage != NULL && !foundLiveOne )
	{
		if ( hostage->IsAlive() )
		{
			foundLiveOne = true;
		}

		hostage = UTIL_FindEntityByClassname( hostage, "hostage_entity" );
	}

	if ( !foundLiveOne )
	{
		CreateAndAddEventToList( ALL_HOSTAGES_DEAD, NULL, NULL );
	}
}

bool CCSTutor::CheckForAllHostagesFollowingSomeone( void )
{
	bool foundUnusedOne = false;

	CHostage *hostage = NULL;
	hostage = (CHostage *)UTIL_FindEntityByClassname( NULL, "hostage_entity" );
	while ( hostage != NULL && !foundUnusedOne )
	{
		if ( hostage->IsAlive() && !hostage->IsFollowingSomeone() )
		{
			foundUnusedOne = true;
		}

		hostage = (CHostage *)UTIL_FindEntityByClassname( hostage, "hostage_entity" );
	}

	if ( foundUnusedOne )
	{
		return false;
	}

	return true;
}

TutorMessage *CCSTutor::GetTutorMessageDefinition( int messageID )
{
	if ( (unsigned int)messageID >= TUTOR_NUM_MESSAGES )
	{
		return NULL;
	}

	TutorMessageMapIter iter = m_messageMap.find( TutorIdentifierList[messageID] );

	if ( iter == m_messageMap.end() )
	{
		return NULL;
	}

	return iter->second;
}

CBaseEntity *CCSTutor::GetEntityForMessageID( int messageID, CBaseEntity *last )
{
	CBaseEntity *ret = NULL;
	switch ( messageID )
	{
	case YOU_SEE_LOOSE_WEAPON:
		ret = UTIL_FindEntityByClassname( last, "weaponbox" );
		break;

	case YOU_SEE_LOOSE_DEFUSER:
		ret = UTIL_FindEntityByClassname( last, "item_thighpack" );
		break;

	case YOU_SEE_HOSTAGE_T:
	case YOU_SEE_HOSTAGE_CT:
	case YOU_SEE_HOSTAGE_CT_EXAMINE:
		ret = UTIL_FindEntityByClassname( last, "hostage_entity" );
		break;

	case YOU_SEE_LOOSE_BOMB_T:
	case YOU_SEE_LOOSE_BOMB_CT:
	case YOU_SEE_BOMB_CARRIER_T:
	case YOU_SEE_BOMB_CARRIER_CT:
		ret = UTIL_FindEntityByClassname( last, "weapon_c4" );
		break;

	case YOU_SEE_PLANTED_BOMB_T:
	case YOU_SEE_PLANTED_BOMB_CT:
		ret = UTIL_FindEntityByClassname( last, "grenade" );
		break;

	case YOU_SEE_FRIEND:
	case YOU_SEE_ENEMY:
		ret = UTIL_FindEntityByClassname( last, "player" );
		break;

	case YOU_SEE_FRIEND_CORPSE:
	{
		CBasePlayer *localPlayer = UTIL_GetLocalPlayer();
		if ( localPlayer == NULL )
		{
			return NULL;
		}

		if ( m_clientCorpseList.empty() )
		{
			return NULL;
		}

		ClientCorpseStruct *lastCorpse = (ClientCorpseStruct *)last;
		if ( lastCorpse == NULL )
		{
			return (CBaseEntity *)m_clientCorpseList.front();
		}

		ClientCorpseListIter iter;
		for ( iter = m_clientCorpseList.begin(); iter != m_clientCorpseList.end(); ++iter )
		{
			ClientCorpseStruct *corpse = *iter;
			if ( corpse == lastCorpse )
			{
				++iter;
				if ( iter == m_clientCorpseList.end() )
				{
					return NULL;
				}

				while ( iter != m_clientCorpseList.end() && localPlayer->m_iTeam != (*iter)->m_team )
				{
					++iter;
				}

				if ( iter == m_clientCorpseList.end() )
				{
					return NULL;
				}
				return (CBaseEntity *)*iter;
			}
		}
	}
	}
	return ret;
}

void CCSTutor::CheckHintMessages( float time )
{
	if ( m_deadAirStartTime <= 0 )
	{
		return;
	}

	if ( time - m_deadAirStartTime > cv_tutor_hint_interval_time.value )
	{
		m_lastHintShown = (TutorMessageID)(m_lastHintShown + 1);

		bool confusingHint = true;
		while ( confusingHint )
		{
			if ( IsHostageMap() )
			{
				if ( m_lastHintShown >= HINT_BOMB_START && m_lastHintShown <= HINT_BOMB_END )
				{
					m_lastHintShown = (TutorMessageID)(m_lastHintShown + 1);
				}
				else
				{
					confusingHint = false;
				}
			}
			else if ( IsBombMap() )
			{
				if ( m_lastHintShown >= HINT_HOSTAGE_START && m_lastHintShown <= HINT_HOSTAGE_END )
				{
					m_lastHintShown = (TutorMessageID)(m_lastHintShown + 1);

					if ( m_lastHintShown == HINT_END )
					{
						m_lastHintShown = HINT_1;
					}
				}
				else
				{
					confusingHint = false;
				}
			}
			else
			{
				confusingHint = false;
			}
		}

		if ( m_lastHintShown < HINT_1 || m_lastHintShown >= HINT_END )
		{
			m_lastHintShown = HINT_1;
		}

		TutorMessage *message = GetTutorMessageDefinition( m_lastHintShown );

		if ( message == NULL )
		{
			return;
		}

		CreateAndAddEventToList( m_lastHintShown, NULL, NULL );
	}
}

void CCSTutor::CheckInGameHintMessages( float time )
{
	if ( m_deadAirStartTime <= 0 )
	{
		return;
	}

	if ( time - m_deadAirStartTime > cv_tutor_hint_interval_time.value )
	{
		m_lastInGameHintShown = (TutorMessageID)(m_lastInGameHintShown + 1);

		if ( m_lastInGameHintShown <= INGAME_HINT_BEGIN || m_lastInGameHintShown >= INGAME_HINT_END )
		{
			return;
		}

		TutorMessage *message = GetTutorMessageDefinition( m_lastInGameHintShown );

		if ( message == NULL )
		{
			return;
		}

		CreateAndAddEventToList( m_lastInGameHintShown, NULL, NULL );
	}
}

void CCSTutor::CheckForNeedToReload( bool isPassiveCheck )
{
	CBasePlayer *localPlayer = UTIL_GetLocalPlayer();

	if ( localPlayer == NULL || !localPlayer->IsAlive() )
	{
		return;
	}

	CBasePlayerItem *currentItem = localPlayer->m_pActiveItem;

	if ( currentItem == NULL || currentItem->IsWeapon() != TRUE )
	{
		return;
	}

	CBasePlayerWeapon *currentWeapon = (CBasePlayerWeapon *)currentItem;
	ItemInfo itemInfo;
	currentItem->GetItemInfo( &itemInfo );

	if ( (unsigned int)itemInfo.iSlot > 1 )
	{
		return;
	}

	if ( localPlayer->m_rgAmmo[currentWeapon->m_iPrimaryAmmoType] == 0 && currentWeapon->m_iClip == 0 )
	{
		if ( !isPassiveCheck )
		{
			TutorMessage *message = GetTutorMessageDefinition( YOU_ARE_OUT_OF_AMMO );
			if ( message != NULL )
			{
				message->m_lastCloseTime = 0;
			}
		}
		CreateAndAddEventToList( YOU_ARE_OUT_OF_AMMO, NULL, NULL );
	}
	else if ( localPlayer->m_rgAmmo[currentWeapon->m_iPrimaryAmmoType] != 0 )
	{
		if ( !isPassiveCheck )
		{
			if ( 5 * currentWeapon->m_iClip < currentWeapon->iMaxClip() && !currentWeapon->m_fInReload )
			{
				TutorMessage *message = GetTutorMessageDefinition( YOU_SHOULD_RELOAD );
				if ( message != NULL )
				{
					message->m_lastCloseTime = 0;
				}
				CreateAndAddEventToList( YOU_SHOULD_RELOAD, NULL, NULL );
			}
			else
			{
				TutorMessage *message = GetTutorMessageDefinition( YOU_SHOULD_RELOAD );
				if ( message != NULL )
				{
					message->m_lastCloseTime = gpGlobals->time;
				}
			}
		}
		else if ( 2 * currentWeapon->m_iClip < currentWeapon->iMaxClip() && !currentWeapon->m_fInReload )
		{
			CreateAndAddEventToList( YOU_SHOULD_RELOAD, NULL, NULL );
		}
	}
}

void CCSTutor::CheckExamineMessages( float time )
{
	CBasePlayer *localPlayer = UTIL_GetLocalPlayer();
	if ( localPlayer == NULL )
	{
		return;
	}

	for ( int i = 0; i < TUTOR_NUM_MESSAGES; ++i )
	{
		TutorMessage *message = GetTutorMessageDefinition( i );

		if ( message == NULL || message->m_class != TUTORMESSAGECLASS_EXAMINE )
		{
			continue;
		}

		CBaseEntity *entity = NULL;
		entity = GetEntityForMessageID( i, NULL );

		bool sawOne = false;

		while ( entity != NULL )
		{
			bool isPlayerLooking = false;
			if ( i == YOU_SEE_FRIEND_CORPSE || i == YOU_SEE_ENEMY_CORPSE )
			{
				isPlayerLooking = IsPlayerLookingAtPosition( (Vector *)entity, localPlayer );
			}
			else if ( i == YOU_SEE_FRIEND || i == YOU_SEE_ENEMY || i == YOU_SEE_HOSTAGE_CT_EXAMINE )
			{
				isPlayerLooking = IsPlayerLookingAtEntity( entity, localPlayer );
			}
			else
			{
				isPlayerLooking = IsPlayerLookingAtPosition( &entity->pev->origin, localPlayer );
			}

			if ( isPlayerLooking )
			{
				sawOne = true;
				if ( message->m_examineStartTime == -1.0f )
				{
					message->m_examineStartTime = time;
				}
				else if ( time - message->m_examineStartTime > cv_tutor_examine_time.value )
				{
					bool validEntity = true;
					switch ( i )
					{
					case YOU_SEE_FRIEND:
					{
						if ( !entity->IsPlayer() )
						{
							validEntity = false;
							break;
						}
						CBasePlayer *player = (CBasePlayer *)entity;
						if ( !player->IsAlive() )
						{
							validEntity = false;
							break;
						}
						if ( player->m_iTeam != localPlayer->m_iTeam )
						{
							validEntity = false;
						}
						break;
					}

					case YOU_SEE_ENEMY:
					{
						if ( !entity->IsPlayer() )
						{
							validEntity = false;
							break;
						}
						CBasePlayer *player = (CBasePlayer *)entity;
						if ( !player->IsAlive() )
						{
							validEntity = false;
							break;
						}
						if ( ( player->m_iTeam == CT && localPlayer->m_iTeam != TERRORIST ) ||
							( player->m_iTeam == TERRORIST && localPlayer->m_iTeam != CT ) )
						{
							validEntity = false;
						}
						break;
					}

					case YOU_SEE_HOSTAGE_CT_EXAMINE:
					{
						CHostage *hostage = (CHostage *)entity;
						if ( hostage->IsFollowingSomeone() )
						{
							validEntity = false;
						}
						else if ( hostage->pev->takedamage != DAMAGE_YES )
						{
							validEntity = false;
						}
						break;
					}
					}

					if ( validEntity )
					{
						CreateAndAddEventToList( (TutorMessageID)i, entity, NULL );
					}
				}

				break;
			}
			entity = GetEntityForMessageID( i, entity );
		}

		if ( !sawOne )
		{
			message->m_examineStartTime = -1.0f;
		}
	}
}

bool CCSTutor::CanLocalPlayerBuyStuff( void )
{
	CBasePlayer *localPlayer = UTIL_GetLocalPlayer();
	if ( localPlayer == NULL )
	{
		return false;
	}

	if ( localPlayer->CanPlayerBuy( false ) )
	{
		return true;
	}

	return false;
}


void CCSTutor::CheckBuyZoneMessages( void )
{
	CBasePlayer *localPlayer = UTIL_GetLocalPlayer();
	if ( localPlayer == NULL )
	{
		return;
	}

	if ( m_currentlyShownMessageID == BUY_TIME_BEGIN )
	{
		return;
	}

	CBasePlayerWeapon *primary = (CBasePlayerWeapon *)localPlayer->m_rgpPlayerItems[PRIMARY_WEAPON_SLOT];
	CBasePlayerWeapon *secondary = (CBasePlayerWeapon *)localPlayer->m_rgpPlayerItems[PISTOL_SLOT];

	if ( primary == NULL && localPlayer->CanAffordPrimary() )
	{
		TheTutor->OnEvent( EVENT_TUTOR_NEED_TO_BUY_PRIMARY_WEAPON, NULL, NULL );
		return;
	}
	if ( primary != NULL && localPlayer->NeedsPrimaryAmmo() && localPlayer->CanAffordPrimaryAmmo() )
	{
		TheTutor->OnEvent( EVENT_TUTOR_NEED_TO_BUY_PRIMARY_AMMO, NULL, NULL );
		return;
	}
	if ( secondary != NULL && localPlayer->NeedsSecondaryAmmo() && localPlayer->CanAffordSecondaryAmmo() )
	{
		TheTutor->OnEvent( EVENT_TUTOR_NEED_TO_BUY_SECONDARY_AMMO, NULL, NULL );
		return;
	}
	if ( localPlayer->NeedsArmor() && localPlayer->CanAffordArmor() )
	{
		TheTutor->OnEvent( EVENT_TUTOR_NEED_TO_BUY_ARMOR, NULL, NULL );
		return;
	}
	if ( localPlayer->NeedsDefuseKit() && localPlayer->CanAffordDefuseKit() )
	{
		TheTutor->OnEvent( EVENT_TUTOR_NEED_TO_BUY_DEFUSE_KIT, NULL, NULL );
		return;
	}
	if ( localPlayer->NeedsGrenade() && localPlayer->CanAffordGrenade() )
	{
		TheTutor->OnEvent( EVENT_TUTOR_NEED_TO_BUY_GRENADE, NULL, NULL );
		return;
	}

	TheTutor->OnEvent( EVENT_TUTOR_NOT_BUYING_ANYTHING, NULL, NULL );
}


bool CCSTutor::IsHostageMap( void )
{
	return UTIL_FindEntityByClassname( NULL, "hostage_entity" ) != NULL;
}


bool CCSTutor::IsBombMap( void )
{
	CHalfLifeMultiplay *mpRules = (CHalfLifeMultiplay *)g_pGameRules;

	return mpRules->m_bMapHasBombTarget;
}


void CCSTutor::ResetPlayerDeathInfo( void )
{
	for ( int i = 0; i < ARRAYSIZE( m_playerDeathInfo ); i++ )
	{
		m_playerDeathInfo[i].m_hasBeenShown = false;
		m_playerDeathInfo[i].m_event = NULL;
	}
}


void CCSTutor::ConstructRecentDeathsList( TeamName team, char *buf, int buflen, TutorMessageEvent *event )
{
	if ( buf == NULL || buflen == 0 )
	{
		return;
	}

	buf[0] = '\0';

	int i;
	for ( i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *player = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( player == NULL )
		{
			continue;
		}

		if ( player->m_iTeam != team )
		{
			continue;
		}

		if ( player->IsAlive() == TRUE )
		{
			continue;
		}

		if ( m_playerDeathInfo[i - 1].m_hasBeenShown )
		{
			continue;
		}

		strcat( buf, "  " );

		strcat( buf, "%n" );
		char scratch[32];
		sprintf( scratch, "%d", i );
		strcat( buf, scratch );
		strcat( buf, "\n" );

		m_playerDeathInfo[i - 1].m_event = event;
	}

	i = strlen( buf ) - 1;
	while ( buf[i] == '\n' )
	{
		buf[i] = '\0';
		i--;
	}
}

void CCSTutor::TransferDeathEvents( TutorMessageEvent *oldEvent, TutorMessageEvent *newEvent )
{
	for ( int i = 0; i < ARRAYSIZE( m_playerDeathInfo ); i++ )
	{
		if ( m_playerDeathInfo[i].m_event == oldEvent )
		{
			m_playerDeathInfo[i].m_event = newEvent;
		}
	}
}
