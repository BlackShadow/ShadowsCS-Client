/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
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
// hud.cpp
//
// implementation of CHud class
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hud.h"
#include "fontstyle.h"
#include "vismods.h"

void AdvancedOptions_Init();
#include "radar2.h"
#include "cl_util.h"
#include "demo.h"
#include "demo_api.h"
#include "parsemsg.h"
#include "buy_presets.h"
#include "../public/game_controls/iviewport.h"
#include "../public/game_controls/iviewportmsgs.h"
#include "VGUI/icstrikeviewport.h"
#include "VGUI/icstrikeviewportmsgs.h"
#include "VGUI/careermenus.h"
#include "../game_shared/voice_status.h"
#include "particleman.h"
#include "vgui/ISurface.h"
#include "vgui/IScheme.h"
#include "../../engine/controls.h"
#include "../../vgui2/controls/controls.h"

hud_player_info_t g_PlayerInfoList[MAX_PLAYERS + 1];	   // player info from the engine
extra_player_info_t g_PlayerExtraInfo[MAX_PLAYERS + 1];   // additional player info sent directly to the client dll
cvar_t* cl_lw;
static cvar_t* hud_color;

static unsigned long HudPackRGB( float r, float g, float b )
{
	const float c[3] = { r, g, b };
	unsigned long packed = 0;
	for ( int i = 0; i < 3; i++ )
	{
		const int v = (int)( c[i] + 0.5f );
		packed = ( packed << 8 ) | (unsigned long)( v < 0 ? 0 : v > 255 ? 255 : v );
	}
	return packed;
}

// hud_color: "r g b", "rainbow" (the colour wheel, round once every 6 seconds), "christmas" (red and white
// fading into each other every 2 seconds) or "dynamic" (health and money coloured by how much there is, the rest the
// default orange)
enum { HUDCOLOR_FIXED, HUDCOLOR_RAINBOW, HUDCOLOR_CHRISTMAS, HUDCOLOR_DYNAMIC };
static unsigned long s_hudRGB = RGB_HUD_DEFAULT;	// the fixed colour (and dynamic's for the rest)

// hud_color read again when it changes
static int HudColorMode()
{
	static char s_parsed[64] = "";
	static int s_mode = HUDCOLOR_FIXED;
	if ( !hud_color || !hud_color->string )
	{
		s_hudRGB = RGB_HUD_DEFAULT;
		return HUDCOLOR_FIXED;
	}
	if ( strncmp( hud_color->string, s_parsed, sizeof( s_parsed ) - 1 ) )
	{
		strncpy( s_parsed, hud_color->string, sizeof( s_parsed ) - 1 );
		s_parsed[sizeof( s_parsed ) - 1] = 0;
		int c[3];
		s_mode = HUDCOLOR_FIXED;
		s_hudRGB = RGB_HUD_DEFAULT;
		if ( !stricmp( s_parsed, "rainbow" ) )
			s_mode = HUDCOLOR_RAINBOW;
		else if ( !stricmp( s_parsed, "christmas" ) )
			s_mode = HUDCOLOR_CHRISTMAS;
		else if ( !stricmp( s_parsed, "dynamic" ) )
			s_mode = HUDCOLOR_DYNAMIC;
		else if ( sscanf( s_parsed, "%d %d %d", &c[0], &c[1], &c[2] ) == 3 )
			s_hudRGB = HudPackRGB( (float)c[0], (float)c[1], (float)c[2] );
	}
	return s_mode;
}

bool HudColorDynamic()
{
	return HudColorMode() == HUDCOLOR_DYNAMIC;
}

unsigned long HudDynamicRGB( float value, float red, float yellow, float green )
{
	if ( value <= red )
		return HudPackRGB( 255.0f, 0.0f, 0.0f );
	if ( value < yellow )
		return HudPackRGB( 255.0f, 255.0f * ( value - red ) / ( yellow - red ), 0.0f );
	if ( value < green )
		return HudPackRGB( 255.0f * ( green - value ) / ( green - yellow ), 255.0f, 0.0f );
	return HudPackRGB( 0.0f, 255.0f, 0.0f );
}

unsigned long HudColorRGB()
{
	const int s_mode = HudColorMode();
	if ( s_mode == HUDCOLOR_FIXED || s_mode == HUDCOLOR_DYNAMIC )
		return s_hudRGB;

	// real time, so the colours keep moving while the game is paused
	const double now = gEngfuncs.GetAbsoluteTime();
	if ( s_mode == HUDCOLOR_RAINBOW )
	{
		// fully saturated hue, 0..6 around the wheel
		const float h = (float)fmod( now / 6.0, 1.0 ) * 6.0f;
		const int sector = (int)h;
		const float f = h - (float)sector;
		const float up = 255.0f * f, down = 255.0f * ( 1.0f - f );
		switch ( sector )
		{
		case 0: return HudPackRGB( 255.0f, up, 0.0f );
		case 1: return HudPackRGB( down, 255.0f, 0.0f );
		case 2: return HudPackRGB( 0.0f, 255.0f, up );
		case 3: return HudPackRGB( 0.0f, down, 255.0f );
		case 4: return HudPackRGB( up, 0.0f, 255.0f );
		default: return HudPackRGB( 255.0f, 0.0f, down );
		}
	}
	// christmas: red (255 20 20) -> white -> red
	const float white = 0.5f - 0.5f * (float)cos( fmod( now, 2.0 ) * 3.14159265358979 );
	const float gb = 20.0f + 235.0f * white;
	return HudPackRGB( 255.0f, gb, gb );
}
cvar_t* cl_tutor;
cvar_t* cl_career_difficulty;
cvar_t* cl_fog_density;
cvar_t* cl_fog_r;
cvar_t* cl_fog_g;
cvar_t* cl_fog_b;
float g_lastFOV;

extern cvar_t* sensitivity;
extern cvar_t* cl_righthand;
extern cvar_t* viewmodel_fov;
extern cvar_t* cl_minmodels;
extern cvar_t* cl_dynamiccrosshair;
extern cvar_t* cl_min_ct;
extern cvar_t* cl_min_t;
extern cvar_t* cl_weather;
extern cvar_t* cl_weather_type;
extern cvar_t* cl_shadows;
extern cvar_t* cl_radartype;
extern vgui2::HFont _font;

const char* GetStringTeamColor( int team );
client_sprite_t* GetSpriteList( client_sprite_t* pList, const char* psz, int iRes, int iCount );
void CountWeaponIconTiers( int tier, int* known, int* atTier );
void StudioSetShadowSprite( int sprite );

class CCStrikeVoiceStatusHelper : public IVoiceStatusHelper
{
public:
	void GetPlayerTextColor( int player, int color[3] ) override
	{
		color[0] = color[1] = color[2] = 255;
		if ( (unsigned int)player <= MAX_PLAYERS )
		{
			vgui2::IScheme* scheme = vgui2::scheme()->GetIScheme( gViewPortInterface->GetViewPortScheme() );
			Color teamColor = scheme->GetColor( GetStringTeamColor( g_PlayerExtraInfo[player].teamnumber ), Color( 255, 255, 255, 0 ) );
			color[0] = teamColor.r();
			color[1] = teamColor.g();
			color[2] = teamColor.b();
		}
	}
	int GetAckIconHeight() override
	{
		return 3 * gHUD.m_iFontHeight + 6;
	}
	bool CanShowSpeakerLabels() override
	{
		return gViewPortInterface ? !gViewPortInterface->IsScoreBoardVisible() : false;
	}
	const char* GetPlayerLocation( int player ) override
	{
		return (unsigned int)player <= MAX_PLAYERS ? g_PlayerExtraInfo[player].location : "";
	}
};

static CCStrikeVoiceStatusHelper g_VoiceStatusHelper;

int __MsgFunc_Logo( const char* name, int size, void* buffer )
{
	return gHUD.MsgFunc_Logo( name, size, buffer );
}

//DECLARE_MESSAGE(m_Logo, Logo)
int __MsgFunc_ResetHUD( const char* name, int size, void* buffer )
{
	return gHUD.MsgFunc_ResetHUD( name, size, buffer );
}

int __MsgFunc_GameMode( const char* name, int size, void* buffer )
{
	return gHUD.MsgFunc_GameMode( name, size, buffer );
}

int __MsgFunc_InitHUD( const char* name, int size, void* buffer )
{
	gHUD.MsgFunc_InitHUD( name, size, buffer );
	return 1;
}

int __MsgFunc_ViewMode( const char* name, int size, void* buffer )
{
	gHUD.MsgFunc_ViewMode( name, size, buffer );
	return 1;
}

int __MsgFunc_SetFOV( const char* name, int size, void* buffer )
{
	return gHUD.MsgFunc_SetFOV( name, size, buffer );
}

int __MsgFunc_HLTV( const char* name, int size, void* buffer )
{
	return gHUD.MsgFunc_HLTV( name, size, buffer );
}

int CHud::MsgFunc_SpecHealth( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	int health = READ_BYTE();
	gHUD.m_Health.m_iPlayerLastPointedAt = g_iUser2;
	g_PlayerExtraInfo[g_iUser2].health = health;
	gViewPortInterface->UpdateSpectatorPanel();
	return 1;
}

int CHud::MsgFunc_SpecHealth2( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	int health = READ_BYTE();
	int player = READ_BYTE();
	gHUD.m_Health.m_iPlayerLastPointedAt = g_iUser2;
	g_PlayerExtraInfo[player].health = health;
	gViewPortInterface->UpdateSpectatorPanel();
	return 1;
}

int __MsgFunc_SpecHealth( const char* name, int size, void* buffer )
{
	return gHUD.MsgFunc_SpecHealth( name, size, buffer );
}

int __MsgFunc_SpecHealth2( const char* name, int size, void* buffer )
{
	return gHUD.MsgFunc_SpecHealth2( name, size, buffer );
}

int __MsgFunc_Concuss( const char* name, int size, void* buffer )
{
	return gHUD.MsgFunc_Concuss( name, size, buffer );
}

int __MsgFunc_ReloadSound( const char* name, int size, void* buffer )
{
	return gHUD.MsgFunc_ReloadSound( name, size, buffer );
}

int __MsgFunc_BombDrop( const char* name, int size, void* buffer )
{
	gHUD.MsgFunc_BombDrop( name, size, buffer );
	return 1;
}

int __MsgFunc_BombPickup( const char* name, int size, void* buffer )
{
	gHUD.MsgFunc_BombPickup( name, size, buffer );
	return 1;
}

int __MsgFunc_ADStop( const char* name, int size, void* buffer )
{
	gHUD.MsgFunc_ADStop( name, size, buffer );
	return 1;
}

int __MsgFunc_HostagePos( const char* name, int size, void* buffer )
{
	gHUD.MsgFunc_HostagePos( name, size, buffer );
	return 1;
}

int __MsgFunc_HostageK( const char* name, int size, void* buffer )
{
	gHUD.MsgFunc_HostageK( name, size, buffer );
	return 1;
}

int __MsgFunc_BotVoice( const char* name, int size, void* buffer )
{
	return gHUD.MsgFunc_BotVoice( name, size, buffer );
}

int __MsgFunc_Brass( const char* name, int size, void* buffer )
{
	return gHUD.MsgFunc_Brass( name, size, buffer );
}

int __MsgFunc_Fog( const char* name, int size, void* buffer )
{
	gHUD.MsgFunc_Fog( name, size, buffer );
	return 1;
}

int __MsgFunc_ShowTimer( const char* name, int size, void* buffer )
{
	gHUD.MsgFunc_ShowTimer( name, size, buffer );
	return 1;
}

int __MsgFunc_TeamNames( const char* name, int size, void* buffer )
{
	return gViewPortMsgs ? gViewPortMsgs->MsgFunc_TeamNames( name, size, buffer ) : 0;
}

int __MsgFunc_Feign( const char* name, int size, void* buffer )
{
	return gViewPortMsgs ? gViewPortMsgs->MsgFunc_Feign( name, size, buffer ) : 0;
}

int __MsgFunc_Detpack( const char* name, int size, void* buffer )
{
	return gViewPortMsgs ? gViewPortMsgs->MsgFunc_Detpack( name, size, buffer ) : 0;
}

int __MsgFunc_VGUIMenu( const char* name, int size, void* buffer )
{
	return gViewPortMsgs ? gViewPortMsgs->MsgFunc_VGUIMenu( name, size, buffer ) : 0;
}

int __MsgFunc_TutorText( const char* name, int size, void* buffer )
{
	return gViewPortMsgs ? gViewPortMsgs->MsgFunc_TutorText( name, size, buffer ) : 0;
}

int __MsgFunc_TutorLine( const char* name, int size, void* buffer )
{
	return gViewPortMsgs ? gViewPortMsgs->MsgFunc_TutorLine( name, size, buffer ) : 0;
}

int __MsgFunc_TutorState( const char* name, int size, void* buffer )
{
	return gViewPortMsgs ? gViewPortMsgs->MsgFunc_TutorState( name, size, buffer ) : 0;
}

int __MsgFunc_TutorClose( const char* name, int size, void* buffer )
{
	return gViewPortMsgs ? gViewPortMsgs->MsgFunc_TutorClose( name, size, buffer ) : 0;
}

int __MsgFunc_MOTD( const char* name, int size, void* buffer )
{
	return gViewPortMsgs ? gViewPortMsgs->MsgFunc_MOTD( name, size, buffer ) : 0;
}

int __MsgFunc_BuildSt( const char* name, int size, void* buffer )
{
	return gViewPortMsgs ? gViewPortMsgs->MsgFunc_BuildSt( name, size, buffer ) : 0;
}

int __MsgFunc_RandomPC( const char* name, int size, void* buffer )
{
	return gViewPortMsgs ? gViewPortMsgs->MsgFunc_RandomPC( name, size, buffer ) : 0;
}

int __MsgFunc_ServerName( const char* name, int size, void* buffer )
{
	return gViewPortMsgs ? gViewPortMsgs->MsgFunc_ServerName( name, size, buffer ) : 0;
}

int __MsgFunc_ScoreInfo( const char* name, int size, void* buffer )
{
	return gViewPortMsgs ? gCSViewPortMsgs->MsgFunc_ScoreInfo( name, size, buffer ) : 0;
}

int __MsgFunc_TeamScore( const char* name, int size, void* buffer )
{
	return gViewPortMsgs ? gViewPortMsgs->MsgFunc_TeamScore( name, size, buffer ) : 0;
}

int __MsgFunc_TeamInfo( const char* name, int size, void* buffer )
{
	return gViewPortMsgs ? gViewPortMsgs->MsgFunc_TeamInfo( name, size, buffer ) : 0;
}

int __MsgFunc_Location( const char* name, int size, void* buffer )
{
	return gViewPortMsgs ? gCSViewPortMsgs->MsgFunc_Location( name, size, buffer ) : 0;
}

int __MsgFunc_Spectator( const char* name, int size, void* buffer )
{
	return gViewPortMsgs ? gViewPortMsgs->MsgFunc_Spectator( name, size, buffer ) : 0;
}

int __MsgFunc_AllowSpec( const char* name, int size, void* buffer )
{
	return gViewPortMsgs ? gViewPortMsgs->MsgFunc_AllowSpec( name, size, buffer ) : 0;
}

int __MsgFunc_ForceCam( const char* name, int size, void* buffer )
{
	return gCSViewPortMsgs ? gCSViewPortMsgs->MsgFunc_ForceCam( name, size, buffer ) : 0;
}

int __MsgFunc_BuyClose( const char*, int, void* )
{
	if ( gViewPortInterface )
		gViewPortInterface->HideVGUIMenu( 28 );
	return 0;
}

int __MsgFunc_ShadowIdx( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	StudioSetShadowSprite( READ_LONG() );
	return 0;
}

int __MsgFunc_BotProgress( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	int action = (char)READ_BYTE();
	switch ( action )
	{
	case 0:
	{
		int progress = READ_BYTE();
		char* text = READ_STRING();
		if ( !gCSViewPortInterface->IsProgressBarVisible() )
			gCSViewPortInterface->StartProgressBar( text, 100, 0, false );
		gCSViewPortInterface->UpdateProgressBar( text, progress );
		break;
	}
	case 1:
		gCSViewPortInterface->StartProgressBar( READ_STRING(), 100, 0, false );
		break;
	case 2:
		gCSViewPortInterface->StopProgressBar();
		break;
	}
	return 0;
}

int __MsgFunc_CZCareerHUD( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	const char* action = READ_STRING();
	if ( !strcmp( action, "MATCH" ) )
	{
		int ctWins = READ_LONG();
		int tWins = READ_LONG();
		int neededWins = READ_BYTE();
		int winBy = READ_BYTE();
		int result = READ_BYTE();
		ShowCareerMatchEndMenu( ctWins, tWins, neededWins, winBy, result );
	}
	else if ( !strcmp( action, "ROUND" ) )
	{
		int ctWins = READ_LONG();
		int tWins = READ_LONG();
		int neededWins = READ_BYTE();
		int winBy = READ_BYTE();
		int result = READ_BYTE();
		ShowCareerRoundEndMenu( ctWins, tWins, neededWins, winBy, result );
	}
	return 0;
}

void __CmdFunc_ShowCommandMenu()
{
	if ( gViewPortInterface )
		gViewPortInterface->ShowCommandMenu();
}

void __CmdFunc_HideCommandMenu()
{
	if ( gViewPortInterface )
		gViewPortInterface->HideCommandMenu();
}

void __CmdFunc_InputPlayerSpecial()
{
	if ( gViewPortInterface )
		gViewPortInterface->InputPlayerSpecial();
}

void __CmdFunc_ToggleGunSmoke()
{
	gHUD.m_bRenderGunSmoke = !gHUD.m_bRenderGunSmoke;
}

void __CmdFunc_AutoBuy()
{
	gHUD.AutoBuy();
}

void __CmdFunc_Rebuy()
{
	gHUD.Rebuy();
}

void __CmdFunc_RunCredits()
{
	gHUD.RunCredits();
}

void __CmdFunc_HudTierInfo()
{
	gHUD.PrintHudTierInfo();
}

void __CmdFunc_ClientBuy()
{
	if ( gViewPortInterface && gHUD.m_StatusIcons.InBuyZone()
		&& !g_PlayerExtraInfo[gEngfuncs.GetLocalPlayer()->index].dead )
	{
		const char* menus = gEngfuncs.LocalPlayerInfo_ValueForKey( "_vgui_menus" );
		if ( menus && *menus && !atoi( menus ) )
			gEngfuncs.pfnServerCmd( "buy\n" );
		else
		{
			gViewPortInterface->ShowVGUIMenu( 28 );
			gEngfuncs.pfnServerCmd( "client_buy_open\n" );
		}
	}
}

void CHud::Init()
{
	HOOK_MESSAGE( Logo );
	HOOK_MESSAGE( ResetHUD );
	HOOK_MESSAGE( GameMode );
	HOOK_MESSAGE( InitHUD );
	HOOK_MESSAGE( ViewMode );
	HOOK_MESSAGE( SetFOV );
	HOOK_MESSAGE( HLTV );
	HOOK_MESSAGE( SpecHealth );
	HOOK_MESSAGE( Concuss );
	HOOK_MESSAGE( ReloadSound );
	HOOK_MESSAGE( BombDrop );
	HOOK_MESSAGE( BombPickup );
	HOOK_MESSAGE( ADStop );
	HOOK_MESSAGE( HostagePos );
	HOOK_MESSAGE( HostageK );
	HOOK_MESSAGE( CZCareerHUD );
	HOOK_MESSAGE( ShadowIdx );
	HOOK_MESSAGE( BotVoice );
	HOOK_MESSAGE( Brass );
	HOOK_MESSAGE( Fog );
	HOOK_MESSAGE( ShowTimer );
	HOOK_COMMAND( "+commandmenu", ShowCommandMenu );
	HOOK_COMMAND( "-commandmenu", HideCommandMenu );
	HOOK_COMMAND( "gunsmoke", ToggleGunSmoke );
	HOOK_COMMAND( "autobuy", AutoBuy );
	HOOK_COMMAND( "rebuy", Rebuy );
	HOOK_COMMAND( "buy", ClientBuy );
	HOOK_COMMAND( "hud_tierinfo", HudTierInfo );
	if ( IsGame( "czero" ) )
		HOOK_COMMAND( "credits", RunCredits );
	HOOK_MESSAGE( TeamNames );
	HOOK_MESSAGE( Feign );
	HOOK_MESSAGE( Detpack );
	HOOK_MESSAGE( MOTD );
	HOOK_MESSAGE( BuildSt );
	HOOK_MESSAGE( RandomPC );
	HOOK_MESSAGE( ServerName );
	HOOK_MESSAGE( ScoreInfo );
	HOOK_MESSAGE( TeamScore );
	HOOK_MESSAGE( TeamInfo );
	HOOK_MESSAGE( Location );
	HOOK_MESSAGE( Spectator );
	HOOK_MESSAGE( AllowSpec );
	HOOK_MESSAGE( ForceCam );

	// VGUI Menus
	HOOK_MESSAGE( VGUIMenu );
	HOOK_MESSAGE( TutorText );
	HOOK_MESSAGE( TutorLine );
	HOOK_MESSAGE( TutorState );
	HOOK_MESSAGE( TutorClose );
	HOOK_MESSAGE( BuyClose );
	HOOK_MESSAGE( SpecHealth2 );
	HOOK_MESSAGE( BotProgress );
	if ( !TheBuyPresets )
		TheBuyPresets = new BuyPresetManager;
	TheBuyPresets->ResetItems();
	gEngfuncs.pfnRegisterVariable( "hud_takesshots", "0", FCVAR_ARCHIVE );
	m_iLogo = 0;
	m_iFOV = 0;
	m_bShowTimer = true;
	m_autoBuyStringSent = true;
	m_rebuyStringSent = true;
	gEngfuncs.pfnRegisterVariable( "zoom_sensitivity_ratio", "1.2", FCVAR_ARCHIVE );
	gEngfuncs.pfnRegisterVariable( "max_shells", "120", FCVAR_ARCHIVE );
	gEngfuncs.pfnRegisterVariable( "max_smokepuffs", "120", FCVAR_ARCHIVE );
	gEngfuncs.pfnRegisterVariable( "fastsprites", "0", FCVAR_ARCHIVE );
	gEngfuncs.pfnRegisterVariable( "cl_corpsestay", "600", FCVAR_ARCHIVE );
	gEngfuncs.pfnRegisterVariable( "_cl_autowepswitch", "1", FCVAR_ARCHIVE | FCVAR_USERINFO );
	gEngfuncs.pfnRegisterVariable( "scoreboard_showhealth", "1", FCVAR_ARCHIVE );
	gEngfuncs.pfnRegisterVariable( "scoreboard_showmoney", "1", FCVAR_ARCHIVE );
	gEngfuncs.pfnRegisterVariable( "scoreboard_shortheaders", "0", FCVAR_ARCHIVE );
	gEngfuncs.pfnRegisterVariable( "scoreboard_showavatars", "1", FCVAR_ARCHIVE );
	default_fov = gEngfuncs.pfnRegisterVariable( "default_fov", "90", FCVAR_ARCHIVE );
	cl_lw = gEngfuncs.pfnGetCvarPointer( "cl_lw" );
	m_pCvarStealMouse = gEngfuncs.pfnRegisterVariable( "hud_capturemouse", "1", FCVAR_ARCHIVE );
	m_pCvarDraw = gEngfuncs.pfnRegisterVariable( "hud_draw", "1", FCVAR_ARCHIVE );
	// HUD sprite tier: 0 = automatic. Read at VidInit (next map load / connect).
	m_pCvarHudTier = gEngfuncs.pfnRegisterVariable( "hud_tier", "0", FCVAR_ARCHIVE );
	FontStyle_Init();
	hud_color = CVAR_CREATE( "hud_color", HUD_COLOR_DEFAULT, FCVAR_ARCHIVE );
	m_fGLTEXSORT = gEngfuncs.pfnGetCvarFloat( "gl_texsort" );
	if ( gEngfuncs.pfnGetCvarFloat( "gl_alphamin" ) == 0 )
		m_bSoftwaremode = true;
	m_flGLZMAX = gEngfuncs.pfnGetCvarFloat( "gl_zmax" );
	cl_righthand = gEngfuncs.pfnRegisterVariable( "cl_righthand", "1", FCVAR_ARCHIVE );
	// FOV the weapon (view model) is drawn with, independent of default_fov and zoom. 0 = follow the world FOV.
	viewmodel_fov = gEngfuncs.pfnRegisterVariable( "viewmodel_fov", "90", FCVAR_ARCHIVE );
	VisMods_Init();
	AdvancedOptions_Init();
	Radar2_Init();
	cl_minmodels = gEngfuncs.pfnRegisterVariable( "cl_minmodels", "0", FCVAR_ARCHIVE );
	cl_dynamiccrosshair = gEngfuncs.pfnRegisterVariable( "cl_dynamiccrosshair", "1", FCVAR_ARCHIVE );
	cl_min_ct = gEngfuncs.pfnRegisterVariable( "cl_min_ct", "2", FCVAR_ARCHIVE );
	cl_min_t = gEngfuncs.pfnRegisterVariable( "cl_min_t", "1", FCVAR_ARCHIVE );
	cl_weather = gEngfuncs.pfnRegisterVariable( "cl_weather", "1", FCVAR_ARCHIVE );
	// the Advanced window's Weather (z_weather.cpp): 0 none, 1 rain, 2 snow, on every map
	cl_weather_type = gEngfuncs.pfnRegisterVariable( "cl_weather_type", "0", FCVAR_ARCHIVE );
	if ( gEngfuncs.pfnGetCvarFloat( "developer" ) > 0 )
	{
		cl_fog_density = gEngfuncs.pfnRegisterVariable( "cl_fog_density", "0", 0 );
		cl_fog_r = gEngfuncs.pfnRegisterVariable( "cl_fog_r", "0", 0 );
		cl_fog_g = gEngfuncs.pfnRegisterVariable( "cl_fog_g", "0", 0 );
		cl_fog_b = gEngfuncs.pfnRegisterVariable( "cl_fog_b", "0", 0 );
	}
	cl_shadows = gEngfuncs.pfnRegisterVariable( "cl_shadows", "1", FCVAR_ARCHIVE );
	cl_radartype = gEngfuncs.pfnRegisterVariable( "cl_radartype", "0", FCVAR_ARCHIVE );
	if ( IsGame( "czero" ) )
	{
		cl_tutor = gEngfuncs.pfnRegisterVariable( "tutor_enable", "1", FCVAR_ARCHIVE );
		cl_career_difficulty = gEngfuncs.pfnRegisterVariable( "cl_career_difficulty", "0", FCVAR_ARCHIVE );
	}
	m_pSpriteList = NULL;

	// Clear any old HUD list
	while ( m_pHudList )
	{
		HUDLIST* item = m_pHudList;
		m_pHudList = item->pNext;
		free( item );
	}
	m_flTime = 1;
	m_NightVision.Init();
	m_sniperScope.Init();
	m_Ammo.Init();
	m_Health.Init();
	m_SayText.Init();
	m_Spectator.Init();
	m_Geiger.Init();
	m_Train.Init();
	m_Battery.Init();
	m_Flash.Init();
	m_Message.Init();
	m_StatusBar.Init();
	m_DeathNotice.Init();
	m_AmmoSecondary.Init();
	m_TextMessage.Init();
	m_StatusIcons.Init();
	m_accountBalance.Init();
	m_roundTimer.Init();
	m_progressBar.Init();
	m_careerTaskTimer.Init();
	m_scenarioStatus.Init();
	m_Menu.Init();
	m_VGUI2Print.Init();
	GetClientVoice()->Init( &g_VoiceStatusHelper );
	MsgFunc_ResetHUD( NULL, 0, NULL );
	m_bRenderGunSmoke = false;
	m_Teamplay = 1;
	g_ShellQueue.m_flDuration = 2;
	g_SmokePuffQueue.m_flDuration = 1;
}

void CHud::VidInit()
{
	m_scrinfo.iSize = sizeof( m_scrinfo );
	gEngfuncs.pfnGetScreenInfo( &m_scrinfo );
	if ( g_pParticleMan )
		g_pParticleMan->ResetParticles();

	// ----------
	// Load Sprites
	// ---------
//	m_hsprFont = LoadSprite("sprites/%d_font.spr");

	m_hsprLogo = 0;
	m_hsprCursor = 0;

	// Only load this once
	if ( !m_pSpriteList )
	{
		// we need to load the hud.txt, and all sprites within
		m_pSpriteList = gEngfuncs.pfnSPR_GetList( "sprites/hud.txt", &m_iSpriteCountAllRes );
	}

	// choose the tier (m_iRes) and its scale (m_iHudScale), and (re)build the sprite table for it
	SetupHudTier();
	VisMods_VidInit();
	Radar2_VidInit();

	// assumption: number_1, number_2, etc, are all listed and loaded sequentially
	m_HUD_number_0 = GetSpriteIndex( "number_0" );
	m_NightVision.VidInit();
	m_sniperScope.VidInit();
	m_Ammo.VidInit();
	m_Health.VidInit();
	m_Spectator.VidInit();
	m_Geiger.VidInit();
	m_Train.VidInit();
	m_Battery.VidInit();
	m_Flash.VidInit();
	m_Message.VidInit();
	m_StatusBar.VidInit();
	m_DeathNotice.VidInit();
	m_SayText.VidInit();
	m_Menu.VidInit();
	m_AmmoSecondary.VidInit();
	m_StatusIcons.VidInit();
	m_accountBalance.VidInit();
	m_roundTimer.VidInit();
	m_progressBar.VidInit();
	m_careerTaskTimer.VidInit();
	m_scenarioStatus.VidInit();
	m_flCheatCheckTime = -1;
	m_flExpensiveCheckTime = -1;
	m_iFontHeight = GetSpriteRect( m_HUD_number_0 ).bottom - GetSpriteRect( m_HUD_number_0 ).top;
	m_iFontEngineHeight = vgui2::surface()->GetFontTall( _font );
	GetClientVoiceHud()->VidInit();
	m_VGUI2Print.VidInit();
}

#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
CHud::CHud() : m_pHudList( NULL ), m_iSpriteCount( 0 ), m_iHudScale( 1 ), m_iSpriteCapacity( 0 )
{
}

// CHud destructor
// cleans up memory allocated for m_rg* arrays
CHud::~CHud()
{
	delete[] m_rghSprites;
	delete[] m_rgrcRects;
	delete[] m_rgszSpriteNames;
	while ( m_pHudList )
	{
		HUDLIST* element = m_pHudList;
		m_pHudList = element->pNext;
		free( element );
	}
	g_ShellQueue.Update( 99999 );
}

void CHud::AddHudElem( CHudBase* element )
{
	if ( !element )
		return;
	HUDLIST* entry = (HUDLIST*)malloc( sizeof( HUDLIST ) );
	if ( !entry )
		return;
	memset( entry, 0, sizeof( HUDLIST ) );
	entry->p = element;
	if ( !m_pHudList )
	{
		m_pHudList = entry;
	}
	else
	{
		HUDLIST* last = m_pHudList;
		while ( last->pNext )
			last = last->pNext;
		last->pNext = entry;
	}
}

int CHud::GetSpriteIndex( const char* name )
{
	// look through the loaded sprite name list for SpriteName
	for ( int i = 0; i < m_iSpriteCount; i++ )
	{
		if ( !strncmp( name, m_rgszSpriteNames + i * MAX_SPRITE_NAME_LENGTH, MAX_SPRITE_NAME_LENGTH ) )
			return i;
	}
	return -1; // invalid sprite
}

// ---------------------------------------------------------------------------------------------------------------
// HUD resolution tiers.
// Each hi-res tier has its own sprites/cs<tier>/ pages and overlay lists with the stock record names.
// Missing or malformed overlays fall back to the stock CS art.
// ---------------------------------------------------------------------------------------------------------------

#define HUD_MAX_FRAME_PIXELS ( 640 * 480 ) // GL_Upload16's static buffer: bigger frames corrupt engine memory

static int HUD_TierScale( int tier )
{
	switch ( tier )
	{
	case 1280: return 2;
	case 3840: return 4;	// (a 2560 tier, 3 times, was dropped: its sprites were not worth their size)
	default:   return 1;
	}
}

// Validates an IDSP v2 sprite in memory (every frame single and inside the file); returns frame 0's size.
static bool HUD_ParseSprite( const unsigned char* data, int length, int* width, int* height )
{
	const int headerSize = 40; // dsprite_t
	if ( !data || length < headerSize + 2 || memcmp( data, "IDSP", 4 ) )
		return false;
	int version, numframes;
	memcpy( &version, data + 4, sizeof( version ) );
	memcpy( &numframes, data + 28, sizeof( numframes ) );
	short paletteCount;
	memcpy( &paletteCount, data + headerSize, sizeof( paletteCount ) );
	if ( version != 2 || numframes < 1 || paletteCount < 1 || paletteCount > 256 )
		return false;
	int offset = headerSize + 2 + paletteCount * 3;
	for ( int frame = 0; frame < numframes; frame++ )
	{
		int header[5]; // frame type, origin x, origin y, width, height
		if ( offset > length - (int)sizeof( header ) )
			return false;
		memcpy( header, data + offset, sizeof( header ) );
		const int w = header[3], h = header[4];
		if ( header[0] != 0 /* SPR_SINGLE */ || w <= 0 || h <= 0 || w > HUD_MAX_FRAME_PIXELS / h )
			return false;
		offset += sizeof( header );
		if ( w * h > length - offset )
			return false;
		offset += w * h;
		if ( frame == 0 )
		{
			*width = w;
			*height = h;
		}
	}
	return true;
}

struct hudspriteprobe_t
{
	char szPath[96];
	bool bValid;
	int iWidth, iHeight;
};
static hudspriteprobe_t g_HudSpriteProbes[192];
static int g_iHudSpriteProbes;

// Checks a hi-res sprite without SPR_Load: SPR_Load prints an error for a missing file (fatal at developer > 2)
// and a malformed file is fatal inside it. Results are cached; sprites cannot change while the game runs.
static bool HUD_ProbeSprite( const char* path, int* width, int* height )
{
	for ( int i = 0; i < g_iHudSpriteProbes; i++ )
	{
		if ( !strcmp( g_HudSpriteProbes[i].szPath, path ) )
		{
			*width = g_HudSpriteProbes[i].iWidth;
			*height = g_HudSpriteProbes[i].iHeight;
			return g_HudSpriteProbes[i].bValid;
		}
	}
	int length = 0, w = 0, h = 0;
	unsigned char* data = (unsigned char*)gEngfuncs.COM_LoadFile( (char*)path, 5, &length ); // silent when missing
	const bool valid = HUD_ParseSprite( data, length, &w, &h );
	if ( data )
		gEngfuncs.COM_FreeFile( data );
	if ( g_iHudSpriteProbes < (int)( sizeof( g_HudSpriteProbes ) / sizeof( g_HudSpriteProbes[0] ) ) && strlen( path ) < sizeof( g_HudSpriteProbes[0].szPath ) )
	{
		hudspriteprobe_t& probe = g_HudSpriteProbes[g_iHudSpriteProbes++];
		strcpy( probe.szPath, path );
		probe.bValid = valid;
		probe.iWidth = w;
		probe.iHeight = h;
	}
	*width = w;
	*height = h;
	return valid;
}

struct hudtierlist_t
{
	char szPath[128];
	client_sprite_t* pList;
	int iCount;
};
static hudtierlist_t g_HudTierLists[128]; // 33 stock overlay lists per hi-res tier, plus custom weapons
static int g_iHudTierLists;

// SPR_GetList for the hi-res overlay lists, cached for the process lifetime (the engine allocates the list; it must
// not be freed by client.dll). A missing list returns NULL and is not cached.
client_sprite_t* CHud::GetTierSpriteList( const char* path, int* count )
{
	*count = 0;
	for ( int i = 0; i < g_iHudTierLists; i++ )
	{
		if ( !strcmp( g_HudTierLists[i].szPath, path ) )
		{
			*count = g_HudTierLists[i].iCount;
			return g_HudTierLists[i].pList;
		}
	}
	int listCount = 0;
	client_sprite_t* list = gEngfuncs.pfnSPR_GetList( (char*)path, &listCount );
	if ( !list || listCount <= 0 )
		return NULL;
	if ( g_iHudTierLists < (int)( sizeof( g_HudTierLists ) / sizeof( g_HudTierLists[0] ) ) && strlen( path ) < sizeof( g_HudTierLists[0].szPath ) )
	{
		hudtierlist_t& entry = g_HudTierLists[g_iHudTierLists++];
		strcpy( entry.szPath, path );
		entry.pList = list;
		entry.iCount = listCount;
	}
	*count = listCount;
	return list;
}

// A hi-res record is usable when it is exactly scale x the 640 record's size and lies inside a valid sprite.
bool CHud::CheckTierRecord( const client_sprite_t* tierRecord, const client_sprite_t* baseRecord, int scale, char* why, int whyLength )
{
	const wrect_t& t = tierRecord->rc;
	const wrect_t& b = baseRecord->rc;
	if ( t.right - t.left != scale * ( b.right - b.left ) || t.bottom - t.top != scale * ( b.bottom - b.top ) )
	{
		snprintf( why, whyLength, "%s is %dx%d, expected %dx%d", tierRecord->szName, t.right - t.left, t.bottom - t.top,
			scale * ( b.right - b.left ), scale * ( b.bottom - b.top ) );
		return false;
	}
	char path[128];
	const int pathLength = snprintf( path, sizeof( path ), "sprites/%s.spr", tierRecord->szSprite );
	int width = 0, height = 0;
	if ( pathLength <= 0 || pathLength >= 64 /* engine model name length */ || !HUD_ProbeSprite( path, &width, &height ) )
	{
		snprintf( why, whyLength, "%s missing or invalid", path );
		return false;
	}
	if ( t.left < 0 || t.top < 0 || t.left >= t.right || t.top >= t.bottom || t.right > width || t.bottom > height )
	{
		snprintf( why, whyLength, "%s rect outside %s", tierRecord->szName, path );
		return false;
	}
	return true;
}

int CHud::GetRequestedHudTier( bool* forced )
{
	const int cvarTier = m_pCvarHudTier ? (int)m_pCvarHudTier->value : 0;
	*forced = cvarTier == 640 || HUD_TierScale( cvarTier ) > 1;
	if ( *forced )
		return cvarTier;

	// by the screen, where Half-Life's own client changes its HUD sprites (CHud::VidInit): above 2560x1600 the biggest
	// (Half-Life's is its 2560 one; here 3840: there is no 2560 tier), 1280 from 1280 wide and above 720 tall, else 640
	if ( ScreenWidth > 2560 && ScreenHeight > 1600 )
		return 3840;
	if ( ScreenWidth >= 1280 && ScreenHeight > 720 )
		return 1280;
	return 640;
}

void CHud::AddHudTierFailure( const char* reason )
{
	m_iHudTierFailures++;
	for ( int i = 0; i < m_iHudTierReasons; i++ )
	{
		if ( !strcmp( m_szHudTierReasons[i], reason ) )
			return;
	}
	if ( m_iHudTierReasons < MAX_HUD_TIER_REASONS )
	{
		strncpy( m_szHudTierReasons[m_iHudTierReasons], reason, sizeof( m_szHudTierReasons[0] ) - 1 );
		m_szHudTierReasons[m_iHudTierReasons][sizeof( m_szHudTierReasons[0] ) - 1] = 0;
		m_iHudTierReasons++;
	}
}

void CHud::GetHudTierReasons( char* buffer, int size )
{
	int used = 0;
	buffer[0] = 0;
	for ( int i = 0; i < m_iHudTierReasons && used < size - 1; i++ )
	{
		const int n = snprintf( buffer + used, size - used, "%s%s", i ? "; " : "", m_szHudTierReasons[i] );
		if ( n < 0 )
			break;
		used += n;
	}
	if ( m_iHudTierFailures > m_iHudTierReasons && used < size - 1 )
		snprintf( buffer + used, size - used, "; %d more", m_iHudTierFailures - m_iHudTierReasons );
}

// Every 640 name must resolve at the tier, or the whole HUD stays at 640 (a mixed HUD would break the layout).
bool CHud::ValidateHudTier( int tier, int scale, client_sprite_t** overlay, int* overlayCount )
{
	char path[64];
	snprintf( path, sizeof( path ), "sprites/cs%d/hud.txt", tier );
	*overlay = GetTierSpriteList( path, overlayCount );
	if ( !*overlay )
	{
		char why[96];
		snprintf( why, sizeof( why ), "%s missing", path );
		AddHudTierFailure( why );
		return false;
	}

	bool valid = true;
	for ( int i = 0; i < m_iSpriteCountAllRes; i++ )
	{
		const client_sprite_t& base = m_pSpriteList[i];
		if ( base.iRes != 640 )
			continue;
		const client_sprite_t* record = GetSpriteList( *overlay, base.szName, tier, *overlayCount );
		char why[96];
		if ( !record )
			snprintf( why, sizeof( why ), "no %s record", base.szName );
		else if ( CheckTierRecord( record, &base, scale, why, sizeof( why ) ) )
			continue;
		AddHudTierFailure( why );
		valid = false;
	}
	return valid;
}

// Builds the sprite table from the stock baseRes block (names, order and indices as stock), taking each record's
// sprite and rect from the overlay when tier != baseRes. Rebuilt on every VidInit.
bool CHud::LoadSpriteTable( int baseRes, int tier, client_sprite_t* overlay, int overlayCount )
{
	int count = 0;
	for ( int i = 0; i < m_iSpriteCountAllRes; i++ )
	{
		if ( m_pSpriteList[i].iRes == baseRes )
			count++;
	}
	if ( count > m_iSpriteCapacity )
	{
		delete[] m_rghSprites;
		delete[] m_rgrcRects;
		delete[] m_rgszSpriteNames;
		m_rghSprites = new HSPRITE[count];
		m_rgrcRects = new wrect_t[count];
		m_rgszSpriteNames = new char[count * MAX_SPRITE_NAME_LENGTH];
		m_iSpriteCapacity = count;
	}

	m_iSpriteCount = 0;
	char spriteName[256];
	int index = 0;
	for ( int i = 0; i < m_iSpriteCountAllRes; i++ )
	{
		const client_sprite_t& base = m_pSpriteList[i];
		if ( base.iRes != baseRes )
			continue;
		const client_sprite_t* sprite = &base;
		if ( tier != baseRes )
		{
			const client_sprite_t* record = GetSpriteList( overlay, base.szName, tier, overlayCount );
			if ( record ) // always found: ValidateHudTier accepted every name
				sprite = record;
		}
		snprintf( spriteName, sizeof( spriteName ), "sprites/%s.spr", sprite->szSprite );
		m_rghSprites[index] = SPR_Load( spriteName );
		m_rgrcRects[index] = sprite->rc;
		strncpy( m_rgszSpriteNames + index * MAX_SPRITE_NAME_LENGTH, base.szName, MAX_SPRITE_NAME_LENGTH );
		index++;
	}
	m_iSpriteCount = index;
	return index > 0;
}

void CHud::SetupHudTier()
{
	m_iHudTierScreenWidth = ScreenWidth;
	m_iHudTierScreenHeight = ScreenHeight;
	m_iHudTierRequested = GetRequestedHudTier( &m_bHudTierForced );
	m_iHudTierFailures = 0;
	m_iHudTierReasons = 0;

	int tier = m_iHudTierRequested;
	client_sprite_t* overlay = NULL;
	int overlayCount = 0;
	if ( tier > 640 && !ValidateHudTier( tier, HUD_TierScale( tier ), &overlay, &overlayCount ) )
	{
		char reasons[320];
		GetHudTierReasons( reasons, sizeof( reasons ) );
		gEngfuncs.Con_Printf( "HUD: hi-res tier %d unavailable (%s) - using 640\n", tier, reasons );
		tier = 640;
	}
	m_iRes = tier;
	m_iHudScale = HUD_TierScale( tier );
	LoadSpriteTable( tier > 640 ? 640 : tier, tier, overlay, overlayCount );

	// weapons whose icons stay at 640 (custom server weapons) get the 640 selection frame in the weapon menu
	m_hsprSelection640 = 0;
	memset( &m_rcSelection640, 0, sizeof( m_rcSelection640 ) );
	if ( m_iRes > 640 )
	{
		client_sprite_t* selection = GetSpriteList( m_pSpriteList, "selection", 640, m_iSpriteCountAllRes );
		if ( selection )
		{
			char spriteName[128];
			snprintf( spriteName, sizeof( spriteName ), "sprites/%s.spr", selection->szSprite );
			m_hsprSelection640 = SPR_Load( spriteName );
			m_rcSelection640 = selection->rc;
		}
	}
}

void CHud::PrintHudTierInfo()
{
	gEngfuncs.Con_Printf( "HUD tier %d (scale %d) at %dx%d, requested %d (%s)\n", m_iRes, m_iHudScale,
		m_iHudTierScreenWidth, m_iHudTierScreenHeight, m_iHudTierRequested, m_bHudTierForced ? "hud_tier" : "auto" );
	if ( m_iHudTierRequested > 640 && m_iRes != m_iHudTierRequested )
	{
		char reasons[320];
		GetHudTierReasons( reasons, sizeof( reasons ) );
		gEngfuncs.Con_Printf( "hi-res HUD art unavailable, %d problem(s): %s\n", m_iHudTierFailures, reasons );
	}
	if ( m_iRes > 640 )
	{
		int known = 0, atTier = 0;
		CountWeaponIconTiers( m_iRes, &known, &atTier );
		gEngfuncs.Con_Printf( "hi-res HUD art OK (sprites/cs%d); weapon icons at %d: %d of %d known weapons\n", m_iRes, m_iRes, atTier, known );
	}
	gEngfuncs.Con_Printf( "hud_tier 0 = auto (3840 above 2560x1600, 1280 from 1280x721, else 640), 640 / 1280 / 3840 = force; applied on the next map load\n" );
}

void CHud::DrawHudSprite( int index, int x, int y, int r, int g, int b )
{
	const HSPRITE sprite = GetSprite( index );
	if ( !sprite )
		return;
	SPR_Set( sprite, r, g, b );
	SPR_DrawAdditive( 0, x, y, &GetSpriteRect( index ) );
}

void COM_FileBase( const char* input, char* output )
{
	int length = strlen( input );
	int end = length - 1;
	while ( end && input[end] != '.' && input[end] != '/' && input[end] != '\\' )
		end--;
	if ( input[end] != '.' )
		end = length - 1;
	else
		end--;
	int start = length - 1;
	while ( start >= 0 && input[start] != '/' && input[start] != '\\' )
		start--;
	if ( input[start] != '/' && input[start] != '\\' )
		start = 0;
	else
		start++;
	length = end - start + 1;
	strncpy( output, input + start, length );
	output[length] = 0;
}

int CHud::IsGame( const char* game )
{
	const char* directory = gEngfuncs.pfnGetGameDirectory();
	if ( !directory || !*directory )
		return 0;
	char name[1024];
	COM_FileBase( directory, name );
	return !stricmp( name, game );
}

float HUD_GetFOV()
{
	if ( gEngfuncs.pDemoAPI->IsRecording() )
	{
		unsigned char buffer[100];
		*(float*)buffer = g_lastFOV;
		Demo_WriteBuffer( TYPE_ZOOM, sizeof( float ), buffer );
	}
	if ( gEngfuncs.pDemoAPI->IsPlayingback() )
		g_lastFOV = g_demozoom;
	return g_lastFOV;
}

// true while a weapon zoom narrows the view (SG552 / AUG scope, sniper scopes). The unzoomed FOV is never below 90
// unless default_fov is set lower, so anything under both is a zoom.
bool HUD_WeaponZoomed()
{
	const int fov = gHUD.m_iFOV;
	if ( fov <= 0 || fov >= 90 )
		return false;
	return !gHUD.default_fov || fov < (int)gHUD.default_fov->value;
}

int CHud::MsgFunc_SetFOV( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	int fov = READ_BYTE();
	int defaultFOV = (int)default_fov->value;
	if ( !IsGame( "tfc" ) || !cl_lw || cl_lw->value == 0 )
	{
		g_lastFOV = (float)fov;
		m_iFOV = fov ? fov : defaultFOV;
		if ( !fov || fov == defaultFOV )
		{
			// reset to saved sensitivity
			m_flMouseSensitivity = 0;
		}
		else
		{
			float scale = (float)fov / defaultFOV * sensitivity->value;
			m_flMouseSensitivity = gEngfuncs.pfnGetCvarFloat( "zoom_sensitivity_ratio" ) * scale;
		}
	}
	return 1;
}

int CHud::MsgFunc_HLTV( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	int player = READ_BYTE();
	int value = READ_BYTE();
	if ( !gEngfuncs.IsSpectateOnly() )
	{
		gEngfuncs.Con_DPrintf( "Warning! Received unexpected HLTV user message.\n" );
		return 1;
	}
	if ( value & 0x80 )
	{
		value &= 0x7f;
		if ( player )
			g_PlayerExtraInfo[player].health = value;
		else
		{
			for ( int i = 0; i < MAX_PLAYERS; i++ )
				g_PlayerExtraInfo[i].health = value;
		}
	}
	else
	{
		if ( !value )
		{
			value = (int)default_fov->value;
			if ( value < 90 )
				value = 90;
		}
		if ( player )
			m_PlayerFOV[player] = value;
		else
		{
			for ( int i = 0; i < MAX_PLAYERS; i++ )
				m_PlayerFOV[i] = value;
		}
	}
	return 1;
}

int CHud::MsgFunc_Logo( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );

	// update Train data
	m_iLogo = READ_BYTE();
	return 1;
}

float CHud::GetSensitivity()
{
	return m_flMouseSensitivity;
}

void CHud::VGUI2HudPrint( char* text, int x, int y, int r, int g, int b )
{
	m_VGUI2Print.VGUI2HudPrint( text, x, y, (float)r, (float)g, (float)b );
}

bool IsBombPlanted()
{
	return !gHUD.m_bShowTimer;
}
