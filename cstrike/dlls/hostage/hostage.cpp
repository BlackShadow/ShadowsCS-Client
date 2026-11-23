/*** 
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

#include "hostage.h"
#include "hostage_improv.h"
#include "hostage_localnav.h"
#include "gamerules.h"
#include "monsters.h"
#include "hitgroups.h"
#include "hltv.h"
#include "cs_bot_manager.h"
#include "trains.h"
#include "career_tasks.h"
#include <stdio.h>
#include <string.h>


extern int gmsgHostagePos;
extern int gmsgHostageK;
extern cvar_t *g_psv_gravity;
extern void Broadcast( const char *sentence );

int g_iHostageNumber;


CHostageManager *g_pHostages;


LINK_ENTITY_TO_CLASS( monster_scientist, CHostage );
LINK_ENTITY_TO_CLASS( hostage_entity, CHostage );


cvar_t cv_hostage_debug = { "hostage_debug", "0", FCVAR_SERVER, 0, NULL };
cvar_t cv_hostage_stop = { "hostage_stop", "0", FCVAR_SERVER, 0, NULL };

void CHostage::Spawn( void )
{
	if ( !g_pHostages )
		g_pHostages = new CHostageManager;

	Precache();

	MAKE_STRING_CLASS( "hostage_entity", pev );
	pev->movetype = MOVETYPE_STEP;
	pev->solid = SOLID_SLIDEBOX;
	pev->takedamage = DAMAGE_YES;
	pev->flags |= FL_MONSTER;
	pev->deadflag = DEAD_NO;
	pev->max_health = 100;
	pev->health = pev->max_health;
	pev->gravity = 1;
	pev->view_ofs = Vector( 0, 0, 12 );
	pev->velocity = Vector( 0, 0, 0 );

	if ( pev->spawnflags & SF_MONSTER_HITMONSTERCLIP )
		pev->flags |= FL_MONSTERCLIP;

	if ( pev->skin < 0 )
		pev->skin = 0;

	SET_MODEL( edict(), STRING( pev->model ) );

	SetActivity( ACT_IDLE );

	m_flNextChange = 0;
	m_State = STAND;
	m_hTargetEnt = NULL;
	m_hStoppedTargetEnt = NULL;
	m_flFlinchTime = 0;
	m_vPathToFollow[0] = Vector( 0, 0, 0 );
	m_bRescueMe = FALSE;

	UTIL_SetSize( pev, Vector( -10, -10, 0 ), Vector( 10, 10, 62 ) );

	UTIL_MakeVectors( pev->v_angle );
	SetBoneController( 0, UTIL_VecToYaw( gpGlobals->v_forward ) );
	SetBoneController( 1, 0 );
	SetBoneController( 2, 0 );
	SetBoneController( 3, 0 );
	SetBoneController( 4, 0 );

	DROP_TO_FLOOR( edict() );

	SetThink( &CHostage::IdleThink );

	pev->nextthink = gpGlobals->time + RANDOM_FLOAT( 0.1f, 0.2f );
	m_flNextFullThink = gpGlobals->time + RANDOM_FLOAT( 0.1f, 0.2f );

	m_vStart = pev->origin;
	m_vStartAngles = pev->angles;

	m_vOldPos = Vector( 9999, 9999, 9999 );

	g_iHostageNumber++;
	m_iHostageIndex = g_iHostageNumber;
	m_flNextRadarTime = gpGlobals->time + RANDOM_FLOAT( 0, 1 );

	nTargetNode = -1;
	m_fHasPath = FALSE;
	m_flLastPathCheck = -1;
	m_flPathAcquired = -1;
	m_flPathCheckInterval = 0.1f;

	m_LocalNav = new CLocalNav( this );

	m_bStuck = FALSE;
	m_flStuckTime = 0;

	m_improv = NULL;
}


void CHostage::Precache( void )
{
	if ( UTIL_IsGame( "czero" ) )
	{
		static int which;

		switch ( which )
		{
		case REGULAR_GUY: pev->model = MAKE_STRING( "models/hostageA.mdl" ); break;
		case OLD_GUY: pev->model = MAKE_STRING( "models/hostageB.mdl" ); break;
		case BLACK_GUY: pev->model = MAKE_STRING( "models/hostageC.mdl" ); break;
		case GOOFY_GUY: pev->model = MAKE_STRING( "models/hostageD.mdl" ); break;
		}

		m_whichModel = (ModelType)which;

		which = ( which + 1 ) & 3;
	}
	else
	{
		m_whichModel = REGULAR_GUY;

		if ( !pev->model )
			pev->model = MAKE_STRING( "models/scientist.mdl" );
	}

	PRECACHE_MODEL( (char *)STRING( pev->model ) );

	PRECACHE_SOUND( "hostage/hos1.wav" );
	PRECACHE_SOUND( "hostage/hos2.wav" );
	PRECACHE_SOUND( "hostage/hos3.wav" );
	PRECACHE_SOUND( "hostage/hos4.wav" );
	PRECACHE_SOUND( "hostage/hos5.wav" );

	PRECACHE_MODEL( "sprites/smoke.spr" );
}


void CHostage::SetActivity( int act )
{
	int sequence;

	if ( m_Activity == act )
		return;

	sequence = LookupActivity( act );
	if ( sequence == -1 )
		return;
	if ( pev->sequence != sequence )
	{
		if ( !( ( m_Activity == ACT_WALK || m_Activity == ACT_RUN ) && ( act == ACT_WALK || act == ACT_RUN ) ) )
			pev->frame = 0;

		pev->sequence = sequence;
	}

	m_Activity = act;
	ResetSequenceInfo();
}


void CHostage::IdleThink( void )
{
	if ( TheNavAreaList.empty() )
	{
		if ( m_improv )
			delete m_improv;

		m_improv = NULL;
	}
	else if ( !m_improv )
	{
		m_improv = new CHostageImprov( this );
	}

	const float upkeepRate = 0.033333335f;
	pev->nextthink = gpGlobals->time + upkeepRate;

	float flInterval = StudioFrameAdvance();
	DispatchAnimEvents( flInterval );

	if ( m_improv )
		m_improv->OnUpkeep( upkeepRate );

	if ( m_flNextFullThink > gpGlobals->time )
		return;

	const float updateRate = 0.1f;
	m_flNextFullThink = gpGlobals->time + updateRate;

	if ( pev->deadflag == DEAD_DEAD )
	{
		UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );
		return;
	}

	if ( m_hTargetEnt )
	{
		const float giveUpTime = 5.0f;
		if ( ( m_bStuck && gpGlobals->time - m_flStuckTime > giveUpTime ) || m_hTargetEnt->pev->deadflag != DEAD_NO )
		{
			m_State = STAND;
			m_hTargetEnt = NULL;
			m_bStuck = FALSE;
		}
	}

	if ( m_hTargetEnt || m_improv )
	{
		CBasePlayer *player = (m_improv) ? (CBasePlayer *)m_improv->GetFollowLeader() : GetClassPtr( (CBasePlayer *)m_hTargetEnt->pev );

		if ( !player || player->m_iTeam == CT )
		{
			if ( !( (CHalfLifeMultiplay *)g_pGameRules )->m_bMapHasRescueZone )
			{
				CBaseEntity *pSpot = NULL;
				BOOL bResHostagePt = FALSE;

				if ( UTIL_FindEntityByClassname( NULL, "info_hostage_rescue" ) )
					bResHostagePt = TRUE;

				pSpot = UTIL_FindEntityByClassname( pSpot, "info_hostage_rescue" );
				while ( pSpot )
				{
					if ( ( pSpot->pev->origin - pev->origin ).Length() < 256 )
					{
						m_bRescueMe = TRUE;
						break;
					}
					pSpot = UTIL_FindEntityByClassname( pSpot, "info_hostage_rescue" );
				}

				if ( !bResHostagePt )
				{
					pSpot = UTIL_FindEntityByClassname( NULL, "info_player_start" );
					while ( pSpot )
					{
						if ( ( pSpot->pev->origin - pev->origin ).Length() < 256 )
						{
							m_bRescueMe = TRUE;
							break;
						}
						pSpot = UTIL_FindEntityByClassname( pSpot, "info_player_start" );
					}
				}
			}

			if ( m_bRescueMe )
			{
				TheBots->OnEvent( EVENT_HOSTAGE_RESCUED, player, this );

				if ( ( (CHalfLifeMultiplay *)g_pGameRules )->IsCareer() )
				{
					if ( player && !player->IsBot() )
						TheCareerTasks->HandleEvent( EVENT_HOSTAGE_RESCUED, player );
				}

				pev->deadflag = DEAD_RESPAWNABLE;

				if ( player )
				{
					player->AddAccount( 1000, true );

					UTIL_LogPrintf( "\"%s<%i><%s><CT>\" triggered \"Rescued_A_Hostage\"\n",
						STRING( player->pev->netname ),
						GETPLAYERUSERID( player->edict() ),
						GETPLAYERAUTHID( player->edict() ) );
				}

				SendHostageEventMsg();

				MESSAGE_BEGIN( MSG_SPEC, SVC_DIRECTOR );
					WRITE_BYTE( 9 );
					WRITE_BYTE( DRC_CMD_EVENT );
					WRITE_SHORT( player ? ENTINDEX( player->edict() ) : 0 );
					WRITE_SHORT( ENTINDEX( edict() ) );
					WRITE_LONG( 15 );
				MESSAGE_END();

				pev->effects |= EF_NODRAW;
				Remove();

				( (CHalfLifeMultiplay *)g_pGameRules )->m_iHostagesRescued++;

				( (CHalfLifeMultiplay *)g_pGameRules )->CheckWinConditions();
				if ( player )
					Broadcast( "rescued" );
				else
					Broadcast( "escaped" );
			}
		}
	}

	if ( m_improv )
	{
		m_improv->OnUpdate( updateRate );
	}
	else
	{
		DoFollow();

		if ( gpGlobals->time >= m_flFlinchTime )
		{
			if ( pev->velocity.Length() > 160 )
				SetActivity( ACT_RUN );
			else if ( pev->velocity.Length() > 15 )
				SetActivity( ACT_WALK );
			else
				SetActivity( ACT_IDLE );
		}
	}

	if ( pev->deadflag != DEAD_DEAD && !( pev->effects & EF_NODRAW ) )
	{
		if ( gpGlobals->time >= m_flNextRadarTime )
		{
			Vector vDistance = m_vOldPos - pev->origin;

			if ( vDistance.Length() > 1 )
			{
				m_vOldPos = pev->origin;

				if ( ( (CHalfLifeMultiplay *)g_pGameRules )->m_fTeamCount == 0 )
				{
					SendHostagePositionMsg();
				}
			}

			m_flNextRadarTime = gpGlobals->time + 1;
		}
	}
}


void CHostage::Remove( void )
{
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	pev->takedamage = DAMAGE_NO;

	UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );

	pev->nextthink = -1;
	m_flNextFullThink = -1;
}


void CHostage::RePosition( void )
{
	pev->health = pev->max_health;
	pev->movetype = MOVETYPE_STEP;
	pev->solid = SOLID_SLIDEBOX;
	pev->takedamage = DAMAGE_YES;
	pev->deadflag = DEAD_NO;
	pev->velocity = Vector( 0, 0, 0 );
	pev->angles = m_vStartAngles;

	pev->effects &= ~EF_NODRAW;

	m_hTargetEnt = NULL;
	m_hStoppedTargetEnt = NULL;
	m_bTouched = FALSE;
	m_bRescueMe = FALSE;
	m_flNextRadarTime = 0;

	m_vOldPos = Vector( 9999, 9999, 9999 );

	UTIL_SetOrigin( pev, m_vStart );

	UTIL_SetSize( pev, Vector( -10, -10, 0 ), Vector( 10, 10, 62 ) );

	DROP_TO_FLOOR( edict() );

	SetActivity( ACT_IDLE );

	SetThink( &CHostage::IdleThink );

	pev->nextthink = gpGlobals->time + RANDOM_FLOAT( 0.1f, 0.2f );
	m_flNextFullThink = gpGlobals->time + RANDOM_FLOAT( 0.1f, 0.2f );

	nTargetNode = -1;
	m_fHasPath = FALSE;
	m_flLastPathCheck = -1;
	m_flPathAcquired = -1;
	m_flPathCheckInterval = 0.1f;
}


int CHostage::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	float flActualDamage;
	CBasePlayer *pAttacker = NULL;

	flActualDamage = GetModifiedDamage( flDamage, m_LastHitGroup );
	if ( flActualDamage > pev->health )
		flActualDamage = pev->health;

	pev->health -= flActualDamage;

	if ( m_improv )
		m_improv->OnInjury( flActualDamage );

	PlayPainSound();

	if ( pevAttacker )
	{
		CBaseEntity *pAttackingEnt;

		pAttackingEnt = GetClassPtr( (CBaseEntity *)pevAttacker );
		if ( pAttackingEnt->Classify() == CLASS_VEHICLE )
		{
			if ( ( (CFuncVehicle *)pAttackingEnt )->m_pDriver )
				pevAttacker = ( (CFuncVehicle *)pAttackingEnt )->m_pDriver->pev;
		}

		if ( pAttackingEnt->IsPlayer() )
			pAttacker = GetClassPtr( (CBasePlayer *)pevAttacker );
	}

	if ( pev->health <= 0 )
	{
		pev->health = 0;
		pev->movetype = MOVETYPE_TOSS;

		pev->flags &= ~FL_ONGROUND;

		SetDeathActivity();

		if ( pAttacker )
		{
			pAttacker->AddAccount( -20 * (int)flActualDamage - 500, true );

			AnnounceDeath( pAttacker );
			ApplyHostagePenalty( pAttacker );
		}

		pev->takedamage = DAMAGE_NO;
		pev->deadflag = DEAD_DEAD;
		pev->solid = SOLID_NOT;

		if ( TheBots )
		{
			TheBots->OnEvent( EVENT_HOSTAGE_KILLED, this, pAttacker );
		}

		if ( m_improv )
		{
			m_improv->Chatter( HOSTAGE_CHATTER_DEATH_CRY, true );
		}

		( (CHalfLifeMultiplay *)g_pGameRules )->CheckWinConditions();

		if ( ( (CHalfLifeMultiplay *)g_pGameRules )->m_fTeamCount == 0 )
		{
			SendHostageEventMsg();
		}

		pev->nextthink = gpGlobals->time + 3;
		SetThink( &CHostage::Remove );

		return 0;
	}

	SetFlinchActivity();

	if ( pAttacker )
	{
		pAttacker->AddAccount( -20 * (int)flActualDamage, true );

		if ( TheBots )
		{
			TheBots->OnEvent( EVENT_HOSTAGE_DAMAGED, this, pAttacker );
		}

		ClientPrint( pAttacker->pev, HUD_PRINTCENTER, "#Injured_Hostage" );

		if ( !( pAttacker->m_flDisplayHistory & 0x20 ) )
		{
			pAttacker->HintMessage( "#Hint_careful_around_hostages", false, false );
			pAttacker->m_flDisplayHistory |= 0x20;
		}

		return 1;
	}
	return 0;
}


float CHostage::GetModifiedDamage( float flDamage, int nHitGroup )
{
	switch ( nHitGroup )
	{
	case HITGROUP_GENERIC: flDamage *= 1.75; break;
	case HITGROUP_HEAD: flDamage *= 2.5; break;
	case HITGROUP_CHEST: flDamage *= 1.5; break;
	case HITGROUP_STOMACH: flDamage *= 1.75; break;
	case HITGROUP_LEFTARM: flDamage *= 0.75; break;
	case HITGROUP_RIGHTARM: flDamage *= 0.75; break;
	case HITGROUP_LEFTLEG: flDamage *= 0.6; break;
	case HITGROUP_RIGHTLEG: flDamage *= 0.6; break;
	default: flDamage *= 1.5; break;
	}

	return flDamage;
}


void CHostage::PlayPainSound( void )
{
	if ( m_LastHitGroup == HITGROUP_HEAD )
	{
		switch ( RANDOM_LONG( 0, 1 ) )
		{
		case 0: EMIT_SOUND( edict(), CHAN_VOICE, "player/headshot1.wav", 1, ATTN_NORM );
			break;
		case 1: EMIT_SOUND( edict(), CHAN_VOICE, "player/headshot2.wav", 1, ATTN_NORM );
			break;
		}
	}
}


void CHostage::SetFlinchActivity( void )
{
	m_flFlinchTime = gpGlobals->time + 0.75f;

	Activity activity = ACT_SMALL_FLINCH;

	if ( m_improv )
	{
		m_improv->Flinch( activity );
	}
	else
	{
		SetActivity( activity );
	}
}


void CHostage::SetDeathActivity( void )
{
	if ( m_improv && m_improv->IsCrouching() )
	{
		m_improv->CrouchDie();
		return;
	}

	if ( UTIL_IsGame( "czero" ) )
	{
		switch ( m_LastHitGroup )
		{
		case HITGROUP_HEAD: SetActivity( ACT_DIE_HEADSHOT ); break;
		case HITGROUP_STOMACH: SetActivity( ACT_DIE_GUTSHOT ); break;
		case HITGROUP_CHEST: SetActivity( ACT_DIE_CHESTSHOT ); break;
		default: SetActivity( ACT_DIESIMPLE ); break;
		}
	}
	else
	{
		switch ( m_LastHitGroup )
		{
		case HITGROUP_CHEST:
		case HITGROUP_RIGHTARM: SetActivity( ACT_DIESIMPLE ); break;
		case HITGROUP_LEFTARM:
		case HITGROUP_LEFTLEG: SetActivity( ACT_DIEBACKWARD ); break;
		case HITGROUP_STOMACH:
		case HITGROUP_RIGHTLEG: SetActivity( ACT_DIEFORWARD ); break;
		default: SetActivity( ACT_DIE_HEADSHOT ); break;
		}
	}
}


void CHostage::AnnounceDeath( CBasePlayer *pAttacker )
{
	ClientPrint( pAttacker->pev, HUD_PRINTCENTER, "#Killed_Hostage" );

	if ( !( pAttacker->m_flDisplayHistory & 0x40 ) )
	{
		pAttacker->HintMessage( "#Hint_lost_money", false, false );
		pAttacker->m_flDisplayHistory |= 0x40;
	}

	if ( !g_pGameRules->IsMultiplayer() )
		( (CHalfLifeTraining *)g_pGameRules )->HostageDied();

	UTIL_LogPrintf(
		"\"%s<%i><%s><%s>\" triggered \"Killed_A_Hostage\"\n",
		STRING( pAttacker->pev->netname ),
		GETPLAYERUSERID( pAttacker->edict() ),
		GETPLAYERAUTHID( pAttacker->edict() ),
		GetTeam( pAttacker->m_iTeam ) );

	MESSAGE_BEGIN( MSG_SPEC, SVC_DIRECTOR );
		WRITE_BYTE( 9 );
		WRITE_BYTE( DRC_CMD_EVENT );
		WRITE_SHORT( ENTINDEX( pAttacker->edict() ) );
		WRITE_SHORT( ENTINDEX( edict() ) );
		WRITE_LONG( 15 );
	MESSAGE_END();
}


void CHostage::ApplyHostagePenalty( CBasePlayer *pAttacker )
{
	int iHostagePenalty;

	if ( pAttacker->m_iTeam != TERRORIST )
		return;

	iHostagePenalty = (int)CVAR_GET_FLOAT( "mp_hostagepenalty" );
	if ( iHostagePenalty )
	{
		pAttacker->m_iHostagesKilled++;

		if ( pAttacker->m_iHostagesKilled == iHostagePenalty - 1 )
		{
			pAttacker->HintMessage( "#Hint_removed_for_next_hostage_killed", true, false );
		}
		else if ( pAttacker->m_iHostagesKilled >= iHostagePenalty )
		{
			CLIENT_COMMAND( pAttacker->edict(), "disconnect\n" );
		}
	}
}

void CHostage::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBasePlayer *pPlayer = (CBasePlayer *)pActivator;

	if ( !pActivator->IsPlayer() )
		return;

	if ( pev->takedamage == DAMAGE_NO )
		return;

	if ( pPlayer->m_iTeam != CT )
	{
		if ( !( pPlayer->m_flDisplayHistory & 0x200000 ) )
		{
			pPlayer->m_flDisplayHistory |= 0x200000;
			pPlayer->HintMessage( "#Only_CT_Can_Move_Hostages", false, true );
		}

		return;
	}

	if ( m_flNextChange > gpGlobals->time )
		return;

	m_flNextChange = gpGlobals->time + 1;

	if ( m_improv )
	{
		if ( !m_improv->IsFollowing( pActivator ) )
		{
			m_improv->Follow( (CBasePlayer *)pActivator );
			TheBots->OnEvent( EVENT_HOSTAGE_USED, pActivator );
		}
		else
		{
			m_improv->Agree();
			m_improv->Idle();
			m_improv->Chatter( HOSTAGE_CHATTER_STOP_FOLLOW, true );
		}
	}
	else
	{
		m_flPathAcquired = gpGlobals->time;

		if ( pActivator != (CBaseEntity *)m_hTargetEnt )
		{
			m_State = FOLLOW;

			m_hTargetEnt = pActivator;
			m_hStoppedTargetEnt = NULL;
		}
		else
		{
			if ( m_State == FOLLOW )
			{
				m_State = STAND;

				m_hTargetEnt = NULL;
				m_hStoppedTargetEnt = pActivator;
			}
			else
			{
				m_State = FOLLOW;
			}
		}

		if ( m_State == FOLLOW )
		{
			PlayFollowRescueSound();

			TheBots->OnEvent( EVENT_HOSTAGE_USED, pActivator );
		}
	}

	GiveCTTouchBonus( pPlayer );

	pPlayer->HostageUsed();
}


void CHostage::PlayFollowRescueSound( void )
{
	switch ( RANDOM_LONG( 0, 4 ) )
	{
	case 0: EMIT_SOUND( edict(), CHAN_VOICE, "hostage/hos1.wav", 1, ATTN_NORM );
		break;
	case 1: EMIT_SOUND( edict(), CHAN_VOICE, "hostage/hos2.wav", 1, ATTN_NORM );
		break;
	case 2: EMIT_SOUND( edict(), CHAN_VOICE, "hostage/hos3.wav", 1, ATTN_NORM );
		break;
	case 3: EMIT_SOUND( edict(), CHAN_VOICE, "hostage/hos4.wav", 1, ATTN_NORM );
		break;
	case 4: EMIT_SOUND( edict(), CHAN_VOICE, "hostage/hos5.wav", 1, ATTN_NORM );
		break;
	}
}


void CHostage::GiveCTTouchBonus( CBasePlayer *pPlayer )
{
	if ( m_bTouched == TRUE )
		return;

	m_bTouched = TRUE;
	( (CHalfLifeMultiplay *)g_pGameRules )->m_iAccountCT += 100;
	pPlayer->AddAccount( 150, true );

	UTIL_LogPrintf(
		"\"%s<%i><%s><CT>\" triggered \"Touched_A_Hostage\"\n",
		STRING( pPlayer->pev->netname ),
		GETPLAYERUSERID( pPlayer->edict() ),
		GETPLAYERAUTHID( pPlayer->edict() ) );
}


int CHostage::ObjectCaps( void )
{
	return FCAP_ACROSS_TRANSITION | FCAP_MUST_SPAWN | FCAP_ONOFF_USE;
}


void CHostage::Touch( CBaseEntity *pOther )
{
	if ( m_improv )
	{
		m_improv->OnTouch( pOther );
		return;
	}

	if ( pOther->IsPlayer() == TRUE )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)pOther;

		if ( pPlayer->m_iTeam != CT )
			return;
	}
	else if ( !FClassnameIs( pOther->pev, "hostage_entity" ) )
	{
		return;
	}

	Vector2D vPush = ( pev->origin - pOther->pev->origin ).Make2D();
	vPush.NormalizeInPlace();

	const float pushForce = 50.0f;
	pev->velocity.x += vPush.x * pushForce;
	pev->velocity.y += vPush.y * pushForce;
}


void CHostage::DoFollow( void )
{
	CBaseEntity *pFollowing;
	Vector vecDest;
	float flRadius = 80;
	float flDistToDest;

	if ( !m_hTargetEnt )
		return;

	if ( cv_hostage_stop.value > 0 )
	{
		m_State = STAND;
		m_hTargetEnt = NULL;
		m_hStoppedTargetEnt = NULL;
		return;
	}

	pFollowing = GetClassPtr( (CBaseEntity *)m_hTargetEnt->pev );
	m_LocalNav->SetTargetEnt( pFollowing );

	vecDest = pFollowing->pev->origin;
	vecDest.z += pFollowing->pev->mins.z;

	flDistToDest = ( vecDest - pev->origin ).Length();

	if ( flDistToDest < flRadius )
	{
		if ( m_fHasPath )
			return;

		if ( m_LocalNav->PathTraversable( pev->origin, vecDest, 1 ) )
			return;
	}

	if ( pev->flags & FL_ONGROUND )
	{
		if ( gpGlobals->time > m_flLastPathCheck + m_flPathCheckInterval )
		{
			if ( !m_fHasPath || pFollowing->pev->velocity.Length2D() > 1 )
			{
				m_flLastPathCheck = gpGlobals->time;
				m_LocalNav->RequestNav( this );
			}
		}
	}

	if ( m_fHasPath )
	{
		nTargetNode = m_LocalNav->GetFurthestTraversableNode( pev->origin, vecNodes, m_nPathNodes, 1 );
		if ( nTargetNode == 0 &&
			( vecNodes[nTargetNode] - pev->origin ).Length2D() < 26 )
		{
			nTargetNode = -1;
		}

		if ( nTargetNode == -1 )
		{
			m_fHasPath = FALSE;
			m_flPathCheckInterval = 0.1f;
		}
	}

	if ( m_flFlinchTime > gpGlobals->time )
		return;

	if ( nTargetNode != -1 )
	{
		if ( pev->flags & FL_ONGROUND )
			PointAt( vecNodes[nTargetNode] );

		if ( pev->movetype == MOVETYPE_FLY )
			pev->v_angle.x = -60;

		MoveToward( vecNodes[nTargetNode] );

		m_bStuck = FALSE;
	}
	else if ( IsFollowingSomeone() )
	{
		if ( !m_bStuck && flDistToDest > 200 )
		{
			m_bStuck = TRUE;
			m_flStuckTime = gpGlobals->time;
		}
	}

	if ( pev->flags & FL_ONGROUND )
	{
		if ( m_flPathAcquired != -1 && m_flPathAcquired + 2.0 > gpGlobals->time )
		{
			if ( pev->velocity.Length2D() < 1 || nTargetNode == -1 )
			{
				Wiggle();
			}
		}
	}
}
void CHostage::PointAt( const Vector &vecLoc )
{
	pev->angles.x = 0;
	pev->angles.y = UTIL_VecToAngles( vecLoc - pev->origin ).y;
	pev->angles.z = 0;
}
void CHostage::MoveToward( const Vector &vecLoc )
{
	int nFwdMove;
	Vector vecFwd;
	Vector vecbigDest;
	Vector vecMove;

	CBaseEntity *pFollowing = GetClassPtr( (CBaseEntity *)m_hTargetEnt->pev );

	Vector vecAng;
	vecAng.x = 0;
	vecAng.y = UTIL_VecToAngles( vecLoc - pev->origin ).y;
	vecAng.z = 0;
	UTIL_MakeVectorsPrivate( vecAng, vecFwd, NULL, NULL );

	float flDist = std::min( ( vecFwd * CLocalNav::s_flStepSize ).Length2D(), ( vecLoc - pev->origin ).Length2D() );

	vecbigDest = pev->origin + vecFwd * flDist;

	nFwdMove = m_LocalNav->PathTraversable( pev->origin, vecbigDest, 0 );

	if ( nFwdMove )
	{
		float flSpeed = 250;

		Vector vecDest = pFollowing->pev->origin;
		vecDest.z += pFollowing->pev->mins.z;

		if ( !( pev->flags & FL_ONGROUND ) )
		{
			flSpeed = 250;
		}
		else
		{
			flDist = ( vecDest - pev->origin ).Length();
			flSpeed = flDist;
			if ( flDist >= 110 )
			{
				flSpeed = flDist >= 250 ? 400.0f : 300.0f;
			}
		}
		pev->velocity.x = vecFwd.x * flSpeed;
		pev->velocity.y = vecFwd.y * flSpeed;

		if ( nFwdMove == 3 )
		{
			if ( pev->flags & FL_ONGROUND )
			{
				pev->velocity.z = 270;
			}
		}
	}
}

BOOL CHostage::IsOnLadder( void )
{
	return pev->movetype == MOVETYPE_FLY;
}
void CHostage::NavReady( void )
{
	CBaseEntity *pFollowing;
	Vector vecDest;
	float flRadius = 40;
	node_index_t nindexPath;

	if ( !m_hTargetEnt )
	{
		return;
	}

	pFollowing = GetClassPtr( (CBaseEntity *)m_hTargetEnt->pev );

	vecDest = pFollowing->pev->origin;
	if ( !( pFollowing->pev->flags & FL_ONGROUND ) )
	{
		TraceResult tr;
		Vector vecDropDest;

		vecDropDest = vecDest;
		vecDropDest.z -= 300;

		UTIL_TraceHull( vecDest, vecDropDest, ignore_monsters, human_hull, pFollowing->edict(), &tr );
		if ( tr.fStartSolid )
			return;
		if ( tr.flFraction == 1 )
			return;
		vecDest = tr.vecEndPos;
	}

	vecDest.z += pFollowing->pev->mins.z;

	m_LocalNav->SetTargetEnt( pFollowing );
	nindexPath = m_LocalNav->FindPath( pev->origin, vecDest, flRadius, 1 );
	if ( nindexPath != -1 )
	{
		m_fHasPath = TRUE;
		nTargetNode = -1;
		m_flPathAcquired = gpGlobals->time;
		m_flPathCheckInterval = 0.5f;
		m_nPathNodes = m_LocalNav->SetupPathNodes( nindexPath, vecNodes, 1 );
	}
	else if ( !m_fHasPath )
	{
		m_flPathCheckInterval = std::min( m_flPathCheckInterval + 0.1, 0.5 );
	}
}


void CHostage::SendHostagePositionMsg( void )
{
	CBaseEntity *pEntity = NULL;

	pEntity = UTIL_FindEntityByClassname( pEntity, "player" );
	while ( pEntity != NULL && !FNullEnt( pEntity->edict() ) )
	{
		if ( pEntity->IsPlayer() && pEntity->pev->flags != FL_DORMANT )
		{
			CBasePlayer *pTempPlayer = GetClassPtr( (CBasePlayer *)pEntity->pev );

			if ( pTempPlayer->pev->deadflag == DEAD_NO && pTempPlayer->m_iTeam == CT )
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgHostagePos, NULL, pTempPlayer->pev );
					WRITE_BYTE( 0 );
					WRITE_BYTE( m_iHostageIndex );
					WRITE_COORD( pev->origin.x );
					WRITE_COORD( pev->origin.y );
					WRITE_COORD( pev->origin.z );
				MESSAGE_END();
			}
		}

		pEntity = UTIL_FindEntityByClassname( pEntity, "player" );
	}
}


void CHostage::SendHostageEventMsg( void )
{
	CBaseEntity *pEntity = NULL;

	pEntity = UTIL_FindEntityByClassname( pEntity, "player" );
	while ( pEntity != NULL && !FNullEnt( pEntity->edict() ) )
	{
		if ( pEntity->IsPlayer() && pEntity->pev->flags != FL_DORMANT )
		{
			CBasePlayer *pTempPlayer = GetClassPtr( (CBasePlayer *)pEntity->pev );

			if ( pTempPlayer->pev->deadflag == DEAD_NO && pTempPlayer->m_iTeam == CT )
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgHostageK, NULL, pTempPlayer->pev );
					WRITE_BYTE( m_iHostageIndex );
				MESSAGE_END();
			}

			if ( pTempPlayer->pev->deadflag == DEAD_NO )
			{
				pTempPlayer->SendHostageIcons();
			}
		}

		pEntity = UTIL_FindEntityByClassname( pEntity, "player" );
	}
}


void CHostage::Wiggle( void )
{
	Vector vec( 0, 0, 0 );

	Vector wiggle_directions[8] =
	{
		Vector( 50, 0, 0 ),
		Vector( -50, 0, 0 ),
		Vector( 0, 50, 0 ),
		Vector( 0, -50, 0 ),
		Vector( 50, 50, 0 ),
		Vector( 50, -50, 0 ),
		Vector( -50, 50, 0 ),
		Vector( -50, -50, 0 )
	};

	for ( int i = 0; i < 8; i++ )
	{
		Vector dest = pev->origin + wiggle_directions[i];
		if ( !m_LocalNav->PathTraversable( pev->origin, dest, 1 ) )
		{
			vec = vec - wiggle_directions[i];
		}
	}

	vec = vec + Vector( RANDOM_FLOAT( -3, 3 ), RANDOM_FLOAT( -3, 3 ), 0 );

	pev->velocity = pev->velocity + vec.Normalize() * 100;
}


void CHostage::PreThink( void )
{
	if ( m_improv )
		return;

	Vector vecSrc;
	Vector vecDest;
	TraceResult tr;
	float flOrigDist;
	float flRaisedDist;
	float flInterval = gpGlobals->frametime;

	if ( !( pev->flags & FL_ONGROUND ) )
		return;

	if ( pev->velocity.Length2D() < 1 )
		return;

	vecSrc = pev->origin;
	vecDest = vecSrc + pev->velocity * flInterval;
	vecDest.z = vecSrc.z;

	TRACE_MONSTER_HULL( edict(), vecSrc, vecDest, 0, edict(), &tr );

	if ( tr.fStartSolid )
		return;

	if ( tr.flFraction == 1 )
		return;

	if ( tr.vecPlaneNormal.z > 0.7 )
		return;

	flOrigDist = ( tr.vecEndPos - pev->origin ).Length2D();

	vecSrc.z += CLocalNav::s_flStepSize;
	vecDest = vecSrc + pev->velocity.Normalize() * 0.1f;
	vecDest.z = vecSrc.z;

	TRACE_MONSTER_HULL( edict(), vecSrc, vecDest, 0, edict(), &tr );

	if ( tr.fStartSolid )
		return;

	vecSrc = tr.vecEndPos;
	vecDest = tr.vecEndPos;
	vecDest.z -= CLocalNav::s_flStepSize;
	TRACE_MONSTER_HULL( edict(), vecSrc, vecDest, 0, edict(), &tr );

	if ( tr.vecPlaneNormal.z < 0.7 )
		return;

	flRaisedDist = ( tr.vecEndPos - pev->origin ).Length2D();

	if ( flRaisedDist > flOrigDist )
	{
		Vector vecNewOrigin( pev->origin );

		vecNewOrigin.z = tr.vecEndPos.z;

		UTIL_SetOrigin( pev, vecNewOrigin );

		pev->velocity.z += g_psv_gravity->value * pev->gravity * flInterval;
	}
}


void Hostage_RegisterCVars( void )
{
	CVAR_REGISTER( &cv_hostage_debug );
	CVAR_REGISTER( &cv_hostage_stop );
}


void InstallHostageManager( void )
{
	if ( g_pHostages )
		delete g_pHostages;

	g_pHostages = new CHostageManager;
}

#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
CHostageManager::CHostageManager( void )
{
	m_hostageCount = 0;
}


void CHostageManager::ServerActivate( void )
{
	m_hostageCount = 0;

	CBaseEntity *pHostage = NULL;

	pHostage = UTIL_FindEntityByClassname( pHostage, "hostage_entity" );

	while ( pHostage )
	{
		AddHostage( (CHostage *)pHostage );

		pHostage = UTIL_FindEntityByClassname( pHostage, "hostage_entity" );
	}

	if ( UTIL_IsGame( "czero" ) )
	{
		m_chatter.AddSound( HOSTAGE_CHATTER_START_FOLLOW, "hostage/huse/getouttahere.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_START_FOLLOW, "hostage/huse/illfollow.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_START_FOLLOW, "hostage/huse/letsdoit.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_START_FOLLOW, "hostage/huse/letsgo.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_START_FOLLOW, "hostage/huse/letshurry.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_START_FOLLOW, "hostage/huse/letsmove.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_START_FOLLOW, "hostage/huse/okletsgo.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_START_FOLLOW, "hostage/huse/youlead.wav" );

		m_chatter.AddSound( HOSTAGE_CHATTER_STOP_FOLLOW, "hostage/hunuse/comeback.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_STOP_FOLLOW, "hostage/hunuse/dontleaveme.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_STOP_FOLLOW, "hostage/hunuse/illstayhere.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_STOP_FOLLOW, "hostage/hunuse/notleaveme.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_STOP_FOLLOW, "hostage/hunuse/yeahillstay.wav" );

		m_chatter.AddSound( HOSTAGE_CHATTER_INTIMIDATED, "hostage/hseenbyt/donthurtme1.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_INTIMIDATED, "hostage/hseenbyt/donthurtme2.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_INTIMIDATED, "hostage/hseenbyt/dontkill.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_INTIMIDATED, "hostage/hseenbyt/endpeace.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_INTIMIDATED, "hostage/hseenbyt/nevernegotiate.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_INTIMIDATED, "hostage/hseenbyt/nottellanything.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_INTIMIDATED, "hostage/hseenbyt/surrender.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_INTIMIDATED, "hostage/hseenbyt/whatdoyou.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_INTIMIDATED, "hostage/hseenbyt/whyareyou.wav" );

		m_chatter.AddSound( HOSTAGE_CHATTER_RETREAT, "hostage/hseenbyt/donthurtme1.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_RETREAT, "hostage/hseenbyt/donthurtme2.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_RETREAT, "hostage/hseenbyt/dontkill.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_RETREAT, "hostage/hseenbyt/dontkill.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_RETREAT, "hostage/hretreat/illgoback1.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_RETREAT, "hostage/hretreat/illgoback2.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_RETREAT, "hostage/hretreat/okokgoing.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_RETREAT, "hostage/hretreat/okokgoing2.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_RETREAT, "hostage/hretreat/sorry1.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_RETREAT, "hostage/hretreat/sorry2.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_RETREAT, "hostage/hretreat/sorry3.wav" );

		m_chatter.AddSound( HOSTAGE_CHATTER_PAIN, "hostage/hpain/hpain1.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_PAIN, "hostage/hpain/hpain2.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_PAIN, "hostage/hpain/hpain3.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_PAIN, "hostage/hpain/hpain4.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_PAIN, "hostage/hpain/hpain5.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_PAIN, "hostage/hpain/hpain6.wav" );

		m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_GUNFIRE, "hostage/hreactions/awwcrap1.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_GUNFIRE, "hostage/hreactions/awwcrap2.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_GUNFIRE, "hostage/hreactions/getdown1.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_GUNFIRE, "hostage/hreactions/getdown2.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_GUNFIRE, "hostage/hreactions/getdown3.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_GUNFIRE, "hostage/hreactions/lookout1.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_GUNFIRE, "hostage/hreactions/lookout2.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_GUNFIRE, "hostage/hreactions/lookshooting1.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_GUNFIRE, "hostage/hreactions/lookshooting2.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_GUNFIRE, "hostage/hreactions/lookshooting3.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_GUNFIRE, "hostage/hreactions/ohgod1.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_GUNFIRE, "hostage/hreactions/ohgod2.wav" );

		m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_MURDER, "hostage/hreactions/awwcrap1.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_MURDER, "hostage/hreactions/awwcrap2.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_MURDER, "hostage/hreactions/deargod1.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_MURDER, "hostage/hreactions/deargod2.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_MURDER, "hostage/hreactions/deargod3.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_MURDER, "hostage/hreactions/ohgod1.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_MURDER, "hostage/hreactions/ohgod2.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_MURDER, "hostage/hreactions/ohno1.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_MURDER, "hostage/hreactions/ohno2.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_MURDER, "hostage/hreactions/awww2.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_MURDER, "hostage/hreactions/awww4.wav" );

		m_chatter.AddSound( HOSTAGE_CHATTER_LOOK_OUT, "hostage/hreactions/awwcrap1.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_LOOK_OUT, "hostage/hreactions/awwcrap2.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_LOOK_OUT, "hostage/hreactions/lookout1.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_LOOK_OUT, "hostage/hreactions/lookout2.wav" );

		m_chatter.AddSound( HOSTAGE_CHATTER_PLEASE_RESCUE_ME, "hostage/hseenbyct/areyousave.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_PLEASE_RESCUE_ME, "hostage/hseenbyct/getmeoutta1.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_PLEASE_RESCUE_ME, "hostage/hseenbyct/getmeoutta2.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_PLEASE_RESCUE_ME, "hostage/hseenbyct/imahostage.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_PLEASE_RESCUE_ME, "hostage/hseenbyct/rescueme1.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_PLEASE_RESCUE_ME, "hostage/hseenbyct/rescueme2.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_PLEASE_RESCUE_ME, "hostage/hseenbyct/theyregonna.wav" );

		m_chatter.AddSound( HOSTAGE_CHATTER_IMPATIENT_FOR_RESCUE, "hostage/hseenbyct/areyousave.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_IMPATIENT_FOR_RESCUE, "hostage/hseenbyct/getmeoutta1.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_IMPATIENT_FOR_RESCUE, "hostage/hseenbyct/getmeoutta2.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_IMPATIENT_FOR_RESCUE, "hostage/hseenbyct/rescueme1.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_IMPATIENT_FOR_RESCUE, "hostage/hseenbyct/rescueme2.wav" );

		m_chatter.AddSound( HOSTAGE_CHATTER_SEE_RESCUE_ZONE, "hostage/hseezone/almostouttahere.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_SEE_RESCUE_ZONE, "hostage/hseezone/almostthere.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_SEE_RESCUE_ZONE, "hostage/hseezone/keepgoing.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_SEE_RESCUE_ZONE, "hostage/hseezone/notfar.wav" );

		m_chatter.AddSound( HOSTAGE_CHATTER_CTS_WIN, "hostage/hctwin/alldead.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_CTS_WIN, "hostage/hctwin/goodnews.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_CTS_WIN, "hostage/hctwin/outtahere.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_CTS_WIN, "hostage/hctwin/over1.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_CTS_WIN, "hostage/hctwin/over2.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_CTS_WIN, "hostage/hctwin/over3.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_CTS_WIN, "hostage/hctwin/over4.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_CTS_WIN, "hostage/hctwin/over5.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_CTS_WIN, "hostage/hctwin/relief.wav" );

		m_chatter.AddSound( HOSTAGE_CHATTER_TERRORISTS_WIN, "hostage/htwin/doomed.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_TERRORISTS_WIN, "hostage/htwin/godno.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_TERRORISTS_WIN, "hostage/htwin/nowwhat.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_TERRORISTS_WIN, "hostage/htwin/ohman.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_TERRORISTS_WIN, "hostage/htwin/ohno.wav" );

		m_chatter.AddSound( HOSTAGE_CHATTER_RESCUED, "hostage/hrescued/finally.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_RESCUED, "hostage/hrescued/finallysafe.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_RESCUED, "hostage/hrescued/thankyou.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_RESCUED, "hostage/hrescued/wemadeit1.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_RESCUED, "hostage/hrescued/wemadeit2.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_RESCUED, "hostage/hrescued/wemadeit3.wav" );

		m_chatter.AddSound( HOSTAGE_CHATTER_WARN_NEARBY, "hostage/hwarn/becareful1.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_WARN_NEARBY, "hostage/hwarn/becareful2.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_WARN_NEARBY, "hostage/hwarn/stillaround1.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_WARN_NEARBY, "hostage/hwarn/stillaround2.wav" );

		m_chatter.AddSound( HOSTAGE_CHATTER_WARN_SPOTTED, "hostage/hwarn/lookout1.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_WARN_SPOTTED, "hostage/hwarn/lookout2.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_WARN_SPOTTED, "hostage/hwarn/overthere1.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_WARN_SPOTTED, "hostage/hwarn/overthere2.wav" );

		m_chatter.AddSound( HOSTAGE_CHATTER_CALL_TO_RESCUER, "hostage/hwavect/helpme1.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_CALL_TO_RESCUER, "hostage/hwavect/helpme2.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_CALL_TO_RESCUER, "hostage/hwavect/hey1.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_CALL_TO_RESCUER, "hostage/hwavect/hey2.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_CALL_TO_RESCUER, "hostage/hwavect/overhere1.wav" );

		m_chatter.AddSound( HOSTAGE_CHATTER_COUGH, "hostage/hsmoke/cough1.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_COUGH, "hostage/hsmoke/cough2.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_COUGH, "hostage/hsmoke/cough3.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_COUGH, "hostage/hsmoke/cough4.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_COUGH, "hostage/hsmoke/cough5.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_COUGH, "hostage/hsmoke/cough6.wav" );

		m_chatter.AddSound( HOSTAGE_CHATTER_BLINDED, "hostage/hflash/cantsee.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_BLINDED, "hostage/hflash/myeyes.wav" );

		m_chatter.AddSound( HOSTAGE_CHATTER_SAW_HE_GRENADE, "hostage/hgrenade/grenade1.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_SAW_HE_GRENADE, "hostage/hgrenade/grenade2.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_SAW_HE_GRENADE, "hostage/hgrenade/run.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_SAW_HE_GRENADE, "hostage/hgrenade/watchout.wav" );

		m_chatter.AddSound( HOSTAGE_CHATTER_DEATH_CRY, "hostage/hdie/hdeath1.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_DEATH_CRY, "hostage/hdie/hdeath2.wav" );
		m_chatter.AddSound( HOSTAGE_CHATTER_DEATH_CRY, "hostage/hdie/hdeath3.wav" );
	}
}


void CHostageManager::ServerDeactivate( void )
{
}


void CHostageManager::RestartRound( void )
{
	for ( int i = 0; i < m_hostageCount; i++ )
		if ( m_hostage[i]->m_improv )
			m_hostage[i]->m_improv->OnReset();
}


void CHostageManager::AddHostage( CHostage *hostage )
{
	if ( m_hostageCount == 12 )
		return;

	for ( int i = 0; i < m_hostageCount; i++ )
		if ( m_hostage[i] == hostage )
			return;

	m_hostage[m_hostageCount++] = hostage;
}


bool CHostageManager::IsNearbyHostageTalking( CHostageImprov *improv ) const
{
	for ( int i = 0; i < m_hostageCount; i++ )
	{
		if ( !m_hostage[i]->m_improv )
			continue;
		const CHostageImprov *other = m_hostage[i]->m_improv;

		if ( !other->IsAlive() )
			continue;

		if ( other == improv )
			continue;

		const float closeRange = 500.0f;
		if ( ( improv->GetCentroid() - other->GetCentroid() ).IsLengthGreaterThan( closeRange ) )
			continue;

		if ( other->IsTalking() )
			return true;
	}

	return false;
}


bool CHostageManager::IsNearbyHostageJumping( CHostageImprov *improv ) const
{
	for ( int i = 0; i < m_hostageCount; i++ )
	{
		if ( !m_hostage[i]->m_improv )
			continue;
		const CHostageImprov *other = m_hostage[i]->m_improv;

		if ( !other->IsAlive() )
			continue;

		if ( other == improv )
			continue;

		const float closeRange = 500.0f;
		if ( ( improv->GetCentroid() - other->GetCentroid() ).IsLengthGreaterThan( closeRange ) )
			continue;

		if ( other->IsJumping() )
			return true;
	}

	return false;
}


void CHostageManager::OnEvent( GameEventType event, CBaseEntity *entity, CBaseEntity *other )
{
	for ( int i = 0; i < m_hostageCount; i++ )
		if ( m_hostage[i]->m_improv )
			m_hostage[i]->m_improv->OnGameEvent( event, entity, other );
}


SimpleChatter::SimpleChatter( void )
{
	for ( int i = 0; i < NUM_HOSTAGE_CHATTER_TYPES; i++ )
	{
		m_chatter[i].count = 0;
		m_chatter[i].index = 0;
		m_chatter[i].needsShuffle = false;
	}
}


SimpleChatter::~SimpleChatter( void )
{
	for ( int i = 0; i < NUM_HOSTAGE_CHATTER_TYPES; i++ )
	{
		for ( int f = 0; f < m_chatter[i].count; f++ )
		{
			delete[] m_chatter[i].file[f].filename;
		}
	}
}


void SimpleChatter::AddSound( HostageChatterType type, char *filename )
{
	ChatterSet *chatter = &m_chatter[type];

	chatter->file[chatter->count].filename = CloneString( filename );

	char actualFilename[128];
	_snprintf( actualFilename, sizeof( actualFilename ), "sound\\%s", filename );
	chatter->file[chatter->count].duration = (float)( g_engfuncs.pfnGetApproxWavePlayLen( actualFilename ) / 1000.0 );

	chatter->count++;

	chatter->needsShuffle = true;

	PRECACHE_SOUND( filename );
}


void SimpleChatter::Shuffle( ChatterSet *chatter )
{
	for ( int i = 0; i < chatter->count; i++ )
	{
		for ( int j = i + 1; j < chatter->count; j++ )
		{
			if ( RANDOM_LONG( 0, 100 ) <= 49 )
			{
				SoundFile tmp = chatter->file[i];
				chatter->file[i] = chatter->file[j];
				chatter->file[j] = tmp;
			}
		}
	}
}


char *SimpleChatter::GetSound( HostageChatterType type, float *duration )
{
	ChatterSet *chatter = &m_chatter[type];

	if ( chatter->needsShuffle )
	{
		Shuffle( chatter );
		chatter->needsShuffle = false;
	}

	char *sound = chatter->file[chatter->index].filename;
	*duration = chatter->file[chatter->index].duration;

	if ( ++chatter->index >= chatter->count )
		chatter->index = 0;

	return sound;
}


float SimpleChatter::PlaySound( CBaseEntity *entity, HostageChatterType type )
{
	CHostage *hostage = (CHostage *)entity;

	float duration;
	char *sound = GetSound( type, &duration );
	if ( !sound )
		return 0;

	int pitch = 0;
	switch ( hostage->m_whichModel )
	{
	case CHostage::REGULAR_GUY: pitch = 92; break;
	case CHostage::OLD_GUY: pitch = 88; break;
	case CHostage::BLACK_GUY: pitch = 86; break;
	case CHostage::GOOFY_GUY: pitch = 100; break;
	}

	int attenuation = 1;

	EMIT_SOUND_DYN( hostage->edict(), CHAN_VOICE, sound, 1, attenuation, 0, pitch );

	if ( type == HOSTAGE_CHATTER_CALL_TO_RESCUER )
	{
		TheBots->OnEvent( EVENT_HOSTAGE_CALLED_FOR_HELP, entity, NULL );
	}

	return duration;
}
