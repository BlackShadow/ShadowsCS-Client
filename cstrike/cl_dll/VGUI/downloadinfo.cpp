// downloadinfo.cpp - how many of a server's files have downloaded, while joining it.
//
// The loading box's status line ("Verifying and downloading resources...") reads instead:
//   Downloaded 44 of 128 files
// The counts come from engine_download.cpp. (No sizes: for a web download the engine drops its file list, sizes
// included, before the files come; and through the game connection servers often send a dummy size.)
//
// A web download (FastDL) is one round of a longer join (hw.so): CL_BatchResourceRequest queues the missing files,
// disconnects and starts a loading bar of one step per file; when the last one is in, the engine reconnects ("retry",
// DownloadManager::CheckActiveDownload) - a new loading bar, from the start - and the files the web address didn't
// have then come through the game connection. That second round carries on the first one's count ("Downloaded 125
// of 128", not "0 of 3"), and the line goes back to the engine's own text the moment a round is over (checked every
// frame while one runs), so the count is never left standing over the reconnect.
//
// The text goes through the game menu's own IGameUI::SetProgressBarStatusText (see engine_download.cpp): nothing of
// ours is put inside the menu's panels. An earlier version placed a panel inside the loading box, and the menu
// crashed walking the box's children (it takes every child for one of its own panels; ours came back as nothing).
//
// Faster downloads, as far as the client can make them faster:
//   - From the game server the engine gets one piece of a file (about 1 KB) per round trip, and never more bytes a
//     second than the "rate" setting allows. While such a download runs, "rate" goes up to the most the engine
//     accepts (100000) and is put back to the old value afterwards. The server's own sv_maxrate and the ping still
//     limit it; a server with a web download (FastDL) is always much faster.
//   - From a web download the engine fetches 5 files at a time; here 8.
// hud_download_info 0: none of this (engine behaviour again).
// What was found and shown is written to cstrike/download_info.log (started fresh each game start).
#include "hud.h"
#include "cl_util.h"
#include "vgui_controls/Controls.h"
#include "vgui/ISurface.h"
#include "vgui/IPanel.h"
#include "../engine_download.h"
#include "downloadinfo.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

class IGameUI;
extern IGameUI* g_pGameUI;	// cdll_int.cpp, from GameUI.dll ("GameUI007"), the same object the engine uses

namespace
{

const int kWebParallel = 8;
const double kPollInterval = 0.1;	// looking for a download to start; once one runs: every frame
const double kCarryOnFor = 30.0;	// a game-connection round this soon after a web round carries on its count
const char kFastRate[] = "100000";	// the engine's highest rate (MAXIMUM_CLIENT_RATE)

cvar_t* hud_download_info;

struct Session
{
	bool active;
	bool web;
	bool logged;
	int totalFiles;
};
Session g_session;
// the web round that ended last: the reconnect after it fetches through the game connection the files the web
// address didn't have, which that round counted already
bool g_bWebEnded;
int g_iWebTotal;
double g_flWebEndedAt;
double g_flNextPoll;
bool g_bRateRaised;
char g_szRateBefore[32];
bool g_bMissingLogged;

void Log( bool fresh, const char* format, ... )
{
	char path[256];
	_snprintf( path, sizeof( path ) - 1, "%s/download_info.log", gEngfuncs.pfnGetGameDirectory() );
	path[sizeof( path ) - 1] = 0;
	FILE* file = fopen( path, fresh ? "w" : "a" );
	if ( !file )
		return;
	va_list arguments;
	va_start( arguments, format );
	vfprintf( file, format, arguments );
	va_end( arguments );
	fputc( '\n', file );
	fclose( file );
}

// "rate" up while a download comes through the game connection, back afterwards
void RaiseRate()
{
	if ( g_bRateRaised )
		return;
	cvar_t* rate = gEngfuncs.pfnGetCvarPointer( "rate" );
	if ( !rate || !rate->string || rate->value >= 100000.0f )
		return;
	strncpy( g_szRateBefore, rate->string, sizeof( g_szRateBefore ) - 1 );
	g_szRateBefore[sizeof( g_szRateBefore ) - 1] = 0;
	gEngfuncs.Cvar_Set( (char*)"rate", (char*)kFastRate );
	g_bRateRaised = true;
}

void RestoreRate()
{
	if ( !g_bRateRaised )
		return;
	g_bRateRaised = false;
	cvar_t* rate = gEngfuncs.pfnGetCvarPointer( "rate" );
	if ( rate && rate->string && !strcmp( rate->string, kFastRate ) )	// not changed by hand meanwhile
		gEngfuncs.Cvar_Set( (char*)"rate", g_szRateBefore );
}

// a panel named 'name' that is shown itself, at or below 'panel' (read only: names and flags, nothing else)
vgui2::VPANEL FindShown( vgui2::VPANEL panel, const char* name, int depth )
{
	const char* panelName = vgui2::ipanel()->GetName( panel );
	if ( panelName && !strcmp( panelName, name ) && vgui2::ipanel()->IsVisible( panel ) )
		return panel;
	if ( depth <= 0 )
		return 0;
	const int count = vgui2::ipanel()->GetChildCount( panel );
	for ( int i = 0; i < count; i++ )
	{
		const vgui2::VPANEL child = vgui2::ipanel()->GetChild( panel, i );
		const vgui2::VPANEL found = child ? FindShown( child, name, depth - 1 ) : 0;
		if ( found )
			return found;
	}
	return 0;
}

// is the engine's loading box ("LoadingDialog", GameUI/LoadingDialog.cpp) on screen? (downloads while playing, such
// as other players' sprays, have none and are left alone)
bool LoadingBoxShown()
{
	const vgui2::VPANEL root = vgui2::surface()->GetEmbeddedPanel();
	if ( !root )
		return false;
	const int count = vgui2::ipanel()->GetChildCount( root );
	for ( int i = 0; i < count; i++ )
	{
		const vgui2::VPANEL child = vgui2::ipanel()->GetChild( root, i );
		const char* name = child ? vgui2::ipanel()->GetName( child ) : NULL;
		if ( !child || ( name && !strcmp( name, "BaseClientPanel" ) ) )
			continue;	// the game's own panels (HUD, menus): never the loading box, and many to search
		if ( FindShown( child, "LoadingDialog", 10 ) )
			return true;
	}
	return false;
}

void EndSession()
{
	EngineDownload_SetStatusText( g_pGameUI, NULL );	// the status line belongs to the engine again
	RestoreRate();
	g_session.active = false;
}

} // namespace

void DownloadInfo_Init()
{
	hud_download_info = gEngfuncs.pfnRegisterVariable( (char*)"hud_download_info", (char*)"1", FCVAR_ARCHIVE );
}

void DownloadInfo_Frame()
{
	if ( !hud_download_info )
		return;
	static bool s_bStartLogged;
	if ( !s_bStartLogged )
	{
		s_bStartLogged = true;
		char engine[256];
		EngineDownload_Describe( engine, sizeof( engine ) );
		Log( true, "Engine: %s", engine );
	}
	const bool enabled = hud_download_info->value != 0.0f;
	EngineDownload_SetWebParallel( enabled ? kWebParallel : 0 );
	if ( !enabled )
	{
		if ( g_session.active )
			EndSession();
		return;
	}
	const double now = gEngfuncs.GetAbsoluteTime();
	if ( !g_session.active && now < g_flNextPoll && g_flNextPoll - now <= kPollInterval )
		return;
	g_flNextPoll = now + kPollInterval;

	EngineDownloadStatus status;
	if ( !EngineDownload_Read( status ) )
	{
		if ( g_session.active )
		{
			if ( g_session.web )
			{
				g_bWebEnded = true;
				g_iWebTotal = g_session.totalFiles;
				g_flWebEndedAt = now;
			}
			EndSession();
		}
		g_bMissingLogged = false;
		return;
	}
	if ( !LoadingBoxShown() )
	{
		if ( !g_bMissingLogged )
		{
			g_bMissingLogged = true;
			Log( false, "Downloading (%s, %d files left) with no loading box on screen.", status.web ? "web" : "game server",
				status.filesLeft );
		}
		if ( g_session.active )
			EndSession();
		return;
	}
	if ( !g_session.active || g_session.web != status.web )
	{
		// the game connection right after a web round: the files the web address didn't have, part of its count
		const bool carryOn = !status.web
							 && ( ( g_session.active && g_session.web )
								  || ( g_bWebEnded && now - g_flWebEndedAt < kCarryOnFor ) );
		const int webTotal = g_session.active && g_session.web ? g_session.totalFiles : g_iWebTotal;
		EndSession();
		memset( &g_session, 0, sizeof( g_session ) );
		g_session.active = true;
		g_session.web = status.web;
		if ( carryOn && status.filesLeft <= webTotal )
			g_session.totalFiles = webTotal;
		g_bWebEnded = false;
	}
	Session& s = g_session;
	// the total: the engine's count for a web round, else the most files seen left (the first look comes before any
	// file has arrived)
	if ( status.webFilesTotal > s.totalFiles )
		s.totalFiles = status.webFilesTotal;
	if ( status.filesLeft > s.totalFiles )
		s.totalFiles = status.filesLeft;
	if ( status.web )
		RestoreRate();
	else
		RaiseRate();

	int downloaded = s.totalFiles - status.filesLeft;
	if ( downloaded < 0 )
		downloaded = 0;
	char text[96];
	_snprintf( text, sizeof( text ) - 1, "Downloaded %d of %d files", downloaded, s.totalFiles );
	text[sizeof( text ) - 1] = 0;
	EngineDownload_SetStatusText( g_pGameUI, text );

	if ( !s.logged )
	{
		s.logged = true;
		Log( false, "Downloading from %s: %d of %d files left, current '%s'.", status.web ? "the web" : "the game server",
			status.filesLeft, s.totalFiles, status.currentFile );
	}
}

void DownloadInfo_Shutdown()
{
	RestoreRate();
	EngineDownload_Unhook();
	EngineDownload_SetWebParallel( 0 );
}
