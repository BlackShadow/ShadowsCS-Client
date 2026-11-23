// vismods.cpp - client visual options
//
//   cl_nosky 1        the sky is drawn solid black. The engine's own sky textures (gfx/env/<sky><side>.tga) are found
//                     by content and blacked out in place; setting 0 puts the original pictures back.
//   cl_teamcolors 1   players (and their weapons and bodies) are drawn in one solid colour per team:
//                     cl_teamcolor_t / cl_teamcolor_ct / cl_teamcolor_vip, "r g b" in 0..255.
//   cl_headcolor      with cl_teamcolors 1: players' heads in one solid colour, "r g b" ("0" = off)
//   cl_weaponmodels   the weapon models: "original" (the game's), "retail" (CS 1.5's) or "beta" (beta 6.1's, made by
//                     tools/betaweapons)
//   cl_playermodels   the player models: "original" (the game's), "retail" (CS 1.0's) or "beta" (beta 6.1's, made by
//                     tools/betaweapons)
//   cl_overridemodels 1: a server's own player and weapon models are drawn as the game's (and so in the sets above)

#include "hud.h"
#include "cl_util.h"
#include "ammo.h"
#include "pm_movevars.h"
#include "com_model.h"
#include "cl_entity.h"
#include "const.h"
#include "studio.h"
#include "r_studioint.h"
#include "vismods.h"
#include "player_fit.h"
#include "picture_file.h"
#include "vgui_controls/Controls.h"
#include "FileSystem.h"

#include <ctype.h>
#include <map>
#include <string>

extern engine_studio_api_t IEngineStudio;
extern bool g_bHoldingShield;	// hl_weapons.cpp

// a file the game has, asked of its file system as the class and buy menus do (COM_ExpandFilename can say yes to a file
// that is not there)
static bool FileExists( const char* path )
{
	return vgui2::filesystem()->FileExists( path );
}

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <algorithm>

#ifdef _WIN32
#include <gl/GL.h>
#pragma comment( lib, "opengl32.lib" )
#else
#include <GL/gl.h>
#endif

#ifndef GL_COMBINE
#define GL_COMBINE 0x8570
#define GL_COMBINE_RGB 0x8571
#define GL_COMBINE_ALPHA 0x8572
#define GL_SOURCE0_RGB 0x8580
#define GL_SOURCE0_ALPHA 0x8588
#define GL_SOURCE1_ALPHA 0x8589
#define GL_OPERAND0_RGB 0x8590
#define GL_OPERAND0_ALPHA 0x8598
#define GL_OPERAND1_ALPHA 0x8599
#define GL_CONSTANT 0x8576
#define GL_PRIMARY_COLOR 0x8577
#define GL_PREVIOUS 0x8578
#endif
#ifndef GL_ACTIVE_TEXTURE
#define GL_ACTIVE_TEXTURE 0x84E0
#define GL_TEXTURE0 0x84C0
#endif
#ifndef GL_CURRENT_PROGRAM
#define GL_CURRENT_PROGRAM 0x8B8D
#endif
#ifndef GL_PIXEL_PACK_BUFFER_BINDING
#define GL_PIXEL_PACK_BUFFER_BINDING 0x88ED
#define GL_PIXEL_UNPACK_BUFFER_BINDING 0x88EF
#endif

static cvar_t* cl_nosky;
static cvar_t* cl_teamcolors;
static cvar_t* cl_teamcolor_t;
static cvar_t* cl_teamcolor_ct;
static cvar_t* cl_teamcolor_vip;
static cvar_t* cl_headcolor;
static cvar_t* cl_weaponmodels;
static cvar_t* cl_playermodels;
static cvar_t* cl_overridemodels;
static movevars_t* g_pMoveVars;

// ---------------------------------------------------------------------------------------------------------------
// GL helpers
// ---------------------------------------------------------------------------------------------------------------

typedef void( APIENTRY* PFNVMACTIVETEXTURE )( GLenum texture );

static PFNVMACTIVETEXTURE GetActiveTextureFunc()
{
	static bool tried = false;
	static PFNVMACTIVETEXTURE func = NULL;
	if ( !tried )
	{
		tried = true;
#ifdef _WIN32
		// some drivers return 1, 2, 3 or -1 instead of NULL for names they do not have
		const char* const names[2] = { "glActiveTexture", "glActiveTextureARB" };
		for ( int i = 0; i < 2 && !func; i++ )
		{
			PROC p = wglGetProcAddress( names[i] );
			const INT_PTR v = (INT_PTR)p;
			if ( v != 0 && v != 1 && v != 2 && v != 3 && v != -1 )
				func = (PFNVMACTIVETEXTURE)p;
		}
#endif
	}
	return func;
}

static void DrainGLErrors()
{
	for ( int i = 0; i < 32 && glGetError() != GL_NO_ERROR; i++ )
		;
}

// Saves and restores everything this file touches while reading/writing textures.
struct CTextureStateGuard
{
	GLint active = GL_TEXTURE0, bound = 0;
	GLint packAlign = 4, unpackAlign = 4, packRow = 0, unpackRow = 0;
	GLint packSkipRows = 0, packSkipPixels = 0, unpackSkipRows = 0, unpackSkipPixels = 0;
	bool switchedUnit = false;
	bool ok = true;

	CTextureStateGuard()
	{
		DrainGLErrors();
		GLint packBuffer = 0, unpackBuffer = 0;
		glGetIntegerv( GL_PIXEL_PACK_BUFFER_BINDING, &packBuffer );
		glGetIntegerv( GL_PIXEL_UNPACK_BUFFER_BINDING, &unpackBuffer );
		DrainGLErrors(); // old contexts do not know these enums
		glGetIntegerv( GL_ACTIVE_TEXTURE, &active );
		DrainGLErrors();
		if ( packBuffer || unpackBuffer || ( active != GL_TEXTURE0 && !GetActiveTextureFunc() ) )
		{
			ok = false;
			return;
		}
		if ( active != GL_TEXTURE0 )
		{
			GetActiveTextureFunc()( GL_TEXTURE0 );
			switchedUnit = true;
		}
		glGetIntegerv( GL_TEXTURE_BINDING_2D, &bound );
		glGetIntegerv( GL_PACK_ALIGNMENT, &packAlign );
		glGetIntegerv( GL_UNPACK_ALIGNMENT, &unpackAlign );
		glGetIntegerv( GL_PACK_ROW_LENGTH, &packRow );
		glGetIntegerv( GL_UNPACK_ROW_LENGTH, &unpackRow );
		glGetIntegerv( GL_PACK_SKIP_ROWS, &packSkipRows );
		glGetIntegerv( GL_PACK_SKIP_PIXELS, &packSkipPixels );
		glGetIntegerv( GL_UNPACK_SKIP_ROWS, &unpackSkipRows );
		glGetIntegerv( GL_UNPACK_SKIP_PIXELS, &unpackSkipPixels );
		glPixelStorei( GL_PACK_ALIGNMENT, 4 );
		glPixelStorei( GL_UNPACK_ALIGNMENT, 4 );
		glPixelStorei( GL_PACK_ROW_LENGTH, 0 );
		glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );
		glPixelStorei( GL_PACK_SKIP_ROWS, 0 );
		glPixelStorei( GL_PACK_SKIP_PIXELS, 0 );
		glPixelStorei( GL_UNPACK_SKIP_ROWS, 0 );
		glPixelStorei( GL_UNPACK_SKIP_PIXELS, 0 );
	}

	~CTextureStateGuard()
	{
		if ( !ok )
			return;
		glPixelStorei( GL_PACK_ALIGNMENT, packAlign );
		glPixelStorei( GL_UNPACK_ALIGNMENT, unpackAlign );
		glPixelStorei( GL_PACK_ROW_LENGTH, packRow );
		glPixelStorei( GL_UNPACK_ROW_LENGTH, unpackRow );
		glPixelStorei( GL_PACK_SKIP_ROWS, packSkipRows );
		glPixelStorei( GL_PACK_SKIP_PIXELS, packSkipPixels );
		glPixelStorei( GL_UNPACK_SKIP_ROWS, unpackSkipRows );
		glPixelStorei( GL_UNPACK_SKIP_PIXELS, unpackSkipPixels );
		glBindTexture( GL_TEXTURE_2D, (GLuint)bound );
		if ( switchedUnit )
			GetActiveTextureFunc()( (GLenum)active );
		DrainGLErrors();
	}
};

static unsigned char* g_pScratch;
static size_t g_nScratch;

static unsigned char* Scratch( size_t bytes )
{
	if ( bytes > g_nScratch )
	{
		unsigned char* p = (unsigned char*)realloc( g_pScratch, bytes );
		if ( !p )
			return NULL;
		g_pScratch = p;
		g_nScratch = bytes;
	}
	return g_pScratch;
}

static void ShrinkScratch()
{
	if ( g_nScratch > 4u * 1024 * 1024 )
	{
		free( g_pScratch );
		g_pScratch = NULL;
		g_nScratch = 0;
	}
}

// binds a 2D texture and gets the size of a level; false if the name is not a usable 2D texture
static bool BindTexture2D( GLuint tex, int level, int* w, int* h )
{
	DrainGLErrors();
	glBindTexture( GL_TEXTURE_2D, tex );
	if ( glGetError() != GL_NO_ERROR )
		return false;
	GLint tw = 0, th = 0;
	glGetTexLevelParameteriv( GL_TEXTURE_2D, level, GL_TEXTURE_WIDTH, &tw );
	glGetTexLevelParameteriv( GL_TEXTURE_2D, level, GL_TEXTURE_HEIGHT, &th );
	if ( glGetError() != GL_NO_ERROR || tw <= 0 || th <= 0 || tw > 4096 || th > 4096 )
		return false;
	*w = tw;
	*h = th;
	return true;
}

// reads a level of the bound texture as RGBA into the scratch buffer
static unsigned char* ReadLevel( int level, int w, int h )
{
	unsigned char* buf = Scratch( (size_t)w * h * 4 );
	if ( !buf )
		return NULL;
	DrainGLErrors();
	glGetTexImage( GL_TEXTURE_2D, level, GL_RGBA, GL_UNSIGNED_BYTE, buf );
	return glGetError() == GL_NO_ERROR ? buf : NULL;
}

// ---------------------------------------------------------------------------------------------------------------
// small pictures for comparing a sky file with a texture
// ---------------------------------------------------------------------------------------------------------------

#define THUMB 16

struct Thumb
{
	float v[THUMB * THUMB * 3];
	float mean, stdev;
};

static void ThumbStats( Thumb& t )
{
	const int n = THUMB * THUMB * 3;
	double s = 0, s2 = 0;
	for ( int i = 0; i < n; i++ )
	{
		s += t.v[i];
		s2 += (double)t.v[i] * t.v[i];
	}
	t.mean = (float)( s / n );
	const double var = s2 / n - (double)t.mean * t.mean;
	t.stdev = (float)sqrt( var > 0 ? var : 0 );
}

static void MakeThumb( const unsigned char* rgba, int w, int h, Thumb& t )
{
	for ( int cy = 0; cy < THUMB; cy++ )
	{
		const int y0 = cy * h / THUMB;
		const int y1 = ( cy + 1 ) * h / THUMB > y0 ? ( cy + 1 ) * h / THUMB : y0 + 1;
		for ( int cx = 0; cx < THUMB; cx++ )
		{
			const int x0 = cx * w / THUMB;
			const int x1 = ( cx + 1 ) * w / THUMB > x0 ? ( cx + 1 ) * w / THUMB : x0 + 1;
			double sum[3] = { 0, 0, 0 };
			int count = 0;
			for ( int y = y0; y < y1 && y < h; y++ )
			{
				const unsigned char* p = rgba + ( (size_t)y * w + x0 ) * 4;
				for ( int x = x0; x < x1 && x < w; x++, p += 4 )
				{
					sum[0] += p[0];
					sum[1] += p[1];
					sum[2] += p[2];
					count++;
				}
			}
			float* out = &t.v[( cy * THUMB + cx ) * 3];
			for ( int c = 0; c < 3; c++ )
				out[c] = count ? (float)( sum[c] / count ) : 0.0f;
		}
	}
	ThumbStats( t );
}

// the 8 ways a square picture can be flipped/rotated
static void OrientThumb( const Thumb& src, int o, Thumb& dst )
{
	for ( int y = 0; y < THUMB; y++ )
	{
		for ( int x = 0; x < THUMB; x++ )
		{
			int a = x, b = y;
			if ( o & 4 )
			{
				const int t = a;
				a = b;
				b = t;
			}
			if ( o & 1 )
				a = THUMB - 1 - a;
			if ( o & 2 )
				b = THUMB - 1 - b;
			for ( int c = 0; c < 3; c++ )
				dst.v[( y * THUMB + x ) * 3 + c] = src.v[( b * THUMB + a ) * 3 + c];
		}
	}
	dst.mean = src.mean;
	dst.stdev = src.stdev;
}

static float ThumbCorrelation( const Thumb& a, const Thumb& b )
{
	if ( a.stdev < 1.0f || b.stdev < 1.0f )
		return 0.0f;
	const int n = THUMB * THUMB * 3;
	double cov = 0;
	for ( int i = 0; i < n; i++ )
		cov += ( (double)a.v[i] - a.mean ) * ( (double)b.v[i] - b.mean );
	return (float)( cov / n / ( (double)a.stdev * b.stdev ) );
}

// ---------------------------------------------------------------------------------------------------------------
// TGA loading (truecolour, colour-mapped and grey; plain and RLE)
// ---------------------------------------------------------------------------------------------------------------

static unsigned char* LoadTGA( const char* path, int* pw, int* ph )
{
	int len = 0;
	unsigned char* f = (unsigned char*)gEngfuncs.COM_LoadFile( (char*)path, 5, &len );
	if ( !f )
		return NULL;
	unsigned char* out = NULL;
	do
	{
		if ( len < 18 )
			break;
		const int idlen = f[0], cmtype = f[1], type = f[2];
		const int cmfirst = f[3] | ( f[4] << 8 ), cmlen = f[5] | ( f[6] << 8 ), cmbits = f[7];
		const int w = f[12] | ( f[13] << 8 ), h = f[14] | ( f[15] << 8 ), bpp = f[16], desc = f[17];
		if ( w <= 0 || h <= 0 || w > 4096 || h > 4096 )
			break;
		const bool mapped = type == 1 || type == 9, grey = type == 3 || type == 11, rle = type >= 9;
		if ( !mapped && !grey && type != 2 && type != 10 )
			break;
		const int psize = bpp / 8;
		if ( ( mapped || grey ) ? psize != 1 : ( psize != 3 && psize != 4 ) )
			break;
		int pos = 18 + idlen;
		const unsigned char* pal = NULL;
		const int cmbytes = ( cmbits + 7 ) / 8;
		if ( cmtype == 1 )
		{
			if ( pos + cmlen * cmbytes > len )
				break;
			pal = f + pos;
			pos += cmlen * cmbytes;
		}
		if ( mapped && ( !pal || ( cmbytes != 3 && cmbytes != 4 ) ) )
			break;
		out = (unsigned char*)malloc( (size_t)w * h * 4 );
		if ( !out )
			break;
		const int total = w * h;
		int i = 0;
		bool bad = false;
		unsigned char px[4] = { 0, 0, 0, 255 };
		auto decode = [&]( const unsigned char* s ) {
			if ( mapped )
			{
				const int e = s[0] - cmfirst;
				if ( e < 0 || e >= cmlen )
				{
					px[0] = px[1] = px[2] = 0;
					return;
				}
				const unsigned char* c = pal + e * cmbytes;
				px[0] = c[2];
				px[1] = c[1];
				px[2] = c[0];
			}
			else if ( grey )
				px[0] = px[1] = px[2] = s[0];
			else
			{
				px[0] = s[2];
				px[1] = s[1];
				px[2] = s[0];
			}
			px[3] = 255;
		};
		auto put = [&]() {
			const int x = i % w, fy = i / w;
			const int y = ( desc & 0x20 ) ? fy : h - 1 - fy;
			memcpy( out + ( (size_t)y * w + x ) * 4, px, 4 );
			i++;
		};
		while ( i < total && !bad )
		{
			if ( !rle )
			{
				if ( pos + psize > len )
				{
					bad = true;
					break;
				}
				decode( f + pos );
				pos += psize;
				put();
				continue;
			}
			if ( pos >= len )
			{
				bad = true;
				break;
			}
			const int head = f[pos++];
			const int count = ( head & 0x7f ) + 1;
			if ( head & 0x80 )
			{
				if ( pos + psize > len )
				{
					bad = true;
					break;
				}
				decode( f + pos );
				pos += psize;
				for ( int k = 0; k < count && i < total; k++ )
					put();
			}
			else
			{
				for ( int k = 0; k < count && i < total; k++ )
				{
					if ( pos + psize > len )
					{
						bad = true;
						break;
					}
					decode( f + pos );
					pos += psize;
					put();
				}
			}
		}
		if ( bad )
		{
			free( out );
			out = NULL;
			break;
		}
		*pw = w;
		*ph = h;
	} while ( false );
	gEngfuncs.COM_FreeFile( f );
	return out;
}

unsigned char* VisMods_LoadTGA( const char* path, int* w, int* h )
{
	return LoadTGA( path, w, h );
}

// a picture file as RGBA (free() it): a PNG or JPEG (picture_file.cpp), else a TGA
unsigned char* VisMods_LoadPicture( const char* path, int* w, int* h )
{
	int length = 0;
	unsigned char* file = (unsigned char*)gEngfuncs.COM_LoadFile( (char*)path, 5, &length );
	if ( !file )
		return NULL;
	std::vector<unsigned char> rgba;
	unsigned char* out = NULL;
	if ( length > 0 && PictureFile_Decode( file, (size_t)length, rgba, *w, *h ) )
	{
		out = (unsigned char*)malloc( rgba.size() );
		if ( out )
			memcpy( out, rgba.data(), rgba.size() );
	}
	gEngfuncs.COM_FreeFile( file );
	return out ? out : LoadTGA( path, w, h );
}

// ---------------------------------------------------------------------------------------------------------------
// cl_nosky
//
// Only the current map's sky is remembered. A texture is written to only when its content proves what it holds: the
// original sky picture (thumbnail match) or our own black (black RGB plus a marker colour in two corners).
// ---------------------------------------------------------------------------------------------------------------

#define SKY_SIDES 6
#define SKY_MAX_LEVELS 13
#define SKY_SEARCH_TRIES 6
#define SKY_QUICK_TRIES 3	// searched again the next frames while the map's first frames draw, then every 3 s

static const char* const kSkySides[SKY_SIDES] = { "rt", "bk", "lf", "ft", "up", "dn" };
static const unsigned char kMarker[3] = { 8, 0, 8 }; // survives 16-bit texture formats, invisible in practice

struct SkyFace
{
	GLuint tex;
	int levels;
	int w[SKY_MAX_LEVELS], h[SKY_MAX_LEVELS];
	unsigned char* orig[SKY_MAX_LEVELS]; // the engine's pictures, RGBA, for putting the sky back
	Thumb thumb;                         // of level 0
};

struct SkyRecord
{
	char name[32];
	int numFaces;
	SkyFace faces[SKY_SIDES];
	bool black; // what we last wrote
};

static SkyRecord g_Sky;
static char g_SearchSky[32];
static int g_SearchTries;
static int g_SkyFiles; // sky files of g_SearchSky that exist
static float g_NextSearch;
static float g_NextCheck;
static float g_CheckUntil;
static float g_LastTime;
static bool g_bRecheck;

static void FreeFace( SkyFace& f )
{
	for ( int l = 0; l < f.levels; l++ )
		free( f.orig[l] );
	memset( &f, 0, sizeof( f ) );
}

static void ClearSky()
{
	for ( int i = 0; i < g_Sky.numFaces; i++ )
		FreeFace( g_Sky.faces[i] );
	memset( &g_Sky, 0, sizeof( g_Sky ) );
}

static bool NearMarker( const unsigned char* p )
{
	return abs( p[0] - kMarker[0] ) <= 2 && abs( p[1] - kMarker[1] ) <= 2 && abs( p[2] - kMarker[2] ) <= 2;
}

enum WriteResult
{
	WRITE_OK,      // the face is in the wanted state
	WRITE_GONE,    // the texture no longer holds our sky (never written to)
	WRITE_RETRY,   // could not check now
};

static WriteResult WriteFace( SkyFace& f, bool black )
{
	int w, h;
	if ( !glIsTexture( f.tex ) || !BindTexture2D( f.tex, 0, &w, &h ) || w != f.w[0] || h != f.h[0] )
		return WRITE_GONE;
	const unsigned char* now = ReadLevel( 0, w, h );
	if ( !now )
		return WRITE_RETRY;
	bool ours = NearMarker( now ) && NearMarker( now + ( (size_t)w * h - 1 ) * 4 );
	if ( ours )
	{
		for ( size_t p = 1, n = (size_t)w * h - 1; p < n && ours; p++ )
			ours = now[p * 4] <= 2 && now[p * 4 + 1] <= 2 && now[p * 4 + 2] <= 2;
	}
	bool orig = false;
	if ( !ours )
	{
		Thumb t;
		MakeThumb( now, w, h, t );
		orig = f.thumb.stdev >= 1.0f ? ThumbCorrelation( t, f.thumb ) > 0.97f
		                             : fabs( t.mean - f.thumb.mean ) < 2.0f && t.stdev < 2.0f;
	}
	if ( !ours && !orig )
		return WRITE_GONE;
	if ( black ? ours : orig )
		return WRITE_OK;
	for ( int l = 0; l < f.levels; l++ )
	{
		const unsigned char* src = f.orig[l];
		if ( black )
		{
			// black RGB, the original alpha, the marker in the first and last texel of level 0
			const size_t n = (size_t)f.w[l] * f.h[l];
			unsigned char* b = Scratch( n * 4 );
			if ( !b )
				return WRITE_RETRY;
			for ( size_t p = 0; p < n; p++ )
			{
				b[p * 4 + 0] = b[p * 4 + 1] = b[p * 4 + 2] = 0;
				b[p * 4 + 3] = src[p * 4 + 3];
			}
			if ( l == 0 )
			{
				memcpy( b, kMarker, 3 );
				memcpy( b + ( n - 1 ) * 4, kMarker, 3 );
			}
			src = b;
		}
		glTexSubImage2D( GL_TEXTURE_2D, l, 0, 0, f.w[l], f.h[l], GL_RGBA, GL_UNSIGNED_BYTE, src );
	}
	return WRITE_OK;
}

// true when every face is done (written, or found to belong to something else now and dropped)
static bool ApplySky( bool black )
{
	CTextureStateGuard guard;
	if ( !guard.ok )
		return false;
	bool done = true;
	for ( int i = 0; i < g_Sky.numFaces; )
	{
		const WriteResult r = WriteFace( g_Sky.faces[i], black );
		if ( r == WRITE_GONE )
		{
			FreeFace( g_Sky.faces[i] );
			g_Sky.faces[i] = g_Sky.faces[g_Sky.numFaces - 1];
			memset( &g_Sky.faces[g_Sky.numFaces - 1], 0, sizeof( SkyFace ) );
			g_Sky.numFaces--;
			continue;
		}
		if ( r == WRITE_RETRY )
			done = false;
		i++;
	}
	if ( done )
		g_Sky.black = black;
	return done;
}

static bool SaveFace( GLuint tex, SkyFace& f )
{
	memset( &f, 0, sizeof( f ) );
	f.tex = tex;
	for ( int l = 0; l < SKY_MAX_LEVELS; l++ )
	{
		int w, h;
		if ( !BindTexture2D( tex, l, &w, &h ) )
			break;
		const unsigned char* data = ReadLevel( l, w, h );
		if ( !data )
			break;
		f.orig[l] = (unsigned char*)malloc( (size_t)w * h * 4 );
		if ( !f.orig[l] )
			break;
		memcpy( f.orig[l], data, (size_t)w * h * 4 );
		f.w[l] = w;
		f.h[l] = h;
		f.levels = l + 1;
		if ( l == 0 )
			MakeThumb( data, w, h, f.thumb );
		if ( w == 1 && h == 1 )
			break;
	}
	return f.levels > 0;
}

struct SkySide
{
	bool ok;
	bool flat;
	int w;
	float mean;
	Thumb orient[8];
};

struct SkyCandidate
{
	int side;
	GLuint tex;
	float score; // textured: correlation (higher is better); flat: mean difference (lower is better)
};

static bool SizeFits( const SkySide& sd, int w )
{
	return sd.ok && ( w == sd.w || w * 2 == sd.w || w * 4 == sd.w );
}

// finds the engine's sky textures by comparing every square texture of the right size with the sky files
static void SearchSky( const char* sky )
{
	ClearSky();
	static SkySide sides[SKY_SIDES];
	int numFiles = 0;
	for ( int s = 0; s < SKY_SIDES; s++ )
	{
		SkySide& sd = sides[s];
		memset( &sd, 0, sizeof( sd ) );
		char path[128];
		snprintf( path, sizeof( path ), "gfx/env/%s%s.tga", sky, kSkySides[s] );
		int w = 0, h = 0;
		unsigned char* rgba = LoadTGA( path, &w, &h );
		if ( !rgba )
			continue;
		Thumb base;
		MakeThumb( rgba, w, h, base );
		free( rgba );
		sd.ok = true;
		sd.w = w > h ? w : h;
		sd.flat = base.stdev < 6.0f;
		sd.mean = base.mean;
		for ( int o = 0; o < 8; o++ )
			OrientThumb( base, o, sd.orient[o] );
		numFiles++;
	}
	if ( !numFiles )
		return;

	CTextureStateGuard guard;
	if ( !guard.ok )
		return;
	std::vector<SkyCandidate> textured, flat;
	int misses = 0;
	for ( GLuint tex = 1; tex < 65536 && misses < 4096; tex++ )
	{
		if ( !glIsTexture( tex ) )
		{
			misses++;
			continue;
		}
		misses = 0;
		int w, h;
		if ( !BindTexture2D( tex, 0, &w, &h ) || w != h || w < 32 )
			continue;
		bool wanted = false;
		for ( int s = 0; s < SKY_SIDES; s++ )
			wanted = wanted || SizeFits( sides[s], w );
		if ( !wanted )
			continue;
		const unsigned char* data = ReadLevel( 0, w, h );
		if ( !data )
			continue;
		Thumb t;
		MakeThumb( data, w, h, t );
		for ( int s = 0; s < SKY_SIDES; s++ )
		{
			const SkySide& sd = sides[s];
			if ( !SizeFits( sd, w ) )
				continue;
			if ( sd.flat )
			{
				const float diff = (float)fabs( t.mean - sd.mean );
				if ( t.stdev < 10.0f && diff < 25.0f )
					flat.push_back( { s, tex, diff } );
				continue;
			}
			float best = 0.0f;
			for ( int o = 0; o < 8; o++ )
			{
				const float c = ThumbCorrelation( t, sd.orient[o] );
				if ( c > best )
					best = c;
			}
			if ( best >= 0.9f )
				textured.push_back( { s, tex, best } );
		}
	}

	// one texture per side and one side per texture, best matches first (identical sides get distinct textures)
	GLuint chosen[SKY_SIDES] = { 0, 0, 0, 0, 0, 0 };
	auto used = [&]( GLuint tex ) {
		for ( int s = 0; s < SKY_SIDES; s++ )
		{
			if ( chosen[s] == tex )
				return true;
		}
		return false;
	};
	std::stable_sort( textured.begin(), textured.end(), []( const SkyCandidate& a, const SkyCandidate& b ) { return a.score > b.score; } );
	GLuint lo = 0, hi = 0;
	for ( const SkyCandidate& c : textured )
	{
		if ( chosen[c.side] || used( c.tex ) )
			continue;
		chosen[c.side] = c.tex;
		if ( !lo || c.tex < lo )
			lo = c.tex;
		if ( c.tex > hi )
			hi = c.tex;
	}

	// flat sides (often the bottom) look like many textures: only take ones loaded next to the textured sides.
	// A sky with no textured side is left alone.
	if ( lo )
	{
		const GLuint from = lo > 8 ? lo - 8 : 1, to = hi + 8;
		std::vector<SkyCandidate> nearby;
		for ( const SkyCandidate& c : flat )
		{
			if ( c.tex >= from && c.tex <= to )
				nearby.push_back( c );
		}
		std::stable_sort( nearby.begin(), nearby.end(), []( const SkyCandidate& a, const SkyCandidate& b ) { return a.score < b.score; } );
		for ( const SkyCandidate& c : nearby )
		{
			if ( !chosen[c.side] && !used( c.tex ) )
				chosen[c.side] = c.tex;
		}
	}

	strncpy( g_Sky.name, sky, sizeof( g_Sky.name ) - 1 );
	for ( int s = 0; s < SKY_SIDES; s++ )
	{
		if ( chosen[s] && SaveFace( chosen[s], g_Sky.faces[g_Sky.numFaces] ) )
			g_Sky.numFaces++;
	}
	if ( !g_Sky.numFaces )
		ClearSky();
}

static int SkyFilesPresent( const char* sky )
{
	int n = 0;
	for ( int s = 0; s < SKY_SIDES; s++ )
	{
		char path[128];
		snprintf( path, sizeof( path ), "gfx/env/%s%s.tga", sky, kSkySides[s] );
		int len = 0;
		unsigned char* f = (unsigned char*)gEngfuncs.COM_LoadFile( path, 5, &len );
		if ( f )
		{
			n++;
			gEngfuncs.COM_FreeFile( f );
		}
	}
	return n;
}

// Run before the world is drawn (V_CalcRefdef), so the sky is black from the map's first frame: the search runs as
// soon as the sky's name is known (the engine's movevars, from the start: HUD_PlayerMoveInit), not a moment after the
// first frames (user: "no-sky happens after i joined the server, so sky is visible for a moment")
static void NoSkyFrame( float time )
{
	if ( time < g_LastTime )
		g_bRecheck = true; // the clock went back: start over
	g_LastTime = time;

	const bool want = cl_nosky && cl_nosky->value != 0.0f;
	const char* sky = g_pMoveVars ? g_pMoveVars->skyName : "";
	static bool wasWanted = false;
	if ( want && !wasWanted )
		g_bRecheck = true; // just switched on: search afresh
	wasWanted = want;

	// put the sky back when the option is off or the map has another sky; forget it once that is done
	if ( g_Sky.numFaces && ( !want || strcmp( g_Sky.name, sky ) ) )
	{
		if ( g_Sky.black && !ApplySky( false ) )
			return; // try again next frame
		ClearSky();
		ShrinkScratch();
	}
	if ( !want || !sky[0] )
		return;

	if ( strcmp( g_SearchSky, sky ) || g_bRecheck )
	{
		strncpy( g_SearchSky, sky, sizeof( g_SearchSky ) - 1 );
		g_SearchSky[sizeof( g_SearchSky ) - 1] = 0;
		g_SearchTries = 0;
		g_NextSearch = time;	// at once
		g_SkyFiles = SkyFilesPresent( sky );
		// the engine may (re)load the sky pictures while a map starts: keep checking for a while
		g_CheckUntil = time + 10.0f;
		g_NextCheck = time + 0.5f;
		g_bRecheck = false;
	}

	if ( g_Sky.numFaces < g_SkyFiles && g_SearchTries < SKY_SEARCH_TRIES && time >= g_NextSearch )
	{
		// (re)search: first put back what is blacked out so those textures can be recognised again
		if ( g_Sky.numFaces && g_Sky.black && !ApplySky( false ) )
			return;
		SearchSky( sky );
		ShrinkScratch();
		g_SearchTries++;
		g_NextSearch = time + ( g_SearchTries < SKY_QUICK_TRIES ? 0.0f : 3.0f );
		g_CheckUntil = time + 10.0f;
		if ( g_SearchTries == SKY_SEARCH_TRIES && g_Sky.numFaces < g_SkyFiles )
			gEngfuncs.Con_Printf( "cl_nosky: found %d of the %d textures of sky \"%s\"\n", g_Sky.numFaces, g_SkyFiles, sky );
	}
	if ( g_Sky.numFaces && ( !g_Sky.black || ( time < g_CheckUntil && time >= g_NextCheck ) ) )
	{
		const int facesBefore = g_Sky.numFaces;
		ApplySky( true );
		g_NextCheck = time + 2.0f;
		// a face was taken over by something else (ApplySky dropped it): look once more. Only then: a sky with faces
		// that were never found used to re-search here forever (each search pushed g_CheckUntil on), every 3 s
		if ( g_Sky.numFaces < facesBefore && g_Sky.numFaces < g_SkyFiles && g_SearchTries >= SKY_SEARCH_TRIES
			&& time < g_CheckUntil )
			g_SearchTries = SKY_SEARCH_TRIES - 1;
	}
}

// ---------------------------------------------------------------------------------------------------------------
// cl_teamcolors
// ---------------------------------------------------------------------------------------------------------------

#define CORPSE_TAG 0x56495300 // stored in a corpse's curstate.iuser4, low byte = colour kind

enum ColorKind
{
	COLOR_NONE,
	COLOR_T,
	COLOR_CT,
	COLOR_VIP,
};

static bool g_bSolid;
static float g_SolidColor[4];
static bool g_bEnvSet;
static GLint g_SavedEnvMode;

static bool ParseColor( cvar_t* cvar, float color[3] )
{
	if ( !cvar || !cvar->string )
		return false;
	int r = 0, g = 0, b = 0;
	if ( sscanf( cvar->string, "%d %d %d", &r, &g, &b ) != 3 )
		return false;
	color[0] = ( r < 0 ? 0 : r > 255 ? 255 : r ) / 255.0f;
	color[1] = ( g < 0 ? 0 : g > 255 ? 255 : g ) / 255.0f;
	color[2] = ( b < 0 ? 0 : b > 255 ? 255 : b ) / 255.0f;
	return true;
}

static bool KindColor( int kind, float color[3] )
{
	if ( !cl_teamcolors || cl_teamcolors->value == 0.0f )
		return false;
	switch ( kind )
	{
	case COLOR_T:
		return ParseColor( cl_teamcolor_t, color );
	case COLOR_CT:
		return ParseColor( cl_teamcolor_ct, color );
	case COLOR_VIP:
		return ParseColor( cl_teamcolor_vip, color );
	default:
		return false;
	}
}

// The scoreboard VIP flag is also set by servers for their paying "VIP" members. It is the round's VIP only on a
// VIP map (as_*), or when the player wears the VIP model, and always on the CT side.
bool VisMods_IsRoundVIP( int entindex )
{
	if ( entindex < 1 || entindex > MAX_PLAYERS || !g_PlayerExtraInfo[entindex].vip )
		return false;
	if ( g_PlayerExtraInfo[entindex].teamnumber != 2 )
		return false;
	const char* level = gEngfuncs.pfnGetLevelName();
	if ( level && level[0] )
	{
		const char* base = strrchr( level, '/' );
		base = base ? base + 1 : level;
		if ( !strnicmp( base, "as_", 3 ) )
			return true;
	}
	hud_player_info_t info;
	memset( &info, 0, sizeof( info ) );
	gEngfuncs.pfnGetPlayerInfo( entindex, &info );
	return info.model && !stricmp( info.model, "vip" );
}

static int PlayerKind( int entindex, int team )
{
	if ( VisMods_IsRoundVIP( entindex ) )
		return COLOR_VIP;
	return team == 1 ? COLOR_T : team == 2 ? COLOR_CT : COLOR_NONE;
}

bool VisMods_PlayerColor( int entindex, float color[3] )
{
	if ( entindex < 1 || entindex > MAX_PLAYERS )
		return false;
	// option off: done (the VIP check below asks the engine for the level name and player info per player per frame)
	if ( !cl_teamcolors || cl_teamcolors->value == 0.0f )
		return false;
	return KindColor( PlayerKind( entindex, g_PlayerExtraInfo[entindex].teamnumber ), color );
}

int VisMods_CorpseTag( int team, int player )
{
	const int kind = PlayerKind( player, team );
	return kind != COLOR_NONE ? CORPSE_TAG | kind : 0;
}

bool VisMods_EntityColor( const struct cl_entity_s* ent, float color[3] )
{
	if ( !ent || ( ent->curstate.iuser4 & ~0xff ) != CORPSE_TAG )
		return false;
	return KindColor( ent->curstate.iuser4 & 0xff, color );
}

void VisMods_BeginSolid( const float color[3] )
{
	g_bSolid = true;
	g_SolidColor[0] = color[0];
	g_SolidColor[1] = color[1];
	g_SolidColor[2] = color[2];
	g_SolidColor[3] = 1.0f;
}

void VisMods_EndSolid()
{
	g_bSolid = false;
}

void VisMods_PreDrawPoints( int rendermode )
{
	g_bEnvSet = false;
	// additive passes (glow shells, kRenderTransAdd) are left to the engine: a constant colour would add at full strength
	if ( !g_bSolid || rendermode == kRenderTransAdd )
		return;
	// texture unit 0 outputs the constant colour; alpha still comes from the skin and the render amount
	glGetTexEnviv( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &g_SavedEnvMode );
	glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE );
	glTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE );
	glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_CONSTANT );
	glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR );
	glTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE );
	glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_TEXTURE );
	glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA );
	glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_PRIMARY_COLOR );
	glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA );
	glTexEnvfv( GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, g_SolidColor );
	g_bEnvSet = true;
}

void VisMods_PostDrawPoints()
{
	if ( !g_bEnvSet )
		return;
	g_bEnvSet = false;
	// back to the GL defaults for the combine settings (the engine does not use them) and the engine's mode
	static const GLfloat zero[4] = { 0, 0, 0, 0 };
	glTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE );
	glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE );
	glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR );
	glTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE );
	glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_TEXTURE );
	glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA );
	glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_PREVIOUS );
	glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA );
	glTexEnvfv( GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, zero );
	glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, g_SavedEnvMode );
}

// ---------------------------------------------------------------------------------------------------------------
// cl_headcolor
//
// The engine draws a body part's meshes in one call, so the head cannot be singled out there. After a player is
// drawn, the triangles whose three corners all hang on the head bone ("Bip01 Head") or a bone under it are drawn again
// over it, flat in the head colour like the solid team colours: same vertices and bone transforms as the engine's
// (each vertex hangs on one bone), pulled a hair towards the eye so they win the depth test against the head just
// drawn, and depth-tested like the model, so a head never shows through a wall.
// ---------------------------------------------------------------------------------------------------------------

typedef void( APIENTRY* PFNVMUSEPROGRAM )( GLuint program );

static PFNVMUSEPROGRAM GetUseProgramFunc()
{
	static bool tried = false;
	static PFNVMUSEPROGRAM func = NULL;
	if ( !tried )
	{
		tried = true;
#ifdef _WIN32
		PROC p = wglGetProcAddress( "glUseProgram" );
		const INT_PTR v = (INT_PTR)p;
		if ( v != 0 && v != 1 && v != 2 && v != 3 && v != -1 )
			func = (PFNVMUSEPROGRAM)p;
#endif
	}
	return func;
}

bool VisMods_HeadColor( float color[3] )
{
	// a part of the solid team colours: only while they are on
	if ( !cl_teamcolors || cl_teamcolors->value == 0.0f )
		return false;
	return ParseColor( cl_headcolor, color );
}

// how far the head's corners are moved towards the eye: the engine poses the same vertices with its own float maths,
// and up close (where a hair of distance is many depth steps) the tiny differences made the engine's textured head win
// the depth test, so the colour disappeared. A quarter unit wins every time and is far too little to show through a wall.
const float kHeadTowardsEye = 0.25f;

void VisMods_DrawPlayerHead( const void* data, int body, const float ( *bones )[3][4], const float eye[3], float alpha )
{
	float color[3];
	const studiohdr_t* header = (const studiohdr_t*)data;
	if ( !header || !bones || !eye || !VisMods_HeadColor( color ) || header->numbones <= 0 || header->numbones > MAXSTUDIOBONES )
		return;
	const byte* base = (const byte*)header;

	// the head bone and every bone under it (a bone's parent always comes before it)
	const mstudiobone_t* bone = (const mstudiobone_t*)( base + header->boneindex );
	bool head[MAXSTUDIOBONES];
	bool any = false;
	for ( int i = 0; i < header->numbones; i++ )
	{
		const int parent = bone[i].parent;
		head[i] = !stricmp( bone[i].name, "Bip01 Head" ) || ( parent >= 0 && parent < i && head[parent] );
		any = any || head[i];
	}
	if ( !any )
		return;

	// the head's triangles of the body parts as worn (the engine's StudioSetupModel choice)
	static std::vector<float> triangles;	// 9 floats each
	static std::vector<float> posed;		// 3 per vertex
	static std::vector<signed char> onHead;	// per vertex: -1 not posed yet, 0 no, 1 yes
	triangles.clear();
	const mstudiobodyparts_t* part = (const mstudiobodyparts_t*)( base + header->bodypartindex );
	for ( int p = 0; p < header->numbodyparts; p++ )
	{
		if ( part[p].nummodels <= 0 || part[p].base <= 0 )
			continue;
		const int index = ( body / part[p].base ) % part[p].nummodels;
		const mstudiomodel_t* model = (const mstudiomodel_t*)( base + part[p].modelindex ) + index;
		const int numVerts = model->numverts;
		if ( numVerts <= 0 )
			continue;
		const vec3_t* verts = (const vec3_t*)( base + model->vertindex );
		const byte* vertBone = base + model->vertinfoindex;
		posed.resize( (size_t)numVerts * 3 );
		onHead.assign( numVerts, -1 );
		// a corner: posed with its bone when it is on the head; false when it is not
		auto corner = [&]( int v, float*& out ) -> bool {
			if ( v < 0 || v >= numVerts )
				return false;
			if ( onHead[v] < 0 )
			{
				const int b = vertBone[v];
				onHead[v] = b < header->numbones && head[b] ? 1 : 0;
				if ( onHead[v] )
				{
					const float( *m )[4] = bones[b];
					const float* in = verts[v];
					float* o = &posed[(size_t)v * 3];
					for ( int k = 0; k < 3; k++ )
						o[k] = in[0] * m[k][0] + in[1] * m[k][1] + in[2] * m[k][2] + m[k][3];
					// a hair towards the eye (kHeadTowardsEye)
					const float d[3] = { eye[0] - o[0], eye[1] - o[1], eye[2] - o[2] };
					const float length = sqrtf( d[0] * d[0] + d[1] * d[1] + d[2] * d[2] );
					if ( length > kHeadTowardsEye * 2.0f )
					{
						const float s = kHeadTowardsEye / length;
						for ( int k = 0; k < 3; k++ )
							o[k] += d[k] * s;
					}
				}
			}
			out = &posed[(size_t)v * 3];
			return onHead[v] != 0;
		};
		auto addTriangle = [&]( int a, int b, int c ) {
			float *pa, *pb, *pc;
			if ( corner( a, pa ) && corner( b, pb ) && corner( c, pc ) )
			{
				triangles.insert( triangles.end(), pa, pa + 3 );
				triangles.insert( triangles.end(), pb, pb + 3 );
				triangles.insert( triangles.end(), pc, pc + 3 );
			}
		};
		const mstudiomesh_t* mesh = (const mstudiomesh_t*)( base + model->meshindex );
		for ( int m = 0; m < model->nummesh; m++ )
		{
			// triangle commands: a count (above 0 a strip, below 0 a fan), then that many corners of 4 shorts (vertex,
			// normal, s, t); 0 ends the list
			const short* cmd = (const short*)( base + mesh[m].triindex );
			int count;
			while ( ( count = *cmd++ ) != 0 )
			{
				const bool fan = count < 0;
				if ( fan )
					count = -count;
				// a fan: v0 its first corner, v1 the previous one; a strip: the two corners before this one (which way
				// a triangle faces does not matter, both sides are drawn)
				int v0 = -1, v1 = -1;
				for ( int k = 0; k < count; k++, cmd += 4 )
				{
					const int v = cmd[0];
					if ( k >= 2 )
						addTriangle( v0, v1, v );
					if ( fan )
					{
						if ( k == 0 )
							v0 = v;
						else
							v1 = v;
					}
					else
					{
						v0 = v1;
						v1 = v;
					}
				}
			}
		}
	}
	if ( triangles.empty() )
		return;

	GLint program = 0, activeUnit = GL_TEXTURE0;
	glGetIntegerv( GL_CURRENT_PROGRAM, &program );
	glGetIntegerv( GL_ACTIVE_TEXTURE, &activeUnit );
	DrainGLErrors();	// the queries above are errors on old drivers that lack those things
	PFNVMUSEPROGRAM useProgram = GetUseProgramFunc();
	PFNVMACTIVETEXTURE activeTexture = GetActiveTextureFunc();
	if ( program && useProgram )
		useProgram( 0 );
	if ( activeUnit != GL_TEXTURE0 && activeTexture )
		activeTexture( GL_TEXTURE0 );
	glPushAttrib( GL_ENABLE_BIT | GL_CURRENT_BIT | GL_POLYGON_BIT | GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT );
	glDisable( GL_TEXTURE_2D );
	if ( alpha < 1.0f )
	{
		glEnable( GL_BLEND );	// a see-through player: the head as see-through
		glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	}
	else
		glDisable( GL_BLEND );
	glDisable( GL_ALPHA_TEST );
	glDisable( GL_CULL_FACE );
	glEnable( GL_DEPTH_TEST );
	glDepthFunc( GL_LEQUAL );
	glDepthMask( GL_FALSE );
	glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	glEnable( GL_POLYGON_OFFSET_FILL );
	glPolygonOffset( -1.0f, -2.0f );
	glColor4f( color[0], color[1], color[2], alpha < 1.0f ? alpha : 1.0f );
	glBegin( GL_TRIANGLES );
	for ( size_t i = 0; i + 2 < triangles.size(); i += 3 )
		glVertex3fv( &triangles[i] );
	glEnd();
	glPopAttrib();
	if ( activeUnit != GL_TEXTURE0 && activeTexture )
		activeTexture( (GLenum)activeUnit );
	if ( program && useProgram )
		useProgram( (GLuint)program );
}

// ---------------------------------------------------------------------------------------------------------------
// cl_weaponmodels
//
// Each set is a folder of weapon models named like the game's (models/shadows/retail/v_ak47.mdl for
// models/v_ak47.mdl), with the game's animations in the game's order, so they play the same animation numbers. They are
// loaded on the client only (the server and its precache list never see them) and put in place of the game's model
// just for the drawing (StudioModelRenderer.cpp); a weapon the set has no file for keeps the game's model (the beta
// takes CS 1.5's first, for the weapons that came after it).
// ---------------------------------------------------------------------------------------------------------------

static const char* const kWeaponSetNames[] = { NULL, "retail", "beta" };	// cl_weaponmodels' sets by number
static std::map<model_t*, model_t*> g_WeaponSwaps[3];	// per set: the game's model -> the set's (NULL: the set has none)

// the set cl_weaponmodels names: 0 the game's own, 1 retail, 2 beta
static int WeaponSet()
{
	const char* set = cl_weaponmodels && cl_weaponmodels->string ? cl_weaponmodels->string : "";
	return !stricmp( set, "retail" ) ? 1 : !stricmp( set, "beta" ) ? 2 : 0;
}

// ---------------------------------------------------------------------------------------------------------------
// cl_overridemodels
//
// Servers dress weapons and players in their own models (a plugin points the first-person model, the gun in the hand
// or the player at a file of its own: models/custom/v_ak47_gold.mdl, models/player/vip_admin/vip_admin.mdl). With the
// option on, such a model is drawn as the game's own it stands for - which the weapon sets then swap like any other:
//   - weapons: the weapon named in the file's name (v_, p_ or w_ and the weapon: v_ak47_gold is the AK-47's first-person
//     model); the first-person model of a name that tells nothing (v_claws) is the weapon held. The weapon code plays
//     the game's animation numbers on it either way, so the game's model plays them the same.
//   - players: a player in a model the game doesn't have is drawn in one of the game's four for their team, the same one
//     for the same player (the VIP as the VIP); their bodies too (ev_hldm.cpp).
// ---------------------------------------------------------------------------------------------------------------

// the weapons as their models are named (models/v_<weapon>.mdl), the longer first so a shorter one inside a longer
// ("m3" in nothing else, "glock" in "glock18") is only taken when the longer isn't there
static const char* const kWeaponNames[] = { "smokegrenade", "fiveseven", "flashbang", "hegrenade", "glock18", "xm1014",
	"deagle", "famas", "galil", "g3sg1", "sg550", "sg552", "ump45", "m4a1", "m249", "mac10", "p228", "ak47", "knife",
	"elite", "scout", "glock", "aug", "awp", "mp5", "p90", "tmp", "usp", "c4", "m3" };

static std::map<model_t*, model_t*> g_GameWeapons;		// a server's weapon model -> the game's (NULL: none known)
static std::map<std::string, model_t*> g_GameWeaponFiles;	// "models/v_ak47.mdl" -> the model (NULL: no such file)

static bool OverrideServerModels()
{
	return cl_overridemodels && cl_overridemodels->value != 0.0f;
}

// the game's model "models/<kind>_[shield_]<weapon>.mdl", loaded once; NULL when the game has no such file
static model_t* GameWeaponFile( char kind, bool shield, const char* weapon )
{
	char path[64];
	snprintf( path, sizeof( path ), "models/%c_%s%s.mdl", kind, shield ? "shield_" : "", weapon );
	std::map<std::string, model_t*>::const_iterator known = g_GameWeaponFiles.find( path );
	if ( known != g_GameWeaponFiles.end() )
		return known->second;
	model_t* model = FileExists( path ) ? (model_t*)IEngineStudio.Mod_ForName( path, 0 ) : NULL;
	if ( model && model->type != mod_studio )
		model = NULL;
	g_GameWeaponFiles[path] = model;
	return model;
}

// the game's own weapon model a server's stands for (see above); the model itself when it is the game's own, the option
// is off or nothing tells which weapon it is
static model_t* GameWeaponModel( model_t* model )
{
	if ( !model || model->type != mod_studio || !OverrideServerModels() )
		return model;
	const cl_entity_t* view = gEngfuncs.GetViewModel();
	const bool firstPerson = view && view->model == model;
	std::map<model_t*, model_t*>::const_iterator known = g_GameWeapons.find( model );
	if ( known != g_GameWeapons.end() && ( known->second || !firstPerson ) )
		return known->second ? known->second : model;
	// the file's name without its folders and ".mdl", in lower case
	const char* slash = strrchr( model->name, '/' );
	std::string base = slash ? slash + 1 : model->name;
	for ( char& c : base )
		c = (char)tolower( (unsigned char)c );
	if ( base.size() > 4 && base.compare( base.size() - 4, 4, ".mdl" ) == 0 )
		base.resize( base.size() - 4 );
	char kind = 0;
	if ( base.size() > 2 && base[1] == '_' && strchr( "vpw", base[0] ) )
		kind = base[0];
	else if ( firstPerson )
		kind = 'v';
	else
	{
		g_GameWeapons[model] = NULL;	// not a weapon's
		return model;
	}
	const char* weapon = NULL;
	for ( const char* name : kWeaponNames )
	{
		if ( base.find( name, base[1] == '_' ? 2 : 0 ) != std::string::npos )
		{
			weapon = !strcmp( name, "glock" ) ? "glock18" : name;
			break;
		}
	}
	const bool shield = base.find( "shield" ) != std::string::npos;
	model_t* game = NULL;
	if ( weapon )
	{
		const std::string own = std::string( 1, kind ) + "_" + ( shield ? "shield_" : "" ) + weapon;
		if ( base == own && !strnicmp( model->name, "models/", 7 ) && !strchr( model->name + 7, '/' ) )
		{
			g_GameWeapons[model] = NULL;	// the game's own
			return model;
		}
		game = ( shield ? GameWeaponFile( kind, true, weapon ) : NULL );
		if ( !game )
			game = GameWeaponFile( kind, false, weapon );
		g_GameWeapons[model] = game;	// named in the file: the same every time
	}
	else if ( firstPerson && gHUD.m_Ammo.m_pWeapon && !strnicmp( gHUD.m_Ammo.m_pWeapon->szName, "weapon_", 7 ) )
	{
		// a first-person model of a name that tells nothing: the weapon held (not kept: the same file may stand in
		// for another weapon later)
		const char* held = gHUD.m_Ammo.m_pWeapon->szName + 7;
		const std::string own = std::string( "v_" ) + held;
		if ( base != own )
			game = GameWeaponFile( 'v', g_bHoldingShield, held );
		if ( !game && g_bHoldingShield )
			game = GameWeaponFile( 'v', false, held );
	}
	return game ? game : model;
}

int VisMods_GamePlayerModel( const char* model, int entindex, int team, bool vip )
{
	extern char* sPlayerModelFiles[];
	const int kFiles = 12;	// cdll_int.cpp's list
	if ( !model || !*model || !OverrideServerModels() )
		return -1;
	for ( int i = 0; i < kFiles; i++ )
	{
		if ( !stricmp( model, sPlayerModelFiles[i] ) )
			return -1;	// the game's own
	}
	// the four every game has for each team, by the player's number
	static const int kTerrorists[4] = { 8, 1, 6, 5 };	// terror, leet, arctic, guerilla
	static const int kCTs[4] = { 9, 4, 7, 2 };			// urban, gsg9, sas, gign
	if ( vip )
		return 3;
	if ( team == 1 )
		return kTerrorists[entindex & 3];
	if ( team == 2 )
		return kCTs[entindex & 3];
	return -1;
}

// the model drawn for 'model' with weapon set 'set'
static model_t* WeaponModelOf( model_t* model, int set )
{
	model = GameWeaponModel( model );	// a server's own weapon model as the game's (cl_overridemodels)
	if ( !model || model->type != mod_studio || set <= 0 || set > 2 )
		return model;
	std::map<model_t*, model_t*>& swaps = g_WeaponSwaps[set];
	std::map<model_t*, model_t*>::const_iterator known = swaps.find( model );
	if ( known != swaps.end() )
		return known->second ? known->second : model;
	model_t* swap = NULL;
	const char* name = model->name;
	if ( !strnicmp( name, "models/", 7 ) )
	{
		const char* file = name + 7;
		const char kind = (char)tolower( (unsigned char)file[0] );
		if ( ( kind == 'v' || kind == 'p' || kind == 'w' ) && file[1] == '_' && !strchr( file, '/' ) )
		{
			// the beta has none of the weapons that came after it (Five-seveN, Elites, SG550, UMP45, smoke grenade):
			// those are CS 1.5's from the retail set; a weapon neither has (FAMAS, Galil...) keeps the game's model
			const int tries[2] = { set, set == 2 ? 1 : 0 };
			for ( int t = 0; t < 2 && !swap && tries[t] > 0; t++ )
			{
				char path[128];
				snprintf( path, sizeof( path ), "models/shadows/%s/%s", kWeaponSetNames[tries[t]], file );
				if ( FileExists( path ) )
					swap = (model_t*)IEngineStudio.Mod_ForName( path, 0 );
				if ( swap && swap->type != mod_studio )
					swap = NULL;
			}
		}
	}
	swaps[model] = swap;
	return swap ? swap : model;
}

model_t* VisMods_WeaponModel( model_t* model )
{
	return WeaponModelOf( model, WeaponSet() );
}

// The weapon menu's pictures: CS 1.5 and the beta show CS 1.5's knife (the other weapons look as in the game).
// tools/handvec/knife15.py makes sprites/shadows/retail/: weapon_knife.txt (the game's list with the two knife pictures
// pointed at its knife.spr / knife320.spr) and cs1280/, cs3840/ (the same for each hi-res HUD tier, the
// pictures traced to vector art). ammo.cpp reads the lists from here.
const char* VisMods_WeaponIconFolder( const char* weapon )
{
	if ( WeaponSet() > 0 && weapon && !stricmp( weapon, "weapon_knife" )
		&& FileExists( "sprites/shadows/retail/weapon_knife.txt" ) )
		return "shadows/retail/";
	return "";
}

// ---------------------------------------------------------------------------------------------------------------
// cl_playermodels
//
// Like the weapon sets: models/shadows/<set>/player/<name>/<name>.mdl drawn in place of the game's
// models/player/<name>/<name>.mdl (GameStudioModelRenderer.cpp), with the game's animations in the game's order and
// 9-way blends (tools/betaweapons makes the beta's so). "retail" is CS 1.0's; the beta set takes it for a model the
// beta does not have, and a model neither has stays the game's.
// ---------------------------------------------------------------------------------------------------------------

static std::map<model_t*, model_t*> g_PlayerSwaps[3];	// per set: the game's model -> the set's (NULL: none)

// the set cl_playermodels names: 0 the game's own, 1 retail, 2 beta
static int PlayerSet()
{
	const char* set = cl_playermodels && cl_playermodels->string ? cl_playermodels->string : "";
	return !stricmp( set, "retail" ) ? 1 : !stricmp( set, "beta" ) ? 2 : 0;
}

model_t* VisMods_PlayerModel( model_t* model )
{
	const int set = PlayerSet();
	if ( !model || model->type != mod_studio || set <= 0 || set > 2 )
		return model;
	std::map<model_t*, model_t*>& swaps = g_PlayerSwaps[set];
	std::map<model_t*, model_t*>::const_iterator known = swaps.find( model );
	if ( known != swaps.end() )
		return known->second ? known->second : model;
	model_t* swap = NULL;
	if ( !strnicmp( model->name, "models/player/", 14 ) )
	{
		const int tries[2] = { set, set == 2 ? 1 : 0 };
		for ( int t = 0; t < 2 && !swap && tries[t] > 0; t++ )
		{
			char path[160];
			snprintf( path, sizeof( path ), "models/shadows/%s/%s", kWeaponSetNames[tries[t]], model->name + 7 );
			if ( FileExists( path ) )
				swap = (model_t*)IEngineStudio.Mod_ForName( path, 0 );
			if ( swap && swap->type != mod_studio )
				swap = NULL;
		}
	}
	swaps[model] = swap;
	return swap ? swap : model;
}

// ---------------------------------------------------------------------------------------------------------------
// Guns in the hand, one set's on the other's players
//
// A gun (p_ model) hangs from the player's hand bone where its own game's skeleton holds it: CS 1.6's and CS 1.5's
// guns alike, beta 6.1's about 10-20 degrees off (on the game's players the barrel points up and the gun floats over
// the other hand; the game's guns on the beta's players the other way). tools/betaweapons works out, gun by gun, the
// move putting one where the other hold has it and writes them to models/shadows/beta/hold.txt; a gun drawn on a
// player of the other hold is moved by its line as it is hung on the hand (StudioMergeBones; the Advanced window's
// preview does the same, studio_preview.cpp) - where a fitted copy of every gun was kept before.
// (user: "Retail guns are like hovering a bit")
// ---------------------------------------------------------------------------------------------------------------

struct HeldMove
{
	float rows[3][4];
};
static std::map<std::string, HeldMove> g_HeldMoves;	// "<players> <gun set> <p_ model>", lower case -> the move
static bool g_bHeldMovesRead;
// per hold of the player drawn (0 the game's, 1 the beta's): the gun drawn -> its move (NULL: none)
static std::map<const model_t*, const float*> g_HeldMoveOf[2];

// the set a model's file comes from: 0 the game's, 1 retail, 2 beta
static int ModelSetOf( const char* name )
{
	if ( name && !strnicmp( name, "models/shadows/retail/", 22 ) )
		return 1;
	if ( name && !strnicmp( name, "models/shadows/beta/", 20 ) )
		return 2;
	return 0;
}

static void ReadHeldMoves()
{
	g_bHeldMovesRead = true;
	g_HeldMoves.clear();
	int length = 0;
	char* file = (char*)gEngfuncs.COM_LoadFile( (char*)"models/shadows/beta/hold.txt", 5, &length );
	if ( !file )
		return;
	const char* at = file;
	const char* const end = file + length;
	while ( at < end )
	{
		const char* stop = at;
		while ( stop < end && *stop != '\n' )
			stop++;
		char line[512];
		const size_t size = (size_t)( stop - at ) < sizeof( line ) - 1 ? (size_t)( stop - at ) : sizeof( line ) - 1;
		memcpy( line, at, size );
		line[size] = 0;
		at = stop < end ? stop + 1 : end;
		char players[16], set[16], model[64];
		HeldMove move;
		float* m = &move.rows[0][0];
		if ( line[0] == '/'
			 || sscanf( line, "%15s %15s %63s %f %f %f %f %f %f %f %f %f %f %f %f", players, set, model, m, m + 1, m + 2, m + 3,
					m + 4, m + 5, m + 6, m + 7, m + 8, m + 9, m + 10, m + 11 ) != 15 )
			continue;
		std::string key = std::string( players ) + " " + set + " " + model;
		for ( char& c : key )
			c = (char)tolower( (unsigned char)c );
		g_HeldMoves[key] = move;
	}
	gEngfuncs.COM_FreeFile( file );
}

// the move for the gun of file 'weapon' drawn on the player of file 'player'; NULL when the gun was made for that
// player's hold, is no p_ model or has no line
static const float* HeldMoveFor( const char* weapon, const char* player )
{
	if ( !weapon || !player )
		return NULL;
	const int gunSet = ModelSetOf( weapon );
	const int hold = ModelSetOf( player ) == 2 ? 1 : 0;	// the beta's players hold their guns their own way
	if ( ( gunSet == 2 ? 1 : 0 ) == hold )
		return NULL;
	const char* file = strrchr( weapon, '/' );
	file = file ? file + 1 : weapon;
	if ( strnicmp( file, "p_", 2 ) )
		return NULL;
	if ( !g_bHeldMovesRead )
		ReadHeldMoves();
	static const char* const kSets[] = { "game", "retail", "beta" };
	const std::string start = std::string( hold ? "beta " : "game " ) + kSets[gunSet] + " ";
	std::string key = start + file;
	for ( char& c : key )
		c = (char)tolower( (unsigned char)c );
	std::map<std::string, HeldMove>::const_iterator found = g_HeldMoves.find( key );
	if ( found == g_HeldMoves.end() )
		found = g_HeldMoves.find( start + "*" );	// every other gun of that set
	if ( found == g_HeldMoves.end() )
		return NULL;
	// (a line that moves nothing: the plain shield)
	bool moves = false;
	for ( int row = 0; row < 3; row++ )
	{
		for ( int col = 0; col < 4; col++ )
			moves = moves || found->second.rows[row][col] != ( row == col ? 1.0f : 0.0f );
	}
	return moves ? &found->second.rows[0][0] : NULL;
}

const float* VisMods_HeldMoveByPath( const char* weaponPath, const char* playerPath )
{
	return HeldMoveFor( weaponPath, playerPath );
}

const float* VisMods_HeldMove( const model_t* weapon, const model_t* player )
{
	if ( !weapon || !player || weapon->type != mod_studio )
		return NULL;
	std::map<const model_t*, const float*>& known = g_HeldMoveOf[ModelSetOf( player->name ) == 2 ? 1 : 0];
	std::map<const model_t*, const float*>::const_iterator found = known.find( weapon );
	if ( found != known.end() )
		return found->second;
	const float* move = HeldMoveFor( weapon->name, player->name );
	known[weapon] = move;
	return move;
}

// ---------------------------------------------------------------------------------------------------------------
// The beta's players drawn bigger
//
// The beta set's player models are drawn a little bigger, a third of the way to as big as their hit boxes let them be
// (player_fit.cpp, kBetaPlayerGrowth); the game's own and CS 1.0's as they are made. (An option for every set, "Player size", came first; user: "Remove Player Size and make
// Biggest default for Beta only".) The boxes are those of the player's own model (the one the server gave him) - the
// game's model of that name when a set's model stands in its place - or, for a skeleton that has not those bones (the
// beta's), the drawn model's own.
// ---------------------------------------------------------------------------------------------------------------

// the plans by the files of the model drawn and the server's (kept for the whole game), and by the models of this map
static std::map<std::pair<std::string, std::string>, PlayerFit> g_PlayerFits;
static std::map<std::pair<const model_t*, const model_t*>, const PlayerFit*> g_PlayerFitOf;

// the model whose hit boxes the server tests: the player's own; for a set's model standing in its place (a body lying
// about is given the set's model itself), the game's model of that name
static const model_t* ServerModelOf( const model_t* entityModel, const model_t* drawn )
{
	const model_t* model = entityModel && entityModel->type == mod_studio ? entityModel : drawn;
	if ( ModelSetOf( model->name ) == 0 )
		return model;
	// models/shadows/<set>/player/<name>/<name>.mdl -> models/player/<name>/<name>.mdl
	const char* rest = strchr( model->name + 15, '/' );
	if ( !rest )
		return model;
	char path[128];
	snprintf( path, sizeof( path ), "models%s", rest );
	if ( !FileExists( path ) )
		return model;
	const model_t* game = (const model_t*)IEngineStudio.Mod_ForName( path, 0 );
	return game && game->type == mod_studio ? game : model;
}

bool VisMods_GrowPlayer( const model_t* drawn, const model_t* entityModel, float ( *bones )[3][4], float ( *saved )[3][4] )
{
	if ( !drawn || drawn->type != mod_studio || ModelSetOf( drawn->name ) != 2 || !bones || !saved )
		return false;	// the beta's models only
	const model_t* server = ServerModelOf( entityModel, drawn );
	const std::pair<const model_t*, const model_t*> models( drawn, server );
	std::map<std::pair<const model_t*, const model_t*>, const PlayerFit*>::const_iterator known = g_PlayerFitOf.find( models );
	const PlayerFit* fit = NULL;
	if ( known != g_PlayerFitOf.end() )
		fit = known->second;
	else
	{
		const std::pair<std::string, std::string> files( drawn->name, server->name );
		std::map<std::pair<std::string, std::string>, PlayerFit>::iterator planned = g_PlayerFits.find( files );
		if ( planned == g_PlayerFits.end() )
		{
			planned = g_PlayerFits.insert( std::make_pair( files, PlayerFit() ) ).first;
			// (the server's first: reading it in may move other models' data about, and the drawn one's is in use)
			const void* serverData = IEngineStudio.Mod_Extradata( (model_t*)server );
			const void* drawnData = IEngineStudio.Mod_Extradata( (model_t*)drawn );
			PlayerFit_Plan( drawnData, serverData, &planned->second );
		}
		fit = &planned->second;
		g_PlayerFitOf[models] = fit;
	}
	if ( fit->numBones <= 0 || fit->numBones > MAXSTUDIOBONES || fit->factor <= 1.0f )
		return false;
	memcpy( saved, bones, sizeof( float[3][4] ) * fit->numBones );
	PlayerFit_Grow( fit, kBetaPlayerGrowth, bones );
	return true;
}

// The weapon set changed while playing and the gun in hand looks different now: it plays its draw animation at once, as
// if just taken out (the game keeps drawing behind the Esc menu, so it shows there too, like the other options that
// change the picture). Not while spectating or dead.
void Ammo_ReloadWeaponIcons();	// ammo.cpp

static void WeaponSetChangeFrame()
{
	static int s_lastSet = -1;
	const int set = WeaponSet();
	if ( set == s_lastSet )
		return;
	const int before = s_lastSet;
	s_lastSet = set;
	if ( before >= 0 )
		Ammo_ReloadWeaponIcons();	// the weapon menu's pictures of the new set (VisMods_WeaponIconFolder)
	cl_entity_t* view = gEngfuncs.GetViewModel();
	if ( before < 0 || !view || !view->model || g_iUser1 || gHUD.m_fPlayerDead
		|| WeaponModelOf( view->model, before ) == WeaponModelOf( view->model, set ) )
		return;
	// the draw animation (the set's model has the game's animations in the game's order): "draw", or "draw_unsil" while
	// the animation playing is an unsilenced one (M4A1, USP); else the first one with "draw" in its name
	const studiohdr_t* header = (const studiohdr_t*)IEngineStudio.Mod_Extradata( view->model );
	if ( !header || header->numseq <= 0 )
		return;
	const mstudioseqdesc_t* seq = (const mstudioseqdesc_t*)( (const byte*)header + header->seqindex );
	const int current = view->curstate.sequence;
	const bool unsilenced = current >= 0 && current < header->numseq && strstr( seq[current].label, "unsil" );
	const char* wanted = unsilenced ? "draw_unsil" : "draw";
	int draw = -1;
	for ( int i = 0; i < header->numseq && draw < 0; i++ )
	{
		if ( !stricmp( seq[i].label, wanted ) )
			draw = i;
	}
	for ( int i = 0; i < header->numseq && draw < 0; i++ )
	{
		char label[32];
		strncpy( label, seq[i].label, sizeof( label ) - 1 );
		label[sizeof( label ) - 1] = 0;
		for ( char* c = label; *c; c++ )
			*c = (char)tolower( (unsigned char)*c );
		if ( strstr( label, "draw" ) )
			draw = i;
	}
	if ( draw >= 0 )
		gEngfuncs.pfnWeaponAnim( draw, view->curstate.body );
}

bool VisMods_WeaponModelRightHanded( const model_t* drawn, bool gameRightHanded )
{
	// every CS 1.5 view model (knife and grenades too) is made for the right hand but the C4, which sits on the left like
	// the beta's and the game's; every beta one for the left hand, like the game's guns - the knife too (its blade sits
	// on the left, where the game's knife has it on the right)
	if ( drawn && !strnicmp( drawn->name, "models/shadows/retail/", 22 ) )
		return stricmp( drawn->name + 22, "v_c4.mdl" ) != 0;
	if ( drawn && !strnicmp( drawn->name, "models/shadows/beta/", 20 ) )
		return false;
	return gameRightHanded;
}

// ---------------------------------------------------------------------------------------------------------------

void VisMods_Init()
{
	cl_nosky = gEngfuncs.pfnRegisterVariable( "cl_nosky", "0", FCVAR_ARCHIVE );
	cl_teamcolors = gEngfuncs.pfnRegisterVariable( "cl_teamcolors", "0", FCVAR_ARCHIVE );
	cl_teamcolor_t = gEngfuncs.pfnRegisterVariable( "cl_teamcolor_t", "255 0 0", FCVAR_ARCHIVE );
	cl_teamcolor_ct = gEngfuncs.pfnRegisterVariable( "cl_teamcolor_ct", "0 0 255", FCVAR_ARCHIVE );
	cl_teamcolor_vip = gEngfuncs.pfnRegisterVariable( "cl_teamcolor_vip", "255 255 0", FCVAR_ARCHIVE );
	cl_headcolor = gEngfuncs.pfnRegisterVariable( "cl_headcolor", "0", FCVAR_ARCHIVE );
	cl_weaponmodels = gEngfuncs.pfnRegisterVariable( "cl_weaponmodels", "original", FCVAR_ARCHIVE );
	cl_playermodels = gEngfuncs.pfnRegisterVariable( "cl_playermodels", "original", FCVAR_ARCHIVE );
	cl_overridemodels = gEngfuncs.pfnRegisterVariable( "cl_overridemodels", "0", FCVAR_ARCHIVE );
}

void VisMods_VidInit()
{
	g_bRecheck = true;
	g_bSolid = false;
	for ( std::map<model_t*, model_t*>& swaps : g_WeaponSwaps )
		swaps.clear();	// a new map's model list: looked up again
	for ( std::map<model_t*, model_t*>& swaps : g_PlayerSwaps )
		swaps.clear();
	for ( std::map<const model_t*, const float*>& moves : g_HeldMoveOf )
		moves.clear();
	g_bHeldMovesRead = false;	// hold.txt read again (the moves handed out so far are forgotten above)
	g_PlayerFitOf.clear();
}

void VisMods_SetMoveVars( struct movevars_s* pMoveVars )
{
	g_pMoveVars = pMoveVars;
}

void VisMods_Frame( float )
{
	WeaponSetChangeFrame();
}

void VisMods_BeforeWorld()
{
	NoSkyFrame( (float)gEngfuncs.GetAbsoluteTime() );
}

// ---------------------------------------------------------------------------------------------------------------
// viewmodel_fov: a wider weapon FOV brings parts of the weapon that sit very close to the eye on screen, and the
// engine's near clip plane (4 units) slices them off. The weapon is drawn with a closer near plane; only the depth
// row of the engine's own projection changes, so the picture is otherwise identical.

static GLfloat g_ViewModelSavedProjection[16];
static bool g_ViewModelNearActive = false;

bool VisMods_BeginViewModelNear( float zNear )
{
	if ( g_ViewModelNearActive || zNear <= 0.0f )
		return false;
	GLfloat m[16];
	glGetFloatv( GL_PROJECTION_MATRIX, m );
	// a plain perspective projection: m[11] = -1, m[15] = 0, m[10] = -(f+n)/(f-n), m[14] = -2fn/(f-n)
	if ( m[11] > -0.999f || m[11] < -1.001f || fabsf( m[15] ) > 1e-6f || fabsf( m[3] ) > 1e-6f || fabsf( m[7] ) > 1e-6f )
		return false;
	const float a = m[10], b = m[14];
	if ( fabsf( a - 1.0f ) < 1e-7f || fabsf( a + 1.0f ) < 1e-7f )
		return false;
	const float n = b / ( a - 1.0f ), f = b / ( a + 1.0f );
	if ( !( n > zNear ) || !( f > n ) || f > 1.0e7f )
		return false;
	memcpy( g_ViewModelSavedProjection, m, sizeof( m ) );
	m[10] = -( f + zNear ) / ( f - zNear );
	m[14] = -2.0f * f * zNear / ( f - zNear );
	GLint mode = GL_MODELVIEW;
	glGetIntegerv( GL_MATRIX_MODE, &mode );
	glMatrixMode( GL_PROJECTION );
	glLoadMatrixf( m );
	glMatrixMode( (GLenum)mode );
	g_ViewModelNearActive = true;
	return true;
}

void VisMods_EndViewModelNear()
{
	if ( !g_ViewModelNearActive )
		return;
	g_ViewModelNearActive = false;
	GLint mode = GL_MODELVIEW;
	glGetIntegerv( GL_MATRIX_MODE, &mode );
	glMatrixMode( GL_PROJECTION );
	glLoadMatrixf( g_ViewModelSavedProjection );
	glMatrixMode( (GLenum)mode );
}
