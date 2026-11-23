//========= Copyright (c) 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

#ifndef CS_BOT_MANAGER_H
#define CS_BOT_MANAGER_H

#include "bot.h"
#include "bot_profile.h"
#include "nav.h"
#include "nav_area.h"

class CCSBot;
class CNavArea;

//--------------------------------------------------------------------------------------------------------------
//
// The manager for Counter-Strike specific bots
//
class CCSBotManager : public CBotManager
{
public:
	CCSBotManager( void );
	virtual void ClientDisconnect( CBasePlayer *player );
	virtual BOOL ClientCommand( CBasePlayer *player, const char *command );
	virtual void ServerActivate( void );
	virtual void ServerDeactivate( void );
	virtual void ServerCommand( const char *command );
	virtual void AddServerCommand( const char *command );
	virtual void AddServerCommands( void );
	virtual void RestartRound( void );						///< (EXTEND) invoked when a new round begins
	virtual void StartFrame( void );						///< (EXTEND) called each frame
	virtual void OnEvent( GameEventType event, CBaseEntity *entity = NULL, CBaseEntity *other = NULL );
	virtual unsigned int GetPlayerPriority( CBasePlayer *player ) const;	///< return priority of player (0 = max pri)
	virtual bool IsImportantPlayer( CBasePlayer *player ) const;				///< return true if player is important to scenario (VIP, bomb carrier, etc)

	enum GameScenarioType
	{
		SCENARIO_DEATHMATCH,
		SCENARIO_DEFUSE_BOMB,
		SCENARIO_RESCUE_HOSTAGES,
		SCENARIO_ESCORT_VIP
	};

	struct Zone
	{
		CBaseEntity *m_entity;									///< the map entity
		CNavArea *m_area[16];									///< nav areas that overlap this zone
		int m_areaCount;
		Vector m_center;
		bool m_isLegacy;										///< if true, use pev->origin and 256 unit radius as zone
		int m_index;
		Extent m_extent;
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
	};

	void ValidateMapData( void );
	const Zone *GetZone( const Vector *position ) const;				///< return the zone that contains the given position
	const Zone *GetZone( int index ) const { return &m_zone[index]; }
	const Zone *GetClosestZone( const Vector *position ) const;		///< return the closest zone to the given position

	template< typename CostFunctor >
	const Zone *GetClosestZone( CNavArea *startArea, CostFunctor costFunc ) const	///< return the zone closest to the given position, using the given cost heuristic
	{
		const Zone *closeZone = NULL;
		float closeDist = 99999999.9f;

		if ( startArea == NULL )
			return NULL;

		for ( int i = 0; i < m_zoneCount; ++i )
		{
			if ( m_zone[i].m_areaCount == 0 )
				continue;

			// just use the first overlapping nav area as a reasonable approximation
			float dist = NavAreaTravelDistance( startArea, m_zone[i].m_area[0], costFunc );

			if ( dist < closeDist )
			{
				closeZone = &m_zone[i];
				closeDist = dist;
			}
		}

		return closeZone;
	}

	CNavArea *GetRandomAreaInZone( const Zone *zone ) const;			///< return a random area inside the given zone
	const Vector *GetRandomPositionInZone( const Zone *zone ) const;	///< return a random position inside the given zone
	bool IsOnDefense( const CBasePlayer *player ) const;		///< return true if this player is on "defense"
	bool IsOnOffense( const CBasePlayer *player ) const;		///< return true if this player is on "offense"
	bool IsWeaponUseable( CBasePlayerItem *weapon );	///< return true if the bot can use this weapon
	void SetLooseBomb( CBaseEntity *bomb );
	float GetBombTimeLeft( void ) const;											///< get the time remaining before the planted bomb explodes
	float GetRadioMessageTimestamp( GameEventType event, int team ) const;			///< return the last time the given radio message was sent for given team
	float GetRadioMessageInterval( GameEventType event, int team ) const;				///< return the interval since the last time this message was sent
	void SetRadioMessageTimestamp( GameEventType event, int team );
	void ResetRadioMessageTimestamps( void );
	bool AddBot( const BotProfile *profile, BotProfileTeamType requestedTeam );
	bool BotAddCommand( BotProfileTeamType team, bool isFromConsole );	///< process the "bot_add" console command
	void MaintainBotQuota( void );
	void MonitorBotCVars( void );

	GameScenarioType GetScenario( void ) const { return m_scenario; }
	int GetZoneCount( void ) const { return m_zoneCount; }
	const Zone *GetRandomZone( void ) const
	{
		if ( m_zoneCount == 0 )
			return NULL;
		return &m_zone[RANDOM_LONG( 0, m_zoneCount - 1 )];
	}
	bool IsBombPlanted( void ) const { return m_isBombPlanted; }			///< returns true if bomb has been planted
	bool IsRoundOver( void ) const { return m_isRoundOver; }		///< return true if the round has ended
	float GetElapsedRoundTime( void ) const { return gpGlobals->time - m_roundStartTimestamp; }	///< return the elapsed time since the current round began
	float GetRoundStartTimestamp( void ) const { return m_roundStartTimestamp; }
	Place GetNavPlace( void ) const { return m_navPlace; }
	void SetNavPlace( Place place ) { m_navPlace = place; }
	CBasePlayer *GetBombDefuser( void ) const { return m_bombDefuser; }			///< return the player currently defusing the bomb, or NULL
	CBaseEntity *GetLooseBomb( void ) { return m_looseBomb; }				///< return the bomb if it is loose on the ground
	CNavArea *GetLooseBombArea( void ) const { return m_looseBombArea; }			///< return area that bomb is in/near
	bool IsDefenseRushing( void ) const { return m_isDefenseRushing; }		///< returns true if defense team has "decided" to rush this round
	static bool IsLearningMap( void ) { return m_isLearningMap; }
	static bool IsAnalysisRequested( void ) { return m_isAnalysisRequested; }

private:
	friend class CCSBot;
	friend class HuntState;
	friend class IdleState;
	friend class HideState;
	GameScenarioType m_scenario;						///< what kind of game are we playing
	Zone m_zone[4];
	int m_zoneCount;
	bool m_isBombPlanted;									///< true if bomb has been planted
	float m_bombPlantTimestamp;								///< time bomb was planted
	float m_earliestBombPlantTimestamp;						///< don't allow planting until after this time has elapsed
	CBasePlayer *m_bombDefuser;								///< the player currently defusing a bomb
	EHANDLE m_looseBomb;									///< will be non-NULL if bomb is loose on the ground
	CNavArea *m_looseBombArea;								///< area that bomb is is/near
	bool m_isRoundOver;										///< true if the round has ended
	float m_radioMsgTimestamp[24][2];
	float m_lastSeenEnemyTimestamp;
	float m_roundStartTimestamp;							///< the time when the current round began
	bool m_isDefenseRushing;								///< whether defensive team is rushing this round or not
	Place m_navPlace;
	CountdownTimer m_respawnTimer;
	bool m_isRespawnStarted;
	bool m_canRespawn;
	bool m_bServerActive;									///< true between ServerActivate() and ServerDeactivate()

	static float m_flNextCVarCheck;
	static NavEditCmdType m_editCmd;
	static bool m_isMapDataLoaded;							///< true if we've attempted to load map data
	static bool m_isLearningMap;
	static bool m_isAnalysisRequested;
};

inline CCSBotManager *TheCSBots( void )
{
	return static_cast<CCSBotManager *>( TheBots );
}

#endif
