// menu_background.cpp - see menu_background.h.
//
// How the tiles are found: GameUI (HL25-Engine/GameUI/BasePanel.cpp, CBasePanel::ApplySchemeSettings) reads the layout
// and makes one texture per tile, one after the other, so their texture ids follow each other; each is sized like its
// picture file. A run of ids sized exactly like the layout's pictures in order is a set of the menu's background tiles
// (nothing else has that pattern). The tiles' pixels are then replaced (DrawSetTextureRGBA, any size); GameUI keeps
// drawing them where it always does (CBasePanel::DrawBackgroundImage, using the sizes it measured when it loaded them),
// so each tile gets the part of the picture that falls under it, at the screen's resolution.
//
// Each time GameUI applies its scheme again it makes a new set, adds it to its list (old sets are never freed) and draws
// the whole list in order, so the newest set covers the others: that one is filled. Sets are looked for four times a
// second while the menu background is on screen (main menu and loading screen: the menu layer is up and no map is
// loaded - CBasePanel::PaintBackground draws the tiles only then). A new set gets the shown picture's soft (fading-size)
// tiles at once and the sharp ones as soon as they are cut. A set whose tiles are back to the size of GameUI's own
// pictures has them back and is filled again (our tiles are never exactly that size); a set whose tiles are neither ours
// nor GameUI's is no longer the menu's and is left alone. No game file is changed.
//
// Before anything is drawn: the menu applies its scheme to all panels (that is when it makes a set), then lets them all
// think, then draws (BaseUISurface::SolveTraverse, then PaintTraverse). A small panel of ours under the engine's menu
// panel does the work in its think, so a set made in this frame is filled before it is first drawn; it also looks over
// the texture ids just above the newest ones each frame (where new sets land). HUD_Frame does the work too (before the
// frame's drawing) and starts the first picture early.
//
// Reading and cutting a picture is done on a worker thread so the menu does not stall. While the game starts, nothing
// of ours has been shown yet: the first set waits for the picture's soft tiles (a few hundredths of a second), so the
// menu never draws its own; the sharp ones follow. A change of picture cross-fades over 1.5 seconds (reduced
// resolution while it fades, then the sharp tiles).
#include "hud.h"
#include "cl_util.h"
#include "vgui_controls/Controls.h"
#include "Panel.h"
#include "vgui/ISurface.h"
#include "vgui/IPanel.h"
#include "menu_background.h"
#include "../stretch_view.h"
#include "../vismods.h"
#include "../picture_file.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace
{

const int kHighestTextureId = 65536;	// texture ids are the engine's GL texture names
const double kSwitchSeconds = 60.0;		// "auto": a minute each
// the pictures (cl_menubackground names them; gfx/shadows/menubg/<name>_169.png and _43.png - PNG files, a third the
// size of the TGAs they were; a TGA of the same name is read when there is no PNG), "auto" goes round them
const char* const kPictureNames[] = { "original", "beta", "retail" };
const int kNumPictures = (int)( sizeof( kPictureNames ) / sizeof( kPictureNames[0] ) );
const double kFadeSeconds = 1.5;
const double kScanSeconds = 0.25;
const double kGiveUpSeconds = 15.0;		// no set of tiles by then: say so, stop looking every frame
const int kMaxTileSide = 4096;
const double kFadePixels = 1.6e6;		// at most this many pixels uploaded per frame while fading
const int kWindowIds = 4096;			// looked over every frame above the highest picture id seen
const int kFirstWaitMs = 1500;			// longest wait for the first picture

cvar_t* cl_menubackground;

struct Tile
{
	int x, y;			// in the layout's units
	int wide, tall;		// its picture's size, as GameUI measured it
	bool scaled, fit;	// how GameUI places it
};

struct Layout
{
	int baseWide = 0, baseTall = 0;
	std::vector<Tile> tiles;
};

// where a tile lands on the screen and the sizes its pixels are made at
struct TileRect
{
	int x0, y0, x1, y1;
	int wide, tall;			// sharp: one texel per screen pixel
	int fadeWide, fadeTall;	// while fading, and for a moment in a new set
};

// a picture cut into tiles for one screen size, made on the worker thread
struct Job
{
	std::vector<unsigned char> file;	// the picture file, read on the main thread (the engine's file system is not thread safe)
	int picture = 0;
	int screenWide = 0, screenTall = 0;
	bool instant = false;				// no fade: straight to the sharp tiles
	std::vector<TileRect> rects;
	std::vector<std::vector<unsigned char>> sharp, faded;
	bool ok = false;
	std::atomic<bool> fadedDone{ false };	// the soft tiles are cut (the worker no longer touches them)
	std::atomic<bool> done{ false };
};

// a set of the menu's tiles
struct Run
{
	int first = 0;						// texture id of its first tile
	std::vector<unsigned int> sizes;	// what we last put in each tile (packed size), 0 = nothing yet
	int found = 0;						// the scan that found it: the sets found last are drawn on top
	bool stock = true;					// shows GameUI's own picture
};

Layout g_layout;
bool g_bLayoutRead;
std::vector<Run> g_runs;
int g_iScans;
int g_iShownPicture = -1;		// in the tiles now: a kPictureNames index, -1 none yet
int g_iShownWide, g_iShownTall;	// the screen size the tiles were made for
std::vector<std::vector<unsigned char>> g_shownFaded;	// the shown picture's soft tiles (to fade from, to fill new sets)
std::vector<TileRect> g_shownRects;
std::shared_ptr<Job> g_ready;	// cut before any set of tiles turned up: shown when the first does
double g_flNextScan;
double g_flFirstLook = -1.0;
bool g_bGaveUp;
std::shared_ptr<Job> g_job;		// being cut
std::thread* g_worker;			// a pointer: a joinable std::thread destroyed when the DLL unloads would end the game
std::shared_ptr<Job> g_fadeTo;	// fading towards
double g_flFadeStart;
bool g_bPictureFailed[kNumPictures];		// its file could not be read (not tried again until the setting changes)
std::string g_lastMode;
bool g_bWantSharp;				// the top set has soft tiles or GameUI's own: cut the sharp ones
int g_iTopId;					// the highest texture id seen holding a picture

unsigned int Pack( int wide, int tall )
{
	return ( (unsigned int)( wide & 0xffff ) << 16 ) | (unsigned int)( tall & 0xffff );
}

// a TGA's size from its header
bool TgaSize( const char* path, int* wide, int* tall )
{
	int length = 0;
	unsigned char* file = (unsigned char*)gEngfuncs.COM_LoadFile( (char*)path, 5, &length );
	if ( !file )
		return false;
	bool ok = length >= 18;
	if ( ok )
	{
		*wide = file[12] | ( file[13] << 8 );
		*tall = file[14] | ( file[15] << 8 );
		ok = *wide > 0 && *tall > 0;
	}
	gEngfuncs.COM_FreeFile( file );
	return ok;
}

// liblist.gam's hd_background "1" makes GameUI read resource/HD_BackgroundLayout.txt (HL25-Engine/GameUI/ModInfo.cpp)
bool HdBackground()
{
	int length = 0;
	char* file = (char*)gEngfuncs.COM_LoadFile( (char*)"liblist.gam", 5, &length );
	if ( !file )
		return false;
	std::string text( file, length > 0 ? length : 0 );
	gEngfuncs.COM_FreeFile( file );
	bool hd = false;
	char* cursor = &text[0];
	char token[1024];
	while ( cursor && *cursor )
	{
		cursor = gEngfuncs.COM_ParseFile( cursor, token );
		if ( !cursor || !token[0] )
			break;
		if ( !stricmp( token, "hd_background" ) )
		{
			cursor = gEngfuncs.COM_ParseFile( cursor, token );
			hd = cursor && !stricmp( token, "1" );
		}
	}
	return hd;
}

// the layout as GameUI reads it: "resolution <w> <h>", then "<picture> <scaled|fit|-> <x> <y>"
void ReadLayout()
{
	g_bLayoutRead = true;
	g_layout = Layout();
	int length = 0;
	const char* path = HdBackground() ? "resource/HD_BackgroundLayout.txt" : "resource/BackgroundLayout.txt";
	char* file = (char*)gEngfuncs.COM_LoadFile( (char*)path, 5, &length );
	if ( !file )
		return;
	std::string text( file, length > 0 ? length : 0 );
	gEngfuncs.COM_FreeFile( file );
	char* cursor = &text[0];
	char token[1024];
	while ( cursor && *cursor )
	{
		cursor = gEngfuncs.COM_ParseFile( cursor, token );
		if ( !cursor || !token[0] )
			break;
		if ( !stricmp( token, "resolution" ) )
		{
			cursor = gEngfuncs.COM_ParseFile( cursor, token );
			g_layout.baseWide = atoi( token );
			cursor = cursor ? gEngfuncs.COM_ParseFile( cursor, token ) : NULL;
			g_layout.baseTall = cursor ? atoi( token ) : 0;
			continue;
		}
		Tile tile = {};
		const std::string picture = token;
		cursor = gEngfuncs.COM_ParseFile( cursor, token );
		tile.scaled = !stricmp( token, "scaled" );
		tile.fit = !stricmp( token, "fit" );
		cursor = cursor ? gEngfuncs.COM_ParseFile( cursor, token ) : NULL;
		tile.x = cursor ? atoi( token ) : 0;
		cursor = cursor ? gEngfuncs.COM_ParseFile( cursor, token ) : NULL;
		tile.y = cursor ? atoi( token ) : 0;
		if ( !TgaSize( picture.c_str(), &tile.wide, &tile.tall ) )
			tile.wide = tile.tall = -1;	// cannot match anything
		g_layout.tiles.push_back( tile );
	}
	if ( g_layout.baseWide <= 0 || g_layout.baseTall <= 0 )
		g_layout.tiles.clear();
}

int TopFound()
{
	int top = 0;
	for ( const Run& run : g_runs )
		top = run.found > top ? run.found : top;
	return top;
}

// does a set on top show GameUI's own picture?
bool TopNeedsFill()
{
	const int top = TopFound();
	for ( const Run& run : g_runs )
	{
		if ( run.found == top && run.stock )
			return true;
	}
	return false;
}

// looks over the textures - all of them, or just the ids above the highest picture id seen (where new sets land) - and
// adds new sets; a full look also drops sets that are no longer the menu's and marks the ones showing GameUI's own
// picture
void ScanRuns( bool full )
{
	const int count = (int)g_layout.tiles.size();
	if ( count == 0 )
		return;
	static std::vector<unsigned int> sizes( kHighestTextureId, 0 );
	const int lo = full ? 1 : ( g_iTopId - count > 1 ? g_iTopId - count : 1 );
	const int hi = full || g_iTopId + kWindowIds > kHighestTextureId - 1 ? kHighestTextureId - 1 : g_iTopId + kWindowIds;
	int top = 0;
	for ( int id = lo; id <= hi; id++ )
	{
		int wide = 0, tall = 0;
		vgui2::surface()->DrawGetTextureSize( id, wide, tall );	// 0 x 0 for an id that holds no picture
		sizes[id] = wide > 0 && tall > 0 ? Pack( wide, tall ) : 0;
		if ( sizes[id] )
			top = id;
	}
	if ( full || top > g_iTopId )
		g_iTopId = top;
	std::vector<unsigned int> stock( count );
	for ( int t = 0; t < count; t++ )
	{
		const Tile& tile = g_layout.tiles[t];
		stock[t] = tile.wide > 0 && tile.tall > 0 ? Pack( tile.wide, tile.tall ) : 0xffffffffu;
	}
	g_iScans++;
	// the sets known: each tile holds what we put in it or GameUI's own picture, else the set is not the menu's any more
	for ( size_t r = 0; full && r < g_runs.size(); )
	{
		Run& run = g_runs[r];
		bool foreign = false, allStock = true;
		for ( int t = 0; t < count && !foreign; t++ )
		{
			const int id = run.first + t;
			const unsigned int size = id < kHighestTextureId ? sizes[id] : 0;
			const bool isStock = size == stock[t];
			foreign = size == 0 || ( !isStock && size != run.sizes[t] );
			allStock = allStock && isStock;
		}
		if ( foreign )
		{
			g_runs.erase( g_runs.begin() + r );
			continue;
		}
		if ( allStock )
			run.stock = true;
		r++;
	}
	// new sets: ids in a row sized like the layout's pictures
	for ( int first = lo; first + count - 1 <= hi; first++ )
	{
		bool match = true;
		for ( int t = 0; t < count && match; t++ )
			match = sizes[first + t] == stock[t];
		if ( !match )
			continue;
		bool known = false;
		for ( const Run& run : g_runs )
			known = known || run.first == first;
		if ( !known )
		{
			Run run;
			run.first = first;
			run.sizes.assign( count, 0 );
			run.found = g_iScans;
			g_runs.push_back( run );
		}
		first += count - 1;
	}
}

// a tile's size, never exactly that of GameUI's own picture for it (a tile of that size again has GameUI's picture back)
void AvoidStockSize( int& wide, int& tall, const Tile& tile )
{
	if ( wide == tile.wide && tall == tile.tall )
		wide += wide < kMaxTileSide ? 1 : -1;
}

// where GameUI draws each tile on a screen W x H (CBasePanel::DrawBackgroundImage, the same sums), and the pixel sizes
// the tiles are made at
std::vector<TileRect> TileRects( int W, int H )
{
	std::vector<TileRect> rects;
	const float xScale = (float)W / g_layout.baseWide;
	const float yScale = (float)H / g_layout.baseTall;
	float fitScale = yScale;
	if ( W > g_layout.baseWide * yScale )
		fitScale = xScale;
	const int fitOffsetX = W / 2 - (int)( g_layout.baseWide * fitScale * 0.5f );
	const int fitOffsetY = H / 2 - (int)( g_layout.baseTall * fitScale * 0.5f );
	double total = 0.0;
	for ( const Tile& tile : g_layout.tiles )
	{
		TileRect r;
		if ( tile.scaled )
		{
			r.x0 = tile.x ? (int)ceil( tile.x * xScale ) : 0;
			r.y0 = tile.y ? (int)ceil( tile.y * yScale ) : 0;
			r.x1 = (int)ceil( ( tile.x + tile.wide ) * xScale );
			r.y1 = (int)ceil( ( tile.y + tile.tall ) * yScale );
		}
		else if ( tile.fit )
		{
			r.x0 = fitOffsetX + ( tile.x ? (int)ceil( tile.x * fitScale ) : 0 );
			r.y0 = fitOffsetY + ( tile.y ? (int)ceil( tile.y * fitScale ) : 0 );
			r.x1 = r.x0 + (int)ceil( tile.wide * fitScale );
			r.y1 = r.y0 + (int)ceil( tile.tall * fitScale );
		}
		else
		{
			r.x0 = tile.x;
			r.y0 = tile.y;
			r.x1 = r.x0 + tile.wide;
			r.y1 = r.y0 + tile.tall;
		}
		if ( r.x1 <= r.x0 )
			r.x1 = r.x0 + 1;
		if ( r.y1 <= r.y0 )
			r.y1 = r.y0 + 1;
		r.wide = r.x1 - r.x0 < kMaxTileSide ? r.x1 - r.x0 : kMaxTileSide;
		r.tall = r.y1 - r.y0 < kMaxTileSide ? r.y1 - r.y0 : kMaxTileSide;
		total += (double)r.wide * r.tall;
		rects.push_back( r );
	}
	// smaller while fading: at most kFadePixels in all
	int factor = 2;
	while ( total / ( (double)factor * factor ) > kFadePixels )
		factor++;
	for ( size_t t = 0; t < rects.size(); t++ )
	{
		TileRect& r = rects[t];
		r.fadeWide = r.wide / factor > 0 ? r.wide / factor : 1;
		r.fadeTall = r.tall / factor > 0 ? r.tall / factor : 1;
		AvoidStockSize( r.wide, r.tall, g_layout.tiles[t] );
		AvoidStockSize( r.fadeWide, r.fadeTall, g_layout.tiles[t] );
	}
	return rects;
}

// --- worker thread: only its own job's data ----------------------------------------------------------------------

// a truecolour TGA (plain or run-length packed, 24 or 32 bit) from memory to RGBA, top row first
bool DecodeTGA( const std::vector<unsigned char>& f, std::vector<unsigned char>& rgba, int& w, int& h )
{
	if ( f.size() < 18 )
		return false;
	const int idlen = f[0], cmtype = f[1], type = f[2];
	const int cmlen = f[5] | ( f[6] << 8 ), cmbits = f[7];
	w = f[12] | ( f[13] << 8 );
	h = f[14] | ( f[15] << 8 );
	const int bpp = f[16], desc = f[17];
	const int psize = bpp / 8;
	if ( ( type != 2 && type != 10 ) || ( psize != 3 && psize != 4 ) || w <= 0 || h <= 0 || w > 8192 || h > 8192 )
		return false;
	size_t pos = 18 + idlen + ( cmtype == 1 ? (size_t)cmlen * ( ( cmbits + 7 ) / 8 ) : 0 );
	rgba.assign( (size_t)w * h * 4, 255 );
	const bool topFirst = ( desc & 0x20 ) != 0;
	const size_t total = (size_t)w * h;
	size_t i = 0, x = 0, fileRow = 0;
	unsigned char* row = &rgba[( topFirst ? 0 : (size_t)( h - 1 ) ) * w * 4];
	auto put = [&]( const unsigned char* s ) {
		unsigned char* o = row + x * 4;
		o[0] = s[2];
		o[1] = s[1];
		o[2] = s[0];
		i++;
		if ( ++x == (size_t)w )
		{
			x = 0;
			if ( ++fileRow < (size_t)h )
				row = &rgba[( topFirst ? fileRow : h - 1 - fileRow ) * w * 4];
		}
	};
	while ( i < total )
	{
		if ( type == 2 )
		{
			if ( pos + psize > f.size() )
				return false;
			put( &f[pos] );
			pos += psize;
			continue;
		}
		if ( pos >= f.size() )
			return false;
		const int head = f[pos++];
		const int count = ( head & 0x7f ) + 1;
		if ( head & 0x80 )
		{
			if ( pos + psize > f.size() )
				return false;
			for ( int k = 0; k < count && i < total; k++ )
				put( &f[pos] );
			pos += psize;
		}
		else
		{
			for ( int k = 0; k < count && i < total; k++ )
			{
				if ( pos + psize > f.size() )
					return false;
				put( &f[pos] );
				pos += psize;
			}
		}
	}
	return true;
}

// the picture smaller by a whole factor (averaged), so the smooth sampling below does not shimmer when it shrinks a lot
void Shrink( std::vector<unsigned char>& image, int& wide, int& tall, int factor )
{
	const int newWide = wide / factor, newTall = tall / factor;
	if ( newWide < 1 || newTall < 1 )
		return;
	std::vector<unsigned char> shrunk( (size_t)newWide * newTall * 4 );
	for ( int y = 0; y < newTall; y++ )
	{
		for ( int x = 0; x < newWide; x++ )
		{
			unsigned int sum[3] = { 0, 0, 0 };
			for ( int dy = 0; dy < factor; dy++ )
			{
				const unsigned char* p = &image[( (size_t)( y * factor + dy ) * wide + x * factor ) * 4];
				for ( int dx = 0; dx < factor; dx++, p += 4 )
				{
					sum[0] += p[0];
					sum[1] += p[1];
					sum[2] += p[2];
				}
			}
			unsigned char* o = &shrunk[( (size_t)y * newWide + x ) * 4];
			const unsigned int n = (unsigned int)( factor * factor );
			o[0] = (unsigned char)( ( sum[0] + n / 2 ) / n );
			o[1] = (unsigned char)( ( sum[1] + n / 2 ) / n );
			o[2] = (unsigned char)( ( sum[2] + n / 2 ) / n );
			o[3] = 255;
		}
	}
	image.swap( shrunk );
	wide = newWide;
	tall = newTall;
}

// one tile's pixels: the part of the picture under screen pixels x0..x1, y0..y1, the picture drawn at offX, offY at
// 'scale' screen pixels per picture pixel (smoothly sampled; past its edges the edge colour)
void BuildTile( const std::vector<unsigned char>& image, int iw, int ih, double scale, double offX, double offY, int x0, int y0,
	int x1, int y1, int tileWide, int tileTall, std::vector<unsigned char>& out )
{
	out.resize( (size_t)tileWide * tileTall * 4 );
	const double stepX = (double)( x1 - x0 ) / tileWide, stepY = (double)( y1 - y0 ) / tileTall;
	for ( int ty = 0; ty < tileTall; ty++ )
	{
		double v = ( y0 + ( ty + 0.5 ) * stepY - offY ) / scale - 0.5;
		v = v < 0.0 ? 0.0 : v > ih - 1 ? ih - 1 : v;
		const int vy0 = (int)v, vy1 = vy0 + 1 < ih ? vy0 + 1 : vy0;
		const float fy = (float)( v - vy0 );
		const unsigned char* row0 = &image[(size_t)vy0 * iw * 4];
		const unsigned char* row1 = &image[(size_t)vy1 * iw * 4];
		unsigned char* o = &out[(size_t)ty * tileWide * 4];
		for ( int tx = 0; tx < tileWide; tx++, o += 4 )
		{
			double u = ( x0 + ( tx + 0.5 ) * stepX - offX ) / scale - 0.5;
			u = u < 0.0 ? 0.0 : u > iw - 1 ? iw - 1 : u;
			const int ux0 = (int)u, ux1 = ux0 + 1 < iw ? ux0 + 1 : ux0;
			const float fx = (float)( u - ux0 );
			const unsigned char *a = row0 + ux0 * 4, *b = row0 + ux1 * 4, *c = row1 + ux0 * 4, *d = row1 + ux1 * 4;
			for ( int k = 0; k < 3; k++ )
			{
				const float top = a[k] + ( b[k] - a[k] ) * fx, bottom = c[k] + ( d[k] - c[k] ) * fx;
				o[k] = (unsigned char)( top + ( bottom - top ) * fy + 0.5f );
			}
			o[3] = 255;
		}
	}
}

// the soft tiles first (they can go in while the sharp ones are cut)
void MakeTiles( std::shared_ptr<Job> job )
{
	std::vector<unsigned char> image;
	int iw = 0, ih = 0;
	if ( PictureFile_Decode( job->file.data(), job->file.size(), image, iw, ih ) || DecodeTGA( job->file, image, iw, ih ) )
	{
		job->file.clear();
		job->file.shrink_to_fit();
		const int W = job->screenWide, H = job->screenTall;
		// the picture covers the screen, centred
		double scale = (double)W / iw > (double)H / ih ? (double)W / iw : (double)H / ih;
		if ( scale < 0.6 )
		{
			const int factor = (int)( 1.0 / scale );
			Shrink( image, iw, ih, factor );
			scale *= factor;
		}
		const double offX = ( W - iw * scale ) * 0.5, offY = ( H - ih * scale ) * 0.5;
		const size_t count = job->rects.size();
		job->faded.resize( count );
		job->sharp.resize( count );
		for ( size_t t = 0; t < count; t++ )
		{
			const TileRect& r = job->rects[t];
			BuildTile( image, iw, ih, scale, offX, offY, r.x0, r.y0, r.x1, r.y1, r.fadeWide, r.fadeTall, job->faded[t] );
		}
		job->fadedDone = true;
		for ( size_t t = 0; t < count; t++ )
		{
			const TileRect& r = job->rects[t];
			BuildTile( image, iw, ih, scale, offX, offY, r.x0, r.y0, r.x1, r.y1, r.wide, r.tall, job->sharp[t] );
		}
		job->ok = true;
	}
	job->done = true;
}

// --- main thread ---------------------------------------------------------------------------------------------------

// the engine's panel holding the game's menu layer (main menu, loading screen, Esc menu)
vgui2::VPANEL MenuLayerPanel()
{
	const vgui2::VPANEL root = vgui2::surface()->GetEmbeddedPanel();
	if ( !root )
		return 0;
	const int count = vgui2::ipanel()->GetChildCount( root );
	for ( int i = 0; i < count; i++ )
	{
		const vgui2::VPANEL child = vgui2::ipanel()->GetChild( root, i );
		const char* name = child ? vgui2::ipanel()->GetName( child ) : NULL;
		if ( name && !strcmp( name, "BaseGameUIPanel" ) )
			return child;
	}
	return 0;
}

bool MenuLayerShown()
{
	const vgui2::VPANEL panel = MenuLayerPanel();
	return panel && vgui2::ipanel()->IsVisible( panel );
}

// these tiles into the sets on top (the newest: the ones on screen); afterwards the menu is told which texture is bound
// (uploading binds each texture behind its back, and it skips binding one it believes is already bound)
void Upload( const std::vector<std::vector<unsigned char>>& tiles, const std::vector<TileRect>& rects, bool faded )
{
	const size_t count = g_layout.tiles.size();
	if ( tiles.size() != count || rects.size() != count )
		return;
	for ( size_t t = 0; t < count; t++ )
	{
		const int wide = faded ? rects[t].fadeWide : rects[t].wide, tall = faded ? rects[t].fadeTall : rects[t].tall;
		if ( tiles[t].size() != (size_t)wide * tall * 4 )
			return;
	}
	const int top = TopFound();
	int lastId = 0;
	for ( Run& run : g_runs )
	{
		if ( run.found != top )
			continue;
		for ( size_t t = 0; t < count; t++ )
		{
			const int wide = faded ? rects[t].fadeWide : rects[t].wide, tall = faded ? rects[t].fadeTall : rects[t].tall;
			vgui2::surface()->DrawSetTextureRGBA( run.first + (int)t, tiles[t].data(), wide, tall, 1, true );
			run.sizes[t] = Pack( wide, tall );
			lastId = run.first + (int)t;
		}
		run.stock = false;
	}
	if ( lastId )
		vgui2::surface()->DrawSetTexture( lastId );
}

// a picture's sharp tiles into the sets on top: it is now the one shown
void Show( const std::shared_ptr<Job>& job )
{
	Upload( job->sharp, job->rects, false );
	g_shownFaded = std::move( job->faded );
	g_shownRects = job->rects;
	g_iShownPicture = job->picture;
	g_iShownWide = job->screenWide;
	g_iShownTall = job->screenTall;
	g_bWantSharp = false;
}

void JoinWorker()
{
	if ( g_worker )
	{
		g_worker->join();
		delete g_worker;
		g_worker = NULL;
	}
}

void StartJob( int picture, int W, int H, bool instant )
{
	const bool wide = (float)W / H > ( 4.0f / 3.0f + 16.0f / 9.0f ) * 0.5f;	// 16:9 from halfway between 4:3 and 16:9
	char path[128];
	int length = 0;
	unsigned char* file = NULL;
	static const char* const kKinds[] = { "png", "tga" };
	for ( int kind = 0; kind < 2 && !file; kind++ )
	{
		snprintf( path, sizeof( path ), "gfx/shadows/menubg/%s_%s.%s", kPictureNames[picture], wide ? "169" : "43", kKinds[kind] );
		file = (unsigned char*)gEngfuncs.COM_LoadFile( path, 5, &length );
	}
	if ( !file || length <= 18 )
	{
		if ( file )
			gEngfuncs.COM_FreeFile( file );
		if ( !g_bPictureFailed[picture] )
			gEngfuncs.Con_Printf( "Menu background: %s is missing\n", path );
		g_bPictureFailed[picture] = true;
		return;
	}
	std::shared_ptr<Job> job = std::make_shared<Job>();
	job->file.assign( file, file + length );
	gEngfuncs.COM_FreeFile( file );
	job->picture = picture;
	job->screenWide = W;
	job->screenTall = H;
	job->instant = instant;
	job->rects = TileRects( W, H );
	g_job = job;
	g_worker = new std::thread( MakeTiles, job );
}

// the finished job's tiles: straight in, kept for the first set, or the start of a fade from the shown picture
void TakeJob( int W, int H )
{
	JoinWorker();
	std::shared_ptr<Job> job = g_job;
	g_job.reset();
	if ( !job->ok )
	{
		if ( !g_bPictureFailed[job->picture] )
			gEngfuncs.Con_Printf( "Menu background: picture %d could not be read\n", job->picture );
		g_bPictureFailed[job->picture] = true;
		return;
	}
	if ( job->screenWide != W || job->screenTall != H )
		return;	// made for another screen size: a new one is started
	if ( g_runs.empty() )
	{
		if ( !g_bGaveUp )
			g_ready = job;	// shown when the first set of tiles turns up
		return;
	}
	bool canFade = !job->instant && g_iShownPicture >= 0 && g_iShownWide == W && g_iShownTall == H
		&& g_shownFaded.size() == job->faded.size() && !TopNeedsFill();
	for ( size_t t = 0; canFade && t < job->faded.size(); t++ )
		canFade = g_shownFaded[t].size() == job->faded[t].size();
	if ( canFade )
	{
		g_fadeTo = job;
		g_flFadeStart = gEngfuncs.GetAbsoluteTime();
		return;
	}
	Show( job );
}

// one step of the fade (eased); at the end the sharp tiles of the new picture
void StepFade()
{
	const double t = ( gEngfuncs.GetAbsoluteTime() - g_flFadeStart ) / kFadeSeconds;
	if ( t >= 1.0 || t < 0.0 )
	{
		std::shared_ptr<Job> job = g_fadeTo;
		g_fadeTo.reset();
		Show( job );
		return;
	}
	const double eased = t * t * ( 3.0 - 2.0 * t );
	const unsigned int k = (unsigned int)( eased * 256.0 + 0.5 ), inverse = 256 - k;
	const std::vector<TileRect>& rects = g_fadeTo->rects;
	static std::vector<std::vector<unsigned char>> blend;
	blend.resize( rects.size() );
	for ( size_t i = 0; i < rects.size(); i++ )
	{
		const std::vector<unsigned char>& from = g_shownFaded[i];
		const std::vector<unsigned char>& to = g_fadeTo->faded[i];
		blend[i].resize( to.size() );
		const size_t bytes = from.size() < to.size() ? from.size() : to.size();
		for ( size_t b = 0; b < bytes; b++ )
			blend[i][b] = (unsigned char)( ( from[b] * inverse + to[b] * k ) >> 8 );
	}
	Upload( blend, rects, true );
}

// nothing of ours has been shown yet (the game is starting): the picture's soft tiles are waited for (a few hundredths
// of a second) so the menu never draws its own background
void WaitForFirstPicture( int W, int H, int picture )
{
	if ( !g_job && !g_bPictureFailed[picture] )
		StartJob( picture, W, H, true );
	if ( !g_job )
		return;
	const std::chrono::steady_clock::time_point until =
		std::chrono::steady_clock::now() + std::chrono::milliseconds( kFirstWaitMs );
	while ( !g_job->fadedDone && !g_job->done && std::chrono::steady_clock::now() < until )
		std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
	if ( g_job->fadedDone )
		Upload( g_job->faded, g_job->rects, true );	// even if cut for another screen size: ours, not GameUI's
	else if ( g_job->done )
		TakeJob( W, H );
}

void Update( bool beforeDraw );

// is a map being played (the menu is then over the game, which the engine keeps drawing behind it)?
bool InGame()
{
	const char* level = gEngfuncs.pfnGetLevelName();
	return level && level[0] && gEngfuncs.GetLocalPlayer();
}

// a panel of our own under the engine's menu panel (see the top of the file), one pixel in the corner, no input, first
// in the menu's paint order. Over a game it also does the client's per-frame picture work that HUD_Redraw does while
// no menu is up - the engine leaves HUD_Redraw out while its menu is on screen: the options' per-frame part
// (vismods.cpp: the weapon models' draw animation, no sky) in its think, stretched 4:3 in its paint (the game's picture
// is drawn by then, the menu's widgets not yet).
class CMenuBackgroundHook : public vgui2::Panel
{
public:
	CMenuBackgroundHook() : vgui2::Panel( NULL, "ShadowsMenuBackground" )
	{
		SetBounds( 0, 0, 1, 1 );	// a panel of no size is not painted
		SetPaintEnabled( true );
		SetPaintBackgroundEnabled( false );
		SetPaintBorderEnabled( false );
	}
	void OnThink() override
	{
		Update( true );
		if ( InGame() )
			VisMods_Frame( (float)gEngfuncs.GetClientTime() );
	}
	void Paint() override
	{
		if ( !InGame() )
			return;
		vgui2::surface()->DrawFlushText();
		StretchView_Draw();
	}
};

CMenuBackgroundHook* g_pHook;

void MakeHook()
{
	const vgui2::VPANEL menu = g_pHook ? 0 : MenuLayerPanel();
	if ( !menu )
		return;
	g_pHook = new CMenuBackgroundHook();
	g_pHook->SetParent( menu );
	g_pHook->SetMouseInputEnabled( false );	// SetParent copies the parent's input settings
	g_pHook->SetKeyBoardInputEnabled( false );
	g_pHook->SetZPos( -100 );	// painted before the menu's own panels (lower first)
	g_pHook->SetVisible( true );
}

// beforeDraw: from the panel's think, after the menu's scheme was applied and before anything is drawn
void Update( bool beforeDraw )
{
	if ( !cl_menubackground || !vgui2::surface() || !vgui2::ipanel() )
		return;
	MakeHook();
	// only while the menu draws its background: menu layer up, no map loaded
	if ( !MenuLayerShown() )
		return;
	const char* level = gEngfuncs.pfnGetLevelName();
	if ( level && level[0] )
		return;
	if ( !g_bLayoutRead )
		ReadLayout();
	if ( g_layout.tiles.empty() )
		return;
	int W = 0, H = 0;
	vgui2::surface()->GetScreenSize( W, H );
	if ( W <= 0 || H <= 0 )
		return;
	const double now = gEngfuncs.GetAbsoluteTime();

	// sets of tiles: all ids every time until the first set turns up, then four times a second; in between, just above
	// the newest ids before each frame is drawn
	if ( ( g_runs.empty() && !g_bGaveUp ) || now >= g_flNextScan )
	{
		ScanRuns( true );
		g_flNextScan = now + kScanSeconds;
	}
	else if ( beforeDraw )
		ScanRuns( false );
	if ( g_runs.empty() && !g_bGaveUp )
	{
		if ( g_flFirstLook < 0.0 )
			g_flFirstLook = now;
		else if ( now - g_flFirstLook > kGiveUpSeconds )
		{
			g_bGaveUp = true;
			g_ready.reset();
			gEngfuncs.Con_Printf( "Menu background: the menu's background pictures were not found\n" );
		}
	}

	const char* mode = cl_menubackground->string ? cl_menubackground->string : "";
	if ( g_lastMode != mode )
	{
		g_lastMode = mode;
		for ( bool& failed : g_bPictureFailed )
			failed = false;
	}
	int picture = -1;
	for ( int i = 0; i < kNumPictures && picture < 0; i++ )
	{
		if ( !stricmp( mode, kPictureNames[i] ) )
			picture = i;
	}
	if ( picture < 0 )
		picture = (int)( now / kSwitchSeconds ) % kNumPictures;	// auto: a minute each

	if ( g_job && g_job->done )
		TakeJob( W, H );
	if ( g_ready && ( g_ready->screenWide != W || g_ready->screenTall != H ) )
		g_ready.reset();
	if ( g_fadeTo && ( g_fadeTo->screenWide != W || g_fadeTo->screenTall != H ) )
		g_fadeTo.reset();	// made for another screen size: the new size's picture comes in straight

	// a set on top shows GameUI's own picture (it is new, or got it back): ours at once
	if ( !g_runs.empty() && TopNeedsFill() )
	{
		if ( g_ready )
		{
			std::shared_ptr<Job> job = g_ready;
			g_ready.reset();
			Show( job );
		}
		else if ( g_job && g_job->instant && g_job->fadedDone )
			Upload( g_job->faded, g_job->rects, true );	// the soft tiles while the sharp ones are cut
		else if ( !g_fadeTo )
		{
			g_bWantSharp = true;
			if ( g_iShownPicture >= 0 )
				Upload( g_shownFaded, g_shownRects, true );	// soft for a moment (ours even if cut for another size)
			else
				WaitForFirstPicture( W, H, picture );
		}
		// while fading, its next step fills it
	}
	if ( g_fadeTo )
	{
		StepFade();
		return;
	}
	if ( g_job || g_ready )
		return;	// being cut, or waiting for the menu's tiles
	if ( g_runs.empty() && g_bGaveUp )
		return;
	const bool screenChanged = W != g_iShownWide || H != g_iShownTall;
	if ( g_iShownPicture < 0 || screenChanged || g_bWantSharp )
	{
		// straight in, no fade
		const int wanted = g_iShownPicture >= 0 && !screenChanged ? g_iShownPicture : picture;
		if ( !g_bPictureFailed[wanted] )
			StartJob( wanted, W, H, true );
		g_bWantSharp = false;
	}
	else if ( picture != g_iShownPicture && !g_bPictureFailed[picture] )
		StartJob( picture, W, H, false );
}

} // namespace

void MenuBackground_Init()
{
	cl_menubackground = gEngfuncs.pfnRegisterVariable( (char*)"cl_menubackground", (char*)"auto", FCVAR_ARCHIVE );
}

void MenuBackground_Shutdown()
{
	JoinWorker();
	g_job.reset();
	g_fadeTo.reset();
	g_ready.reset();
	g_shownFaded.clear();
}

// from HUD_Frame, before the frame is drawn (and the first picture is started early); the panel's think does the rest
void MenuBackground_Frame()
{
	Update( false );
}
