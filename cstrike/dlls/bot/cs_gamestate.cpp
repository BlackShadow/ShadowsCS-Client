//========= Copyright (c) 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "cs_bot.h"
#include "cs_bot_manager.h"
#include "hostage.h"

//--------------------------------------------------------------------------------------------------------------
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
CSGameState::CSGameState( CCSBot *owner )
{
	m_owner = owner;
	m_isRoundOver = false;

	m_bombState = MOVING;
	m_isPlantedBombPosKnown = false;
	m_plantedBombsite = UNKNOWN;

	m_bombsiteCount = 0;
	m_bombsiteSearchIndex = 0;

	for ( int i = 0; i < ARRAYSIZE( m_hostage ); ++i )
	{
		m_hostage[i].hostage = NULL;
		m_hostage[i].knownPos = Vector( 0.0f, 0.0f, 0.0f );
		m_hostage[i].isValid = false;
		m_hostage[i].isAlive = false;
		m_hostage[i].isFree = true;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Reset at round start
 */
void CSGameState::Reset( void )
{
	m_isRoundOver = false;

	// bomb -----------------------------------------------------------------------
	m_bombState = MOVING;
	m_lastSawBomber.Invalidate();
	m_lastSawLooseBomb.Invalidate();
	m_isPlantedBombPosKnown = false;
	m_plantedBombsite = UNKNOWN;

	m_bombsiteCount = TheCSBots()->GetZoneCount();

	int i;
	for ( i = 0; i < m_bombsiteCount; ++i )
	{
		m_isBombsiteClear[i] = false;
		m_bombsiteSearchOrder[i] = i;
	}

	// shuffle the bombsite search order
	// allows T's to plant at random site, and TEAM_CT's to search in a random order
	// NOTE: VS6 std::random_shuffle() doesn't work well with an array of two elements (most maps)
	for ( i = 0; i < m_bombsiteCount; ++i )
	{
		int swap = m_bombsiteSearchOrder[i];
		int rnd = RANDOM_LONG( i, m_bombsiteCount - 1 );
		m_bombsiteSearchOrder[i] = m_bombsiteSearchOrder[ rnd ];
		m_bombsiteSearchOrder[ rnd ] = swap;
	}

	m_bombsiteSearchIndex = 0;

	// hostage ---------------------------------------------------------------------
	InitializeHostageInfo();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Update game state based on events we have received
 */
void CSGameState::OnEvent( GameEventType event, CBaseEntity *, CBaseEntity *other )
{
	switch ( event )
	{
	case EVENT_BOMB_PLANTED:
		SetBombState( PLANTED );
		if ( m_owner->m_iTeam == TERRORIST && other != NULL )
			UpdatePlantedBomb( &other->pev->origin );
		break;
	case EVENT_BOMB_DEFUSED:
		SetBombState( DEFUSED );
		break;
	case EVENT_BOMB_EXPLODED:
		SetBombState( EXPLODED );
		break;
	case EVENT_ALL_HOSTAGES_RESCUED:
		m_allHostagesRescued = true;
		break;
	case EVENT_TERRORISTS_WIN:
	case EVENT_CTS_WIN:
	case EVENT_ROUND_DRAW:
		m_isRoundOver = true;
		break;
	default:
		break;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * True if round has been won or lost (but not yet reset)
 */
bool CSGameState::IsRoundOver( void ) const
{
	return m_isRoundOver;
}

//--------------------------------------------------------------------------------------------------------------
void CSGameState::SetBombState( BombState state )
{
	// if state changed, reset "last seen" timestamps
	if ( m_bombState != state )
		m_bombState = state;
}

//--------------------------------------------------------------------------------------------------------------
void CSGameState::UpdateLooseBomb( const Vector *pos )
{
	m_looseBombPos = *pos;
	m_lastSawLooseBomb.Start();

	// we saw the loose bomb, update our state
	SetBombState( LOOSE );
}

//--------------------------------------------------------------------------------------------------------------
float CSGameState::TimeSinceLastSawLooseBomb( void ) const
{
	return m_lastSawLooseBomb.GetElapsedTime();
}

//--------------------------------------------------------------------------------------------------------------
bool CSGameState::IsLooseBombLocationKnown( void ) const
{
	return m_bombState == LOOSE && m_lastSawLooseBomb.HasStarted();
}

//--------------------------------------------------------------------------------------------------------------
void CSGameState::UpdateBomber( const Vector *pos )
{
	m_bomberPos = *pos;
	m_lastSawBomber.Start();

	// we saw the bomber, update our state
	SetBombState( MOVING );
}

//--------------------------------------------------------------------------------------------------------------
float CSGameState::TimeSinceLastSawBomber( void ) const
{
	return m_lastSawBomber.GetElapsedTime();
}

//--------------------------------------------------------------------------------------------------------------
bool CSGameState::IsPlantedBombLocationKnown( void ) const
{
	return m_bombState == PLANTED && m_isPlantedBombPosKnown;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return the zone index of the planted bombsite, or UNKNOWN
 */
int CSGameState::GetPlantedBombsite( void ) const
{
	return m_bombState == PLANTED ? m_plantedBombsite : UNKNOWN;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we are currently in the bombsite where the bomb is planted
 */
bool CSGameState::IsAtPlantedBombsite( void ) const
{
	if ( m_bombState != PLANTED )
		return false;

	const CCSBotManager::Zone *zone = TheCSBots()->GetClosestZone( &m_owner->pev->origin );
	return zone != NULL && m_plantedBombsite == zone->m_index;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return the zone index of the next bombsite to search
 */
int CSGameState::GetNextBombsiteToSearch( void )
{
	if ( m_bombsiteCount <= 0 )
		return 0;

	for ( ;; )
	{
		// return next non-cleared bombsite index
		for ( int i = m_bombsiteSearchIndex; i < m_bombsiteCount; ++i )
		{
			int bombsite = m_bombsiteSearchOrder[i];
			if ( !m_isBombsiteClear[bombsite] )
			{
				m_bombsiteSearchIndex = i;
				return bombsite;
			}
		}

		// all the bombsites are clear, someone must have been mistaken - start search over
		for ( int i = 0; i < m_bombsiteCount; ++i )
			m_isBombsiteClear[i] = false;
		m_bombsiteSearchIndex = 0;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Returns position of bomb in its various states (moving, loose, planted),
 * or NULL if we don't know where the bomb is
 */
const Vector *CSGameState::GetBombPosition( void ) const
{
	switch ( m_bombState )
	{
	case MOVING:
		return m_lastSawBomber.HasStarted() ? &m_bomberPos : NULL;
	case LOOSE:
		return m_lastSawLooseBomb.HasStarted() ? &m_looseBombPos : NULL;
	case PLANTED:
		return m_isPlantedBombPosKnown ? &m_plantedBombPos : NULL;
	default:
		return NULL;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * We see the planted bomb at 'pos'
 */
void CSGameState::UpdatePlantedBomb( const Vector *pos )
{
	const CCSBotManager::Zone *zone = TheCSBots()->GetClosestZone( pos );
	if ( zone != NULL )
		m_plantedBombsite = zone->m_index;
	else
	{
		CONSOLE_ECHO( "ERROR: Bomb planted outside of a zone!\n" );
		m_plantedBombsite = UNKNOWN;
	}

	m_plantedBombPos = *pos;
	m_isPlantedBombPosKnown = true;
	SetBombState( PLANTED );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Someone told us where the bomb is planted
 */
void CSGameState::MarkBombsiteAsPlanted( int zoneIndex )
{
	m_plantedBombsite = zoneIndex;
	SetBombState( PLANTED );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Someone told us a bombsite is clear
 */
void CSGameState::ClearBombsite( int zoneIndex )
{
	if ( zoneIndex >= 0 && zoneIndex < m_bombsiteCount )
		m_isBombsiteClear[zoneIndex] = true;
}

//--------------------------------------------------------------------------------------------------------------
bool CSGameState::IsBombsiteClear( int zoneIndex ) const
{
	return zoneIndex >= 0 && zoneIndex < m_bombsiteCount && m_isBombsiteClear[zoneIndex];
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Initialize our knowledge of the number and location of hostages
 */
void CSGameState::InitializeHostageInfo( void )
{
	m_hostageCount = 0;
	m_allHostagesRescued = false;
	m_haveSomeHostagesBeenTaken = false;

	CBaseEntity *entity = NULL;
	while ( (entity = UTIL_FindEntityByClassname( entity, "hostage_entity" )) != NULL )
	{
		if ( m_hostageCount >= ARRAYSIZE( m_hostage ) )
			break;

		if ( entity->pev->takedamage == DAMAGE_YES )
		{
			HostageInfo &info = m_hostage[m_hostageCount++];
			info.hostage = static_cast<CHostage *>( entity );
			info.knownPos = entity->pev->origin;
			info.isValid = true;
			info.isAlive = true;
			info.isFree = true;
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return the closest free and live hostage
 * If we are a CT this information is perfect.
 * Otherwise, this is based on our individual memory of the game state.
 * If NULL is returned, we don't think there are any hostages left, or we dont know where they are.
 * NOTE: a T can remember a hostage who has died.  knowPos will be filled in, but NULL will be
 *       returned, since CHostages get deleted when they die.
 */
CHostage *CSGameState::GetNearestFreeHostage( Vector *knowPos ) const
{
	if ( m_owner == NULL || m_owner->GetLastKnownArea() == NULL )
		return NULL;

	CNavArea *startArea = m_owner->GetLastKnownArea();

	CHostage *close = NULL;
	const Vector *closePos = NULL;
	float closeDistance = 10000000000.0f;

	for ( int i = 0; i < m_hostageCount; ++i )
	{
		const HostageInfo &info = m_hostage[i];
		const Vector *hostagePos;

		if ( m_owner->m_iTeam == CT )
		{
			// we know exactly where the hostages are, and if they are alive
			if ( !info.hostage->IsValid() || info.hostage->IsFollowingSomeone() )
				continue;

			hostagePos = &info.hostage->pev->origin;
		}
		else
		{
			// use our memory of where we think the hostages are
			if ( !info.isValid )
				continue;

			hostagePos = &info.knownPos;
		}

		CNavArea *hostageArea = TheNavAreaGrid.GetNearestNavArea( hostagePos );
		if ( hostageArea == NULL )
			continue;

		ShortestPathCost cost;
		float travelDistance = NavAreaTravelDistance( startArea, hostageArea, cost );
		if ( travelDistance < closeDistance )
		{
			closeDistance = travelDistance;
			close = info.hostage;
			closePos = hostagePos;
		}
	}

	if ( knowPos != NULL && closePos != NULL )
		*knowPos = *closePos;

	return close;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return the location of a "free" hostage, or NULL if we dont know of any
 */
const Vector *CSGameState::GetRandomFreeHostagePosition( void ) const
{
	if ( m_owner == NULL )
		return NULL;

	const Vector *freePos[12];
	int freeCount = 0;

	for ( int i = 0; i < m_hostageCount; ++i )
	{
		const HostageInfo &info = m_hostage[i];

		if ( m_owner->m_iTeam == CT )
		{
			// we know exactly where the hostages are, and if they are alive
			if ( info.hostage->IsValid() && !info.hostage->IsFollowingSomeone() )
				freePos[ freeCount++ ] = &info.hostage->pev->origin;
		}
		else if ( info.isValid )
		{
			// use our memory of where we think the hostages are
			freePos[ freeCount++ ] = &info.knownPos;
		}
	}

	if ( freeCount == 0 )
		return NULL;

	return freePos[ RANDOM_LONG( 0, freeCount - 1 ) ];
}

//--------------------------------------------------------------------------------------------------------------
/**
 * If we can see any of the positions where we think a hostage is, validate it
 * Return status of any changes (a hostage died or was moved)
 */
unsigned int CSGameState::ValidateHostagePositions( void )
{
	// limit how often we validate
	if ( !m_validateInterval.IsElapsed() )
		return 0;

	m_validateInterval.Start( 0.5f );

	// check the status of hostages
	int startValidCount = 0;
	for ( int i = 0; i < m_hostageCount; ++i )
		startValidCount += m_hostage[i].isValid;

	const unsigned int hostageDied = 1;
	const unsigned int hostageMoved = 2;
	const unsigned int allHostagesGone = 4;
	unsigned int changes = 0;

	for ( int i = 0; i < m_hostageCount; ++i )
	{
		HostageInfo &info = m_hostage[i];
		CHostage *hostage = info.hostage;

		// if we can see a hostage, update our knowledge of it
		if ( m_owner->IsVisible( &hostage->pev->origin, true ) )
		{
			if ( !hostage->IsValid() )
			{
				info.isValid = false;
				if ( info.isAlive )
					changes |= hostageDied;
				info.isAlive = false;
			}
			else if ( hostage->IsFollowingSomeone() )
			{
				info.isValid = false;
			}
			else
			{
				info.knownPos = hostage->pev->origin;
				info.isValid = true;
			}
		}
		else if ( info.isValid && m_owner->IsVisible( &info.knownPos, true ) )
		{
			if ( !hostage->IsValid() )
			{
				info.isAlive = false;
				info.isValid = false;
				changes |= hostageDied;
			}
			else if ( hostage->IsFollowingSomeone() || (hostage->pev->origin - info.knownPos).LengthSquared() > 2500.0f )
			{
				info.isValid = false;
				changes |= hostageMoved;
			}
		}
	}

	int endValidCount = 0;
	for ( int i = 0; i < m_hostageCount; ++i )
		endValidCount += m_hostage[i].isValid;

	if ( startValidCount > 0 && endValidCount == 0 )
		changes = (changes & ~(hostageMoved | allHostagesGone)) | allHostagesGone;

	return changes;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return the nearest visible free hostage
 * Since we can actually see any hostage we return, we know its actual position
 */
CHostage *CSGameState::GetNearestVisibleFreeHostage( void ) const
{
	CHostage *close = NULL;
	float closeRangeSq = 1000000000.0f;

	for ( int i = 0; i < m_hostageCount; ++i )
	{
		CHostage *hostage = m_hostage[i].hostage;

		// if the hostage is dead or rescued, its not free
		if ( !hostage->IsValid() )
			continue;

		// if this hostage is following someone, its not free
		if ( hostage->IsFollowingSomeone() )
			continue;

		Vector pos = hostage->pev->origin + Vector( 0.0f, 0.0f, 54.0f );
		float rangeSq = (pos - m_owner->pev->origin).LengthSquared();
		if ( rangeSq < closeRangeSq && m_owner->IsVisible( &pos, false ) )
		{
			closeRangeSq = rangeSq;
			close = hostage;
		}
	}

	return close;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if there are no free hostages
 */
bool CSGameState::AreAllHostagesBeingRescued( void ) const
{
	// if the hostages have all been rescued, they are not being rescued any longer
	if ( m_allHostagesRescued )
		return false;

	bool isAllDead = true;

	for ( int i = 0; i < m_hostageCount; ++i )
	{
		if ( m_owner->m_iTeam == CT )
		{
			// CT's have perfect knowledge via their radar
			if ( !m_hostage[i].hostage->IsValid() )
				continue;

			if ( !m_hostage[i].hostage->IsFollowingSomeone() )
				return false;

			isAllDead = false;
		}
		else
		{
			if ( m_hostage[i].isValid && m_hostage[i].isAlive )
				return false;

			if ( !m_hostage[i].isValid && m_hostage[i].isAlive )
				isAllDead = false;
		}
	}

	// if all of the remaining hostages are dead, they arent being rescued
	if ( isAllDead )
		return false;

	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * All hostages have been rescued or are dead
 */
bool CSGameState::AreAllHostagesGone( void ) const
{
	if ( m_allHostagesRescued )
		return true;

	for ( int i = 0; i < m_hostageCount; ++i )
	{
		if ( m_owner->m_iTeam == CT )
		{
			// CT's have perfect knowledge via their radar
			if ( m_hostage[i].hostage->IsValid() )
				return false;
		}
		else if ( m_hostage[i].isValid && m_hostage[i].isAlive )
		{
			return false;
		}
	}

	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Someone told us all the hostages are gone
 */
void CSGameState::AllHostagesGone( void )
{
	for ( int i = 0; i < m_hostageCount; ++i )
		m_hostage[i].isValid = false;
}
