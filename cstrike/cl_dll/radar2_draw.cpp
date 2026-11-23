// radar2_draw.cpp - see radar2_draw.h.
//
// Radar2_DrawSquare is the square radar's drawing as it was in radar2.cpp, with what it read from the game (HUD scale,
// cvars, the map, the icons, the screen) passed in, so the Advanced window's preview draws the very same radar.
// GL state: the attributes it changes are pushed and popped, the matrices copied and put back (not pushed: the stacks
// can be only two deep), the shader program and texture unit restored.
#include "radar2_draw.h"

#include <math.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <gl/GL.h>
#else
#include <GL/gl.h>
#endif

#ifndef GL_CURRENT_PROGRAM
#define GL_CURRENT_PROGRAM 0x8B8D
#endif
#ifndef GL_ACTIVE_TEXTURE
#define GL_ACTIVE_TEXTURE 0x84E0
#define GL_TEXTURE0 0x84C0
#endif

namespace
{

const float kPi = 3.14159265f;

typedef void( APIENTRY* UseProgramFn )( GLuint program );
typedef void( APIENTRY* ActiveTextureFn )( GLenum texture );

// some drivers return 1, 2, 3 or -1 instead of NULL for names they do not have
PROC GetGLFunction( const char* name )
{
	PROC p = wglGetProcAddress( name );
	const INT_PTR v = (INT_PTR)p;
	return ( v == 0 || v == 1 || v == 2 || v == 3 || v == -1 ) ? NULL : p;
}

// 2D drawing in screen pixels (origin top left) over whatever was set up, and back
struct Saved2D
{
	GLfloat projection[16];
	GLfloat modelview[16];
	GLint program;
	GLint activeUnit;
	GLint viewport[4];
};

void Begin2D( int W, int H, Saved2D& saved )
{
	static UseProgramFn useProgram = (UseProgramFn)GetGLFunction( "glUseProgram" );
	static ActiveTextureFn activeTexture = (ActiveTextureFn)GetGLFunction( "glActiveTexture" );
	saved.program = 0;
	saved.activeUnit = GL_TEXTURE0;
	glGetIntegerv( GL_CURRENT_PROGRAM, &saved.program );
	glGetIntegerv( GL_ACTIVE_TEXTURE, &saved.activeUnit );
	for ( int i = 0; i < 32 && glGetError() != GL_NO_ERROR; i++ )
		;	// the queries above are errors on old drivers that lack those things
	glGetFloatv( GL_PROJECTION_MATRIX, saved.projection );
	glGetFloatv( GL_MODELVIEW_MATRIX, saved.modelview );
	saved.viewport[0] = saved.viewport[1] = 0;
	saved.viewport[2] = W;
	saved.viewport[3] = H;
	glGetIntegerv( GL_VIEWPORT, saved.viewport );
	glPushAttrib( GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_SCISSOR_BIT | GL_TEXTURE_BIT | GL_CURRENT_BIT | GL_TRANSFORM_BIT | GL_POLYGON_BIT );
	if ( saved.program && useProgram )
		useProgram( 0 );
	if ( saved.activeUnit != GL_TEXTURE0 && activeTexture )
		activeTexture( GL_TEXTURE0 );
	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();
	glOrtho( 0, W, H, 0, -1, 1 );
	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity();
	glDisable( GL_DEPTH_TEST );
	glDisable( GL_CULL_FACE );
	glDisable( GL_ALPHA_TEST );
	glDisable( GL_FOG );
	glDisable( GL_TEXTURE_2D );
	glDisable( GL_SCISSOR_TEST );
	glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
}

void End2D( const Saved2D& saved )
{
	static UseProgramFn useProgram = (UseProgramFn)GetGLFunction( "glUseProgram" );
	static ActiveTextureFn activeTexture = (ActiveTextureFn)GetGLFunction( "glActiveTexture" );
	glMatrixMode( GL_PROJECTION );
	glLoadMatrixf( saved.projection );
	glMatrixMode( GL_MODELVIEW );
	glLoadMatrixf( saved.modelview );
	glPopAttrib();
	if ( saved.activeUnit != GL_TEXTURE0 && activeTexture )
		activeTexture( (GLenum)saved.activeUnit );
	if ( saved.program && useProgram )
		useProgram( (GLuint)saved.program );
}

bool HasClip( const float clip[4] )
{
	return clip[2] > 0.0f && clip[3] > 0.0f;
}

// GL scissor for a screen rectangle (origin top left), cut down to the clip rectangle when there is one
void Scissor( float x, float y, float w, float h, const float clip[4], const Saved2D& saved, int W, int H )
{
	if ( HasClip( clip ) )
	{
		const float x1 = x + w < clip[0] + clip[2] ? x + w : clip[0] + clip[2];
		const float y1 = y + h < clip[1] + clip[3] ? y + h : clip[1] + clip[3];
		x = x > clip[0] ? x : clip[0];
		y = y > clip[1] ? y : clip[1];
		w = x1 - x > 0.0f ? x1 - x : 0.0f;
		h = y1 - y > 0.0f ? y1 - y : 0.0f;
	}
	const GLint* vp = saved.viewport;
	const float sx = (float)vp[2] / W, sy = (float)vp[3] / H;
	glScissor( vp[0] + (int)( x * sx ), vp[1] + (int)( ( H - y - h ) * sy ), (int)( w * sx + 0.5f ), (int)( h * sy + 0.5f ) );
}

// outside the radar square: only the clip rectangle limits the drawing
void ScissorToClip( const float clip[4], const Saved2D& saved, int W, int H )
{
	if ( HasClip( clip ) )
	{
		glEnable( GL_SCISSOR_TEST );
		Scissor( clip[0], clip[1], clip[2], clip[3], clip, saved, W, H );
	}
	else
		glDisable( GL_SCISSOR_TEST );
}

struct RadarView
{
	float cx, cy, half, scale;
	float sinYaw, cosYaw;
	bool rotate;
	float ox, oy;

	void ToScreen( float X, float Y, float* sx, float* sy ) const
	{
		const float dx = X - ox, dy = Y - oy;
		if ( rotate )
		{
			const float fwd = dx * cosYaw + dy * sinYaw, right = dx * sinYaw - dy * cosYaw;
			*sx = cx + right * scale;
			*sy = cy - fwd * scale;
		}
		else
		{
			*sx = cx + dx * scale;
			*sy = cy - dy * scale;
		}
	}

	// screen direction (unit) of a world yaw in degrees
	void Direction( float yawDeg, float* dx, float* dy ) const
	{
		const float a = yawDeg * kPi / 180.0f;
		const float wx = cosf( a ), wy = sinf( a );
		if ( rotate )
		{
			*dx = wx * sinYaw - wy * cosYaw;
			*dy = -( wx * cosYaw + wy * sinYaw );
		}
		else
		{
			*dx = wx;
			*dy = -wy;
		}
	}
};

void Color( int r, int g, int b, float a )
{
	glColor4f( r / 255.0f, g / 255.0f, b / 255.0f, a );
}

void FillRect( float x, float y, float w, float h )
{
	glBegin( GL_QUADS );
	glVertex2f( x, y );
	glVertex2f( x + w, y );
	glVertex2f( x + w, y + h );
	glVertex2f( x, y + h );
	glEnd();
}

void Disc( float x, float y, float r )
{
	glBegin( GL_TRIANGLE_FAN );
	glVertex2f( x, y );
	for ( int i = 0; i <= 20; i++ )
	{
		const float a = i * 2.0f * kPi / 20.0f;
		glVertex2f( x + cosf( a ) * r, y + sinf( a ) * r );
	}
	glEnd();
}

void Ring( float x, float y, float r, float width )
{
	glBegin( GL_TRIANGLE_STRIP );
	for ( int i = 0; i <= 24; i++ )
	{
		const float a = i * 2.0f * kPi / 24.0f;
		glVertex2f( x + cosf( a ) * r, y + sinf( a ) * r );
		glVertex2f( x + cosf( a ) * ( r + width ), y + sinf( a ) * ( r + width ) );
	}
	glEnd();
}

// arrow pointing along (dx, dy); size = half length
void Arrow( float x, float y, float dx, float dy, float size )
{
	const float rx = -dy, ry = dx; // right of the direction
	const float pts[4][2] = { { 0.0f, -1.25f }, { 0.85f, 0.95f }, { 0.0f, 0.45f }, { -0.85f, 0.95f } };
	float v[4][2];
	for ( int i = 0; i < 4; i++ )
	{
		const float lx = pts[i][0], ly = -pts[i][1];
		v[i][0] = x + ( lx * rx + ly * dx ) * size;
		v[i][1] = y + ( lx * ry + ly * dy ) * size;
	}
	glBegin( GL_TRIANGLES );
	glVertex2f( v[0][0], v[0][1] );
	glVertex2f( v[1][0], v[1][1] );
	glVertex2f( v[2][0], v[2][1] );
	glVertex2f( v[0][0], v[0][1] );
	glVertex2f( v[2][0], v[2][1] );
	glVertex2f( v[3][0], v[3][1] );
	glEnd();
}

void Triangle( float x, float y, float size, bool up )
{
	const float s = up ? -1.0f : 1.0f;
	glBegin( GL_TRIANGLES );
	glVertex2f( x, y + s * size );
	glVertex2f( x - size, y - s * size * 0.6f );
	glVertex2f( x + size, y - s * size * 0.6f );
	glEnd();
}

// a tinted HUD icon centred on (x, y), size = width; false when the icon is not loaded
bool DrawIcon( const Radar2Picture& icon, float x, float y, float size, int r, int g, int b, float a )
{
	if ( !icon.texture || icon.w <= 0 || icon.h <= 0 || icon.texW <= 0 || icon.texH <= 0 )
		return false;
	glEnable( GL_TEXTURE_2D );
	glBindTexture( GL_TEXTURE_2D, icon.texture );
	glBlendFunc( GL_ONE, GL_ONE_MINUS_SRC_ALPHA );
	glColor4f( r / 255.0f * a, g / 255.0f * a, b / 255.0f * a, a );
	const float u = (float)icon.w / icon.texW, v = (float)icon.h / icon.texH;
	const float hw = size * 0.5f, hh = size * 0.5f * icon.h / icon.w;
	glBegin( GL_QUADS );
	glTexCoord2f( 0, 0 );
	glVertex2f( x - hw, y - hh );
	glTexCoord2f( u, 0 );
	glVertex2f( x + hw, y - hh );
	glTexCoord2f( u, v );
	glVertex2f( x + hw, y + hh );
	glTexCoord2f( 0, v );
	glVertex2f( x - hw, y + hh );
	glEnd();
	glDisable( GL_TEXTURE_2D );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	return true;
}

void DrawC4( const Radar2Picture& c4, float x, float y, float size, int r, int g, int b, float a )
{
	if ( DrawIcon( c4, x, y, size, r, g, b, a ) )
		return;
	// fallback: a small block
	Color( r, g, b, a );
	FillRect( x - size * 0.4f, y - size * 0.3f, size * 0.8f, size * 0.6f );
}

// an X of two bars, half length 'size', half width 'width'
void Cross( float x, float y, float size, float width )
{
	glBegin( GL_QUADS );
	for ( int k = 0; k < 2; k++ )
	{
		const float ax = ( k ? -0.7071f : 0.7071f ), ay = 0.7071f; // along the bar
		const float nx = -ay, ny = ax;                            // across it
		glVertex2f( x - ax * size - nx * width, y - ay * size - ny * width );
		glVertex2f( x + ax * size - nx * width, y + ay * size - ny * width );
		glVertex2f( x + ax * size + nx * width, y + ay * size + ny * width );
		glVertex2f( x - ax * size + nx * width, y - ay * size + ny * width );
	}
	glEnd();
}

// keeps a point inside the radar; true if it had to be moved to the edge
bool ClampToRadar( const RadarView& v, float inset, float* sx, float* sy )
{
	const float dx = *sx - v.cx, dy = *sy - v.cy;
	const float lim = v.half - inset;
	const float m = fabsf( dx ) > fabsf( dy ) ? fabsf( dx ) : fabsf( dy );
	if ( m <= lim || m <= 0.0f )
		return false;
	*sx = v.cx + dx * lim / m;
	*sy = v.cy + dy * lim / m;
	return true;
}

void TeamColor( const Radar2Blip& b, int* r, int* g, int* bl )
{
	if ( b.vip )
	{
		*r = 255, *g = 215, *bl = 40;
	}
	else if ( b.team == 1 )
	{
		*r = 240, *g = 90, *bl = 60;
	}
	else
	{
		*r = 80, *g = 160, *bl = 255;
	}
}

// the HUD's FillRGBA: an additive rectangle
void AdditiveRect( float x, float y, float w, float h, int r, int g, int b, int alpha )
{
	glColor4f( r / 255.0f, g / 255.0f, b / 255.0f, alpha / 255.0f );
	FillRect( x, y, w, h );
}

// health.cpp DrawRadarDot (flags 0): a square, or a line with a foot / a hat for teammates well below / above
void ClassicDot( float x, float y, float z, float size, float S, int r, int g, int b, int alpha )
{
	if ( z < -128.0f )
	{
		const float offset = ( -z <= 3096.0f ? (float)(int)( -z / 400.0f ) : 7.0f ) * S;
		AdditiveRect( x, y, S, 2.0f * size + offset, r, g, b, alpha );
		AdditiveRect( x - 2.0f * S, y + 2.0f * size + offset, 5.0f * S, S, r, g, b, alpha );
	}
	else if ( z <= 128.0f )
		AdditiveRect( x, y, size, size, r, g, b, alpha );
	else
	{
		const float offset = ( z <= 3096.0f ? (float)(int)( z / 400.0f ) : 7.0f ) * S;
		AdditiveRect( x - 2.0f * S, y, 5.0f * S, S, r, g, b, alpha );
		AdditiveRect( x, y + S, S, offset + 2.0f * size - S, r, g, b, alpha );
	}
}

} // namespace

void Radar2_DrawSquare( const Radar2Square& radar )
{
	const int W = radar.screenWide, H = radar.screenTall;
	if ( W <= 0 || H <= 0 || radar.size <= 0.0f || !radar.origin || !radar.angles )
		return;
	const float S = radar.unit;
	const float x0 = radar.x0, y0 = radar.y0, size = radar.size;
	const float* origin = radar.origin;
	const float* angles = radar.angles;
	const float time = radar.time;
	const float mapAlpha = radar.mapAlpha < 0.0f ? 0.0f : radar.mapAlpha > 1.0f ? 1.0f : radar.mapAlpha;

	RadarView v;
	v.half = size * 0.5f;
	v.cx = x0 + v.half;
	v.cy = y0 + v.half;
	v.scale = v.half / ( radar.range > 1.0f ? radar.range : 1.0f );
	v.rotate = radar.rotate;
	const float yaw = angles[1] * kPi / 180.0f;
	v.sinYaw = sinf( yaw );
	v.cosYaw = cosf( yaw );
	v.ox = origin[0];
	v.oy = origin[1];

	Saved2D saved;
	Begin2D( W, H, saved );
	ScissorToClip( radar.clip, saved, W, H );

	// background
	Color( 8, 12, 16, 0.55f );
	FillRect( x0, y0, size, size );

	glEnable( GL_SCISSOR_TEST );
	Scissor( x0, y0, size, size, radar.clip, saved, W, H );

	// map
	if ( radar.map.texture && radar.map.texW > 0 && radar.map.texH > 0 )
	{
		glEnable( GL_TEXTURE_2D );
		glBindTexture( GL_TEXTURE_2D, radar.map.texture );
		glBlendFunc( GL_ONE, GL_ONE_MINUS_SRC_ALPHA );
		glColor4f( mapAlpha, mapAlpha, mapAlpha, mapAlpha );
		const float u1 = (float)radar.map.w / radar.map.texW, v1 = (float)radar.map.h / radar.map.texH;
		const float tc[4][2] = { { 0, 0 }, { u1, 0 }, { u1, v1 }, { 0, v1 } };
		glBegin( GL_QUADS );
		for ( int i = 0; i < 4; i++ )
		{
			float px, py;
			v.ToScreen( radar.mapCorner[i][0], radar.mapCorner[i][1], &px, &py );
			glTexCoord2f( tc[i][0], tc[i][1] );
			glVertex2f( px, py );
		}
		glEnd();
		glDisable( GL_TEXTURE_2D );
		glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	}

	const float blink = fmodf( time * 2.0f, 1.0f ) < 0.5f ? 1.0f : 0.0f;

	// teammates' death marks, hostages, then the bomb, then players on top
	for ( int pass = 0; pass < 4; pass++ )
	{
		for ( int i = 0; i < radar.numBlips; i++ )
		{
			const Radar2Blip& b = radar.blips[i];
			static const int order[4] = { RADAR2_DEAD, RADAR2_HOSTAGE, RADAR2_BOMB, RADAR2_PLAYER };
			const int want = order[pass];
			if ( b.kind != want )
				continue;
			float sx, sy;
			v.ToScreen( b.origin[0], b.origin[1], &sx, &sy );
			const bool edge = ClampToRadar( v, 4.0f * S, &sx, &sy );
			const float dz = b.origin[2] - origin[2];

			if ( b.kind == RADAR2_DEAD )
			{
				const float shown = 1.0f - ( b.fade < 0.0f ? 0.0f : b.fade > 1.0f ? 1.0f : b.fade );
				if ( edge || shown <= 0.0f )
					continue;
				Color( 0, 0, 0, 0.8f * shown );
				Cross( sx, sy, 5.2f * S, 1.9f * S );
				Color( 235, 30, 30, shown );
				Cross( sx, sy, 4.2f * S, 1.0f * S );
				continue;
			}

			if ( b.kind == RADAR2_HOSTAGE )
			{
				// shown steadily while its position is known (the round radar blinks it, which on this one made
				// it pop in and out)
				// the HUD's hostage head (no backing: its black is see-through); red when the hostage is dead
				const float icon = 13.0f * S;
				const int hr = b.dead ? 250 : 210, hg = b.dead ? 50 : 235, hb = b.dead ? 40 : 255;
				if ( !DrawIcon( radar.hostage, sx, sy, icon, hr, hg, hb, edge ? 0.7f : 1.0f ) )
				{
					Color( hr, hg, hb, 0.95f );
					Disc( sx, sy - 1.5f * S, 2.0f * S );                      // head
					FillRect( sx - 2.8f * S, sy + 1.0f * S, 5.6f * S, 3.0f * S ); // shoulders
				}
				continue;
			}

			if ( b.kind == RADAR2_BOMB )
			{
				const float icon = 15.0f * S;
				Color( 0, 0, 0, 0.55f );
				Disc( sx, sy, icon * 0.62f );
				if ( b.planted )
				{
					// planted: pulsing red ring and a blinking icon
					const float pulse = fmodf( time * 1.5f, 1.0f );
					Color( 255, 40, 30, 0.8f * ( 1.0f - pulse ) );
					Ring( sx, sy, icon * ( 0.6f + pulse * 0.6f ), 1.5f * S );
					if ( blink > 0.0f )
						DrawC4( radar.c4, sx, sy, icon, 255, 50, 40, 1.0f );
					else
						DrawC4( radar.c4, sx, sy, icon, 255, 200, 160, 1.0f );
				}
				else
					DrawC4( radar.c4, sx, sy, icon, 255, 170, 60, b.flash || !edge ? 1.0f : 0.6f );
				continue;
			}

			// players
			int r, g, bl;
			TeamColor( b, &r, &g, &bl );
			const float sz = ( b.tracked ? 6.0f : 4.5f ) * S;
			if ( b.flash )
			{
				Color( 255, 140, 30, 0.9f );
				Ring( sx, sy, sz * 1.5f, 1.5f * S );
			}
			if ( b.hasYaw && !edge )
			{
				float dx, dy;
				v.Direction( b.yaw, &dx, &dy );
				Color( 0, 0, 0, 0.75f );
				Arrow( sx, sy, dx, dy, sz * 1.35f );
				Color( r, g, bl, 1.0f );
				Arrow( sx, sy, dx, dy, sz );
			}
			else
			{
				const float rad = edge ? sz * 0.6f : sz * 0.75f;
				Color( 0, 0, 0, 0.75f );
				Disc( sx, sy, rad + 1.0f * S );
				Color( r, g, bl, edge ? 0.7f : 1.0f );
				Disc( sx, sy, rad );
			}
			// above / below you
			if ( !edge && ( dz > 100.0f || dz < -100.0f ) )
			{
				const bool up = dz > 0.0f;
				const float ty = sy + ( up ? -sz * 1.9f : sz * 1.9f );
				Color( 0, 0, 0, 0.7f );
				Triangle( sx, ty, 2.6f * S, up );
				Color( 235, 235, 235, 0.95f );
				Triangle( sx, ty, 1.8f * S, up );
			}
			if ( b.carrier )
				DrawC4( radar.c4, sx + sz * 1.6f, sy - sz * 1.4f, 9.0f * S, 255, 170, 60, 1.0f );
		}
	}

	// you: view cone and arrow in the centre
	{
		float dx = 0.0f, dy = -1.0f;
		if ( !v.rotate )
			v.Direction( angles[1], &dx, &dy );
		const float fov = ( radar.fovDegrees > 0.0f ? radar.fovDegrees : 90.0f ) * 0.5f * kPi / 180.0f;
		const float len = 34.0f * S, c = cosf( fov ), s = sinf( fov );
		glBegin( GL_TRIANGLES );
		glColor4f( 1, 1, 1, 0.16f );
		glVertex2f( v.cx, v.cy );
		glColor4f( 1, 1, 1, 0.0f );
		glVertex2f( v.cx + ( dx * c - dy * s ) * len, v.cy + ( dy * c + dx * s ) * len );
		glVertex2f( v.cx + ( dx * c + dy * s ) * len, v.cy + ( dy * c - dx * s ) * len );
		glEnd();
		Color( 0, 0, 0, 0.8f );
		Arrow( v.cx, v.cy, dx, dy, 6.5f * S );
		Color( 255, 255, 255, 1.0f );
		Arrow( v.cx, v.cy, dx, dy, 5.0f * S );
	}

	ScissorToClip( radar.clip, saved, W, H );

	// frame
	{
		const float t = S;
		Color( 0, 0, 0, 0.8f );
		FillRect( x0 - t, y0 - t, size + 2 * t, t );
		FillRect( x0 - t, y0 + size, size + 2 * t, t );
		FillRect( x0 - t, y0, t, size );
		FillRect( x0 + size, y0, t, size );
		Color( 200, 210, 220, 0.55f );
		FillRect( x0, y0, size, t );
		FillRect( x0, y0 + size - t, size, t );
		FillRect( x0, y0, t, size );
		FillRect( x0 + size - t, y0, t, size );
		if ( !v.rotate )
		{
			// north mark
			Color( 255, 255, 255, 0.8f );
			Triangle( v.cx, y0 + 4.0f * S, 3.0f * S, true );
		}
	}

	End2D( saved );
}

void Radar2_DrawClassic( const Radar2Classic& radar )
{
	const int W = radar.screenWide, H = radar.screenTall;
	if ( W <= 0 || H <= 0 || radar.unit <= 0.0f || !radar.origin || !radar.angles )
		return;
	const float S = radar.unit;
	const float x0 = radar.x0, y0 = radar.y0, size = 128.0f * S;

	Saved2D saved;
	Begin2D( W, H, saved );
	ScissorToClip( radar.clip, saved, W, H );

	// the radar picture: SPR_DrawAdditive with colour 25 75 25, or the solid one with holes in 200 200 200
	if ( radar.sprite.texture && radar.sprite.texW > 0 && radar.sprite.texH > 0 )
	{
		glEnable( GL_TEXTURE_2D );
		glBindTexture( GL_TEXTURE_2D, radar.sprite.texture );
		if ( radar.opaque )
		{
			glEnable( GL_ALPHA_TEST );
			glAlphaFunc( GL_GREATER, 0.5f );
			glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			Color( 200, 200, 200, 1.0f );
		}
		else
		{
			glBlendFunc( GL_ONE, GL_ONE );
			Color( 25, 75, 25, 1.0f );
		}
		const float u1 = (float)radar.sprite.w / radar.sprite.texW, v1 = (float)radar.sprite.h / radar.sprite.texH;
		glBegin( GL_QUADS );
		glTexCoord2f( 0, 0 );
		glVertex2f( x0, y0 );
		glTexCoord2f( u1, 0 );
		glVertex2f( x0 + size, y0 );
		glTexCoord2f( u1, v1 );
		glVertex2f( x0 + size, y0 + size );
		glTexCoord2f( 0, v1 );
		glVertex2f( x0, y0 + size );
		glEnd();
		glDisable( GL_ALPHA_TEST );
		glDisable( GL_TEXTURE_2D );
	}
	else
	{
		glBlendFunc( GL_ONE, GL_ONE );
		Color( 18, 54, 18, 1.0f );
		Disc( x0 + size * 0.5f, y0 + size * 0.5f, size * 0.5f );
	}

	// teammates: the HUD's sums (health.cpp DrawRadar), additive dots
	glBlendFunc( GL_SRC_ALPHA, GL_ONE );
	const float* origin = radar.origin;
	const float viewYaw = radar.angles[1];
	const float center = 64.0f * S, edge = center * 2.0f, dotSize = 2.0f * S;
	const double range = 2048.0;
	for ( int i = 0; i < radar.numBlips; i++ )
	{
		const Radar2Blip& b = radar.blips[i];
		if ( b.kind != RADAR2_PLAYER )
			continue;
		const float xDiff = b.origin[0] - origin[0], yDiff = b.origin[1] - origin[1];
		const float distance = (float)sqrt( (double)xDiff * xDiff + (double)yDiff * yDiff );
		if ( distance <= 0.0f )
			continue;
		double bearing = atan2( (double)yDiff, (double)xDiff ) * 180.0 / 3.141592654;
		if ( bearing < 0.0 )
			bearing += 360.0;
		const double radians = ( viewYaw - bearing ) * 3.141592654 / 180.0;
		const float rotatedX = (float)( sin( radians ) * distance );
		const float rotatedY = (float)( -distance * cos( radians ) );
		const double scale = distance <= range ? 1.0 : distance / range;
		const float x = center + (float)( rotatedX / scale * 0.03125 ) * S;
		const float y = center + (float)( rotatedY / scale * 0.03125 ) * S;
		if ( x < 0.0f || y < 0.0f || x > edge || y > edge )
			continue;
		const bool red = ( b.carrier && radar.viewerIsT ) || b.vip;
		ClassicDot( x0 + x, y0 + y, b.origin[2] - origin[2], dotSize, S, red ? 250 : 150, red ? 0 : 75, red ? 0 : 250, 235 );
	}

	End2D( saved );
}

// ---------------------------------------------------------------------------------------------------------------
// the Advanced window's radar preview
// ---------------------------------------------------------------------------------------------------------------

namespace
{

// a position on de_dust2's overview picture (pixels of the 1024 x 768 picture) in world X / Y
void OverviewSpot( const Radar2Preview& p, float px, float py, float out[2] )
{
	const float u = px / ( p.map.w > 0 ? p.map.w : 1024.0f ), v = py / ( p.map.h > 0 ? p.map.h : 768.0f );
	const float( *c )[2] = p.mapCorner;
	for ( int i = 0; i < 2; i++ )
		out[i] = c[0][i] * ( 1 - u ) * ( 1 - v ) + c[1][i] * u * ( 1 - v ) + c[2][i] * u * v + c[3][i] * ( 1 - u ) * v;
}

float Smooth( float x )
{
	x = x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x;
	return x * x * ( 3.0f - 2.0f * x );
}

// world yaw (degrees) of the direction from a to b
float YawOf( const float a[2], const float b[2] )
{
	return atan2f( b[1] - a[1], b[0] - a[0] ) * 180.0f / kPi;
}

// someone walking from a to b and back, at time t: where they are and which way they face. Each crossing takes 'walk'
// seconds (setting off and stopping gently); at each end they stand for 'rest' seconds and turn round in its middle,
// smoothly - nobody snaps round, so a radar turning with the view turns calmly too
void WalkBackAndForth( const float a[2], const float b[2], float t, float walk, float rest, float out[2], float* yaw )
{
	const float cycle = 2.0f * ( walk + rest );
	float phase = fmodf( t, cycle );
	phase = phase < 0.0f ? phase + cycle : phase;
	const float there = YawOf( a, b );
	const float turnFrom = rest * 0.2f, turnFor = rest * 0.6f;	// the turn: the middle 60 % of the rest
	float s = 0.0f;
	if ( phase < walk )
	{
		s = Smooth( phase / walk );	// a to b
		*yaw = there;
	}
	else if ( ( phase -= walk ) < rest )
	{
		s = 1.0f;					// standing at b, turning round
		*yaw = there + 180.0f * Smooth( ( phase - turnFrom ) / turnFor );
	}
	else if ( ( phase -= rest ) < walk )
	{
		s = 1.0f - Smooth( phase / walk );	// b to a
		*yaw = there + 180.0f;
	}
	else
	{
		phase -= walk;				// standing at a, turning round (on round the same way: back to facing b)
		*yaw = there + 180.0f + 180.0f * Smooth( ( phase - turnFrom ) / turnFor );
	}
	out[0] = a[0] + ( b[0] - a[0] ) * s;
	out[1] = a[1] + ( b[1] - a[1] ) * s;
}

// you: walking mid from east to west and back, glancing a little to the sides as you go
void PreviewViewer( const Radar2Preview& p, float origin[3], float angles[3] )
{
	float east[2], west[2];
	OverviewSpot( p, 770.0f, 412.0f, east );
	OverviewSpot( p, 470.0f, 412.0f, west );
	float yaw = 0.0f;
	WalkBackAndForth( east, west, p.time, 6.0f, 2.2f, origin, &yaw );
	origin[2] = 0.0f;
	angles[0] = 0.0f;
	angles[1] = yaw + 12.0f * sinf( p.time * 0.55f );
	angles[2] = 0.0f;
}

// a teammate walking back and forth between two overview spots: 'period' seconds there and back, 'offset' a part of
// it so they do not all move together
void PreviewMate( const Radar2Preview& p, Radar2Blip& blip, float ax, float ay, float bx, float by, float z, float period,
	float offset )
{
	float a[2], b[2];
	OverviewSpot( p, ax, ay, a );
	OverviewSpot( p, bx, by, b );
	const float rest = 1.2f;
	memset( &blip, 0, sizeof( blip ) );
	blip.kind = RADAR2_PLAYER;
	blip.team = 1;
	WalkBackAndForth( a, b, p.time + offset * period, period * 0.5f - rest, rest, blip.origin, &blip.yaw );
	blip.origin[2] = z;
	blip.hasYaw = true;
}

} // namespace

void Radar2_DrawPreviewBox( const Radar2Preview& p )
{
	if ( p.box <= 0 || p.screenWide <= 0 || p.screenTall <= 0 )
		return;
	const float box = (float)p.box;
	const float clip[4] = { (float)p.x, (float)p.y, box, box };

	// the team at this moment
	float origin[3], angles[3];
	PreviewViewer( p, origin, angles );
	Radar2Blip blips[4];
	PreviewMate( p, blips[0], 452.0f, 470.0f, 452.0f, 548.0f, -150.0f, 9.0f, 0.0f );	// down in the tunnel, below you
	PreviewMate( p, blips[1], 520.0f, 140.0f, 548.0f, 220.0f, 0.0f, 8.0f, 0.3f );		// up in the corridor
	PreviewMate( p, blips[2], 868.0f, 300.0f, 868.0f, 470.0f, 0.0f, 11.0f, 0.6f );		// the bomb carrier on the right
	blips[2].carrier = true;
	memset( &blips[3], 0, sizeof( blips[3] ) );
	blips[3].kind = RADAR2_DEAD;	// where a teammate died: shown 5 seconds, fading over one, again every 9 seconds
	OverviewSpot( p, 645.0f, 462.0f, blips[3].origin );
	const float deathAge = fmodf( p.time, 9.0f );
	blips[3].fade = deathAge < 5.0f ? 0.0f : deathAge < 6.0f ? deathAge - 5.0f : 1.0f;

	if ( p.square )
	{
		const float size = p.size < 64.0f ? 64.0f : p.size > 400.0f ? 400.0f : p.size;
		const float range = p.range < 300.0f ? 300.0f : p.range > 8000.0f ? 8000.0f : p.range;
		// the radar and its frame (one unit round the map) fill the box; the unit is the HUD pixel at this size, so the
		// dots, icons and frame keep the proportions they have in game
		const float unit = box / ( size + 2.0f );
		Radar2Square radar;
		memset( &radar, 0, sizeof( radar ) );
		radar.x0 = p.x + unit;
		radar.y0 = p.y + unit;
		radar.size = box - 2.0f * unit;
		radar.unit = unit;
		radar.range = range;
		radar.mapAlpha = p.mapAlpha;
		radar.rotate = p.rotate;
		radar.fovDegrees = 90.0f;
		radar.screenWide = p.screenWide;
		radar.screenTall = p.screenTall;
		memcpy( radar.clip, clip, sizeof( clip ) );
		radar.origin = origin;
		radar.angles = angles;
		radar.blips = blips;
		radar.numBlips = 4;
		radar.time = p.time;
		radar.map = p.map;
		memcpy( radar.mapCorner, p.mapCorner, sizeof( radar.mapCorner ) );
		radar.c4 = p.c4;
		radar.hostage = p.hostage;
		Radar2_DrawSquare( radar );
	}
	else
	{
		// the round radar picture (128 units) fills the box. In game it adds its light to the scene behind; here it
		// goes on a dark disc of its own size
		const float unit = box / 128.0f;
		{
			Saved2D saved;
			Begin2D( p.screenWide, p.screenTall, saved );
			ScissorToClip( clip, saved, p.screenWide, p.screenTall );
			const float cx = p.x + box * 0.5f, cy = p.y + box * 0.5f, r = box * 0.5f;
			Color( 14, 16, 14, 1.0f );
			glBegin( GL_TRIANGLE_FAN );
			glVertex2f( cx, cy );
			for ( int i = 0; i <= 96; i++ )
			{
				const float a = i * 2.0f * kPi / 96.0f;
				glVertex2f( cx + cosf( a ) * r, cy + sinf( a ) * r );
			}
			glEnd();
			End2D( saved );
		}
		Radar2Classic radar;
		memset( &radar, 0, sizeof( radar ) );
		radar.x0 = (float)p.x;
		radar.y0 = (float)p.y;
		radar.unit = unit;
		radar.screenWide = p.screenWide;
		radar.screenTall = p.screenTall;
		memcpy( radar.clip, clip, sizeof( clip ) );
		radar.origin = origin;
		radar.angles = angles;
		radar.blips = blips;
		radar.numBlips = 4;
		radar.viewerIsT = true;
		radar.opaque = p.opaque;
		radar.sprite = p.opaque ? p.roundOpaque : p.round;
		Radar2_DrawClassic( radar );
	}
}
