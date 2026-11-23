// about_page.cpp - see about_page.h. Everything sits in one column in the middle of the page: the title in Counter-
// Strike's own lettering (Facelift, which resource/TrackerScheme.res registers), the line under it, the build
// number, then the author as in game credits - "CREATED BY" between two fading gold lines, the name and link right
// against the round picture. Colours are the scheme's: titles BrightControlText, text BaseText, notes the dim text
// colour, like the other tabs.
#include "hud.h"
#include "cl_util.h"
#include "vgui_controls/Controls.h"
#include "vgui/ISurface.h"
#include "vgui/IScheme.h"
#include "Label.h"
#include "URLLabel.h"
#include "about_page.h"
#include "../author_avatar.h"

#include <math.h>
#include <stdio.h>
#include <string>
#include <vector>

namespace
{
const wchar_t kTitle[] = L"SHADOW'S";	// in capitals, as Counter-Strike's own logo sets Facelift
const char kTagline[] = "Custom client for Counter-Strike 1.6.";
const char kBuildNumber[] = "v1.0.0";
const char kCreatedBy[] = "C R E A T E D    B Y";	// spaced out, a caption
const char kAuthor[] = "BlackShadow";
const char kAuthorURL[] = "https://github.com/BlackShadow";
const char kAuthorLinkText[] = "github.com/BlackShadow";
const char kPictureCache[] = "gfx/shadows/author_picture.cache";	// in the game folder

// the title (proportional units: 480 lines high)
const int kTitleTall = 44;			// the lettering's height
const int kShadowReach = 6;			// how far the shadow falls
const int kShadowLayers = 7;		// soft: this many copies, each fainter
const int kShadowAlpha = 54;		// the nearest copy's darkness (0..255; they add up)
const double kSwingPeriod = 5.0;	// seconds for the light to swing there and back
const double kSwingAngle = 0.62;	// radians the shadow leans either side of straight down
const double kGleamDelay = 0.25;	// the first gleam this soon after the title shows...
const double kGleamPeriod = 3.0;	// ...then one every this many seconds...
const double kGleamTime = 1.0;		// ...each taking this long to pass over the word
const double kRestartGap = 0.5;		// not drawn for this long (another tab, the window shut): starts again when back
const double kGleamStrength = 0.75;
const double kPi = 3.14159265358979323846;

// the author (proportional units)
const int kNameTall = 21;			// the name's lettering
const int kPictureSide = 62;		// the round picture
const int kRowGap = 18;				// between the name / link and the picture (more than the glow's reach)
const int kLineSpan = 150;			// each line beside the caption, before it has faded out
const double kGlowScale = 1.55;		// the glow round the picture: this many times its size
const double kGlowPeriod = 4.5;		// seconds for the glow to breathe in and out
const int kGlowLow = 55;			// its strength (alpha) at its faintest...
const int kGlowHigh = 125;			// ...and brightest

Color Mix( const Color& a, const Color& b, double t )
{
	return Color( (int)( a.r() + ( b.r() - a.r() ) * t ), (int)( a.g() + ( b.g() - a.g() ) * t ),
		(int)( a.b() + ( b.b() - a.b() ) * t ), (int)( a.a() + ( b.a() - a.a() ) * t ) );
}

// a white texture, alpha(d) for d the distance from the middle (0 there, 1 at the edge), made once
int RoundTexture( int& texture, double ( *alpha )( double d ) )
{
	if ( texture >= 0 )
		return texture;
	const int size = 128;
	std::vector<unsigned char> rgba( size * size * 4 );
	for ( int y = 0; y < size; y++ )
	{
		for ( int x = 0; x < size; x++ )
		{
			const double d = hypot( x + 0.5 - size / 2.0, y + 0.5 - size / 2.0 ) / ( size / 2.0 );
			const double a = alpha( d );
			unsigned char* p = &rgba[( y * size + x ) * 4];
			p[0] = p[1] = p[2] = 255;
			p[3] = (unsigned char)( 255.0 * ( a < 0 ? 0 : a > 1 ? 1 : a ) );
		}
	}
	texture = vgui2::surface()->CreateNewTextureID( true );
	vgui2::surface()->DrawSetTextureRGBA( texture, rgba.data(), size, size, 1, true );
	return texture;
}

// a disc, its edge smooth (the picture's ring and its shadow)
int DiscTexture()
{
	static int texture = -1;
	return RoundTexture( texture, []( double d ) { return ( 1.0 - d ) * 64.0 + 0.5; } );	// one pixel's soft edge
}

// a glow: strongest in the middle, nothing at its edge (behind the picture)
int GlowTexture()
{
	static int texture = -1;
	return RoundTexture( texture, []( double d ) { return d < 1.0 ? ( 1.0 - d ) * ( 1.0 - d ) : 0.0; } );
}
}

// ------------------------------------------------------------------------------------------------------------------
// the title

class CAnimatedTitle : public vgui2::Panel
{
	typedef vgui2::Panel BaseClass;

public:
	CAnimatedTitle( vgui2::Panel* parent, const char* name ) : BaseClass( parent, name )
	{
		SetPaintBackgroundEnabled( false );
		SetMouseInputEnabled( false );
	}

	// the room the lettering and its shadow take
	int ContentWide()
	{
		EnsureFont();
		int wide = 0;
		for ( const wchar_t* c = kTitle; *c; c++ )
		{
			int a, b, w;
			vgui2::surface()->GetCharABCwide( m_hFont, *c, a, b, w );
			wide += a + b + w;
		}
		return wide + 2 * GetProportionalScaledValue( kShadowReach );
	}

	int ContentTall()
	{
		EnsureFont();
		return vgui2::surface()->GetFontTall( m_hFont ) + GetProportionalScaledValue( kShadowReach );
	}

protected:
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override
	{
		BaseClass::ApplySchemeSettings( scheme );
		m_Bright = GetSchemeColor( "BrightControlText", Color( 196, 181, 80, 255 ), scheme );
		m_Gleam = Mix( m_Bright, GetSchemeColor( "BrightBaseText", Color( 255, 255, 255, 255 ), scheme ), 0.8 );
		m_iFontTall = 0;	// the size may have changed (resolution): made again
	}

	void Paint() override
	{
		EnsureFont();
		int wide, tall;
		GetSize( wide, tall );
		const int reach = GetProportionalScaledValue( kShadowReach );
		const int fontTall = vgui2::surface()->GetFontTall( m_hFont );
		const int textWide = ContentWide() - 2 * reach;
		const int x0 = ( wide - textWide ) / 2;
		const int y0 = ( tall - fontTall - reach ) / 2;
		// the animation's clock: from when the title came on screen (so the first gleam comes at once, not whenever the
		// game's clock says)
		const double now = gEngfuncs.GetAbsoluteTime();
		if ( now - m_flLastPaint > kRestartGap || now < m_flLastPaint )
			m_flShownAt = now;
		m_flLastPaint = now;
		const double t = now - m_flShownAt;
		vgui2::surface()->DrawSetTextFont( m_hFont );

		// the shadow: falls below, leaning slowly from one side to the other as if its light moved; soft, the copies
		// further away fainter
		const double angle = kPi / 2 + kSwingAngle * sin( t * 2.0 * kPi / kSwingPeriod );
		for ( int k = kShadowLayers; k >= 1; k-- )
		{
			const double d = reach * (double)k / kShadowLayers;
			const int alpha = (int)( kShadowAlpha * ( 1.0 - ( k - 1 ) / (double)kShadowLayers ) );
			DrawWord( x0 + (int)floor( cos( angle ) * d + 0.5 ), y0 + (int)floor( sin( angle ) * d + 0.5 ), textWide, -1.0,
				Color( 0, 0, 0, alpha ) );
		}
		// the letters, a gleam passing over them now and then
		const double gleam = t < kGleamDelay ? -1.0 : fmod( t - kGleamDelay, kGleamPeriod ) / kGleamTime;
		DrawWord( x0, y0, textWide, gleam >= 0.0 && gleam < 1.0 ? gleam : -1.0, m_Bright );
		vgui2::surface()->DrawFlushText();
	}

private:
	// the word letter by letter at x, y; gleam 0..1: where the gleam is on its way over the word (-1: none)
	void DrawWord( int x, int y, int textWide, double gleam, const Color& color )
	{
		const double sweep = -0.25 * textWide + gleam * 1.5 * textWide;
		const double spread = textWide * 0.13;
		int along = 0;
		for ( const wchar_t* c = kTitle; *c; c++ )
		{
			int a, b, w;
			vgui2::surface()->GetCharABCwide( m_hFont, *c, a, b, w );
			const int advance = a + b + w;
			Color shade = color;
			if ( gleam >= 0.0 )
			{
				const double u = ( along + advance * 0.5 - sweep ) / spread;
				shade = Mix( color, m_Gleam, kGleamStrength * exp( -u * u ) );
			}
			vgui2::surface()->DrawSetTextColor( shade );
			vgui2::surface()->DrawSetTextPos( x + along, y );
			vgui2::surface()->DrawUnicodeChar( *c );
			along += advance;
		}
	}

	void EnsureFont()
	{
		const int tall = GetProportionalScaledValue( kTitleTall );
		if ( m_hFont && tall == m_iFontTall )
			return;
		static bool registered;
		if ( !registered )
		{
			registered = true;
			vgui2::surface()->AddCustomFontFile( "resource/font/facelift.ttf" );	// TrackerScheme.res has it too
		}
		m_hFont = vgui2::surface()->CreateFont();
		vgui2::surface()->AddGlyphSetToFont( m_hFont, "Facelift", tall, 0, 0, 0, vgui2::ISurface::FONTFLAG_ANTIALIAS, 0, 0 );
		m_iFontTall = tall;
	}

	vgui2::HFont m_hFont = 0;
	int m_iFontTall = 0;
	double m_flShownAt = 0.0;	// when the title last came on screen
	double m_flLastPaint = -1000.0;
	Color m_Bright = Color( 196, 181, 80, 255 );
	Color m_Gleam = Color( 255, 245, 200, 255 );
};

// ------------------------------------------------------------------------------------------------------------------
// the author's picture: round, in a gold ring with a soft shadow under it and a gold glow behind it that slowly
// breathes (a dim disc until the picture has arrived). The panel is bigger than the picture by the glow's reach.

class CAuthorPicture : public vgui2::Panel
{
	typedef vgui2::Panel BaseClass;

public:
	CAuthorPicture( vgui2::Panel* parent, const char* name ) : BaseClass( parent, name )
	{
		SetPaintBackgroundEnabled( false );
		SetMouseInputEnabled( false );
	}

protected:
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override
	{
		BaseClass::ApplySchemeSettings( scheme );
		m_Ring = GetSchemeColor( "BrightControlText", Color( 196, 181, 80, 255 ), scheme );
	}

	void Paint() override
	{
		int wide, tall;
		GetSize( wide, tall );
		const int box = wide < tall ? wide : tall;
		const int side = (int)( box / kGlowScale );
		const int x = ( wide - side ) / 2, y = ( tall - side ) / 2;
		const int ring = GetProportionalScaledValue( 1 ) > 1 ? GetProportionalScaledValue( 1 ) : 1;
		UploadIfNew();
		// the glow, breathing
		const double breath = 0.5 + 0.5 * sin( gEngfuncs.GetAbsoluteTime() * 2.0 * kPi / kGlowPeriod );
		vgui2::surface()->DrawSetTexture( GlowTexture() );
		vgui2::surface()->DrawSetColor( m_Ring.r(), m_Ring.g(), m_Ring.b(), (int)( kGlowLow + ( kGlowHigh - kGlowLow ) * breath ) );
		vgui2::surface()->DrawTexturedRect( ( wide - box ) / 2, ( tall - box ) / 2, ( wide + box ) / 2, ( tall + box ) / 2 );
		// the soft shadow a little below, the gold ring, the picture
		vgui2::surface()->DrawSetTexture( DiscTexture() );
		vgui2::surface()->DrawSetColor( 0, 0, 0, 110 );
		vgui2::surface()->DrawTexturedRect( x, y + ring, x + side, y + side + ring );
		vgui2::surface()->DrawSetColor( m_Ring );
		vgui2::surface()->DrawTexturedRect( x, y, x + side, y + side );
		if ( m_iTexture >= 0 && m_iVersion > 0 )
		{
			vgui2::surface()->DrawSetColor( 255, 255, 255, 255 );
			vgui2::surface()->DrawSetTexture( m_iTexture );
		}
		else
			vgui2::surface()->DrawSetColor( 40, 46, 36, 255 );	// waiting for the picture
		vgui2::surface()->DrawTexturedRect( x + ring, y + ring, x + side - ring, y + side - ring );
	}

private:
	// a newer picture from author_avatar.cpp: cut round (soft edge) and handed to the surface
	void UploadIfNew()
	{
		const int version = AuthorAvatar_Version();
		if ( version == m_iVersion )
			return;
		std::vector<unsigned char> rgba;
		int w = 0, h = 0;
		if ( AuthorAvatar_Copy( rgba, w, h ) )
		{
			const double r = ( w < h ? w : h ) / 2.0 - 0.5;
			for ( int py = 0; py < h; py++ )
			{
				for ( int px = 0; px < w; px++ )
				{
					const double d = hypot( px + 0.5 - w / 2.0, py + 0.5 - h / 2.0 );
					const double a = r - d + 0.5;
					unsigned char* p = &rgba[( py * w + px ) * 4];
					p[3] = (unsigned char)( p[3] * ( a < 0 ? 0 : a > 1 ? 1 : a ) );
				}
			}
			if ( m_iTexture < 0 )
				m_iTexture = vgui2::surface()->CreateNewTextureID( true );
			vgui2::surface()->DrawSetTextureRGBA( m_iTexture, rgba.data(), w, h, 1, true );
		}
		m_iVersion = version;
	}

	int m_iTexture = -1;
	int m_iVersion = 0;
	Color m_Ring = Color( 196, 181, 80, 255 );
};

// ------------------------------------------------------------------------------------------------------------------
// the page

static vgui2::Label* MakeLabel( vgui2::Panel* parent, const char* name, const char* text, vgui2::Label::EColorState color,
	vgui2::Label::Alignment alignment )
{
	vgui2::Label* label = new vgui2::Label( parent, name, text );
	label->SetTextColorState( color );
	label->SetContentAlignment( alignment );
	return label;
}

CAboutPage::CAboutPage( vgui2::Panel* parent, const char* name ) : BaseClass( parent, name, true )
{
	m_pTitle = new CAnimatedTitle( this, "AboutTitle" );
	m_pTagline = MakeLabel( this, "AboutTagline", kTagline, vgui2::Label::CS_NORMAL, vgui2::Label::a_center );
	char build[64];
	snprintf( build, sizeof( build ), "Build number: %s", kBuildNumber );
	m_pBuild = MakeLabel( this, "AboutBuild", build, vgui2::Label::CS_DULL, vgui2::Label::a_center );
	m_pCreatedBy = MakeLabel( this, "AboutCreatedBy", kCreatedBy, vgui2::Label::CS_DULL, vgui2::Label::a_center );
	m_pAuthorName = MakeLabel( this, "AboutAuthorName", kAuthor, vgui2::Label::CS_BRIGHT, vgui2::Label::a_east );
	m_pAuthorLink = new vgui2::URLLabel( this, "AboutAuthorLink", kAuthorLinkText, kAuthorURL );
	m_pAuthorLink->SetContentAlignment( vgui2::Label::a_east );
	m_pPicture = new CAuthorPicture( this, "AboutAuthorPicture" );
	// the picture: from its copy on disk, then GitHub's current one (once per game)
	const std::string cache = std::string( gEngfuncs.pfnGetGameDirectory() ) + "/" + kPictureCache;
	AuthorAvatar_Request( cache.c_str() );
}

void CAboutPage::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	BaseClass::ApplySchemeSettings( scheme );
	const bool proportional = IsProportional();
	m_pBuild->SetFont( scheme->GetFont( "DefaultSmall", proportional ) );
	m_pCreatedBy->SetFont( scheme->GetFont( "DefaultVerySmall", proportional ) );
	// the name: larger and bold, smooth (a font of its own, made again when the size changes)
	const int nameTall = GetProportionalScaledValue( kNameTall );
	if ( !m_hNameFont || nameTall != m_iNameTall )
	{
		m_hNameFont = vgui2::surface()->CreateFont();
		vgui2::surface()->AddGlyphSetToFont( m_hNameFont, "Tahoma", nameTall, 700, 0, 0, vgui2::ISurface::FONTFLAG_ANTIALIAS, 0, 0 );
		m_iNameTall = nameTall;
	}
	m_pAuthorName->SetFont( m_hNameFont );
	const Color accent = GetSchemeColor( "BrightControlText", Color( 196, 181, 80, 255 ), scheme );
	m_AccentColor[0] = accent.r();
	m_AccentColor[1] = accent.g();
	m_AccentColor[2] = accent.b();
	m_AccentColor[3] = 150;
	InvalidateLayout( false, false );
}

void CAboutPage::PerformLayout()
{
	BaseClass::PerformLayout();
	int wide, tall;
	GetSize( wide, tall );
	const int margin = GetProportionalScaledValue( 12 );
	int w;

	// the column's height, to stand it in the middle of the page (a little above)
	const int titleTall = m_pTitle->ContentTall();
	int tagTall, buildTall, captionWide, captionTall;
	m_pTagline->GetContentSize( w, tagTall );
	m_pBuild->GetContentSize( w, buildTall );
	m_pCreatedBy->GetContentSize( captionWide, captionTall );
	const int pictureSide = GetProportionalScaledValue( kPictureSide );
	const int gapSmall = GetProportionalScaledValue( 3 );
	const int gapBig = GetProportionalScaledValue( 16 );
	const int column = titleTall + gapSmall + tagTall + gapSmall + buildTall + gapBig + captionTall + gapBig + pictureSide;
	int y = ( tall - column ) / 2 - GetProportionalScaledValue( 12 );
	if ( y < margin )
		y = margin;

	m_pTitle->SetBounds( margin, y, wide - 2 * margin, titleTall );
	y += titleTall + gapSmall;
	m_pTagline->SetBounds( margin, y, wide - 2 * margin, tagTall );
	y += tagTall + gapSmall;
	m_pBuild->SetBounds( margin, y, wide - 2 * margin, buildTall );
	y += buildTall + gapBig;

	// "CREATED BY" in the middle, its fading lines either side (Paint)
	m_pCreatedBy->SetBounds( ( wide - captionWide ) / 2, y, captionWide + 2, captionTall );
	m_iCaptionY = y + captionTall / 2;
	m_iCaptionHalf = captionWide / 2 + 1;
	y += captionTall + gapBig;

	// the author: name and link right against the picture, the picture on the right, the three in the middle
	int nameWide, nameTall, linkWide, linkTall;
	m_pAuthorName->GetContentSize( nameWide, nameTall );
	m_pAuthorLink->GetContentSize( linkWide, linkTall );
	const int textWide = nameWide > linkWide ? nameWide : linkWide;
	const int gapRow = GetProportionalScaledValue( kRowGap );
	const int rowX = ( wide - ( textWide + gapRow + pictureSide ) ) / 2;
	const int lineGap = GetProportionalScaledValue( 2 );
	int ty = y + ( pictureSide - ( nameTall + lineGap + linkTall ) ) / 2;
	m_pAuthorName->SetBounds( rowX, ty, textWide, nameTall );
	ty += nameTall + lineGap;
	m_pAuthorLink->SetBounds( rowX + textWide - linkWide - 2, ty, linkWide + 2, linkTall );	// only the text links
	const int glowBox = (int)( pictureSide * kGlowScale );
	const int pictureX = rowX + textWide + gapRow;
	m_pPicture->SetBounds( pictureX - ( glowBox - pictureSide ) / 2, y - ( glowBox - pictureSide ) / 2, glowBox, glowBox );
}

void CAboutPage::Paint()
{
	BaseClass::Paint();
	if ( m_iCaptionHalf <= 0 )
		return;
	// the lines either side of "CREATED BY": gold, fading out away from it
	int wide, tall;
	GetSize( wide, tall );
	const int gap = GetProportionalScaledValue( 10 );
	const int span = GetProportionalScaledValue( kLineSpan );
	const int thick = GetProportionalScaledValue( 1 ) / 2 > 1 ? GetProportionalScaledValue( 1 ) / 2 : 1;
	const int steps = 40;
	for ( int side = -1; side <= 1; side += 2 )
	{
		const int start = wide / 2 + side * ( m_iCaptionHalf + gap );
		for ( int i = 0; i < steps; i++ )
		{
			const int alpha = (int)( m_AccentColor[3] * pow( 1.0 - i / (double)steps, 1.3 ) );
			const int a = start + side * span * i / steps;
			const int b = start + side * span * ( i + 1 ) / steps;
			vgui2::surface()->DrawSetColor( m_AccentColor[0], m_AccentColor[1], m_AccentColor[2], alpha );
			vgui2::surface()->DrawFilledRect( a < b ? a : b, m_iCaptionY, a < b ? b : a, m_iCaptionY + thick );
		}
	}
}
