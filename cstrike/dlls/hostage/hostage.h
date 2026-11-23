#ifndef HOSTAGE_H
#define HOSTAGE_H

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "basemonster.h"
#include "GameEvent.h"
#include "hostage_improv.h"

class CLocalNav;
class CBasePlayer;

enum state
{
	FOLLOW,
	STAND,
	DUCK,
	SCARED,
	IDLE,
	FOLLOWPATH
};

class CHostage : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	void EXPORT IdleThink();
	void Remove();
	void RePosition();
	void SetActivity( int act );
	int ObjectCaps() override;
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value ) override;
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType ) override;
	void Touch( CBaseEntity *pOther ) override;
	int Classify() override { return CLASS_HUMAN_PASSIVE; }
	int GetActivity() { return m_Activity; }
	int BloodColor() override { return BLOOD_COLOR_RED; }
	float GetModifiedDamage( float flDamage, int nHitGroup );
	void SetFlinchActivity();
	void SetDeathActivity();
	void PlayPainSound();
	void PlayFollowRescueSound();
	void AnnounceDeath( CBasePlayer *pAttacker );
	void ApplyHostagePenalty( CBasePlayer *pAttacker );
	void GiveCTTouchBonus( CBasePlayer *pPlayer );
	void SendHostagePositionMsg();
	void SendHostageEventMsg();
	void DoFollow();
	BOOL IsOnLadder();
	void PointAt( const Vector &vecLoc );
	void MoveToward( const Vector &vecLoc );
	void NavReady();
	void Wiggle();
	void PreThink();
	bool IsFollowingSomeone( void )
	{
		if ( !IsValid() )
			return false;
		if ( m_improv )
			return m_improv->IsFollowing();

		if ( m_hTargetEnt != NULL && m_State == FOLLOW )
			return true;

		return false;
	}
	CBaseEntity *GetLeader( void )
	{
		if ( !IsValid() )
			return NULL;
		if ( m_improv )
		{
			return m_improv->GetFollowLeader();
		}
		else
		{
			if ( IsFollowingSomeone() )
				return m_hTargetEnt;
		}

		return NULL;
	}
	bool IsFollowing( const CBaseEntity *entity )
	{
		if ( !IsValid() )
			return false;
		if ( m_improv )
			return m_improv->IsFollowing( entity );

		if ( entity == m_hTargetEnt && m_State == FOLLOW )
			return true;

		return false;
	}
	bool IsValid() { return pev->takedamage == DAMAGE_YES; }
	bool IsDead() { return pev->deadflag == DEAD_DEAD; }
	bool IsAtHome() const { return (pev->origin - m_vStart).LengthSquared() < 400; }
	const Vector &GetHomePosition() const { return m_vStart; }

	int m_Activity;
	BOOL m_bTouched;
	BOOL m_bRescueMe;
	float m_flFlinchTime;
	float m_flNextChange;
	float m_flMarkPosition;
	int m_iModel;
	int m_iSkin;
	float m_flNextRadarTime;
	state m_State;
	Vector m_vStart;
	Vector m_vStartAngles;
	Vector m_vPathToFollow[20];
	int m_iWaypoint;
	CBasePlayer *m_target;
	CLocalNav *m_LocalNav;
	int nTargetNode;
	Vector vecNodes[100];
	EHANDLE m_hStoppedTargetEnt;
	float m_flNextFullThink;
	float m_flPathCheckInterval;
	float m_flLastPathCheck;
	int m_nPathNodes;
	BOOL m_fHasPath;
	float m_flPathAcquired;
	Vector m_vOldPos;
	int m_iHostageIndex;
	BOOL m_bStuck;
	float m_flStuckTime;
	CHostageImprov *m_improv;
	enum ModelType
	{
		REGULAR_GUY,
		OLD_GUY,
		BLACK_GUY,
		GOOFY_GUY
	};
	ModelType m_whichModel;
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
};

class SimpleChatter
{
public:
	SimpleChatter();
	~SimpleChatter();
	void AddSound( HostageChatterType type, char *filename );
	float PlaySound( CBaseEntity *entity, HostageChatterType type );

private:
	struct SoundFile
	{
		char *filename;
		float duration;
	};
	struct ChatterSet
	{
		SoundFile file[32];
		int count;
		int index;
		bool needsShuffle;
	};
	char *GetSound( HostageChatterType type, float *duration );
	void Shuffle( ChatterSet *chatter );
	ChatterSet m_chatter[NUM_HOSTAGE_CHATTER_TYPES];
};

class CHostageManager
{
	friend class CHostageImprov;
	friend class CCSBot;
public:
	CHostageManager();
	void ServerActivate();
	void ServerDeactivate();
	void RestartRound();
	void AddHostage( CHostage *hostage );
	SimpleChatter *GetChatter() { return &m_chatter; }
	bool IsNearbyHostageTalking( CHostageImprov *improv ) const;
	bool IsNearbyHostageJumping( CHostageImprov *improv ) const;
	void OnEvent( GameEventType event, CBaseEntity *entity, CBaseEntity *other );
	CHostage *GetClosestHostage( const Vector &position, float *range )
	{
		float closestRange = 100000000;
		CHostage *closest = NULL;
		for ( int i = 0; i < m_hostageCount; ++i )
		{
			float distance = (m_hostage[i]->pev->origin - position).Length();
			if ( closestRange > distance )
			{
				closestRange = distance;
				closest = m_hostage[i];
			}
		}
		if ( range )
			*range = closestRange;
		return closest;
	}

	template< typename Functor >
	bool ForEachHostage( Functor &func ) const
	{
		for ( int i = 0; i < m_hostageCount; ++i )
		{
			if ( m_hostage[i]->pev->deadflag == DEAD_DEAD )
				continue;

			if ( func( m_hostage[i] ) == false )
				return false;
		}

		return true;
	}

private:
	CHostage *m_hostage[12];
	int m_hostageCount;
	SimpleChatter m_chatter;
};

extern CHostageManager *g_pHostages;
extern cvar_t cv_hostage_debug;
extern cvar_t cv_hostage_stop;

void Hostage_RegisterCVars();
void InstallHostageManager();

#endif
