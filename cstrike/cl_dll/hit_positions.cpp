// hit_positions.cpp - see hit_positions.h.
//
// Where the server tests a shot (hw.so; the dedicated server runs the same code):
// - With each packet of commands the client sends the number of the newest update it has read. The server takes that
//   update's round trip, less its interval between updates to that client, as the client's latency
//   (SV_ExecuteClientMessage: realtime - senttime - next_messageinterval; SV_CalcClientTime: the newest
//   sv_unlagsamples of them, 1 unless the server's owner changed it).
// - For each command in the packet it then moves the other players to where they were at
//   realtime - latency - lerp, lerp being the command's lerp_msec, 100 msec at most and that interval at least
//   (SV_SetupMove) - so at the acknowledged update's own time, less only what lerp_msec is over the interval: the
//   place each player had in that very update (a straight line between two updates when it falls between them).
//   Places only: a player's pose is the one he has now. (The time is kept as a float of the server's running time,
//   so on a server left running for days it is rounded to 8, 16, 31 msec - nothing a client can do about that.)
// What the client draws:
// - The engine sets its clock by each update (CL_Parse_Time, CL_AdjustClock) and puts every other player where he was
//   at clock - ex_interp, between the places it has kept (CL_ComputePlayerOrigin): smooth, but behind the place the
//   server tests by ex_interp less the time since the newest update came - half an update's time on average, more on
//   a server that sends few (33 msec apart at sv_maxupdaterate 30: 4 units on average, 8 at most, at running speed).
// - A frame sends its command first (CL_Move), then reads the updates that came, then draws (_Host_Frame). So the
//   command made from what a frame showed acknowledges just the update that frame drew from - when it is sent in the
//   frame that makes it (cl_cmdrate: Auto-rate keeps it high for this).
//
// So another player is drawn at the place the server will test (always: it was an option of the Advanced window's Misc
// tab at first; user: "make exact hit pos default and remove the option"): the one in the newest update -
// or, with an ex_interp over the update interval, where he was that much before it. His animation, angles and
// everything else stay the engine's; only the model is set down there, for the drawing. He then moves in steps, one an
// update (hardly seen at 100 a second, plain at 30): those steps are where he can be hit.
// Not while watching (dead, spectating, a demo, HLTV), and not with lag compensation off (cl_lc or cl_lw 0: the
// server then tests the players where they are at the moment the command arrives, a round trip ahead of any update).
// (user: "any other improvements that will improve bullet reg")
#include "hud.h"
#include "cl_util.h"
#include "cl_entity.h"
#include "demo_api.h"
#include "auto_rate.h"
#include "hit_positions.h"

#include <math.h>
#include <string.h>

namespace
{

const int kKept = 64;			// updates kept for each player (lerp_msec reaches back 100 msec at most)
const float kGap = 1.0f;		// seconds without an update of a player: what was kept is no use
const float kJump = 128.0f;		// units along an axis between two updates: the server doesn't follow him back over it

struct Update
{
	int number;		// the update it came in (messagenum)
	float time;		// the server's time on it
	float origin[3];
};

struct Track
{
	Update updates[kKept];	// the newest at 'newest'
	int newest;
	int count;
};

Track g_tracks[MAX_PLAYERS + 1];

// the update n before the newest
const Update& Back( const Track& track, int n )
{
	return track.updates[( track.newest - n + kKept ) % kKept];
}

// how long before the acknowledged update the server tests: the command's lerp_msec over its update interval
float TestedBefore()
{
	// lerp_msec as the engine works it out (CL_ComputeClientInterpolationAmount)
	float updaterate = CVAR_GET_FLOAT( "cl_updaterate" );
	updaterate = updaterate < 10.0f ? 10.0f : updaterate > 102.0f ? 102.0f : updaterate;
	int least = (int)( 1000.0f / updaterate );
	if ( least <= 0 )
		least = 1;
	int lerp = (int)( 1000.0f * CVAR_GET_FLOAT( "ex_interp" ) );
	if ( lerp < least )
		lerp = least;
	if ( lerp > 100 )
		lerp = 100;
	// the server's interval: the one asked for, unless it keeps to a rate of its own (sv_maxupdaterate,
	// sv_minupdaterate) - then what arrives tells it, to within one of its frames
	float interval = 1.0f / updaterate;
	const int arriving = AutoRate_Arriving();
	if ( arriving >= 10 && ( arriving < updaterate * 0.8f || arriving > updaterate * 1.2f ) )
		interval = 1.0f / arriving;
	const float before = lerp / 1000.0f - interval;
	return before > 0.0005f ? before : 0.0f;
}

} // namespace

void HitPositions_VidInit()
{
	memset( g_tracks, 0, sizeof( g_tracks ) );
}

void HitPositions_AddPlayer( cl_entity_t* player )
{
	if ( !player || !player->player || player->index < 1 || player->index > MAX_PLAYERS )
		return;
	Track& track = g_tracks[player->index];
	const entity_state_t& state = player->curstate;
	if ( track.count > 0 )
	{
		const Update& last = Back( track, 0 );
		if ( last.number == state.messagenum )
			return;	// the update of the frame before
		// another game's updates, a while out of sight, or a jump the server gives up on too: begun again
		if ( state.messagenum < last.number || state.msg_time <= last.time || state.msg_time - last.time > kGap
			 || fabsf( state.origin[0] - last.origin[0] ) > kJump || fabsf( state.origin[1] - last.origin[1] ) > kJump
			 || fabsf( state.origin[2] - last.origin[2] ) > kJump )
			track.count = 0;
	}
	track.newest = ( track.newest + 1 ) % kKept;
	Update& update = track.updates[track.newest];
	update.number = state.messagenum;
	update.time = state.msg_time;
	for ( int i = 0; i < 3; i++ )
		update.origin[i] = state.origin[i];
	if ( track.count < kKept )
		track.count++;
}

bool HitPositions_Origin( const cl_entity_t* player, float* origin )
{
	if ( !player || !player->player || player->index < 1 || player->index > MAX_PLAYERS )
		return false;
	const cl_entity_t* local = gEngfuncs.GetLocalPlayer();
	if ( !local || player->index == local->index )
		return false;
	if ( g_iUser1 || gHUD.m_fPlayerDead || gEngfuncs.IsSpectateOnly() || gEngfuncs.pDemoAPI->IsPlayingback() )
		return false;	// watching
	if ( CVAR_GET_FLOAT( "cl_lc" ) == 0.0f || CVAR_GET_FLOAT( "cl_lw" ) == 0.0f )
		return false;	// no lag compensation
	const Track& track = g_tracks[player->index];
	if ( track.count <= 0 || Back( track, 0 ).number != player->curstate.messagenum )
		return false;	// (not kept this frame)
	const Update& newest = Back( track, 0 );
	const float before = TestedBefore();
	if ( before > 0.0f )
	{
		const float when = newest.time - before;
		for ( int n = 1; n < track.count; n++ )
		{
			const Update& older = Back( track, n );
			if ( older.time > when )
				continue;
			const Update& newer = Back( track, n - 1 );
			const float span = newer.time - older.time;
			const float part = span > 0.0f ? ( when - older.time ) / span : 0.0f;
			for ( int i = 0; i < 3; i++ )
				origin[i] = older.origin[i] + part * ( newer.origin[i] - older.origin[i] );
			return true;
		}
		// further back than is kept (he has just come into sight): the oldest there is
		const Update& oldest = Back( track, track.count - 1 );
		for ( int i = 0; i < 3; i++ )
			origin[i] = oldest.origin[i];
		return true;
	}
	for ( int i = 0; i < 3; i++ )
		origin[i] = newest.origin[i];
	return true;
}
