#include "hostage.h"
#include "cs_bot_manager.h"
#include "gamerules.h"
#include "weapons.h"
#include <math.h>
#include <stdio.h>


inline void DrawAxes( const Vector &origin, int red, int green, int blue )
{
	float size = 10.0f;
	UTIL_DrawBeamPoints( origin - Vector( size, 0, 0 ),
		origin + Vector( size, 0, 0 ),
		2, red, green, blue );

	UTIL_DrawBeamPoints( origin - Vector( 0, size, 0 ),
		origin + Vector( 0, size, 0 ),
		2, red, green, blue );

	UTIL_DrawBeamPoints( origin + Vector( 0, 0, size ),
		origin - Vector( 0, 0, size ),
		2, red, green, blue );
}


CHostageImprov::CHostageImprov( CBaseEntity *entity )
{
	m_hostage = (CHostage *)entity;

	OnReset();
}


bool CHostageImprov::IsAlive( void ) const
{
	return m_hostage->pev->deadflag != DEAD_DEAD;
}


void CHostageImprov::MoveTo( const Vector &goal )
{
	m_moveGoal = goal;
	m_path.Invalidate();

	if ( !m_hasKnownGoodPos )
	{
		m_hasKnownGoodPos = true;
		SetKnownGoodPosition( GetFeet() );
	}
}


bool CHostageImprov::GetSimpleGroundHeightWithFloor( const Vector *pos, float *height, Vector *normal )
{
	if ( GetSimpleGroundHeight( pos, height, normal ) )
	{
		if ( m_lastKnownArea && m_lastKnownArea->IsOverlapping( pos ) )
			*height = std::max( *height, m_lastKnownArea->GetZ( pos ) );

		return true;
	}

	return false;
}


bool CHostageImprov::DiscontinuityJump( float ground, bool onlyJumpDown, bool mustJump )
{
	if ( IsJumping() || IsCrouching() || IsUsingLadder() )
		return false;

	float dz = ground - GetFeet().z;

	if ( ( dz > 18 && !onlyJumpDown ) || dz < -41.8f )
	{
		Jump();
		return true;
	}

	return false;
}


void CHostageImprov::LookAt( const Vector &target )
{
	m_viewGoal = target;
	m_isLookingAt = true;
}


void CHostageImprov::ClearLookAt( void )
{
	m_isLookingAt = false;
}


void CHostageImprov::FaceTo( const Vector &goal )
{
	m_faceGoal = goal;
	m_isFacingTo = true;
}


void CHostageImprov::ClearFaceTo( void )
{
	m_isFacingTo = false;
}


void CHostageImprov::MoveTowards( const Vector &pos, float deltaT )
{
	ClearPath();

	if ( ( m_lastKnownArea == NULL || !( m_lastKnownArea->GetAttributes() & NAV_NO_JUMP ) ) &&
		!IsUsingLadder() &&
		!IsJumping() &&
		IsOnGround() &&
		!IsCrouching() )
	{
		float ground;
		Vector aheadRay( pos.x - GetFeet().x, pos.y - GetFeet().y, 0 );
		aheadRay.NormalizeInPlace();

		bool jumped = false;
		if ( IsRunning() )
		{
			const float farLookAheadRange = 80.0f;
			Vector normal;
			Vector stepAhead = GetFeet() + farLookAheadRange * aheadRay;
			stepAhead.z += 72;

			if ( GetSimpleGroundHeightWithFloor( &stepAhead, &ground, &normal ) )
			{
				if ( normal.z > 0.9f )
					jumped = DiscontinuityJump( ground, true, false );
			}
		}

		if ( !jumped )
		{
			const float lookAheadRange = 30.0f;
			Vector stepAhead = GetFeet() + lookAheadRange * aheadRay;
			stepAhead.z += 72;
			if ( GetSimpleGroundHeightWithFloor( &stepAhead, &ground, NULL ) )
			{
				jumped = DiscontinuityJump( ground, false, false );
			}
		}

		if ( !jumped )
		{
			const float lookAheadRange = 10.0f;
			Vector stepAhead = GetFeet() + lookAheadRange * aheadRay;
			stepAhead.z += 72;
			if ( GetSimpleGroundHeightWithFloor( &stepAhead, &ground, NULL ) )
			{
				DiscontinuityJump( ground, true, true );
			}
		}
	}

	Vector move = pos - GetFeet();
	move.z = 0;

	if ( !move.IsZero() )
		move.NormalizeInPlace();

	float accelRate;
	const float crouchWalkRate = 250.0f;

	switch ( m_moveType )
	{
	case Running:
		accelRate = IsCrouching() ? crouchWalkRate : 1000.0f;
		break;

	case Walking:
		accelRate = IsCrouching() ? crouchWalkRate : 400.0f;
		break;

	case Stopped:
		accelRate = 0;
		break;

	default:
		return;
	}

	m_vel.x += accelRate * deltaT * move.x;
	m_vel.y += accelRate * deltaT * move.y;
}


bool CHostageImprov::FaceTowards( const Vector &target, float deltaT )
{
	Vector2D to = ( target - GetFeet() ).Make2D();
	to.NormalizeInPlace();

	float moveAngle = GetMoveAngle();

	Vector2D dir( BotCOS( moveAngle ), BotSIN( moveAngle ) );
	Vector2D lat( -dir.y, dir.x );

	const float maxTurnRate = 300.0f;

	float error = DotProduct( to, lat );

	float dot = DotProduct( to, dir );
	if ( dot < 0 )
	{
		error = ( error < 0 ) ? -1.0f : 1.0f;
	}
	else
	{
		const float tolerance = 0.05f;
		if ( fabs( error ) < tolerance )
			return true;
	}

	moveAngle += deltaT * maxTurnRate * error;

	BotCOS( moveAngle );
	BotSIN( moveAngle );

	SetMoveAngle( moveAngle );
	m_hostage->pev->angles.y = moveAngle;

	return false;
}


void CHostageImprov::FaceOutwards( void )
{
	TraceResult result;

	float farthestRange = 0;
	int farthest = 0;

	const int cornerCount = 4;
	static Vector corner[ cornerCount ] =
	{
		Vector( -1000, 1000, 0 ),
		Vector( 1000, 1000, 0 ),
		Vector( -1000, -1000, 0 ),
		Vector( 1000, -1000, 0 )
	};

	for ( int i = 0; i < cornerCount; ++i )
	{
		UTIL_TraceLine( GetCentroid(), GetCentroid() + corner[i], ignore_monsters, ignore_glass, m_hostage->edict(), &result );
		float range = ( result.vecEndPos - GetCentroid() ).LengthSquared();

		if ( range > farthestRange )
		{
			farthestRange = range;
			farthest = i;
		}
	}

	FaceTo( GetCentroid() + corner[ farthest ] );
}


bool CHostageImprov::IsAtMoveGoal( float error ) const
{
	return ( GetFeet() - m_moveGoal ).IsLengthLessThan( error );
}


bool CHostageImprov::IsAtFaceGoal( void ) const
{
	return false;
}


class CheckWayFunctor
{
public:
	CheckWayFunctor( const CHostageImprov *me, const Vector &goalPos )
	{
		m_me = me;
		m_goalPos = goalPos;
		m_blocker = NULL;
	}

	bool operator()( CHostage *them )
	{
		if ( !them->IsAlive() )
			return true;

		if ( m_me->IsFriendInTheWay( them, m_goalPos ) )
		{
			m_blocker = them;
			return false;
		}

		return true;
	}

	const CHostageImprov *m_me;
	Vector m_goalPos;
	CHostage *m_blocker;
};


bool CHostageImprov::IsFriendInTheWay( const Vector &goalPos ) const
{
	if ( !m_avoidFriendTimer.IsElapsed() )
		return m_isFriendInTheWay;

	const float avoidFriendInterval = 0.5f;
	m_avoidFriendTimer.Start( avoidFriendInterval );

	m_isFriendInTheWay = false;

	if ( g_pHostages )
	{
		CheckWayFunctor check( this, goalPos );
		g_pHostages->ForEachHostage( check );
		if ( check.m_blocker )
		{
			m_isFriendInTheWay = true;
			return true;
		}
	}

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *player = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( player == NULL )
			continue;

		if ( FNullEnt( player->pev ) )
			continue;

		if ( !player->IsAlive() )
			continue;

		if ( player->m_iTeam == TERRORIST )
			continue;

		if ( IsFriendInTheWay( player, goalPos ) )
		{
			m_isFriendInTheWay = true;
			break;
		}
	}

	return m_isFriendInTheWay;
}


bool CHostageImprov::IsFriendInTheWay( CBaseEntity *myFriend, const Vector &goalPos ) const
{
	if ( m_hostage == myFriend )
		return false;

	Vector moveDir = goalPos - GetFeet();

	float length = moveDir.NormalizeInPlace();

	Vector friendFeet;
	friendFeet.x = myFriend->pev->origin.x;
	friendFeet.y = myFriend->pev->origin.y;

	if ( myFriend->IsPlayer() )
		friendFeet.z = myFriend->pev->absmin.z;
	else
		friendFeet.z = myFriend->pev->origin.z;

	Vector toFriend = friendFeet - GetFeet();

	const float personalSpace = 100.0f;
	if ( toFriend.IsLengthGreaterThan( personalSpace ) )
		return false;

	float friendDistAlong = DotProduct( toFriend, moveDir );

	if ( friendDistAlong <= 0 )
		return false;

	Vector pos;
	if ( friendDistAlong >= length )
		pos = goalPos;
	else
		pos = GetFeet() + friendDistAlong * moveDir;

	const float friendRadius = 30.0f;
	if ( ( pos - friendFeet ).IsLengthLessThan( friendRadius ) )
	{
		if ( myFriend->pev->velocity.IsZero() )
			return true;

		return DotProduct( myFriend->pev->velocity, m_hostage->pev->velocity ) < 0;
	}

	return false;
}


class CheckAhead
{
public:
	CheckAhead( const CHostageImprov *me )
	{
		m_me = me;
		m_dir = Vector( BotCOS( me->GetMoveAngle() ), BotSIN( me->GetMoveAngle() ), 0 );
		m_isBlocked = false;
	}

	bool operator()( CBaseEntity *entity )
	{
		if ( entity != m_me->GetEntity() )
		{
			Vector to = entity->Center() - m_me->GetCentroid();
			float range = to.NormalizeInPlace();

			const float closeRange = 60.0f;
			if ( range > closeRange )
				return true;

			const float aheadTolerance = 0.95f;
			if ( DotProduct( to, m_dir ) < aheadTolerance )
				return true;

			m_isBlocked = true;
		}

		return true;
	}

	bool IsBlocked( void ) const { return m_isBlocked; }

	const CHostageImprov *m_me;
	Vector m_dir;
	bool m_isBlocked;
};


bool CHostageImprov::IsFriendInTheWay( void ) const
{
	CheckAhead check( this );

	g_pHostages->ForEachHostage( check );
	ForEachPlayer( check );

	return check.IsBlocked();
}


float CHostageImprov::GetSpeed( void )
{
	return -1;
}


bool CHostageImprov::Jump( void )
{
	if ( IsCrouching() )
		return false;

	if ( g_pHostages->IsNearbyHostageJumping( this ) )
		return false;

	if ( m_jumpTimer.IsElapsed() )
	{
		m_moveFlags |= IN_JUMP;
		m_hasJumped = false;

		const float epsilon = 1.0f;
		if ( m_hostage->pev->velocity.IsLengthLessThan( epsilon ) )
		{
			const float fudge = 2.0f;
			m_hostage->pev->origin.z += fudge;
		}

		const float minJumpInterval = 3.0f;
		m_jumpTimer.Start( minJumpInterval );

		m_animateState.Reset();
		m_animateState.SetPerformance( HostageAnimateState::Jump );
		m_animateState.AddSequence( this, ACT_HOP, -1, 1 );

		return true;
	}

	return false;
}


void CHostageImprov::Run( void )
{
	m_moveType = m_moveLimit;
}


void CHostageImprov::Walk( void )
{
	m_moveType = std::min( Walking, m_moveLimit );
}


void CHostageImprov::Stop( void )
{
	MoveTo( GetFeet() );

	m_hostage->pev->velocity = Vector( 0, 0, 0 );

	m_moveType = std::min( Stopped, m_moveLimit );
}


const Vector &CHostageImprov::GetFeet( void ) const
{
	return m_hostage->pev->origin;
}


const Vector &CHostageImprov::GetCentroid( void ) const
{
	m_centroid = m_hostage->pev->origin;
	m_centroid.z += 36;

	return m_centroid;
}


const Vector &CHostageImprov::GetEyes( void ) const
{
	m_eye = m_hostage->pev->origin;
	m_eye.z += 72 - 7;

	return m_eye;
}


bool CHostageImprov::IsOnGround( void ) const
{
	return ( m_hostage->pev->flags & FL_ONGROUND ) != 0;
}


bool CHostageImprov::IsMoving( void ) const
{
	const float epsilon = 10.0f;
	return m_actualVel.IsLengthGreaterThan( epsilon );
}


bool CHostageImprov::IsVisible( const Vector &pos, bool testFOV ) const
{
	const Vector eye = GetEyes();

	TraceResult result;
	UTIL_TraceLine( eye, pos, ignore_monsters, ignore_glass, m_hostage->edict(), &result );
	return ( result.flFraction == 1 );
}


bool CHostageImprov::IsPlayerLookingAtMe( CBasePlayer *other, float cosTolerance ) const
{
	Vector2D toOther;
	toOther.x = other->pev->origin.x - GetCentroid().x;
	toOther.y = other->pev->origin.y - GetCentroid().y;
	toOther.NormalizeInPlace();

	UTIL_MakeVectors( other->pev->v_angle + other->pev->punchangle );
	Vector2D otherDir;
	otherDir.x = gpGlobals->v_forward.x;
	otherDir.y = gpGlobals->v_forward.y;
	otherDir.NormalizeInPlace();

	if ( DotProduct( toOther, otherDir ) < -cosTolerance )
	{
		if ( IsVisible( other->EyePosition() ) )
			return true;
	}

	return false;
}


CBasePlayer *CHostageImprov::IsAnyPlayerLookingAtMe( int team, float cosTolerance ) const
{
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *player = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( !IsEntityValid( player ) )
			continue;

		if ( !player->IsAlive() )
			continue;

		if ( team && player->m_iTeam != team )
			continue;

		if ( IsPlayerLookingAtMe( player, cosTolerance ) )
			return player;
	}

	return NULL;
}


CBasePlayer *CHostageImprov::GetClosestPlayerByTravelDistance( int team, float *range ) const
{
	if ( GetLastKnownArea() == NULL )
		return NULL;

	CBasePlayer *close = NULL;
	float closeRange = 1.0e11f;

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *player = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( !IsEntityValid( player ) )
			continue;

		if ( !player->IsAlive() )
			continue;

		if ( team && player->m_iTeam != team )
			continue;

		ShortestPathCost cost;
		Vector vecCenter = player->Center();
#pragma warning( suppress : 4457 )
		float range = NavAreaTravelDistance( &GetFeet(), GetLastKnownArea(), &vecCenter, cost );

		if ( range > 0 && range < closeRange )
		{
			closeRange = range;
			close = player;
		}
	}

	if ( range )
		*range = closeRange;

	return close;
}


void CHostageImprov::OnReset( void )
{
	m_moveFlags = 0;
	m_vel = Vector2D( 0, 0 );
	m_actualVel = Vector( 0, 0, 0 );

	m_lastKnownArea = NULL;

	m_hasKnownGoodPos = false;
	m_hasPriorKnownGoodPos = false;

	m_checkNearbyTerroristTimer.Invalidate();
	m_isTerroristNearby = false;

	m_moveType = Stopped;
	m_moveLimit = Running;
	m_isCrouching = false;

	m_moveAngle = m_hostage->pev->angles.y;
	m_moveGoal = m_hostage->GetHomePosition();

	ClearLookAt();
	ClearFaceTo();

	m_blinkTimer.Invalidate();
	m_blinkCounter = 0;

	m_hostage->SetBoneController( EYEBROW, 0 );
	m_hostage->SetBoneController( SNEER, 0 );
	m_hostage->SetBoneController( MOUTH, 0 );

	m_isDelayedChatterPending = false;
	m_chatterTimer.Invalidate();

	m_path.Invalidate();

	m_visiblePlayerCount = 0;

	m_animateState.Reset();
	m_didFidget = false;

	m_lastSawCT.Start();
	m_lastSawT.Start();

	m_scaredTimer.Invalidate();

	m_jumpTimer.Invalidate();
	m_hasJumped = false;

	m_isFriendInTheWay = false;

	m_aggression = RANDOM_FLOAT( 0, 1 );

	StandUp();

	m_behavior.Reset( this );
	Idle();
}


void CHostageImprov::UpdateVision( void )
{
	if ( !m_visionTimer.IsElapsed() )
		return;

	m_visiblePlayerCount = 0;

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *player = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( player == NULL )
			continue;

		if ( FNullEnt( player->pev ) )
			continue;

		if ( FStrEq( STRING( player->pev->netname ), "" ) )
			continue;

		if ( !player->IsAlive() )
			continue;

		if ( !IsVisible( player->pev->origin, true ) )
			continue;
		m_visiblePlayer[ m_visiblePlayerCount++ ] = player;

		if ( player->m_iTeam == TERRORIST )
			m_lastSawT.Start();
		else
			m_lastSawCT.Start();

		if ( m_visiblePlayerCount == 32 )
			break;
	}

	m_visionTimer.Start( RANDOM_FLOAT( 0.4f, 0.6f ) );
}


class KeepPersonalSpace
{
public:
	KeepPersonalSpace( CHostageImprov *improv )
	{
		m_improv = improv;
		m_velDir = improv->GetActualVelocity();
		m_velDir.z = 0;
		m_speed = m_velDir.NormalizeInPlace();
	}

	bool operator()( CBaseEntity *entity ) const
	{
		if ( entity == m_improv->GetEntity() )
			return true;

		if ( entity->IsPlayer() )
		{
			CBasePlayer *player = (CBasePlayer *)entity;
			if ( !player->IsAlive() )
				return true;
		}

		const float space = 50.0f;

		Vector to = entity->pev->origin - m_improv->GetCentroid();
		float range = to.NormalizeInPlace();

		if ( range < space )
		{
			CBasePlayer *player = (CBasePlayer *)entity;
			const float cosTolerance = 0.8f;
			if ( entity->IsPlayer() && player->m_iTeam == CT && !m_improv->IsFollowing() && m_improv->IsPlayerLookingAtMe( player, cosTolerance ) )
				return true;

			m_improv->ApplyForce( to * -( 50 - range ) );

			const float damper = -0.1f;
			m_improv->ApplyForce( damper * m_speed * m_velDir );
		}

		return true;
	}

	CHostageImprov *m_improv;
	Vector m_velDir;
	float m_speed;
};


void CHostageImprov::TrackPath( const Vector &pathGoal, float deltaT )
{
	FaceTowards( pathGoal, deltaT );
	MoveTowards( pathGoal, deltaT );

	m_jumpTarget = pathGoal;

	if ( cv_hostage_debug.value > 0 )
		DrawAxes( pathGoal, 255, 0, 255 );
}


void CHostageImprov::SetKnownGoodPosition( const Vector &pos )
{
	if ( IsJumping() || IsCrouching() )
		return;

	if ( m_hasKnownGoodPos && m_priorKnownGoodPosTimer.IsElapsed() )
	{
		m_priorKnownGoodPosTimer.Start( 1 );

		m_hasPriorKnownGoodPos = true;
		m_priorKnownGoodPos = m_knownGoodPos;
	}

	m_knownGoodPos = pos;
	m_hasKnownGoodPos = true;
}


void CHostageImprov::ResetToKnownGoodPosition( void )
{
	if ( m_hasKnownGoodPos )
	{
		const float tolerance = 10.0f;
		if ( ( m_hostage->pev->origin - m_knownGoodPos ).IsLengthLessThan( tolerance ) )
		{
			if ( m_hasPriorKnownGoodPos )
			{
				m_knownGoodPos = m_priorKnownGoodPos;
				m_hasPriorKnownGoodPos = false;

				m_priorKnownGoodPosTimer.Start( 5 );
			}
			else
			{
				m_hasKnownGoodPos = false;
				m_knownGoodPos = m_hostage->GetHomePosition();
			}
		}
	}
	else
	{
		m_knownGoodPos = m_hostage->GetHomePosition();
	}

	m_hostage->pev->origin = m_knownGoodPos;
	m_hostage->pev->velocity = Vector( 0, 0, 0 );
	m_path.Invalidate();
	Stop();
}


void CHostageImprov::StartLadder( const CNavLadder *ladder, NavTraverseType how, const Vector *approachPos, const Vector *departPos )
{
	m_traversingLadder = true;
}


bool CHostageImprov::TraverseLadder( const CNavLadder *ladder, NavTraverseType how,
	const Vector *approachPos, const Vector *departPos,
	float deltaT )
{
	Vector goal;

	if ( how == GO_LADDER_DOWN )
	{
		goal = ladder->m_bottom;
		AddDirectionVector( &goal, ladder->m_dir, 16 );

		if ( GetFeet().z < ladder->m_top.z - 36 )
		{
			const float ladderRange = 50.0f;
			if ( ( GetFeet() - goal ).Make2D().IsLengthLessThan( ladderRange ) )
			{
				m_hostage->pev->velocity.z = -100;
			}
		}

		if ( GetFeet().z < ladder->m_bottom.z + 36 )
			return true;
	}
	else
	{
		goal = ladder->m_top;
		AddDirectionVector( &goal, ladder->m_dir, 16 );

		if ( m_traversingLadder )
		{
			const float walkRange = 100.0f;
			if ( ( GetFeet() - goal ).Make2D().IsLengthLessThan( walkRange ) )
				Walk();

			const float ladderRange = 40.0f;
			if ( ( GetFeet() - goal ).Make2D().IsLengthLessThan( ladderRange ) )
			{
				m_hostage->pev->velocity.z = 150;
			}

			if ( GetFeet().z > ladder->m_top.z )
			{
				m_traversingLadder = false;
			}
		}
		else
		{
			if ( departPos )
			{
				float closeRange = 999999.9f, range;

				if ( ladder->m_topForwardArea )
				{
					range = ( *departPos - *ladder->m_topForwardArea->GetCenter() ).LengthSquared();
					if ( range < closeRange )
					{
						closeRange = range;
						goal = *ladder->m_topForwardArea->GetCenter();
						if ( ladder->m_topForwardArea->GetAttributes() & NAV_CROUCH )
							Crouch();
					}
				}

				if ( ladder->m_topLeftArea )
				{
					range = ( *departPos - *ladder->m_topLeftArea->GetCenter() ).LengthSquared();
					if ( range < closeRange )
					{
						closeRange = range;
						goal = *ladder->m_topLeftArea->GetCenter();
						if ( ladder->m_topLeftArea->GetAttributes() & NAV_CROUCH )
							Crouch();
					}
				}

				if ( ladder->m_topRightArea )
				{
					range = ( *departPos - *ladder->m_topRightArea->GetCenter() ).LengthSquared();
					if ( range < closeRange )
					{
						closeRange = range;
						goal = *ladder->m_topRightArea->GetCenter();
						if ( ladder->m_topRightArea->GetAttributes() & NAV_CROUCH )
							Crouch();
					}
				}
			}

			const float atGoalRange = 20.0f;
			if ( ( GetFeet() - goal ).Make2D().IsLengthLessThan( atGoalRange ) )
				return true;
		}
	}

	TrackPath( goal, deltaT );

	return false;
}


void CHostageImprov::UpdatePosition( float deltaT )
{
	CNavArea *area = TheNavAreaGrid.GetNavArea( &m_hostage->pev->origin );
	if ( area )
		m_lastKnownArea = area;

	if ( cv_hostage_debug.value > 0 )
	{
		DrawAxes( m_moveGoal, 255, 255, 0 );
	}

	if ( IsJumping() )
	{
		if ( !m_hasJumped )
		{
			m_hasJumped = true;
			m_hostage->pev->velocity.z += 300;

			m_hasJumpedIntoAir = false;
		}
		else
		{
			if ( m_hasJumpedIntoAir )
			{
				if ( IsOnGround() )
				{
					ResetJump();
				}
			}
			else if ( !IsOnGround() )
			{
				m_hasJumpedIntoAir = true;
			}
		}

		const float pushSpeed = 100.0f;

		Vector dir = m_jumpTarget - GetFeet();
		dir.z = 0;
		dir.NormalizeInPlace();

		m_hostage->pev->velocity.x = dir.x * pushSpeed;
		m_hostage->pev->velocity.y = dir.y * pushSpeed;

		m_hostage->SetBoneController( HEAD_YAW, 0 );
		m_hostage->SetBoneController( HEAD_PITCH, 0 );
		FaceTowards( m_jumpTarget, deltaT );

		return;
	}

	if ( m_isLookingAt )
	{
		Vector angles = UTIL_VecToAngles( m_viewGoal - GetEyes() );

		float yaw = angles.y - m_hostage->pev->angles.y;
		while ( yaw > 180 ) yaw -= 360;
		while ( yaw < -180 ) yaw += 360;

		float pitch = angles.x - m_hostage->pev->angles.x;
		while ( pitch > 180 ) pitch -= 360;
		while ( pitch < -180 ) pitch += 360;

		m_hostage->SetBoneController( HEAD_YAW, yaw );
		m_hostage->SetBoneController( HEAD_PITCH, -pitch );

		if ( IsAtMoveGoal() && !HasFaceTo() )
		{
			if ( yaw < -45 || yaw > 45 )
				FaceTowards( m_viewGoal, deltaT );
		}
	}
	else
	{
		m_hostage->SetBoneController( HEAD_YAW, 0 );
		m_hostage->SetBoneController( HEAD_PITCH, 0 );
	}

	if ( HasFaceTo() )
	{
		if ( FaceTowards( m_faceGoal, deltaT ) )
			ClearFaceTo();
	}

	if ( !IsAtMoveGoal() || m_path.IsValid() )
	{
		if ( !m_path.IsValid() )
		{
			HostagePathCost pathCost;
			if ( m_path.Compute( &GetFeet(), &m_moveGoal, pathCost ) )
			{
				m_follower.SetImprov( this );
				m_follower.SetPath( &m_path );
				m_follower.Reset();
				m_follower.Debug( cv_hostage_debug.value > 0 );
			}
		}

		m_follower.Update( deltaT, m_inhibitObstacleAvoidance.IsElapsed() );

		if ( m_moveType == Stopped )
			m_follower.ResetStuck();

		if ( m_follower.IsStuck() )
			Wiggle();
	}

	const float friction = 3.0f;
	m_vel.x += m_vel.x * -friction * deltaT;
	m_vel.y += m_vel.y * -friction * deltaT;

	float speed = m_vel.NormalizeInPlace();

	const float maxSpeed = 285.0f;
	if ( speed > maxSpeed )
		speed = maxSpeed;

	m_vel = m_vel * speed;

	KeepPersonalSpace spacer( this );
	ForEachPlayer( spacer );

	if ( g_pHostages )
		g_pHostages->ForEachHostage( spacer );

	m_hostage->pev->velocity.x = m_vel.x;
	m_hostage->pev->velocity.y = m_vel.y;

	m_moveFlags = 0;
}


void CHostageImprov::OnUpkeep( float deltaT )
{
	if ( !IsAlive() )
		return;

	UpdatePosition( deltaT );
}


bool CHostageImprov::IsTerroristNearby( void )
{
	if ( !m_nearbyTerroristTimer.IsElapsed() )
		return true;

	CheckForNearbyTerrorists();

	return m_isTerroristNearby;
}


void CHostageImprov::CheckForNearbyTerrorists( void )
{
	if ( !m_checkNearbyTerroristTimer.IsElapsed() )
		return;

	const float checkInterval = 2.0f;
	m_checkNearbyTerroristTimer.Start( checkInterval );

	m_isTerroristNearby = false;

	float range;
	if ( GetClosestPlayerByTravelDistance( TERRORIST, &range ) )
	{
		const float nearbyTerroristRange = 2000.0f;
		if ( range < nearbyTerroristRange )
		{
			m_isTerroristNearby = true;
			m_nearbyTerroristTimer.Start( 10 );
		}
	}
}


void CHostageImprov::UpdateGrenadeReactions( void )
{
	if ( m_coughTimer.IsElapsed() )
	{
		if ( TheBots->IsInsideSmokeCloud( &GetCentroid() ) )
		{
			m_coughTimer.Start( RANDOM_FLOAT( 1, 3 ) );
			Chatter( HOSTAGE_CHATTER_COUGH, true );
			Frighten( SCARED );
		}
	}

	if ( m_grenadeTimer.IsElapsed() )
	{
		m_grenadeTimer.Start( RANDOM_FLOAT( 0.4f, 0.6f ) );

		CBaseEntity *entity = NULL;
		const float watchGrenadeRadius = 500.0f;

		while ( ( entity = UTIL_FindEntityInSphere( entity, GetCentroid(), watchGrenadeRadius ) ) != NULL )
		{
			if ( FClassnameIs( entity->pev, "grenade" ) )
			{
				CGrenade *grenade = (CGrenade *)entity;

				if ( grenade->m_iBounceCount <= 1 )
				{
					if ( IsVisible( entity->Center() ) )
					{
						Chatter( HOSTAGE_CHATTER_SAW_HE_GRENADE, true );

						if ( entity->pev->dmg > 50 )
						{
							m_idleState.m_fleeTimer.Invalidate();
							m_idleState.m_mustFlee = true;
							Frighten( TERRIFIED );
						}
						else
						{
							Frighten( SCARED );
						}

						m_grenadeTimer.Start( 10 );
						break;
					}
				}
			}
		}
	}
}


void CHostageImprov::OnUpdate( float deltaT )
{
	if ( !IsAlive() || cv_hostage_stop.value > 0 )
		return;

	if ( m_blinkTimer.IsElapsed() )
	{
		if ( IsScared() )
		{
			m_blinkTimer.Start( RANDOM_FLOAT( 0.5f, 2 ) );
			m_blinkCounter = (char)RANDOM_LONG( 1, 2 );
		}
		else
		{
			m_blinkTimer.Start( RANDOM_FLOAT( 3, 10 ) );
			m_blinkCounter = (char)RANDOM_LONG( 2, 4 );
		}
	}

	if ( m_blinkCounter )
	{
		m_hostage->pev->body = 1;

		--m_blinkCounter;
	}
	else
	{
		m_hostage->pev->body = 0;
	}

	UpdateGrenadeReactions();

	UpdateDelayedChatter();

	UpdateVision();

	m_behavior.Update();

	const float runSpeed = 17.0f;
	const float walkSpeed = 3.0f;

	m_actualVel.x = m_hostage->pev->origin.x - m_lastPosition.x;
	m_actualVel.y = m_hostage->pev->origin.y - m_lastPosition.y;

	const float fallVelocity = -1000.0f;

	if ( !IsOnGround() )
	{
		if ( m_hostage->pev->velocity.z < fallVelocity )
		{
			if ( m_animateState.GetPerformance() != HostageAnimateState::Fall )
			{
				m_animateState.Reset();
				m_animateState.SetPerformance( HostageAnimateState::Fall );
				m_animateState.AddSequence( this, ACT_FALL, 99.9f, 1 );
			}
		}
	}
	else
	{
		if ( IsCrouching() )
		{
			if ( m_actualVel.IsLengthGreaterThan( walkSpeed ) )
			{
				if ( m_animateState.GetPerformance() != HostageAnimateState::CrouchWalk )
				{
					m_animateState.Reset();
					m_animateState.SetPerformance( HostageAnimateState::CrouchWalk );

					ClearLookAt();

					if ( IsScared() )
						m_animateState.AddSequence( this, ACT_CROUCH_WALK_SCARED, 99.9f, 2 );
					else
						m_animateState.AddSequence( this, ACT_CROUCH_WALK, 99.9f, 2 );
				}
			}
			else
			{
				if ( m_animateState.GetPerformance() != HostageAnimateState::Crouch )
				{
					m_animateState.Reset();
					m_animateState.SetPerformance( HostageAnimateState::Crouch );

					if ( IsScared() )
					{
						m_animateState.AddSequence( this, ACT_CROUCH_IDLE_SCARED, -1, 1 );
					}
					else
					{
						m_animateState.AddSequence( this, ACT_CROUCH_IDLE, 99.9f, 1 );
					}
				}
			}
		}
		else
		{
			UTIL_MakeVectors( m_hostage->pev->angles );
			float dot = gpGlobals->v_forward.x * m_actualVel.x + gpGlobals->v_forward.y * m_actualVel.y;
			if ( dot < -3 )
			{
				if ( m_animateState.GetPerformance() != HostageAnimateState::Walk )
				{
					m_animateState.Reset();
					m_animateState.SetPerformance( HostageAnimateState::Walk );

					ClearLookAt();

					float speed = m_actualVel.IsLengthGreaterThan( runSpeed ) ? 2.0f : 1.0f;
					m_animateState.AddSequence( this, ACT_WALK_BACK, 99.9f, speed );
				}
			}
			else if ( m_actualVel.IsLengthGreaterThan( runSpeed ) )
			{
				if ( m_animateState.GetPerformance() != HostageAnimateState::Run )
				{
					m_animateState.Reset();
					m_animateState.SetPerformance( HostageAnimateState::Run );

					ClearLookAt();

					if ( IsScared() || IsEscaping() )
						m_animateState.AddSequence( this, ACT_RUN_SCARED, 99.9f, 2 );
					else
						m_animateState.AddSequence( this, ACT_RUN, 99.9f, 2 );
				}
			}
			else if ( m_actualVel.IsLengthGreaterThan( walkSpeed ) )
			{
				if ( m_animateState.GetPerformance() != HostageAnimateState::Walk )
				{
					m_animateState.Reset();
					m_animateState.SetPerformance( HostageAnimateState::Walk );

					ClearLookAt();

					if ( IsEscaping() )
						m_animateState.AddSequence( this, ACT_WALK_SNEAKY, 99.9f, 1.5f );
					else if ( IsScared() )
						m_animateState.AddSequence( this, ACT_WALK_SCARED, 99.9f, 1.5f );
					else
						m_animateState.AddSequence( this, ACT_WALK, 99.9f, 1.5f );
				}
			}
			else
			{
				if ( m_animateState.GetPerformance() == HostageAnimateState::Walk || m_animateState.GetPerformance() == HostageAnimateState::Run )
				{
					m_animateState.Reset();
				}

				UpdateStationaryAnimation();
			}
		}
	}

	const float safeTime = 0.4f;
	if ( m_collisionTimer.GetElapsedTime() > safeTime )
		SetKnownGoodPosition( m_lastPosition );

	m_lastPosition = m_hostage->pev->origin;

	m_animateState.OnUpdate( this );
}


void CHostageImprov::OnGameEvent( GameEventType event, CBaseEntity *entity, CBaseEntity *other )
{
	switch ( event )
	{
	case EVENT_BULLET_IMPACT:
	{
		Vector *impactPos = (Vector *)other;

		const float nearRange = 100.0f;
		if ( ( GetCentroid() - *impactPos ).IsLengthLessThan( nearRange ) )
		{
			Frighten( TERRIFIED );
		}
		break;
	}

	case EVENT_PLAYER_DIED:
	case EVENT_HOSTAGE_KILLED:
		if ( IsVisible( entity->pev->origin, true ) )
		{
			Frighten( TERRIFIED );

			if ( !entity->IsPlayer() ||
				( entity->IsPlayer() && ( (CBasePlayer *)entity )->m_iTeam != TERRORIST ) )
			{
				DelayedChatter( RANDOM_FLOAT( 0.5f, 0.7f ), HOSTAGE_CHATTER_SCARED_OF_MURDER, true );
			}

			if ( !entity->IsPlayer() )
			{
				m_idleState.m_fleeTimer.Invalidate();
				m_idleState.m_mustFlee = true;
			}
		}
		break;

	case EVENT_HOSTAGE_RESCUED:
		if ( other == m_hostage )
		{
			if ( !entity )
				return;
			Chatter( HOSTAGE_CHATTER_RESCUED, false );
		}
		break;

	case EVENT_CTS_WIN:
		m_scaredTimer.Invalidate();

		m_isDelayedChatterPending = false;

		DelayedChatter( RANDOM_FLOAT( 1, 4 ), HOSTAGE_CHATTER_CTS_WIN, false );
		return;

	case EVENT_TERRORISTS_WIN:
		Frighten( SCARED );

		m_isDelayedChatterPending = false;

		DelayedChatter( RANDOM_FLOAT( 1, 4 ), HOSTAGE_CHATTER_TERRORISTS_WIN, false );
		return;
	}

	if ( entity )
	{
		float range;
		PriorityType priority;
		bool isHostile;
		if ( IsGameEventAudible( event, entity, other, &range, &priority, &isHostile ) )
		{
			const float fudge = 0.4f;
			if ( ( m_hostage->pev->origin - entity->pev->origin ).IsLengthLessThan( fudge * range ) )
			{
				m_lastNoiseTimer.Start();

				if ( isHostile )
				{
					Frighten( SCARED );

					switch ( event )
					{
					case EVENT_WEAPON_FIRED:
						DelayedChatter( RANDOM_FLOAT( 0.3f, 1 ), HOSTAGE_CHATTER_SCARED_OF_GUNFIRE, false );
						break;

					case EVENT_HE_GRENADE_EXPLODED:
					case EVENT_SMOKE_GRENADE_EXPLODED:
					case EVENT_BREAK_GLASS:
					case EVENT_BREAK_WOOD:
					case EVENT_BREAK_METAL:
					case EVENT_BREAK_FLESH:
					case EVENT_BREAK_CONCRETE:
						DelayedChatter( RANDOM_FLOAT( 0.3f, 1 ), HOSTAGE_CHATTER_LOOK_OUT, false );
						break;
					}
				}
			}
		}
	}

	if ( event == EVENT_FLASHBANG_GRENADE_EXPLODED )
	{
		Vector *impactPos = (Vector *)other;

		const float flashRange = 1000.0f;
		if ( ( GetEyes() - *impactPos ).IsLengthLessThan( flashRange ) )
		{
			if ( IsVisible( *impactPos ) )
			{
				DelayedChatter( RANDOM_FLOAT( 0, 1 ), HOSTAGE_CHATTER_BLINDED, true );
				Frighten( TERRIFIED );
			}
		}
	}
}


void CHostageImprov::OnTouch( CBaseEntity *other )
{
	const char *classname = STRING( other->pev->classname );

	if ( cv_hostage_debug.value != 0 )
		CONSOLE_ECHO( "%5.1f: Hostage hit '%s'\n", gpGlobals->time, classname );

	m_collisionTimer.Start();

	if ( FStrEq( classname, "worldspawn" ) )
	{
		const float lookAheadRange = 30.0f;

		UTIL_MakeVectors( m_hostage->pev->angles );

		float ground;
		Vector normal = Vector( 0, 0, 1 );

		if ( !GetSimpleGroundHeightWithFloor( &GetEyes(), &ground, &normal ) )
			return;

		if ( cv_hostage_debug.value < 0 )
			UTIL_DrawBeamPoints( GetFeet() + 50 * normal, GetFeet(), 2, 255, 255, 0 );

		Vector alongFloor = CrossProduct( normal, gpGlobals->v_right );

		TraceResult result;
		bool isStep = false;
		for ( float offset = 1.0f; offset < 18; offset += 3.0f )
		{
			Vector pos = GetFeet();
			pos.z += offset;

			UTIL_TraceLine( pos, pos + lookAheadRange * alongFloor, ignore_monsters, dont_ignore_glass, m_hostage->edict(), &result );

			if ( result.flFraction < 1 && result.vecPlaneNormal.z < 0.7f )
			{
				isStep = true;
				break;
			}
		}

		if ( isStep )
		{
			m_inhibitObstacleAvoidance.Start( 0.5f );

			float stepAheadGround;
			Vector stepAheadNormal;

			for ( float range = 1.0f; range < 30; range += 5.0f )
			{
				Vector stepAhead = GetFeet() + range * alongFloor;
				stepAhead.z = GetEyes().z;

				if ( GetSimpleGroundHeightWithFloor( &stepAhead, &stepAheadGround, &stepAheadNormal ) )
				{
					float dz = stepAheadGround - GetFeet().z;

					if ( dz > 0 && dz < 18 )
					{
						m_hostage->pev->origin.z = stepAheadGround + 3;
						break;
					}
				}
			}
		}

		if ( !isStep && !IsMoving() && !IsUsingLadder() )
		{
			bool isSeam = false;
			const float checkSeamRange = 50.0f;

			Vector posBehind = GetEyes() - checkSeamRange * alongFloor;
			UTIL_TraceLine( posBehind, posBehind - Vector( 0, 0, 9999 ), ignore_monsters, dont_ignore_glass, m_hostage->edict(), &result );

			if ( result.flFraction < 1 && DotProduct( normal, result.vecPlaneNormal ) < 1 )
			{
				isSeam = true;
			}
			else
			{
				Vector posAhead = GetEyes() + checkSeamRange * alongFloor;
				UTIL_TraceLine( posAhead, posAhead - Vector( 0, 0, 9999 ), ignore_monsters, dont_ignore_glass, m_hostage->edict(), &result );

				if ( result.flFraction < 1 && DotProduct( normal, result.vecPlaneNormal ) < 1 )
					isSeam = true;
			}

			if ( isSeam )
			{
				if ( cv_hostage_debug.value != 0 )
					CONSOLE_ECHO( "Hostage stuck on seam.\n" );

				const float nudge = 3.0f;
				m_hostage->pev->origin.z += nudge;
			}
		}
	}

	if ( FStrEq( classname, "func_breakable" ) )
	{
		other->TakeDamage( m_hostage->pev, m_hostage->pev, 9999.9f, DMG_BULLET );
		return;
	}

	if ( !other->IsPlayer() && !FClassnameIs( other->pev, "hostage_entity" ) )
		return;

	Vector2D to = ( m_hostage->pev->origin - other->pev->origin ).Make2D();
	to.NormalizeInPlace();

	const float pushForce = 20.0f;
	m_vel.x += to.x * pushForce;
	m_vel.y += to.y * pushForce;
}


void CHostageImprov::ApplyForce( Vector force )
{
	m_vel.x += force.x;
	m_vel.y += force.y;
}


bool CHostageImprov::IsAtHome( void ) const
{
	return m_hostage->IsAtHome();
}


bool CHostageImprov::CanSeeRescueZone( void ) const
{
	CCSBotManager *ctrl = TheCSBots();
	if ( ctrl == NULL )
		return false;

	const CCSBotManager::Zone *zone = ctrl->GetClosestZone( &GetCentroid() );
	if ( zone )
	{
		if ( IsVisible( zone->m_center ) )
		{
			return true;
		}
	}

	return false;
}


CBasePlayer *CHostageImprov::GetClosestVisiblePlayer( int team )
{
	CBasePlayer *close = NULL;
	float closeRangeSq = 100000000;

	for ( int i = 0; i < m_visiblePlayerCount; ++i )
	{
		CBasePlayer *player = (CBasePlayer *)(CBaseEntity *)m_visiblePlayer[i];

		if ( player == NULL )
			continue;

		if ( team > 0 && player->m_iTeam != team )
			continue;

		float rangeSq = ( GetCentroid() - player->pev->origin ).LengthSquared();
		if ( rangeSq < closeRangeSq )
		{
			closeRangeSq = rangeSq;
			close = player;
		}
	}

	return close;
}


float CHostageImprov::GetTimeSinceLastSawPlayer( int team )
{
	if ( team == TERRORIST )
		return m_lastSawT.GetElapsedTime();

	if ( team == CT )
		return m_lastSawCT.GetElapsedTime();

	float timeCT = m_lastSawCT.GetElapsedTime();
	float timeT = m_lastSawT.GetElapsedTime();

	return std::min( timeCT, timeT );
}


float CHostageImprov::GetTimeSinceLastInjury( void )
{
	return m_lastInjuryTimer.GetElapsedTime();
}


float CHostageImprov::GetTimeSinceLastNoise( void )
{
	return m_lastNoiseTimer.GetElapsedTime();
}


bool CHostageImprov::IsScared( void ) const
{
	if ( !m_scaredTimer.IsElapsed() )
		return true;

	return ( m_animateState.GetPerformance() == HostageAnimateState::Afraid );
}


void CHostageImprov::Frighten( ScareType scare )
{
	if ( !IsScared() )
	{
		m_animateState.Reset();

		m_blinkTimer.Invalidate();
	}

	m_scareIntensity = scare;

	switch ( scare )
	{
	case NERVOUS: m_scaredTimer.Start( RANDOM_FLOAT( 2, 4 ) ); break;
	case SCARED: m_scaredTimer.Start( RANDOM_FLOAT( 3, 8 ) ); break;
	case TERRIFIED:
	{
		m_scaredTimer.Start( RANDOM_FLOAT( 5, 10 ) );

		const float ignoreTime = 10.0f;
		m_ignoreTerroristTimer.Start( ignoreTime );
		break;
	}
	}
}


void CHostageImprov::Afraid( void )
{
	if ( IsCrouching() )
		return;

	if ( m_animateState.GetPerformance() == HostageAnimateState::Flinching ||
		m_animateState.GetPerformance() == HostageAnimateState::Afraid )
		return;

	if ( !IsMoving() )
	{
		m_animateState.Reset();
		m_animateState.SetPerformance( HostageAnimateState::Afraid );

		int which = RANDOM_LONG( 0, 100 ) % 3 + 1;
		char animInto[32], animLoop[32], animExit[32];
		sprintf( animInto, "cower_into_%d", which );
		sprintf( animLoop, "cower_loop_%d", which );
		sprintf( animExit, "cower_exit_%d", which );

		m_animateState.AddSequence( this, animInto, -1, 1 );
		m_animateState.AddSequence( this, animLoop, RANDOM_FLOAT( 3, 10 ), 1 );
		m_animateState.AddSequence( this, animExit, -1, 1 );
	}
}


void CHostageImprov::UpdateIdleActivity( Activity activity, Activity fidget )
{
	if ( m_animateState.m_sequenceCount > 0 )
		return;

	m_animateState.Reset();

	if ( m_didFidget || fidget == ACT_RESET )
	{
		m_animateState.AddSequence( this, activity, RANDOM_FLOAT( 1, 10 ), RANDOM_FLOAT( 0.9f, 1.1f ) );
		m_didFidget = false;
	}
	else
	{
		m_animateState.AddSequence( this, fidget, -1, RANDOM_FLOAT( 0.9f, 1.5f ) );
		m_didFidget = true;
	}
}


void CHostageImprov::Chatter( HostageChatterType sayType, bool mustSpeak )
{
	if ( !IsAlive() && sayType != HOSTAGE_CHATTER_DEATH_CRY )
		return;

	if ( g_pHostages->IsNearbyHostageTalking( this ) && !mustSpeak )
		return;

	if ( !m_chatterTimer.IsElapsed() && !mustSpeak )
		return;
	m_chatterTimer.Start( RANDOM_FLOAT( 5, 15 ) );

	float duration = g_pHostages->GetChatter()->PlaySound( m_hostage, sayType );

	m_talkingTimer.Start( duration );
}


void CHostageImprov::DelayedChatter( float delayTime, HostageChatterType sayType, bool mustSpeak )
{
	if ( !IsAlive() )
		return;

	if ( m_isDelayedChatterPending && !mustSpeak )
		return;

	m_isDelayedChatterPending = true;
	m_delayedChatterTimer.Start( delayTime );
	m_delayedChatterType = sayType;
	m_delayedChatterMustSpeak = mustSpeak;
}


void CHostageImprov::UpdateDelayedChatter( void )
{
	if ( m_isDelayedChatterPending && m_delayedChatterTimer.IsElapsed() )
	{
		m_isDelayedChatterPending = false;

		switch ( m_delayedChatterType )
		{
		case HOSTAGE_CHATTER_SCARED_OF_GUNFIRE:
		{
			if ( RANDOM_FLOAT( 0, 100 ) > 25 )
				return;

			break;
		}

		case HOSTAGE_CHATTER_LOOK_OUT:
		case HOSTAGE_CHATTER_PLEASE_RESCUE_ME:
		case HOSTAGE_CHATTER_IMPATIENT_FOR_RESCUE:
		{
			if ( RANDOM_FLOAT( 0, 100 ) > 60 )
				return;

			break;
		}
		}

		Chatter( m_delayedChatterType, m_delayedChatterMustSpeak );
	}
}


void CHostageImprov::Flinch( Activity activity )
{
	Chatter( HOSTAGE_CHATTER_PAIN, true );

	if ( IsCrouching() )
		return;

	if ( IsMoving() )
		return;

	m_animateState.Reset();
	m_animateState.SetPerformance( HostageAnimateState::Flinching );
	m_animateState.AddSequence( this, activity, -1, 1 );
}


void CHostageImprov::CrouchDie( void )
{
	m_animateState.Reset();
	m_animateState.AddSequence( this, ACT_CROUCH_DIE, -1, 1 );
}


void CHostageImprov::Agree( void )
{
	m_animateState.Reset();
	m_animateState.AddSequence( this, ACT_YES, -1, 1 );
}


void CHostageImprov::Disagree( void )
{
	if ( m_animateState.GetPerformance() == HostageAnimateState::Disagreeing )
		return;

	m_animateState.Reset();
	m_animateState.SetPerformance( HostageAnimateState::Disagreeing );
	m_animateState.AddSequence( this, ACT_NO, -1, RANDOM_FLOAT( 1.5f, 3 ) );
}


void CHostageImprov::Wave( void )
{
	m_animateState.Reset();
	m_animateState.SetPerformance( HostageAnimateState::Waving );
	m_animateState.AddSequence( this, ACT_WAVE, -1, 1 );
}


void CHostageImprov::OnMoveToFailure( const Vector &goal, MoveToFailureType reason )
{
	m_behavior.OnMoveToFailure( goal, reason );

	if ( reason == FAIL_STUCK )
	{
		ResetToKnownGoodPosition();
	}
}


void CHostageImprov::Wiggle( void )
{
	if ( m_wiggleTimer.IsElapsed() )
	{
		m_wiggleDirection = (NavRelativeDirType)RANDOM_LONG( 0, 3 );
		m_wiggleTimer.Start( RANDOM_FLOAT( 0.3f, 0.5f ) );
	}

	Vector dir( BotCOS( m_moveAngle ), BotSIN( m_moveAngle ), 0 );
	Vector lat( -dir.y, dir.x, 0 );

	const float force = 15.0f;

	switch ( m_wiggleDirection )
	{
	case LEFT:
		ApplyForce( force * lat );
		break;

	case RIGHT:
		ApplyForce( -force * lat );
		break;

	case FORWARD:
		ApplyForce( force * dir );
		break;

	case BACKWARD:
		ApplyForce( -force * dir );
		break;
	}

	const float minStuckJumpTime = 0.5f;
	if ( m_follower.IsStuck() && m_follower.GetStuckDuration() > minStuckJumpTime )
	{
		if ( m_wiggleJumpTimer.IsElapsed() )
		{
			if ( Jump() )
				m_wiggleJumpTimer.Start( RANDOM_FLOAT( 0.75f, 1.2f ) );
		}
	}
}


void CHostageImprov::ClearPath( void )
{
	if ( !m_clearPathTimer.IsElapsed() )
		return;

	m_clearPathTimer.Start( RANDOM_FLOAT( 0.3f, 0.5f ) );

	const Vector eye = GetEyes();
	UTIL_MakeVectors( m_hostage->pev->angles );

	Vector start = eye;
	Vector end = start + gpGlobals->v_forward * 64;

	TraceResult result;
	UTIL_TraceLine( start, end, ignore_monsters, dont_ignore_glass, m_hostage->edict(), &result );
	if ( result.flFraction != 1 )
	{
		entvars_t *entity = VARS( result.pHit );
		if ( entity == NULL )
			return;

		if ( FClassnameIs( entity, "func_door" ) || FClassnameIs( entity, "func_door_rotating" ) )
		{
			CBaseEntity *pObject = CBaseEntity::Instance( entity );
			if ( pObject )
			{
				pObject->Touch( m_hostage );
				return;
			}
		}

		if ( FClassnameIs( entity, "func_breakable" ) && entity->takedamage == DAMAGE_YES )
		{
			CBaseEntity *pObject = CBaseEntity::Instance( entity );
			if ( pObject )
			{
				pObject->TakeDamage( m_hostage->pev, m_hostage->pev, 9999.9f, DMG_BULLET );
			}
		}
	}
}


void CHostageImprov::Crouch( void )
{
	if ( !IsCrouching() )
	{
		m_isCrouching = true;

		const float minCrouchTime = 1.0f;
		m_minCrouchTimer.Start( minCrouchTime );

		UTIL_SetSize( m_hostage->pev, Vector( -10, -10, 0 ), Vector( 10, 10, 30 ) );
	}
}


void CHostageImprov::StandUp( void )
{
	if ( !IsCrouching() )
		return;

	if ( m_minCrouchTimer.IsElapsed() )
	{
		TraceResult result;

		for ( int y = -24; y != 36; y += 12 )
		{
			for ( int x = -24; x != 36; x += 12 )
			{
				UTIL_TraceLine( GetFeet() + Vector( 0, 0, 3 ),
					GetFeet() + Vector( (float)x, (float)y, 72 ),
					ignore_monsters, ignore_glass, m_hostage->edict(), &result );
				if ( result.flFraction < 1 )
					return;
			}
		}

		m_animateState.Reset();
		m_animateState.SetPerformance( HostageAnimateState::None );

		UTIL_SetSize( m_hostage->pev, Vector( -10, -10, 0 ), Vector( 10, 10, 62 ) );

		m_isCrouching = false;
	}
}


void CHostageImprov::UpdateStationaryAnimation( void )
{
	m_behavior.UpdateStationaryAnimation( this );
}
