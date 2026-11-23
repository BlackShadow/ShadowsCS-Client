#ifndef TUTOR_CS_STATES_H
#define TUTOR_CS_STATES_H

#include "tutor_base_states.h"

enum TutorStateType
{
	TUTORSTATE_UNDEFINED,
	TUTORSTATE_LOOKING_FOR_HOSTAGE,
	TUTORSTATE_ESCORTING_HOSTAGE,
	TUTORSTATE_LOOKING_FOR_LOST_HOSTAGE,
	TUTORSTATE_FOLLOWING_HOSTAGE_ESCORT,
	TUTORSTATE_MOVING_TO_BOMBSITE,
	TUTORSTATE_LOOKING_FOR_BOMB_CARRIER,
	TUTORSTATE_GUARDING_LOOSE_BOMB,
	TUTORSTATE_DEFUSING_BOMB,
	TUTORSTATE_GUARDING_HOSTAGE,
	TUTORSTATE_MOVING_TO_INTERCEPT_ENEMY,
	TUTORSTATE_LOOKING_FOR_HOSTAGE_ESCORT,
	TUTORSTATE_ATTACKING_HOSTAGE_ESCORT,
	TUTORSTATE_ESCORTING_BOMB_CARRIER,
	TUTORSTATE_MOVING_TO_BOMB_SITE,
	TUTORSTATE_PLANTING_BOMB,
	TUTORSTATE_GUARDING_BOMB,
	TUTORSTATE_LOOKING_FOR_LOOSE_BOMB,
	TUTORSTATE_RUNNING_AWAY_FROM_TICKING_BOMB,
	TUTORSTATE_BUYTIME,
	TUTORSTATE_WAITING_FOR_START
};

class CCSTutorStateSystem : public CBaseTutorStateSystem
{
public:
	CCSTutorStateSystem( void );
	virtual ~CCSTutorStateSystem( void );
	virtual bool UpdateState( GameEventType event, CBaseEntity *entity, CBaseEntity *other );
	virtual char *GetCurrentStateString( void );

protected:
	virtual CBaseTutorState *ConstructNewState( int stateType );
};

class CCSTutorUndefinedState : public CBaseTutorState
{
public:
	CCSTutorUndefinedState( void );
	virtual ~CCSTutorUndefinedState( void );
	virtual int CheckForStateTransition( GameEventType event, CBaseEntity *entity, CBaseEntity *other );
	virtual char *GetStateString( void );

private:
	int HandlePlayerSpawned( CBaseEntity *entity, CBaseEntity *other );
};

class CCSTutorWaitingForStartState : public CBaseTutorState
{
public:
	CCSTutorWaitingForStartState( void );
	virtual ~CCSTutorWaitingForStartState( void );
	virtual int CheckForStateTransition( GameEventType event, CBaseEntity *entity, CBaseEntity *other );
	virtual char *GetStateString( void );

private:
	int HandlePlayerSpawned( CBaseEntity *entity, CBaseEntity *other );
	int HandleBuyTimeStart( CBaseEntity *entity, CBaseEntity *other );
};

class CCSTutorBuyMenuState : public CBaseTutorState
{
public:
	CCSTutorBuyMenuState( void );
	virtual ~CCSTutorBuyMenuState( void );
	virtual int CheckForStateTransition( GameEventType event, CBaseEntity *entity, CBaseEntity *other );
	virtual char *GetStateString( void );

private:
	int HandleRoundStart( CBaseEntity *entity, CBaseEntity *other );
};

#endif
