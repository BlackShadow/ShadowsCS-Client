// autorate_panel.cpp - see autorate_panel.h. A card a little darker than the page, in four rows:
//   - the connection: ping, jitter, loss, updates a second and frames a second, each with a bar under it in green,
//     amber or red (good, fair, poor);
//   - the ping of the last 30 seconds, a bar a second (a red mark on top where packets went missing), faint lines
//     every 50 ms;
//   - the settings now, gold where Auto-rate sets them;
//   - what Auto-rate is doing, after a square in the same colours (an empty one while it is off).
// Drawn with the surface in Tahoma (like the About page's name); the numbers come from AutoRate_Readout four times a
// second.
#include "hud.h"
#include "cl_util.h"
#include "vgui_controls/Controls.h"
#include "vgui/ISurface.h"
#include "vgui/IScheme.h"
#include "autorate_panel.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

namespace
{
// proportional units (480 lines high)
const int kPad = 8;
const int kGap = 7;
const int kCaptionTall = 9;
const int kValueTall = 16;
const int kUnitTall = 11;
const int kTextTall = 12;
const int kGraphTall = 30;
const int kBarTall = 2;

const Color kGood( 124, 196, 92, 255 );
const Color kFair( 214, 176, 64, 255 );
const Color kPoor( 214, 86, 62, 255 );

enum Quality
{
	GOOD,
	FAIR,
	POOR
};

Color QualityColor( Quality quality )
{
	return quality == GOOD ? kGood : quality == FAIR ? kFair : kPoor;
}

Quality PingQuality( int ms )
{
	return ms < 60 ? GOOD : ms < 120 ? FAIR : POOR;
}

Quality JitterQuality( int ms )
{
	return ms < 5 ? GOOD : ms < 15 ? FAIR : POOR;
}

Quality LossQuality( int percent )
{
	return percent == 0 ? GOOD : percent < 5 ? FAIR : POOR;
}

Quality UpdatesQuality( int perSecond )
{
	return perSecond >= 60 ? GOOD : perSecond >= 25 ? FAIR : POOR;	// 25: a server left at 30, on its frames
}

Quality FpsQuality( int fps )
{
	return fps >= 100 ? GOOD : fps >= 60 ? FAIR : POOR;
}

Color Faded( Color color, int alpha )
{
	return Color( color.r(), color.g(), color.b(), alpha );
}

int Larger( int a, int b )
{
	return a > b ? a : b;
}

vgui2::HFont MakeFont( int tall, int weight )
{
	const vgui2::HFont font = vgui2::surface()->CreateFont();
	vgui2::surface()->AddGlyphSetToFont( font, "Tahoma", tall, weight, 0, 0, vgui2::ISurface::FONTFLAG_ANTIALIAS, 0, 0 );
	return font;
}

int TextWide( vgui2::HFont font, const wchar_t* text )
{
	int wide = 0, tall = 0;
	vgui2::surface()->GetTextSize( font, text, wide, tall );
	return wide;
}

void Text( vgui2::HFont font, Color color, int x, int y, const wchar_t* text )
{
	vgui2::surface()->DrawSetTextFont( font );
	vgui2::surface()->DrawSetTextColor( color );
	vgui2::surface()->DrawSetTextPos( x, y );
	vgui2::surface()->DrawPrintText( text, (int)wcslen( text ) );
}

void Rect( Color color, int x0, int y0, int x1, int y1 )
{
	vgui2::surface()->DrawSetColor( color );
	vgui2::surface()->DrawFilledRect( x0, y0, x1, y1 );
}
} // namespace

CAutoRatePanel::CAutoRatePanel( vgui2::Panel* parent, const char* name ) : BaseClass( parent, name )
{
	SetPaintBackgroundEnabled( false );
	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );
	memset( &m_read, 0, sizeof( m_read ) );
	AutoRate_Readout( m_read );
}

void CAutoRatePanel::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	BaseClass::ApplySchemeSettings( scheme );
	m_Bright = GetSchemeColor( "BrightControlText", Color( 196, 181, 80, 255 ), scheme );
	m_Text = GetSchemeColor( "BaseText", Color( 216, 222, 211, 255 ), scheme );
	m_Dim = GetSchemeColor( "DimBaseText", Color( 160, 170, 149, 255 ), scheme );
	m_iFontScale = 0;	// the size may have changed (resolution): made again
}

void CAutoRatePanel::EnsureFonts()
{
	const int scale = P( 100 );
	if ( scale == m_iFontScale && m_hValue )
		return;
	m_iFontScale = scale;
	m_hCaption = MakeFont( P( kCaptionTall ), 700 );
	m_hValue = MakeFont( P( kValueTall ), 700 );
	m_hUnit = MakeFont( P( kUnitTall ), 400 );
	m_hText = MakeFont( P( kTextTall ), 400 );
}

int CAutoRatePanel::WantedTall()
{
	EnsureFonts();
	vgui2::ISurface* surface = vgui2::surface();
	const int caption = surface->GetFontTall( m_hCaption );
	const int value = surface->GetFontTall( m_hValue );
	const int text = surface->GetFontTall( m_hText );
	return P( kPad ) + caption + P( 1 ) + value + P( 2 ) + Larger( 2, P( kBarTall ) ) + P( kGap )	// the connection
		   + caption + P( 2 ) + P( kGraphTall ) + P( kGap )											// the graph
		   + caption + P( 1 ) + value + P( kGap )													// the settings
		   + text + P( kPad );																		// what Auto-rate does
}

void CAutoRatePanel::OnThink()
{
	BaseClass::OnThink();
	const double now = gEngfuncs.GetAbsoluteTime();
	if ( now >= m_flNextRead || now < m_flNextRead - 1.0 )
	{
		m_flNextRead = now + 0.25;
		AutoRate_Readout( m_read );
	}
}

void CAutoRatePanel::Paint()
{
	EnsureFonts();
	vgui2::ISurface* surface = vgui2::surface();
	int wide, tall;
	GetSize( wide, tall );
	const AutoRateReadout& r = m_read;
	const int pad = P( kPad ), gap = P( kGap );
	const int captionTall = surface->GetFontTall( m_hCaption );
	const int valueTall = surface->GetFontTall( m_hValue );
	const int unitTall = surface->GetFontTall( m_hUnit );
	const int textTall = surface->GetFontTall( m_hText );
	const int inner = wide - 2 * pad;
	const int cell = inner / 5;
	const Color caption = Faded( m_Dim, 220 );

	// the card: darker than the page, a line round it
	Rect( Color( 0, 0, 0, 46 ), 0, 0, wide, tall );
	surface->DrawSetColor( Color( 0, 0, 0, 90 ) );
	surface->DrawOutlinedRect( 0, 0, wide, tall );

	// the connection
	struct Stat
	{
		const wchar_t* caption;
		int value;
		const wchar_t* unit;
		Quality quality;
	};
	const Stat stats[5] = {
		{ L"PING", r.ping, L"ms", PingQuality( r.ping ) },
		{ L"JITTER", r.jitter, L"ms", JitterQuality( r.jitter ) },
		{ L"LOSS", r.loss, L"%", LossQuality( r.loss ) },
		{ L"UPDATES", r.updates, L"/s", UpdatesQuality( r.updates ) },
		{ L"FPS", r.fps, L"", FpsQuality( r.fps ) },
	};
	int y = pad;
	const int valueY = y + captionTall + P( 1 );
	const int barY = valueY + valueTall + P( 2 );
	const int barTall = Larger( 2, P( kBarTall ) );
	for ( int i = 0; i < 5; i++ )
	{
		const int x = pad + i * cell;
		Text( m_hCaption, caption, x, y, stats[i].caption );
		wchar_t number[16];
		if ( r.measured )
			swprintf( number, 16, L"%d", stats[i].value );
		else
			wcscpy( number, L"-" );
		Text( m_hValue, r.measured ? m_Text : m_Dim, x, valueY, number );
		if ( r.measured && stats[i].unit[0] )
			Text( m_hUnit, m_Dim, x + TextWide( m_hValue, number ) + P( 2 ), valueY + valueTall - unitTall - P( 1 ),
				stats[i].unit );
		Rect( r.measured ? Faded( QualityColor( stats[i].quality ), 210 ) : Color( 0, 0, 0, 60 ), x, barY,
			x + cell - P( 10 ), barY + barTall );
	}
	y = barY + barTall + gap;

	// the last 30 seconds' ping
	Text( m_hCaption, caption, pad, y, L"PING, LAST 30 SECONDS" );
	const int graphY = y + captionTall + P( 2 );
	const int graphTall = P( kGraphTall );
	Rect( Color( 0, 0, 0, 70 ), pad, graphY, pad + inner, graphY + graphTall );
	float top = 100.0f;	// the graph's top: 100 ms, or the next 50 over the highest ping
	for ( int i = 0; i < r.history; i++ )
	{
		if ( r.pingHistory[i] > top )
			top = ceilf( r.pingHistory[i] / 50.0f ) * 50.0f;
	}
	const int step = top <= 200.0f ? 50 : 100;
	for ( int ms = step; ms < (int)top; ms += step )
	{
		const int lineY = graphY + graphTall - (int)( graphTall * ms / top );
		Rect( Color( 255, 255, 255, 16 ), pad, lineY, pad + inner, lineY + 1 );
	}
	const float slot = inner / (float)AutoRateReadout::kHistory;
	const int mark = Larger( 2, P( 2 ) );
	for ( int i = 0; i < r.history; i++ )
	{
		const int place = AutoRateReadout::kHistory - r.history + i;	// the newest at the right
		const int x0 = pad + (int)( place * slot ) + 1;
		const int x1 = Larger( x0 + 1, pad + (int)( ( place + 1 ) * slot ) - 1 );
		const float ping = r.pingHistory[i];
		int barHigh = (int)( graphTall * ( ping < top ? ping : top ) / top );
		if ( barHigh < 1 && ping > 0.0f )
			barHigh = 1;
		Rect( Faded( QualityColor( PingQuality( (int)ping ) ), 150 ), x0, graphY + graphTall - barHigh, x1, graphY + graphTall );
		if ( r.lossHistory[i] > 0 )
			Rect( kPoor, x0, graphY, x1, graphY + mark );
	}
	wchar_t scale[16];
	swprintf( scale, 16, L"%d ms", (int)top );
	Text( m_hCaption, Faded( m_Dim, 170 ), pad + inner - P( 4 ) - TextWide( m_hCaption, scale ), graphY + P( 3 ), scale );
	if ( !r.measured )
	{
		const wchar_t* why = r.where == AutoRateReadout::NOT_CONNECTED ? L"Not connected to a server"
							 : r.where == AutoRateReadout::LOCAL_GAME	? L"A game on this computer: nothing to measure"
							 : r.where == AutoRateReadout::DEMO			? L"Playing a demo"
																		: L"Measuring...";
		Text( m_hText, m_Dim, pad + ( inner - TextWide( m_hText, why ) ) / 2, graphY + ( graphTall - textTall ) / 2, why );
	}
	y = graphY + graphTall + gap;

	// the settings now: gold where Auto-rate sets them
	wchar_t values[5][16];
	swprintf( values[0], 16, L"%d", r.rate );
	swprintf( values[1], 16, L"%d", r.updateRate );
	swprintf( values[2], 16, L"%d", r.cmdRate );
	swprintf( values[3], 16, L"%d", r.cmdBackup );
	swprintf( values[4], 16, L"%.3f", r.interp );
	const wchar_t* names[5] = { L"rate", L"cl_updaterate", L"cl_cmdrate", L"cl_cmdbackup", L"ex_interp" };
	for ( int i = 0; i < 5; i++ )
	{
		const int x = pad + i * cell;
		Text( m_hCaption, caption, x, y, names[i] );
		Text( m_hValue, r.on && r.managed[i] ? m_Bright : m_Text, x, y + captionTall + P( 1 ), values[i] );
	}
	y += captionTall + P( 1 ) + valueTall + gap;

	// what Auto-rate is doing
	const int square = Larger( 4, P( 6 ) );
	const int squareY = y + ( textTall - square ) / 2;
	if ( r.tone == AutoRateReadout::TONE_OFF )
	{
		surface->DrawSetColor( m_Dim );
		surface->DrawOutlinedRect( pad, squareY, pad + square, squareY + square );
	}
	else
		Rect( r.tone == AutoRateReadout::TONE_GOOD ? kGood : r.tone == AutoRateReadout::TONE_BUSY ? kFair : kPoor, pad,
			squareY, pad + square, squareY + square );
	wchar_t line[192];
	int n = 0;
	for ( const char* c = r.doing; *c && n < 191; c++ )
		line[n++] = (unsigned char)*c;
	line[n] = 0;
	Text( m_hText, r.on ? m_Text : m_Dim, pad + square + P( 5 ), y, line );
}
