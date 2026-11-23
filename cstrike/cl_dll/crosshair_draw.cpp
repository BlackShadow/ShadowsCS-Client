// crosshair_draw.cpp - see crosshair_draw.h.
//
// The spread, size and line sums are the HUD's own (ammo.cpp CHudAmmo::DrawCrosshair / CalculateCrosshairSize), moved
// here unchanged so the preview opens and closes exactly like the game. With the options at their defaults (thickness 1,
// gap 0, no outline / dot / T shape) the parts are the game's four lines, pixel for pixel. Thicker lines grow round the
// middle lines; the gap is measured from the thick middle so both sides stay even.
#include "crosshair_draw.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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

bool SameWord( const char* a, const char* b )
{
	for ( ; *a && *b; a++, b++ )
	{
		if ( tolower( (unsigned char)*a ) != tolower( (unsigned char)*b ) )
			return false;
	}
	return *a == *b;
}

typedef void( APIENTRY* UseProgramFn )( GLuint program );
typedef void( APIENTRY* ActiveTextureFn )( GLenum texture );

// some drivers return 1, 2, 3 or -1 instead of NULL for names they do not have
PROC GetGLFunction( const char* name )
{
	PROC p = wglGetProcAddress( name );
	const INT_PTR v = (INT_PTR)p;
	return ( v == 0 || v == 1 || v == 2 || v == 3 || v == -1 ) ? NULL : p;
}

// the preview routine (seconds into each 6 second round)
const double kStep = 0.01;
const double kRoundLength = 6.0;
const double kRunFrom = 1.5, kRunTo = 3.0;
const double kFireFrom = 3.4, kShotEvery = 0.1;	// an AK-47 fires ten times a second
const int kBurst = 7;

} // namespace

int Crosshair_RestingDistance( int distance, int accuracy, bool onGround, bool ducking, bool fast )
{
	if ( !onGround && ( accuracy & 1 ) )
		distance *= 2;
	else if ( ducking && ( accuracy & 4 ) )
		distance = (int)(float)( distance * 0.5 );
	else if ( fast && ( accuracy & 2 ) )
		distance = (int)(float)( distance * 1.5 );
	if ( accuracy & 8 )
		distance = (int)(float)( distance * 1.4f );
	if ( accuracy & 16 )
		distance = (int)(float)( distance * 1.4f );
	return distance;
}

int Crosshair_ParseSize( const char* text )
{
	if ( !text )
		return -1;
	int size = atoi( text );
	if ( (unsigned int)size >= 4 || ( !size && strcmp( text, "0" ) ) )
		size = -1;
	if ( SameWord( text, "auto" ) )
		size = 0;
	else if ( SameWord( text, "small" ) )
		size = 1;
	else if ( SameWord( text, "medium" ) )
		size = 2;
	else if ( SameWord( text, "large" ) )
		size = 3;
	return size;
}

int Crosshair_ScaleBase( int size, int screenWide )
{
	switch ( size )
	{
	case 0:
		return screenWide <= 640 ? 1024 : ( screenWide <= 1023 ? 800 : 640 );
	case 1:
		return 1024;
	case 2:
		return 800;
	default:
		return 640;
	}
}

void Crosshair_Step( float& distance, int& alpha, int baseDistance, int delta, bool shot, bool recoil )
{
	if ( !shot )
	{
		distance = (float)( distance - ( 0.013 * distance + 0.1 ) );
		alpha += 2;
	}
	else
	{
		if ( recoil )
			distance += (float)delta;
		alpha -= 40;
		if ( distance > 15 )
			distance = 15;
		if ( alpha < 120 )
			alpha = 120;
	}
	if ( (float)baseDistance > distance )
		distance = (float)baseDistance;
	if ( alpha > 255 )
		alpha = 255;
}

void Crosshair_Measure( float distance, int baseDistance, int gapSetting, int lengthSetting, int screenWide, int scaleBase,
	int* gap, int* length )
{
	int lineLength = (int)(float)( ( distance - baseDistance ) * 0.5 ) + 5 + lengthSetting;
	if ( lineLength < 0 )
		lineLength = 0;
	double scaledDistance = distance + gapSetting;
	if ( screenWide != scaleBase && scaleBase > 0 )
	{
		scaledDistance = scaledDistance * screenWide / scaleBase;
		lineLength = screenWide * lineLength / scaleBase;
	}
	if ( scaledDistance < 0.0 )
		scaledDistance = 0.0;
	*gap = (int)(float)scaledDistance;
	*length = lineLength;
}

int Crosshair_Build( const CrosshairSettings& settings, bool additive, int r, int g, int b, int alpha, int cx, int cy,
	int gap, int length, CrosshairRect* out )
{
	const int t = settings.thickness < 1 ? 1 : settings.thickness > 16 ? 16 : settings.thickness;
	const int before = ( t - 1 ) / 2, after = t / 2;	// the middle lines: cx - before .. cx + after (one pixel: cx)
	const int a = settings.alpha > 0 ? ( settings.alpha < 255 ? settings.alpha : 255 ) : alpha;
	struct Part
	{
		int x, y, w, h;
	} parts[5];
	int numParts = 0;
	if ( length > 0 )
	{
		// the game's order: left, right, top, bottom
		parts[numParts++] = { cx - before - gap - length + 1, cy - before, length, t };
		parts[numParts++] = { cx + after + gap, cy - before, length, t };
		if ( !settings.tshape )
			parts[numParts++] = { cx - before, cy - before - gap - length + 1, t, length };
		parts[numParts++] = { cx - before, cy + after + gap, t, length };
	}
	if ( settings.dot > 0 )
	{
		const int d = settings.dot < 16 ? settings.dot : 16;
		parts[numParts++] = { cx - ( d - 1 ) / 2, cy - ( d - 1 ) / 2, d, d };
	}
	int count = 0;
	if ( settings.outline > 0 )
	{
		// every edge first, so no edge lands on a line
		const int o = settings.outline < 4 ? settings.outline : 4;
		for ( int i = 0; i < numParts; i++ )
			out[count++] = { parts[i].x - o, parts[i].y - o, parts[i].w + 2 * o, parts[i].h + 2 * o, 0, 0, 0, a, false };
	}
	for ( int i = 0; i < numParts; i++ )
		out[count++] = { parts[i].x, parts[i].y, parts[i].w, parts[i].h, r, g, b, a, additive };
	return count;
}

CCrosshairRoutine::CCrosshairRoutine()
	: m_flStart( -1.0 ), m_flDone( 0.0 ), m_flDistance( (float)kCrosshairProfile.distance ), m_iAlpha( 255 ),
	  m_iBase( kCrosshairProfile.distance ),
	  m_iShots( 0 )
{
}

void CCrosshairRoutine::Advance( double now, const CrosshairSettings& settings )
{
	if ( m_flStart < 0.0 || now < m_flStart )
	{
		m_flStart = now;
		m_flDone = 0.0;
	}
	const double target = now - m_flStart;
	if ( target - m_flDone > 1.0 )
		m_flDone = target - 1.0;	// back after a while (the window was closed): at most a second of steps
	while ( m_flDone + kStep <= target )
	{
		m_flDone += kStep;
		const double t = fmod( m_flDone, kRoundLength );
		// the resting distance this frame, with the HUD's widening for movement (running, standing)
		const CrosshairProfile& weapon = kCrosshairProfile;
		int base = weapon.distance;
		if ( settings.dynamic && weapon.accuracy )
			base = Crosshair_RestingDistance( base, weapon.accuracy, true, false, t >= kRunFrom && t < kRunTo );
		bool shot = false;
		if ( t < kFireFrom )
			m_iShots = 0;
		else if ( m_iShots < kBurst && (int)( ( t - kFireFrom ) / kShotEvery ) + 1 > m_iShots )
		{
			shot = true;
			m_iShots++;
		}
		Crosshair_Step( m_flDistance, m_iAlpha, base, weapon.delta, shot, settings.recoil );
		m_iBase = base;
	}
}

int Crosshair_PreviewReach( const CrosshairSettings& settings, int screenWide )
{
	int gap = 0, length = 0;
	Crosshair_Measure( 15.0f, kCrosshairProfile.distance, settings.gap, settings.length, screenWide,
		Crosshair_ScaleBase( settings.size, screenWide ), &gap, &length );
	const int t = settings.thickness < 1 ? 1 : settings.thickness > 16 ? 16 : settings.thickness;
	const int dot = settings.dot < 16 ? settings.dot : 16;
	int reach = length > 0 ? gap + length + t / 2 : 0;
	if ( dot / 2 + 1 > reach )
		reach = dot / 2 + 1;
	return reach + ( settings.outline < 4 ? settings.outline : 4 );
}

void Crosshair_DrawPreview( const CrosshairSettings& settings, const CCrosshairRoutine& routine, int x, int y, int wide,
	int tall, int screenWide, int screenTall )
{
	if ( wide <= 0 || tall <= 0 || screenWide <= 0 || screenTall <= 0 )
		return;
	int gap = 0, length = 0;
	Crosshair_Measure( routine.Distance(), routine.BaseDistance(), settings.gap, settings.length, screenWide,
		Crosshair_ScaleBase( settings.size, screenWide ), &gap, &length );
	CrosshairRect rects[CROSSHAIR_MAX_RECTS];
	const int count = Crosshair_Build( settings, settings.translucent, settings.r, settings.g, settings.b, routine.Alpha(),
		x + wide / 2, y + tall / 2, gap, length, rects );

	// 2D in screen pixels over whatever is set up, and every setting back afterwards (matrices copied, not pushed: the
	// stacks can be only two deep)
	static UseProgramFn useProgram = (UseProgramFn)GetGLFunction( "glUseProgram" );
	static ActiveTextureFn activeTexture = (ActiveTextureFn)GetGLFunction( "glActiveTexture" );
	GLint program = 0, activeUnit = GL_TEXTURE0;
	glGetIntegerv( GL_CURRENT_PROGRAM, &program );
	glGetIntegerv( GL_ACTIVE_TEXTURE, &activeUnit );
	for ( int i = 0; i < 32 && glGetError() != GL_NO_ERROR; i++ )
		;	// the queries above are errors on old drivers that lack those things
	GLfloat projection[16], modelview[16];
	glGetFloatv( GL_PROJECTION_MATRIX, projection );
	glGetFloatv( GL_MODELVIEW_MATRIX, modelview );
	GLint viewport[4] = { 0, 0, screenWide, screenTall };
	glGetIntegerv( GL_VIEWPORT, viewport );
	glPushAttrib( GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_SCISSOR_BIT | GL_TEXTURE_BIT | GL_CURRENT_BIT | GL_TRANSFORM_BIT | GL_POLYGON_BIT );
	if ( program && useProgram )
		useProgram( 0 );
	if ( activeUnit != GL_TEXTURE0 && activeTexture )
		activeTexture( GL_TEXTURE0 );
	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();
	glOrtho( 0, screenWide, screenTall, 0, -1, 1 );
	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity();
	glDisable( GL_DEPTH_TEST );
	glDisable( GL_CULL_FACE );
	glDisable( GL_ALPHA_TEST );
	glDisable( GL_FOG );
	glDisable( GL_TEXTURE_2D );
	glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	// only inside the box
	const double kx = (double)viewport[2] / screenWide, ky = (double)viewport[3] / screenTall;
	glEnable( GL_SCISSOR_TEST );
	glScissor( viewport[0] + (int)( x * kx ), viewport[1] + (int)( ( screenTall - y - tall ) * ky ), (int)( wide * kx + 0.5 ),
		(int)( tall * ky + 0.5 ) );
	// the engine's Draw_FillRGBA (adds) / Draw_FillRGBABlend (blends): untextured quads in the colour, alpha a / 255
	glEnable( GL_BLEND );
	for ( int i = 0; i < count; i++ )
	{
		const CrosshairRect& rect = rects[i];
		glBlendFunc( GL_SRC_ALPHA, rect.additive ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA );
		glColor4f( (float)( rect.r / 255.0 ), (float)( rect.g / 255.0 ), (float)( rect.b / 255.0 ), (float)( rect.a / 255.0 ) );
		glBegin( GL_QUADS );
		glVertex2f( (float)rect.x, (float)rect.y );
		glVertex2f( (float)( rect.x + rect.w ), (float)rect.y );
		glVertex2f( (float)( rect.x + rect.w ), (float)( rect.y + rect.h ) );
		glVertex2f( (float)rect.x, (float)( rect.y + rect.h ) );
		glEnd();
	}
	glMatrixMode( GL_PROJECTION );
	glLoadMatrixf( projection );
	glMatrixMode( GL_MODELVIEW );
	glLoadMatrixf( modelview );
	glPopAttrib();
	if ( activeUnit != GL_TEXTURE0 && activeTexture )
		activeTexture( (GLenum)activeUnit );
	if ( program && useProgram )
		useProgram( (GLuint)program );
}
