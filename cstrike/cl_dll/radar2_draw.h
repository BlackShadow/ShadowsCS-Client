// radar2_draw.h - drawing of the radars with plain OpenGL, no engine headers: used by the HUD (radar2.cpp) and by the
// Advanced window's radar preview, and runnable in an offline test.
#pragma once

#include "radar2.h"

// an uploaded GL texture and the size of the picture in it
struct Radar2Picture
{
	unsigned int texture;	// GL name, 0 = none
	int w, h;				// the picture
	int texW, texH;			// the texture (the picture sits at its top left)
};

// the square map radar (cl_radar_square 1)
struct Radar2Square
{
	float x0, y0, size;			// the radar square, screen pixels (origin top left)
	float unit;					// the HUD pixel unit: sizes of marks, arrows and the frame
	float range;				// world units from the centre to the edge
	float mapAlpha;				// map opacity, 0..1
	bool rotate;				// the map turns with the view
	float fovDegrees;			// the view cone
	int screenWide, screenTall;
	float clip[4];				// x, y, w, h: nothing drawn outside it (w <= 0: no limit)
	const float* origin;		// the viewer
	const float* angles;
	const Radar2Blip* blips;
	int numBlips;
	float time;
	Radar2Picture map;			// the layout picture (premultiplied alpha)
	float mapCorner[4][2];		// world X / Y of the picture's corners (u, v) = (0, 0), (1, 0), (1, 1), (0, 1)
	Radar2Picture c4;			// HUD icons (premultiplied); texture 0 = simple shapes instead
	Radar2Picture hostage;
};
void Radar2_DrawSquare( const Radar2Square& radar );

// the classic round radar as the HUD draws it (health.cpp DrawRadar): the radar picture and teammates as dots
struct Radar2Classic
{
	float x0, y0;				// top left of the radar picture
	float unit;					// the HUD pixel unit (the picture is 128 units across)
	int screenWide, screenTall;
	float clip[4];
	const float* origin;
	const float* angles;
	const Radar2Blip* blips;	// teammates (RADAR2_PLAYER) and where they are; others are ignored
	int numBlips;
	bool viewerIsT;				// the bomb carrier's dot is red for Terrorists
	Radar2Picture sprite;		// the radar picture: additive (or with holes when opaque); texture 0 = a drawn disc
	bool opaque;				// cl_radartype 1: the solid variant
};
void Radar2_DrawClassic( const Radar2Classic& radar );

// the Advanced window's radar preview: just the radar, filling the square box, over a sample Terrorist team in
// de_dust2's mid (positions on the 1024 x 768 overview picture)
struct Radar2Preview
{
	int x, y, box;				// the box, screen pixels
	int screenWide, screenTall;
	bool square;				// square map radar, else the classic round one
	float size, range, mapAlpha;
	bool rotate;
	bool opaque;				// classic: cl_radartype 1
	float time;
	Radar2Picture map;			// de_dust2's overview
	float mapCorner[4][2];
	Radar2Picture c4, hostage;
	Radar2Picture round, roundOpaque;	// the round radar pictures
};
void Radar2_DrawPreviewBox( const Radar2Preview& preview );
