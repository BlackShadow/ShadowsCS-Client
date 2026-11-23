#ifndef CAREER_TASKS_H
#define CAREER_TASKS_H

#include <list>
#include "GameEvent.h"

class CBasePlayer;

class CCareerTask
{
public:
	CCareerTask( const char *taskName, GameEventType event, const char *weaponName, int n, bool mustLive, bool crossRounds, int id, bool isComplete );
	static CCareerTask *NewTask( const char *taskName, GameEventType event, const char *weaponName, int n, bool mustLive, bool crossRounds, int id, bool isComplete );

	virtual void OnEvent( GameEventType event, CBasePlayer *pAttacker, CBasePlayer *pVictim );
	void OnWeaponKill( int weaponId, int weaponClassId, bool headshot, bool killerHasShield, CBasePlayer *pAttacker, CBasePlayer *pVictim );
	void OnWeaponInjury( int weaponId, int weaponClassId, bool attackerHasShield, CBasePlayer *pAttacker );

	bool IsComplete( void ) { return m_isComplete; }
	const char *GetTaskName( void ) { return m_name; }

	int GetWeaponId( void ) { return m_weaponId; }
	int GetWeaponClassId( void ) { return m_weaponClassId; }

	virtual void Reset( void );

	bool IsValidFor( CBasePlayer *pPlayer ) const;

	void SendPartialNotification( void );

	virtual bool IsTaskCompletableThisRound( void ) const { return true; }
protected:
	bool m_isComplete;
	const char *m_name;
	int m_id;
	GameEventType m_event;
	int m_eventsNeeded;
	int m_eventsSeen;
	bool m_mustLive;
	bool m_crossRounds;
	bool m_diedThisRound;
	int m_weaponId;
	int m_weaponClassId;
	bool m_rescuer;
	bool m_defuser;
	bool m_vip;
};


class CPreventDefuseTask : public CCareerTask
{
public:
	CPreventDefuseTask( const char *taskName, GameEventType event, const char *weaponName, int n, bool mustLive, bool crossRounds, int id, bool isComplete );
	static CCareerTask *NewTask( const char *taskName, GameEventType event, const char *weaponName, int n, bool mustLive, bool crossRounds, int id, bool isComplete );

	virtual void OnEvent( GameEventType event, CBasePlayer *pAttacker, CBasePlayer *pVictim );

	virtual void Reset( void );

	virtual bool IsTaskCompletableThisRound( void ) const { return m_bombPlantedThisRound && !m_defuseStartedThisRound; }
protected:
	bool m_bombPlantedThisRound;
	bool m_defuseStartedThisRound;
};


typedef std::list< CCareerTask * > CareerTaskList;
typedef CareerTaskList::iterator CareerTaskListIt;


class CCareerTaskManager
{
public:
	static void Create( void );
	CCareerTaskManager( void );

	void Reset( bool deleteTasks );

	void AddTask( const char *taskName, const char *weaponName, int eventCount, bool mustLive, bool crossRounds, bool isComplete );
	void HandleEvent( GameEventType event, CBasePlayer *pAttacker = NULL, CBasePlayer *pVictim = NULL );
	void HandleEnemyKill( bool wasBlind, const char *weaponName, bool headshot, bool killerHasShield, CBasePlayer *pAttacker, CBasePlayer *pVictim );
	void HandleWeaponKill( int weaponId, int weaponClassId, bool headshot, bool killerHasShield, CBasePlayer *pAttacker, CBasePlayer *pVictim );
	void HandleDeath( int team, CBasePlayer *pAttacker );

	void HandleWeaponInjury( int weaponId, int weaponClassId, bool attackerHasShield, CBasePlayer *pAttacker );
	void HandleEnemyInjury( const char *weaponName, bool attackerHasShield, CBasePlayer *pAttacker );

	bool AreAllTasksComplete( void );
	int GetNumRemainingTasks( void );
	float GetRoundElapsedTime( void );

	int GetTaskTime( void ) { return m_taskTime; }
	void SetFinishedTaskTime( int val );
	int GetFinishedTaskTime( void ) { return m_finishedTaskTime; }
	int GetFinishedTaskRound( void ) { return m_finishedTaskRound; }

	CareerTaskList *GetTasks( void ) { return &m_tasks; }


private:
	CareerTaskList m_tasks;
	int m_nextId;
	float m_roundStartTime;
	int m_taskTime;
	int m_finishedTaskTime;
	int m_finishedTaskRound;

public:
	void LatchRoundEndMessage( void );
	void UnlatchRoundEndMessage( void );

private:
	GameEventType m_roundEndMessage;
	bool m_shouldLatchRoundEndMessage;
};

extern CCareerTaskManager *TheCareerTasks;

#endif
