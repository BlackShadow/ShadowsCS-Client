// misc_features.h - the Advanced window's Misc tab: the radio switched off, demos recorded (and old ones deleted) on
// their own
#pragma once

void MiscFeatures_Init();		// cvars (HUD_Init)
void MiscFeatures_VidInit();	// a new map (HUD_VidInit): recorded once in the level
void MiscFeatures_Frame();		// HUD_Frame

// cl_disableradio: players' radio - its sounds, its chat lines, the icon over their heads - neither heard nor shown
bool MiscFeatures_RadioOff();

// cl_hideamxx: server plugins' (AMX Mod X's and the like) adverts, repeated notices and info boards not shown, on the
// screen (HUD and director texts) and in chat; their replies to the player, countdowns, first notices, lines naming the
// player and players' chat stay (see misc_features.cpp). Each asks about one thing that has come in:
bool MiscFeatures_HidePluginScreenText( const char* hudTextName );	// HudText's name: __NETMESSAGE__n is a TE_TEXTMESSAGE
bool MiscFeatures_HidePluginDirectorText( const char* text, float holdTime );	// a director text (DRC_CMD_MESSAGE)
bool MiscFeatures_HidePluginPrint( int destination, const char* text );	// TextMsg: a chat or centre line
bool MiscFeatures_HidePluginChat( int sender, const char* format, const char* const* args, int count );	// SayText
// the player sent a chat line or picked a menu item: plugin lines in the next few seconds are its replies, and stay
void MiscFeatures_PlayerAsked();
