// radar2.h - square map radar (cl_radar_square 1): map layout from overviews/<map>.txt, or built from the map's BSP
// when the map has no overview (then also saved to overviews/ for the spectator overview).
#pragma once

#define RADAR_MAP_OPACITY_DEFAULT "1"
#define RADAR_MAP_OPACITY_DEFAULT_VALUE 1.0f

enum Radar2BlipKind
{
	RADAR2_PLAYER,
	RADAR2_BOMB,
	RADAR2_HOSTAGE,
	RADAR2_DEAD, // where a teammate died
};

struct Radar2Blip
{
	int kind;
	float origin[3];
	float yaw;      // degrees, valid when hasYaw
	bool hasYaw;
	int team;       // 1 = T, 2 = CT (players)
	bool vip;
	bool carrier;   // carries the bomb (players)
	bool tracked;   // spectator-tracked / highlighted
	bool flash;     // radio / event flash is on this frame
	bool planted;   // bomb
	bool dead;      // hostage
	float fade;     // 0 = fully shown .. 1 = gone (a teammate's death mark fading out)
};

void Radar2_Init();
void Radar2_VidInit();
bool Radar2_Enabled();
// draws the radar at the top-left corner; returns the bottom edge (y) so the location text can go under it
int Radar2_Draw( const float origin[3], const float angles[3], const Radar2Blip* blips, int numBlips, float time );
void Radar2_GetRect( int* x, int* y, int* size );
// the Advanced window's preview: the radar as set (square map radar or the classic round one) filling a box 'box'
// pixels square at x, y, over a sample team on de_dust2
void Radar2_DrawPreview( int x, int y, int box, int screenWide, int screenTall, bool square, float size, float range,
	float mapAlpha, bool rotate, float time );
