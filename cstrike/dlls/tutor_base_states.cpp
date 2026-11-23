#include "extdll.h"
#include "tutor_base_states.h"

#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
CBaseTutorStateSystem::CBaseTutorStateSystem( void )
{
}

CBaseTutorStateSystem::~CBaseTutorStateSystem( void )
{
}

int CBaseTutorStateSystem::GetCurrentStateType( void )
{
	return m_currentState != NULL ? m_currentState->GetType() : 0;
}


#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
CBaseTutorState::CBaseTutorState( void )
{
}

CBaseTutorState::~CBaseTutorState( void )
{
}

int CBaseTutorState::GetType( void )
{
	return m_type;
}
