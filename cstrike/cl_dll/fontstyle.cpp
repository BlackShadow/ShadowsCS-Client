// fontstyle.cpp - see fontstyle.h. The menus take the scheme in CounterStrikeViewport::UpdateScheme, which runs again
// as soon as the style changes (CounterStrikeViewport::OnTick); the chat asks for its font every time it draws
// (CHudVGUI2Print::DrawChatString).
#include "hud.h"
#include "cl_util.h"
#include "vgui/IScheme.h"
#include "../../engine/controls.h"
#include "../../vgui2/controls/controls.h"
#include "fontstyle.h"

static cvar_t* cl_fontstyle;
static vgui2::HScheme g_hWONScheme;

void FontStyle_Init()
{
	cl_fontstyle = CVAR_CREATE( "cl_fontstyle", "original", FCVAR_ARCHIVE );
}

void FontStyle_LoadScheme()
{
	g_hWONScheme = vgui2::scheme()->LoadSchemeFromFile( "resource/ClientSchemeWON.res", "ClientSchemeWON" );
	if ( g_hWONScheme == vgui2::scheme()->GetDefaultScheme() )
		g_hWONScheme = 0;	// the file did not read
}

bool FontStyle_IsWON()
{
	return g_hWONScheme && cl_fontstyle && cl_fontstyle->string && !stricmp( cl_fontstyle->string, "won" );
}

const char* FontStyle_MenuScheme()
{
	return FontStyle_IsWON() ? "ClientSchemeWON" : "ClientScheme";
}

unsigned long FontStyle_ChatFont()
{
	if ( !FontStyle_IsWON() )
		return 0;
	vgui2::IScheme* scheme = vgui2::scheme()->GetIScheme( g_hWONScheme );
	return scheme ? scheme->GetFont( "WONChat", false ) : 0;
}
