#ifndef HOSTAGE_LOCALNAV_H
#define HOSTAGE_LOCALNAV_H

#include "hostage.h"

typedef int node_index_t;

typedef struct localnode_s
{
	Vector vecLoc;
	int offsetX;
	int offsetY;
	byte bDepth;
	BOOL fSearched;
	node_index_t nindexParent;
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
} localnode_t;

class CLocalNav
{
public:
	CLocalNav( CHostage *pOwner );
	virtual ~CLocalNav();
	void SetTargetEnt( CBaseEntity *pTarget ) { m_pTargetEnt = pTarget ? pTarget->edict() : NULL; }
	node_index_t FindPath( Vector &vecStart, Vector &vecDest, float flTargetRadius, int fNoMonsters );
	int SetupPathNodes( node_index_t nindex, Vector *vecNodes, int fNoMonsters );
	int GetFurthestTraversableNode( Vector &vecStartingLoc, Vector *vecNodes, int nTotalNodes, int fNoMonsters );
	int PathTraversable( Vector &vecSource, Vector &vecDest, int fNoMonsters );
	BOOL PathClear( Vector &vecOrigin, Vector &vecDest, int fNoMonsters, TraceResult &tr );
	BOOL PathClear( Vector &source, Vector &destination, int noMonsters )
	{
		TraceResult trace;
		return PathClear( source, destination, noMonsters, trace );
	}
	static void Think();
	void RequestNav( CHostage *pCaller );
	static void Reset();
	static void HostagePrethink();
	static float s_flStepSize;

private:
	node_index_t AddNode( node_index_t nindexParent, Vector &vecLoc, int offsetX, int offsetY, byte bDepth );
	localnode_t *GetNode( node_index_t nindex );
	node_index_t NodeExists( int offsetX, int offsetY );
	void AddPathNodes( node_index_t nindexSource, int fNoMonsters );
	void AddPathNode( node_index_t nindexSource, int offsetX, int offsetY, int fNoMonsters );
	node_index_t GetBestNode( Vector &vecOrigin, Vector &vecDest );
	BOOL SlopeTraversable( Vector &vecSource, Vector &vecDest, int fNoMonsters, TraceResult &tr );
	BOOL LadderTraversable( Vector &vecSource, Vector &vecDest, int fNoMonsters, TraceResult &tr );
	BOOL StepTraversable( Vector &vecSource, Vector &vecDest, int fNoMonsters, TraceResult &tr );
	BOOL StepJumpable( Vector &vecSource, Vector &vecDest, int fNoMonsters, TraceResult &tr );
	node_index_t FindDirectPath( Vector &vecStart, Vector &vecDest, float flTargetRadius, int fNoMonsters );
	BOOL LadderHit( Vector &vecSource, Vector &vecDest, TraceResult &tr );
	CHostage *m_pOwner;
	edict_t *m_pTargetEnt;
	BOOL m_fTargetEntHit;
	localnode_t *m_nodeArr;
	node_index_t m_nindexAvailableNode;
	Vector m_vecStartingLoc;
	static EHANDLE queue[20];
	static int qptr;
	static int tot_inqueue;
	static float nodeval;
	static float flNextCvarCheck;
	static float flLastThinkTime;
	static EHANDLE hostages[20];
	static int tot_hostages;
};

#endif
