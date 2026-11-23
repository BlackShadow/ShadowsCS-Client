#ifndef HOSTAGE_IMPROV_H
#define HOSTAGE_IMPROV_H

#include "hostage_states.h"

class CHostage;

enum HostageChatterType
{
	HOSTAGE_CHATTER_START_FOLLOW,
	HOSTAGE_CHATTER_STOP_FOLLOW,
	HOSTAGE_CHATTER_INTIMIDATED,
	HOSTAGE_CHATTER_PAIN,
	HOSTAGE_CHATTER_SCARED_OF_GUNFIRE,
	HOSTAGE_CHATTER_SCARED_OF_MURDER,
	HOSTAGE_CHATTER_LOOK_OUT,
	HOSTAGE_CHATTER_PLEASE_RESCUE_ME,
	HOSTAGE_CHATTER_SEE_RESCUE_ZONE,
	HOSTAGE_CHATTER_IMPATIENT_FOR_RESCUE,
	HOSTAGE_CHATTER_CTS_WIN,
	HOSTAGE_CHATTER_TERRORISTS_WIN,
	HOSTAGE_CHATTER_RESCUED,
	HOSTAGE_CHATTER_WARN_NEARBY,
	HOSTAGE_CHATTER_WARN_SPOTTED,
	HOSTAGE_CHATTER_CALL_TO_RESCUER,
	HOSTAGE_CHATTER_RETREAT,
	HOSTAGE_CHATTER_COUGH,
	HOSTAGE_CHATTER_BLINDED,
	HOSTAGE_CHATTER_SAW_HE_GRENADE,
	HOSTAGE_CHATTER_DEATH_CRY,

	NUM_HOSTAGE_CHATTER_TYPES
};

class CHostageImprov : public CImprov
{
public:
	CHostageImprov( CBaseEntity *entity );
	virtual ~CHostageImprov( void ) { }
	virtual bool IsAlive( void ) const;
	virtual void MoveTo( const Vector &goal );
	virtual void LookAt( const Vector &target );
	virtual void ClearLookAt( void );
	virtual void FaceTo( const Vector &goal );
	virtual void ClearFaceTo( void );
	void FaceOutwards( void );
	virtual bool IsAtMoveGoal( float error = 20.0f ) const;
	virtual bool HasLookAt( void ) const { return m_isLookingAt; }
	virtual bool HasFaceTo( void ) const { return m_isFacingTo; }
	virtual bool IsAtFaceGoal( void ) const;
	virtual bool IsFriendInTheWay( const Vector &goalPos ) const;
	virtual bool IsFriendInTheWay( CBaseEntity *myFriend, const Vector &goalPos ) const;
	bool IsFriendInTheWay( void ) const;
	virtual void MoveForward( void ) { m_moveFlags |= IN_FORWARD; }
	virtual void MoveBackward( void ) { m_moveFlags |= IN_BACK; }
	virtual void StrafeLeft( void ) { m_moveFlags |= IN_MOVELEFT; }
	virtual void StrafeRight( void ) { m_moveFlags |= IN_MOVERIGHT; }
	virtual bool Jump( void );
	virtual void Crouch( void );
	virtual void StandUp( void );
	virtual void TrackPath( const Vector &pathGoal, float deltaT );
	void SetKnownGoodPosition( const Vector &pos );
	const Vector &GetKnownGoodPosition( void ) const { return m_knownGoodPos; }
	void ResetToKnownGoodPosition( void );
	virtual void StartLadder( const CNavLadder *ladder, NavTraverseType how,
		const Vector *approachPos, const Vector *departPos );
	virtual bool TraverseLadder( const CNavLadder *ladder, NavTraverseType how,
		const Vector *approachPos, const Vector *departPos, float deltaT );
	virtual void Run( void );
	virtual void Walk( void );
	virtual void Stop( void );
	virtual float GetMoveAngle( void ) const { return m_moveAngle; }
	virtual float GetFaceAngle( void ) const { return m_moveAngle; }
	virtual const Vector &GetFeet( void ) const;
	virtual const Vector &GetCentroid( void ) const;
	virtual const Vector &GetEyes( void ) const;
	virtual bool IsRunning( void ) const { return m_moveType == Running; }
	virtual bool IsWalking( void ) const { return m_moveType == Walking; }
	virtual bool IsStopped( void ) const { return m_moveType == Stopped; }
	virtual bool IsCrouching( void ) const { return m_isCrouching; }
	virtual bool IsJumping( void ) const { return !m_jumpTimer.IsElapsed(); }
	virtual bool IsUsingLadder( void ) const { return false; }
	virtual bool IsOnGround( void ) const;
	virtual bool IsMoving( void ) const;
	virtual bool CanRun( void ) const { return true; }
	virtual bool CanCrouch( void ) const { return true; }
	virtual bool CanJump( void ) const { return true; }
	virtual bool IsVisible( const Vector &pos, bool testFOV = false ) const;
	virtual bool IsPlayerLookingAtMe( CBasePlayer *other, float cosTolerance = 0.95f ) const;
	virtual CBasePlayer *IsAnyPlayerLookingAtMe( int team = 0, float cosTolerance = 0.95f ) const;
	virtual CBasePlayer *GetClosestPlayerByTravelDistance( int team = 0, float *range = NULL ) const;
	virtual CNavArea *GetLastKnownArea( void ) const { return m_lastKnownArea; }
	virtual void OnUpdate( float deltaT );
	virtual void OnUpkeep( float deltaT );
	virtual void OnReset( void );
	virtual void OnMoveToSuccess( const Vector &goal ) { m_behavior.OnMoveToSuccess( goal ); }
	virtual void OnMoveToFailure( const Vector &goal, MoveToFailureType reason );
	virtual void OnInjury( float amount )
	{
		m_behavior.OnInjury( amount );
		m_lastInjuryTimer.Start();
		Frighten( TERRIFIED );
	}
	virtual void OnGameEvent( GameEventType event, CBaseEntity *entity, CBaseEntity *other );
	virtual void OnTouch( CBaseEntity *other );
	void ResetJump( void ) { m_jumpTimer.Invalidate(); }
	void ApplyForce( Vector force );
	const Vector GetActualVelocity( void ) const { return m_actualVel; }
	enum MoveType { Stopped, Walking, Running };
	void SetMoveLimit( MoveType limit ) { m_moveLimit = limit; }
	MoveType GetMoveLimit( void ) const { return m_moveLimit; }
	CNavPath *GetPath( void ) { return &m_path; }
	CNavPathFollower *GetPathFollower( void ) { return &m_follower; }
	void Idle( void ) { m_behavior.SetState( &m_idleState ); }
	bool IsIdle( void ) const { return m_behavior.IsState( &m_idleState ); }
	void Follow( CBasePlayer *leader )
	{
		m_followState.SetLeader( leader );
		m_behavior.SetState( &m_followState );
	}
	bool IsFollowing( const CBaseEntity *leader = NULL ) const
	{
		if ( !m_behavior.IsState( &m_followState ) )
			return false;

		if ( leader && leader != m_followState.GetLeader() )
			return false;

		return true;
	}
	void Escape( void ) { m_behavior.SetState( &m_escapeState ); }
	bool IsEscaping( void ) const { return m_behavior.IsState( &m_escapeState ); }
	void Retreat( void ) { m_behavior.SetState( &m_retreatState ); }
	bool IsRetreating( void ) const { return m_behavior.IsState( &m_retreatState ); }
	bool IsAtHome( void ) const;
	bool CanSeeRescueZone( void ) const;
	CBaseEntity *GetFollowLeader( void ) const { return (IsFollowing()) ? m_followState.GetLeader() : NULL; }
	CBasePlayer *GetClosestVisiblePlayer( int team );
	float GetTimeSinceLastSawPlayer( int team );
	float GetTimeSinceLastInjury( void );
	float GetTimeSinceLastNoise( void );
	bool IsTerroristNearby( void );
	enum ScareType { NERVOUS, SCARED, TERRIFIED };
	void Frighten( ScareType scare );
	bool IsScared( void ) const;
	ScareType GetScareIntensity( void ) const { return m_scareIntensity; }
	bool IsIgnoringTerrorists( void ) const { return !m_ignoreTerroristTimer.IsElapsed(); }
	float GetAggression( void ) const { return m_aggression; }
	void Chatter( HostageChatterType sayType, bool mustSpeak );
	void DelayedChatter( float delayTime, HostageChatterType sayType, bool mustSpeak );
	void UpdateDelayedChatter( void );
	bool IsTalking( void ) const { return !m_talkingTimer.IsElapsed(); }
	CountdownTimer m_coughTimer;
	CountdownTimer m_grenadeTimer;
	void UpdateGrenadeReactions( void );
	void Afraid( void );
	void Wave( void );
	void Agree( void );
	void Disagree( void );
	void CrouchDie( void );
	void Flinch( Activity activity );
	void UpdateIdleActivity( Activity activity, Activity fidget );
	void UpdateStationaryAnimation( void );
	CHostage *GetEntity( void ) const { return m_hostage; }

private:
	void CheckForNearbyTerrorists( void );
	void UpdatePosition( float deltaT );
	void MoveTowards( const Vector &pos, float deltaT );
	bool FaceTowards( const Vector &target, float deltaT );
	float GetSpeed( void );
	void SetMoveAngle( float angle ) { m_moveAngle = angle; }
	void Wiggle( void );
	void ClearPath( void );
	bool DiscontinuityJump( float ground, bool onlyJumpDown, bool mustJump );
	virtual bool GetSimpleGroundHeightWithFloor( const Vector *pos, float *height, Vector *normal );
	void UpdateVision( void );
	enum { HEAD_YAW, HEAD_PITCH, EYEBROW, SNEER, MOUTH };
	CHostage *m_hostage;
	CNavArea *m_lastKnownArea;
	mutable Vector m_centroid;
	mutable Vector m_eye;
	HostageStateMachine m_behavior;
	HostageIdleState m_idleState;
	HostageEscapeState m_escapeState;
	HostageRetreatState m_retreatState;
	HostageFollowState m_followState;
	HostageAnimateState m_animateState;
	bool m_didFidget;
	float m_aggression;
	IntervalTimer m_lastSawCT;
	IntervalTimer m_lastSawT;
	CountdownTimer m_checkNearbyTerroristTimer;
	bool m_isTerroristNearby;
	CountdownTimer m_nearbyTerroristTimer;
	CountdownTimer m_scaredTimer;
	ScareType m_scareIntensity;
	CountdownTimer m_ignoreTerroristTimer;
	CountdownTimer m_blinkTimer;
	char m_blinkCounter;
	IntervalTimer m_lastInjuryTimer;
	IntervalTimer m_lastNoiseTimer;
	mutable CountdownTimer m_avoidFriendTimer;
	mutable bool m_isFriendInTheWay;
	CountdownTimer m_chatterTimer;
	bool m_isDelayedChatterPending;
	CountdownTimer m_delayedChatterTimer;
	HostageChatterType m_delayedChatterType;
	bool m_delayedChatterMustSpeak;
	CountdownTimer m_talkingTimer;
	unsigned int m_moveFlags;
	Vector2D m_vel;
	Vector m_actualVel;
	Vector m_moveGoal;
	Vector m_knownGoodPos;
	bool m_hasKnownGoodPos;
	Vector m_priorKnownGoodPos;
	bool m_hasPriorKnownGoodPos;
	CountdownTimer m_priorKnownGoodPosTimer;
	IntervalTimer m_collisionTimer;
	Vector m_viewGoal;
	bool m_isLookingAt;
	Vector m_faceGoal;
	bool m_isFacingTo;
	CNavPath m_path;
	CNavPathFollower m_follower;
	Vector m_lastPosition;
	MoveType m_moveType;
	MoveType m_moveLimit;
	bool m_isCrouching;
	CountdownTimer m_minCrouchTimer;
	float m_moveAngle;
	NavRelativeDirType m_wiggleDirection;
	CountdownTimer m_wiggleTimer;
	CountdownTimer m_wiggleJumpTimer;
	CountdownTimer m_inhibitObstacleAvoidance;
	CountdownTimer m_jumpTimer;
	bool m_hasJumped;
	bool m_hasJumpedIntoAir;
	Vector m_jumpTarget;
	CountdownTimer m_clearPathTimer;
	bool m_traversingLadder;
	EHANDLE m_visiblePlayer[32];
	int m_visiblePlayerCount;
	CountdownTimer m_visionTimer;
};

class HostagePathCost
{
public:
	float operator()( CNavArea *area, CNavArea *fromArea, const CNavLadder *ladder )
	{
		if ( !fromArea )
			return 0;

		if ( ladder )
			return 10.0f * ladder->m_length + fromArea->GetCostSoFar();

		float dist = ( *area->GetCenter() - *fromArea->GetCenter() ).Length();

		float cost = dist + fromArea->GetCostSoFar();

		if ( area->GetAttributes() & NAV_CROUCH )
			cost += 10.0f * dist;

		if ( area->GetAttributes() & NAV_JUMP )
			cost += 10.0f * dist;

		return cost;
	}
};

#endif
