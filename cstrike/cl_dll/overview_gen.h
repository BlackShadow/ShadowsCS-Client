//========= Shadows custom CS client =========================================
//
// overview_gen: builds a top-down floor layout of a GoldSrc map (BSP v30)
// straight from the BSP bytes, for the radar and for generating classic
// spectator overviews (overviews/<map>.bmp + .txt) when a map ships none.
//
// Pure standard C++: no engine headers, no globals, no exceptions escape.
//
//=============================================================================
#ifndef OVERVIEW_GEN_H
#define OVERVIEW_GEN_H
#pragma once

#include <vector>

#define OVERVIEW_NO_FLOOR (-1.0e30f)

// Size of the classic overview image (what the engine's spectator overview expects)
#define OVERVIEW_CLASSIC_W 1024
#define OVERVIEW_CLASSIC_H 768

// The viewer sees floors up to viewerZ + this (Overview_Render / Overview_RenderPicture with useViewer).
inline constexpr float OVERVIEW_VIEWER_SLACK = 40.0f;

struct OverviewLayers
{
	int w = 0, h = 0;            // grid size in cells
	float x0 = 0, y0 = 0;        // world X of column 0's west edge, world Y of row 0's NORTH edge
	                             // (row index grows southwards = -Y)
	float cell = 16;             // world units per cell (square)
	int layers = 0;              // L, floor layers per cell actually used (<= maxLayers)
	std::vector<float> z;        // w*h*L heights; cell (col,row) layer k at ((row*w)+col)*L + k;
	                             // per cell sorted DESCENDING, unused slots = OVERVIEW_NO_FLOOR
	std::vector<unsigned char> water; // w*h*L, 1 if that layer is a water surface
	float zmin = 0, zmax = 0;    // range of all floor heights
};

// Builds the floor layers. maxCells = target max(w,h) (e.g. 512), maxLayers e.g. 6.
// Returns false and writes a message into err on bad/unsupported data.
bool Overview_BuildLayers( const unsigned char* bsp, int len, int maxCells, int maxLayers, OverviewLayers& out, char* err, int errLen );

// RGBA (w*h*4, row 0 = north). If useViewer: per cell show the highest layer with z <= viewerZ + 40,
// else the lowest layer, drawn darker (it is above the viewer, e.g. a roof). If !useViewer: the highest layer.
void Overview_Render( const OverviewLayers& L, bool useViewer, float viewerZ, unsigned char* rgba );

// Classic CS overview for the engine's spectator overview: 1024x768 8-bit paletted .bmp file bytes
// (palette index 0 = pure green 0,255,0 = empty) plus the values for overviews/<map>.txt:
//   global { ZOOM <zoom> ORIGIN <origin[0]> <origin[1]> <origin[2]> ROTATED <rotated> }
//   layer  { IMAGE "overviews/<map>.bmp" HEIGHT <origin[2]> }
// zoom is rounded down to 0.01 and origin to whole units; the image is rendered with exactly
// these rounded values, so printing zoom with 2 decimals and origin as integers is lossless.
bool Overview_MakeClassic( const OverviewLayers& L, std::vector<unsigned char>& bmpFile, float& zoom, float origin[3], int& rotated );

// Classic overview image for GIVEN parameters: index1024x768 gets 1024*768 palette indices,
// row 0 = top of the image. Mapping (matches CHudSpectator::DrawOverviewLayer, aspect 4/3):
//   rotated=1: u (left->right) = world X from ox-4096/zoom to ox+4096/zoom,
//              v (top->bottom) = world Y from oy+3072/zoom down to oy-3072/zoom
//   rotated=0: u = world Y from oy+4096/zoom down to oy-4096/zoom,
//              v = world X from ox+3072/zoom down to ox-3072/zoom
void Overview_RenderClassic( const OverviewLayers& L, float zoom, const float origin[3], int rotated, std::vector<unsigned char>& index1024x768, unsigned char palette[256][3] );

// ---------------------------------------------------------------------------
// Picture: the map's real surfaces seen from straight above (its own textures and
// lightmaps), like the engine's dev_overview. Every pixel keeps the few surfaces
// stacked over it, so the radar can show the floor the viewer is on.
// ---------------------------------------------------------------------------
#include <cstdint>

#define OVERVIEW_PIC_NONE (-32768)

// Loads a game file by relative path (e.g. "cs_dust.wad"); true + data when found.
typedef bool ( *OverviewLoadFile )( const char* path, std::vector<unsigned char>& data, void* ctx );

struct OverviewPicture
{
	int w = 0, h = 0;            // pixels
	float x0 = 0, y0 = 0;        // world X of the west edge, world Y of the north edge (row 0)
	float unit = 4;              // world units per pixel
	int depth = 0;               // surfaces kept per pixel
	std::vector<int16_t> z;      // w*h*depth heights, highest first, OVERVIEW_PIC_NONE = unused slot
	std::vector<uint16_t> rgb;   // same layout, RGB565 as seen from above (texture x light)
	std::vector<unsigned char> reach; // w*h bit masks: surface k can be walked to from the spawns
	bool reachValid = false;     // false: reachability failed, every surface counts
	int missingTextures = 0;     // used textures not found in the map or its WADs (drawn grey)
	int wadsLoaded = 0, wadsMissing = 0;
};

// Builds the picture over the area of L (from Overview_BuildLayers on the same BSP).
// maxPixels = max(w,h). loader may be null (then only textures stored in the BSP are used).
bool Overview_BuildPicture( const unsigned char* bsp, int len, const OverviewLayers& L, int maxPixels,
	OverviewLoadFile loader, void* ctx, OverviewPicture& out, char* err, int errLen );

// RGBA (w*h*4, row 0 = north). useViewer: per pixel the walkable surface the viewer is on or under
// (highest at or below viewerZ + 40, else the lowest, darker); otherwise the highest walkable one.
// Surfaces nobody can walk on (roofs, wall tops) are drawn faint.
void Overview_RenderPicture( const OverviewPicture& P, bool useViewer, float viewerZ, unsigned char* rgba );

// Classic spectator overview (1024x768 8-bit .bmp, green = empty) made from the picture.
bool Overview_MakeClassicPicture( const OverviewPicture& P, std::vector<unsigned char>& bmpFile, float& zoom, float origin[3], int& rotated );

#endif // OVERVIEW_GEN_H
