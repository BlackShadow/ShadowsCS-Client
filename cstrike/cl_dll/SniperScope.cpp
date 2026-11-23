#include "hud.h"
#include "cl_util.h"
#include "pm_shared.h"
#include "r_studioint.h"
#include "vgui_controls/Controls.h"
#include "vgui/VGuiVertex.h"
#include "vgui/ISurface.h"
#include "vgui/IScheme.h"
#include "ammohistory.h"
#include "triangleapi.h"
#include "const.h"
#include "weapontype.h"

extern engine_studio_api_t IEngineStudio;

#define SCOPE_UV_MIN 0.001953125f
#define SCOPE_UV_MAX 0.998046875f

CHudSniperScope::~CHudSniperScope()
{
}

int CHudSniperScope::Init()
{
	m_iFlags |= HUD_ACTIVE;
	gHUD.AddHudElem(this);
	return 1;
}

void CHudSniperScope::Reset()
{
}

int CHudSniperScope::VidInit()
{
	m_hReticle = SPR_Load( "sprites/sniper_scope.spr" );
	memset( &m_rcReticle, 0, sizeof( m_rcReticle ) );
	if ( m_hReticle )
	{
		m_rcReticle.right = SPR_Width( m_hReticle, 0 );
		m_rcReticle.bottom = SPR_Height( m_hReticle, 0 );
	}
	if (IEngineStudio.IsHardware())
	{
		m_iScopeTextureID = vgui2::surface()->CreateNewTextureID(false);
		vgui2::surface()->DrawSetTextureFile(m_iScopeTextureID, "sprites/scope_arc", 1, false);
	}
	else
	{
		m_iScopeNWTextureID = vgui2::surface()->CreateNewTextureID(false);
		m_iScopeNETextureID = vgui2::surface()->CreateNewTextureID(false);
		m_iScopeSWTextureID = vgui2::surface()->CreateNewTextureID(false);
		m_iScopeSETextureID = vgui2::surface()->CreateNewTextureID(false);
		vgui2::surface()->DrawSetTextureFile(m_iScopeNWTextureID, "sprites/scope_arc_nw", 1, false);
		vgui2::surface()->DrawSetTextureFile(m_iScopeNETextureID, "sprites/scope_arc_ne", 1, false);
		vgui2::surface()->DrawSetTextureFile(m_iScopeSWTextureID, "sprites/scope_arc_sw", 1, false);
		vgui2::surface()->DrawSetTextureFile(m_iScopeSETextureID, "sprites/scope_arc", 1, false);
	}
	return 1;
}

// The scope reticle (mil-dots and the red centre dot): the stock sprites/sniper_scope.spr, the same for your own
// rifle and for a player you spectate, even when a server's custom weapon brings its own scope picture.
// Drawn here rather than by the engine, so it shows whether 'crosshair' is 1 or 0, and enlarged with the HUD.
static void DrawScopeReticle( HSPRITE stock, const wrect_t& stockRect, int centerX, int centerY )
{
	HSPRITE sprite = stock;
	wrect_t rc = stockRect;
	if ( !sprite )
	{
		WEAPON* weapon = gWR.GetWeapon( WEAPON_AWP );
		if ( !weapon || !weapon->hZoomedCrosshair )
			return;
		sprite = weapon->hZoomedCrosshair;
		rc = weapon->rcZoomedCrosshair;
	}
	const int rw = rc.right - rc.left, rh = rc.bottom - rc.top;
	if ( rw <= 0 || rh <= 0 )
		return;
	const bool hardware = IEngineStudio.IsHardware() != 0;
	// the centre of the reticle's middle texel (127,129 in the 256x256 sniper_scope)...
	const float anchorX = rw * 0.5f - 0.5f, anchorY = rh * 0.5f + 1.5f;
	// ...goes on the middle of the scope lines: vertical [centerX, centerX+1) (software [centerX-1, centerX)),
	// horizontal [centerY+1, centerY+2)
	const float lineX = hardware ? centerX + 0.5f : centerX - 0.5f, lineY = centerY + 1.5f;
	// the software renderer's TriAPI works in world space: there the reticle stays at its own size
	const int scale = hardware && gHUD.m_iHudScale > 1 ? gHUD.m_iHudScale : 1;
	struct model_s* model = (struct model_s*)gEngfuncs.GetSpritePointer( sprite );
	const int frameW = SPR_Width( sprite, 0 ), frameH = SPR_Height( sprite, 0 );
	if ( scale == 1 || !model || frameW <= 0 || frameH <= 0 || !gEngfuncs.pTriAPI )
	{
		SPR_Set( sprite, 255, 255, 255 );
		SPR_DrawHoles( 0, (int)floor( lineX - anchorX ), (int)floor( lineY - anchorY ), &rc );
		return;
	}
	// whole pixels, texel t of the rect at x + t * scale
	const float x = (float)floor( lineX - anchorX * scale );
	const float y = (float)floor( lineY - anchorY * scale );
	const float w = (float)( rw * scale ), h = (float)( rh * scale );
	// half a texel in from the edges on both the picture and the quad, so nothing bleeds in and nothing stretches
	const float in = 0.5f * scale;
	const float u0 = ( rc.left + 0.5f ) / frameW, u1 = ( rc.right - 0.5f ) / frameW;
	const float v0 = ( rc.top + 0.5f ) / frameH, v1 = ( rc.bottom - 0.5f ) / frameH;
	triangleapi_s* tri = gEngfuncs.pTriAPI;
	tri->SpriteTexture( model, 0 );
	tri->RenderMode( kRenderTransAlpha );
	tri->CullFace( TRI_NONE );
	tri->Color4ub( 255, 255, 255, 255 );
	tri->Begin( TRI_QUADS );
	tri->TexCoord2f( u0, v0 );
	tri->Vertex3f( x + in, y + in, 0.0f );
	tri->TexCoord2f( u1, v0 );
	tri->Vertex3f( x + w - in, y + in, 0.0f );
	tri->TexCoord2f( u1, v1 );
	tri->Vertex3f( x + w - in, y + h - in, 0.0f );
	tri->TexCoord2f( u0, v1 );
	tri->Vertex3f( x + in, y + h - in, 0.0f );
	tri->End();
	tri->RenderMode( kRenderNormal );
}

int CHudSniperScope::Draw(float)
{
	if ((g_iUser1 & ~OBS_IN_EYE) || gHUD.m_iFOV > 44)
		return 1;
	int bottom = ScreenHeight - ScreenHeight / 16;
	int left = (ScreenWidth - ScreenHeight) / 2 + ScreenHeight / 16;
	int top = ScreenHeight / 16;
	int right = ScreenWidth - left;
	int centerX = ScreenWidth / 2;
	int centerY = ScreenHeight / 2;
	if (IEngineStudio.IsHardware())
	{
		vgui2::surface()->DrawSetTexture(m_iScopeTextureID);
		vgui2::surface()->DrawSetColor(0, 0, 0, 255);
		vgui2::VGuiVertex vertices[4] =
		{
			{ centerX, centerY, SCOPE_UV_MIN, SCOPE_UV_MIN },
			{ right, centerY, SCOPE_UV_MAX, SCOPE_UV_MIN },
			{ right, bottom, SCOPE_UV_MAX, SCOPE_UV_MAX },
			{ centerX, bottom, SCOPE_UV_MIN, SCOPE_UV_MAX }
		};
		vgui2::surface()->DrawTexturedPolygon(vertices, 4);
		vertices[0] = { centerX - 1, top, SCOPE_UV_MIN, SCOPE_UV_MAX };
		vertices[1] = { right, top, SCOPE_UV_MAX, SCOPE_UV_MAX };
		vertices[2] = { right, centerY + 1, SCOPE_UV_MAX, SCOPE_UV_MIN };
		vertices[3] = { centerX - 1, centerY + 1, SCOPE_UV_MIN, SCOPE_UV_MIN };
		vgui2::surface()->DrawTexturedPolygon(vertices, 4);
		vertices[0] = { left, centerY, SCOPE_UV_MAX, SCOPE_UV_MIN };
		vertices[1] = { centerX, centerY, SCOPE_UV_MIN, SCOPE_UV_MIN };
		vertices[2] = { centerX, bottom, SCOPE_UV_MIN, SCOPE_UV_MAX };
		vertices[3] = { left, bottom, SCOPE_UV_MAX, SCOPE_UV_MAX };
		vgui2::surface()->DrawTexturedPolygon(vertices, 4);
		vertices[0] = { left, top, SCOPE_UV_MAX, SCOPE_UV_MAX };
		vertices[1] = { centerX, top, SCOPE_UV_MIN, SCOPE_UV_MAX };
		vertices[2] = { centerX, centerY, SCOPE_UV_MIN, SCOPE_UV_MIN };
		vertices[3] = { left, centerY, SCOPE_UV_MAX, SCOPE_UV_MIN };
		vgui2::surface()->DrawTexturedPolygon(vertices, 4);
	}
	else
	{
		vgui2::surface()->DrawSetColor(0, 0, 0, 255);
		vgui2::surface()->DrawSetTexture(m_iScopeNWTextureID);
		vgui2::surface()->DrawTexturedRect(left, top, centerX, centerY);
		vgui2::surface()->DrawSetTexture(m_iScopeNETextureID);
		vgui2::surface()->DrawTexturedRect(centerX, top, right, centerY);
		vgui2::surface()->DrawSetTexture(m_iScopeSWTextureID);
		vgui2::surface()->DrawTexturedRect(left, centerY, centerX, bottom);
		vgui2::surface()->DrawSetTexture(m_iScopeSETextureID);
		vgui2::surface()->DrawTexturedRect(centerX, centerY, right, bottom);
	}
	if (IEngineStudio.IsHardware() == 1)
	{
		int opening = vgui2::scheme()->GetProportionalScaledValue(10);
		vgui2::surface()->DrawFilledRect(centerX, 0, centerX + 1, centerY - opening);
		vgui2::surface()->DrawFilledRect(centerX, centerY + opening, centerX + 1, ScreenHeight);
		vgui2::surface()->DrawFilledRect(0, centerY + 1, centerX - opening, centerY + 2);
		vgui2::surface()->DrawFilledRect(centerX + opening, centerY + 1, ScreenWidth, centerY + 2);
	}
	else if (!IEngineStudio.IsHardware())
	{
		int opening = vgui2::scheme()->GetProportionalScaledValue(10);
		vgui2::surface()->DrawFilledRect(centerX - 1, 0, centerX, centerY - opening);
		vgui2::surface()->DrawFilledRect(centerX - 1, centerY + opening, centerX, ScreenHeight);
		vgui2::surface()->DrawFilledRect(0, centerY + 1, centerX - opening, centerY + 2);
		vgui2::surface()->DrawFilledRect(centerX + opening, centerY + 1, ScreenWidth, centerY + 2);
	}
	vgui2::surface()->DrawFilledRect(0, 0, ScreenWidth, top + 1);
	vgui2::surface()->DrawFilledRect(0, bottom - 1, ScreenWidth, ScreenHeight);
	vgui2::surface()->DrawFilledRect(0, top, left + 1, ScreenHeight);
	vgui2::surface()->DrawFilledRect(right - 1, top, ScreenWidth, ScreenHeight);
	DrawScopeReticle( m_hReticle, m_rcReticle, centerX, centerY );
	return 1;
}
