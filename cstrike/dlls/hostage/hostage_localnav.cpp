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

#include "hostage_localnav.h"
#include <math.h>
#include <string.h>


float CLocalNav::s_flStepSize;

EHANDLE CLocalNav::queue[20];
int CLocalNav::qptr;
int CLocalNav::tot_inqueue;
float CLocalNav::nodeval;
float CLocalNav::flNextCvarCheck;
float CLocalNav::flLastThinkTime;

EHANDLE CLocalNav::hostages[20];
int CLocalNav::tot_hostages;

#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
CLocalNav::CLocalNav( CHostage *pOwner )
{
	m_pOwner = pOwner;
	m_pTargetEnt = NULL;

	m_nodeArr = new localnode_t[100];

	if ( tot_hostages <= 19 )
	{
		hostages[tot_hostages] = pOwner;
		tot_hostages++;
	}
}


CLocalNav::~CLocalNav( void )
{
	delete[] m_nodeArr;
}


node_index_t CLocalNav::AddNode( node_index_t nindexParent, Vector &vecLoc, int offsetX, int offsetY, byte bDepth )
{
	localnode_t *nodeNew;

	if ( m_nindexAvailableNode == 100 )
		return -1;

	nodeNew = GetNode( m_nindexAvailableNode );

	nodeNew->vecLoc = vecLoc;
	nodeNew->offsetX = offsetX;
	nodeNew->offsetY = offsetY;
	nodeNew->bDepth = bDepth;
	nodeNew->fSearched = FALSE;
	nodeNew->nindexParent = nindexParent;

	return m_nindexAvailableNode++;
}


inline localnode_t *CLocalNav::GetNode( node_index_t nindex )
{
	return &m_nodeArr[nindex];
}


node_index_t CLocalNav::NodeExists( int offsetX, int offsetY )
{
	node_index_t nindexCurrent;
	localnode_t *nodeCurrent;

	nindexCurrent = m_nindexAvailableNode - 1;
	nodeCurrent = GetNode( m_nindexAvailableNode );

	while ( nindexCurrent != -1 )
	{
		nodeCurrent--;

		if ( nodeCurrent->offsetX == offsetX && nodeCurrent->offsetY == offsetY )
			return nindexCurrent;

		nindexCurrent--;
	}

	return -1;
}


void CLocalNav::AddPathNodes( node_index_t nindexSource, int fNoMonsters )
{
	AddPathNode( nindexSource, 1, 0, fNoMonsters );
	AddPathNode( nindexSource, -1, 0, fNoMonsters );
	AddPathNode( nindexSource, 0, 1, fNoMonsters );
	AddPathNode( nindexSource, 0, -1, fNoMonsters );
	AddPathNode( nindexSource, 1, 1, fNoMonsters );
	AddPathNode( nindexSource, 1, -1, fNoMonsters );
	AddPathNode( nindexSource, -1, 1, fNoMonsters );
	AddPathNode( nindexSource, -1, -1, fNoMonsters );
}


void CLocalNav::AddPathNode( node_index_t nindexSource, int offsetX, int offsetY, int fNoMonsters )
{
	int bDepth;
	Vector vecSource;
	Vector vecDest;
	int offsetXAbs;
	int offsetYAbs;

	if ( nindexSource == -1 )
	{
		vecSource = m_vecStartingLoc;
		vecDest = vecSource + Vector( offsetX * 26.0f, offsetY * 26.0f, 0 );

		bDepth = 1;
	}
	else
	{
		localnode_t *nodeSource;
		localnode_t *nodeCurrent;
		node_index_t nindexCurrent;

		nodeSource = GetNode( nindexSource );

		offsetXAbs = nodeSource->offsetX + offsetX;
		offsetYAbs = nodeSource->offsetY + offsetY;

		if ( NodeExists( offsetXAbs, offsetYAbs ) != -1 )
			return;

		vecSource = nodeSource->vecLoc;
		vecDest = vecSource + Vector( offsetX * 26.0f, offsetY * 26.0f, 0 );

		nindexCurrent = m_nindexAvailableNode - 1;
		nodeCurrent = GetNode( m_nindexAvailableNode );

		while ( nindexCurrent != -1 )
		{
			nodeCurrent--;

			if ( abs( nodeCurrent->offsetX - offsetXAbs ) <= 1 &&
				abs( nodeCurrent->offsetY - offsetYAbs ) <= 1 &&
				PathTraversable( nodeCurrent->vecLoc, vecDest, fNoMonsters ) )
			{
				nodeSource = nodeCurrent;
				nindexSource = nindexCurrent;
			}

			nindexCurrent--;
		}
		vecSource = nodeSource->vecLoc;
		bDepth = nodeSource->bDepth + 1;
		offsetX = offsetXAbs;
		offsetY = offsetYAbs;
	}
	if ( PathTraversable( vecSource, vecDest, fNoMonsters ) )
	{
		AddNode( nindexSource, vecDest, offsetX, offsetY, bDepth );
	}
}


node_index_t CLocalNav::GetBestNode( Vector &vecOrigin, Vector &vecDest )
{
	node_index_t nindexCurrent;
	localnode_t *nodeCurrent;
	node_index_t nindexBest;
	float flBestVal;

	nindexBest = -1;
	flBestVal = 1000000.0f;

	nodeCurrent = m_nodeArr;

	for ( nindexCurrent = 0; nindexCurrent < m_nindexAvailableNode; nindexCurrent++, nodeCurrent++ )
	{
		float flCurrentVal;
		float flDistFromStart;
		float flDistToDest;
		float flZDiff;

		if ( nodeCurrent->fSearched != TRUE )
		{
			flDistFromStart = nodeCurrent->bDepth * 26.0;

			flDistToDest = ( nodeCurrent->vecLoc - vecDest ).Length();

			flZDiff = nodeCurrent->vecLoc.z - vecDest.z;
			flZDiff = fabs( flZDiff );

			flZDiff = flZDiff <= s_flStepSize ? 1.0f : 1.25f;

			flCurrentVal = ( flDistToDest + flDistFromStart ) * flZDiff;

			if ( flBestVal > flCurrentVal )
			{
				flBestVal = flCurrentVal;
				nindexBest = nindexCurrent;
			}
		}
	}

	return nindexBest;
}


int CLocalNav::SetupPathNodes( node_index_t nindex, Vector *vecNodes, int fNoMonsters )
{
	node_index_t nCurrentIndex;
	int nNodeCount;

	nNodeCount = 0;
	nCurrentIndex = nindex;

	while ( nCurrentIndex != -1 )
	{
		localnode_t *nodeCurrent;
		Vector vecCurrentLoc;

		nodeCurrent = GetNode( nCurrentIndex );
		vecCurrentLoc = nodeCurrent->vecLoc;

		vecNodes[nNodeCount] = vecCurrentLoc;
		nNodeCount++;

		nCurrentIndex = nodeCurrent->nindexParent;
	}

	return nNodeCount;
}


int CLocalNav::GetFurthestTraversableNode( Vector &vecStartingLoc, Vector *vecNodes, int nTotalNodes, int fNoMonsters )
{
	int nCount;

	for ( nCount = 0; nCount < nTotalNodes; nCount++ )
	{
		if ( PathTraversable( vecStartingLoc, vecNodes[nCount], fNoMonsters ) )
			return nCount;
	}

	return -1;
}


node_index_t CLocalNav::FindPath( Vector &vecStart, Vector &vecDest, float flTargetRadius, int fNoMonsters )
{
	node_index_t nIndexBest;

	nIndexBest = FindDirectPath( vecStart, vecDest, flTargetRadius, fNoMonsters );
	if ( nIndexBest != -1 )
	{
		return nIndexBest;
	}

	m_vecStartingLoc = vecStart;

	m_nindexAvailableNode = 0;
	AddPathNodes( -1, fNoMonsters );

	nIndexBest = GetBestNode( vecStart, vecDest );
	while ( nIndexBest != -1 )
	{
		localnode_t *node;
		Vector vecNodeLoc;
		float flDistToDest;

		node = GetNode( nIndexBest );
		node->fSearched = TRUE;
		vecNodeLoc = node->vecLoc;
		flDistToDest = ( vecDest - vecNodeLoc ).Length2D();

		if ( flDistToDest <= flTargetRadius )
			break;

		if ( flDistToDest <= 26 )
			break;

		if ( flDistToDest - flTargetRadius > ( 100 - m_nindexAvailableNode ) * 26.0 )
		{
			nIndexBest = -1;
			break;
		}

		if ( m_nindexAvailableNode == 100 )
		{
			nIndexBest = -1;
			break;
		}

		AddPathNodes( nIndexBest, fNoMonsters );
		nIndexBest = GetBestNode( vecStart, vecDest );
	}

	if ( m_nindexAvailableNode <= 10 )
		nodeval += 2;
	else if ( m_nindexAvailableNode <= 20 )
		nodeval += 4;
	else if ( m_nindexAvailableNode <= 30 )
		nodeval += 8;
	else if ( m_nindexAvailableNode <= 40 )
		nodeval += 13;
	else if ( m_nindexAvailableNode <= 50 )
		nodeval += 19;
	else if ( m_nindexAvailableNode <= 60 )
		nodeval += 26;
	else if ( m_nindexAvailableNode <= 70 )
		nodeval += 34;
	else if ( m_nindexAvailableNode <= 80 )
		nodeval += 43;
	else if ( m_nindexAvailableNode <= 90 )
		nodeval += 53;
	else if ( m_nindexAvailableNode <= 100 )
		nodeval += 64;
	else if ( m_nindexAvailableNode <= 110 )
		nodeval += 76;
	else if ( m_nindexAvailableNode <= 120 )
		nodeval += 89;
	else if ( m_nindexAvailableNode <= 130 )
		nodeval += 103;
	else if ( m_nindexAvailableNode <= 140 )
		nodeval += 118;
	else if ( m_nindexAvailableNode <= 150 )
		nodeval += 134;
	else if ( m_nindexAvailableNode <= 160 )
		nodeval += 151;
	else
		nodeval += 169;

	return nIndexBest;
}


node_index_t CLocalNav::FindDirectPath( Vector &vecStart, Vector &vecDest, float flTargetRadius, int fNoMonsters )
{
	Vector vecActualDest;
	Vector vecPathDir;
	Vector vecNodeLoc;
	node_index_t nindexLast;

	vecPathDir = ( vecDest - vecStart ).Normalize();

	vecActualDest = vecDest - vecPathDir * flTargetRadius;
	if ( !PathTraversable( vecStart, vecActualDest, fNoMonsters ) )
	{
		return -1;
	}

	m_nindexAvailableNode = 0;
	nindexLast = -1;
	vecNodeLoc = vecStart;

	while ( ( vecNodeLoc - vecActualDest ).Length2D() >= 26 )
	{
		node_index_t nindexCurrent;

		vecNodeLoc = vecNodeLoc + vecPathDir * 26;

		nindexCurrent = AddNode( nindexLast, vecNodeLoc, 0, 0, 0 );
		if ( nindexCurrent == -1 )
			break;

		nindexLast = nindexCurrent;
	}

	return nindexLast;
}


BOOL CLocalNav::PathClear( Vector &vecOrigin, Vector &vecDest, int fNoMonsters, TraceResult &tr )
{
	TRACE_MONSTER_HULL( m_pOwner->edict(), vecOrigin, vecDest, fNoMonsters, m_pOwner->edict(), &tr );

	if ( tr.fStartSolid )
		return FALSE;

	if ( tr.flFraction == 1 )
		return TRUE;

	if ( tr.pHit == m_pTargetEnt )
	{
		m_fTargetEntHit = TRUE;
		return TRUE;
	}

	return FALSE;
}


int CLocalNav::PathTraversable( Vector &vecSource, Vector &vecDest, int fNoMonsters )
{
	TraceResult tr;
	Vector vecSrcTmp;
	Vector vecDestTmp;
	Vector vecDir;
	float flTotal;
	int retval;

	retval = 0;

	vecSrcTmp = vecSource;
	vecDir = ( vecDest - vecSource ).Normalize();
	vecDir.z = 0;
	flTotal = ( vecDest - vecSource ).Length2D();

	while ( flTotal > 1 )
	{
		if ( flTotal < s_flStepSize )
		{
			vecDestTmp = vecDest;
		}
		else
		{
			vecDestTmp = vecSrcTmp + vecDir * s_flStepSize;
		}

		m_fTargetEntHit = FALSE;

		if ( PathClear( vecSrcTmp, vecDestTmp, fNoMonsters, tr ) )
		{
			vecDestTmp = tr.vecEndPos;

			if ( !retval )
				retval = 1;
		}
		else
		{
			if ( tr.fStartSolid )
				return 0;

			if ( !fNoMonsters && tr.pHit )
			{
				if ( tr.pHit->v.classname && FClassnameIs( tr.pHit, "hostage_entity" ) )
					return 0;
			}

			vecSrcTmp = tr.vecEndPos;

			if ( tr.vecPlaneNormal.z > 0.7 )
			{
				if ( !SlopeTraversable( vecSrcTmp, vecDestTmp, fNoMonsters, tr ) )
					return 0;

				if ( !retval )
					retval = 1;
			}
			else
			{
				if ( StepTraversable( vecSrcTmp, vecDestTmp, fNoMonsters, tr ) )
				{
					if ( !retval )
						retval = 2;
				}
				else if ( StepJumpable( vecSrcTmp, vecDestTmp, fNoMonsters, tr ) )
				{
					if ( !retval )
						retval = 3;
				}
				else
				{
					return 0;
				}
			}
		}

		Vector vecDropDest;

		vecDropDest = vecDestTmp;
		vecDropDest.z -= 300;

		if ( PathClear( vecDestTmp, vecDropDest, fNoMonsters, tr ) )
		{
			return 0;
		}

		if ( !tr.fStartSolid )
		{
			vecDestTmp = tr.vecEndPos;
		}

		vecSrcTmp = vecDestTmp;

		if ( m_fTargetEntHit )
			break;

		flTotal = ( vecDest - vecSrcTmp ).Length2D();
	}

	vecDest = vecDestTmp;

	return retval;
}


BOOL CLocalNav::SlopeTraversable( Vector &vecSource, Vector &vecDest, int fNoMonsters, TraceResult &tr )
{
	Vector vecSlopeEnd;
	Vector vecDown;

	vecSlopeEnd = vecDest;
	vecSlopeEnd.z = (float)( vecSource.z + tan( ( 90.0 - UTIL_VecToAngles( tr.vecPlaneNormal ).x ) * M_PI / 180.0 ) * ( vecDest - vecSource ).Length2D() );

	if ( !PathClear( vecSource, vecSlopeEnd, fNoMonsters, tr ) )
	{
		if ( tr.fStartSolid )
			return FALSE;

		if ( ( tr.vecEndPos - vecSource ).Length2D() < 1 )
			return FALSE;
	}

	vecSlopeEnd = tr.vecEndPos;
	vecDown = vecSlopeEnd;
	vecDown.z -= s_flStepSize;

	if ( !PathClear( vecSlopeEnd, vecDown, fNoMonsters, tr ) )
	{
		if ( tr.fStartSolid )
		{
			vecDest = vecSlopeEnd;
			return TRUE;
		}
	}

	vecDest = tr.vecEndPos;

	return TRUE;
}


BOOL CLocalNav::LadderTraversable( Vector &vecSource, Vector &vecDest, int fNoMonsters, TraceResult &tr )
{
	Vector vecLadderStart;
	Vector vecLadderDest;

	vecLadderStart = tr.vecEndPos;
	vecLadderDest = vecLadderStart;
	vecLadderDest.z += 26;

	if ( !PathClear( vecLadderStart, vecLadderDest, fNoMonsters, tr ) )
	{
		if ( tr.fStartSolid )
			return FALSE;

		if ( ( tr.vecEndPos - vecLadderStart ).Length() < 1 )
			return FALSE;
	}

	vecLadderStart = tr.vecEndPos;
	vecDest.z = vecLadderStart.z;

	return PathTraversable( vecLadderStart, vecDest, fNoMonsters );
}


BOOL CLocalNav::StepTraversable( Vector &vecSource, Vector &vecDest, int fNoMonsters, TraceResult &tr )
{
	Vector vecStepStart;
	Vector vecStepDest;

	vecStepStart = vecSource;
	vecStepStart.z += s_flStepSize;

	vecStepDest = vecDest;
	vecStepDest.z = vecStepStart.z;

	if ( !PathClear( vecStepStart, vecStepDest, fNoMonsters, tr ) )
	{
		if ( tr.fStartSolid )
		{
			return FALSE;
		}
		if ( ( tr.vecEndPos - vecStepStart ).Length() < 1 )
			return FALSE;
	}

	vecStepStart = tr.vecEndPos;
	vecStepDest = vecStepStart;
	vecStepDest.z -= s_flStepSize;

	if ( !PathClear( vecStepStart, vecStepDest, fNoMonsters, tr ) )
	{
		if ( tr.fStartSolid )
		{
			vecDest = vecStepStart;
			return TRUE;
		}
	}

	vecDest = tr.vecEndPos;

	return TRUE;
}


BOOL CLocalNav::StepJumpable( Vector &vecSource, Vector &vecDest, int fNoMonsters, TraceResult &tr )
{
	Vector vecStepStart;
	Vector vecStepDest;
	float flJumpHeight;
	BOOL fJumpClear;

	fJumpClear = FALSE;

	flJumpHeight = s_flStepSize + 1;

	vecStepStart = vecSource;
	vecStepStart.z += flJumpHeight;

	while ( flJumpHeight < 40 )
	{
		vecStepDest = vecDest;
		vecStepDest.z = vecStepStart.z;

		if ( !PathClear( vecStepStart, vecStepDest, fNoMonsters, tr ) )
		{
			if ( tr.fStartSolid )
				break;

			if ( ( tr.vecEndPos - vecStepStart ).Length2D() < 1 )
			{
				flJumpHeight += 10;
				vecStepStart.z += 10;
				continue;
			}
		}

		fJumpClear = TRUE;
		break;
	}

	if ( !fJumpClear )
		return FALSE;

	vecStepStart = tr.vecEndPos;
	vecStepDest = vecStepStart;
	vecStepDest.z -= s_flStepSize;

	if ( !PathClear( vecStepStart, vecStepDest, fNoMonsters, tr ) )
	{
		if ( tr.fStartSolid )
		{
			vecDest = vecStepStart;
			return TRUE;
		}
	}

	vecDest = tr.vecEndPos;

	return TRUE;
}


BOOL CLocalNav::LadderHit( Vector &vecSource, Vector &vecDest, TraceResult &tr )
{
	Vector vecFwd;
	Vector vecRight;
	Vector vecUp;

	UTIL_MakeVectorsPrivate( UTIL_VecToAngles( -tr.vecPlaneNormal ), vecFwd, vecRight, vecUp );

	if ( UTIL_PointContents( tr.vecEndPos + vecFwd * 15 + vecUp * 36 ) == CONTENTS_LADDER )
		return TRUE;
	if ( UTIL_PointContents( tr.vecEndPos + vecFwd * 15 - vecUp * 36 ) == CONTENTS_LADDER )
		return TRUE;
	if ( UTIL_PointContents( tr.vecEndPos + vecFwd * 15 + vecRight * 16 + vecUp * 36 ) == CONTENTS_LADDER )
		return TRUE;
	if ( UTIL_PointContents( tr.vecEndPos + vecFwd * 15 - vecRight * 16 + vecUp * 36 ) == CONTENTS_LADDER )
		return TRUE;
	if ( UTIL_PointContents( tr.vecEndPos + vecFwd * 15 + vecRight * 16 - vecUp * 36 ) == CONTENTS_LADDER )
		return TRUE;
	if ( UTIL_PointContents( tr.vecEndPos + vecFwd * 15 - vecRight * 16 - vecUp * 36 ) == CONTENTS_LADDER )
		return TRUE;

	return FALSE;
}


void CLocalNav::Think( void )
{
	float flElapsedTime;
	EHANDLE hCallback;
	static cvar_t *sv_stepsize;

	if ( gpGlobals->time >= flNextCvarCheck )
	{
		if ( !sv_stepsize )
		{
			sv_stepsize = CVAR_GET_POINTER( "sv_stepsize" );
		}

		flNextCvarCheck = gpGlobals->time + 1;

		if ( sv_stepsize )
			s_flStepSize = sv_stepsize->value;
		else
			s_flStepSize = 18;
	}

	flElapsedTime = gpGlobals->time - flLastThinkTime;
	flLastThinkTime = gpGlobals->time;

	HostagePrethink();

	nodeval -= flElapsedTime * 250;
	if ( nodeval < 0 )
		nodeval = 0;
	else if ( nodeval > 17 )
		return;

	if ( !tot_inqueue )
		return;

	hCallback = NULL;

	hCallback = queue[qptr];
	while ( !hCallback && tot_inqueue > 0 )
	{
		qptr++;
		if ( qptr == 20 )
			qptr = 0;
		tot_inqueue--;

		if ( !tot_inqueue )
		{
			hCallback = NULL;
			break;
		}

		hCallback = queue[qptr];
	}

	if ( hCallback )
	{
		CHostage *pHostage;

		pHostage = GetClassPtr( (CHostage *)hCallback->pev );

		if ( ++qptr == 20 )
			qptr = 0;

		tot_inqueue--;
		pHostage->NavReady();
	}
}


void CLocalNav::RequestNav( CHostage *pCaller )
{
	if ( nodeval <= 17 && !tot_inqueue )
	{
		pCaller->NavReady();
	}
	else
	{
		if ( tot_inqueue <= 19 )
		{
			int curr = qptr;
			int found = FALSE;
			for ( int i = 0; i < tot_inqueue; i++ )
			{
				CHostage *pQueueItem;

				pQueueItem = GetClassPtr( (CHostage *)queue[curr]->pev );

				if ( pQueueItem == pCaller )
				{
					found = TRUE;
					break;
				}

				curr++;

				if ( curr == 20 )
					curr = 0;
			}
			if ( !found )
			{
				queue[curr] = pCaller;
				tot_inqueue++;
			}
		}
	}
}


void CLocalNav::Reset( void )
{
	flNextCvarCheck = 0;
	flLastThinkTime = 0;
	tot_inqueue = 0;
	qptr = 0;
	nodeval = 0;

	tot_hostages = 0;
}


void CLocalNav::HostagePrethink( void )
{
	int iCount;

	for ( iCount = 0; iCount < tot_hostages; iCount++ )
	{
		if ( hostages[iCount] )
		{
			GetClassPtr( (CHostage *)hostages[iCount]->pev )->PreThink();
		}
	}
}
