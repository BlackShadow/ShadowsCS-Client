// stretch_view.cpp - see stretch_view.h.
//
// The middle part is copied with a framebuffer blit into a texture of our own (in fullscreen the engine draws into a
// multisampled framebuffer, which glCopyTexSubImage2D cannot read; a blit of the same size resolves it), then drawn over
// the whole screen with smooth filtering. GL state is put back the way radar2_draw.cpp does it: attributes pushed,
// matrices copied and reloaded (the stacks can be only two deep), shader program, texture unit and framebuffers restored.
// No engine change: the engine calls HUD_Redraw right after drawing the world (HL25-Engine/engine/vgui_EngineSurfaceHW.cpp,
// VGui_ViewportPaintBackground).
#include "hud.h"
#include "cl_util.h"
#include "stretch_view.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <gl/GL.h>

#ifndef GL_FRAMEBUFFER_EXT
#define GL_FRAMEBUFFER_EXT 0x8D40
#endif
#ifndef GL_READ_FRAMEBUFFER_EXT
#define GL_READ_FRAMEBUFFER_EXT 0x8CA8
#define GL_DRAW_FRAMEBUFFER_EXT 0x8CA9
#define GL_READ_FRAMEBUFFER_BINDING_EXT 0x8CAA
#endif
#ifndef GL_DRAW_FRAMEBUFFER_BINDING_EXT
#define GL_DRAW_FRAMEBUFFER_BINDING_EXT 0x8CA6
#endif
#ifndef GL_COLOR_ATTACHMENT0_EXT
#define GL_COLOR_ATTACHMENT0_EXT 0x8CE0
#endif
#ifndef GL_FRAMEBUFFER_COMPLETE_EXT
#define GL_FRAMEBUFFER_COMPLETE_EXT 0x8CD5
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_RGBA8
#define GL_RGBA8 0x8058
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

typedef void( APIENTRY* GenFramebuffersFn )( GLsizei n, GLuint* framebuffers );
typedef void( APIENTRY* BindFramebufferFn )( GLenum target, GLuint framebuffer );
typedef void( APIENTRY* FramebufferTexture2DFn )( GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level );
typedef GLenum( APIENTRY* CheckFramebufferStatusFn )( GLenum target );
typedef void( APIENTRY* BlitFramebufferFn )( GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0,
	GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter );
typedef void( APIENTRY* UseProgramFn )( GLuint program );
typedef void( APIENTRY* ActiveTextureFn )( GLenum texture );

cvar_t* cl_stretch43;

GenFramebuffersFn genFramebuffers;
BindFramebufferFn bindFramebuffer;
FramebufferTexture2DFn framebufferTexture2D;
CheckFramebufferStatusFn checkFramebufferStatus;
BlitFramebufferFn blitFramebuffer;
UseProgramFn useProgram;
ActiveTextureFn activeTexture;
bool g_bLoaded, g_bUnavailable;

GLuint g_texture, g_fbo;
int g_texWide, g_texTall;

// some drivers return 1, 2, 3 or -1 instead of NULL for names they do not have
PROC GetGLFunction( const char* name )
{
	PROC p = wglGetProcAddress( name );
	const INT_PTR v = (INT_PTR)p;
	return ( v == 0 || v == 1 || v == 2 || v == 3 || v == -1 ) ? NULL : p;
}

// the EXT names first, as the engine uses them (its framebuffers are EXT ones)
PROC GetEitherFunction( const char* ext, const char* core )
{
	PROC p = GetGLFunction( ext );
	return p ? p : GetGLFunction( core );
}

bool LoadFunctions()
{
	if ( !g_bLoaded )
	{
		g_bLoaded = true;
		genFramebuffers = (GenFramebuffersFn)GetEitherFunction( "glGenFramebuffersEXT", "glGenFramebuffers" );
		bindFramebuffer = (BindFramebufferFn)GetEitherFunction( "glBindFramebufferEXT", "glBindFramebuffer" );
		framebufferTexture2D =
			(FramebufferTexture2DFn)GetEitherFunction( "glFramebufferTexture2DEXT", "glFramebufferTexture2D" );
		checkFramebufferStatus =
			(CheckFramebufferStatusFn)GetEitherFunction( "glCheckFramebufferStatusEXT", "glCheckFramebufferStatus" );
		blitFramebuffer = (BlitFramebufferFn)GetEitherFunction( "glBlitFramebufferEXT", "glBlitFramebuffer" );
		useProgram = (UseProgramFn)GetGLFunction( "glUseProgram" );
		activeTexture = (ActiveTextureFn)GetGLFunction( "glActiveTexture" );
		if ( !genFramebuffers || !bindFramebuffer || !framebufferTexture2D || !checkFramebufferStatus || !blitFramebuffer )
		{
			g_bUnavailable = true;
			gEngfuncs.Con_Printf( "Stretched 4:3: the graphics driver lacks framebuffer objects, so it is off\n" );
		}
	}
	return !g_bUnavailable;
}

// the spectator's picture-in-picture window is drawn into the world's frame too: it would be cut and moved
bool InsetWindowShown()
{
	if ( !g_iUser1 || !gHUD.m_Spectator.m_pip || !gHUD.m_Spectator.m_pip->value )
		return false;
	cl_entity_t* local = gEngfuncs.GetLocalPlayer();
	return local && local->index != g_iUser2;
}

// the width of the middle 4:3 part of a screen W x H (exactly centred); W or more: 4:3 or narrower already
int MiddleWide( int W, int H )
{
	int wide = ( H * 4 + 1 ) / 3;
	if ( ( W - wide ) & 1 )
		wide++;
	return wide;
}

} // namespace

bool StretchView_WorksOn( int wide, int tall )
{
	return wide > 0 && tall > 0 && MiddleWide( wide, tall ) < wide;
}

void StretchView_Init()
{
	cl_stretch43 = gEngfuncs.pfnRegisterVariable( (char*)"cl_stretch43", (char*)"0", FCVAR_ARCHIVE );
}

void StretchView_Draw()
{
	if ( !cl_stretch43 || cl_stretch43->value == 0.0f || g_bUnavailable || InsetWindowShown() )
		return;
	// the screen as the HUD is drawn on it (GLBeginHud set the viewport to it)
	GLint vp[4] = { 0, 0, 0, 0 };
	glGetIntegerv( GL_VIEWPORT, vp );
	const int W = vp[2], H = vp[3];
	if ( W <= 0 || H <= 0 )
		return;
	if ( !StretchView_WorksOn( W, H ) )
		return;	// 4:3 or narrower already
	const int srcWide = MiddleWide( W, H );	// the middle 4:3 part
	if ( !LoadFunctions() )
		return;
	const int srcX = vp[0] + ( W - srcWide ) / 2, srcY = vp[1];

	GLint program = 0, activeUnit = GL_TEXTURE0, drawFbo = 0, readFbo = 0;
	glGetIntegerv( GL_CURRENT_PROGRAM, &program );
	glGetIntegerv( GL_ACTIVE_TEXTURE, &activeUnit );
	glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING_EXT, &drawFbo );
	glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING_EXT, &readFbo );
	for ( int i = 0; i < 32 && glGetError() != GL_NO_ERROR; i++ )
		;	// the queries above are errors on old drivers that lack those things
	glPushAttrib( GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_SCISSOR_BIT | GL_TEXTURE_BIT | GL_CURRENT_BIT | GL_TRANSFORM_BIT
		| GL_POLYGON_BIT | GL_DEPTH_BUFFER_BIT );
	if ( program && useProgram )
		useProgram( 0 );
	if ( activeUnit != GL_TEXTURE0 && activeTexture )
		activeTexture( GL_TEXTURE0 );
	GLint boundTexture = 0;
	glGetIntegerv( GL_TEXTURE_BINDING_2D, &boundTexture );
	glDisable( GL_SCISSOR_TEST );	// blits are cut by it too

	bool ok = true;
	if ( !g_texture || g_texWide != srcWide || g_texTall != H )
	{
		if ( !g_texture )
			glGenTextures( 1, &g_texture );
		glBindTexture( GL_TEXTURE_2D, g_texture );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, srcWide, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL );
		if ( !g_fbo )
			genFramebuffers( 1, &g_fbo );
		bindFramebuffer( GL_FRAMEBUFFER_EXT, g_fbo );
		framebufferTexture2D( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, g_texture, 0 );
		ok = checkFramebufferStatus( GL_FRAMEBUFFER_EXT ) == GL_FRAMEBUFFER_COMPLETE_EXT;
		bindFramebuffer( GL_READ_FRAMEBUFFER_EXT, (GLuint)readFbo );
		bindFramebuffer( GL_DRAW_FRAMEBUFFER_EXT, (GLuint)drawFbo );
		g_texWide = srcWide;
		g_texTall = H;
		if ( !ok )
		{
			g_bUnavailable = true;
			gEngfuncs.Con_Printf( "Stretched 4:3: could not make its framebuffer, so it is off\n" );
		}
	}
	if ( ok )
	{
		// the middle part of the frame being drawn, into our texture
		bindFramebuffer( GL_READ_FRAMEBUFFER_EXT, (GLuint)drawFbo );
		bindFramebuffer( GL_DRAW_FRAMEBUFFER_EXT, g_fbo );
		blitFramebuffer( srcX, srcY, srcX + srcWide, srcY + H, 0, 0, srcWide, H, GL_COLOR_BUFFER_BIT, GL_NEAREST );
		bindFramebuffer( GL_READ_FRAMEBUFFER_EXT, (GLuint)readFbo );
		bindFramebuffer( GL_DRAW_FRAMEBUFFER_EXT, (GLuint)drawFbo );

		// and stretched over the whole screen
		GLfloat projection[16], modelview[16];
		glGetFloatv( GL_PROJECTION_MATRIX, projection );
		glGetFloatv( GL_MODELVIEW_MATRIX, modelview );
		glMatrixMode( GL_PROJECTION );
		glLoadIdentity();
		glMatrixMode( GL_MODELVIEW );
		glLoadIdentity();
		glDisable( GL_DEPTH_TEST );
		glDisable( GL_CULL_FACE );
		glDisable( GL_ALPHA_TEST );
		glDisable( GL_BLEND );
		glDisable( GL_FOG );
		glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		glEnable( GL_TEXTURE_2D );
		glBindTexture( GL_TEXTURE_2D, g_texture );
		glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
		glColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
		glBegin( GL_QUADS );
		glTexCoord2f( 0.0f, 0.0f );
		glVertex2f( -1.0f, -1.0f );
		glTexCoord2f( 1.0f, 0.0f );
		glVertex2f( 1.0f, -1.0f );
		glTexCoord2f( 1.0f, 1.0f );
		glVertex2f( 1.0f, 1.0f );
		glTexCoord2f( 0.0f, 1.0f );
		glVertex2f( -1.0f, 1.0f );
		glEnd();
		glMatrixMode( GL_PROJECTION );
		glLoadMatrixf( projection );
		glMatrixMode( GL_MODELVIEW );
		glLoadMatrixf( modelview );
	}

	glBindTexture( GL_TEXTURE_2D, (GLuint)boundTexture );
	glPopAttrib();
	if ( activeUnit != GL_TEXTURE0 && activeTexture )
		activeTexture( (GLenum)activeUnit );
	if ( program && useProgram )
		useProgram( (GLuint)program );
}
