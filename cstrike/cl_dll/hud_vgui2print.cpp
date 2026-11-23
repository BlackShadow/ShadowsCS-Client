#include "hud.h"
#include "cl_util.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "../../engine/controls.h"
#include "../../vgui2/controls/controls.h"
#include "fontstyle.h"
#include <wchar.h>
#include <wctype.h>

// Draws a run of letters in one colour. It makes the same calls per letter as the engine's VGUI2DrawCharacter (font,
// position, colour, letter, printable letters only), but leaves the letters in the engine's text batch and sends
// the batch once at the end, instead of one separate draw per letter. The pixels are the same; the batch is sent
// with the same colour and blending the per-letter draws used. Returns the x after the last letter.
static int DrawLetterRun( vgui2::HFont font, const wchar_t* text, int length, int x, int y, int red, int green,
	int blue )
{
	vgui2::ISurface* surface = vgui2::surface();
	surface->DrawSetTextFont( font );
	surface->DrawSetTextColor( Color( red, green, blue, 255 ) );
	for ( int i = 0; i < length; i++ )
	{
		if ( iswprint( (wint_t)text[i] ) )
		{
			surface->DrawSetTextPos( x, y );
			surface->DrawUnicodeChar( text[i] );
		}
		int a, b, c;
		surface->GetCharABCwide( font, text[i], a, b, c );
		x += a + b + c;
	}
	surface->DrawFlushText();
	return x;
}

// the engine's VGUI2DrawCharacter colour: pfnDrawSetTextColor turns 0..1 into (int)( value * 255 )
static int TextColorByte( float value )
{
	return (int)( value * 255.0f );
}

int CHudVGUI2Print::Init()
{
	gHUD.AddHudElem( this );
	m_iFlags |= HUD_ACTIVE;
	m_hudfont = 0;
	return 1;
}

int CHudVGUI2Print::VidInit()
{
	m_flVGUI2StringTime = 0;
	vgui2::IScheme* scheme = vgui2::scheme()->GetIScheme( vgui2::scheme()->GetDefaultScheme() );
	m_hudfont = scheme->GetFont( "EngineFont", true );
	return 1;
}

int CHudVGUI2Print::Draw( float )
{
	if ( m_flVGUI2StringTime > gEngfuncs.GetClientTime() && m_hudfont )
		DrawVGUI2String( m_wCharBuf, m_iX, m_iY, m_fR, m_fG, m_fB );
	return 1;
}

int CHudVGUI2Print::DrawVGUI2String( char* text, int x, int y, float r, float g, float b )
{
	wchar_t* localized = vgui2::localize()->Find( text );
	wchar_t buffer[512];
	if ( !localized )
	{
		vgui2::localize()->ConvertANSIToUnicode( text, buffer, sizeof( buffer ) );
		localized = buffer;
	}
	return DrawVGUI2String( localized, x, y, r, g, b );
}

int CHudVGUI2Print::DrawVGUI2String( wchar_t* text, int x, int y, float r, float g, float b )
{
	if ( !m_hudfont )
		return 0;
	int lines = 1;
	for ( wchar_t* scan = text; *scan; scan++ )
	{
		if ( *scan == '\r' )
			lines++;
	}
	if ( y == -1 )
		y = ( ScreenHeight - vgui2::surface()->GetFontTall( m_hudfont ) ) / 2;
	int position = x;
	for ( int i = 0; i < lines; i++ )
	{
		wchar_t line[1024];
		int length = 0;
		while ( *text && *text != '\r' )
		{
			if ( *text != '\n' )
				line[length++] = *text;
			text++;
		}
		text++;
		line[length] = 0;
		int width, height;
		GetStringSize( line, &width, &height );
		position = x;
		if ( x == -1 )
		{
			float centered = ScreenWidth * 0.5 - width * 0.5;
			position = (int)centered;
		}
		// black shadow pass, then the coloured pass on top, each as one batch
		DrawLetterRun( m_hudfont, line, length, position, y, 0, 0, 0 );
		position = DrawLetterRun( m_hudfont, line, length, position, y, TextColorByte( r ), TextColorByte( g ),
			TextColorByte( b ) );
		gEngfuncs.pfnDrawSetTextColor( r, g, b );	// leave the engine's text colour where stock left it
		y += height;
	}
	return position;
}

int CHudVGUI2Print::DrawVGUI2StringReverse( char* text, int x, int y, float r, float g, float b )
{
	wchar_t* localized = vgui2::localize()->Find( text );
	wchar_t buffer[512];
	if ( !localized )
	{
		vgui2::localize()->ConvertANSIToUnicode( text, buffer, sizeof( buffer ) );
		localized = buffer;
	}
	return DrawVGUI2StringReverse( localized, x, y, r, g, b );
}

int CHudVGUI2Print::DrawVGUI2StringReverse( wchar_t* text, int x, int y, float r, float g, float b )
{
	if ( !m_hudfont )
		return 0;
	if ( y == -1 )
		y = ( ScreenHeight - vgui2::surface()->GetFontTall( m_hudfont ) ) / 2;
	gEngfuncs.pfnDrawSetTextColor( r, g, b );
	// the text ends at x: find where it starts, then draw it left to right as one batch (same letter positions)
	const int length = (int)wcslen( text );
	for ( int i = length - 1; i >= 0; i-- )
	{
		int a, width, c;
		vgui2::surface()->GetCharABCwide( m_hudfont, text[i], a, width, c );
		x -= a + width + c;
	}
	DrawLetterRun( m_hudfont, text, length, x, y, TextColorByte( r ), TextColorByte( g ), TextColorByte( b ) );
	return x;
}

void CHudVGUI2Print::VGUI2HudPrint( char* text, int x, int y, float r, float g, float b )
{
	wchar_t* localized = vgui2::localize()->Find( text );
	if ( localized )
	{
		wcsncpy( m_wCharBuf, localized, ARRAYSIZE( m_wCharBuf ) - 1 );
		m_wCharBuf[ARRAYSIZE( m_wCharBuf ) - 1] = 0;
		m_iX = x;
		m_iY = y;
		m_fR = r;
		m_fG = g;
		m_fB = b;
		m_flVGUI2StringTime = gEngfuncs.GetClientTime() + 4;
	}
}

void CHudVGUI2Print::VGUI2LocalizeArg( char* text, wchar_t* buffer, int size )
{
	if ( !text || !buffer )
		return;
	if ( *text == '#' )
	{
		wchar_t* localized = vgui2::localize()->Find( text );
		if ( localized )
		{
			int length = size / sizeof( wchar_t ) - 1;
			wcsncpy( buffer, localized, length );
			buffer[length] = 0;
			return;
		}
	}
	vgui2::localize()->ConvertANSIToUnicode( text, buffer, size );
}

void CHudVGUI2Print::VGUI2HudPrintArgs( char* text, char* arg1, char* arg2, char* arg3, char* arg4,
	int x, int y, float r, float g, float b )
{
	wchar_t* localized = vgui2::localize()->Find( text );
	wchar_t first[64], second[64], third[64], fourth[64];
	VGUI2LocalizeArg( arg1, first, sizeof( first ) );
	VGUI2LocalizeArg( arg2, second, sizeof( second ) );
	VGUI2LocalizeArg( arg3, third, sizeof( third ) );
	VGUI2LocalizeArg( arg4, fourth, sizeof( fourth ) );
	m_iX = x;
	m_iY = y;
	m_fR = r;
	m_fG = g;
	m_fB = b;
	float centerTime = gEngfuncs.pfnGetCvarFloat( "scr_centertime" );
	if ( centerTime < 0 )
		centerTime = 2;
	m_flVGUI2StringTime = gEngfuncs.GetClientTime() + centerTime;
	if ( localized )
	{
		vgui2::localize()->ConstructString( m_wCharBuf, sizeof( m_wCharBuf ), localized, 4, first, second, third, fourth );
		return;
	}
#ifdef _WIN32
	wchar_t format[1024];
#else
	wchar_t format[2048];
#endif
	vgui2::localize()->ConvertANSIToUnicode( text, format, sizeof( format ) );
#ifndef _WIN32
	for ( wchar_t* scan = format; *scan; scan++ )
	{
		if ( *scan == '%' )
		{
			if ( towlower( scan[1] ) == 's' )
			{
				scan[1] = 'S';
				scan++;
			}
			else if ( scan[1] == '.' && scan[2] >= '0' && scan[2] <= '9' )
			{
				if ( towlower( scan[3] == 's' ) )
					scan[3] = 'S';
				else if ( scan[3] >= '0' && scan[3] <= '9' && towlower( scan[4] ) == 's' )
					scan[4] = 'S';
			}
		}
	}
	swprintf( m_wCharBuf, ARRAYSIZE( m_wCharBuf ), format, first, second, third, fourth );
#else
	_snwprintf( m_wCharBuf, ARRAYSIZE( m_wCharBuf ), format, first, second, third, fourth );
#endif
}

// The WON lettering is light one-pixel strokes inside a one-pixel black outline; its fonts hold the strokes
// (tools/fonts/won_fonts.py), and the outline is the letters in black at the eight one-pixel offsets, under them.
int CHudVGUI2Print::DrawChatString( wchar_t* text, int x, int y, float r, float g, float b )
{
	const vgui2::HFont won = FontStyle_ChatFont();
	if ( !won )
		return DrawVGUI2String( text, x, y, r, g, b );
	const int length = (int)wcslen( text );
	for ( int dy = -1; dy <= 1; dy++ )
	{
		for ( int dx = -1; dx <= 1; dx++ )
		{
			if ( dx || dy )
				DrawLetterRun( won, text, length, x + dx, y + dy, 0, 0, 0 );
		}
	}
	x = DrawLetterRun( won, text, length, x, y, TextColorByte( r ), TextColorByte( g ), TextColorByte( b ) );
	gEngfuncs.pfnDrawSetTextColor( r, g, b );	// leave the engine's text colour where stock left it
	return x;
}

void CHudVGUI2Print::GetChatStringSize( const wchar_t* text, int* width, int* height )
{
	const vgui2::HFont won = FontStyle_ChatFont();
	if ( !won )
	{
		GetStringSize( text, width, height );
		return;
	}
	if ( width )
	{
		*width = 0;
		for ( const wchar_t* letter = text; *letter; letter++ )
		{
			int a, b, c;
			vgui2::surface()->GetCharABCwide( won, *letter, a, b, c );
			*width += a + b + c;
		}
	}
	if ( height )
		*height = vgui2::surface()->GetFontTall( won );
}

int CHudVGUI2Print::GetHudFontHeight()
{
	return m_hudfont ? vgui2::surface()->GetFontTall( m_hudfont ) : 0;
}

void CHudVGUI2Print::GetStringSize( const wchar_t* text, int* width, int* height )
{
	if ( width )
		*width = 0;
	if ( height )
		*height = 0;
	if ( !m_hudfont )
		return;
	int length = wcslen( text );
	if ( width )
	{
		for ( int i = 0; i < length; i++ )
		{
			int a, b, c;
			vgui2::surface()->GetCharABCwide( m_hudfont, text[i], a, b, c );
			*width += a + b + c;
		}
	}
	if ( height )
		*height = vgui2::surface()->GetFontTall( m_hudfont );
}
