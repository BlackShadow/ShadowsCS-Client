#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"
#include "game.h"
#include "career_tasks.h"
#include "tutor.h"
#include "hostage/hostage.h"
#include "bot/bot_util.h"

extern int gmsgCZCareer;
extern cvar_t humans_join_team;


typedef CCareerTask *( *TaskFactoryFunction )( const char *, GameEventType, const char *, int, bool, bool, int, bool );
struct TaskInfo
{
	const char *taskName;
	GameEventType event;
	TaskFactoryFunction factory;
};


static const TaskInfo taskInfo[] =
{
	{ "defuse", EVENT_BOMB_DEFUSED, CCareerTask::NewTask },
	{ "plant", EVENT_BOMB_PLANTED, CCareerTask::NewTask },
	{ "rescue", EVENT_HOSTAGE_RESCUED, CCareerTask::NewTask },
	{ "killall", EVENT_KILL_ALL, CCareerTask::NewTask },
	{ "kill", EVENT_KILL, CCareerTask::NewTask },
	{ "killwith", EVENT_KILL, CCareerTask::NewTask },
	{ "killblind", EVENT_KILL_FLASHBANGED, CCareerTask::NewTask },
	{ "killvip", EVENT_KILL, CCareerTask::NewTask },
	{ "headshot", EVENT_HEADSHOT, CCareerTask::NewTask },
	{ "headshotwith", EVENT_HEADSHOT, CCareerTask::NewTask },
	{ "winfast", EVENT_ROUND_WIN, CCareerTask::NewTask },
	{ "rescue", EVENT_HOSTAGE_RESCUED, CCareerTask::NewTask },
	{ "rescueall", EVENT_ALL_HOSTAGES_RESCUED, CCareerTask::NewTask },
	{ "injure", EVENT_PLAYER_TOOK_DAMAGE, CCareerTask::NewTask },
	{ "injurewith", EVENT_PLAYER_TOOK_DAMAGE, CCareerTask::NewTask },
	{ "killdefuser", EVENT_KILL, CCareerTask::NewTask },
	{ "stoprescue", EVENT_KILL, CCareerTask::NewTask },
	{ "defendhostages", EVENT_ROUND_WIN, CCareerTask::NewTask },
	{ "hostagessurvive", EVENT_ROUND_WIN, CCareerTask::NewTask },
	{ "preventdefuse", EVENT_ROUND_WIN, CPreventDefuseTask::NewTask },
	{ NULL, EVENT_INVALID, CCareerTask::NewTask }
};


CCareerTask *CPreventDefuseTask::NewTask( const char *taskName, GameEventType event, const char *weaponName, int n, bool mustLive, bool crossRounds, int id, bool isComplete )
{
	return new CPreventDefuseTask( taskName, event, weaponName, n, mustLive, crossRounds, id, isComplete );
}


CPreventDefuseTask::CPreventDefuseTask( const char *taskName, GameEventType event, const char *weaponName, int n, bool mustLive, bool crossRounds, int id, bool isComplete )
	: CCareerTask( taskName, event, weaponName, n, mustLive, crossRounds, id, isComplete )
{
	m_bombPlantedThisRound = false;
	m_defuseStartedThisRound = false;
}


void CPreventDefuseTask::Reset( void )
{
	m_bombPlantedThisRound = false;
	m_defuseStartedThisRound = false;

	CCareerTask::Reset();
}


void CPreventDefuseTask::OnEvent( GameEventType event, CBasePlayer *pAttacker, CBasePlayer *pVictim )
{
	if ( m_isComplete )
		return;

	switch ( event )
	{
	case EVENT_BOMB_PLANTED:
		m_bombPlantedThisRound = true;
		break;
	case EVENT_BOMB_DEFUSING:
		m_defuseStartedThisRound = true;
		break;
	}

	CCareerTask::OnEvent( event, pAttacker, pVictim );

	if ( event >= EVENT_ROUND_DRAW && event <= EVENT_ROUND_LOSS )
	{
		m_bombPlantedThisRound = false;
		m_defuseStartedThisRound = false;
	}
}


CCareerTask *CCareerTask::NewTask( const char *taskName, GameEventType event, const char *weaponName, int n, bool mustLive, bool crossRounds, int id, bool isComplete )
{
	return new CCareerTask( taskName, event, weaponName, n, mustLive, crossRounds, id, isComplete );
}


CCareerTask::CCareerTask( const char *taskName, GameEventType event, const char *weaponName, int n, bool mustLive, bool crossRounds, int id, bool isComplete )
{
	m_isComplete = isComplete;
	m_name = taskName;
	m_event = event;
	m_eventsNeeded = n;
	m_eventsSeen = 0;
	m_mustLive = mustLive;
	m_crossRounds = crossRounds;
	m_diedThisRound = false;
	m_id = id;
	m_weaponId = AliasToWeaponID( weaponName );
	m_weaponClassId = AliasToWeaponClass( weaponName );
	m_rescuer = !strcmp( taskName, "stoprescue" );
	m_defuser = !strcmp( taskName, "killdefuser" );
	m_vip = !stricmp( taskName, "killvip" );

	if ( event == EVENT_ALL_HOSTAGES_RESCUED )
	{
		m_mustLive = true;
		m_crossRounds = false;
	}

	if ( m_isComplete )
	{
		MESSAGE_BEGIN( MSG_ALL, gmsgCZCareer );
			WRITE_STRING( "TASKDONE" );
			WRITE_BYTE( m_id );
		MESSAGE_END();
	}
}


void CCareerTask::Reset( void )
{
	m_eventsSeen = 0;
	m_isComplete = false;
	MESSAGE_BEGIN( MSG_ALL, gmsgCZCareer );
		WRITE_STRING( "TASKUNDONE" );
		WRITE_BYTE( m_id );
	MESSAGE_END();
	MESSAGE_BEGIN( MSG_ALL, gmsgCZCareer );
		WRITE_STRING( "TASKPART" );
		WRITE_BYTE( m_id );
		WRITE_SHORT( m_eventsSeen );
	MESSAGE_END();
}


void CCareerTask::SendPartialNotification( void )
{
	MESSAGE_BEGIN( MSG_ALL, gmsgCZCareer );
		WRITE_STRING( "TASKPART" );
		WRITE_BYTE( m_id );
		WRITE_SHORT( m_eventsSeen );
	MESSAGE_END();
	UTIL_LogPrintf( "Career Task Partial %d %d\n", m_id, m_eventsSeen );
}


void CCareerTask::OnWeaponKill( int weaponId, int weaponClassId, bool headshot, bool killerHasShield, CBasePlayer *pAttacker, CBasePlayer *pVictim )
{
	if ( m_isComplete )
		return;

	if ( m_event != EVENT_KILL && ( m_event != EVENT_HEADSHOT || !headshot ) )
		return;

	if ( !pVictim )
		return;

	if ( m_defuser && !pVictim->m_bIsDefusing )
		return;

	if ( m_vip && !pVictim->m_bIsVIP )
		return;

	if ( m_rescuer )
	{
		int hostages = 0;
		CBaseEntity *hostageEntity = NULL;
		hostageEntity = UTIL_FindEntityByClassname( hostageEntity, "hostage_entity" );
		while ( hostageEntity )
		{
			CHostage *hostage = (CHostage *)hostageEntity;
			if ( hostage->IsFollowingSomeone() )
			{
				if ( hostage->IsValid() && hostage->m_target == pVictim )
					++hostages;
			}
			hostageEntity = UTIL_FindEntityByClassname( hostageEntity, "hostage_entity" );
		}

		if ( !hostages )
			return;
	}

	if ( m_weaponId == WEAPON_SHIELDGUN )
	{
		if ( killerHasShield )
		{
			++m_eventsSeen;
			SendPartialNotification();
		}
	}
	else if ( m_weaponId )
	{
		if ( m_weaponId == weaponId )
		{
			++m_eventsSeen;
			SendPartialNotification();
		}
	}
	else
	{
		if ( m_weaponClassId == weaponClassId && m_weaponClassId )
		{
			++m_eventsSeen;
			SendPartialNotification();
		}
	}
}


void CCareerTask::OnWeaponInjury( int weaponId, int weaponClassId, bool attackerHasShield, CBasePlayer *pAttacker )
{
	if ( m_isComplete )
		return;

	if ( m_event != EVENT_PLAYER_TOOK_DAMAGE )
		return;

	if ( m_weaponId == WEAPON_SHIELDGUN )
	{
		if ( attackerHasShield )
		{
			++m_eventsSeen;
			SendPartialNotification();
		}
	}
	else if ( m_weaponId )
	{
		if ( m_weaponId == weaponId )
		{
			++m_eventsSeen;
			SendPartialNotification();
		}
	}
	else
	{
		if ( m_weaponClassId && m_weaponClassId == weaponClassId )
		{
			++m_eventsSeen;
			SendPartialNotification();
		}
	}
}


void CCareerTask::OnEvent( GameEventType event, CBasePlayer *pAttacker, CBasePlayer *pVictim )
{
	if ( m_isComplete )
		return;

	if ( event == m_event )
	{
		if ( m_defuser && !pVictim->m_bIsDefusing )
			return;

		if ( m_vip && !pVictim->m_bIsVIP )
			return;

		if ( m_rescuer )
		{
			int hostages = 0;
			CBaseEntity *hostageEntity = NULL;
			hostageEntity = UTIL_FindEntityByClassname( hostageEntity, "hostage_entity" );
			while ( hostageEntity )
			{
				CHostage *hostage = (CHostage *)hostageEntity;
				if ( hostage->IsFollowingSomeone() )
				{
					if ( hostage->IsValid() && hostage->m_target == pVictim )
						++hostages;
				}
				hostageEntity = UTIL_FindEntityByClassname( hostageEntity, "hostage_entity" );
			}

			if ( !hostages )
				return;
		}

		if ( m_event == EVENT_KILL && ( m_weaponId || m_weaponClassId ) )
		{
		}
		else if ( m_event == EVENT_HEADSHOT && ( m_weaponId || m_weaponClassId ) )
		{
		}
		else if ( m_event == EVENT_PLAYER_TOOK_DAMAGE && ( m_weaponId || m_weaponClassId ) )
		{
		}
		else if ( m_event == EVENT_ROUND_WIN )
		{
			if ( !strcmp( m_name, "defendhostages" ) )
			{
				int hostages = 0;
				CBaseEntity *hostageEntity = NULL;
				hostageEntity = UTIL_FindEntityByClassname( hostageEntity, "hostage_entity" );
				while ( hostageEntity )
				{
					CHostage *hostage = (CHostage *)hostageEntity;
					if ( hostage->pev->takedamage != DAMAGE_YES && hostage->pev->deadflag != DEAD_DEAD )
						++hostages;

					hostageEntity = UTIL_FindEntityByClassname( hostageEntity, "hostage_entity" );
				}

				if ( !hostages )
				{
					++m_eventsSeen;
					SendPartialNotification();
				}
			}
			else if ( !strcmp( m_name, "hostagessurvive" ) )
			{
				int hostages = 0;
				CBaseEntity *hostageEntity = NULL;
				hostageEntity = UTIL_FindEntityByClassname( hostageEntity, "hostage_entity" );
				while ( hostageEntity )
				{
					CHostage *hostage = (CHostage *)hostageEntity;
					if ( hostage->IsDead() )
					{
						++hostages;
					}
					hostageEntity = UTIL_FindEntityByClassname( hostageEntity, "hostage_entity" );
				}

				if ( !hostages )
				{
					++m_eventsSeen;
					SendPartialNotification();
				}
			}
			else if ( !strcmp( m_name, "winfast" ) )
			{
				if ( TheCareerTasks->GetRoundElapsedTime() <= m_eventsNeeded )
				{
					m_eventsSeen = m_eventsNeeded;
					SendPartialNotification();
				}
			}
			else if ( IsTaskCompletableThisRound() )
			{
				++m_eventsSeen;
				SendPartialNotification();
			}
		}
		else
		{
			++m_eventsSeen;
			SendPartialNotification();
		}
	}

	if ( event == m_event && !m_mustLive && m_eventsSeen >= m_eventsNeeded && IsTaskCompletableThisRound() )
	{
		EMIT_SOUND_DYN( UTIL_GetLocalPlayer()->edict(), CHAN_VOICE, "events/task_complete.wav", VOL_NORM, ATTN_NORM, 0, PITCH_NORM );

		m_isComplete = true;
		MESSAGE_BEGIN( MSG_ALL, gmsgCZCareer );
			WRITE_STRING( "TASKDONE" );
			WRITE_BYTE( m_id );
		MESSAGE_END();

		if ( TheTutor )
		{
			TheTutor->OnEvent( EVENT_CAREER_TASK_DONE );
		}

		UTIL_LogPrintf( "Career Task Done %d\n", m_id );

		if ( m_event == EVENT_ROUND_WIN && !strcmp( m_name, "winfast" ) )
		{
			TheCareerTasks->SetFinishedTaskTime( (int)TheCareerTasks->GetRoundElapsedTime() );
			UTIL_GetLocalPlayer()->SyncRoundTimer();
		}

		return;
	}

	switch ( event )
	{
	case EVENT_DIE:
		if ( m_mustLive || m_crossRounds )
		{
			m_eventsSeen = 0;
			SendPartialNotification();
			m_diedThisRound = true;
		}
		break;
	case EVENT_ROUND_DRAW:
	case EVENT_ROUND_WIN:
	case EVENT_ROUND_LOSS:

		if ( m_mustLive )
		{
			if ( m_eventsSeen >= m_eventsNeeded && !m_diedThisRound && IsTaskCompletableThisRound() )
			{
				EMIT_SOUND_DYN( UTIL_GetLocalPlayer()->edict(), CHAN_VOICE, "events/task_complete.wav", VOL_NORM, ATTN_NORM, 0, PITCH_NORM );

				m_isComplete = true;
				MESSAGE_BEGIN( MSG_ALL, gmsgCZCareer );
					WRITE_STRING( "TASKDONE" );
					WRITE_BYTE( m_id );
				MESSAGE_END();

				UTIL_LogPrintf( "Career Task Done %d\n", m_id );

				if ( m_event == EVENT_ROUND_WIN && !strcmp( m_name, "winfast" ) )
				{
					TheCareerTasks->SetFinishedTaskTime( (int)TheCareerTasks->GetRoundElapsedTime() );
					UTIL_GetLocalPlayer()->SyncRoundTimer();
				}

				if ( TheTutor )
				{
					TheTutor->OnEvent( EVENT_CAREER_TASK_DONE );
				}
			}
			m_diedThisRound = false;

			if ( m_mustLive )
			{
				m_eventsSeen = 0;
				SendPartialNotification();
			}
		}
		break;
	}
}


CCareerTaskManager *TheCareerTasks = NULL;


void CCareerTaskManager::Create( void )
{
	if ( TheCareerTasks != NULL )
	{
		TheCareerTasks->Reset( true );
	}
	else
	{
		TheCareerTasks = new CCareerTaskManager;
	}
}


CCareerTaskManager::CCareerTaskManager( void )
{
	m_taskTime = 0;
	Reset( true );
}


void CCareerTaskManager::Reset( bool deleteTasks )
{
	if ( deleteTasks )
	{
		for ( CareerTaskListIt it = m_tasks.begin(); it != m_tasks.end(); ++it )
		{
			delete ( *it );
		}
		m_tasks.clear();
		m_nextId = 0;
	}
	else
	{
		for ( CareerTaskListIt it = m_tasks.begin(); it != m_tasks.end(); ++it )
		{
			CCareerTask *pTask = ( *it );
			pTask->Reset();
		}
	}
	m_roundStartTime = gpGlobals->time + CVAR_GET_FLOAT( "mp_freezetime" );

	m_finishedTaskTime = 0;
	m_finishedTaskRound = 0;

	m_shouldLatchRoundEndMessage = false;
}


void CCareerTaskManager::SetFinishedTaskTime( int val )
{
	m_finishedTaskTime = val;
	CHalfLifeMultiplay *mp = (CHalfLifeMultiplay *)g_pGameRules;
	m_finishedTaskRound = mp->m_iTotalRoundsPlayed;
}


void CCareerTaskManager::AddTask( const char *taskName, const char *weaponName, int eventCount, bool mustLive, bool crossRounds, bool isComplete )
{
	++m_nextId;

	for ( int i = 0; taskInfo[i].taskName != NULL; ++i )
	{
		if ( !stricmp( taskInfo[i].taskName, taskName ) )
		{
			m_tasks.push_back( taskInfo[i].factory( taskInfo[i].taskName,
				taskInfo[i].event,
				weaponName,
				eventCount,
				mustLive,
				crossRounds,
				m_nextId,
				isComplete
				) );

			if ( taskInfo[i].event == EVENT_ROUND_WIN && !strcmp( taskName, "winfast" ) )
			{
				m_taskTime = eventCount;

				if ( isComplete )
				{
					m_finishedTaskTime = eventCount;
				}
			}

			return;
		}
	}

	MESSAGE_BEGIN( MSG_ALL, gmsgCZCareer );
		WRITE_STRING( "TASKDONE" );
		WRITE_BYTE( m_nextId );
	MESSAGE_END();
}


void CCareerTaskManager::HandleEvent( GameEventType event, CBasePlayer *pAttacker, CBasePlayer *pVictim )
{
	if ( event == EVENT_ROUND_START )
	{
		m_roundStartTime = gpGlobals->time;
	}
	else if ( event >= EVENT_ROUND_DRAW && event <= EVENT_ROUND_LOSS )
	{
		if ( m_shouldLatchRoundEndMessage )
		{
			m_roundEndMessage = event;
			return;
		}
	}

	for ( CareerTaskListIt it = m_tasks.begin(); it != m_tasks.end(); ++it )
	{
		( *it )->OnEvent( event, pAttacker, pVictim );
	}
}


void CCareerTaskManager::HandleWeaponKill( int weaponId, int weaponClassId, bool headshot, bool killerHasShield, CBasePlayer *pAttacker, CBasePlayer *pVictim )
{
	for ( CareerTaskListIt it = m_tasks.begin(); it != m_tasks.end(); ++it )
	{
		( *it )->OnWeaponKill( weaponId, weaponClassId, headshot, killerHasShield, pAttacker, pVictim );
	}
}


void CCareerTaskManager::HandleEnemyKill( bool wasBlind, const char *weaponName, bool headshot, bool killerHasShield, CBasePlayer *pAttacker, CBasePlayer *pVictim )
{
	HandleWeaponKill( AliasToWeaponID( weaponName ), AliasToWeaponClass( weaponName ), headshot, killerHasShield, pAttacker, pVictim );
	HandleEvent( EVENT_KILL, pAttacker, pVictim );
	if ( headshot )
		HandleEvent( EVENT_HEADSHOT, pAttacker, pVictim );
	if ( wasBlind )
		HandleEvent( EVENT_KILL_FLASHBANGED, pAttacker, pVictim );
}


void CCareerTaskManager::HandleWeaponInjury( int weaponId, int weaponClassId, bool attackerHasShield, CBasePlayer *pAttacker )
{
	for ( CareerTaskListIt it = m_tasks.begin(); it != m_tasks.end(); ++it )
	{
		( *it )->OnWeaponInjury( weaponId, weaponClassId, attackerHasShield, pAttacker );
	}
}


void CCareerTaskManager::HandleEnemyInjury( const char *weaponName, bool attackerHasShield, CBasePlayer *pAttacker )
{
	HandleWeaponInjury( AliasToWeaponID( weaponName ), AliasToWeaponClass( weaponName ), attackerHasShield, pAttacker );
	HandleEvent( EVENT_PLAYER_TOOK_DAMAGE );
}


void CCareerTaskManager::HandleDeath( int team, CBasePlayer *pAttacker )
{
	int enemyTeam = !strcmp( humans_join_team.string, "CT" ) ? TERRORIST : CT;
	if ( enemyTeam != team )
		return;

	int numEnemies = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( pPlayer && pPlayer->m_iTeam == enemyTeam )
		{
			if ( pPlayer->IsAlive() )
				++numEnemies;
		}
	}
	if ( !numEnemies )
		HandleEvent( EVENT_KILL_ALL );
}


bool CCareerTaskManager::AreAllTasksComplete( void )
{
	for ( CareerTaskListIt it = m_tasks.begin(); it != m_tasks.end(); ++it )
	{
		if ( !( *it )->IsComplete() )
			return false;
	}

	return true;
}


int CCareerTaskManager::GetNumRemainingTasks( void )
{
	int ret = 0;
	for ( CareerTaskListIt it = m_tasks.begin(); it != m_tasks.end(); ++it )
	{
		if ( !( *it )->IsComplete() )
			++ret;
	}

	return ret;
}


float CCareerTaskManager::GetRoundElapsedTime( void )
{
	return ( gpGlobals->time - m_roundStartTime );
}


void CCareerTaskManager::LatchRoundEndMessage( void )
{
	m_shouldLatchRoundEndMessage = true;
}


void CCareerTaskManager::UnlatchRoundEndMessage( void )
{
	m_shouldLatchRoundEndMessage = false;
	HandleEvent( m_roundEndMessage );
}
