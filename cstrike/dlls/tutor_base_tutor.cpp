#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "bot_util.h"
#include "tutor.h"
#include "tutor_base_states.h"

extern int gmsgTutorText;
extern int gmsgTutorLine;
extern int gmsgTutorState;
extern int gmsgTutorClose;


TutorMessageEvent::TutorMessageEvent( int mid, int duplicateID, float time, float lifetime, int priority )
{
	m_messageID = mid;
	m_duplicateID = duplicateID;
	m_activationTime = time;
	m_lifetime = lifetime;
	m_priority = priority;
	m_paramList = NULL;
	m_numParameters = 0;
	m_next = NULL;
}

TutorMessageEvent::~TutorMessageEvent( void )
{
}

bool TutorMessageEvent::IsActive( float time )
{
	return time <= m_lifetime + m_activationTime;
}

int TutorMessageEvent::GetPriority( void )
{
	return m_priority;
}

float TutorMessageEvent::GetTimeActive( float time )
{
	return time - m_activationTime;
}

void TutorMessageEvent::SetActivationTime( float time )
{
	m_activationTime = time;
}

int TutorMessageEvent::GetID( void )
{
	return m_messageID;
}

int TutorMessageEvent::GetDuplicateID( void )
{
	return m_duplicateID;
}

void TutorMessageEvent::SetNext( TutorMessageEvent *next )
{
	m_next = next;
}

TutorMessageEvent *TutorMessageEvent::GetNext( void )
{
	return m_next;
}

void TutorMessageEvent::AddParameter( char *str )
{
	if ( str == NULL )
		return;

	TutorMessageEventParam *param = new TutorMessageEventParam;
	param->m_next = NULL;
	param->m_data = new char[strlen( str ) + 1];
	if ( param->m_data == NULL )
	{
		return;
	}

	strcpy( param->m_data, str );
	param->m_data[strlen( str )] = '\0';

	m_numParameters++;

	TutorMessageEventParam *temp = m_paramList;
	if ( temp == NULL )
	{
		m_paramList = param;
		return;
	}
	while ( temp->m_next != NULL )
	{
		temp = temp->m_next;
	}

	temp->m_next = param;
}

char *TutorMessageEvent::GetNextParameter( char *buf, int buflen )
{
	TutorMessageEventParam *param = m_paramList;
	if ( param == NULL )
	{
		return NULL;
	}

	m_paramList = param->m_next;
	m_numParameters--;

	strncpy( buf, param->m_data, buflen );

	delete param;

	return buf;
}

int TutorMessageEvent::GetNumParameters( void )
{
	return m_numParameters;
}

#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
CBaseTutor::CBaseTutor( void )
{
	m_eventList = NULL;

	m_deadAirStartTime = 0;
	m_roundStartTime = 0;
}

CBaseTutor::~CBaseTutor( void )
{
	TutorMessageEvent *event = m_eventList;
	while ( event != NULL )
	{
		TutorMessageEvent *temp = event;
		event = event->GetNext();
		delete temp;
	}
}

void CBaseTutor::OnEvent( GameEventType event, CBaseEntity *entity, CBaseEntity *other )
{
	CallEventHandler( event, entity, other );

	CheckForStateTransition( event, entity, other );
}

void CBaseTutor::ShotFired( Vector source, Vector target )
{
	HandleShotFired( source, target );
}

void CBaseTutor::CheckForStateTransition( GameEventType event, CBaseEntity *entity, CBaseEntity *other )
{
	if ( m_stateSystem->UpdateState( event, entity, other ) )
	{
		DisplayNewStateDescriptionToPlayer();
	}
}

void CBaseTutor::StartFrame( float time )
{
	TutorThink( time );
}


void CBaseTutor::DisplayMessageToPlayer( CBasePlayer *player, int id, const char *szMessage, TutorMessageEvent *event )
{
	TutorMessage *definition = GetTutorMessageDefinition( event->GetID() );

	MESSAGE_BEGIN( MSG_ONE, gmsgTutorText, NULL, player->edict() );

		WRITE_STRING( szMessage );

		unsigned char numArgs = event->GetNumParameters();

		WRITE_BYTE( numArgs );

		char param[512];

		for ( int arg = 0; arg < numArgs; arg++ )
		{
			char *str = event->GetNextParameter( param, sizeof( param ) );
			if ( str != NULL )
			{
				WRITE_STRING( str );
			}
			else
			{
				WRITE_STRING( "" );
			}
		}

		WRITE_SHORT( (short)id );
		WRITE_SHORT( !player->IsAlive() );
		WRITE_SHORT( definition != NULL ? definition->m_type : TUTORMESSAGETYPE_DEFAULT );
	MESSAGE_END();

	m_deadAirStartTime = -1.0f;

	if ( definition != NULL )
	{
		if ( gpGlobals->time - m_roundStartTime > 1.0f )
		{
			switch ( definition->m_type )
			{
			case TUTORMESSAGETYPE_FRIEND_DEATH:

				EMIT_SOUND_DYN( player->edict(), CHAN_ITEM, "events/friend_died.wav", VOL_NORM, ATTN_NORM, 0, 120 );
				break;

			case TUTORMESSAGETYPE_ENEMY_DEATH:

				EMIT_SOUND_DYN( player->edict(), CHAN_ITEM, "events/enemy_died.wav", VOL_NORM, ATTN_NORM, 0, 85 );
				break;

			default:

				EMIT_SOUND_DYN( player->edict(), CHAN_ITEM, "events/tutor_msg.wav", VOL_NORM, ATTN_NORM, 0, PITCH_NORM );
				break;
			}
		}

		if ( definition->m_decay != 0 )
		{
			g_engfuncs.pfnRegisterTutorMessageShown( id );
		}
	}
}

void CBaseTutor::DrawLineToEntity( CBasePlayer *player, int entindex, int id )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgTutorLine, NULL, player->edict() );

		WRITE_SHORT( entindex );

		WRITE_SHORT( (short)id );
	MESSAGE_END();
}


void CBaseTutor::DisplayNewStateDescriptionToPlayer( void )
{
	CBasePlayer *localPlayer = UTIL_GetLocalPlayer();
	if ( localPlayer == NULL )
	{
		return;
	}

	char *desc = m_stateSystem->GetCurrentStateString();
	if ( desc == NULL )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgTutorState, NULL, localPlayer->edict() );
			WRITE_STRING( NULL );
		MESSAGE_END();
	}
}


void CBaseTutor::CloseCurrentWindow( void )
{
	CBasePlayer *localPlayer = UTIL_GetLocalPlayer();
	if ( localPlayer == NULL )
	{
		return;
	}

	MESSAGE_BEGIN( MSG_ONE, gmsgTutorClose, NULL, localPlayer->edict() );
	MESSAGE_END();

	m_deadAirStartTime = gpGlobals->time;
}

void CBaseTutor::CalculatePathForObjective( CBaseEntity *player )
{
}


bool CBaseTutor::IsEntityInViewOfPlayer( CBaseEntity *entity, CBasePlayer *player )
{
	if ( entity == NULL || player == NULL )
	{
		return false;
	}

	if ( ( entity->pev->origin - player->pev->origin ).Length() > cv_tutor_view_distance.value )
	{
		return false;
	}

	if ( !player->FInViewCone( entity ) )
	{
		return false;
	}

	Vector eye = player->pev->origin + player->pev->view_ofs;

	TraceResult result;
	UTIL_TraceLine( eye, entity->pev->origin, ignore_monsters, ignore_glass, player->edict(), &result );
	return result.flFraction == 1.0f;
}


bool CBaseTutor::IsPlayerLookingAtPosition( Vector *origin, CBasePlayer *player )
{
	if ( origin == NULL || player == NULL )
	{
		return false;
	}

	if ( ( *origin - player->pev->origin ).Length() > cv_tutor_look_distance.value )
	{
		return false;
	}

	if ( !player->IsLookingAtPosition( origin, cv_tutor_look_angle.value ) )
	{
		return false;
	}

	Vector eye = player->pev->origin + player->pev->view_ofs;
	TraceResult result;
	UTIL_TraceLine( eye, *origin, ignore_monsters, ignore_glass, player->edict(), &result );
	return result.flFraction == 1.0f;
}


bool CBaseTutor::IsPlayerLookingAtEntity( CBaseEntity *entity, CBasePlayer *player )
{
	if ( entity == NULL || player == NULL )
	{
		return false;
	}

	Vector srcVec = player->pev->origin + player->pev->view_ofs;

	UTIL_MakeVectors( player->pev->v_angle );

	Vector destVec = gpGlobals->v_forward * cv_tutor_look_distance.value + srcVec;

	TraceResult result;
	UTIL_TraceLine( srcVec, destVec, dont_ignore_monsters, ignore_glass, player->edict(), &result );
	if ( !FNullEnt( result.pHit ) )
	{
		if ( CBaseEntity::Instance( result.pHit ) == entity )
		{
			return true;
		}
	}

	return false;
}


bool CBaseTutor::IsBombsiteInViewOfPlayer( CBaseEntity *entity, CBasePlayer *player )
{
	if ( entity == NULL || player == NULL )
	{
		return false;
	}

	Vector bombSiteCenter;

	bombSiteCenter.x = ( entity->pev->absmax.x + entity->pev->absmin.x ) * 0.5f;
	bombSiteCenter.y = ( entity->pev->absmax.y + entity->pev->absmin.y ) * 0.5f;
	bombSiteCenter.z = ( entity->pev->absmax.z + entity->pev->absmin.z ) * 0.5f;

	if ( ( bombSiteCenter - player->pev->origin ).Length() > cv_tutor_view_distance.value )
	{
		return false;
	}

	if ( !player->FInViewCone( &bombSiteCenter ) )
	{
		return false;
	}

	Vector eye = player->pev->origin + player->pev->view_ofs;

	TraceResult result;
	UTIL_TraceLine( eye, bombSiteCenter, ignore_monsters, ignore_glass, player->edict(), &result );
	return result.flFraction == 1.0f;
}


bool CBaseTutor::IsEntityInBombsite( CBaseEntity *bombsite, CBaseEntity *entity )
{
	if ( bombsite == NULL || entity == NULL )
	{
		return false;
	}

	return entity->pev->origin.x <= bombsite->pev->absmax.x && entity->pev->origin.y <= bombsite->pev->absmax.y
		&& entity->pev->origin.z <= bombsite->pev->absmax.z
		&& bombsite->pev->absmin.x <= entity->pev->origin.x
		&& bombsite->pev->absmin.y <= entity->pev->origin.y
		&& bombsite->pev->absmin.z <= entity->pev->origin.z;
}


bool CBaseTutor::DoMessagesHaveSameID( int id1, int id2 )
{
	if ( id1 == id2 )
	{
		return true;
	}

	TutorMessage *message1 = GetTutorMessageDefinition( id1 );
	TutorMessage *message2 = GetTutorMessageDefinition( id2 );

	if ( message1 == NULL || message2 == NULL )
	{
		return false;
	}

	return message1->m_duplicateID != 0 && message1->m_duplicateID == message2->m_duplicateID;
}
