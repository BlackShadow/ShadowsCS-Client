// perf_probe.h - developer timing probe for the client DLL.
//
// Compiled in only when SHADOWS_PERF_PROBE is defined (never in normal builds). It times the engine's calls into
// the client and the HUD elements, and every few seconds prints how much of each frame they took to the console.
#pragma once

#ifdef SHADOWS_PERF_PROBE
#include <chrono>

enum PerfBucket
{
	PERF_REDRAW,
	PERF_UPDATECLIENTDATA,
	PERF_CREATEMOVE,
	PERF_CALCREFDEF,
	PERF_ADDENTITY,
	PERF_CREATEENTITIES,
	PERF_TEMPENTUPDATE,
	PERF_TRIANGLES,
	PERF_POSTRUNCMD,
	PERF_STUDIOEVENT,
	PERF_HUDFRAME,
	PERF_STUDIO_MODEL,
	PERF_STUDIO_PLAYER,
	PERF_ENG_DRAWPOINTS,
	PERF_ENG_LIGHTING,
	PERF_ENG_CHECKBBOX,
	PERF_VISMODS,
	PERF_OTHER_EXPORTS,
	PERF_BUCKETS
};

inline long long PerfNow()
{
	return std::chrono::duration_cast<std::chrono::nanoseconds>(
		std::chrono::steady_clock::now().time_since_epoch() ).count();
}

void PerfProbe_Add( int bucket, long long nanoseconds );
void PerfProbe_AddHud( const void* element, const char* name, long long nanoseconds );
void PerfProbe_BoneCheck( int bones, int different );
void PerfProbe_BoneTime( long long oldNanoseconds, long long newNanoseconds );

struct PerfScope
{
	int bucket;
	long long start;
	explicit PerfScope( int b ) : bucket( b ), start( PerfNow() ) {}
	~PerfScope() { PerfProbe_Add( bucket, PerfNow() - start ); }
};
#define PERF_CONCAT2( a, b ) a##b
#define PERF_CONCAT( a, b ) PERF_CONCAT2( a, b )
#define PERF_SCOPE( bucket ) PerfScope PERF_CONCAT( perfScope, __LINE__ )( bucket )
#else
#define PERF_SCOPE( bucket )
#endif
