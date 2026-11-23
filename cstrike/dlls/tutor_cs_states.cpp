#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "bot_util.h"
#include "tutor_cs_states.h"

static char *g_TutorStateStrings[] =
{
	"#Cstrike_TutorState_Undefined",
	"#Cstrike_TutorState_Looking_For_Hostage",
	"#Cstrike_TutorState_Escorting_Hostage",
	"#Cstrike_TutorState_Following_Hostage_Escort",
	"#Cstrike_TutorState_Moving_To_Bombsite",
	"#Cstrike_TutorState_Looking_For_Bomb_Carrier",
	"#Cstrike_TutorState_Guarding_Loose_Bomb",
	"#Cstrike_TutorState_Defusing_Bomb",
	"#Cstrike_TutorState_Guarding_Hostage",
	"#Cstrike_TutorState_Moving_To_Intercept_Enemy",
	"#Cstrike_TutorState_Looking_For_Hostage_Escort",
	"#Cstrike_TutorState_Attacking_Hostage_Escort",
	"#Cstrike_TutorState_Escorting_Bomb_Carrier",
	"#Cstrike_TutorState_Moving_To_Bomb_Site",
	"#Cstrike_TutorState_Planting_Bomb",
	"#Cstrike_TutorState_Guarding_Bomb",
	"#Cstrike_TutorState_Looking_For_Loose_Bomb",
	"#Cstrike_TutorState_Running_Away_From_Ticking_Bomb",
	"#Cstrike_TutorState_Buy_Time",
	"#Cstrike_TutorState_Waiting_For_Start"
};

CCSTutorStateSystem::CCSTutorStateSystem( void )
{
	m_currentState = new CCSTutorUndefinedState;
}

CCSTutorStateSystem::~CCSTutorStateSystem( void )
{
	if ( m_currentState != NULL )
	{
		delete m_currentState;
		m_currentState = NULL;
	}
}


bool CCSTutorStateSystem::UpdateState( GameEventType event, CBaseEntity *entity, CBaseEntity *other )
{
	TutorStateType nextStateType;

	if ( m_currentState == NULL )
	{
		m_currentState = new CCSTutorUndefinedState;
	}

	if ( m_currentState != NULL )
	{
		nextStateType = (TutorStateType)m_currentState->CheckForStateTransition( event, entity, other );
		if ( nextStateType != TUTORSTATE_UNDEFINED )
		{
			if ( m_currentState != NULL )
			{
				delete m_currentState;
			}
			m_currentState = NULL;

			m_currentState = ConstructNewState( nextStateType );
			return true;
		}
	}

	return false;
}

char *CCSTutorStateSystem::GetCurrentStateString( void )
{
	return m_currentState != NULL ? m_currentState->GetStateString() : NULL;
}

CBaseTutorState *CCSTutorStateSystem::ConstructNewState( int stateType )
{
	CBaseTutorState *ret = NULL;
	switch ( stateType )
	{
	case TUTORSTATE_UNDEFINED:
		ret = new CCSTutorUndefinedState;
		break;

	case TUTORSTATE_WAITING_FOR_START:
		ret = new CCSTutorWaitingForStartState;
		break;

	case TUTORSTATE_BUYTIME:
		ret = new CCSTutorBuyMenuState;
		break;
	}

	return ret;
}


CCSTutorUndefinedState::CCSTutorUndefinedState( void )
{
	m_type = TUTORSTATE_UNDEFINED;
}

CCSTutorUndefinedState::~CCSTutorUndefinedState( void )
{
}

int CCSTutorUndefinedState::CheckForStateTransition( GameEventType event, CBaseEntity *entity, CBaseEntity *other )
{
	int ret = TUTORSTATE_UNDEFINED;

	if ( event == EVENT_PLAYER_SPAWNED )
	{
		ret = HandlePlayerSpawned( entity, other );
	}

	return ret;
}

int CCSTutorUndefinedState::HandlePlayerSpawned( CBaseEntity *entity, CBaseEntity *other )
{
	if ( entity == NULL || !entity->IsPlayer() )
	{
		return TUTORSTATE_UNDEFINED;
	}

	CBasePlayer *localPlayer = UTIL_GetLocalPlayer();
	if ( localPlayer == NULL )
	{
		return TUTORSTATE_UNDEFINED;
	}

	CBasePlayer *player = (CBasePlayer *)entity;
	if ( player != localPlayer )
	{
		return TUTORSTATE_UNDEFINED;
	}

	return TUTORSTATE_WAITING_FOR_START;
}

char *CCSTutorUndefinedState::GetStateString( void )
{
	return NULL;
}


CCSTutorWaitingForStartState::CCSTutorWaitingForStartState( void )
{
	m_type = TUTORSTATE_WAITING_FOR_START;
}

CCSTutorWaitingForStartState::~CCSTutorWaitingForStartState( void )
{
}

int CCSTutorWaitingForStartState::CheckForStateTransition( GameEventType event, CBaseEntity *entity, CBaseEntity *other )
{
	int ret = TUTORSTATE_UNDEFINED;

	switch ( event )
	{
	case EVENT_PLAYER_SPAWNED:
		ret = HandlePlayerSpawned( entity, other );
		break;
	case EVENT_BUY_TIME_START:
		ret = HandleBuyTimeStart( entity, other );
		break;
	}

	return ret;
}

char *CCSTutorWaitingForStartState::GetStateString( void )
{
	return g_TutorStateStrings[m_type];
}

int CCSTutorWaitingForStartState::HandlePlayerSpawned( CBaseEntity *entity, CBaseEntity *other )
{
	if ( entity == NULL || !entity->IsPlayer() )
	{
		return TUTORSTATE_UNDEFINED;
	}

	CBasePlayer *localPlayer = UTIL_GetLocalPlayer();
	if ( localPlayer == NULL )
	{
		return TUTORSTATE_UNDEFINED;
	}

	CBasePlayer *player = (CBasePlayer *)entity;
	if ( player != localPlayer )
	{
		return TUTORSTATE_UNDEFINED;
	}

	return TUTORSTATE_WAITING_FOR_START;
}

int CCSTutorWaitingForStartState::HandleBuyTimeStart( CBaseEntity *entity, CBaseEntity *other )
{
	return TUTORSTATE_BUYTIME;
}


CCSTutorBuyMenuState::CCSTutorBuyMenuState( void )
{
	m_type = TUTORSTATE_BUYTIME;
}

CCSTutorBuyMenuState::~CCSTutorBuyMenuState( void )
{
}

int CCSTutorBuyMenuState::CheckForStateTransition( GameEventType event, CBaseEntity *entity, CBaseEntity *other )
{
	int ret = TUTORSTATE_UNDEFINED;

	if ( event == EVENT_ROUND_START )
	{
		ret = HandleRoundStart( entity, other );
	}

	return ret;
}

char *CCSTutorBuyMenuState::GetStateString( void )
{
	return g_TutorStateStrings[m_type];
}

int CCSTutorBuyMenuState::HandleRoundStart( CBaseEntity *entity, CBaseEntity *other )
{
	return TUTORSTATE_WAITING_FOR_START;
}
