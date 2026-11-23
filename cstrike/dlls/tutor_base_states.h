#ifndef TUTOR_BASE_STATES_H
#define TUTOR_BASE_STATES_H

#include "GameEvent.h"

class CBaseEntity;

class CBaseTutorState
{
public:
	CBaseTutorState( void );
	virtual ~CBaseTutorState( void );
	virtual int CheckForStateTransition( GameEventType event, CBaseEntity *entity, CBaseEntity *other ) = 0;
	virtual char *GetStateString( void ) = 0;
	int GetType( void );

protected:
	int m_type;
};

class CBaseTutorStateSystem
{
public:
	CBaseTutorStateSystem( void );
	virtual ~CBaseTutorStateSystem( void );
	virtual bool UpdateState( GameEventType event, CBaseEntity *entity, CBaseEntity *other ) = 0;
	virtual char *GetCurrentStateString( void ) = 0;
	int GetCurrentStateType( void );

protected:
	virtual CBaseTutorState *ConstructNewState( int stateType ) = 0;
	CBaseTutorState *m_currentState;
};

#endif
