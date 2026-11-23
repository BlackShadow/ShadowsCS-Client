// misc_features.cpp - see misc_features.h.
//
// Radio off (cl_disableradio): CS's radio (CBasePlayer::Radio) reaches a player as a SendAudio message from the speaker,
// a TextMsg to HUD_PRINTRADIO (its chat line, "#Game_radio") and a TE_PLAYERATTACHMENT of sprites/radio.spr over the
// speaker's head; each is dropped where the client takes it in (saytext.cpp, text_message.cpp, entity.cpp). The round's
// announcements ("Terrorists win", "Bomb has been planted") come as SendAudio from no one (0) and still play.
//
// Demos (cl_autorecord): on each map, once in the level, "record" with a name like 23112026-cs-cs.warzone-1 - the day,
// "cs", the server (its name, else its address) shortened to 12 characters, and the first number not taken yet - which
// the engine writes to the game folder. A recording started by hand is left alone; ours is stopped and a new one begun
// on the next map, and stopped when the option is switched off.
// Auto-delete (cl_autorecord_delete, days, 30 at first; 0 never): our demos - only names of exactly that form - dated more than that
// many days ago are deleted, at game start and whenever a recording begins.
//
// Hide AMXX texts (cl_hideamxx): server plugins' (AMX Mod X's and the like) adverts, repeated notices and info boards -
// between hiding every plugin text, as at first (replies to the player's own commands, kill info and countdowns went
// too), and only adverts and exact repeats (the user: "a bit less strict", then "a bit more stricter"):
//   - an advert (a link, a site, a server address, Discord or TeamSpeak) goes wherever it comes, centre lines too, and
//     even when it names the player (welcome adverts do);
//   - chat: a line whose shape (the text, numbers aside: "time left: 12:34" and "11:34" are one) came within the last
//     half hour goes - adverts and timers come round every few minutes - unless it names the player; and no plugin
//     line goes within a few seconds of the player sending a chat line or picking a menu item: those are its replies
//     ("/rank", "timeleft", a shop's);
//   - the screen: a text held 5 s or more goes (welcome and info boards); by its shape, one coming again goes, unless
//     it is a countdown (its numbers counting in a run of it, each within 5 s of the last: shown every time), and so
//     does an info panel (a run going on over a minute, or the same text sent again and again for over 3 s);
//   - centre lines otherwise stay.
// Plugins' texts are told from the game's own by how they come:
//   - texts on the screen: set_hudmessage's (TE_TEXTMESSAGE: hw.so's CL_ParseTextMessage keeps it as
//     __NETMESSAGE__<channel> and hands that name to the client's HudText, CL_HudMessage) - maps' game_text come so too -
//     and set_dhudmessage's (a director text, DRC_CMD_MESSAGE; kept when watching through HLTV, whose own they are);
//   - chat and centre lines: client_print's (TextMsg with plain text - every line of the game's is a "#..." name to look
//     up, the server's ClientPrint calls) and coloured adverts (SayText with plain text, where players' chat is
//     "#Cstrike_Chat_..."), except a line showing its sender's name and a colon: chat plugins rewrite players' own lines
//     that way, and those stay.
#include "hud.h"
#include "cl_util.h"
#include "net_api.h"
#include "demo_api.h"
#include "../public/game_controls/iviewport.h"
#include "misc_features.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <map>
#include <string>

namespace
{

cvar_t* cl_disableradio;
cvar_t* cl_hideamxx;
cvar_t* cl_autorecord;
cvar_t* cl_autorecord_delete;

const size_t kServerChars = 12;		// the server's part of a demo name, at most
const double kNameWait = 3.0;		// seconds in the level before recording (the server sends its name on joining)
const double kStartupWait = 5.0;	// seconds after the game starts before auto-delete looks (config.cfg read by then)

bool g_bPending;			// a new map: record once in the level
double g_flInLevelSince;	// when the level was first seen (0: not yet)
bool g_bOurs;				// the recording running is one of ours
double g_flStarted;			// when ours was asked for (the engine starts it on the next frame)
bool g_bCleaned;			// auto-delete ran at game start
double g_flFirstFrame;
bool g_bWasOn;				// cl_autorecord last frame

bool Kept( char c )
{
	return ( c >= 'a' && c <= 'z' ) || ( c >= '0' && c <= '9' ) || c == '.' || c == '-';
}

void TrimEnds( std::string& s )
{
	while ( !s.empty() && strchr( "_.-", s.back() ) )
		s.pop_back();
	while ( !s.empty() && strchr( "_.-", s.front() ) )
		s.erase( 0, 1 );
}

// the server's part of a demo name: lower case; letters, digits, '.' and '-' kept, every run of anything else one '_';
// a web address that is too long loses its ending (cs.warzone.com -> cs.warzone); at most 12 characters, with no '_',
// '.' or '-' at either end
std::string ServerPart( const char* name )
{
	std::string out;
	for ( const char* c = name; *c; c++ )
	{
		const char lower = (char)tolower( (unsigned char)*c );
		if ( Kept( lower ) )
			out += lower;
		else if ( !out.empty() && out.back() != '_' )
			out += '_';
	}
	TrimEnds( out );
	if ( out.size() > kServerChars && out.find( '_' ) == std::string::npos )
	{
		const size_t dot = out.rfind( '.' );
		if ( dot != std::string::npos && dot > 0 )
			out.erase( dot );
	}
	if ( out.size() > kServerChars )
		out.resize( kServerChars );
	TrimEnds( out );
	return out.empty() ? "server" : out;
}

// the server's name (the scoreboard's), else its address
std::string ServerName()
{
	const char* name = gViewPortInterface ? gViewPortInterface->GetServerName() : NULL;
	if ( name && *name )
		return name;
	net_status_t status;
	memset( &status, 0, sizeof( status ) );
	gEngfuncs.pNetAPI->Status( &status );
	if ( status.remote_address.type == NA_LOOPBACK )
		return "local";
	char address[32];
	snprintf( address, sizeof( address ), "%d.%d.%d.%d", status.remote_address.ip[0], status.remote_address.ip[1],
		status.remote_address.ip[2], status.remote_address.ip[3] );
	return address;
}

std::string GamePath( const std::string& file )
{
	return std::string( gEngfuncs.pfnGetGameDirectory() ) + "/" + file;
}

// "DDMMYYYY-cs-<server>-<number>" with the first number whose .dem is not in the game folder yet
std::string DemoName()
{
	const time_t now = time( NULL );
	tm local;
	localtime_s( &local, &now );
	char base[64];
	snprintf( base, sizeof( base ), "%02d%02d%04d-cs-%s-", local.tm_mday, local.tm_mon + 1, local.tm_year + 1900,
		ServerPart( ServerName().c_str() ).c_str() );
	for ( int number = 1; number < 100000; number++ )
	{
		const std::string name = base + std::to_string( number );
		if ( GetFileAttributesA( GamePath( name + ".dem" ).c_str() ) == INVALID_FILE_ATTRIBUTES )
			return name;
	}
	return std::string( base ) + "0";
}

// the date of one of our demos ("DDMMYYYY-cs-<1..12 of a-z 0-9 . _ ->-<number>.dem"); false for any other name
bool OurDemoDate( const char* file, tm& date )
{
	const size_t length = strlen( file );
	if ( length < 8 + 4 + 1 + 2 + 4 )
		return false;
	for ( int i = 0; i < 8; i++ )
	{
		if ( !isdigit( (unsigned char)file[i] ) )
			return false;
	}
	if ( strncmp( file + 8, "-cs-", 4 ) || _stricmp( file + length - 4, ".dem" ) )
		return false;
	const char* server = file + 12;
	const char* end = file + length - 4;
	const char* dash = end;
	while ( dash > server && dash[-1] != '-' )
		dash--;
	if ( dash == end || dash <= server + 1 )
		return false;
	for ( const char* c = dash; c < end; c++ )
	{
		if ( !isdigit( (unsigned char)*c ) )
			return false;
	}
	const size_t serverLength = (size_t)( dash - 1 - server );
	if ( serverLength < 1 || serverLength > kServerChars )
		return false;
	for ( const char* c = server; c < dash - 1; c++ )
	{
		if ( !Kept( *c ) && *c != '_' )
			return false;
	}
	const int day = ( file[0] - '0' ) * 10 + ( file[1] - '0' );
	const int month = ( file[2] - '0' ) * 10 + ( file[3] - '0' );
	const int year = ( file[4] - '0' ) * 1000 + ( file[5] - '0' ) * 100 + ( file[6] - '0' ) * 10 + ( file[7] - '0' );
	if ( day < 1 || day > 31 || month < 1 || month > 12 || year < 2000 || year > 2200 )
		return false;
	memset( &date, 0, sizeof( date ) );
	date.tm_mday = day;
	date.tm_mon = month - 1;
	date.tm_year = year - 1900;
	date.tm_hour = 12;
	date.tm_isdst = -1;
	return true;
}

// our demos dated more than the chosen number of days ago, deleted
void DeleteOldDemos()
{
	const int days = cl_autorecord_delete ? (int)cl_autorecord_delete->value : 0;
	if ( days <= 0 || !cl_autorecord || cl_autorecord->value == 0.0f )
		return;
	const time_t now = time( NULL );
	tm today;
	localtime_s( &today, &now );
	today.tm_hour = 12;
	today.tm_min = today.tm_sec = 0;
	today.tm_isdst = -1;
	const time_t todayNoon = mktime( &today );
	WIN32_FIND_DATAA found;
	const HANDLE search = FindFirstFileA( GamePath( "*-cs-*.dem" ).c_str(), &found );
	if ( search == INVALID_HANDLE_VALUE )
		return;
	int deleted = 0;
	do
	{
		tm date;
		if ( ( found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) || !OurDemoDate( found.cFileName, date ) )
			continue;
		const time_t when = mktime( &date );
		if ( when == (time_t)-1 )
			continue;
		const int age = (int)floor( difftime( todayNoon, when ) / 86400.0 + 0.5 );
		if ( age > days && DeleteFileA( GamePath( found.cFileName ).c_str() ) )
			deleted++;
	}
	while ( FindNextFileA( search, &found ) );
	FindClose( search );
	if ( deleted )
		gEngfuncs.Con_Printf( "Auto-delete: %d demo(s) older than %d days deleted\n", deleted, days );
}

bool Recording()
{
	return gEngfuncs.pDemoAPI->IsRecording() != 0;
}

} // namespace

void MiscFeatures_Init()
{
	cl_disableradio = CVAR_CREATE( "cl_disableradio", "0", FCVAR_ARCHIVE );
	cl_hideamxx = CVAR_CREATE( "cl_hideamxx", "0", FCVAR_ARCHIVE );
	cl_autorecord = CVAR_CREATE( "cl_autorecord", "0", FCVAR_ARCHIVE );
	cl_autorecord_delete = CVAR_CREATE( "cl_autorecord_delete", "30", FCVAR_ARCHIVE );
}

void MiscFeatures_VidInit()
{
	g_bPending = true;
	g_flInLevelSince = 0;
}

bool MiscFeatures_RadioOff()
{
	return cl_disableradio && cl_disableradio->value != 0.0f;
}

static bool HideAmxx()
{
	return cl_hideamxx && cl_hideamxx->value != 0.0f;
}

// a text as compared: colour codes and other control bytes out, lower case, runs of spaces one
static std::string Plain( const std::string& text )
{
	std::string out;
	for ( unsigned char c : text )
	{
		if ( c < ' ' )
			c = ' ';
		if ( c == ' ' && ( out.empty() || out.back() == ' ' ) )
			continue;
		out += (char)tolower( c );
	}
	while ( !out.empty() && out.back() == ' ' )
		out.pop_back();
	return out;
}

// a link, a site ("warzone.com", "site.ro/vip"), a server address ("1.2.3.4:27015"), Discord or TeamSpeak
static bool Advertises( const std::string& line )
{
	static const char* const kMarks[] = { "www.", "http", "://", "discord", "teamspeak", "ts3.", "facebook.", "fb.com",
		"instagram", "youtube." };
	for ( const char* mark : kMarks )
	{
		if ( line.find( mark ) != std::string::npos )
			return true;
	}
	static const char* const kEndings[] = { "com", "net", "org", "info", "eu", "ro", "ru", "de", "pl", "tr", "br", "gg",
		"us", "uk", "io", "me", "lt", "lv", "ua", "bg", "cz", "sk", "hu", "rs", "nl", "fr", "es", "it", "pt", "ar", "xyz" };
	for ( size_t dot = line.find( '.' ); dot != std::string::npos; dot = line.find( '.', dot + 1 ) )
	{
		if ( dot == 0 || !isalnum( (unsigned char)line[dot - 1] ) )
			continue;
		// four numbers with dots: a server's address
		int dots = 0, i = (int)dot;
		while ( i > 0 && ( isdigit( (unsigned char)line[i - 1] ) || line[i - 1] == '.' ) )
			i--;
		for ( size_t j = (size_t)i; j < line.size() && ( isdigit( (unsigned char)line[j] ) || line[j] == '.' ); j++ )
			dots += line[j] == '.';
		if ( dots == 3 )
			return true;
		// "name.ending" followed by the end, a space, '/' or ':'
		size_t end = dot + 1;
		while ( end < line.size() && isalpha( (unsigned char)line[end] ) )
			end++;
		const std::string ending = line.substr( dot + 1, end - dot - 1 );
		if ( end < line.size() && !strchr( " /:", line[end] ) )
			continue;
		for ( const char* known : kEndings )
		{
			if ( ending == known )
				return true;
		}
	}
	return false;
}

const double kRepeatWindow = 1800.0;	// a text's shape again within this long (seconds): a repeat
const double kReplyWindow = 3.0;		// plugin lines this soon after the player asked something: its replies
const float kBoardHold = 5.0f;			// a screen text held this long: a board
const double kRunGap = 5.0;				// a screen text's shape again within this long: the same run
const double kTickerRun = 60.0;			// a run going on longer: an info panel ticking along (a C4 countdown is 45 s)
const double kStandStill = 3.0;			// a run's text the same this long, sent again and again: a standing panel
const int kStandResends = 3;			// (that many times at least)

static double g_flPlayerAsked = -1000.0;	// when the player last sent a chat line or picked a menu item

// a text's shape: each run of digits one '#' - "time left: #:#" for "time left: 12:34" and "11:34"
static std::string Shape( const std::string& line )
{
	std::string out;
	for ( char c : line )
	{
		if ( !isdigit( (unsigned char)c ) )
			out += c;
		else if ( out.empty() || out.back() != '#' )
			out += '#';
	}
	return out;
}

// the player's own name, as compared ("" when not known)
static std::string LocalName()
{
	cl_entity_t* local = gEngfuncs.GetLocalPlayer();
	if ( !local || local->index < 1 || local->index > MAX_PLAYERS || !g_PlayerInfoList[local->index].name )
		return "";
	return Plain( g_PlayerInfoList[local->index].name );
}

// a plugin line now is a reply to what the player asked
static bool Replying()
{
	return gEngfuncs.GetAbsoluteTime() - g_flPlayerAsked < kReplyWindow;
}

// a plugin's chat line to hide (see the top)
static bool HideChatLine( const std::string& text )
{
	const std::string line = Plain( text );
	if ( line.empty() )
		return false;
	// its shape again within half an hour (kept fresh while it keeps coming; a reply counts as having come, so the
	// same line coming later unasked - "timeleft" asked by others - is a repeat)
	static std::map<std::string, double> seen;
	const double now = gEngfuncs.GetAbsoluteTime();
	if ( seen.size() > 1024 )
		seen.clear();
	const std::string shape = Shape( line );
	auto it = seen.find( shape );
	const bool again = it != seen.end() && now - it->second < kRepeatWindow;
	seen[shape] = now;
	if ( Replying() )
		return false;	// a reply to what the player asked
	if ( Advertises( line ) )
		return true;
	const std::string me = LocalName();
	if ( me.size() >= 2 && line.find( me ) != std::string::npos )
		return false;	// about the player
	return again;
}

// a plugin's screen text to hide (see the top)
static bool HideScreenText( const std::string& text, float holdTime )
{
	const std::string line = Plain( text );
	if ( line.empty() )
		return false;
	if ( Advertises( line ) || holdTime >= kBoardHold )
		return true;
	// how its shape has come before
	struct Seen
	{
		double runStart;	// when its current run began (each again within kRunGap: the same run)
		double last;		// when it last came
		double changed;		// when the run's text last changed
		int resends;		// the same text since
		std::string text;	// the text it last came with
		bool counts;		// its numbers have counted in a run: a countdown, shown every time
		bool panel;			// it has ticked or stood too long: an info panel, hidden
		bool hidden;		// the current run is hidden
	};
	static std::map<std::string, Seen> seen;
	const double now = gEngfuncs.GetAbsoluteTime();
	if ( seen.size() > 1024 )
		seen.clear();
	const std::string shape = Shape( line );
	auto it = seen.find( shape );
	if ( it == seen.end() || now - it->second.last >= kRepeatWindow )
	{
		seen[shape] = { now, now, now, 0, line, false, false, false };	// new: shown
		return false;
	}
	Seen& s = it->second;
	if ( now - s.last > kRunGap )
	{
		// a new run: a countdown again is shown, a notice again or a panel isn't
		s.runStart = s.changed = now;
		s.resends = 0;
		s.hidden = s.panel || !s.counts;
	}
	else if ( line != s.text )
	{
		s.changed = now;
		s.resends = 0;
		if ( !s.panel )
		{
			s.counts = true;	// its numbers counting: a countdown (until it goes on too long)
			s.hidden = false;
		}
	}
	else
		s.resends++;
	s.last = now;
	s.text = line;
	if ( now - s.runStart > kTickerRun || ( s.resends >= kStandResends && now - s.changed > kStandStill ) )
	{
		s.panel = true;
		s.counts = false;
		s.hidden = true;
	}
	return s.hidden;
}

bool MiscFeatures_HidePluginScreenText( const char* hudTextName )
{
	if ( !HideAmxx() || !hudTextName || strncmp( hudTextName, "__NETMESSAGE__", 14 ) )
		return false;
	const client_textmessage_t* message = gEngfuncs.pfnTextMessageGet( hudTextName );
	return message && message->pMessage && HideScreenText( message->pMessage, message->holdtime );
}

bool MiscFeatures_HidePluginDirectorText( const char* text, float holdTime )
{
	return HideAmxx() && !gEngfuncs.IsSpectateOnly() && text && HideScreenText( text, holdTime );
}

bool MiscFeatures_HidePluginPrint( int destination, const char* text )
{
	if ( !HideAmxx() || !text || text[0] == '#' )
		return false;	// off, or the game's own
	if ( destination == HUD_PRINTCENTER )
		return !Replying() && Advertises( Plain( text ) );	// centre lines: only adverts go
	return destination == HUD_PRINTTALK && HideChatLine( text );
}

void MiscFeatures_PlayerAsked()
{
	g_flPlayerAsked = gEngfuncs.GetAbsoluteTime();
}

bool MiscFeatures_HidePluginChat( int sender, const char* format, const char* const* args, int count )
{
	if ( !HideAmxx() || !format || format[0] == '#' )
		return false;
	// the line as shown (colour codes out), to look for "<sender's name> :" in
	std::string line;
	auto add = [&line]( const char* text ) {
		for ( const char* c = text; c && *c; c++ )
		{
			if ( (unsigned char)*c >= ' ' )
				line += *c;
		}
		line += ' ';
	};
	add( format );
	for ( int i = 0; i < count; i++ )
		add( args[i] );
	const char* name = sender >= 1 && sender <= MAX_PLAYERS ? g_PlayerInfoList[sender].name : NULL;
	if ( name && *name )
	{
		for ( size_t at = line.find( name ); at != std::string::npos; at = line.find( name, at + 1 ) )
		{
			size_t after = at + strlen( name );
			while ( after < line.size() && line[after] == ' ' )
				after++;
			if ( after < line.size() && line[after] == ':' )
				return false;	// a player's line, as a chat plugin writes it
		}
	}
	return HideChatLine( line );
}

void MiscFeatures_Frame()
{
	if ( !cl_autorecord )
		return;
	const double now = gEngfuncs.GetAbsoluteTime();
	if ( g_flFirstFrame == 0 )
		g_flFirstFrame = now;
	if ( !g_bCleaned && now - g_flFirstFrame >= kStartupWait )
	{
		g_bCleaned = true;
		DeleteOldDemos();
	}

	const bool on = cl_autorecord->value != 0.0f;
	if ( on != g_bWasOn )
	{
		g_bWasOn = on;
		if ( on )
		{
			g_bPending = true;	// switched on while playing: this map too
			g_flInLevelSince = 0;
		}
		else if ( g_bOurs && Recording() )
		{
			gEngfuncs.pfnClientCmd( (char*)"stop\n" );
			g_bOurs = false;
		}
	}
	if ( g_bOurs && !Recording() && now - g_flStarted > 2.0 )
		g_bOurs = false;	// stopped: by the engine (disconnect) or by hand
	if ( !on || !g_bPending )
		return;
	if ( gEngfuncs.pDemoAPI->IsPlayingback() )
	{
		g_bPending = false;
		return;
	}
	net_status_t status;
	memset( &status, 0, sizeof( status ) );
	gEngfuncs.pNetAPI->Status( &status );
	const char* level = gEngfuncs.pfnGetLevelName();
	if ( !status.connected || !level || !*level )
	{
		g_flInLevelSince = 0;
		return;
	}
	if ( g_flInLevelSince == 0 )
	{
		g_flInLevelSince = now;
		return;
	}
	if ( now - g_flInLevelSince < kNameWait )
		return;
	g_bPending = false;
	const bool recording = Recording();
	if ( recording && !g_bOurs )
		return;	// recording by hand: left alone
	DeleteOldDemos();
	const std::string name = DemoName();
	const std::string command = std::string( recording ? "stop\n" : "" ) + "record " + name + "\n";
	gEngfuncs.pfnClientCmd( (char*)command.c_str() );
	g_bOurs = true;
	g_flStarted = now;
	gEngfuncs.Con_Printf( "Auto-record: %s.dem\n", name.c_str() );
}
