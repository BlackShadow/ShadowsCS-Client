#ifndef HOSTAGE_STATES_H
#define HOSTAGE_STATES_H

#include "bot_util.h"
#pragma warning( push )
#pragma warning( disable: 4100 )
#include "improv.h"
#include "simple_state_machine.h"
#pragma warning( pop )

class CHostageImprov;

class HostageState : public SimpleState<CHostageImprov *>, public IImprovEvent
{
public:
	virtual void UpdateStationaryAnimation( CHostageImprov *improv ) {}
};

class HostageStateMachine : public SimpleStateMachine<CHostageImprov *, HostageState>, public IImprovEvent
{
	friend class CHostageImprov;
public:
	void OnMoveToSuccess( const Vector &goal ) override { if ( m_state ) m_state->OnMoveToSuccess( goal ); }
	void OnMoveToFailure( const Vector &goal, MoveToFailureType reason ) override { if ( m_state ) m_state->OnMoveToFailure( goal, reason ); }
	void OnInjury( float amount ) override { if ( m_state ) m_state->OnInjury( amount ); }
	void UpdateStationaryAnimation( CHostageImprov *improv ) { if ( m_state ) m_state->UpdateStationaryAnimation( improv ); }
};

class HostageIdleState : public HostageState
{
	friend class CHostageImprov;
public:
	void OnEnter( CHostageImprov *improv ) override;
	void OnUpdate( CHostageImprov *improv ) override;
	void OnExit( CHostageImprov *improv ) override;
	const char *GetName() const override { return "Idle"; }
	void OnMoveToSuccess( const Vector &goal ) override { m_moveState = MoveDone; }
	void OnMoveToFailure( const Vector &goal, MoveToFailureType reason ) override { m_moveState = MoveFailed; }
	void OnInjury( float amount ) override { m_fleeTimer.Invalidate(); m_mustFlee = true; }
	void UpdateStationaryAnimation( CHostageImprov *improv ) override;

private:
	CountdownTimer m_waveTimer;
	CountdownTimer m_fleeTimer;
	CountdownTimer m_disagreeTimer;
	CountdownTimer m_escapeTimer;
	CountdownTimer m_askTimer;
	IntervalTimer m_intimidatedTimer;
	CountdownTimer m_pleadTimer;
	enum { NotMoving, Moving, MoveDone, MoveFailed } m_moveState;
	bool m_mustFlee;
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
};

class HostageEscapeToCoverState : public HostageState
{
public:
	void OnEnter( CHostageImprov *improv ) override;
	void OnUpdate( CHostageImprov *improv ) override;
	void OnExit( CHostageImprov *improv ) override;
	const char *GetName() const override { return "Escape:ToCover"; }
	void OnMoveToFailure( const Vector &goal, MoveToFailureType reason ) override;
	void SetRescueGoal( const Vector &rescueGoal ) { m_rescueGoal = rescueGoal; }

private:
	Vector m_rescueGoal;
	Vector m_spot;
	bool m_canEscape;
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
};

class HostageEscapeLookAroundState : public HostageState
{
public:
	void OnEnter( CHostageImprov *improv ) override;
	void OnUpdate( CHostageImprov *improv ) override;
	void OnExit( CHostageImprov *improv ) override;
	const char *GetName() const override { return "Escape:LookAround"; }

private:
	CountdownTimer m_timer;
};

class HostageEscapeState : public HostageState
{
public:
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
	HostageEscapeState()
	{
		m_toCoverState.SetParent( this );
		m_lookAroundState.SetParent( this );
	}
	void OnEnter( CHostageImprov *improv ) override;
	void OnUpdate( CHostageImprov *improv ) override;
	void OnExit( CHostageImprov *improv ) override;
	void OnMoveToFailure( const Vector &goal, MoveToFailureType reason ) override { m_behavior.OnMoveToFailure( goal, reason ); }
	const char *GetName() const override { return "Escape"; }
	void ToCover() { m_behavior.SetState( &m_toCoverState ); }
	void LookAround() { m_behavior.SetState( &m_lookAroundState ); }

private:
	HostageEscapeToCoverState m_toCoverState;
	HostageEscapeLookAroundState m_lookAroundState;
	HostageStateMachine m_behavior;
	bool m_canEscape;
	CountdownTimer m_runTimer;
};

class HostageRetreatState : public HostageState
{
public:
	void OnEnter( CHostageImprov *improv ) override;
	void OnUpdate( CHostageImprov *improv ) override;
	void OnExit( CHostageImprov *improv ) override;
	const char *GetName() const override { return "Retreat"; }
};

class HostageFollowState : public HostageState
{
public:
	void OnEnter( CHostageImprov *improv ) override;
	void OnUpdate( CHostageImprov *improv ) override;
	void OnExit( CHostageImprov *improv ) override;
	void UpdateStationaryAnimation( CHostageImprov *improv ) override;
	const char *GetName() const override { return "Follow"; }
	void SetLeader( CBaseEntity *leader ) { m_leader = leader; }
	CBaseEntity *GetLeader() const { return (EHANDLE &)m_leader; }

private:
	EHANDLE m_leader;
	Vector m_lastLeaderPos;
	bool m_isWaiting;
	float m_stopRange;
	CountdownTimer m_makeWayTimer;
	CountdownTimer m_impatientTimer;
	CountdownTimer m_repathTimer;
	bool m_isWaitingForFriend;
	CountdownTimer m_waitForFriendTimer;
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
};

class HostageAnimateState : public HostageState
{
	friend class CHostageImprov;
public:
	enum PerformanceType
	{
		None, Walk, Run, Jump, Fall, Crouch, CrouchWalk, Calm, Anxious, Afraid,
		Sitting, GettingUp, Waving, LookingAround, Disagreeing, Flinching
	};
	HostageAnimateState() { Reset(); }
	void OnEnter( CHostageImprov *improv ) override;
	void OnUpdate( CHostageImprov *improv ) override;
	void OnExit( CHostageImprov *improv ) override;
	const char *GetName() const override { return "Animate"; }
	void Reset();
	void AddSequence( CHostageImprov *improv, const char *seqName, float holdTime, float rate );
	void AddSequence( CHostageImprov *improv, int activity, float holdTime, float rate );
	bool IsBusy() const { return m_sequenceCount != 0; }
	bool IsPlaying( CHostageImprov *improv, const char *seqName ) const;
	int GetCurrentSequenceID() const { return m_sequence[m_currentSequence].seqID; }
	PerformanceType GetPerformance() const { return m_performance; }
	void SetPerformance( PerformanceType performance ) { m_performance = performance; }

private:
	enum { MAX_SEQUENCES = 8 };
	struct SeqInfo
	{
		int seqID;
		float holdTime;
		float rate;
	};
	void StartSequence( CHostageImprov *improv, const SeqInfo *seqInfo );
	bool IsDoneHolding();
	SeqInfo m_sequence[MAX_SEQUENCES];
	int m_sequenceCount;
	int m_currentSequence;
	PerformanceType m_performance;
	bool m_isHolding;
	CountdownTimer m_holdTimer;
};

#endif
