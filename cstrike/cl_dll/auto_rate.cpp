// auto_rate.cpp - see auto_rate.h.
//
// What the engine does (hw.so):
// - Every update from the server carries the player's own data (svc_clientdata), which CL_ParseClientdata hands to
//   HUD_TxferPredictionData - once per update, from the second one after joining on - so updates are counted there.
// - Net_Status: latency is the round trip of the last command the server confirmed (set on each update; what net_graph
//   shows); packet_loss the share of the last CL_UPDATE_BACKUP updates that never came, worked out once a second in
//   CL_Move - the loss on the way in. The client sends the server that number too (the scoreboard's loss).
// - The client keeps cl_updaterate within 10..102 and puts ex_interp up to 1000 / cl_updaterate msec (or down to 100)
//   when it is outside that (CL_ComputeClientInterpolationAmount, easing the interpolation it uses towards it); the
//   result goes to the server with every command (lerp_msec), which lag compensation rewinds the other players by.
//   It keeps cl_cmdrate at 10 or more, sends at most one command packet a frame and keeps cl_cmdbackup within 0..8
//   (CL_Move).
// - rate, cl_updaterate, cl_lc and cl_lw are userinfo: set while connected, they go to the server ("setinfo",
//   Cvar_DirectSet). The server takes rate within 1000..100000 and its sv_minrate / sv_maxrate, cl_updaterate within its
//   sv_minupdaterate / sv_maxupdaterate (SV_ExtractFromUserinfo; 10 and 30 when left as they are, so such a server
//   sends 30 a second); cl_lc 0 turns lag compensation off for the player, cl_lw 0 weapon prediction. It sends at most
//   once a server frame (the next update due 1 / cl_updaterate after the last, at the first frame from then: 77 asked
//   of a server running 100 frames a second brings 50), and fewer when rate holds it back (choke).
//
// - The server's lag compensation: each packet of commands names the newest update the client had read; the server
//   takes that update's round trip less its update interval as the latency (SV_ExecuteClientMessage,
//   SV_CalcClientTime) and moves the other players to where they were at realtime - latency - lerp, lerp being the
//   command's lerp_msec, 100 ms at most and that interval at least (SV_SetupMove). That is the acknowledged update's
//   own time, less only what lerp_msec is over the interval: the places the players had in that very update. Their
//   places only: the pose the hit boxes take is the one they have now.
// - A frame makes its command and sends it (CL_Move) before it reads what came and draws (_Host_Frame): a command
//   sent in the frame that makes it acknowledges just the update the frame before drew from. One held back (no more
//   than cl_cmdrate packets go out a second; the engine has no upper limit on it) goes with the next packet and that
//   one's acknowledgement - a newer update than the player aimed at, if one came meanwhile.
//
// So, for the best hit registration: rate and cl_updaterate at the top (100000, 102), whatever the server then sends
// (asking for exactly what arrives would land between its frames and bring fewer still). ex_interp at the engine's
// least for the cl_updaterate asked (1000 / cl_updaterate msec): lerp_msec is then never over the server's interval,
// whatever it holds the update rate to, so the server tests the acknowledged update's places exactly - what the
// Exact hit positions option draws (hit_positions.cpp) - and the engine's own drawing is at its nearest to them
// (0.035 on a server sending 30 a second, as Auto-rate had it, drew the players 4.6 units from where they were
// tested, on average at running speed; 0.009 draws them 1.8 from it). cl_cmdrate 250: at up to 125 frames a second
// every command leaves in its own frame, and at 480 nine in ten are tested against the update they were aimed at
// (at 105, six in ten); not more, since servers count a client's packets (ReHLDS drops one sending over 400 a
// second, unless its owner changed that). cl_lc and cl_lw on.
// For poor connections: cl_cmdbackup up with the loss (more of the last commands sent again in each packet, so a lost
// packet loses no shot); loss that stays high: lower update and command rates tried, kept only if the loss falls
// with them, taken back up once it is gone.
//
// A change is made as soon as it is wanted (user: "make it update everytime when change needs to be occured"); small
// wobbles of what arrives don't make one. A setting someone else changes (the server, the console) is left alone for
// the rest of the map. The player's own settings from before are kept in cl_autorate_saved and put back when
// Auto-rate is switched off. Measuring goes on with Auto-rate off too, for the Misc tab's readout.
#include "hud.h"
#include "cl_util.h"
#include "net_api.h"
#include "demo_api.h"
#include "auto_rate.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <string>

namespace
{

cvar_t* cl_autorate;
cvar_t* cl_autorate_saved;	// the player's own settings from before, in kNames' order

// what Auto-rate sets, in the order it sets them (cl_updaterate before ex_interp: the engine checks one by the other;
// the first five in AutoRateReadout::managed's order)
enum Setting
{
	RATE,
	UPDATE_RATE,
	CMD_RATE,
	CMD_BACKUP,
	INTERP,		// ex_interp, in msec here
	LAG_COMP,	// cl_lc
	PREDICTION,	// cl_lw
	SETTINGS
};
const char* const kNames[SETTINGS] = { "rate", "cl_updaterate", "cl_cmdrate", "cl_cmdbackup", "ex_interp", "cl_lc", "cl_lw" };

const int kTopRate = 100000;		// the most a server takes (SV_ExtractFromUserinfo)
const int kTopUpdateRate = 102;		// the most the client asks for (CL_ComputeClientInterpolationAmount)
const int kTopCmdRate = 250;		// packets a second at most (see the top)
const int kTopInterp = 100;			// msec: the engine's own most
const int kLowestUpdateRate = 20;	// lower rates tried against loss go no lower
const int kLowestCmdRate = 30;
const double kSettle = 3.0;			// seconds in the level before measuring (loading)
const double kHitch = 0.25;			// a frame longer than this: that second isn't counted
const double kStartupWait = 5.0;	// seconds after the game starts before the switch is looked at (config.cfg read)
const double kJustSet = 6.0;		// seconds the readout says what was just set
const int kWindow = AutoRateReadout::kHistory;	// seconds of measurements kept
const int kRecent = 10;				// seconds the connection is judged over
const int kFirstSeconds = 3;		// measured before the map's first change, and after any before what arrives counts
const int kSendsSeconds = 10;		// measured at the top cl_updaterate before what the server sends is judged
const int kTrialSeconds = 20;		// the loss measured before and after lower rates are tried
const int kLossLag = 3;				// seconds the engine's loss figure (its last 64 updates) trails a change by
const int kCleanSeconds = 120;		// seconds in a row without loss before lower rates go back up (at first)
const int kMostCleanSeconds = 900;	// (at most, after going up brought the loss back)
const int kRelapseSeconds = 60;		// loss back this soon after going up: from going up
const double kNoTrialsFor = 300.0;	// the loss wasn't from the rates: not tried again for this long
const float kSomeLoss = 1.0f;		// percent: cl_cmdbackup 4
const float kHighLoss = 5.0f;		// percent: cl_cmdbackup 6, lower rates tried
const float kVeryHighLoss = 10.0f;	// percent: cl_cmdbackup 8 (the engine's most)

// one second of measurements
struct Second
{
	int updates;	// updates from the server that arrived
	int frames;
	int loss;		// percent (Net_Status at the second's end)
	float ping;		// round trip, on average (ms; 0: none came)
	float spread;	// how much it varied (ms, standard deviation; from 2 samples on)
	int pings;		// round trips measured in it
};

// where the game is
enum Where
{
	NOWHERE,	// not connected
	LOCAL,		// a game on this computer
	DEMO,		// a demo playing
	JOINING,	// connected, no level yet
	PLAYING
};

Second g_seconds[kWindow];	// the newest at g_iNewest
int g_iNewest;
int g_iTotal;				// seconds measured this map

// this second so far
double g_flSecondStart;		// 0: not measuring
double g_flLastFrame;
bool g_bHitch;
int g_iUpdates;
int g_iFrames;
double g_flPingSum;
double g_flPingSquares;
int g_iPings;

// this map
Where g_where = NOWHERE;
bool g_bUpdatesCame;			// an update has arrived (the level's name is there while signing on and downloading too)
double g_flInLevelSince;		// 0: not in the level yet
int g_iSet[SETTINGS];			// each one's value when Auto-rate last set or took it over (-1: not yet this map)
bool g_bLeftAlone[SETTINGS];	// changed by someone else: not touched again this map
int g_want[SETTINGS];			// what Auto-rate wants them at
bool g_bStarted;				// the map's first decision made
int g_iUpdateRateMark;			// g_iTotal when cl_updaterate last changed
int g_iServerSends;				// updates a second the server sends when asked for the most (0: not known)
int g_iBackup;					// cl_cmdbackup wanted: up with the loss at once, back to 2 once it has been gone a while
int g_iLevel;					// lower rates against loss wanted: 0 none, 1, 2
int g_iLevelInEffect;			// the level the settings are at
int g_iLevelBefore;				// the one before it took effect
int g_iLevelMark;				// g_iTotal when it took effect
bool g_bTrial;					// the level wanted is a trial: kept only if the loss falls with it
bool g_bTrying;					// the level in effect is that trial
bool g_bRecovery;				// the level wanted is a step back up after the loss was gone a while
bool g_bRecovered;				// the level in effect is that step (a failed trial's undoing isn't)
float g_flLossBefore;			// the loss before the trial
double g_flNoTrialsUntil;
int g_iCleanRun;				// seconds in a row without loss
int g_iCleanNeeded;				// of those before going back up (twice as many each time that brought the loss back)
enum Outcome
{
	OUTCOME_NONE,
	OUTCOME_KEPT,		// the lower rates cut the loss
	OUTCOME_NOT_RATES	// they didn't: back up
} g_outcome;
double g_flJustSetAt;			// when Auto-rate last changed something (0: not this map)
std::string g_justSet;			// what it changed ("ex_interp 0.020, cl_cmdbackup 4")

// the game
double g_flFirstFrame;
bool g_bWatching;	// the switch is looked at (after kStartupWait)
bool g_bWasOn;

int Within( int value, int low, int high )
{
	return value < low ? low : value > high ? high : value;
}

// (the SDK's min / max macros keep std::min / std::max out)
template <typename T>
T Least( T a, T b )
{
	return a < b ? a : b;
}

template <typename T>
T Most( T a, T b )
{
	return a > b ? a : b;
}

bool SwitchedOn()
{
	return cl_autorate && cl_autorate->value != 0.0f;
}

bool On()
{
	return g_bWatching && SwitchedOn();
}

// a setting's value now (ex_interp in msec)
int Current( int setting )
{
	const float value = CVAR_GET_FLOAT( kNames[setting] );
	return (int)floorf( ( setting == INTERP ? value * 1000.0f : value ) + 0.5f );
}

// the second n before the newest (0: the newest)
const Second& Back( int n )
{
	return g_seconds[( g_iNewest - n + kWindow ) % kWindow];
}

int Measured()
{
	return g_iTotal < kWindow ? g_iTotal : kWindow;
}

template <typename T>
float AverageOver( int n, T Second::*field )
{
	n = Least( n, Measured() );
	float sum = 0.0f;
	for ( int i = 0; i < n; i++ )
		sum += (float)( Back( i ).*field );
	return n > 0 ? sum / n : 0.0f;
}

float LossOver( int n )
{
	return AverageOver( n, &Second::loss );
}

// the round trip of the last n seconds, on average, and how much it varied: seconds with no round trip measured (or
// one, for the variation) left out, not counted as 0
float PingOver( int n, bool spread )
{
	n = Least( n, Measured() );
	float sum = 0.0f;
	int used = 0;
	for ( int i = 0; i < n; i++ )
	{
		const Second& second = Back( i );
		if ( second.pings > ( spread ? 1 : 0 ) )
		{
			sum += spread ? second.spread : second.ping;
			used++;
		}
	}
	return used > 0 ? sum / used : 0.0f;
}

// what the network adds to the round trip's variation: the measured spread less what the timing itself explains - the
// latency is measured from when the command the update confirms was sent (CL_ParseClientdata: the newest the server
// had, up to one command's time old, however often updates come), and the update waits up to a frame here before it
// is read; each spread evenly, so by its length / sqrt( 12 )
float JitterOver( int n )
{
	const float spread = PingOver( n, true );
	const float frames = Most( AverageOver( n, &Second::frames ), 1.0f );
	const float commands = Least( frames, (float)Most( Current( CMD_RATE ), 1 ) );
	const float wait = 1000.0f / commands;
	const float frame = 1000.0f / frames;
	const float timing = ( wait * wait + frame * frame ) / 12.0f;
	return sqrtf( Most( spread * spread - timing, 0.0f ) );
}

// of the last n seconds: the value 'part' of the way up (0.5 the middle one; 0.75 a high one, not the highest)
int Among( int n, int Second::*field, float part )
{
	n = Least( n, Measured() );
	if ( n <= 0 )
		return 0;
	int values[kWindow];
	for ( int i = 0; i < n; i++ )
		values[i] = Back( i ).*field;
	std::sort( values, values + n );
	return values[Within( (int)( n * part ), 0, n - 1 )];
}

// updates a second the server sent over the last n seconds (a high second of those arriving, the lost ones added)
int SentOver( int n )
{
	const float arriving = 1.0f - LossOver( n ) / 100.0f;
	return (int)( Among( n, &Second::updates, 0.75f ) / Most( arriving, 0.5f ) + 0.5f );
}

net_status_t NetStatus()
{
	net_status_t status;
	memset( &status, 0, sizeof( status ) );
	gEngfuncs.pNetAPI->Status( &status );
	return status;
}

Where WhereNow()
{
	if ( gEngfuncs.pDemoAPI->IsPlayingback() )
		return DEMO;
	const net_status_t status = NetStatus();
	if ( !status.connected )
		return NOWHERE;
	if ( status.remote_address.type == NA_LOOPBACK )
		return LOCAL;
	const char* level = gEngfuncs.pfnGetLevelName();
	return level && *level ? PLAYING : JOINING;
}

// the player's own settings from before (cl_autorate_saved, one word each); false when none are kept
bool Saved( std::string own[SETTINGS] )
{
	if ( !cl_autorate_saved )
		return false;
	int count = 0;
	const char* c = cl_autorate_saved->string;
	while ( count < SETTINGS )
	{
		while ( *c == ' ' )
			c++;
		if ( !*c )
			break;
		const char* start = c;
		while ( *c && *c != ' ' )
			c++;
		own[count++].assign( start, c - start );
	}
	return count == SETTINGS;
}

// switched on: the player's own settings kept (unless kept already: Auto-rate's own may be in the settings by now)
void SaveOwn()
{
	std::string own[SETTINGS];
	if ( Saved( own ) )
		return;
	std::string text;
	for ( int s = 0; s < SETTINGS; s++ )
	{
		const char* value = CVAR_GET_STRING( kNames[s] );
		if ( s > 0 )
			text += " ";
		text += value && *value && !strchr( value, ' ' ) ? value : "-";	// "-": not known, left as it is when put back
	}
	gEngfuncs.Cvar_Set( (char*)"cl_autorate_saved", (char*)text.c_str() );
}

// switched off: the player's own settings back - not those someone changed since (the player at the console, the
// server), which stay as they are
void PutBackOwn()
{
	std::string own[SETTINGS];
	if ( !Saved( own ) )
		return;
	std::string back;
	for ( int s = 0; s < SETTINGS; s++ )
	{
		if ( own[s] == "-" || g_bLeftAlone[s] )
			continue;
		gEngfuncs.Cvar_Set( (char*)kNames[s], (char*)own[s].c_str() );
		back += std::string( back.empty() ? "" : ", " ) + kNames[s] + " " + own[s];
	}
	gEngfuncs.Cvar_Set( (char*)"cl_autorate_saved", (char*)"" );
	if ( !back.empty() )
		gEngfuncs.Con_Printf( "Auto-rate off, your own settings back: %s\n", back.c_str() );
}

void RestartSecond( double now )
{
	g_flSecondStart = now;
	g_flLastFrame = now;
	g_bHitch = false;
	g_iUpdates = 0;
	g_iFrames = 0;
	g_flPingSum = 0.0;
	g_flPingSquares = 0.0;
	g_iPings = 0;
}

// a second measured: kept unless a frame took too long in it (loading, the game in the background)
void CloseSecond( double now )
{
	const double length = now - g_flSecondStart;
	if ( !g_bHitch && g_iFrames > 0 && length > 0.0 )
	{
		g_iNewest = ( g_iNewest + 1 ) % kWindow;
		Second& second = g_seconds[g_iNewest];
		second.updates = (int)( g_iUpdates / length + 0.5 );
		second.frames = (int)( g_iFrames / length + 0.5 );
		second.loss = NetStatus().packet_loss;
		const double mean = g_iPings > 0 ? g_flPingSum / g_iPings : 0.0;
		const double variance = g_iPings > 1 ? g_flPingSquares / g_iPings - mean * mean : 0.0;
		second.ping = (float)mean;
		second.spread = variance > 0.0 ? (float)sqrt( variance ) : 0.0f;
		second.pings = g_iPings;
		g_iTotal++;
		g_iCleanRun = second.loss == 0 ? g_iCleanRun + 1 : 0;
	}
	RestartSecond( now );
}

void ResetDecisions()
{
	for ( int s = 0; s < SETTINGS; s++ )
	{
		g_iSet[s] = -1;
		g_bLeftAlone[s] = false;
		g_want[s] = 0;
	}
	g_bStarted = false;
	g_iUpdateRateMark = g_iTotal;
	g_iServerSends = 0;
	g_iBackup = 2;
	g_iLevel = 0;
	g_iLevelInEffect = 0;
	g_iLevelBefore = 0;
	g_iLevelMark = g_iTotal;
	g_bTrial = false;
	g_bTrying = false;
	g_bRecovery = false;
	g_bRecovered = false;
	g_flLossBefore = 0.0f;
	g_flNoTrialsUntil = 0.0;
	g_iCleanRun = 0;
	g_iCleanNeeded = kCleanSeconds;
	g_outcome = OUTCOME_NONE;
	g_flJustSetAt = 0.0;
	g_justSet.clear();
}

// a setting changed by someone else since Auto-rate set or took it over: theirs for the rest of the map
void NoticeOthers()
{
	for ( int s = 0; s < SETTINGS; s++ )
	{
		if ( g_iSet[s] >= 0 && !g_bLeftAlone[s] && Current( s ) != g_iSet[s] )
		{
			// ex_interp put up by the engine itself to its least for a lower cl_updaterate (the server's, say):
			// still Auto-rate's to set
			const int least = (int)( 1000.0f / Most( CVAR_GET_FLOAT( "cl_updaterate" ), 10.0f ) );
			if ( s == INTERP && Current( INTERP ) == least )
			{
				g_iSet[s] = least;
				continue;
			}
			g_bLeftAlone[s] = true;
			if ( s == UPDATE_RATE )
				g_iUpdateRateMark = g_iTotal;	// what arrives is measured afresh
			gEngfuncs.Con_DPrintf( "Auto-rate: %s was changed elsewhere, left alone on this map\n", kNames[s] );
		}
	}
}

void Want()
{
	const float factor = g_iLevel == 0 ? 1.0f : g_iLevel == 1 ? 0.75f : 0.5f;
	g_want[RATE] = kTopRate;

	// the most, or while lower rates are tried a share of what the server sends at the most
	int update = kTopUpdateRate;
	if ( g_iLevel > 0 )
	{
		const int base = g_iServerSends > 0 ? Least( g_iServerSends, kTopUpdateRate ) : kTopUpdateRate;
		update = Most( (int)( base * factor + 0.5f ), Least( base, kLowestUpdateRate ) );
	}
	g_want[UPDATE_RATE] = Within( update, 10, kTopUpdateRate );
	g_want[CMD_RATE] = Within( (int)( kTopCmdRate * factor + 0.5f ), kLowestCmdRate, kTopCmdRate );

	// more of the last commands again in each packet with the loss of the last 10 seconds, at once; back to 2 once
	// the last 30 have been clear (not step by step as their average comes down)
	const float loss = LossOver( kRecent );
	const int backup = loss >= kVeryHighLoss ? 8 : loss >= kHighLoss ? 6 : loss >= kSomeLoss ? 4 : 2;
	if ( backup > g_iBackup )
		g_iBackup = backup;
	else if ( LossOver( kWindow ) < 0.5f )
		g_iBackup = 2;
	g_want[CMD_BACKUP] = g_iBackup;

	// the engine's least for the cl_updaterate asked - as wanted, or as the server or console left it (see the top)
	const int updates = Within( g_bLeftAlone[UPDATE_RATE] ? Current( UPDATE_RATE ) : g_want[UPDATE_RATE], 10,
		kTopUpdateRate );
	g_want[INTERP] = Within( 1000 / updates, 1, kTopInterp );	// msec, as CL_ComputeClientInterpolationAmount has it

	g_want[LAG_COMP] = 1;
	g_want[PREDICTION] = 1;
}

bool Differs( int setting )
{
	return g_want[setting] != Current( setting );
}

// a setting as the engine takes it
void ValueText( int setting, int value, char* text, int size )
{
	if ( setting == INTERP )
		snprintf( text, size, "%.3f", value / 1000.0f );
	else
		snprintf( text, size, "%d", value );
}

// the level wanted is what the settings are at now (set with them, or nothing it changes could be changed)
void LevelInEffect()
{
	g_iLevelBefore = g_iLevelInEffect;
	g_iLevelInEffect = g_iLevel;
	g_iLevelMark = g_iTotal;
	g_bTrying = g_bTrial;
	g_bTrial = false;
	g_bRecovered = g_bRecovery;
	g_bRecovery = false;
}

// what is wanted set, at once
void Apply( double now )
{
	NoticeOthers();
	std::string changes;
	for ( int s = 0; s < SETTINGS; s++ )
	{
		if ( g_bLeftAlone[s] || !Differs( s ) )
			continue;
		char value[16];
		ValueText( s, g_want[s], value, sizeof( value ) );
		gEngfuncs.Cvar_Set( (char*)kNames[s], value );
		g_iSet[s] = Current( s );
		if ( s == UPDATE_RATE )
			g_iUpdateRateMark = g_iTotal;
		if ( !changes.empty() )
			changes += ", ";
		changes += std::string( kNames[s] ) + " " + value;
	}
	if ( g_iLevel != g_iLevelInEffect )
		LevelInEffect();	// with its settings, or nothing it changes could be changed: measured as it is
	if ( changes.empty() )
		return;
	g_flJustSetAt = now;
	g_justSet = changes;
	RestartSecond( now );	// this second so far was measured with the settings from before
	gEngfuncs.Con_Printf( "Auto-rate: %s\n", changes.c_str() );
}

// once a second while Auto-rate is on and the level is being measured
void Decide( double now )
{
	NoticeOthers();
	if ( !g_bStarted )
	{
		if ( Measured() < kFirstSeconds )
			return;
		g_bStarted = true;
		for ( int s = 0; s < SETTINGS; s++ )
			g_iSet[s] = Current( s );
	}

	// what the server sends when asked for the most (shown, and what lower rates are a share of): judged while
	// asking for the most once cl_updaterate has stayed the same a while
	const int still = g_iTotal - g_iUpdateRateMark;
	if ( g_iLevelInEffect == 0 && Current( UPDATE_RATE ) >= kTopUpdateRate && still >= kSendsSeconds )
	{
		const int sends = SentOver( Least( still, kWindow ) );
		if ( sends >= 10 )	// (fewer: not sending at all for a while, not a limit)
			g_iServerSends = Least( sends, kTopUpdateRate );
	}

	// loss that stays high: a level lower tried, kept only if the loss falls by a third with it; back up once it has
	// been gone a while - and if that brings it back within a minute, straight down again and twice as long a wait
	// before the next try (a line that can't take the higher rates isn't made to drop packets every two minutes)
	const int since = g_iTotal - g_iLevelMark - kLossLag;	// seconds measured at the level in effect (loss caught up)
	if ( g_iLevel == g_iLevelInEffect )
	{
		if ( g_bTrying )
		{
			if ( since >= kTrialSeconds )
			{
				g_bTrying = false;
				if ( LossOver( kTrialSeconds ) <= g_flLossBefore * ( 2.0f / 3.0f ) )
				{
					g_outcome = OUTCOME_KEPT;
					g_iCleanRun = 0;
				}
				else
				{
					g_iLevel = g_iLevelBefore;
					g_outcome = OUTCOME_NOT_RATES;
					g_flNoTrialsUntil = now + kNoTrialsFor;
				}
			}
		}
		else if ( g_bRecovered && since >= kRecent && since <= kRelapseSeconds && LossOver( kRecent ) >= kHighLoss )
		{
			g_bRecovered = false;
			g_iLevel = g_iLevelBefore;
			g_iCleanNeeded = Least( g_iCleanNeeded * 2, kMostCleanSeconds );
			g_iCleanRun = 0;
			g_outcome = OUTCOME_KEPT;
		}
		else if ( g_iLevel < 2 && now >= g_flNoTrialsUntil && since >= kTrialSeconds
				  && LossOver( kTrialSeconds ) >= kHighLoss )
		{
			g_flLossBefore = LossOver( kTrialSeconds );
			g_iLevel++;
			g_bTrial = true;
			g_outcome = OUTCOME_NONE;
		}
		else if ( g_iLevel > 0 && g_iCleanRun >= g_iCleanNeeded )
		{
			g_iLevel--;	// the loss is gone: back up
			g_bRecovery = true;
			g_iCleanRun = 0;
			g_outcome = OUTCOME_NONE;
		}
	}

	Want();
	Apply( now );
}

std::string LeftAloneList()
{
	std::string list;
	for ( int s = 0; s < SETTINGS; s++ )
	{
		if ( !g_bLeftAlone[s] )
			continue;
		if ( !list.empty() )
			list += ", ";
		list += kNames[s];
	}
	return list;
}

} // namespace

void AutoRate_Init()
{
	cl_autorate = CVAR_CREATE( "cl_autorate", "0", FCVAR_ARCHIVE );
	cl_autorate_saved = CVAR_CREATE( "cl_autorate_saved", "", FCVAR_ARCHIVE );
	ResetDecisions();	// before the first map too (nothing taken over, no settings left alone)
}

void AutoRate_VidInit()
{
	g_bUpdatesCame = false;
	g_flInLevelSince = 0.0;
	g_flSecondStart = 0.0;
	g_iTotal = 0;
	g_iNewest = 0;
	ResetDecisions();
}

void AutoRate_Update()
{
	g_bUpdatesCame = true;
	if ( g_flSecondStart == 0.0 )
		return;
	g_iUpdates++;
	const double ms = NetStatus().latency * 1000.0;
	if ( ms > 0.0 && ms < 5000.0 )
	{
		g_flPingSum += ms;
		g_flPingSquares += ms * ms;
		g_iPings++;
	}
}

int AutoRate_Arriving()
{
	if ( g_where != PLAYING || Measured() < kFirstSeconds )
		return 0;
	return Among( 5, &Second::updates, 0.5f );
}

void AutoRate_Frame()
{
	if ( !cl_autorate )
		return;
	const double now = gEngfuncs.GetAbsoluteTime();
	if ( g_flFirstFrame == 0.0 )
		g_flFirstFrame = now;
	if ( !g_bWatching && now - g_flFirstFrame >= kStartupWait )
	{
		g_bWatching = true;
		g_bWasOn = On();
		if ( g_bWasOn )
			SaveOwn();
		else
			PutBackOwn();	// switched off since the last game (a config, the console): the player's own back if kept
	}
	if ( g_bWatching && On() != g_bWasOn )
	{
		g_bWasOn = On();
		if ( g_bWasOn )
		{
			SaveOwn();
			ResetDecisions();
		}
		else
			PutBackOwn();
	}

	g_where = WhereNow();
	if ( g_where == PLAYING && !g_bUpdatesCame )
		g_where = JOINING;	// signing on or downloading: nothing to measure yet
	if ( g_where != PLAYING )
	{
		g_flInLevelSince = 0.0;
		g_flSecondStart = 0.0;
		return;
	}
	if ( g_flInLevelSince == 0.0 )
		g_flInLevelSince = now;
	if ( now - g_flInLevelSince < kSettle )
		return;
	if ( g_flSecondStart == 0.0 )
	{
		RestartSecond( now );
		return;
	}
	g_iFrames++;
	if ( now - g_flLastFrame > kHitch )
		g_bHitch = true;
	g_flLastFrame = now;
	if ( now - g_flSecondStart >= 1.0 )
	{
		CloseSecond( now );
		if ( On() )
			Decide( now );
	}
}

void AutoRate_Readout( AutoRateReadout& out )
{
	memset( &out, 0, sizeof( out ) );
	out.where = g_where == NOWHERE ? AutoRateReadout::NOT_CONNECTED
				: g_where == LOCAL ? AutoRateReadout::LOCAL_GAME
				: g_where == DEMO  ? AutoRateReadout::DEMO
				: g_where == JOINING ? AutoRateReadout::JOINING
									 : AutoRateReadout::PLAYING;
	out.on = SwitchedOn();
	out.measured = g_where == PLAYING && Measured() > 0;
	if ( out.measured )
	{
		const int n = Least( 5, Measured() );
		out.ping = (int)( PingOver( n, false ) + 0.5f );
		out.jitter = (int)( JitterOver( n ) + 0.5f );
		out.loss = Back( 0 ).loss;
		out.updates = (int)( AverageOver( n, &Second::updates ) + 0.5f );
		out.fps = (int)( AverageOver( n, &Second::frames ) + 0.5f );
		out.history = Measured();
		for ( int i = 0; i < out.history; i++ )
		{
			const Second& second = Back( out.history - 1 - i );	// the oldest first
			out.pingHistory[i] = second.ping;
			out.lossHistory[i] = second.loss;
		}
	}
	out.serverSends = g_where == PLAYING ? g_iServerSends : 0;
	out.rate = Current( RATE );
	out.updateRate = Current( UPDATE_RATE );
	out.cmdRate = Current( CMD_RATE );
	out.cmdBackup = Current( CMD_BACKUP );
	out.interp = CVAR_GET_FLOAT( "ex_interp" );
	out.lagComp = Current( LAG_COMP ) != 0;
	out.prediction = Current( PREDICTION ) != 0;
	for ( int s = 0; s <= INTERP; s++ )
		out.managed[s] = On() && g_where == PLAYING && g_bStarted && !g_bLeftAlone[s];

	// what Auto-rate is doing
	std::string doing;
	AutoRateReadout::Tone tone = AutoRateReadout::TONE_GOOD;
	if ( !out.on )
	{
		tone = AutoRateReadout::TONE_OFF;
		doing = "Off: your own settings are used.";
		if ( !out.lagComp || !out.prediction )
		{
			doing = std::string( "Off. Note: " ) + ( !out.lagComp ? "cl_lc" : "cl_lw" ) + " is 0, so hits register badly.";
			tone = AutoRateReadout::TONE_ATTENTION;
		}
	}
	else if ( !g_bWatching )
	{
		doing = "On: starting...";
		tone = AutoRateReadout::TONE_BUSY;
	}
	else if ( g_where == NOWHERE )
		doing = "ON: Your rate will be automatically adjusted";
	else if ( g_where == LOCAL || g_where == DEMO )
		doing = "On: nothing to tune here.";
	else if ( g_where == JOINING || !g_bStarted )
	{
		doing = "On: measuring this server...";
		tone = AutoRateReadout::TONE_BUSY;
	}
	else if ( g_flJustSetAt != 0.0 && gEngfuncs.GetAbsoluteTime() - g_flJustSetAt < kJustSet )
		doing = "On: just set " + g_justSet + ".";
	else if ( g_bTrying || g_bTrial )
	{
		doing = "On: trying lower rates against the packet loss...";
		tone = AutoRateReadout::TONE_BUSY;
	}
	else if ( !LeftAloneList().empty() )
	{
		doing = "On: tuned; " + LeftAloneList() + " left as the server or console set it.";
		tone = AutoRateReadout::TONE_BUSY;
	}
	else if ( g_iLevel > 0 )	// (a later trial that failed went back to these)
		doing = "On: tuned; lower rates kept, they cut the packet loss.";
	else if ( g_outcome == OUTCOME_NOT_RATES )
	{
		doing = "On: tuned; the packet loss isn't from the rates.";
		tone = AutoRateReadout::TONE_BUSY;
	}
	else if ( g_iServerSends > 0 && g_iServerSends < 90 )
	{
		char line[96];
		snprintf( line, sizeof( line ), "On: tuned; this server sends %d updates a second.", g_iServerSends );
		doing = line;
	}
	else
		doing = "On: tuned for this server.";
	snprintf( out.doing, sizeof( out.doing ), "%s", doing.c_str() );
	out.tone = tone;
}
