// auto_rate.h - the Advanced window's Auto-rate (Misc tab): the network settings tuned to the server being played on,
// and kept tuned while playing
#pragma once

void AutoRate_Init();		// cvars (HUD_Init)
void AutoRate_VidInit();	// a new map (HUD_VidInit): the server is measured again
void AutoRate_Frame();		// HUD_Frame
void AutoRate_Update();		// an update from the server arrived (HUD_TxferPredictionData)
int AutoRate_Arriving();	// updates a second arriving from the server lately (0: not known)

// what the Misc tab's Auto-rate panel shows (VGUI/autorate_panel.cpp)
struct AutoRateReadout
{
	enum Where
	{
		NOT_CONNECTED,
		LOCAL_GAME,	// a game on this computer: nothing to measure
		DEMO,
		JOINING,
		PLAYING
	};
	enum Tone	// the square before what Auto-rate is doing
	{
		TONE_OFF,		// switched off (an empty square)
		TONE_GOOD,		// on, tuned
		TONE_BUSY,		// measuring, trying something, or something it can't change
		TONE_ATTENTION	// a setting that hurts hit registration
	};
	static const int kHistory = 30;	// seconds of the graph

	Where where;
	bool on;			// switched on
	bool measured;		// playing on a server, with at least a second measured
	int ping;			// the last few seconds (ms)
	int jitter;			// what the network adds to the round trip's variation (ms)
	int loss;			// percent
	int updates;		// arriving a second
	int fps;
	int serverSends;	// updates a second the server sends when asked for the most (0: not known yet)
	int history;		// seconds in the graph, the oldest first
	float pingHistory[kHistory];
	int lossHistory[kHistory];
	int rate, updateRate, cmdRate, cmdBackup;	// the settings now
	float interp;		// ex_interp, seconds
	bool lagComp, prediction;	// cl_lc, cl_lw on
	bool managed[5];	// rate, cl_updaterate, cl_cmdrate, cl_cmdbackup, ex_interp: Auto-rate sets it now
	char doing[160];	// what Auto-rate is doing, one line
	Tone tone;
};
void AutoRate_Readout( AutoRateReadout& out );
