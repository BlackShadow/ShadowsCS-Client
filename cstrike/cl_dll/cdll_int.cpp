/***
*
*	Copyright (c) 1999, Valve LLC. All rights reserved.
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
//
//  cdll_int.c
//
// this implementation handles the linking of the engine to the DLL
//

#include <string.h>
#include "../../public/interface.h"
#include "hud.h"
#include "cl_util.h"
#include "../../cl_dll/Exports.h"
#include "particleman.h"
#include "tri.h"
#include "cl_dll/IGameClientExports.h"
#include "../public/game_controls/iviewport.h"
#include "../game_shared/voice_status.h"

extern "C"
{
#include "pm_shared.h"
#include "pm_defs.h"
}
#include "vismods.h"

#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"cstrike\\cl_dll\\cdll_int.cpp" )

#include "VGUI/chatwindow.h"
#include "VGUI/downloadinfo.h"
#include "VGUI/menu_background.h"
#include "stretch_view.h"
#include "misc_features.h"
#include "auto_rate.h"
#include "hit_positions.h"
#include "author_avatar.h"
#include "perf_probe.h"
#ifdef SHADOWS_PERF_PROBE
#include <algorithm>
#include "r_studioint.h"

extern engine_studio_api_t IEngineStudio;

namespace
{
const char* const g_perfNames[PERF_BUCKETS] =
{
	"HUD_Redraw", "UpdateClientData", "CreateMove", "CalcRefdef", "AddEntity", "CreateEntities", "TempEntUpdate",
	"Triangles", "PostRunCmd", "StudioEvent", "HUD_Frame", "StudioDrawModel", "StudioDrawPlayer",
	"  engine DrawPoints", "  engine lighting", "  engine CheckBBox", "  VisMods_Frame", "other"
};
long long g_perfTotal[PERF_BUCKETS];
long long g_perfCalls[PERF_BUCKETS];
struct HudStat
{
	const void* element;
	const char* name;
	long long total;
};
HudStat g_hudStats[64];
int g_hudCount;
long long g_bonePoses, g_bonesChecked, g_bonesDifferent, g_boneOldTime, g_boneNewTime;
long long g_lastFrame, g_windowStart, g_frameTotal, g_frameWorst;
int g_frames;

bool IsNested( int bucket )
{
	return bucket == PERF_ENG_DRAWPOINTS || bucket == PERF_ENG_LIGHTING || bucket == PERF_ENG_CHECKBBOX
		|| bucket == PERF_VISMODS;
}

void PerfProbe_Report( long long window )
{
	if ( !g_frames )
		return;
	const double frames = g_frames;
	const double frameMs = g_frameTotal / frames / 1e6;
	double clientMs = 0;
	for ( int i = 0; i < PERF_BUCKETS; i++ )
	{
		if ( !IsNested( i ) )
			clientMs += g_perfTotal[i] / frames / 1e6;
	}
	gEngfuncs.Con_Printf( "[perf] %.1fs %d frames %.1f fps  frame %.3f ms (worst %.2f)  client.dll %.3f ms = %.1f%%\n",
		window / 1e9, g_frames, frames * 1e9 / window, frameMs, g_frameWorst / 1e6, clientMs,
		frameMs > 0 ? clientMs / frameMs * 100 : 0 );
	for ( int i = 0; i < PERF_BUCKETS; i++ )
	{
		if ( !g_perfCalls[i] )
			continue;
		const double ms = g_perfTotal[i] / frames / 1e6;
		gEngfuncs.Con_Printf( "[perf]   %-20s %7.3f ms %5.1f%%  %6.1f calls/frame\n", g_perfNames[i], ms,
			frameMs > 0 ? ms / frameMs * 100 : 0, g_perfCalls[i] / frames );
	}
	const double studioOwn = ( g_perfTotal[PERF_STUDIO_MODEL] + g_perfTotal[PERF_STUDIO_PLAYER]
		- g_perfTotal[PERF_ENG_DRAWPOINTS] - g_perfTotal[PERF_ENG_LIGHTING] - g_perfTotal[PERF_ENG_CHECKBBOX] )
		/ frames / 1e6;
	gEngfuncs.Con_Printf( "[perf]   studio, client's own work %.3f ms\n", studioOwn );
	gEngfuncs.Con_Printf( "[perf]   bone check (since start): %lld player poses, %lld bones, %lld different\n",
		g_bonePoses, g_bonesChecked, g_bonesDifferent );
	if ( g_bonePoses )
		gEngfuncs.Con_Printf( "[perf]   player pose time per player: old way %.2f us, new way %.2f us\n",
			g_boneOldTime / 1000.0 / g_bonePoses, g_boneNewTime / 1000.0 / g_bonePoses );
	std::sort( g_hudStats, g_hudStats + g_hudCount,
		[]( const HudStat& a, const HudStat& b ) { return a.total > b.total; } );
	for ( int i = 0; i < g_hudCount && i < 10; i++ )
	{
		gEngfuncs.Con_Printf( "[perf]     hud %-32s %7.3f ms\n", g_hudStats[i].name, g_hudStats[i].total / frames / 1e6 );
	}
	memset( g_perfTotal, 0, sizeof( g_perfTotal ) );
	memset( g_perfCalls, 0, sizeof( g_perfCalls ) );
	for ( int i = 0; i < g_hudCount; i++ )
		g_hudStats[i].total = 0;
	g_frameTotal = 0;
	g_frameWorst = 0;
	g_frames = 0;
}

void PerfProbe_FrameMark()
{
	const long long now = PerfNow();
	if ( g_lastFrame )
	{
		const long long frame = now - g_lastFrame;
		g_frameTotal += frame;
		g_frameWorst = std::max( g_frameWorst, frame );
		g_frames++;
	}
	g_lastFrame = now;
	if ( !g_windowStart )
		g_windowStart = now;
	if ( now - g_windowStart >= 5000000000LL )
	{
		PerfProbe_Report( now - g_windowStart );
		g_windowStart = now;
	}
}

template <int B, auto Fn, typename Signature> struct PerfWrapImpl;
template <int B, auto Fn, typename R, typename... A> struct PerfWrapImpl<B, Fn, R ( * )( A... )>
{
	static R Call( A... args )
	{
		PerfScope scope( B );
		return Fn( args... );
	}
};

r_studio_interface_t* g_realStudio;
r_studio_interface_t g_perfStudio;
engine_studio_api_t g_realEngineStudio;

int PerfStudioDrawModel( int flags )
{
	PerfScope scope( PERF_STUDIO_MODEL );
	return g_realStudio->StudioDrawModel( flags );
}
int PerfStudioDrawPlayer( int flags, struct entity_state_s* player )
{
	PerfScope scope( PERF_STUDIO_PLAYER );
	return g_realStudio->StudioDrawPlayer( flags, player );
}
void PerfDrawPoints()
{
	PerfScope scope( PERF_ENG_DRAWPOINTS );
	g_realEngineStudio.StudioDrawPoints();
}
void PerfDynamicLight( struct cl_entity_s* entity, struct alight_s* light )
{
	PerfScope scope( PERF_ENG_LIGHTING );
	g_realEngineStudio.StudioDynamicLight( entity, light );
}
void PerfEntityLight( struct alight_s* light )
{
	PerfScope scope( PERF_ENG_LIGHTING );
	g_realEngineStudio.StudioEntityLight( light );
}
void PerfSetupLighting( struct alight_s* light )
{
	PerfScope scope( PERF_ENG_LIGHTING );
	g_realEngineStudio.StudioSetupLighting( light );
}
int PerfCheckBBox()
{
	PerfScope scope( PERF_ENG_CHECKBBOX );
	return g_realEngineStudio.StudioCheckBBox();
}
}

void PerfProbe_Add( int bucket, long long nanoseconds )
{
	g_perfTotal[bucket] += nanoseconds;
	g_perfCalls[bucket]++;
}

void PerfProbe_BoneCheck( int bones, int different )
{
	g_bonePoses++;
	g_bonesChecked += bones;
	g_bonesDifferent += different;
}

void PerfProbe_BoneTime( long long oldNanoseconds, long long newNanoseconds )
{
	g_boneOldTime += oldNanoseconds;
	g_boneNewTime += newNanoseconds;
}

void PerfProbe_AddHud( const void* element, const char* name, long long nanoseconds )
{
	for ( int i = 0; i < g_hudCount; i++ )
	{
		if ( g_hudStats[i].element == element )
		{
			g_hudStats[i].total += nanoseconds;
			return;
		}
	}
	if ( g_hudCount < (int)ARRAYSIZE( g_hudStats ) )
	{
		HudStat& stat = g_hudStats[g_hudCount++];
		stat.element = element;
		stat.name = name;
		stat.total = nanoseconds;
	}
}

static int PerfGetStudioModelInterface( int version, r_studio_interface_t** renderer, engine_studio_api_t* engine )
{
	const int result = HUD_GetStudioModelInterface( version, renderer, engine );
	if ( result && *renderer )
	{
		g_realStudio = *renderer;
		g_perfStudio = **renderer;
		g_perfStudio.StudioDrawModel = PerfStudioDrawModel;
		g_perfStudio.StudioDrawPlayer = PerfStudioDrawPlayer;
		*renderer = &g_perfStudio;
		g_realEngineStudio = IEngineStudio;
		IEngineStudio.StudioDrawPoints = PerfDrawPoints;
		IEngineStudio.StudioDynamicLight = PerfDynamicLight;
		IEngineStudio.StudioEntityLight = PerfEntityLight;
		IEngineStudio.StudioSetupLighting = PerfSetupLighting;
		IEngineStudio.StudioCheckBBox = PerfCheckBBox;
	}
	return result;
}

#define PERF_WRAP( bucket, fn ) PerfWrapImpl<bucket, fn, decltype( &fn )>::Call
#else
#define PERF_WRAP( bucket, fn ) fn
#endif

class IFileSystem;
class ICareerUI;
class IGameUI;
class IGameConsole;
class IMusicManager;
class IBaseUI;
class IFriendsUser;

cl_enginefunc_t gEngfuncs;
CHud gHUD;
modfuncs_t g_modfuncs;
cldll_func_dst_t* g_pcldstAddrs;
CSysModule* g_hFileSystemModule;
IFileSystem* g_pFileSystem;
CSysModule* g_hGameUIModule;
ICareerUI* g_pCareerUI;
IGameUI* g_pGameUI;
IGameConsole* g_pGameConsole;
IMusicManager* g_pMusicManager;
IBaseUI* g_pBaseUI;
CSysModule* g_hTrackerModule;
IFriendsUser* g_pFriendsUser;
CSysModule* g_hParticleManModule;
IParticleMan* g_pParticleMan;

char* sPlayerModelFiles[] =
{
	"models/player.mdl",
	"models/player/leet/leet.mdl",
	"models/player/gign/gign.mdl",
	"models/player/vip/vip.mdl",
	"models/player/gsg9/gsg9.mdl",
	"models/player/guerilla/guerilla.mdl",
	"models/player/arctic/arctic.mdl",
	"models/player/sas/sas.mdl",
	"models/player/terror/terror.mdl",
	"models/player/urban/urban.mdl",
	"models/player/spetsnaz/spetsnaz.mdl",
	"models/player/militia/militia.mdl"
};

void InitInput();
void ShutdownInput();
void IN_Commands();
void EV_HookEvents();

void CL_UnloadParticleMan()
{
	Sys_UnloadModule( g_hParticleManModule );

	g_pParticleMan = NULL;
	g_hParticleManModule = NULL;
}

void CL_LoadParticleMan()
{
	char szPDir[MAX_PATH];
	szPDir[0] = 0;

	if ( gEngfuncs.COM_ExpandFilename( PARTICLEMAN_DLLNAME, szPDir, sizeof( szPDir ) ) == FALSE )
	{
		g_pParticleMan = NULL;
		g_hParticleManModule = NULL;
		return;
	}

	g_hParticleManModule = Sys_LoadModule( szPDir );
	CreateInterfaceFn particleManFactory = Sys_GetFactory( g_hParticleManModule );

	if ( particleManFactory == NULL )
	{
		g_pParticleMan = NULL;
		g_hParticleManModule = NULL;
		return;
	}

	g_pParticleMan = (IParticleMan*)particleManFactory( PARTICLEMAN_INTERFACE, NULL );

	if ( g_pParticleMan )
	{
		g_pParticleMan->SetUp( &gEngfuncs );

		// Add custom particle classes here BEFORE calling anything else or you will die.
		g_pParticleMan->AddCustomParticleClassSize( sizeof( CBaseParticle ) );
		g_pParticleMan->AddCustomParticleClassSize( sizeof( CPartSmokeGrenade ) );
	}
}

extern "C" int EXPORT Initialize( cl_enginefunc_t* pEnginefuncs, int iVersion )
{
	memcpy( &gEngfuncs, pEnginefuncs, sizeof( cl_enginefunc_t ) );

//	RecClInitialize(pEnginefuncs, iVersion);

	if ( iVersion != CLDLL_INTERFACE_VERSION )
		return 0;

	EV_HookEvents();
	CL_LoadParticleMan();

	const char* szFsModule = "filesystem_stdio.dll";
	char szFSDir[MAX_PATH];
	szFSDir[0] = 0;
	if ( gEngfuncs.COM_ExpandFilename( szFsModule, szFSDir, sizeof( szFSDir ) ) == FALSE )
		return false;

	g_hFileSystemModule = Sys_LoadModule( szFSDir );
	ASSERT_AT( g_hFileSystemModule, 244 );
	if ( !g_hFileSystemModule )
		return false;

	CreateInterfaceFn fileSystemFactory = Sys_GetFactory( g_hFileSystemModule );
	if ( !fileSystemFactory )
		return false;

	g_pFileSystem = (IFileSystem*)fileSystemFactory( "VFileSystem009", NULL );
	ASSERT_AT( g_pFileSystem, 257 );
	if ( !g_pFileSystem )
		return false;

	char szDir[512];
	if ( gEngfuncs.COM_ExpandFilename( "cl_dlls\\GameUI.dll", szDir, sizeof( szDir ) ) )
	{
		g_hGameUIModule = Sys_LoadModule( szDir );
		if ( !g_hGameUIModule )
		{
			ASSERT_AT( g_hGameUIModule, 283 );
			return false;
		}

		CreateInterfaceFn gameUIFactory = Sys_GetFactory( g_hGameUIModule );
		if ( !gameUIFactory )
			return false;

		g_pGameConsole = (IGameConsole*)gameUIFactory( "GameConsole003", NULL );
		g_pGameUI = (IGameUI*)gameUIFactory( "GameUI007", NULL );
		g_pCareerUI = (ICareerUI*)gameUIFactory( "CareerUI001", NULL );
		g_pMusicManager = (IMusicManager*)gameUIFactory( "MusicManager001", NULL );
		ASSERT_AT( g_pGameConsole, 297 );
		ASSERT_AT( g_pGameUI, 298 );
		ASSERT_AT( g_pCareerUI, 299 );
		ASSERT_AT( g_pMusicManager, 300 );

		// get tracker interface, if any
		if ( gEngfuncs.COM_ExpandFilename( "Bin/TrackerUI.dll", szDir, sizeof( szDir ) ) )
		{
			g_hTrackerModule = Sys_LoadModule( szDir );
			CreateInterfaceFn trackerFactory = Sys_GetFactory( g_hTrackerModule );
			if ( trackerFactory )
			{
				g_pFriendsUser = (IFriendsUser*)trackerFactory( "FriendsUser001", NULL );
				return true;
			}
		}
	}

	g_pFriendsUser = NULL;
	g_hTrackerModule = NULL;

	return true;
}

extern "C" int EXPORT HUD_GetHullBounds( int hullnumber, float* mins, float* maxs )
{
//	RecClGetHullBounds(hullnumber, mins, maxs);

	int iret = 0;

	switch ( hullnumber )
	{
	case 0: // Normal player
		mins = Vector( -16, -16, -36 );
		maxs = Vector( 16, 16, 36 );
		iret = 1;
		break;
	case 1: // Crouched player
		mins = Vector( -16, -16, -18 );
		maxs = Vector( 16, 16, 18 );
		iret = 1;
		break;
	case 2: // Point based hull
		mins = Vector( 0, 0, 0 );
		maxs = Vector( 0, 0, 0 );
		iret = 1;
		break;
	}

	return iret;
}

extern "C" int EXPORT HUD_ConnectionlessPacket( const netadr_s*, const char*, char*, int* responseSize )
{
	*responseSize = 0;
	return 0;
}

extern "C" int EXPORT HUD_GetPlayerTeam( int player )
{
	return (unsigned int)player <= MAX_PLAYERS ? g_PlayerExtraInfo[player].team_id : 0;
}

extern "C" void EXPORT HUD_PlayerMoveInit( playermove_s* move )
{
//	RecClClientMoveInit(ppmove);

	PM_Init( move );
	VisMods_SetMoveVars( move->movevars );	// the engine's movevars: the sky's name from the start (cl_nosky)
}

extern "C" char EXPORT HUD_PlayerMoveTexture( char* name )
{
//	RecClClientTextureType(name);

	return PM_FindTextureType( name );
}

extern "C" void EXPORT HUD_PlayerMove( playermove_s* move, int server )
{
//	RecClClientMove(ppmove, server);

	PM_Move( move, server );
}

extern "C" int EXPORT HUD_VidInit()
{
	gHUD.VidInit();
	// a new map: the engine calls this from CL_ParseServerInfo on every map / connect / demo start; it never calls
	// HUD_Reset (hw.so: cl_funcs.pHudResetFunc is only ever written, by LoadInsecureClient)
	MiscFeatures_VidInit();
	AutoRate_VidInit();
	HitPositions_VidInit();
	return 1;
}

extern "C" void EXPORT HUD_Reset()
{
//	RecClHudReset();

	gHUD.VidInit();
}

extern "C" void EXPORT HUD_Init()
{
//	RecClHudInit();
	InitInput();
	gHUD.Init();
	DownloadInfo_Init();
	MiscFeatures_Init();
	AutoRate_Init();
	MenuBackground_Init();
	StretchView_Init();
}

extern "C" void EXPORT HUD_Shutdown()
{
	MenuBackground_Shutdown();
	DownloadInfo_Shutdown();
	AuthorAvatar_Shutdown();	// its download thread must end before client.dll unloads
	ShutdownInput();
	CL_UnloadParticleMan();
	if ( g_hTrackerModule )
		Sys_UnloadModule( g_hTrackerModule );
	if ( g_hFileSystemModule )
		Sys_UnloadModule( g_hFileSystemModule );
	if ( g_hGameUIModule )
		Sys_UnloadModule( g_hGameUIModule );
	g_pGameConsole = NULL;
	g_pGameUI = NULL;
	g_pCareerUI = NULL;
	g_pMusicManager = NULL;
	g_pBaseUI = NULL;
}

extern "C" int EXPORT HUD_Redraw( float time, int intermission )
{
//	RecClHudRedraw(time, intermission);

	StretchView_Draw();	// the world is drawn, the HUD not yet
	gHUD.Redraw( time, intermission );
	return 1;
}

extern "C" int EXPORT HUD_UpdateClientData( client_data_t* data, float time )
{
//	RecClHudUpdateClientData(pcldata, flTime);

	IN_Commands();
	return gHUD.UpdateClientData( data, time );
}

extern "C" void EXPORT HUD_Frame( double time )
{
#ifdef SHADOWS_PERF_PROBE
	PerfProbe_FrameMark();
	PERF_SCOPE( PERF_HUDFRAME );
#endif
	GetClientVoice()->Frame( time );
	ChatWindow_Frame();
	DownloadInfo_Frame();
	MiscFeatures_Frame();
	AutoRate_Frame();
	MenuBackground_Frame();
}

extern "C" void EXPORT HUD_VoiceStatus( int entity, qboolean talking )
{
	if ( entity >= 0 && entity < gEngfuncs.GetMaxClients() )
	{
		if ( talking )
		{
			g_PlayerExtraInfo[entity].radarflash = gHUD.m_flTime;
			g_PlayerExtraInfo[entity].radarflashon = 1;
			g_PlayerExtraInfo[entity].radarflashes = 99999;
		}
		else
		{
			g_PlayerExtraInfo[entity].radarflash = -1;
			g_PlayerExtraInfo[entity].radarflashon = 0;
			g_PlayerExtraInfo[entity].radarflashes = 0;
		}
	}
	GetClientVoice()->UpdateSpeakerStatus( entity, talking != 0 );
}

extern "C" void EXPORT HUD_DirectorMessage( int size, void* buffer )
{
//	RecClDirectorMessage(iSize, pbuf);

	gHUD.m_Spectator.DirectorMessage( size, buffer );
}

extern "C" CreateInterfaceFn EXPORT ClientFactory()
{
	return Sys_GetFactoryThis();
}

extern "C" void EXPORT F( void* buffer )
{
	cldll_func_t* destination = (cldll_func_t*)buffer;
	memcpy( &g_modfuncs, (void*)destination->pInitFunc, sizeof( g_modfuncs ) );
	g_pcldstAddrs = (cldll_func_dst_t*)destination->pHudVidInitFunc;
#ifdef SHADOWS_PERF_PROBE
#define PERF_STUDIO_INTERFACE PerfGetStudioModelInterface
#else
#define PERF_STUDIO_INTERFACE HUD_GetStudioModelInterface
#endif
	cldll_func_t callbacks =
	{
		Initialize, HUD_Init, HUD_VidInit, PERF_WRAP( PERF_REDRAW, HUD_Redraw ),
		PERF_WRAP( PERF_UPDATECLIENTDATA, HUD_UpdateClientData ), HUD_Reset,
		PERF_WRAP( PERF_OTHER_EXPORTS, HUD_PlayerMove ), HUD_PlayerMoveInit,
		PERF_WRAP( PERF_OTHER_EXPORTS, HUD_PlayerMoveTexture ), IN_ActivateMouse,
		IN_DeactivateMouse, IN_MouseEvent, IN_ClearStates, PERF_WRAP( PERF_OTHER_EXPORTS, IN_Accumulate ),
		PERF_WRAP( PERF_CREATEMOVE, CL_CreateMove ),
		CL_IsThirdPerson, CL_CameraOffset, KB_Find, PERF_WRAP( PERF_OTHER_EXPORTS, CAM_Think ),
		PERF_WRAP( PERF_CALCREFDEF, V_CalcRefdef ), PERF_WRAP( PERF_ADDENTITY, HUD_AddEntity ),
		PERF_WRAP( PERF_CREATEENTITIES, HUD_CreateEntities ), PERF_WRAP( PERF_TRIANGLES, HUD_DrawNormalTriangles ),
		PERF_WRAP( PERF_TRIANGLES, HUD_DrawTransparentTriangles ),
		PERF_WRAP( PERF_STUDIOEVENT, HUD_StudioEvent ), PERF_WRAP( PERF_POSTRUNCMD, HUD_PostRunCmd ), HUD_Shutdown,
		PERF_WRAP( PERF_OTHER_EXPORTS, HUD_TxferLocalOverrides ),
		PERF_WRAP( PERF_OTHER_EXPORTS, HUD_ProcessPlayerState ),
		PERF_WRAP( PERF_OTHER_EXPORTS, HUD_TxferPredictionData ), Demo_ReadBuffer,
		HUD_ConnectionlessPacket, HUD_GetHullBounds, HUD_Frame, HUD_Key_Event,
		PERF_WRAP( PERF_TEMPENTUPDATE, HUD_TempEntUpdate ), HUD_GetUserEntity, HUD_VoiceStatus, HUD_DirectorMessage,
		PERF_STUDIO_INTERFACE, HUD_ChatInputPosition, HUD_GetPlayerTeam, (CLIENTFACTORY)ClientFactory
	};
	*destination = callbacks;
}

//-----------------------------------------------------------------------------
// Purpose: Exports functions that are used by the gameUI for UI dialogs
//-----------------------------------------------------------------------------
class CClientExports : public IGameClientExports
{
public:
	const char* GetServerHostName() override
	{
		return gViewPortInterface ? gViewPortInterface->GetServerName() : "";
	}
	bool IsPlayerGameVoiceMuted( int player ) override
	{
		return GetClientVoice() ? GetClientVoice()->IsPlayerBlocked( player ) : false;
	}
	void MutePlayerGameVoice( int player ) override
	{
		if ( GetClientVoice() )
			GetClientVoice()->SetPlayerBlockedState( player, true );
	}
	void UnmutePlayerGameVoice( int player ) override
	{
		if ( GetClientVoice() )
			GetClientVoice()->SetPlayerBlockedState( player, false );
	}
};

EXPOSE_SINGLE_INTERFACE( CClientExports, IGameClientExports, GAMECLIENTEXPORTS_INTERFACE_VERSION );
