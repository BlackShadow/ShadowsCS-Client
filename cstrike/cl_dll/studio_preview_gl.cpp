// studio_preview_gl.cpp - see studio_preview_gl.h.
//
// Drawn with plain OpenGL right where the menu draws the preview panel (the menu draws immediately, in order):
//   - solid team colour: every pixel the flat colour, no shading - vismods.cpp draws players and their weapons the
//     same way in game (the texture colour replaced by a constant)
//   - normal: the skins, lit by a key light the way the engine lights studio models (ambient + softened lambert); the
//     chrome sunglasses get a normal-based chrome mapping like the engine's
// The weapon is posed on the player's bones (CStudioPreviewModel::MergeBones, the engine's StudioMergeBones).
// All GL state is saved and restored around the drawing: the attribute stacks, the matrices (copied, not pushed: the
// stacks can be only two deep), the shader program and the texture unit. Skin textures use GL names far above the
// engine's own numbering (as radar2.cpp does), 64 per model.
#include "studio_preview_gl.h"
#include "studio_preview.h"

#include <math.h>
#include <string.h>
#include <vector>

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
#ifndef GL_PIXEL_UNPACK_BUFFER_BINDING
#define GL_PIXEL_UNPACK_BUFFER_BINDING 0x88EF
#endif

namespace
{

const GLuint kSkinTextureBase = 0x53505200u;	// + 64 per model + skin index: far above the engine's own numbers
const int kSkinsPerModel = 64;

// framing: the player (feet at z = -36, the hull's floor) standing near the bottom
const double kFovY = 32.0 * 3.14159265358979 / 180.0;
const double kVisibleHeight = 72.0 / 0.86;		// world units from the bottom to the top of the rectangle
const double kFeetAt = 0.93;					// feet at this fraction of the height, from the top
const double kDistance = kVisibleHeight * 0.5 / tan( kFovY * 0.5 );
const double kCentreZ = -36.0 + kVisibleHeight * ( kFeetAt - 0.5 );

// the GL textures of each model drawn so far
struct ModelSkins
{
	const CStudioPreviewModel* model;
	GLuint base;
	std::vector<bool> uploaded;	// per skin: its texture holds the picture
};
std::vector<ModelSkins> g_skins;

ModelSkins& SkinsOf( const CStudioPreviewModel& model )
{
	for ( ModelSkins& skins : g_skins )
	{
		if ( skins.model == &model )
			return skins;
	}
	ModelSkins skins;
	skins.model = &model;
	skins.base = kSkinTextureBase + (GLuint)( g_skins.size() * kSkinsPerModel );
	g_skins.push_back( skins );
	return g_skins.back();
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

void DrainGLErrors()
{
	for ( int i = 0; i < 32 && glGetError() != GL_NO_ERROR; i++ )
		;
}

// the bound texture: the skin and its mipmaps (2 x 2 box filter), so it stays smooth while the model turns
void UploadSkin( const CStudioPreviewModel::Texture& skin )
{
	glPixelStorei( GL_UNPACK_ALIGNMENT, 4 );
	glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );
	glPixelStorei( GL_UNPACK_SKIP_ROWS, 0 );
	glPixelStorei( GL_UNPACK_SKIP_PIXELS, 0 );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
	std::vector<unsigned char> level = skin.rgba, next;
	int w = skin.width, h = skin.height;
	for ( int mip = 0; mip < 16; mip++ )
	{
		glTexImage2D( GL_TEXTURE_2D, mip, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, level.data() );
		if ( w == 1 && h == 1 )
			break;
		const int nw = w > 1 ? w / 2 : 1, nh = h > 1 ? h / 2 : 1;
		next.assign( (size_t)nw * nh * 4, 0 );
		for ( int y = 0; y < nh; y++ )
		{
			for ( int x = 0; x < nw; x++ )
			{
				const int x0 = x * 2 < w ? x * 2 : w - 1, x1 = x * 2 + 1 < w ? x * 2 + 1 : w - 1;
				const int y0 = y * 2 < h ? y * 2 : h - 1, y1 = y * 2 + 1 < h ? y * 2 + 1 : h - 1;
				for ( int c = 0; c < 4; c++ )
				{
					const int sum = level[( (size_t)y0 * w + x0 ) * 4 + c] + level[( (size_t)y0 * w + x1 ) * 4 + c]
						+ level[( (size_t)y1 * w + x0 ) * 4 + c] + level[( (size_t)y1 * w + x1 ) * 4 + c];
					next[( (size_t)y * nw + x ) * 4 + c] = (unsigned char)( ( sum + 2 ) / 4 );
				}
			}
		}
		level.swap( next );
		w = nw;
		h = nh;
	}
}

// the engine's studio lighting, simplified: ambient plus a softened lambert from a key light fixed to the camera
float Shade( const float* normal, const float light[3] )
{
	float lambert = normal[0] * light[0] + normal[1] * light[1] + normal[2] * light[2];
	lambert = ( lambert + 0.5f ) / 1.5f;
	const float shade = 0.55f + 0.6f * ( lambert > 0.0f ? lambert : 0.0f );
	return shade < 1.0f ? shade : 1.0f;
}

struct DrawContext
{
	bool solid;
	float color[3];
	float light[3];			// the key light in model space
	float cosYaw, sinYaw;
	bool canUpload;			// no buffer feeds texture uploads right now
};

void DrawModelMeshes( const CStudioPreviewModel& model, const std::vector<float>& positions, const std::vector<float>& normals,
	const DrawContext& draw )
{
	const std::vector<CStudioPreviewModel::Mesh>& meshes = model.Meshes();
	if ( draw.solid )
	{
		glDisable( GL_TEXTURE_2D );
		glColor4f( draw.color[0], draw.color[1], draw.color[2], 1.0f );
		glBegin( GL_TRIANGLES );
		for ( const CStudioPreviewModel::Mesh& mesh : meshes )
		{
			for ( const CStudioPreviewModel::Corner& corner : mesh.corners )
				glVertex3fv( &positions[(size_t)corner.vert * 3] );
		}
		glEnd();
		return;
	}
	const std::vector<CStudioPreviewModel::Texture>& textures = model.Textures();
	ModelSkins& skins = SkinsOf( model );
	if ( skins.uploaded.size() < textures.size() )
		skins.uploaded.resize( textures.size(), false );
	for ( const CStudioPreviewModel::Mesh& mesh : meshes )
	{
		const CStudioPreviewModel::Texture* skin =
			mesh.texture >= 0 && mesh.texture < (int)textures.size() && mesh.texture < kSkinsPerModel ? &textures[mesh.texture] : NULL;
		bool textured = false;
		if ( skin )
		{
			const GLuint name = skins.base + (GLuint)mesh.texture;
			if ( skins.uploaded[mesh.texture] && glIsTexture( name ) == GL_TRUE )
			{
				glBindTexture( GL_TEXTURE_2D, name );
				textured = true;
			}
			else if ( draw.canUpload )	// first time, or the GL context was replaced
			{
				glBindTexture( GL_TEXTURE_2D, name );
				UploadSkin( *skin );
				skins.uploaded[mesh.texture] = true;
				textured = true;
			}
		}
		if ( textured )
			glEnable( GL_TEXTURE_2D );
		else
			glDisable( GL_TEXTURE_2D );
		const float invWide = skin ? 1.0f / skin->width : 0.0f, invTall = skin ? 1.0f / skin->height : 0.0f;
		glBegin( GL_TRIANGLES );
		for ( const CStudioPreviewModel::Corner& corner : mesh.corners )
		{
			const float* normal = &normals[(size_t)corner.norm * 3];
			const float shade = Shade( normal, draw.light );
			glColor4f( shade, shade, shade, 1.0f );
			if ( mesh.chrome )
			{
				// chrome: the texture position follows the normal as seen from the camera
				const float eyeRight = draw.sinYaw * normal[0] + draw.cosYaw * normal[1];	// eye X (screen right)
				const float eyeUp = normal[2];											// eye Y (screen up)
				glTexCoord2f( eyeRight * 0.5f + 0.5f, 0.5f - eyeUp * 0.5f );
			}
			else
				glTexCoord2f( corner.s * invWide, corner.t * invTall );
			glVertex3fv( &positions[(size_t)corner.vert * 3] );
		}
		glEnd();
	}
}

std::vector<float> g_playerPositions, g_playerNormals, g_weaponPositions, g_weaponNormals;
CStudioPreviewModel::BoneMatrix g_playerBones[CStudioPreviewModel::MAX_BONES];
CStudioPreviewModel::BoneMatrix g_weaponBones[CStudioPreviewModel::MAX_BONES];
CStudioPreviewModel::BoneMatrix g_grownBones[CStudioPreviewModel::MAX_BONES];

// the player skinned on its bones (g_playerBones) - grown towards the hit boxes when it has a plan (the beta's
// players, as the game draws them; the weapon hangs from the bones as they are, as in game)
void SkinPlayer( const CStudioPreviewModel& player )
{
	const PlayerFit* fit = player.Fit();
	if ( !fit )
	{
		player.Skin( g_playerBones, g_playerPositions, g_playerNormals );
		return;
	}
	memcpy( g_grownBones, g_playerBones, sizeof( g_grownBones ) );
	PlayerFit_Grow( fit, kBetaPlayerGrowth, g_grownBones );
	player.Skin( g_grownBones, g_playerPositions, g_playerNormals, g_playerBones );
}

} // namespace

float StudioPreview_CentreShift( const CStudioPreviewModel& player, const CStudioPreviewModel* weapon, float yaw, float aspect )
{
	if ( !player.IsLoaded() || aspect <= 0.0f )
		return 0.0f;
	player.ComputeBones( 0.0, g_playerBones );
	SkinPlayer( player );
	const bool armed = weapon && weapon->IsLoaded();
	if ( armed )
	{
		weapon->MergeBones( player, g_playerBones, g_weaponBones );
		weapon->Skin( g_weaponBones, g_weaponPositions, g_weaponNormals );
	}
	// across the rectangle (0 left .. 1 right) as StudioPreview_Draw projects it
	const double yawRad = yaw * 3.14159265358979 / 180.0, c = cos( yawRad ), s = sin( yawRad );
	const double halfWide = tan( kFovY * 0.5 ) * aspect;
	// across the rectangle: the player's body alone, and the body with the weapon
	double bodyLeft = 1e9, bodyRight = -1e9, allLeft = 1e9, allRight = -1e9;
	for ( int pass = 0; pass < ( armed ? 2 : 1 ); pass++ )
	{
		const std::vector<float>& positions = pass ? g_weaponPositions : g_playerPositions;
		for ( size_t i = 0; i + 2 < positions.size(); i += 3 )
		{
			const double front = positions[i] * c - positions[i + 1] * s, side = positions[i] * s + positions[i + 1] * c;
			const double depth = kDistance - front;
			if ( depth < 1.0 )
				continue;
			const double across = 0.5 + 0.5 * side / ( depth * halfWide );
			allLeft = across < allLeft ? across : allLeft;
			allRight = across > allRight ? across : allRight;
			if ( pass == 0 )
			{
				bodyLeft = across < bodyLeft ? across : bodyLeft;
				bodyRight = across > bodyRight ? across : bodyRight;
			}
		}
	}
	if ( bodyLeft > bodyRight )
		return 0.0f;
	// the body in the middle (centring body and weapon together pushed rifle carriers off to one side), but body and
	// weapon kept inside the rectangle with a small margin where they fit
	const double margin = 0.04;
	double move = 0.5 - ( bodyLeft + bodyRight ) * 0.5;
	if ( allRight + move > 1.0 - margin )
		move = 1.0 - margin - allRight;
	if ( allLeft + move < margin )
		move = margin - allLeft;
	// measured at the model's own distance
	double shift = move * 2.0 * kDistance * halfWide;
	shift = shift < -24.0 ? -24.0 : shift > 24.0 ? 24.0 : shift;
	return (float)shift;
}

void StudioPreview_Draw( const CStudioPreviewModel& player, const CStudioPreviewModel* weapon, double time, int x, int y,
	int wide, int tall, int screenWide, int screenTall, float yaw, float shift, bool solid, const float color[3],
	const float* headColor, bool viewModel, bool mirror, bool leftHanded )
{
	if ( !player.IsLoaded() || wide <= 0 || tall <= 0 || screenWide <= 0 || screenTall <= 0 )
		return;
	if ( viewModel )
	{
		weapon = NULL;	// a view model carries nothing and is not turned
		yaw = 0.0f;
		shift = 0.0f;
	}
	player.ComputeBones( time, g_playerBones );
	SkinPlayer( player );
	const bool armed = weapon && weapon->IsLoaded();
	if ( armed )
	{
		weapon->MergeBones( player, g_playerBones, g_weaponBones );
		weapon->Skin( g_weaponBones, g_weaponPositions, g_weaponNormals );
	}

	// the rectangle in window pixels (GL's origin is bottom left)
	GLint viewport[4] = { 0, 0, screenWide, screenTall };
	glGetIntegerv( GL_VIEWPORT, viewport );
	const double kx = (double)viewport[2] / screenWide, ky = (double)viewport[3] / screenTall;
	const int gx = viewport[0] + (int)floor( x * kx + 0.5 );
	const int gw = (int)floor( wide * kx + 0.5 ), gh = (int)floor( tall * ky + 0.5 );
	const int gy = viewport[1] + viewport[3] - (int)floor( y * ky + 0.5 ) - gh;
	if ( gw <= 0 || gh <= 0 )
		return;

	static UseProgramFn useProgram = (UseProgramFn)GetGLFunction( "glUseProgram" );
	static ActiveTextureFn activeTexture = (ActiveTextureFn)GetGLFunction( "glActiveTexture" );
	GLint program = 0, activeUnit = GL_TEXTURE0, unpackBuffer = 0;
	glGetIntegerv( GL_CURRENT_PROGRAM, &program );
	glGetIntegerv( GL_ACTIVE_TEXTURE, &activeUnit );
	glGetIntegerv( GL_PIXEL_UNPACK_BUFFER_BINDING, &unpackBuffer );
	DrainGLErrors();	// the queries above are errors on old drivers that lack those things

	GLfloat savedProjection[16], savedModelview[16];
	glGetFloatv( GL_PROJECTION_MATRIX, savedProjection );
	glGetFloatv( GL_MODELVIEW_MATRIX, savedModelview );
	glPushAttrib( GL_ALL_ATTRIB_BITS );
	glPushClientAttrib( GL_CLIENT_ALL_ATTRIB_BITS );
	if ( program && useProgram )
		useProgram( 0 );
	if ( activeUnit != GL_TEXTURE0 && activeTexture )
		activeTexture( GL_TEXTURE0 );

	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();
	if ( viewModel )
	{
		// kViewModelFov degrees across a 4:3 screen (a little narrower than the game's 90, so the weapon is a little
		// bigger); the rectangle shows its full height and as much of its width as fits, kept to the edge the weapon
		// is on (the left one for a left-handed weapon)
		const double kViewModelFov = 80.0 * 3.14159265358979 / 180.0;
		const double zNear = 1.0, zFar = 512.0, edge = zNear * tan( kViewModelFov * 0.5 ), top = 0.75 * edge;
		const double shown = 2.0 * top * wide / tall;
		if ( leftHanded )
			glFrustum( -edge, -edge + shown, -top, top, zNear, zFar );
		else
			glFrustum( edge - shown, edge, -top, top, zNear, zFar );
	}
	else
	{
		const double zNear = 8.0, zFar = 1024.0, top = zNear * tan( kFovY * 0.5 ), right = top * wide / tall;
		glFrustum( -right, right, -top, top, zNear, zFar );
	}
	glMatrixMode( GL_MODELVIEW );
	if ( viewModel )
	{
		// the eye at the model's origin looking along its +X axis, +Y to the left, Z up
		const GLfloat eye[16] = {
			0.0f, 0.0f, -1.0f, 0.0f,
			-1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f,
		};
		glLoadMatrixf( eye );
		if ( mirror )
			glScalef( 1.0f, -1.0f, 1.0f );	// left and right swapped, as the game mirrors view models (cl_righthand)
	}
	else
	{
		// eye = camera * turn * model: the camera out on the model's +X axis (its front) looking back at it, Z up, the
		// model moved 'shift' to the right
		const GLfloat camera[16] = {
			0.0f, 0.0f, 1.0f, 0.0f,
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			shift, (GLfloat)-kCentreZ, (GLfloat)-kDistance, 1.0f,
		};
		glLoadMatrixf( camera );
		glRotatef( yaw, 0.0f, 0.0f, 1.0f );
	}

	glViewport( gx, gy, gw, gh );
	glEnable( GL_SCISSOR_TEST );
	glScissor( gx, gy, gw, gh );
	glDepthMask( GL_TRUE );
	glClearDepth( 1.0 );
	glClear( GL_DEPTH_BUFFER_BIT );
	glEnable( GL_DEPTH_TEST );
	glDepthFunc( GL_LEQUAL );
	glDepthRange( 0.0, 1.0 );
	glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	glDisable( GL_CULL_FACE );
	glDisable( GL_BLEND );
	glDisable( GL_FOG );
	glDisable( GL_LIGHTING );
	glDisable( GL_STENCIL_TEST );
	glDisable( GL_TEXTURE_GEN_S );
	glDisable( GL_TEXTURE_GEN_T );
	glShadeModel( GL_SMOOTH );
	glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	glEnable( GL_ALPHA_TEST );
	glAlphaFunc( GL_GREATER, 0.5f );	// masked skins
	glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	DrawContext draw;
	draw.solid = solid;
	for ( int i = 0; i < 3; i++ )
		draw.color[i] = color[i];
	// the key light from the front, a little to the left and above, fixed to the camera: in model space turned back by
	// the model's turn
	const float yawRad = yaw * 3.14159265f / 180.0f;
	draw.cosYaw = cosf( yawRad );
	draw.sinYaw = sinf( yawRad );
	float key[3] = { 0.6f, -0.35f, 0.72f };
	const float keyLength = sqrtf( key[0] * key[0] + key[1] * key[1] + key[2] * key[2] );
	for ( int i = 0; i < 3; i++ )
		key[i] /= keyLength;
	draw.light[0] = draw.cosYaw * key[0] + draw.sinYaw * key[1];
	draw.light[1] = -draw.sinYaw * key[0] + draw.cosYaw * key[1];
	draw.light[2] = key[2];
	if ( viewModel )
	{
		// first person: the same key light (from the eye, a little to the left and above), the eye looking along +X:
		// towards the eye is -X, screen right is -Y (the chrome mapping's "right")
		draw.light[0] = -key[0];
		draw.light[1] = -key[1];
		draw.cosYaw = -1.0f;
		draw.sinYaw = 0.0f;
		if ( mirror )
		{
			// the model's left and right are swapped on screen: so are the light's and the chrome's
			draw.light[1] = -draw.light[1];
			draw.cosYaw = 1.0f;
		}
	}
	draw.canUpload = !unpackBuffer;

	DrawModelMeshes( player, g_playerPositions, g_playerNormals, draw );
	if ( headColor )
	{
		// the head colour: the triangles whose three corners are on the head, again over the head, flat, pulled a hair
		// towards the eye (as vismods.cpp draws it in game)
		glDisable( GL_TEXTURE_2D );
		glDisable( GL_ALPHA_TEST );
		glEnable( GL_POLYGON_OFFSET_FILL );
		glPolygonOffset( -1.0f, -2.0f );
		glColor4f( headColor[0], headColor[1], headColor[2], 1.0f );
		glBegin( GL_TRIANGLES );
		for ( const CStudioPreviewModel::Mesh& mesh : player.Meshes() )
		{
			for ( size_t i = 0; i + 2 < mesh.corners.size(); i += 3 )
			{
				const CStudioPreviewModel::Corner* c = &mesh.corners[i];
				if ( !player.OnHead( c[0].vert ) || !player.OnHead( c[1].vert ) || !player.OnHead( c[2].vert ) )
					continue;
				for ( int k = 0; k < 3; k++ )
					glVertex3fv( &g_playerPositions[(size_t)c[k].vert * 3] );
			}
		}
		glEnd();
		glDisable( GL_POLYGON_OFFSET_FILL );
		glEnable( GL_ALPHA_TEST );
	}
	if ( armed )
		DrawModelMeshes( *weapon, g_weaponPositions, g_weaponNormals, draw );

	glMatrixMode( GL_PROJECTION );
	glLoadMatrixf( savedProjection );
	glMatrixMode( GL_MODELVIEW );
	glLoadMatrixf( savedModelview );
	glPopClientAttrib();
	glPopAttrib();	// also the matrix mode, viewport, scissor, depth, texture binding and the rest
	if ( activeUnit != GL_TEXTURE0 && activeTexture )
		activeTexture( (GLenum)activeUnit );
	if ( program && useProgram )
		useProgram( (GLuint)program );
	DrainGLErrors();
}
