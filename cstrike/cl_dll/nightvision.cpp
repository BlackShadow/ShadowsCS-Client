#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "eventscripts.h"
#include "const.h"
#include "r_efx.h"

// Private light key: far above any entity number, which is what the engine uses as keys for entity lights.
#define NVG_LIGHT_KEY 0x4E564700

DECLARE_MESSAGE(m_NightVision, NVGToggle)
DECLARE_COMMAND(m_NightVision, NVGAdjustUp)
DECLARE_COMMAND(m_NightVision, NVGAdjustDown)

CHudNightVision::~CHudNightVision()
{
}

int CHudNightVision::Init()
{
	m_fOn = 0;
	m_iAlpha = 110;
	HOOK_MESSAGE(NVGToggle);
	HOOK_COMMAND("+nvgadjust", NVGAdjustUp);
	HOOK_COMMAND("-nvgadjust", NVGAdjustDown);
	m_iFlags |= HUD_ACTIVE;
	gHUD.AddHudElem(this);
	return 1;
}

int CHudNightVision::VidInit()
{
	return 1;
}

void CHudNightVision::Reset()
{
	m_fOn = 0;
	m_iAlpha = 110;
}

int CHudNightVision::MsgFunc_NVGToggle(const char *, int size, void *buffer)
{
	BEGIN_READ(buffer, size);
	m_fOn = READ_BYTE();
	return 1;
}

void CHudNightVision::UserCmd_NVGAdjustUp()
{
	m_iAlpha += 10;
	if (m_iAlpha > 220)
		m_iAlpha = 220;
}

void CHudNightVision::UserCmd_NVGAdjustDown()
{
	m_iAlpha -= 10;
	if (m_iAlpha < 30)
		m_iAlpha = 30;
}

int CHudNightVision::Draw(float)
{
	if (!m_fOn || gEngfuncs.IsSpectateOnly())
		return 1;
	FillRGBA(0, 0, ScreenWidth, ScreenHeight, 50, 225, 50, m_iAlpha);

	// Stock made a brand new light every frame (colour 1 20 1, radius 750..800, lasting 0.1 s), so about
	// frame rate / 10 big lights stood on the same spot: 10 at 100 fps, 30 at 300 fps, filling all 32 light
	// slots the game has. Every lit wall had to be relit once per light, every frame.
	// Now it is one light, moved every frame. The engine just adds light colours together, so ten 1 20 1 lights
	// of radius 750..800 add up to exactly one 10 200 10 light of radius 775 (the average); that is the look
	// stock had at 100 fps.
	dlight_t* light = gEngfuncs.pEfxAPI->CL_AllocDlight(NVG_LIGHT_KEY);
	if (light)
	{
		light->origin = gHUD.m_vecOrigin;
		light->radius = 775;
		light->color.r = 10;
		light->color.g = 200;
		light->color.b = 10;
		light->die = gHUD.m_flTime + 0.1f;
	}
	return 1;
}
