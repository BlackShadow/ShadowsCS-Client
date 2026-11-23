//========= Shadows custom CS client =========================================
//
// overview_gen: top-down floor layout of a GoldSrc BSP (v30) for the radar,
// plus classic spectator overview generation (overviews/<map>.bmp + .txt).
//
// Pure standard C++ (no engine headers, no windows.h). Every read from the
// BSP is bounds-checked; malformed maps make the API return false, never crash.
//
//=============================================================================
#include "overview_gen.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace
{

// ---------------------------------------------------------------------------
// small helpers (no std::min/std::max: windows.h macros live in other TUs)
// ---------------------------------------------------------------------------
template< class T > inline T OvMin( T a, T b ) { return a < b ? a : b; }
template< class T > inline T OvMax( T a, T b ) { return a > b ? a : b; }
template< class T > inline T OvClamp( T v, T lo, T hi ) { return v < lo ? lo : ( v > hi ? hi : v ); }

const int   kBspVersion    = 30;
const int   kNumLumps      = 15;
const int   kHeaderSize    = 4 + kNumLumps * 8;

const int   LUMP_ENTITIES  = 0;
const int   LUMP_PLANES    = 1;
const int   LUMP_TEXTURES  = 2;
const int   LUMP_VERTEXES  = 3;
const int   LUMP_TEXINFO   = 6;
const int   LUMP_FACES     = 7;
const int   LUMP_EDGES     = 12;
const int   LUMP_SURFEDGES = 13;
const int   LUMP_MODELS    = 14;

const int   kPlaneSize     = 20;
const int   kVertSize      = 12;
const int   kTexinfoSize   = 40;
const int   kFaceSize      = 20;
const int   kEdgeSize      = 4;
const int   kSurfedgeSize  = 4;
const int   kModelSize     = 64;

const float kFloorNormalZ  = 0.7f;   // outward normal z >= this = floor
const float kMergeDist     = 24.0f;  // layers closer than this are one floor
const float kViewerSlack   = OVERVIEW_VIEWER_SLACK;  // viewer sees floors up to viewerZ + this (radar2.cpp relies on it)
const float kEdgeStep      = 48.0f;  // height step drawn as an outline
const float kCoverFrac     = 0.35f;  // polygon/cell overlap that counts as covered
const float kMinCell       = 4.0f;
const int   kMaxPolyVerts  = 128;
const int   kClipCap       = kMaxPolyVerts + 16;
const float kSaneCoord     = 131072.0f;
const float kAspect        = 4.0f / 3.0f;
const size_t kMaxGridFloats = 24u * 1024u * 1024u; // ~96 MB hard cap

const int   kClassicW      = OVERVIEW_CLASSIC_W;
const int   kClassicH      = OVERVIEW_CLASSIC_H;

// classic palette layout
const int   kPalFloorFirst = 1;
const int   kPalFloorCount = 200;     // 1..200 floor ramp
const int   kPalWaterFirst = 201;
const int   kPalWaterCount = 40;      // 201..240 water ramp
const int   kPalOutline    = 241;     // 241 outline, 242..255 copies of it

enum TexClass : unsigned char { TEX_NORMAL = 0, TEX_SKIP = 1, TEX_WATER = 2 };

inline int32_t RdI32( const unsigned char* p ) { int32_t v; memcpy( &v, p, 4 ); return v; }
inline int16_t RdI16( const unsigned char* p ) { int16_t v; memcpy( &v, p, 2 ); return v; }
inline uint16_t RdU16( const unsigned char* p ) { uint16_t v; memcpy( &v, p, 2 ); return v; }
inline float RdF32( const unsigned char* p ) { float v; memcpy( &v, p, 4 ); return v; }

inline bool IsSane( float v ) { return v == v && v > -kSaneCoord && v < kSaneCoord; }

inline void SetErr( char* err, int errLen, const char* msg )
{
	if ( !err || errLen <= 0 )
		return;
	size_t n = strlen( msg );
	const size_t cap = static_cast< size_t >( errLen - 1 );
	if ( n > cap )
		n = cap;
	memcpy( err, msg, n );
	err[n] = 0;
}

inline char ToLowerAscii( char c ) { return ( c >= 'A' && c <= 'Z' ) ? static_cast< char >( c - 'A' + 'a' ) : c; }

inline bool IEquals( const char* a, const char* b )
{
	for ( ;; )
	{
		const char ca = ToLowerAscii( *a ), cb = ToLowerAscii( *b );
		if ( ca != cb )
			return false;
		if ( !ca )
			return true;
		++a;
		++b;
	}
}

inline bool IStartsWith( const char* s, const char* prefix )
{
	for ( ; *prefix; ++s, ++prefix )
	{
		if ( ToLowerAscii( *s ) != ToLowerAscii( *prefix ) )
			return false;
	}
	return true;
}

// ---------------------------------------------------------------------------
// BSP view
// ---------------------------------------------------------------------------
struct BspView
{
	const unsigned char* base = nullptr;
	int len = 0;
	int ofs[kNumLumps] = {};
	int size[kNumLumps] = {};

	const unsigned char* planes = nullptr;    int numPlanes = 0;
	const unsigned char* verts = nullptr;     int numVerts = 0;
	const unsigned char* texinfo = nullptr;   int numTexinfo = 0;
	const unsigned char* faces = nullptr;     int numFaces = 0;
	const unsigned char* edges = nullptr;     int numEdges = 0;
	const unsigned char* surfedges = nullptr; int numSurfedges = 0;
	const unsigned char* models = nullptr;    int numModels = 0;

	const unsigned char* Lump( int i ) const { return base + ofs[i]; }
};

bool OpenBsp( const unsigned char* data, int len, BspView& b, char* err, int errLen )
{
	if ( !data || len < kHeaderSize )
	{
		SetErr( err, errLen, "BSP: file too small" );
		return false;
	}
	const int version = RdI32( data );
	if ( version != kBspVersion )
	{
		char msg[96];
		snprintf( msg, sizeof( msg ), "BSP: unsupported version %d (need %d)", version, kBspVersion );
		SetErr( err, errLen, msg );
		return false;
	}

	b.base = data;
	b.len = len;
	static const int used[] = { LUMP_ENTITIES, LUMP_PLANES, LUMP_TEXTURES, LUMP_VERTEXES, LUMP_TEXINFO,
		LUMP_FACES, LUMP_EDGES, LUMP_SURFEDGES, LUMP_MODELS };
	for ( int i = 0; i < kNumLumps; i++ )
	{
		b.ofs[i] = RdI32( data + 4 + i * 8 );
		b.size[i] = RdI32( data + 8 + i * 8 );
	}
	for ( int lump : used )
	{
		const int o = b.ofs[lump], s = b.size[lump];
		if ( o < 0 || s < 0 || o > len || s > len - o )
		{
			char msg[96];
			snprintf( msg, sizeof( msg ), "BSP: lump %d out of file bounds", lump );
			SetErr( err, errLen, msg );
			return false;
		}
	}

	b.planes = b.Lump( LUMP_PLANES );       b.numPlanes = b.size[LUMP_PLANES] / kPlaneSize;
	b.verts = b.Lump( LUMP_VERTEXES );      b.numVerts = b.size[LUMP_VERTEXES] / kVertSize;
	b.texinfo = b.Lump( LUMP_TEXINFO );     b.numTexinfo = b.size[LUMP_TEXINFO] / kTexinfoSize;
	b.faces = b.Lump( LUMP_FACES );         b.numFaces = b.size[LUMP_FACES] / kFaceSize;
	b.edges = b.Lump( LUMP_EDGES );         b.numEdges = b.size[LUMP_EDGES] / kEdgeSize;
	b.surfedges = b.Lump( LUMP_SURFEDGES ); b.numSurfedges = b.size[LUMP_SURFEDGES] / kSurfedgeSize;
	b.models = b.Lump( LUMP_MODELS );       b.numModels = b.size[LUMP_MODELS] / kModelSize;

	if ( b.numModels < 1 || b.numFaces < 1 || b.numPlanes < 1 || b.numVerts < 3 || b.numEdges < 1 || b.numSurfedges < 3 )
	{
		SetErr( err, errLen, "BSP: missing geometry lumps" );
		return false;
	}
	return true;
}

void ClassifyTextures( const BspView& b, std::vector< unsigned char >& cls )
{
	cls.clear();
	const int l = b.size[LUMP_TEXTURES];
	if ( l < 4 )
		return;
	const unsigned char* p = b.Lump( LUMP_TEXTURES );
	int n = RdI32( p );
	if ( n <= 0 )
		return;
	if ( n > ( l - 4 ) / 4 )
		n = ( l - 4 ) / 4;
	cls.assign( static_cast< size_t >( n ), TEX_NORMAL );

	static const char* const skipNames[] = { "sky", "aaatrigger", "clip", "null", "origin", "bevel", "hint", "skip" };
	for ( int i = 0; i < n; i++ )
	{
		const int o = RdI32( p + 4 + 4 * i );
		if ( o < 0 || o > l - 16 )
			continue; // -1 = missing (external WAD texture with no header) or corrupt
		char name[17];
		memcpy( name, p + o, 16 );
		name[16] = 0;
		if ( name[0] == '!' || IStartsWith( name, "water" ) )
		{
			cls[static_cast< size_t >( i )] = TEX_WATER;
			continue;
		}
		for ( const char* s : skipNames )
		{
			if ( IEquals( name, s ) )
			{
				cls[static_cast< size_t >( i )] = TEX_SKIP;
				break;
			}
		}
	}
}

// ---------------------------------------------------------------------------
// entity lump: brush entities that are static floors
// ---------------------------------------------------------------------------
struct BrushEnt
{
	int model;
	float ox, oy, oz;
};

bool IsStaticBrushClass( const std::string& cn )
{
	static const char* const classes[] = { "func_wall", "func_wall_toggle", "func_breakable", "func_illusionary",
		"func_door", "func_pushable", "func_water", "func_conveyor" };
	for ( const char* c : classes )
	{
		if ( IEquals( cn.c_str(), c ) )
			return true;
	}
	return false;
}

void ParseOrigin( const std::string& s, float& x, float& y, float& z )
{
	x = y = z = 0.0f;
	const char* p = s.c_str();
	char* end = nullptr;
	float v[3] = { 0.0f, 0.0f, 0.0f };
	for ( int i = 0; i < 3; i++ )
	{
		v[i] = strtof( p, &end );
		if ( end == p )
			break;
		p = end;
	}
	if ( IsSane( v[0] ) && IsSane( v[1] ) && IsSane( v[2] ) )
	{
		x = v[0];
		y = v[1];
		z = v[2];
	}
}

void ParseEntities( const BspView& b, std::vector< BrushEnt >& out )
{
	out.clear();
	const char* p = reinterpret_cast< const char* >( b.Lump( LUMP_ENTITIES ) );
	const int n = b.size[LUMP_ENTITIES];
	int i = 0;

	bool inEnt = false;
	std::string key, value, classname, model, origin, rendermode, renderamt;
	bool haveKey = false;

	while ( i < n )
	{
		const char c = p[i];
		if ( c == 0 )
			break;
		if ( c == '{' )
		{
			inEnt = true;
			haveKey = false;
			classname.clear();
			model.clear();
			origin.clear();
			rendermode.clear();
			renderamt.clear();
			i++;
			continue;
		}
		if ( c == '}' )
		{
			if ( inEnt && model.size() > 1 && model[0] == '*' && IsStaticBrushClass( classname ) )
			{
				const long m = strtol( model.c_str() + 1, nullptr, 10 );
				// fully invisible brush entities (rendermode != normal, renderamt 0) are not drawn floors
				const bool invisible = !rendermode.empty() && atoi( rendermode.c_str() ) != 0 &&
					!renderamt.empty() && atoi( renderamt.c_str() ) == 0;
				if ( m > 0 && m < b.numModels && !invisible )
				{
					BrushEnt e;
					e.model = static_cast< int >( m );
					ParseOrigin( origin, e.ox, e.oy, e.oz );
					out.push_back( e );
				}
			}
			inEnt = false;
			haveKey = false;
			i++;
			continue;
		}
		if ( c == '"' )
		{
			i++;
			const int start = i;
			while ( i < n && p[i] != '"' && p[i] != 0 )
				i++;
			int slen = i - start;
			if ( slen > 1024 )
				slen = 1024;
			std::string tok( p + start, static_cast< size_t >( slen ) );
			if ( i < n && p[i] == '"' )
				i++;
			if ( !inEnt )
				continue;
			if ( !haveKey )
			{
				key.swap( tok );
				haveKey = true;
			}
			else
			{
				value.swap( tok );
				haveKey = false;
				if ( IEquals( key.c_str(), "classname" ) )
					classname = value;
				else if ( IEquals( key.c_str(), "model" ) )
					model = value;
				else if ( IEquals( key.c_str(), "origin" ) )
					origin = value;
				else if ( IEquals( key.c_str(), "rendermode" ) )
					rendermode = value;
				else if ( IEquals( key.c_str(), "renderamt" ) )
					renderamt = value;
			}
			continue;
		}
		i++; // whitespace / junk
	}
}

// ---------------------------------------------------------------------------
// floor polygons
// ---------------------------------------------------------------------------
struct FloorPoly
{
	int first;      // index into the xy vertex array (pairs)
	int count;
	float nx, ny, nz, d; // outward plane, world space
	unsigned char water;
};

struct FloorSet
{
	std::vector< float > xy; // x0,y0,x1,y1,...
	std::vector< FloorPoly > polys;
	float minx = 1e30f, maxx = -1e30f, miny = 1e30f, maxy = -1e30f;
};

void CollectModelFloors( const BspView& b, const std::vector< unsigned char >& texCls, int model,
	float ox, float oy, float oz, FloorSet& fs )
{
	if ( model < 0 || model >= b.numModels )
		return;
	const unsigned char* m = b.models + static_cast< size_t >( model ) * kModelSize;
	const int firstFace = RdI32( m + 56 );
	int numFaces = RdI32( m + 60 );
	if ( firstFace < 0 || numFaces <= 0 || firstFace >= b.numFaces )
		return;
	if ( numFaces > b.numFaces - firstFace )
		numFaces = b.numFaces - firstFace;

	float px[kMaxPolyVerts], py[kMaxPolyVerts];
	const int numTexCls = static_cast< int >( texCls.size() );

	for ( int f = 0; f < numFaces; f++ )
	{
		const unsigned char* fp = b.faces + static_cast< size_t >( firstFace + f ) * kFaceSize;
		const int planeNum = RdU16( fp );
		const int side = RdI16( fp + 2 );
		const int firstEdge = RdI32( fp + 4 );
		const int numEdges = RdU16( fp + 8 );
		const int texInfo = RdU16( fp + 10 );

		if ( planeNum >= b.numPlanes )
			continue;
		if ( numEdges < 3 || numEdges > kMaxPolyVerts )
			continue;
		if ( firstEdge < 0 || firstEdge > b.numSurfedges - numEdges )
			continue;

		unsigned char cls = TEX_NORMAL;
		if ( texInfo < b.numTexinfo )
		{
			const int mip = RdI32( b.texinfo + static_cast< size_t >( texInfo ) * kTexinfoSize + 32 );
			if ( mip >= 0 && mip < numTexCls )
				cls = texCls[static_cast< size_t >( mip )];
		}
		if ( cls == TEX_SKIP )
			continue;

		const unsigned char* pl = b.planes + static_cast< size_t >( planeNum ) * kPlaneSize;
		float nx = RdF32( pl ), ny = RdF32( pl + 4 ), nz = RdF32( pl + 8 ), d = RdF32( pl + 12 );
		if ( side != 0 )
		{
			nx = -nx;
			ny = -ny;
			nz = -nz;
			d = -d;
		}
		if ( !( nz >= kFloorNormalZ && nz <= 1.001f ) || !IsSane( nx ) || !IsSane( ny ) || !IsSane( d ) )
			continue;

		int n = 0;
		bool bad = false;
		for ( int e = 0; e < numEdges; e++ )
		{
			const int se = RdI32( b.surfedges + static_cast< size_t >( firstEdge + e ) * kSurfedgeSize );
			int vi;
			if ( se >= 0 )
			{
				if ( se >= b.numEdges )
				{
					bad = true;
					break;
				}
				vi = RdU16( b.edges + static_cast< size_t >( se ) * kEdgeSize );
			}
			else
			{
				if ( se <= -b.numEdges )
				{
					bad = true;
					break;
				}
				vi = RdU16( b.edges + static_cast< size_t >( -se ) * kEdgeSize + 2 );
			}
			if ( vi >= b.numVerts )
			{
				bad = true;
				break;
			}
			const unsigned char* vp = b.verts + static_cast< size_t >( vi ) * kVertSize;
			const float x = RdF32( vp ) + ox, y = RdF32( vp + 4 ) + oy;
			if ( !IsSane( x ) || !IsSane( y ) || !IsSane( RdF32( vp + 8 ) ) )
			{
				bad = true;
				break;
			}
			if ( n > 0 && std::fabs( x - px[n - 1] ) < 0.01f && std::fabs( y - py[n - 1] ) < 0.01f )
				continue;
			px[n] = x;
			py[n] = y;
			n++;
		}
		if ( bad )
			continue;
		if ( n > 1 && std::fabs( px[0] - px[n - 1] ) < 0.01f && std::fabs( py[0] - py[n - 1] ) < 0.01f )
			n--;
		if ( n < 3 )
			continue;

		// orientation in XY, computed relative to vertex 0 in double for precision
		double area2 = 0.0;
		for ( int k = 1; k + 1 < n; k++ )
		{
			const double ax = static_cast< double >( px[k] ) - px[0], ay = static_cast< double >( py[k] ) - py[0];
			const double bx = static_cast< double >( px[k + 1] ) - px[0], by = static_cast< double >( py[k + 1] ) - py[0];
			area2 += ax * by - ay * bx;
		}
		if ( std::fabs( area2 ) < 1.0 )
			continue; // < half a square unit
		if ( area2 < 0.0 )
		{
			for ( int a = 0, z = n - 1; a < z; a++, z-- )
			{
				const float tx = px[a], ty = py[a];
				px[a] = px[z];
				py[a] = py[z];
				px[z] = tx;
				py[z] = ty;
			}
		}

		FloorPoly poly;
		poly.first = static_cast< int >( fs.xy.size() / 2 );
		poly.count = n;
		poly.nx = nx;
		poly.ny = ny;
		poly.nz = nz;
		poly.d = d + nx * ox + ny * oy + nz * oz;
		poly.water = ( cls == TEX_WATER ) ? 1 : 0;
		for ( int k = 0; k < n; k++ )
		{
			fs.xy.push_back( px[k] );
			fs.xy.push_back( py[k] );
			fs.minx = OvMin( fs.minx, px[k] );
			fs.maxx = OvMax( fs.maxx, px[k] );
			fs.miny = OvMin( fs.miny, py[k] );
			fs.maxy = OvMax( fs.maxy, py[k] );
		}
		fs.polys.push_back( poly );
	}
}

// ---------------------------------------------------------------------------
// layer grid
// ---------------------------------------------------------------------------
struct Grid
{
	int w = 0, h = 0, L = 0;
	float x0 = 0.0f, y0 = 0.0f, cell = 16.0f;
	std::vector< float > z;
	std::vector< unsigned char > water;
	std::vector< unsigned char > count;
};

void InsertLayer( Grid& g, size_t cellIdx, float zv, unsigned char wat )
{
	float* zs = &g.z[cellIdx * static_cast< size_t >( g.L )];
	unsigned char* ws = &g.water[cellIdx * static_cast< size_t >( g.L )];
	int n = g.count[cellIdx];

	for ( int k = 0; k < n; k++ )
	{
		if ( std::fabs( zs[k] - zv ) <= kMergeDist )
		{
			if ( zv > zs[k] )
			{
				zs[k] = zv;
				ws[k] = wat;
				// raising it may bring it within reach of the layer above: keep the higher one
				if ( k > 0 && zs[k - 1] - zs[k] <= kMergeDist )
				{
					for ( int j = k; j + 1 < n; j++ )
					{
						zs[j] = zs[j + 1];
						ws[j] = ws[j + 1];
					}
					n--;
					zs[n] = OVERVIEW_NO_FLOOR;
					ws[n] = 0;
					g.count[cellIdx] = static_cast< unsigned char >( n );
				}
			}
			return;
		}
	}

	int pos = 0;
	while ( pos < n && zs[pos] > zv )
		pos++;
	if ( n == g.L )
	{
		if ( pos >= g.L )
			return; // new floor is the lowest: dropped
		n = g.L - 1; // drop the lowest
	}
	for ( int j = n; j > pos; j-- )
	{
		zs[j] = zs[j - 1];
		ws[j] = ws[j - 1];
	}
	zs[pos] = zv;
	ws[pos] = wat;
	g.count[cellIdx] = static_cast< unsigned char >( n + 1 );
}

// Sutherland-Hodgman against one axis-aligned boundary. axis 0 = x, 1 = y.
int ClipAxis( const float* ix, const float* iy, int n, float* ox, float* oy, int axis, float bound, bool keepGreater )
{
	int m = 0;
	for ( int i = 0; i < n; i++ )
	{
		const int j = ( i + 1 == n ) ? 0 : i + 1;
		const float ai = axis ? iy[i] : ix[i];
		const float aj = axis ? iy[j] : ix[j];
		const bool inI = keepGreater ? ( ai >= bound ) : ( ai <= bound );
		const bool inJ = keepGreater ? ( aj >= bound ) : ( aj <= bound );
		if ( inI )
		{
			if ( m >= kClipCap )
				return m;
			ox[m] = ix[i];
			oy[m] = iy[i];
			m++;
		}
		if ( inI != inJ )
		{
			if ( m >= kClipCap )
				return m;
			const float den = aj - ai;
			const float t = ( den != 0.0f ) ? ( bound - ai ) / den : 0.0f;
			ox[m] = ix[i] + t * ( ix[j] - ix[i] );
			oy[m] = iy[i] + t * ( iy[j] - iy[i] );
			m++;
		}
	}
	return m;
}

// Area and centroid of polygon (local coords) clipped to the square [-hs,hs]^2.
float ClippedArea( const float* lx, const float* ly, int n, float hs, float& cx, float& cy )
{
	float ax[kClipCap], ay[kClipCap], bx[kClipCap], by[kClipCap];
	int m = ClipAxis( lx, ly, n, ax, ay, 0, -hs, true );
	m = ClipAxis( ax, ay, m, bx, by, 0, hs, false );
	m = ClipAxis( bx, by, m, ax, ay, 1, -hs, true );
	m = ClipAxis( ax, ay, m, bx, by, 1, hs, false );
	cx = cy = 0.0f;
	if ( m < 3 )
		return 0.0f;
	float a2 = 0.0f, sx = 0.0f, sy = 0.0f;
	for ( int i = 0; i < m; i++ )
	{
		const int j = ( i + 1 == m ) ? 0 : i + 1;
		const float cr = bx[i] * by[j] - bx[j] * by[i];
		a2 += cr;
		sx += ( bx[i] + bx[j] ) * cr;
		sy += ( by[i] + by[j] ) * cr;
	}
	if ( std::fabs( a2 ) < 1e-6f )
		return 0.0f;
	cx = sx / ( 3.0f * a2 );
	cy = sy / ( 3.0f * a2 );
	return std::fabs( a2 ) * 0.5f;
}

void RasterPoly( Grid& g, const FloorSet& fs, const FloorPoly& poly )
{
	const int n = poly.count;
	const float* v = &fs.xy[static_cast< size_t >( poly.first ) * 2];

	float minx = v[0], maxx = v[0], miny = v[1], maxy = v[1];
	float enx[kMaxPolyVerts], eny[kMaxPolyVerts], eax[kMaxPolyVerts], eay[kMaxPolyVerts];
	for ( int i = 0; i < n; i++ )
	{
		const int j = ( i + 1 == n ) ? 0 : i + 1;
		const float ax = v[i * 2], ay = v[i * 2 + 1];
		const float dx = v[j * 2] - ax, dy = v[j * 2 + 1] - ay;
		const float len = std::sqrt( dx * dx + dy * dy );
		eax[i] = ax;
		eay[i] = ay;
		if ( len > 1e-4f )
		{
			// CCW polygon: outward normal is (dy, -dx)
			enx[i] = dy / len;
			eny[i] = -dx / len;
		}
		else
		{
			enx[i] = eny[i] = 0.0f;
		}
		minx = OvMin( minx, ax );
		maxx = OvMax( maxx, ax );
		miny = OvMin( miny, ay );
		maxy = OvMax( maxy, ay );
	}

	const float cell = g.cell;
	int c0 = static_cast< int >( std::floor( ( minx - g.x0 ) / cell ) );
	int c1 = static_cast< int >( std::floor( ( maxx - g.x0 ) / cell ) );
	int r0 = static_cast< int >( std::floor( ( g.y0 - maxy ) / cell ) );
	int r1 = static_cast< int >( std::floor( ( g.y0 - miny ) / cell ) );
	c0 = OvClamp( c0, 0, g.w - 1 );
	c1 = OvClamp( c1, 0, g.w - 1 );
	r0 = OvClamp( r0, 0, g.h - 1 );
	r1 = OvClamp( r1, 0, g.h - 1 );

	const float hs = cell * 0.5f;
	const float halfDiag = cell * 0.7072f;
	const float coverArea = kCoverFrac * cell * cell;
	const float inz = 1.0f / poly.nz;
	float lx[kMaxPolyVerts], ly[kMaxPolyVerts];

	for ( int r = r0; r <= r1; r++ )
	{
		const float cy = g.y0 - ( static_cast< float >( r ) + 0.5f ) * cell;
		for ( int c = c0; c <= c1; c++ )
		{
			const float cx = g.x0 + ( static_cast< float >( c ) + 0.5f ) * cell;
			float maxd = -1e30f;
			for ( int i = 0; i < n; i++ )
			{
				const float dist = enx[i] * ( cx - eax[i] ) + eny[i] * ( cy - eay[i] );
				if ( dist > maxd )
				{
					maxd = dist;
					if ( maxd >= halfDiag )
						break;
				}
			}
			if ( maxd >= halfDiag )
				continue; // cell entirely outside one edge

			float sx = cx, sy = cy;
			if ( maxd > 0.01f )
			{
				// centre outside: count the cell if the polygon covers a good part of it
				for ( int i = 0; i < n; i++ )
				{
					lx[i] = v[i * 2] - cx;
					ly[i] = v[i * 2 + 1] - cy;
				}
				float ccx, ccy;
				const float area = ClippedArea( lx, ly, n, hs, ccx, ccy );
				if ( area < coverArea )
					continue;
				sx = cx + ccx;
				sy = cy + ccy;
			}
			const float zv = ( poly.d - poly.nx * sx - poly.ny * sy ) * inz;
			if ( !IsSane( zv ) )
				continue;
			InsertLayer( g, static_cast< size_t >( r ) * static_cast< size_t >( g.w ) + static_cast< size_t >( c ), zv, poly.water );
		}
	}
}

bool BuildLayersImpl( const unsigned char* bsp, int len, int maxCells, int maxLayers, OverviewLayers& out, char* err, int errLen )
{
	BspView b;
	if ( !OpenBsp( bsp, len, b, err, errLen ) )
		return false;

	std::vector< unsigned char > texCls;
	ClassifyTextures( b, texCls );

	FloorSet fs;
	CollectModelFloors( b, texCls, 0, 0.0f, 0.0f, 0.0f, fs );

	std::vector< BrushEnt > ents;
	ParseEntities( b, ents );
	for ( const BrushEnt& e : ents )
		CollectModelFloors( b, texCls, e.model, e.ox, e.oy, e.oz, fs );

	if ( fs.polys.empty() )
	{
		SetErr( err, errLen, "BSP: no floor faces found" );
		return false;
	}

	maxCells = OvClamp( maxCells, 16, 4096 );
	maxLayers = OvClamp( maxLayers, 1, 16 );

	const float extX = fs.maxx - fs.minx, extY = fs.maxy - fs.miny;
	const float ext = OvMax( extX, extY );
	float cell = ext / static_cast< float >( maxCells - 2 );
	if ( !( cell >= kMinCell ) )
		cell = kMinCell;

	Grid g;
	g.cell = cell;
	g.w = static_cast< int >( std::ceil( extX / cell - 0.001f ) ) + 2;
	g.h = static_cast< int >( std::ceil( extY / cell - 0.001f ) ) + 2;
	g.w = OvMax( g.w, 3 );
	g.h = OvMax( g.h, 3 );
	g.L = maxLayers;
	g.x0 = fs.minx - cell;
	g.y0 = fs.maxy + cell;

	const size_t numCells = static_cast< size_t >( g.w ) * static_cast< size_t >( g.h );
	if ( numCells * static_cast< size_t >( g.L ) > kMaxGridFloats )
	{
		SetErr( err, errLen, "overview: grid too large (lower maxCells/maxLayers)" );
		return false;
	}
	g.z.assign( numCells * static_cast< size_t >( g.L ), OVERVIEW_NO_FLOOR );
	g.water.assign( numCells * static_cast< size_t >( g.L ), 0 );
	g.count.assign( numCells, 0 );

	for ( const FloorPoly& p : fs.polys )
		RasterPoly( g, fs, p );

	// compact to the number of layers actually used
	int used = 0;
	for ( size_t i = 0; i < numCells; i++ )
		used = OvMax( used, static_cast< int >( g.count[i] ) );
	if ( used == 0 )
	{
		SetErr( err, errLen, "overview: floors did not cover any cell" );
		return false;
	}

	out.w = g.w;
	out.h = g.h;
	out.x0 = g.x0;
	out.y0 = g.y0;
	out.cell = g.cell;
	out.layers = used;
	out.z.assign( numCells * static_cast< size_t >( used ), OVERVIEW_NO_FLOOR );
	out.water.assign( numCells * static_cast< size_t >( used ), 0 );
	float zmin = 1e30f, zmax = -1e30f;
	for ( size_t i = 0; i < numCells; i++ )
	{
		const int cnt = g.count[i];
		for ( int k = 0; k < cnt; k++ )
		{
			const float zv = g.z[i * static_cast< size_t >( g.L ) + static_cast< size_t >( k )];
			out.z[i * static_cast< size_t >( used ) + static_cast< size_t >( k )] = zv;
			out.water[i * static_cast< size_t >( used ) + static_cast< size_t >( k )] = g.water[i * static_cast< size_t >( g.L ) + static_cast< size_t >( k )];
			zmin = OvMin( zmin, zv );
			zmax = OvMax( zmax, zv );
		}
	}
	out.zmin = zmin;
	out.zmax = zmax;
	return true;
}

// ---------------------------------------------------------------------------
// rendering
// ---------------------------------------------------------------------------
bool LayersValid( const OverviewLayers& L )
{
	if ( L.w <= 0 || L.h <= 0 || L.layers <= 0 || !( L.cell > 0.0f ) )
		return false;
	const size_t need = static_cast< size_t >( L.w ) * static_cast< size_t >( L.h ) * static_cast< size_t >( L.layers );
	return L.z.size() >= need && L.water.size() >= need;
}

enum CellFlags : unsigned char { CF_FLOOR = 1, CF_WATER = 2, CF_DIM = 4, CF_OUTLINE = 8 };

// Picks the shown layer of every cell and flags outlines. shown[] = height or OVERVIEW_NO_FLOOR.
void ClassifyCells( const OverviewLayers& L, bool useViewer, float viewerZ, std::vector< float >& shown, std::vector< unsigned char >& flags )
{
	const size_t numCells = static_cast< size_t >( L.w ) * static_cast< size_t >( L.h );
	const size_t Ls = static_cast< size_t >( L.layers );
	shown.assign( numCells, OVERVIEW_NO_FLOOR );
	flags.assign( numCells, 0 );
	const float limit = viewerZ + kViewerSlack;

	for ( size_t i = 0; i < numCells; i++ )
	{
		const float* zs = &L.z[i * Ls];
		int pick = -1, lowest = -1;
		for ( int k = 0; k < L.layers; k++ )
		{
			if ( zs[k] <= OVERVIEW_NO_FLOOR * 0.5f )
				break;
			lowest = k;
			if ( pick < 0 && ( !useViewer || zs[k] <= limit ) )
				pick = k;
		}
		if ( lowest < 0 )
			continue;
		unsigned char f = CF_FLOOR;
		if ( pick < 0 )
		{
			pick = lowest;
			f |= CF_DIM;
		}
		if ( L.water[i * Ls + static_cast< size_t >( pick )] )
			f |= CF_WATER;
		shown[i] = zs[pick];
		flags[i] = f;
	}

	// outlines: border to empty space, or the higher side of a step > kEdgeStep
	for ( int r = 0; r < L.h; r++ )
	{
		for ( int c = 0; c < L.w; c++ )
		{
			const size_t i = static_cast< size_t >( r ) * static_cast< size_t >( L.w ) + static_cast< size_t >( c );
			if ( !( flags[i] & CF_FLOOR ) )
				continue;
			const float zc = shown[i];
			bool edge = false;
			const int nc[4] = { c - 1, c + 1, c, c };
			const int nr[4] = { r, r, r - 1, r + 1 };
			for ( int k = 0; k < 4 && !edge; k++ )
			{
				if ( nc[k] < 0 || nc[k] >= L.w || nr[k] < 0 || nr[k] >= L.h )
				{
					edge = true;
					break;
				}
				const size_t j = static_cast< size_t >( nr[k] ) * static_cast< size_t >( L.w ) + static_cast< size_t >( nc[k] );
				if ( !( flags[j] & CF_FLOOR ) || zc - shown[j] > kEdgeStep )
					edge = true;
			}
			if ( edge )
				flags[i] |= CF_OUTLINE;
		}
	}
}

inline float HeightT( const OverviewLayers& L, float zv )
{
	const float range = L.zmax - L.zmin;
	if ( !( range > 1.0f ) )
		return 0.5f;
	return OvClamp( ( zv - L.zmin ) / range, 0.0f, 1.0f );
}

inline unsigned char ToByte( float v )
{
	return static_cast< unsigned char >( OvClamp( v, 0.0f, 255.0f ) + 0.5f ); // <= 255.5 truncates to 255
}

// radar look: cool grey-blue floor brightening with height, blue water
inline void FloorColour( float t, bool water, float rgb[3] )
{
	const float b = 70.0f + 120.0f * t;
	if ( water )
	{
		rgb[0] = b * 0.30f;
		rgb[1] = b * 0.55f + 12.0f;
		rgb[2] = b * 0.90f + 50.0f;
	}
	else
	{
		rgb[0] = b * 0.80f;
		rgb[1] = b * 0.87f;
		rgb[2] = b;
	}
}

const float kOutlineRGB[3] = { 220.0f, 225.0f, 235.0f };
const float kDimScale = 0.55f;
const float kDimOutlineScale = 0.62f;

void RenderImpl( const OverviewLayers& L, bool useViewer, float viewerZ, unsigned char* rgba )
{
	std::vector< float > shown;
	std::vector< unsigned char > flags;
	ClassifyCells( L, useViewer, viewerZ, shown, flags );

	const size_t numCells = static_cast< size_t >( L.w ) * static_cast< size_t >( L.h );
	for ( size_t i = 0; i < numCells; i++ )
	{
		unsigned char* px = rgba + i * 4;
		const unsigned char f = flags[i];
		if ( !( f & CF_FLOOR ) )
		{
			px[0] = px[1] = px[2] = px[3] = 0;
			continue;
		}
		float rgb[3];
		unsigned char a;
		if ( f & CF_OUTLINE )
		{
			const float s = ( f & CF_DIM ) ? kDimOutlineScale : 1.0f;
			rgb[0] = kOutlineRGB[0] * s;
			rgb[1] = kOutlineRGB[1] * s;
			rgb[2] = kOutlineRGB[2] * s;
			a = 255;
		}
		else
		{
			FloorColour( HeightT( L, shown[i] ), ( f & CF_WATER ) != 0, rgb );
			if ( f & CF_DIM )
			{
				rgb[0] *= kDimScale;
				rgb[1] *= kDimScale;
				rgb[2] *= kDimScale;
			}
			a = 220;
		}
		px[0] = ToByte( rgb[0] );
		px[1] = ToByte( rgb[1] );
		px[2] = ToByte( rgb[2] );
		px[3] = a;
	}
}

void BuildClassicPalette( unsigned char pal[256][3] )
{
	pal[0][0] = 0;
	pal[0][1] = 255;
	pal[0][2] = 0;
	float rgb[3];
	for ( int i = 0; i < kPalFloorCount; i++ )
	{
		FloorColour( static_cast< float >( i ) / static_cast< float >( kPalFloorCount - 1 ), false, rgb );
		for ( int k = 0; k < 3; k++ )
			pal[kPalFloorFirst + i][k] = ToByte( rgb[k] );
	}
	for ( int i = 0; i < kPalWaterCount; i++ )
	{
		FloorColour( static_cast< float >( i ) / static_cast< float >( kPalWaterCount - 1 ), true, rgb );
		for ( int k = 0; k < 3; k++ )
			pal[kPalWaterFirst + i][k] = ToByte( rgb[k] );
	}
	for ( int i = kPalOutline; i < 256; i++ )
	{
		for ( int k = 0; k < 3; k++ )
			pal[i][k] = ToByte( kOutlineRGB[k] );
	}
}

void RenderClassicImpl( const OverviewLayers& L, float zoom, const float origin[3], int rotated,
	std::vector< unsigned char >& index, unsigned char palette[256][3] )
{
	BuildClassicPalette( palette );
	index.assign( static_cast< size_t >( kClassicW ) * static_cast< size_t >( kClassicH ), 0 );
	if ( !LayersValid( L ) || !( zoom > 0.0f ) || !origin )
		return;

	std::vector< float > shown;
	std::vector< unsigned char > flags;
	ClassifyCells( L, false, 0.0f, shown, flags );

	const size_t numCells = static_cast< size_t >( L.w ) * static_cast< size_t >( L.h );
	std::vector< unsigned char > cellIdx( numCells, 0 );
	for ( size_t i = 0; i < numCells; i++ )
	{
		const unsigned char f = flags[i];
		if ( !( f & CF_FLOOR ) )
			continue;
		if ( f & CF_OUTLINE )
		{
			cellIdx[i] = static_cast< unsigned char >( kPalOutline );
			continue;
		}
		const float t = HeightT( L, shown[i] );
		if ( f & CF_WATER )
			cellIdx[i] = static_cast< unsigned char >( kPalWaterFirst + static_cast< int >( t * ( kPalWaterCount - 1 ) + 0.5f ) );
		else
			cellIdx[i] = static_cast< unsigned char >( kPalFloorFirst + static_cast< int >( t * ( kPalFloorCount - 1 ) + 0.5f ) );
	}

	// world rectangle covered by the image (CHudSpectator::DrawOverviewLayer with aspect 4/3)
	const float halfU = 4096.0f / zoom;             // along image width
	const float halfV = 4096.0f / ( zoom * kAspect ); // along image height
	const float ox = origin[0], oy = origin[1];
	const float invCell = 1.0f / L.cell;

	for ( int py = 0; py < kClassicH; py++ )
	{
		const float v = ( static_cast< float >( py ) + 0.5f ) / static_cast< float >( kClassicH );
		unsigned char* row = &index[static_cast< size_t >( py ) * static_cast< size_t >( kClassicW )];
		for ( int px = 0; px < kClassicW; px++ )
		{
			const float u = ( static_cast< float >( px ) + 0.5f ) / static_cast< float >( kClassicW );
			float wx, wy;
			if ( rotated )
			{
				wx = ox - halfU + u * 2.0f * halfU;
				wy = oy + halfV - v * 2.0f * halfV;
			}
			else
			{
				wy = oy + halfU - u * 2.0f * halfU;
				wx = ox + halfV - v * 2.0f * halfV;
			}
			const float fc = ( wx - L.x0 ) * invCell;
			const float fr = ( L.y0 - wy ) * invCell;
			if ( !( fc >= 0.0f ) || !( fr >= 0.0f ) )
				continue;
			const int c = static_cast< int >( fc ), r = static_cast< int >( fr );
			if ( c >= L.w || r >= L.h )
				continue;
			row[px] = cellIdx[static_cast< size_t >( r ) * static_cast< size_t >( L.w ) + static_cast< size_t >( c )];
		}
	}
}

inline void PutU16( std::vector< unsigned char >& v, unsigned int x )
{
	v.push_back( static_cast< unsigned char >( x & 0xFF ) );
	v.push_back( static_cast< unsigned char >( ( x >> 8 ) & 0xFF ) );
}

inline void PutU32( std::vector< unsigned char >& v, unsigned int x )
{
	PutU16( v, x & 0xFFFF );
	PutU16( v, ( x >> 16 ) & 0xFFFF );
}

void WriteBmp8( const std::vector< unsigned char >& index, const unsigned char pal[256][3], std::vector< unsigned char >& file )
{
	const unsigned int imageSize = static_cast< unsigned int >( kClassicW * kClassicH ); // 1024 is 4-aligned: no row padding
	const unsigned int offBits = 14u + 40u + 256u * 4u;
	file.clear();
	file.reserve( offBits + imageSize );
	// BITMAPFILEHEADER
	file.push_back( 'B' );
	file.push_back( 'M' );
	PutU32( file, offBits + imageSize );
	PutU16( file, 0 );
	PutU16( file, 0 );
	PutU32( file, offBits );
	// BITMAPINFOHEADER
	PutU32( file, 40 );
	PutU32( file, static_cast< unsigned int >( kClassicW ) );
	PutU32( file, static_cast< unsigned int >( kClassicH ) ); // positive = bottom-up
	PutU16( file, 1 );   // planes
	PutU16( file, 8 );   // bpp
	PutU32( file, 0 );   // BI_RGB
	PutU32( file, imageSize );
	PutU32( file, 2835 ); // 72 dpi
	PutU32( file, 2835 );
	PutU32( file, 256 );
	PutU32( file, 256 );
	// palette (B,G,R,0)
	for ( int i = 0; i < 256; i++ )
	{
		file.push_back( pal[i][2] );
		file.push_back( pal[i][1] );
		file.push_back( pal[i][0] );
		file.push_back( 0 );
	}
	for ( int y = kClassicH - 1; y >= 0; y-- )
	{
		const unsigned char* row = &index[static_cast< size_t >( y ) * static_cast< size_t >( kClassicW )];
		file.insert( file.end(), row, row + kClassicW );
	}
}

bool MakeClassicImpl( const OverviewLayers& L, std::vector< unsigned char >& bmpFile, float& zoom, float origin[3], int& rotated )
{
	if ( !LayersValid( L ) )
		return false;

	// extents of all non-empty cells
	int cmin = L.w, cmax = -1, rmin = L.h, rmax = -1;
	const size_t Ls = static_cast< size_t >( L.layers );
	for ( int r = 0; r < L.h; r++ )
	{
		for ( int c = 0; c < L.w; c++ )
		{
			const size_t i = static_cast< size_t >( r ) * static_cast< size_t >( L.w ) + static_cast< size_t >( c );
			if ( L.z[i * Ls] <= OVERVIEW_NO_FLOOR * 0.5f )
				continue;
			cmin = OvMin( cmin, c );
			cmax = OvMax( cmax, c );
			rmin = OvMin( rmin, r );
			rmax = OvMax( rmax, r );
		}
	}
	if ( cmax < 0 )
		return false;

	const float wx0 = L.x0 + static_cast< float >( cmin ) * L.cell;
	const float wx1 = L.x0 + static_cast< float >( cmax + 1 ) * L.cell;
	const float wy1 = L.y0 - static_cast< float >( rmin ) * L.cell;
	const float wy0 = L.y0 - static_cast< float >( rmax + 1 ) * L.cell;
	const float extX = OvMax( wx1 - wx0, 64.0f );
	const float extY = OvMax( wy1 - wy0, 64.0f );
	const float margin = 1.08f; // ~4% on each side

	rotated = ( extX >= extY ) ? 1 : 0;
	const float extU = rotated ? extX : extY; // along the image width (covers 8192/zoom)
	const float extV = rotated ? extY : extX; // along the image height (covers 8192/(zoom*4/3))
	float z = OvMin( 8192.0f / ( extU * margin ), ( 8192.0f / kAspect ) / ( extV * margin ) );
	z = std::floor( z * 100.0f ) / 100.0f;
	zoom = OvClamp( z, 0.01f, 99.99f );

	origin[0] = std::floor( ( wx0 + wx1 ) * 0.5f + 0.5f );
	origin[1] = std::floor( ( wy0 + wy1 ) * 0.5f + 0.5f );
	origin[2] = std::floor( L.zmin + 0.5f );

	std::vector< unsigned char > index;
	unsigned char pal[256][3];
	RenderClassicImpl( L, zoom, origin, rotated, index, pal );
	WriteBmp8( index, pal, bmpFile );
	return true;
}

// ---------------------------------------------------------------------------
// picture: the map's real surfaces seen from above (textures + lightmaps)
// ---------------------------------------------------------------------------
const int   LUMP_LIGHTING  = 8;
const int   kPicDepth      = 4;      // surfaces kept per pixel
const int   kPicMerge      = 16;     // surfaces closer than this in one pixel are one
const float kPicMinNz      = 0.05f;  // faces turned up at least this much are seen from above
const float kStepUp        = 18.0f;  // walk up
const float kJumpUp        = 64.0f;  // crouch-jump up
const float kHeadroom      = 36.0f;  // crouched player
const float kTexGamma      = 0.8f;   // texgamma 2.0 / gamma 2.5
const float kLightGamma    = 2.5f;   // the engine's lightgamma / gamma
const float kBrightness    = 1.0f;   // the engine's brightness (lifts dark light)
const float kUnreachScale  = 0.45f;  // roofs, wall tops
const float kUpperDimScale = 0.65f;  // a floor above the viewer
const float kPicGain       = 1.3f;   // the engine draws light brighter than the raw lightmap
const int   kMaxMiptexDim  = 4096;

struct EntKV
{
	std::string classname, model, origin, rendermode, renderamt, wad;
};

void ParseAllEntities( const BspView& b, std::vector< EntKV >& out )
{
	out.clear();
	const char* p = reinterpret_cast< const char* >( b.Lump( LUMP_ENTITIES ) );
	const int n = b.size[LUMP_ENTITIES];
	int i = 0;
	bool inEnt = false, haveKey = false;
	std::string key;
	EntKV cur;
	while ( i < n )
	{
		const char c = p[i];
		if ( c == 0 )
			break;
		if ( c == '{' )
		{
			inEnt = true;
			haveKey = false;
			cur = EntKV();
			i++;
			continue;
		}
		if ( c == '}' )
		{
			if ( inEnt )
				out.push_back( cur );
			inEnt = false;
			haveKey = false;
			i++;
			continue;
		}
		if ( c == '"' )
		{
			i++;
			const int start = i;
			while ( i < n && p[i] != '"' && p[i] != 0 )
				i++;
			int slen = i - start;
			if ( slen > 4096 )
				slen = 4096;
			std::string tok( p + start, static_cast< size_t >( slen ) );
			if ( i < n && p[i] == '"' )
				i++;
			if ( !inEnt )
				continue;
			if ( !haveKey )
			{
				key.swap( tok );
				haveKey = true;
			}
			else
			{
				haveKey = false;
				const char* k = key.c_str();
				if ( IEquals( k, "classname" ) )
					cur.classname.swap( tok );
				else if ( IEquals( k, "model" ) )
					cur.model.swap( tok );
				else if ( IEquals( k, "origin" ) )
					cur.origin.swap( tok );
				else if ( IEquals( k, "rendermode" ) )
					cur.rendermode.swap( tok );
				else if ( IEquals( k, "renderamt" ) )
					cur.renderamt.swap( tok );
				else if ( IEquals( k, "wad" ) )
					cur.wad.swap( tok );
			}
			continue;
		}
		i++;
	}
}

int EntModelIndex( const BspView& b, const EntKV& e )
{
	if ( e.model.size() < 2 || e.model[0] != '*' )
		return -1;
	const long m = strtol( e.model.c_str() + 1, nullptr, 10 );
	return ( m > 0 && m < b.numModels ) ? static_cast< int >( m ) : -1;
}

// ---- textures -------------------------------------------------------------
struct PicTexture
{
	char name[17] = "";
	int w = 0, h = 0;
	const unsigned char* mip[4] = { nullptr, nullptr, nullptr, nullptr };
	const unsigned char* pal = nullptr;
	std::vector< unsigned char > own;   // copy of a WAD miptex
	std::vector< unsigned char > rgba[4];
	bool masked = false, water = false, skip = false, ok = false;
};

// miptex at p with 'avail' bytes after it
bool ReadMiptex( const unsigned char* p, size_t avail, PicTexture& t )
{
	if ( avail < 40 )
		return false;
	const int w = RdI32( p + 16 ), h = RdI32( p + 20 );
	if ( w < 8 || h < 8 || w > kMaxMiptexDim || h > kMaxMiptexDim )
		return false;
	const unsigned char* mips[4];
	size_t end3 = 0;
	for ( int m = 0; m < 4; m++ )
	{
		const int off = RdI32( p + 24 + 4 * m );
		const size_t size = static_cast< size_t >( w >> m ) * static_cast< size_t >( h >> m );
		if ( off < 40 || static_cast< size_t >( off ) > avail || size == 0 || size > avail - static_cast< size_t >( off ) )
			return false;
		mips[m] = p + off;
		if ( m == 3 )
			end3 = static_cast< size_t >( off ) + size;
	}
	if ( end3 + 2 + 768 > avail )
		return false;
	for ( int m = 0; m < 4; m++ )
		t.mip[m] = mips[m];
	t.pal = p + end3 + 2;
	t.w = w;
	t.h = h;
	t.ok = true;
	return true;
}

void LowerName( const char* in, char out[17] )
{
	int i = 0;
	for ( ; i < 16 && in[i]; i++ )
		out[i] = ToLowerAscii( in[i] );
	out[i] = 0;
}

struct WadEntry
{
	char name[17];
	int pos, size;
};

// WAD2/WAD3 directory of uncompressed miptex lumps
void IndexWad( const std::vector< unsigned char >& wad, std::vector< WadEntry >& dir )
{
	dir.clear();
	const size_t n = wad.size();
	if ( n < 12 || ( memcmp( wad.data(), "WAD3", 4 ) && memcmp( wad.data(), "WAD2", 4 ) ) )
		return;
	const int num = RdI32( wad.data() + 4 ), ofs = RdI32( wad.data() + 8 );
	if ( num <= 0 || ofs < 12 || static_cast< size_t >( ofs ) > n || static_cast< size_t >( num ) > ( n - static_cast< size_t >( ofs ) ) / 32 )
		return;
	for ( int i = 0; i < num; i++ )
	{
		const unsigned char* e = wad.data() + ofs + static_cast< size_t >( i ) * 32;
		const int pos = RdI32( e ), disk = RdI32( e + 4 );
		const unsigned char type = e[12], comp = e[13];
		if ( ( type != 0x43 && type != 0x44 ) || comp != 0 )
			continue;
		if ( pos < 0 || disk <= 0 || static_cast< size_t >( pos ) > n || static_cast< size_t >( disk ) > n - static_cast< size_t >( pos ) )
			continue;
		WadEntry w;
		char raw[17];
		memcpy( raw, e + 16, 16 );
		raw[16] = 0;
		LowerName( raw, w.name );
		w.pos = pos;
		w.size = disk;
		dir.push_back( w );
	}
}

void ClassifyPicTexture( PicTexture& t )
{
	const char* nm = t.name;
	t.water = nm[0] == '!' || IStartsWith( nm, "water" );
	t.masked = nm[0] == '{';
	static const char* const skipNames[] = { "sky", "aaatrigger", "clip", "null", "origin", "bevel", "hint", "skip", "black_hidden" };
	for ( const char* s : skipNames )
	{
		if ( IEquals( nm, s ) )
			t.skip = true;
	}
}

void LoadTextures( const BspView& b, const std::vector< EntKV >& ents, OverviewLoadFile loader, void* ctx,
	std::vector< PicTexture >& tex, int& wadsLoaded, int& wadsMissing )
{
	tex.clear();
	wadsLoaded = wadsMissing = 0;
	const int l = b.size[LUMP_TEXTURES];
	if ( l < 4 )
		return;
	const unsigned char* p = b.Lump( LUMP_TEXTURES );
	int n = RdI32( p );
	if ( n <= 0 )
		return;
	if ( n > ( l - 4 ) / 4 )
		n = ( l - 4 ) / 4;
	tex.resize( static_cast< size_t >( n ) );
	int unresolved = 0;
	for ( int i = 0; i < n; i++ )
	{
		PicTexture& t = tex[static_cast< size_t >( i )];
		const int o = RdI32( p + 4 + 4 * i );
		if ( o < 0 || o > l - 40 )
			continue;
		char raw[17];
		memcpy( raw, p + o, 16 );
		raw[16] = 0;
		LowerName( raw, t.name );
		ClassifyPicTexture( t );
		if ( RdI32( p + o + 24 ) != 0 )
			ReadMiptex( p + o, static_cast< size_t >( l - o ), t );
		if ( !t.ok && !t.skip && t.name[0] )
			unresolved++;
	}
	if ( !unresolved || !loader )
		return;

	// the map's WAD list: "\half-life\cstrike\cs_dust.wad;\half-life\valve\halflife.wad"
	std::string wadKey;
	for ( const EntKV& e : ents )
	{
		if ( IEquals( e.classname.c_str(), "worldspawn" ) )
		{
			wadKey = e.wad;
			break;
		}
	}
	std::vector< std::string > names;
	size_t start = 0;
	while ( start < wadKey.size() )
	{
		size_t semi = wadKey.find( ';', start );
		if ( semi == std::string::npos )
			semi = wadKey.size();
		std::string item = wadKey.substr( start, semi - start );
		const size_t slash = item.find_last_of( "\\/" );
		if ( slash != std::string::npos )
			item = item.substr( slash + 1 );
		while ( !item.empty() && ( item.back() == ' ' || item.back() == '\t' ) )
			item.pop_back();
		if ( !item.empty() )
			names.push_back( item );
		start = semi + 1;
	}
	const size_t listed = names.size();
	// textures often come from these even when the list does not say so
	static const char* const extra[] = { "cstrike.wad", "halflife.wad", "decals.wad", "liquids.wad", "xeno.wad" };
	for ( const char* x : extra )
	{
		bool have = false;
		for ( const std::string& s : names )
			have = have || IEquals( s.c_str(), x );
		if ( !have )
			names.push_back( x );
	}

	std::vector< unsigned char > wad;
	std::vector< WadEntry > dir;
	for ( size_t w = 0; w < names.size() && unresolved > 0; w++ )
	{
		wad.clear();
		if ( !loader( names[w].c_str(), wad, ctx ) || wad.empty() )
		{
			if ( w < listed )
				wadsMissing++;
			continue;
		}
		wadsLoaded++;
		IndexWad( wad, dir );
		for ( PicTexture& t : tex )
		{
			if ( t.ok || t.skip || !t.name[0] )
				continue;
			for ( const WadEntry& e : dir )
			{
				if ( strcmp( e.name, t.name ) )
					continue;
				t.own.assign( wad.begin() + e.pos, wad.begin() + e.pos + e.size );
				if ( ReadMiptex( t.own.data(), t.own.size(), t ) )
					unresolved--;
				else
					t.own.clear();
				break;
			}
		}
	}
}

unsigned char g_TexGammaTable[256];
float g_LightCurve[257];
bool g_TexGammaReady = false;

void BuildTexGamma()
{
	if ( g_TexGammaReady )
		return;
	for ( int i = 0; i < 256; i++ )
		g_TexGammaTable[i] = ToByte( 255.0f * std::pow( static_cast< float >( i ) / 255.0f, kTexGamma ) );
	// GoldSrc's light gamma table: to linear, lift the darks by 'brightness', back to gamma
	const float g3 = kBrightness <= 0.0f ? 0.125f : ( kBrightness > 1.0f ? 0.05f : 0.125f - kBrightness * kBrightness * 0.075f );
	for ( int i = 0; i < 256; i++ )
	{
		float f = std::pow( static_cast< float >( i ) / 255.0f, kLightGamma );
		f = f <= g3 ? f / g3 * 0.125f : 0.125f + ( f - g3 ) / ( 1.0f - g3 ) * 0.875f;
		g_LightCurve[i] = OvMin( 255.0f * std::pow( f, 1.0f / kLightGamma ), 255.0f );
	}
	g_LightCurve[256] = g_LightCurve[255];
	g_TexGammaReady = true;
}

const std::vector< unsigned char >& DecodedMip( PicTexture& t, int m )
{
	std::vector< unsigned char >& out = t.rgba[m];
	if ( !out.empty() || !t.ok )
		return out;
	const int w = t.w >> m, h = t.h >> m;
	out.resize( static_cast< size_t >( w ) * static_cast< size_t >( h ) * 4 );
	const unsigned char* src = t.mip[m];
	for ( int i = 0; i < w * h; i++ )
	{
		const int idx = src[i];
		unsigned char* d = &out[static_cast< size_t >( i ) * 4];
		d[0] = g_TexGammaTable[t.pal[idx * 3 + 0]];
		d[1] = g_TexGammaTable[t.pal[idx * 3 + 1]];
		d[2] = g_TexGammaTable[t.pal[idx * 3 + 2]];
		d[3] = ( t.masked && idx == 255 ) ? 0 : 255;
	}
	return out;
}

inline int WrapI( int v, int n )
{
	v %= n;
	return v < 0 ? v + n : v;
}

// texel at (s,t) of mip m (s,t in mip-0 texels); false when see-through
bool SampleTex( PicTexture& t, int m, float s, float tt, float rgb[3] )
{
	const std::vector< unsigned char >& img = DecodedMip( t, m );
	if ( img.empty() )
		return false;
	const int w = t.w >> m, h = t.h >> m;
	const float scale = 1.0f / static_cast< float >( 1 << m );
	const float fs = s * scale - 0.5f, ft = tt * scale - 0.5f;
	if ( !( fs > -1.0e7f && fs < 1.0e7f && ft > -1.0e7f && ft < 1.0e7f ) )
		return false;
	if ( t.masked )
	{
		const int x = WrapI( static_cast< int >( std::floor( fs + 0.5f ) ), w ), y = WrapI( static_cast< int >( std::floor( ft + 0.5f ) ), h );
		const unsigned char* px = &img[( static_cast< size_t >( y ) * static_cast< size_t >( w ) + static_cast< size_t >( x ) ) * 4];
		if ( !px[3] )
			return false;
		rgb[0] = px[0];
		rgb[1] = px[1];
		rgb[2] = px[2];
		return true;
	}
	const float flx = std::floor( fs ), fly = std::floor( ft );
	const float ax = fs - flx, ay = ft - fly;
	const int x0 = WrapI( static_cast< int >( flx ), w ), y0 = WrapI( static_cast< int >( fly ), h );
	const int x1 = ( x0 + 1 ) % w, y1 = ( y0 + 1 ) % h;
	const size_t W = static_cast< size_t >( w );
	const unsigned char* a = &img[( static_cast< size_t >( y0 ) * W + static_cast< size_t >( x0 ) ) * 4];
	const unsigned char* bb = &img[( static_cast< size_t >( y0 ) * W + static_cast< size_t >( x1 ) ) * 4];
	const unsigned char* c = &img[( static_cast< size_t >( y1 ) * W + static_cast< size_t >( x0 ) ) * 4];
	const unsigned char* d = &img[( static_cast< size_t >( y1 ) * W + static_cast< size_t >( x1 ) ) * 4];
	for ( int k = 0; k < 3; k++ )
	{
		const float top = a[k] + ( bb[k] - a[k] ) * ax;
		const float bot = c[k] + ( d[k] - c[k] ) * ax;
		rgb[k] = top + ( bot - top ) * ay;
	}
	return true;
}

// ---- picture build ---------------------------------------------------------
struct PicBuild
{
	OverviewPicture* P = nullptr;
	const BspView* b = nullptr;
	std::vector< PicTexture >* tex = nullptr;
	const unsigned char* light = nullptr;
	int lightLen = 0;
	int missing = 0;
	std::vector< unsigned char > missingSeen;
};

inline uint16_t Pack565( float r, float g, float bl )
{
	const int R = static_cast< int >( OvClamp( r, 0.0f, 255.0f ) + 0.5f ) >> 3;
	const int G = static_cast< int >( OvClamp( g, 0.0f, 255.0f ) + 0.5f ) >> 2;
	const int B = static_cast< int >( OvClamp( bl, 0.0f, 255.0f ) + 0.5f ) >> 3;
	return static_cast< uint16_t >( ( OvMin( R, 31 ) << 11 ) | ( OvMin( G, 63 ) << 5 ) | OvMin( B, 31 ) );
}

inline void Unpack565( uint16_t c, float rgb[3] )
{
	const int R = ( c >> 11 ) & 31, G = ( c >> 5 ) & 63, B = c & 31;
	rgb[0] = static_cast< float >( ( R << 3 ) | ( R >> 2 ) );
	rgb[1] = static_cast< float >( ( G << 2 ) | ( G >> 4 ) );
	rgb[2] = static_cast< float >( ( B << 3 ) | ( B >> 2 ) );
}

void PicInsert( OverviewPicture& P, size_t pix, float zf, uint16_t col )
{
	const int D = P.depth;
	int16_t* zs = &P.z[pix * static_cast< size_t >( D )];
	uint16_t* cs = &P.rgb[pix * static_cast< size_t >( D )];
	const int z = static_cast< int >( std::floor( OvClamp( zf, -32000.0f, 32000.0f ) + 0.5f ) );
	int n = 0;
	while ( n < D && zs[n] != OVERVIEW_PIC_NONE )
		n++;
	for ( int k = 0; k < n; k++ )
	{
		if ( std::abs( zs[k] - z ) <= kPicMerge )
		{
			if ( z > zs[k] )
			{
				zs[k] = static_cast< int16_t >( z );
				cs[k] = col;
				// raised into reach of the one above: keep the higher one
				if ( k > 0 && zs[k - 1] - zs[k] <= kPicMerge )
				{
					for ( int j = k; j + 1 < n; j++ )
					{
						zs[j] = zs[j + 1];
						cs[j] = cs[j + 1];
					}
					zs[n - 1] = static_cast< int16_t >( OVERVIEW_PIC_NONE );
					cs[n - 1] = 0;
				}
			}
			return;
		}
	}
	int pos = 0;
	while ( pos < n && zs[pos] > z )
		pos++;
	if ( n == D )
	{
		if ( pos >= D )
			return; // lowest of too many: dropped
		n = D - 1;
	}
	for ( int j = n; j > pos; j-- )
	{
		zs[j] = zs[j - 1];
		cs[j] = cs[j - 1];
	}
	zs[pos] = static_cast< int16_t >( z );
	cs[pos] = col;
}

void PicDrawModel( PicBuild& pb, int model, float ox, float oy, float oz )
{
	const BspView& b = *pb.b;
	OverviewPicture& P = *pb.P;
	if ( model < 0 || model >= b.numModels )
		return;
	const unsigned char* m = b.models + static_cast< size_t >( model ) * kModelSize;
	const int firstFace = RdI32( m + 56 );
	int numFaces = RdI32( m + 60 );
	if ( firstFace < 0 || numFaces <= 0 || firstFace >= b.numFaces )
		return;
	if ( numFaces > b.numFaces - firstFace )
		numFaces = b.numFaces - firstFace;

	float sx[kMaxPolyVerts], sy[kMaxPolyVerts]; // pixel space
	const float inv = 1.0f / P.unit;
	const int numTex = static_cast< int >( pb.tex->size() );

	for ( int f = 0; f < numFaces; f++ )
	{
		const unsigned char* fp = b.faces + static_cast< size_t >( firstFace + f ) * kFaceSize;
		const int planeNum = RdU16( fp );
		const int side = RdI16( fp + 2 );
		const int firstEdge = RdI32( fp + 4 );
		const int numEdges = RdU16( fp + 8 );
		const int texInfo = RdU16( fp + 10 );
		const unsigned char style0 = fp[12];
		const int lightOfs = RdI32( fp + 16 );
		if ( planeNum >= b.numPlanes || numEdges < 3 || numEdges > kMaxPolyVerts || firstEdge < 0 || firstEdge > b.numSurfedges - numEdges )
			continue;
		if ( texInfo >= b.numTexinfo )
			continue;
		const unsigned char* ti = b.texinfo + static_cast< size_t >( texInfo ) * kTexinfoSize;
		float sv[4], tv[4];
		bool bad = false;
		for ( int k = 0; k < 4; k++ )
		{
			sv[k] = RdF32( ti + 4 * k );
			tv[k] = RdF32( ti + 16 + 4 * k );
			bad = bad || !IsSane( sv[k] ) || !IsSane( tv[k] );
		}
		if ( bad )
			continue;
		const int mipIdx = RdI32( ti + 32 );
		const int tiFlags = RdI32( ti + 36 );
		PicTexture* tex = ( mipIdx >= 0 && mipIdx < numTex ) ? &( *pb.tex )[static_cast< size_t >( mipIdx )] : nullptr;
		if ( tex && tex->skip )
			continue;

		const unsigned char* pl = b.planes + static_cast< size_t >( planeNum ) * kPlaneSize;
		float nx = RdF32( pl ), ny = RdF32( pl + 4 ), nz = RdF32( pl + 8 ), d = RdF32( pl + 12 );
		if ( side != 0 )
		{
			nx = -nx;
			ny = -ny;
			nz = -nz;
			d = -d;
		}
		if ( !( nz > kPicMinNz && nz <= 1.001f ) || !IsSane( nx ) || !IsSane( ny ) || !IsSane( d ) )
			continue;

		int n = 0;
		float smin = 1e30f, smax = -1e30f, tmin = 1e30f, tmax = -1e30f;
		float pminx = 1e30f, pmaxx = -1e30f, pminy = 1e30f, pmaxy = -1e30f;
		for ( int e = 0; e < numEdges; e++ )
		{
			const int se = RdI32( b.surfedges + static_cast< size_t >( firstEdge + e ) * kSurfedgeSize );
			int vi;
			if ( se >= 0 )
			{
				if ( se >= b.numEdges )
				{
					bad = true;
					break;
				}
				vi = RdU16( b.edges + static_cast< size_t >( se ) * kEdgeSize );
			}
			else
			{
				if ( se <= -b.numEdges )
				{
					bad = true;
					break;
				}
				vi = RdU16( b.edges + static_cast< size_t >( -se ) * kEdgeSize + 2 );
			}
			if ( vi >= b.numVerts )
			{
				bad = true;
				break;
			}
			const unsigned char* vp = b.verts + static_cast< size_t >( vi ) * kVertSize;
			const float x = RdF32( vp ), y = RdF32( vp + 4 ), z = RdF32( vp + 8 );
			if ( !IsSane( x ) || !IsSane( y ) || !IsSane( z ) )
			{
				bad = true;
				break;
			}
			const float s = x * sv[0] + y * sv[1] + z * sv[2] + sv[3];
			const float t = x * tv[0] + y * tv[1] + z * tv[2] + tv[3];
			smin = OvMin( smin, s );
			smax = OvMax( smax, s );
			tmin = OvMin( tmin, t );
			tmax = OvMax( tmax, t );
			sx[n] = ( x + ox - P.x0 ) * inv;
			sy[n] = ( P.y0 - ( y + oy ) ) * inv;
			pminx = OvMin( pminx, sx[n] );
			pmaxx = OvMax( pmaxx, sx[n] );
			pminy = OvMin( pminy, sy[n] );
			pmaxy = OvMax( pmaxy, sy[n] );
			n++;
		}
		if ( bad || n < 3 )
			continue;
		if ( pmaxx < 0.0f || pmaxy < 0.0f || pminx > static_cast< float >( P.w ) || pminy > static_cast< float >( P.h ) )
			continue;

		// lightmap (GoldSrc: one luxel per 16 texels, extents from floor/ceil of the texture coordinates)
		const unsigned char* lm = nullptr;
		int lw = 0, lh = 0;
		float lsmin = 0.0f, ltmin = 0.0f;
		const bool special = ( tiFlags & 1 ) != 0 || ( tex && tex->water );
		if ( !special && pb.light && lightOfs >= 0 && style0 != 255 )
		{
			const float bs0 = std::floor( smin / 16.0f ), bs1 = std::ceil( smax / 16.0f );
			const float bt0 = std::floor( tmin / 16.0f ), bt1 = std::ceil( tmax / 16.0f );
			if ( bs1 - bs0 < 1024.0f && bt1 - bt0 < 1024.0f )
			{
				lw = static_cast< int >( bs1 - bs0 ) + 1;
				lh = static_cast< int >( bt1 - bt0 ) + 1;
				const long long need = static_cast< long long >( lw ) * lh * 3;
				if ( lw > 0 && lh > 0 && lightOfs <= pb.lightLen && need <= static_cast< long long >( pb.lightLen - lightOfs ) )
				{
					lm = pb.light + lightOfs;
					lsmin = bs0 * 16.0f;
					ltmin = bt0 * 16.0f;
				}
			}
		}

		// texture level: about one texel per pixel
		int mipLevel = 0;
		const bool haveTex = tex && tex->ok;
		if ( haveTex )
		{
			const float sl = std::sqrt( sv[0] * sv[0] + sv[1] * sv[1] + sv[2] * sv[2] );
			const float tl = std::sqrt( tv[0] * tv[0] + tv[1] * tv[1] + tv[2] * tv[2] );
			float tpp = OvMax( sl, tl ) * P.unit / nz;
			while ( mipLevel < 3 && tpp >= 2.0f )
			{
				tpp *= 0.5f;
				mipLevel++;
			}
		}
		else if ( tex && tex->name[0] && !pb.missingSeen[static_cast< size_t >( mipIdx )] )
		{
			pb.missingSeen[static_cast< size_t >( mipIdx )] = 1;
			pb.missing++;
		}

		const float dw = d + nx * ox + ny * oy + nz * oz;
		const float invNz = 1.0f / nz;

		int r0 = static_cast< int >( std::floor( pminy - 0.5f ) );
		int r1 = static_cast< int >( std::ceil( pmaxy - 0.5f ) );
		r0 = OvClamp( r0, 0, P.h - 1 );
		r1 = OvClamp( r1, 0, P.h - 1 );
		for ( int r = r0; r <= r1; r++ )
		{
			const float cy = static_cast< float >( r ) + 0.5f;
			// convex polygon: the span of this row of pixel centres
			float xl = 1e30f, xr = -1e30f;
			for ( int i = 0; i < n; i++ )
			{
				const int j = ( i + 1 == n ) ? 0 : i + 1;
				const float ya = sy[i], yb = sy[j];
				if ( ( ya <= cy && yb > cy ) || ( yb <= cy && ya > cy ) )
				{
					const float x = sx[i] + ( cy - ya ) / ( yb - ya ) * ( sx[j] - sx[i] );
					xl = OvMin( xl, x );
					xr = OvMax( xr, x );
				}
			}
			if ( xl > xr )
				continue;
			int c0 = static_cast< int >( std::ceil( xl - 0.5f ) );
			int c1 = static_cast< int >( std::floor( xr - 0.5f ) );
			c0 = OvMax( c0, 0 );
			c1 = OvMin( c1, P.w - 1 );
			const float wy = P.y0 - cy * P.unit;
			for ( int c = c0; c <= c1; c++ )
			{
				const float wx = P.x0 + ( static_cast< float >( c ) + 0.5f ) * P.unit;
				const float wz = ( dw - nx * wx - ny * wy ) * invNz;
				if ( !IsSane( wz ) )
					continue;
				const float mx = wx - ox, my = wy - oy, mz = wz - oz;
				const float s = mx * sv[0] + my * sv[1] + mz * sv[2] + sv[3];
				const float t = mx * tv[0] + my * tv[1] + mz * tv[2] + tv[3];
				float rgb[3] = { 128.0f, 128.0f, 128.0f };
				if ( haveTex && !SampleTex( *tex, mipLevel, s, t, rgb ) )
					continue; // see-through texel
				float l3[3] = { 255.0f, 255.0f, 255.0f };
				if ( lm )
				{
					float u = ( s - lsmin ) / 16.0f, v = ( t - ltmin ) / 16.0f;
					u = OvClamp( u, 0.0f, static_cast< float >( lw - 1 ) );
					v = OvClamp( v, 0.0f, static_cast< float >( lh - 1 ) );
					const int u0 = static_cast< int >( u ), v0 = static_cast< int >( v );
					const int u1 = OvMin( u0 + 1, lw - 1 ), v1 = OvMin( v0 + 1, lh - 1 );
					const float au = u - static_cast< float >( u0 ), av = v - static_cast< float >( v0 );
					const size_t LW = static_cast< size_t >( lw );
					const unsigned char* q00 = lm + ( static_cast< size_t >( v0 ) * LW + static_cast< size_t >( u0 ) ) * 3;
					const unsigned char* q10 = lm + ( static_cast< size_t >( v0 ) * LW + static_cast< size_t >( u1 ) ) * 3;
					const unsigned char* q01 = lm + ( static_cast< size_t >( v1 ) * LW + static_cast< size_t >( u0 ) ) * 3;
					const unsigned char* q11 = lm + ( static_cast< size_t >( v1 ) * LW + static_cast< size_t >( u1 ) ) * 3;
					for ( int k = 0; k < 3; k++ )
					{
						const float top = q00[k] + ( q10[k] - q00[k] ) * au;
						const float bot = q01[k] + ( q11[k] - q01[k] ) * au;
						const float raw = OvClamp( top + ( bot - top ) * av, 0.0f, 255.0f );
						const int li = static_cast< int >( raw );
						l3[k] = g_LightCurve[li] + ( g_LightCurve[li + 1] - g_LightCurve[li] ) * ( raw - static_cast< float >( li ) );
					}
				}
				const float gain = kPicGain / 255.0f;
				const uint16_t col = Pack565( rgb[0] * l3[0] * gain, rgb[1] * l3[1] * gain, rgb[2] * l3[2] * gain );
				PicInsert( P, static_cast< size_t >( r ) * static_cast< size_t >( P.w ) + static_cast< size_t >( c ), wz, col );
			}
		}
	}
}

// ---- reachability: which surfaces a player can get to from the spawns ------
const int LUMP_CLIPNODES = 9;

// the map's own collision for a crouching player (clip hull 3: walls, player-clip brushes)
struct HullTest
{
	const unsigned char* clip = nullptr;
	int numClip = 0;
	const unsigned char* planes = nullptr;
	int numPlanes = 0;
	int head = -1;

	bool Init( const BspView& b, int hull )
	{
		const int o = b.ofs[LUMP_CLIPNODES], s = b.size[LUMP_CLIPNODES];
		if ( o < 0 || s < 8 || o > b.len || s > b.len - o || hull < 1 || hull > 3 )
			return false;
		clip = b.base + o;
		numClip = s / 8;
		planes = b.planes;
		numPlanes = b.numPlanes;
		head = RdI32( b.models + 36 + 4 * hull );
		return head >= 0 && head < numClip;
	}
	bool Solid( float x, float y, float z ) const
	{
		int num = head;
		for ( int guard = 0; num >= 0 && guard < 1024; guard++ )
		{
			if ( num >= numClip )
				return true;
			const unsigned char* c = clip + static_cast< size_t >( num ) * 8;
			const int pn = RdI32( c );
			if ( pn < 0 || pn >= numPlanes )
				return true;
			const unsigned char* pl = planes + static_cast< size_t >( pn ) * kPlaneSize;
			const float d = RdF32( pl ) * x + RdF32( pl + 4 ) * y + RdF32( pl + 8 ) * z - RdF32( pl + 12 );
			num = d < 0.0f ? RdI16( c + 6 ) : RdI16( c + 4 );
		}
		return num >= 0 || num == -2; // CONTENTS_SOLID
	}
};

struct LadderBox
{
	float zmin, zmax;
};

void ComputeReach( OverviewPicture& P, const BspView& b, const std::vector< EntKV >& ents )
{
	const int D = P.depth;
	const size_t Ds = static_cast< size_t >( D );
	const size_t numPix = static_cast< size_t >( P.w ) * static_cast< size_t >( P.h );
	P.reach.assign( numPix, 0 );
	P.reachValid = false;

	std::vector< LadderBox > ladders;
	std::vector< int > ladderAt( numPix, -1 );
	std::vector< int32_t > queue;
	queue.reserve( numPix );

	auto entryCount = [&]( size_t pix ) -> int
	{
		const int16_t* zs = &P.z[pix * Ds];
		int n = 0;
		while ( n < D && zs[n] != OVERVIEW_PIC_NONE )
			n++;
		return n;
	};
	auto push = [&]( size_t pix, int k )
	{
		const unsigned char bit = static_cast< unsigned char >( 1u << k );
		if ( P.reach[pix] & bit )
			return;
		P.reach[pix] |= bit;
		queue.push_back( static_cast< int32_t >( pix * Ds + static_cast< size_t >( k ) ) );
	};
	auto headroom = [&]( size_t pix, int k ) -> bool
	{
		const int16_t* zs = &P.z[pix * Ds];
		return k == 0 || static_cast< float >( zs[k - 1] - zs[k] ) >= kHeadroom;
	};
	// a crouching player fits there (map collision), cached per surface
	HullTest hull;
	const bool haveHull = hull.Init( b, 3 );
	std::vector< unsigned char > tested( haveHull ? numPix : 0, 0 ), fits( haveHull ? numPix : 0, 0 );
	auto standable = [&]( size_t pix, int k ) -> bool
	{
		if ( !headroom( pix, k ) )
			return false;
		if ( !haveHull )
			return true;
		const unsigned char bit = static_cast< unsigned char >( 1u << k );
		if ( !( tested[pix] & bit ) )
		{
			tested[pix] |= bit;
			const float x = P.x0 + ( static_cast< float >( pix % static_cast< size_t >( P.w ) ) + 0.5f ) * P.unit;
			const float y = P.y0 - ( static_cast< float >( pix / static_cast< size_t >( P.w ) ) + 0.5f ) * P.unit;
			const float z = static_cast< float >( P.z[pix * Ds + static_cast< size_t >( k )] );
			if ( !hull.Solid( x, y, z + 19.0f ) || !hull.Solid( x, y, z + 34.0f ) )
				fits[pix] |= bit;
		}
		return ( fits[pix] & bit ) != 0;
	};

	for ( const EntKV& e : ents )
	{
		if ( !IEquals( e.classname.c_str(), "func_ladder" ) )
			continue;
		const int mi = EntModelIndex( b, e );
		if ( mi < 0 )
			continue;
		const unsigned char* m = b.models + static_cast< size_t >( mi ) * kModelSize;
		float ox, oy, oz;
		ParseOrigin( e.origin, ox, oy, oz );
		const float x0 = RdF32( m ) + ox, y0 = RdF32( m + 4 ) + oy, z0 = RdF32( m + 8 ) + oz;
		const float x1 = RdF32( m + 12 ) + ox, y1 = RdF32( m + 16 ) + oy, z1 = RdF32( m + 20 ) + oz;
		if ( !IsSane( x0 ) || !IsSane( y0 ) || !IsSane( z0 ) || !IsSane( x1 ) || !IsSane( y1 ) || !IsSane( z1 ) )
			continue;
		const float pad = 24.0f;
		const int c0 = OvClamp( static_cast< int >( std::floor( ( x0 - pad - P.x0 ) / P.unit ) ), 0, P.w - 1 );
		const int c1 = OvClamp( static_cast< int >( std::floor( ( x1 + pad - P.x0 ) / P.unit ) ), 0, P.w - 1 );
		const int r0 = OvClamp( static_cast< int >( std::floor( ( P.y0 - ( y1 + pad ) ) / P.unit ) ), 0, P.h - 1 );
		const int r1 = OvClamp( static_cast< int >( std::floor( ( P.y0 - ( y0 - pad ) ) / P.unit ) ), 0, P.h - 1 );
		LadderBox lb;
		lb.zmin = z0;
		lb.zmax = z1;
		const int idx = static_cast< int >( ladders.size() );
		ladders.push_back( lb );
		for ( int r = r0; r <= r1; r++ )
			for ( int c = c0; c <= c1; c++ )
				ladderAt[static_cast< size_t >( r ) * static_cast< size_t >( P.w ) + static_cast< size_t >( c )] = idx;
	}

	// seeds: spawn points, hostages, VIP start
	int seeds = 0;
	for ( const EntKV& e : ents )
	{
		const char* cn = e.classname.c_str();
		if ( !IEquals( cn, "info_player_start" ) && !IEquals( cn, "info_player_deathmatch" ) && !IEquals( cn, "info_vip_start" ) &&
			!IEquals( cn, "hostage_entity" ) && !IEquals( cn, "monster_scientist" ) )
			continue;
		float x, y, z;
		ParseOrigin( e.origin, x, y, z );
		const int c = static_cast< int >( std::floor( ( x - P.x0 ) / P.unit ) );
		const int r = static_cast< int >( std::floor( ( P.y0 - y ) / P.unit ) );
		for ( int dr = -1; dr <= 1; dr++ )
		{
			for ( int dc = -1; dc <= 1; dc++ )
			{
				const int cc = c + dc, rr = r + dr;
				if ( cc < 0 || rr < 0 || cc >= P.w || rr >= P.h )
					continue;
				const size_t pix = static_cast< size_t >( rr ) * static_cast< size_t >( P.w ) + static_cast< size_t >( cc );
				const int16_t* zs = &P.z[pix * Ds];
				const int n = entryCount( pix );
				for ( int k = 0; k < n; k++ )
				{
					if ( static_cast< float >( zs[k] ) <= z + 8.0f )
					{
						push( pix, k );
						seeds++;
						break;
					}
				}
			}
		}
	}
	if ( !seeds )
		return;

	for ( size_t qi = 0; qi < queue.size(); qi++ )
	{
		const size_t node = static_cast< size_t >( queue[qi] );
		const size_t pix = node / Ds;
		const int k = static_cast< int >( node % Ds );
		const float z = static_cast< float >( P.z[node] );
		const int c = static_cast< int >( pix % static_cast< size_t >( P.w ) );
		const int r = static_cast< int >( pix / static_cast< size_t >( P.w ) );

		float climb = -1e30f, climbLow = 1e30f;
		const int li = ladderAt[pix];
		if ( li >= 0 )
		{
			const LadderBox& lb = ladders[static_cast< size_t >( li )];
			if ( z >= lb.zmin - kJumpUp && z <= lb.zmax + kJumpUp )
			{
				climb = lb.zmax + kJumpUp;
				climbLow = lb.zmin - kJumpUp;
				const int16_t* zs = &P.z[pix * Ds];
				const int n = entryCount( pix );
				for ( int j = 0; j < n; j++ )
				{
					const float zj = static_cast< float >( zs[j] );
					if ( j != k && zj <= climb && zj >= climbLow && headroom( pix, j ) )
						push( pix, j );
				}
			}
		}

		const int nc[4] = { c - 1, c + 1, c, c };
		const int nr[4] = { r, r, r - 1, r + 1 };
		for ( int a = 0; a < 4; a++ )
		{
			if ( nc[a] < 0 || nr[a] < 0 || nc[a] >= P.w || nr[a] >= P.h )
				continue;
			const size_t q = static_cast< size_t >( nr[a] ) * static_cast< size_t >( P.w ) + static_cast< size_t >( nc[a] );
			const int16_t* zq = &P.z[q * Ds];
			const int n = entryCount( q );
			int walk = -1, jump = -1;
			for ( int j = 0; j < n; j++ )
			{
				const float zj = static_cast< float >( zq[j] );
				if ( jump < 0 && zj <= z + kJumpUp )
					jump = j;
				if ( walk < 0 && zj <= z + kStepUp )
					walk = j;
				if ( climb > -1e29f && zj <= climb && zj >= climbLow && headroom( q, j ) )
					push( q, j );
			}
			if ( walk >= 0 && standable( q, walk ) )
				push( q, walk );
			if ( jump >= 0 && jump != walk && standable( q, jump ) )
				push( q, jump );
		}
	}

	// the player's centre stays 16 units off the walls: walk the floor out to the walls for the picture
	if ( haveHull )
	{
		const int steps = static_cast< int >( std::ceil( 20.0f / P.unit ) );
		std::vector< int32_t > front( queue.begin(), queue.end() ), next;
		for ( int st = 0; st < steps && !front.empty(); st++ )
		{
			next.clear();
			for ( const int32_t nodeId : front )
			{
				const size_t node = static_cast< size_t >( nodeId );
				const size_t pix = node / Ds;
				const float z = static_cast< float >( P.z[node] );
				const int c = static_cast< int >( pix % static_cast< size_t >( P.w ) );
				const int r = static_cast< int >( pix / static_cast< size_t >( P.w ) );
				const int nc[4] = { c - 1, c + 1, c, c };
				const int nr[4] = { r, r, r - 1, r + 1 };
				for ( int a = 0; a < 4; a++ )
				{
					if ( nc[a] < 0 || nr[a] < 0 || nc[a] >= P.w || nr[a] >= P.h )
						continue;
					const size_t q = static_cast< size_t >( nr[a] ) * static_cast< size_t >( P.w ) + static_cast< size_t >( nc[a] );
					const int16_t* zq = &P.z[q * Ds];
					const int n = entryCount( q );
					for ( int j = 0; j < n; j++ )
					{
						const float zj = static_cast< float >( zq[j] );
						if ( zj > z + kStepUp )
							continue;
						if ( zj >= z - kStepUp )
						{
							const unsigned char bit = static_cast< unsigned char >( 1u << j );
							if ( !( P.reach[q] & bit ) )
							{
								P.reach[q] |= bit;
								next.push_back( static_cast< int32_t >( q * Ds + static_cast< size_t >( j ) ) );
							}
						}
						break;
					}
				}
			}
			front.swap( next );
		}
	}

	size_t withAny = 0, reached = 0;
	for ( size_t i = 0; i < numPix; i++ )
	{
		if ( P.z[i * Ds] != OVERVIEW_PIC_NONE )
			withAny++;
		if ( P.reach[i] )
			reached++;
	}
	P.reachValid = withAny > 0 && reached * 20 >= withAny; // at least 5% reached, else trust nothing
}

bool BuildPictureImpl( const unsigned char* bsp, int len, const OverviewLayers& L, int maxPixels,
	OverviewLoadFile loader, void* ctx, OverviewPicture& out, char* err, int errLen )
{
	BspView b;
	if ( !OpenBsp( bsp, len, b, err, errLen ) )
		return false;
	if ( !LayersValid( L ) )
	{
		SetErr( err, errLen, "picture: no layout" );
		return false;
	}
	BuildTexGamma();

	std::vector< EntKV > ents;
	ParseAllEntities( b, ents );

	std::vector< PicTexture > tex;
	int wadsLoaded = 0, wadsMissing = 0;
	LoadTextures( b, ents, loader, ctx, tex, wadsLoaded, wadsMissing );

	maxPixels = OvClamp( maxPixels, 64, 2048 );
	const float extX = static_cast< float >( L.w ) * L.cell, extY = static_cast< float >( L.h ) * L.cell;
	float unit = OvMax( extX, extY ) / static_cast< float >( maxPixels );
	if ( unit < 2.0f )
		unit = 2.0f;

	OverviewPicture P;
	P.unit = unit;
	P.x0 = L.x0;
	P.y0 = L.y0;
	P.w = OvClamp( static_cast< int >( std::ceil( extX / unit - 0.001f ) ), 1, maxPixels );
	P.h = OvClamp( static_cast< int >( std::ceil( extY / unit - 0.001f ) ), 1, maxPixels );
	P.depth = kPicDepth;
	const size_t numPix = static_cast< size_t >( P.w ) * static_cast< size_t >( P.h );
	P.z.assign( numPix * static_cast< size_t >( P.depth ), static_cast< int16_t >( OVERVIEW_PIC_NONE ) );
	P.rgb.assign( numPix * static_cast< size_t >( P.depth ), 0 );

	PicBuild pb;
	pb.P = &P;
	pb.b = &b;
	pb.tex = &tex;
	pb.missingSeen.assign( tex.size(), 0 );
	{
		const int lo = b.ofs[LUMP_LIGHTING], ls = b.size[LUMP_LIGHTING];
		if ( lo >= 0 && ls > 0 && lo <= len && ls <= len - lo )
		{
			pb.light = bsp + lo;
			pb.lightLen = ls;
		}
	}

	PicDrawModel( pb, 0, 0.0f, 0.0f, 0.0f );
	for ( const EntKV& e : ents )
	{
		if ( !IsStaticBrushClass( e.classname ) )
			continue;
		const int mi = EntModelIndex( b, e );
		if ( mi < 0 )
			continue;
		const int rm = e.rendermode.empty() ? 0 : atoi( e.rendermode.c_str() );
		const bool invisible = rm != 0 && !e.renderamt.empty() && atoi( e.renderamt.c_str() ) == 0;
		if ( invisible || rm == 5 ) // additive glows are not surfaces
			continue;
		float ox, oy, oz;
		ParseOrigin( e.origin, ox, oy, oz );
		PicDrawModel( pb, mi, ox, oy, oz );
	}

	ComputeReach( P, b, ents );
	P.missingTextures = pb.missing;
	P.wadsLoaded = wadsLoaded;
	P.wadsMissing = wadsMissing;
	out = std::move( P );
	return true;
}

// ---- picture output ---------------------------------------------------------
// surface shown at a pixel (slot or -1) and its brightness
int PickSurface( const OverviewPicture& P, size_t pix, bool useViewer, float limit, float& scale )
{
	const int D = P.depth;
	const int16_t* zs = &P.z[pix * static_cast< size_t >( D )];
	int n = 0;
	while ( n < D && zs[n] != OVERVIEW_PIC_NONE )
		n++;
	scale = 1.0f;
	if ( !n )
		return -1;
	const unsigned int mask = P.reachValid ? P.reach[pix] : ( ( 1u << n ) - 1u );
	int count = 0, lowest = -1, pick = -1;
	for ( int k = 0; k < n; k++ )
	{
		if ( !( mask & ( 1u << k ) ) )
			continue;
		count++;
		lowest = k;
		if ( pick < 0 && ( !useViewer || static_cast< float >( zs[k] ) <= limit ) )
			pick = k;
	}
	if ( !count )
	{
		scale = kUnreachScale;
		return 0;
	}
	if ( pick < 0 )
	{
		pick = lowest;
		if ( count > 1 )
			scale = kUpperDimScale;
	}
	return pick;
}

void RenderPictureImpl( const OverviewPicture& P, bool useViewer, float viewerZ, unsigned char* rgba )
{
	const size_t numPix = static_cast< size_t >( P.w ) * static_cast< size_t >( P.h );
	const float limit = viewerZ + kViewerSlack;
	for ( size_t i = 0; i < numPix; i++ )
	{
		unsigned char* px = rgba + i * 4;
		float scale;
		const int k = PickSurface( P, i, useViewer, limit, scale );
		if ( k < 0 )
		{
			px[0] = px[1] = px[2] = px[3] = 0;
			continue;
		}
		float rgb[3];
		Unpack565( P.rgb[i * static_cast< size_t >( P.depth ) + static_cast< size_t >( k )], rgb );
		px[0] = ToByte( rgb[0] * scale );
		px[1] = ToByte( rgb[1] * scale );
		px[2] = ToByte( rgb[2] * scale );
		px[3] = scale < 0.5f ? 170 : 255;
	}
}

// median cut over a 5-bit-per-channel histogram; palette 1..255, 0 = green (empty)
struct QBox
{
	int lo[3], hi[3];
	uint64_t count;
};

void QuantizeToPalette( const std::vector< unsigned char >& rgb, const std::vector< unsigned char >& used, std::vector< unsigned char >& index, unsigned char pal[256][3] )
{
	const size_t num = used.size();
	std::vector< uint32_t > hist( 32768, 0 );
	std::vector< double > sum( 32768 * 3, 0.0 );
	auto binOf = [&]( size_t i ) -> size_t
	{
		return static_cast< size_t >( ( ( rgb[i * 3] >> 3 ) << 10 ) | ( ( rgb[i * 3 + 1] >> 3 ) << 5 ) | ( rgb[i * 3 + 2] >> 3 ) );
	};
	for ( size_t i = 0; i < num; i++ )
	{
		if ( !used[i] )
			continue;
		const size_t bin = binOf( i );
		hist[bin]++;
		for ( size_t k = 0; k < 3; k++ )
			sum[bin * 3 + k] += rgb[i * 3 + k];
	}
	auto shrink = [&]( QBox& bx )
	{
		int lo[3] = { 31, 31, 31 }, hi[3] = { 0, 0, 0 };
		uint64_t cnt = 0;
		for ( int r = bx.lo[0]; r <= bx.hi[0]; r++ )
			for ( int g = bx.lo[1]; g <= bx.hi[1]; g++ )
				for ( int bb = bx.lo[2]; bb <= bx.hi[2]; bb++ )
				{
					const uint32_t h = hist[static_cast< size_t >( ( r << 10 ) | ( g << 5 ) | bb )];
					if ( !h )
						continue;
					cnt += h;
					lo[0] = OvMin( lo[0], r );
					hi[0] = OvMax( hi[0], r );
					lo[1] = OvMin( lo[1], g );
					hi[1] = OvMax( hi[1], g );
					lo[2] = OvMin( lo[2], bb );
					hi[2] = OvMax( hi[2], bb );
				}
		bx.count = cnt;
		if ( cnt )
		{
			for ( int k = 0; k < 3; k++ )
			{
				bx.lo[k] = lo[k];
				bx.hi[k] = hi[k];
			}
		}
	};
	std::vector< QBox > boxes;
	QBox all = { { 0, 0, 0 }, { 31, 31, 31 }, 0 };
	shrink( all );
	if ( all.count )
		boxes.push_back( all );
	while ( boxes.size() < 255 )
	{
		int best = -1;
		double bestScore = 0.0;
		for ( size_t i = 0; i < boxes.size(); i++ )
		{
			const QBox& bx = boxes[i];
			const int span = OvMax( bx.hi[0] - bx.lo[0], OvMax( bx.hi[1] - bx.lo[1], bx.hi[2] - bx.lo[2] ) );
			if ( span <= 0 )
				continue;
			const double score = static_cast< double >( bx.count ) * static_cast< double >( span );
			if ( score > bestScore )
			{
				bestScore = score;
				best = static_cast< int >( i );
			}
		}
		if ( best < 0 )
			break;
		const QBox bx = boxes[static_cast< size_t >( best )];
		int axis = 0;
		for ( int k = 1; k < 3; k++ )
		{
			if ( bx.hi[k] - bx.lo[k] > bx.hi[axis] - bx.lo[axis] )
				axis = k;
		}
		uint64_t line[32] = {};
		for ( int r = bx.lo[0]; r <= bx.hi[0]; r++ )
			for ( int g = bx.lo[1]; g <= bx.hi[1]; g++ )
				for ( int bb = bx.lo[2]; bb <= bx.hi[2]; bb++ )
				{
					const int v[3] = { r, g, bb };
					line[v[axis]] += hist[static_cast< size_t >( ( r << 10 ) | ( g << 5 ) | bb )];
				}
		uint64_t acc = 0;
		int cut = bx.lo[axis];
		for ( int v = bx.lo[axis]; v < bx.hi[axis]; v++ )
		{
			acc += line[v];
			cut = v;
			if ( acc * 2 >= bx.count )
				break;
		}
		QBox a = bx, c = bx;
		a.hi[axis] = cut;
		c.lo[axis] = cut + 1;
		shrink( a );
		shrink( c );
		boxes[static_cast< size_t >( best )] = a;
		if ( c.count )
			boxes.push_back( c );
	}

	std::vector< unsigned char > binIndex( 32768, 1 );
	for ( int i = 0; i < 256; i++ )
		pal[i][0] = pal[i][1] = pal[i][2] = 0;
	pal[0][1] = 255;
	for ( size_t bi = 0; bi < boxes.size(); bi++ )
	{
		const QBox& bx = boxes[bi];
		double s[3] = { 0, 0, 0 };
		uint64_t cnt = 0;
		for ( int r = bx.lo[0]; r <= bx.hi[0]; r++ )
			for ( int g = bx.lo[1]; g <= bx.hi[1]; g++ )
				for ( int bb = bx.lo[2]; bb <= bx.hi[2]; bb++ )
				{
					const size_t bin = static_cast< size_t >( ( r << 10 ) | ( g << 5 ) | bb );
					if ( !hist[bin] )
						continue;
					binIndex[bin] = static_cast< unsigned char >( bi + 1 );
					cnt += hist[bin];
					for ( size_t k = 0; k < 3; k++ )
						s[k] += sum[bin * 3 + k];
				}
		for ( int k = 0; k < 3; k++ )
			pal[bi + 1][k] = cnt ? ToByte( static_cast< float >( s[k] / static_cast< double >( cnt ) ) ) : 0;
		if ( pal[bi + 1][0] == 0 && pal[bi + 1][1] == 255 && pal[bi + 1][2] == 0 )
			pal[bi + 1][1] = 254;
	}
	index.assign( num, 0 );
	for ( size_t i = 0; i < num; i++ )
	{
		if ( used[i] )
			index[i] = binIndex[binOf( i )];
	}
}

bool MakeClassicPictureImpl( const OverviewPicture& P, std::vector< unsigned char >& bmpFile, float& zoom, float origin[3], int& rotated )
{
	if ( P.w <= 0 || P.h <= 0 || P.depth <= 0 )
		return false;
	const size_t numPix = static_cast< size_t >( P.w ) * static_cast< size_t >( P.h );
	if ( P.z.size() < numPix * static_cast< size_t >( P.depth ) || P.rgb.size() < P.z.size() )
		return false;

	// extents and lowest height of everything walkable
	int cmin = P.w, cmax = -1, rmin = P.h, rmax = -1;
	float zmin = 1e30f;
	for ( int r = 0; r < P.h; r++ )
	{
		for ( int c = 0; c < P.w; c++ )
		{
			const size_t i = static_cast< size_t >( r ) * static_cast< size_t >( P.w ) + static_cast< size_t >( c );
			float scale;
			const int k = PickSurface( P, i, false, 0.0f, scale );
			if ( k < 0 || scale < 0.5f )
				continue;
			cmin = OvMin( cmin, c );
			cmax = OvMax( cmax, c );
			rmin = OvMin( rmin, r );
			rmax = OvMax( rmax, r );
			zmin = OvMin( zmin, static_cast< float >( P.z[i * static_cast< size_t >( P.depth ) + static_cast< size_t >( k )] ) );
		}
	}
	if ( cmax < 0 )
		return false;
	const float wx0 = P.x0 + static_cast< float >( cmin ) * P.unit;
	const float wx1 = P.x0 + static_cast< float >( cmax + 1 ) * P.unit;
	const float wy1 = P.y0 - static_cast< float >( rmin ) * P.unit;
	const float wy0 = P.y0 - static_cast< float >( rmax + 1 ) * P.unit;
	const float extX = OvMax( wx1 - wx0, 64.0f );
	const float extY = OvMax( wy1 - wy0, 64.0f );
	const float margin = 1.08f;
	rotated = ( extX >= extY ) ? 1 : 0;
	const float extU = rotated ? extX : extY;
	const float extV = rotated ? extY : extX;
	float zz = OvMin( 8192.0f / ( extU * margin ), ( 8192.0f / kAspect ) / ( extV * margin ) );
	zz = std::floor( zz * 100.0f ) / 100.0f;
	zoom = OvClamp( zz, 0.01f, 99.99f );
	origin[0] = std::floor( ( wx0 + wx1 ) * 0.5f + 0.5f );
	origin[1] = std::floor( ( wy0 + wy1 ) * 0.5f + 0.5f );
	origin[2] = std::floor( zmin + 0.5f );

	const size_t numOut = static_cast< size_t >( kClassicW ) * static_cast< size_t >( kClassicH );
	std::vector< unsigned char > rgb( numOut * 3, 0 ), used( numOut, 0 );
	const float halfU = 4096.0f / zoom;
	const float halfV = 4096.0f / ( zoom * kAspect );
	for ( int py = 0; py < kClassicH; py++ )
	{
		const float v = ( static_cast< float >( py ) + 0.5f ) / static_cast< float >( kClassicH );
		for ( int px = 0; px < kClassicW; px++ )
		{
			const float u = ( static_cast< float >( px ) + 0.5f ) / static_cast< float >( kClassicW );
			float wx, wy;
			if ( rotated )
			{
				wx = origin[0] - halfU + u * 2.0f * halfU;
				wy = origin[1] + halfV - v * 2.0f * halfV;
			}
			else
			{
				wy = origin[1] + halfU - u * 2.0f * halfU;
				wx = origin[0] + halfV - v * 2.0f * halfV;
			}
			const float fc = ( wx - P.x0 ) / P.unit, fr = ( P.y0 - wy ) / P.unit;
			if ( !( fc >= 0.0f ) || !( fr >= 0.0f ) )
				continue;
			const int c = static_cast< int >( fc ), r = static_cast< int >( fr );
			if ( c >= P.w || r >= P.h )
				continue;
			const size_t i = static_cast< size_t >( r ) * static_cast< size_t >( P.w ) + static_cast< size_t >( c );
			float scale;
			const int k = PickSurface( P, i, false, 0.0f, scale );
			if ( k < 0 || scale < 0.5f )
				continue; // nothing walkable: green, like the official overviews
			float col[3];
			Unpack565( P.rgb[i * static_cast< size_t >( P.depth ) + static_cast< size_t >( k )], col );
			const size_t o = static_cast< size_t >( py ) * static_cast< size_t >( kClassicW ) + static_cast< size_t >( px );
			rgb[o * 3] = ToByte( col[0] );
			rgb[o * 3 + 1] = ToByte( col[1] );
			rgb[o * 3 + 2] = ToByte( col[2] );
			used[o] = 1;
		}
	}
	std::vector< unsigned char > index;
	unsigned char pal[256][3];
	QuantizeToPalette( rgb, used, index, pal );
	WriteBmp8( index, pal, bmpFile );
	return true;
}

} // namespace

// ---------------------------------------------------------------------------
// public API
// ---------------------------------------------------------------------------
bool Overview_BuildLayers( const unsigned char* bsp, int len, int maxCells, int maxLayers, OverviewLayers& out, char* err, int errLen )
{
	SetErr( err, errLen, "" );
	try
	{
		OverviewLayers tmp;
		if ( !BuildLayersImpl( bsp, len, maxCells, maxLayers, tmp, err, errLen ) )
			return false;
		out = std::move( tmp );
		return true;
	}
	catch ( ... )
	{
		SetErr( err, errLen, "overview: out of memory" );
		return false;
	}
}

void Overview_Render( const OverviewLayers& L, bool useViewer, float viewerZ, unsigned char* rgba )
{
	if ( !rgba || !LayersValid( L ) )
		return;
	memset( rgba, 0, static_cast< size_t >( L.w ) * static_cast< size_t >( L.h ) * 4 );
	try
	{
		RenderImpl( L, useViewer, viewerZ, rgba );
	}
	catch ( ... )
	{
	}
}

bool Overview_MakeClassic( const OverviewLayers& L, std::vector< unsigned char >& bmpFile, float& zoom, float origin[3], int& rotated )
{
	if ( !origin )
		return false;
	try
	{
		return MakeClassicImpl( L, bmpFile, zoom, origin, rotated );
	}
	catch ( ... )
	{
		bmpFile.clear();
		return false;
	}
}

void Overview_RenderClassic( const OverviewLayers& L, float zoom, const float origin[3], int rotated, std::vector< unsigned char >& index1024x768, unsigned char palette[256][3] )
{
	if ( !palette )
		return;
	try
	{
		RenderClassicImpl( L, zoom, origin, rotated, index1024x768, palette );
	}
	catch ( ... )
	{
		index1024x768.clear();
	}
}

bool Overview_BuildPicture( const unsigned char* bsp, int len, const OverviewLayers& L, int maxPixels,
	OverviewLoadFile loader, void* ctx, OverviewPicture& out, char* err, int errLen )
{
	SetErr( err, errLen, "" );
	try
	{
		OverviewPicture tmp;
		if ( !BuildPictureImpl( bsp, len, L, maxPixels, loader, ctx, tmp, err, errLen ) )
			return false;
		out = std::move( tmp );
		return true;
	}
	catch ( ... )
	{
		SetErr( err, errLen, "picture: out of memory" );
		return false;
	}
}

void Overview_RenderPicture( const OverviewPicture& P, bool useViewer, float viewerZ, unsigned char* rgba )
{
	if ( !rgba || P.w <= 0 || P.h <= 0 || P.depth <= 0 )
		return;
	const size_t numPix = static_cast< size_t >( P.w ) * static_cast< size_t >( P.h );
	if ( P.z.size() < numPix * static_cast< size_t >( P.depth ) || P.rgb.size() < P.z.size() || P.reach.size() < numPix )
	{
		memset( rgba, 0, numPix * 4 );
		return;
	}
	RenderPictureImpl( P, useViewer, viewerZ, rgba );
}

bool Overview_MakeClassicPicture( const OverviewPicture& P, std::vector< unsigned char >& bmpFile, float& zoom, float origin[3], int& rotated )
{
	if ( !origin || P.reach.size() < static_cast< size_t >( P.w ) * static_cast< size_t >( P.h ) )
		return false;
	try
	{
		return MakeClassicPictureImpl( P, bmpFile, zoom, origin, rotated );
	}
	catch ( ... )
	{
		bmpFile.clear();
		return false;
	}
}
