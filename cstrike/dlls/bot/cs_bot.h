//========= Copyright (c) 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

//
// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003
//
// NOTE: The CS Bot code uses Doxygen-style comments. If you run Doxygen over this code, it will 
// auto-generate documentation.  Visit www.doxygen.org to download the system for free.
//

#ifndef CS_BOT_H
#define CS_BOT_H

#include "bot.h"
#include "cs_bot_manager.h"
#include "nav_area.h"
#include "nav_node.h"
#include "cs_bot_chatter.h"

class CCSBot;
class CHostage;

const Vector *FindNearbyRetreatSpot( CCSBot *me, float maxRange );
int GetBotFollowCount( CBasePlayer *leader );

//--------------------------------------------------------------------------------------------------------------
/**
 * For use with player->m_rgpPlayerItems[]
 */
enum InventorySlotType
{
	PRIMARY_WEAPON_SLOT = 1,
	PISTOL_SLOT,
	KNIFE_SLOT,
	GRENADE_SLOT,
	C4_SLOT
};

class BotState
{
public:
	virtual void OnEnter( CCSBot *me );
	virtual void OnUpdate( CCSBot *me );
	virtual void OnExit( CCSBot *me );
	virtual const char *GetName( void ) const = 0;
};

class IdleState : public BotState
{
public:
	virtual void OnEnter( CCSBot *me );
	virtual void OnUpdate( CCSBot *me );
	virtual const char *GetName( void ) const;
};

class HuntState : public BotState
{
public:
	virtual void OnEnter( CCSBot *me );
	virtual void OnUpdate( CCSBot *me );
	virtual void OnExit( CCSBot *me );
	virtual const char *GetName( void ) const;

private:
	friend class CCSBot;
	CNavArea *m_huntArea;
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
};

class InvestigateNoiseState : public BotState
{
public:
	virtual void OnEnter( CCSBot *me );
	virtual void OnUpdate( CCSBot *me );
	virtual void OnExit( CCSBot *me );
	virtual const char *GetName( void ) const;
	void AttendCurrentNoise( CCSBot *me );

private:
	Vector m_checkNoisePosition;
};

class FetchBombState : public BotState
{
public:
	virtual void OnEnter( CCSBot *me );
	virtual void OnUpdate( CCSBot *me );
	virtual const char *GetName( void ) const;
};

class PlantBombState : public BotState
{
public:
	virtual void OnEnter( CCSBot *me );
	virtual void OnUpdate( CCSBot *me );
	virtual void OnExit( CCSBot *me );
	virtual const char *GetName( void ) const;
};

class DefuseBombState : public BotState
{
public:
	virtual void OnEnter( CCSBot *me );
	virtual void OnUpdate( CCSBot *me );
	virtual void OnExit( CCSBot *me );
	virtual const char *GetName( void ) const;
};

class EscapeFromBombState : public BotState
{
public:
	virtual void OnEnter( CCSBot *me );
	virtual void OnUpdate( CCSBot *me );
	virtual void OnExit( CCSBot *me );
	virtual const char *GetName( void ) const;
};

class UseEntityState : public BotState
{
public:
	virtual void OnEnter( CCSBot *me );
	virtual void OnUpdate( CCSBot *me );
	virtual void OnExit( CCSBot *me );
	virtual const char *GetName( void ) const;
	void SetEntity( CBaseEntity *entity ) { m_entity = entity; }

private:
	EHANDLE m_entity;
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
};

class MoveToState : public BotState
{
public:
	virtual void OnEnter( CCSBot *me );
	virtual void OnUpdate( CCSBot *me );
	virtual void OnExit( CCSBot *me );
	virtual const char *GetName( void ) const;

private:
	friend class CCSBot;
	Vector m_goalPosition;
	RouteType m_routeType;
	bool m_radioedPlan;
	bool m_askedForCover;
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
};

class AttackState : public BotState
{
public:
	virtual void OnEnter( CCSBot *me );
	virtual void OnUpdate( CCSBot *me );
	virtual void OnExit( CCSBot *me );
	virtual const char *GetName( void ) const;
	void StopAttacking( CCSBot *me );

private:
	friend class CCSBot;
	enum DodgeStateType
	{
		STEADY_ON,
		SLIDE_LEFT,
		SLIDE_RIGHT,
		JUMP,

		NUM_ATTACK_STATES
	};
	DodgeStateType m_dodgeState;
	float m_nextDodgeStateTimestamp;
	CountdownTimer m_repathTimer;
	float m_scopeTimestamp;
	bool m_haveSeenEnemy;
	bool m_isEnemyHidden;
	float m_reacquireTimestamp;
	float m_shieldToggleTimestamp;
	bool m_shieldForceOpen;
	float m_pinnedDownTimestamp;
	bool m_crouchAndHold;
	bool m_didAmbushCheck;
	bool m_shouldDodge;
	bool m_firstDodge;
	bool m_isCoward;
	CountdownTimer m_retreatTimer;
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
};

class BuyState : public BotState
{
public:
	virtual void OnEnter( CCSBot *me );
	virtual void OnUpdate( CCSBot *me );
	virtual void OnExit( CCSBot *me );
	virtual const char *GetName( void ) const;

private:
	bool m_isInitialDelay;
	int m_prefRetries;
	int m_prefIndex;
	int m_retries;
	bool m_doneBuying;
	bool m_buyDefuseKit;
	bool m_buyGrenade;
	bool m_buyShield;
	bool m_buyPistol;
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
};

class HideState : public BotState
{
public:
	virtual void OnEnter( CCSBot *me );
	virtual void OnUpdate( CCSBot *me );
	virtual void OnExit( CCSBot *me );
	virtual const char *GetName( void ) const;

private:
	friend class CCSBot;
	CNavArea *m_searchFromArea;
	float m_range;
	Vector m_hidingSpot;
	bool m_isAtSpot;
	float m_duration;
	bool m_isHoldingPosition;
	float m_holdPositionTime;
	bool m_heardEnemy;
	float m_firstHeardEnemyTime;
	int m_retry;
	Vector m_leaderAnchorPos;
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
};

class FollowState : public BotState
{
public:
	virtual void OnEnter( CCSBot *me );
	virtual void OnUpdate( CCSBot *me );
	virtual void OnExit( CCSBot *me );
	virtual const char *GetName( void ) const;
	void ComputeLeaderMotionState( float speed );

private:
	friend class CCSBot;
	EHANDLE m_leader;
	Vector m_lastLeaderPos;
	bool m_isStopped;
	float m_stoppedTimestamp;
	enum LeaderMotionStateType
	{
		INVALID,
		STOPPED,
		WALKING,
		RUNNING
	};
	LeaderMotionStateType m_leaderMotionState;
	IntervalTimer m_leaderMotionStateTime;
	bool m_isSneaking;
	float m_lastSawLeaderTime;
	CountdownTimer m_repathInterval;
	IntervalTimer m_walkTime;
	bool m_isAtWalkSpeed;
	float m_waitTime;
	CountdownTimer m_idleTimer;
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
};

class CSGameState
{
public:
	enum BombState
	{
		MOVING,
		LOOSE,
		PLANTED,
		DEFUSED,
		EXPLODED
	};

	explicit CSGameState( CCSBot *owner );
	void Reset( void );
	void OnEvent( GameEventType event, CBaseEntity *entity, CBaseEntity *other );
	bool IsRoundOver( void ) const;
	BombState GetBombState( void ) const { return m_bombState; }
	void SetHostageTaken( bool taken ) { m_haveSomeHostagesBeenTaken = taken; }
	void SetBombState( BombState state );
	void UpdateBomber( const Vector *position );
	void UpdateLooseBomb( const Vector *position );
	void UpdatePlantedBomb( const Vector *position );
	float TimeSinceLastSawBomber( void ) const;
	float TimeSinceLastSawLooseBomb( void ) const;
	bool IsLooseBombLocationKnown( void ) const;
	bool IsPlantedBombLocationKnown( void ) const;
	const Vector *GetBombPosition( void ) const;
	enum { UNKNOWN = -1 };
	int GetPlantedBombsite( void ) const;
	bool IsAtPlantedBombsite( void ) const;
	void MarkBombsiteAsPlanted( int bombsite );
	void ClearBombsite( int bombsite );
	bool IsBombsiteClear( int bombsite ) const;
	int GetNextBombsiteToSearch( void );
	void InitializeHostageInfo( void );
	unsigned int ValidateHostagePositions( void );
	void AllHostagesGone( void );
	bool AreAllHostagesGone( void ) const;
	bool AreAllHostagesBeingRescued( void ) const;
	CHostage *GetNearestFreeHostage( Vector *position ) const;
	CHostage *GetNearestVisibleFreeHostage( void ) const;
	const Vector *GetRandomFreeHostagePosition( void ) const;

private:
	friend class IdleState;
	struct HostageInfo
	{
		CHostage *hostage;
		Vector knownPos;
		bool isValid;
		bool isAlive;
		bool isFree;
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
	};

	CCSBot *m_owner;
	bool m_isRoundOver;
	BombState m_bombState;
	IntervalTimer m_lastSawBomber;
	Vector m_bomberPos;
	IntervalTimer m_lastSawLooseBomb;
	Vector m_looseBombPos;
	bool m_isBombsiteClear[4];
	int m_bombsiteSearchOrder[4];
	int m_bombsiteCount;
	int m_bombsiteSearchIndex;
	int m_plantedBombsite;
	bool m_isPlantedBombPosKnown;
	Vector m_plantedBombPos;
	HostageInfo m_hostage[12];
	int m_hostageCount;
	CountdownTimer m_validateInterval;
	bool m_allHostagesRescued;
	bool m_haveSomeHostagesBeenTaken;
};

class CCSBot : public CBot
{
public:
	enum TaskType
	{
		SEEK_AND_DESTROY,
		PLANT_BOMB,
		FIND_TICKING_BOMB,
		DEFUSE_BOMB,
		GUARD_TICKING_BOMB,
		GUARD_BOMB_DEFUSER,
		GUARD_LOOSE_BOMB,
		GUARD_BOMB_ZONE,
		ESCAPE_FROM_BOMB,
		HOLD_POSITION,
		FOLLOW,
		VIP_ESCAPE,
		GUARD_VIP_ESCAPE_ZONE,
		COLLECT_HOSTAGES,
		RESCUE_HOSTAGES,
		GUARD_HOSTAGES,
		GUARD_HOSTAGE_RESCUE_ZONE,
		MOVE_TO_LAST_KNOWN_ENEMY_POSITION,
		MOVE_TO_SNIPER_SPOT,
		SNIPING
	};

	enum DispositionType
	{
		ENGAGE_AND_INVESTIGATE,
		OPPORTUNITY_FIRE,
		SELF_DEFENSE,
		IGNORE_ENEMIES
	};

	CCSBot( void );
	virtual bool Initialize( const BotProfile *profile );
	virtual void SpawnBot( void );
	virtual void Upkeep( void );
	virtual void Update( void );
	virtual void Walk( void );
	virtual bool Jump( bool mustJump = false );
	virtual int TakeDamage( entvars_t *inflictor, entvars_t *attacker, float damage, int damageType );
	virtual void Killed( entvars_t *attacker, int gib );
	virtual void RoundRespawn( void );
	virtual void Blind( float duration, float holdTime, float fadeTime, int alpha );
	virtual void OnTouchingWeapon( CWeaponBox *box );
	virtual void OnEvent( GameEventType event, CBaseEntity *entity = NULL, CBaseEntity *other = NULL );
	virtual bool IsVisible( const Vector *position, bool testFOV = false ) const;
	virtual bool IsVisible( CBasePlayer *player, bool testFOV = false, unsigned char *visibleParts = NULL ) const;
	virtual bool IsEnemyPartVisible( VisiblePartType part ) const;

	void ResetValues( void );
	void Disconnect( void );

private:
	void EXPORT BotTouch( CBaseEntity *entity );

public:
	void BotDeathThink( void );
	CBasePlayer *FindNearbyPlayer( void );
	void SetEnemy( CBasePlayer *enemy );
	void Panic( CBasePlayer *enemy );
	CBasePlayer *GetAttacker( void ) const;
	bool IsDoingScenario( void ) const;
	bool NoticeLooseBomb( void ) const;
	bool CanSeeLooseBomb( void ) const;
	bool CanSeePlantedBomb( void ) const;
	void GetOffLadder( void );
	float GetHidingSpotCheckTimestamp( HidingSpot *spot ) const;
	void SetHidingSpotCheckTimestamp( HidingSpot *spot );
	void UpdateHostageEscortCount( void );
	bool IsOutnumbered( void ) const;
	int OutnumberedCount( void ) const;
	CBasePlayer *GetImportantEnemy( bool checkVisibility ) const;
	void SetDisposition( DispositionType disposition );
	DispositionType GetDisposition( void ) const;
	void IgnoreEnemies( float duration );
	void IncreaseMorale( void );
	void DecreaseMorale( void );
	bool IsRogue( void ) const;
	bool IsHurrying( void ) const;
	bool IsSafe( void ) const;
	bool IsWellPastSafe( void ) const;
	bool IsEndOfSafeTime( void ) const;
	float GetSafeTimeRemaining( void ) const;
	void AdjustSafeTime( void );
	bool HasNotSeenEnemyForLongTime( void ) const;
	bool GuardRandomZone( float range );
	float GetRangeToFarthestEscortedHostage( void ) const;

	void SetState( BotState *state );
	void StopAttacking( void );
	void Attack( CBasePlayer *victim );
	void Idle( void );
	void Hunt( void );
	void Follow( CBasePlayer *player );
	void ContinueFollowing( void );
	void StopFollowing( void );
	void Hide( CNavArea *searchFromArea, float duration = -1.0f, float hideRange = 750.0f, bool holdPosition = false );
	void Hide( const Vector *hidingSpot, float duration = -1.0f, bool holdPosition = false );
	bool TryToHide( CNavArea *searchFromArea = NULL, float duration = -1.0f, float hideRange = 750.0f, bool holdPosition = false, bool useNearest = false );
	bool TryToRetreat( void );
	void MoveTo( const Vector *pos, RouteType route = SAFEST_ROUTE );
	void UseEntity( CBaseEntity *entity );
	void FetchBomb( void );
	void PlantBomb( void );
	void DefuseBomb( void );
	void EscapeFromBomb( void );
	void InvestigateNoise( void );
	void RescueHostages( void );
	bool IsAttacking( void ) const;
	bool IsEscapingFromBomb( void ) const;
	bool IsDefusingBomb( void ) const;
	bool IsHiding( void ) const;
	bool IsAtHidingSpot( void ) const;
	bool IsHunting( void ) const;
	bool IsMovingTo( void ) const;
	bool IsBuying( void ) const;
	bool IsBusy( void ) const;
	Place GetPlace( void ) const;

	bool ComputePath( CNavArea *goalArea, const Vector *goal, RouteType route );
	void DestroyPath( void ) { m_pathLength = 0; m_pathLadder = NULL; }
	void BuildTrivialPath( const Vector *goal );
	bool ComputePathPositions( void );
	void SetPathIndex( int newIndex );
	float GetPathDistanceRemaining( void ) const;
	enum PathResult
	{
		PROGRESSING,		///< we are moving along the path
		END_OF_PATH,		///< we reached the end of the path
		PATH_FAILURE		///< we failed to reach the end of the path
	};
	PathResult UpdatePathMovement( bool allowSpeedChange = true );
	bool IsNearJump( void ) const;
	bool IsStraightLinePathWalkable( const Vector *position ) const;
	int FindClosestPointOnPath( const Vector *worldPos, int startIndex, int endIndex, Vector *close ) const;
	int FindOurPositionOnPath( Vector *close, bool local = false ) const;
	int FindPathPoint( float aheadRange, Vector *point, int *prevIndex );
	void DrawPath( void );
	void ComputeApproachPoints( void );
	void DrawApproachPoints( void );
	bool FindApproachPointNearestPath( Vector *pos );
	void SetupLadderMovement( void );
	bool UpdateLadderMovement( void );
	void ComputeLadderEndpoint( bool isAscending );
	void FeelerReflexAdjustment( Vector *goalPosition );
	void MoveTowardsPosition( const Vector *pos );
	void MoveAwayFromPosition( const Vector *pos );
	void StrafeAwayFromPosition( const Vector *pos );
	bool DiscontinuityJump( float ground, bool onlyJumpDown, bool mustJump );
	bool GetSimpleGroundHeightWithFloor( const Vector *pos, float *height, Vector *normal );
	float GetApproximateFallDamage( float height ) const;
	bool IsFriendInTheWay( const Vector *goalPos ) const;
	void ResetStuckMonitor( void );
	void StuckCheck( void );
	void Wiggle( void );
	bool StayOnNavMesh( void );

	void UpdateLookAngles( void );
	void UpdateLookAt( void );
	void UpdateLookAround( bool updateApproachPoints = true );
	bool UpdateLookAtNoise( void );
	void UpdatePeripheralVision( void );
	void SetLookAt( const char *description, const Vector *position, PriorityType priority, float duration, bool clearIfClose = false, float tolerance = 5.0f );
	void InhibitLookAround( float duration );
	bool BendLineOfSight( const Vector *eye, const Vector *target, Vector *visible ) const;
	bool CanSeeNoisePosition( void ) const;
	bool CanHearNearbyEnemyGunfire( float range ) const;
	bool ShouldInvestigateNoise( float *range );
	CBasePlayer *FindMostDangerousThreat( void );
	float ComputeWeaponSightRange( void );
	void UpdateReactionQueue( void );
	CBasePlayer *GetRecognizedEnemy( void );
	bool IsRecognizedEnemyReloading( void );
	bool IsRecognizedEnemyProtectedByShield( void );
	float GetRangeToNearestRecognizedEnemy( void );
	void SetAimOffset( float accuracy );
	void UpdateAimOffset( void );
	bool AdjustZoom( float range );
	bool IsUsingAWP( void ) const;
	bool DoesActiveWeaponHaveSilencer( void ) const;
	bool IsUsingSniperRifle( void ) const;
	bool IsSniper( void ) const;
	bool IsSniping( void ) const;
	bool IsUsingShotgun( void ) const;
	bool IsUsingMachinegun( void ) const;
	bool IsPrimaryWeaponEmpty( void ) const;
	bool IsPistolEmpty( void ) const;
	bool IsUsingKnife( void ) const;
	bool IsUsingPistol( void ) const;
	bool IsUsingGrenade( void ) const;
	bool IsUsingHEGrenade( void ) const;
	bool HasGrenade( void ) const;
	bool DoEquip( CBasePlayerWeapon *weapon );
	void EquipKnife( void );
	void EquipPistol( void );
	void EquipBestWeapon( bool force = false );
	bool EquipGrenade( bool avoidSmoke = false );
	void ThrowGrenade( const Vector *target );
	bool FindGrenadeTossPathTarget( Vector *target );
	void ReloadCheck( void );
	void SilencerCheck( void );
	bool IsFriendInLineOfFire( void );
	void FireWeaponAtEnemy( void );

	void StartVoiceFeedback( float duration );
	void EndVoiceFeedback( bool force = false );
	bool IsUsingVoice( void ) const { return m_voiceEndTime != 0.0f; }
	void SendRadioMessage( GameEventType event );
	bool IsRadioCommand( GameEventType event ) const;
	void RespondToRadioCommands( void );
	bool RespondToHelpRequest( CBasePlayer *them, Place place, float maxRange );
	void StartNormalProcess( void );
	void StartLearnProcess( void );
	void UpdateLearnProcess( void );
	void StartAnalyzeAlphaProcess( void );
	void UpdateAnalyzeAlphaProcess( void );
	void StartAnalyzeBetaProcess( void );
	void UpdateAnalyzeBetaProcess( void );
	void StartSaveProcess( void );
	void UpdateSaveProcess( void );
	bool LearnStep( void );
	CNavNode *AddNode( const Vector *position, const Vector *normal, NavDirType direction, CNavNode *parent );

	CSGameState *GetGameState( void ) { return &m_gameState; }
	const CSGameState *GetGameState( void ) const { return &m_gameState; }
	BotChatterInterface *GetChatter( void ) { return &m_chatter; }
	int GetNearbyEnemyCount( void ) const { return min( GetEnemiesRemaining(), m_nearbyEnemyCount ); }
	int GetNearbyFriendCount( void ) const { return min( GetFriendsRemaining(), m_nearbyFriendCount ); }
	Place GetLastEnemyPlace( void ) const { return m_enemyPlace; }
	TaskType GetTask( void ) const { return m_task; }
	void SetTask( TaskType task, CBaseEntity *entity = NULL ) { m_task = task; m_taskEntity = entity; }
	CNavArea *GetLastKnownArea( void ) const { return m_lastKnownArea; }
	float GetEnemyLastSeenTimestamp( void ) const { return m_lastSawEnemyTimestamp; }
	float GetLastEnemyDeathTimestamp( void ) const { return m_enemyDeathTimestamp; }
	int GetHostageEscortCount( void ) const { return m_hostageEscortCount; }

private:
	friend class IdleState;
	friend class CCSBotManager;
	friend class HuntState;
	friend class AttackState;
	friend class InvestigateNoiseState;
	friend class BuyState;
	friend class MoveToState;
	friend class FetchBombState;
	friend class PlantBombState;
	friend class DefuseBombState;
	friend class HideState;
	friend class EscapeFromBombState;
	friend class FollowState;
	friend class UseEntityState;
	friend class PathCost;
	friend int GetBotFollowCount( CBasePlayer *leader );

	struct ConnectInfo
	{
		CNavArea *area;
		NavTraverseType how;
		Vector pos;
		const CNavLadder *ladder;
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
	};
	struct HidingSpotCheckInfo
	{
		HidingSpot *spot;
		float timestamp;
	};
	struct WatchInfo
	{
		float timestamp;
		bool isEnemy;
	};
	struct ReactionState
	{
		EHANDLE player;
		bool isReloading;
		bool isProtectedByShield;
	};

	char m_name[64];
	float m_combatRange;
	mutable bool m_isRogue;
	mutable CountdownTimer m_rogueTimer;
	int m_morale;
	bool m_diedLastRound;
	float m_safeTime;
	bool m_wasSafe;
	int m_blindMoveDir;
	bool m_blindFire;
	float m_surpriseDelay;
	float m_lastPanicTime;
	bool m_isFollowing;
	EHANDLE m_leader;
	float m_followTimestamp;
	float m_allowAutoFollowTime;
	CountdownTimer m_hurryTimer;
	IdleState m_idleState;
	HuntState m_huntState;
	AttackState m_attackState;
	InvestigateNoiseState m_investigateNoiseState;
	BuyState m_buyState;
	MoveToState m_moveToState;
	FetchBombState m_fetchBombState;
	PlantBombState m_plantBombState;
	DefuseBombState m_defuseBombState;
	HideState m_hideState;
	EscapeFromBombState m_escapeFromBombState;
	FollowState m_followState;
	UseEntityState m_useEntityState;
	BotState *m_state;
	float m_stateTimestamp;
	bool m_isAttacking;
	TaskType m_task;
	EHANDLE m_taskEntity;
	Vector m_goalPosition;
	EHANDLE m_goalEntity;
	CNavArea *m_currentArea;
	CNavArea *m_lastKnownArea;
	EHANDLE m_avoid;
	float m_avoidTimestamp;
	bool m_isJumpCrouching;
	bool m_hasJumpCrouched;
	float m_jumpCrouchTimestamp;
	ConnectInfo m_path[256];
	int m_pathLength;
	int m_pathIndex;
	float m_areaEnteredTimestamp;
	CountdownTimer m_repathTimer;
	mutable CountdownTimer m_avoidFriendTimer;
	mutable bool m_isFriendInTheWay;
	CountdownTimer m_politeTimer;
	bool m_isWaitingBehindFriend;
	int m_pathLadderState;
	bool m_pathLadderFaceIn;
	const CNavLadder *m_pathLadder;
	int m_pathLadderDismountDir;
	float m_pathLadderDismountTimestamp;
	float m_pathLadderEnd;
	float m_pathLadderTimestamp;
	CountdownTimer m_mustRunTimer;
	CSGameState m_gameState;
	unsigned char m_hostageEscortCount;
	float m_hostageEscortCountTimestamp;
	bool m_isWaitingForHostage;
	CountdownTimer m_inhibitWaitingForHostageTimer;
	CountdownTimer m_waitForHostageTimer;
	Vector m_noisePosition;
	float m_noiseTimestamp;
	CNavArea *m_noiseArea;
	float m_noiseCheckTimestamp;
	PriorityType m_noisePriority;
	bool m_noiseTravelDistanceChecked;
	float m_lookAroundStateTimestamp;
	float m_lookAheadAngle;
	float m_forwardAngle;
	float m_inhibitLookAroundTimestamp;
	int m_lookAtSpotState;
	Vector m_lookAtSpot;
	PriorityType m_lookAtSpotPriority;
	float m_lookAtSpotDuration;
	float m_lookAtSpotTimestamp;
	float m_lookAtSpotAngleTolerance;
	bool m_lookAtSpotClearIfClose;
	const char *m_lookAtDesc;
	float m_peripheralTimestamp;
	Vector m_approachPoint[16];
	unsigned char m_approachPointCount;
	Vector m_approachPointViewPosition;
	bool m_isWaitingToThrowGrenade;
	CountdownTimer m_tossGrenadeTimer;
	SpotEncounter *m_spotEncounter;
	float m_spotCheckTimestamp;
	HidingSpotCheckInfo m_checkedHidingSpot[64];
	int m_checkedHidingSpotCount;
	float m_lookPitch;
	float m_lookPitchVel;
	float m_lookYaw;
	float m_lookYawVel;
	mutable Vector m_eyePosition;
	Vector m_aimOffset;
	Vector m_aimOffsetGoal;
	float m_aimOffsetTimestamp;
	float m_aimSpreadTimestamp;
	Vector m_aimSpot;
	DispositionType m_disposition;
	CountdownTimer m_ignoreEnemiesTimer;
	EHANDLE m_enemy;
	bool m_isEnemyVisible;
	unsigned char m_visibleEnemyParts;
	Vector m_lastEnemyPosition;
	float m_lastSawEnemyTimestamp;
	float m_firstSawEnemyTimestamp;
	float m_currentEnemyAcquireTimestamp;
	float m_enemyDeathTimestamp;
	bool m_isLastEnemyDead;
	int m_nearbyEnemyCount;
	Place m_enemyPlace;
	WatchInfo m_watchInfo[32];
	EHANDLE m_bomber;
	int m_nearbyFriendCount;
	EHANDLE m_closestVisibleFriend;
	EHANDLE m_closestVisibleHumanFriend;
	CBasePlayer *m_attacker;
	float m_attackedTimestamp;
	int m_lastVictimID;
	bool m_isAimingAtEnemy;
	bool m_isRapidFiring;
	IntervalTimer m_equipTimer;
	float m_fireWeaponTimestamp;
	ReactionState m_enemyQueue[20];
	unsigned char m_enemyQueueIndex;
	unsigned char m_enemyQueueCount;
	unsigned char m_enemyQueueAttendIndex;
	bool m_isStuck;
	float m_stuckTimestamp;
	Vector m_stuckSpot;
	int m_wiggleDirection;
	float m_nextWiggleTime;
	float m_nextJumpTime;
	float m_avgVel[5];
	int m_avgVelIndex;
	int m_avgVelCount;
	Vector m_lastOrigin;
	GameEventType m_lastRadioCommand;
	float m_lastRadioRecievedTimestamp;
	float m_lastRadioSentTimestamp;
	EHANDLE m_radioSubject;
	Vector m_radioPosition;
	float m_voiceStartTime;
	float m_voiceEndTime;
	BotChatterInterface m_chatter;
	unsigned int m_navigationState;
	CNavNode *m_currentNode;
	NavDirType m_currentLearnDirection;
	NavAreaList::iterator m_currentAnalyzeArea;
	int m_process;
	CountdownTimer m_processTimer[3];
};

//--------------------------------------------------------------------------------------------------------------
/**
 * Functor used with NavAreaBuildPath()
 */
class PathCost
{
public:
	PathCost( CCSBot *bot, RouteType route = SAFEST_ROUTE ) : m_bot( bot ), m_route( route ) {}

	float operator()( CNavArea *area, CNavArea *fromArea, const CNavLadder *ladder )
	{
		if ( fromArea == NULL && m_route == FASTEST_ROUTE )
			return 0.0f;
		const float aggression = m_bot->GetProfile()->GetAggression();

		// respond to the danger modulated by our aggression (even super-aggressives pay SOME attention to danger)
		const float dangerFactor = (1.0f - aggression * 0.95f) * 100.0f;

		// first area in path, cost is just danger
		if ( fromArea == NULL )
			return area->GetDanger( m_bot->m_iTeam - 1 ) * dangerFactor;

		// compute distance from previous area to this area
		float dist = ladder ? ladder->m_length : (*area->GetCenter() - *fromArea->GetCenter()).Length();

		// compute distance travelled along path so far
		float cost = fromArea->GetCostSoFar() + dist;

		// zombies ignore all path penalties
		if ( cv_bot_zombie.value > 0.0f )
			return cost;

		if ( !area->IsConnected( fromArea, NUM_DIRECTIONS ) )
		{
			// this is a "jump down" (one way drop) transition - estimate damage we will take to traverse it
			float fallDamage = -fromArea->ComputeHeightChange( area ) * 0.2178f - 26.0f;
			if ( fallDamage > 0.0f )
			{
				// if the fall would kill us, don't use it
				if ( fallDamage + 10.0f >= m_bot->pev->health )
					return -1.0f;

				// if we need to get there in a hurry, ignore minor pain
				if ( m_route != FASTEST_ROUTE || fallDamage > 15.0f * aggression + 10.0f )
				{
					// cost is proportional to how much it hurts when we fall
					// 10 points - not a big deal, 50 points - ouch!
					cost += fallDamage * (100.0f * fallDamage);
				}
			}
		}
		if ( area->GetAttributes() & NAV_CROUCH )
		{
			// these areas are very slow to move through
			float penalty = m_route == FASTEST_ROUTE ? 20.0f : 5.0f;

			// avoid crouch areas if we are rescuing hostages 
			if ( m_bot->GetHostageEscortCount() )
				penalty *= 3.0f;

			cost += penalty * dist;
		}

		// if this is a "jump" area, add penalty
		if ( area->GetAttributes() & NAV_JUMP )
		{
			// jumping can slow you down
			cost += dist;
		}

		if ( m_route == SAFEST_ROUTE )
		{
			// add in the danger of this path - danger is per unit length travelled
			cost += area->GetDanger( m_bot->m_iTeam - 1 ) * (dist * dangerFactor);
		}

		if ( !m_bot->IsAttacking() )
		{
			// add in cost of teammates in the way

			// approximate density of teammates based on area
			float size = (area->GetSizeX() + area->GetSizeY()) * 0.5f;

			// degenerate check
			if ( size >= 1.0f )
			{
				// cost is proportional to the density of teammates in this area
				cost += area->GetPlayerCount( m_bot->m_iTeam, m_bot ) * 50000.0f / size;
			}
		}
		return cost;
	}

private:
	CCSBot *m_bot;
	RouteType m_route;
};

#endif
