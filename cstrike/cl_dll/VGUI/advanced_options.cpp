// advanced_options.cpp - the "Advanced" window (main menu item, console command cl_advanced): the Shadows client's own
// options, in tabs in this order - Models (solid team colours, head colour, player and weapon models, servers' own
// models overridden), Radar, FX (HUD colour, menu background, font style, weather, bob style, no sky, stretched 4:3),
// Crosshair, Misc (Auto-rate, radio, demos) and About. A tab whose rows don't fit scrolls (a scroll bar at its right,
// the mouse wheel; the previews stay where they are).
//
// Built exactly like the game's own Options window (HL25-Engine/GameUI/OptionsDialog.cpp): a vgui2::PropertyDialog
// with tabs, OK / Cancel / Apply, the main menu's scheme and HD-proportional sizing, so title, tabs, fonts, controls and
// spacing are the stock ones at every resolution. The window can be resized (down to a minimum) and keeps its size
// (cl_advanced_size). Each page: the names on the left, the lists right after the longest name of any tab (so the
// tabs line up), check boxes at the left edge (a dim note under the ones that have one, lined up with the box's name),
// a dim note at the bottom, a live preview on the right. The Crosshair tab has its preview as a banner across the top
// and three columns under it (shape and colour with sliders, its behaviour check boxes). Changes are written to
// the cvars on OK or Apply (Cancel drops them). Options that do nothing with the current choices are disabled (box and
// name): the team colours while solid team colours are off; size, range, opacity and rotation with the classic round
// radar; stretched 4:3 on a screen that is not wider than 4:3. Each list marks its default choice "(Default)"; Use Defaults sets every tab back to the defaults (written on
// OK / Apply like any change). The defaults here are the cvars' registration values (vismods.cpp, radar2.cpp,
// ammo.cpp, hud.cpp): keep them in step.
#include "hud.h"
#include "cl_util.h"
#include "IBaseUI.h"
#include "GameUI_Interfaces.h"
// windows.h (pulled in above) names a function PropertySheet: the vgui2 class must keep its own name
#ifdef PropertySheet
#undef PropertySheet
#endif
#include "vgui_controls/Controls.h"
#include "vgui/ISurface.h"
#include "vgui/IInput.h"
#include "vgui/IPanel.h"
#include "vgui/IScheme.h"
#include "vgui/KeyValues.h"
#include "vgui/KeyCode.h"
#include "PropertyDialog.h"
#include "PropertyPage.h"
#include "PropertySheet.h"
#include "Label.h"
#include "Button.h"
#include "CheckButton.h"
#include "Slider.h"
#include "Divider.h"
#include "ScrollBar.h"
#include "vgui_controls/ComboBox.h"
#include "TextImage.h"
#include "player_preview.h"
#include "about_page.h"
#include "../crosshair_draw.h"
#include "../radar2.h"
#include "../stretch_view.h"
#include "autorate_panel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <algorithm>
#include <string>
#include <vector>

void AdvancedOptions_Init();
void Crosshair_TakeOverCvars();	// ammo.cpp

namespace
{

struct Choice
{
	const char* label;
	const char* value;
	const char* alias;	// another text the cvar may hold for the same choice, or none
};

const Choice kColors[] = {
	{ "Red", "255 0 0" }, { "Orange", "255 128 0" }, { "Yellow", "255 255 0" }, { "Green", "0 255 0" },
	{ "Cyan", "0 255 255" }, { "Blue", "0 0 255" }, { "Purple", "160 0 255" }, { "Pink", "255 0 200" },
	{ "White", "255 255 255" },
};
// cl_headcolor (vismods.cpp): off, or the team colours' choices
const Choice kHeadColors[] = {
	{ "Off", "0" }, { "Red", "255 0 0" }, { "Orange", "255 128 0" }, { "Yellow", "255 255 0" }, { "Green", "0 255 0" },
	{ "Cyan", "0 255 255" }, { "Blue", "0 0 255" }, { "Purple", "160 0 255" }, { "Pink", "255 0 200" },
	{ "White", "255 255 255" },
};
// cl_weaponmodels (vismods.cpp): the game's weapon models, CS 1.5's (models/shadows/retail) or the beta's
// (models/shadows/beta); a weapon the set has no model for keeps the game's. Numbered 0, 1, 2 in this order for the
// preview (CPlayerPreview::SetWeapons)
const Choice kWeaponSets[] = { { "Original", "original" }, { "Retail", "retail" }, { "Beta", "beta" } };
// cl_playermodels (vismods.cpp): the game's player models, CS 1.0's (models/shadows/retail/player) or the beta's
// (models/shadows/beta/player); a model the set has none of keeps the game's. Numbered 0, 1, 2 in this order for the
// preview (CPlayerPreview::SetPlayers)
const Choice kPlayerSets[] = { { "Original", "original" }, { "Retail", "retail" }, { "Beta", "beta" } };
// cl_fontstyle (fontstyle.cpp): the game's fonts, or the WON engine's - menus in CS 1.5's fonts, chat in its lettering
const Choice kFontStyles[] = { { "Original", "original" }, { "WON", "won" } };
const Choice kRadarStyle[] = { { "Enhanced Radar", "1" }, { "Classic radar", "0" } };
const Choice kRadarSize[] = { { "Small", "120" }, { "Medium", "150" }, { "Large", "200" }, { "Very large", "260" } };
// cl_radar_range (radar2.cpp): world units from the middle to the edge
const Choice kRadarRange[] = { { "Closer", "450" }, { "Close", "600" }, { "Normal", "1000" }, { "Far", "1600" } };
const Choice kRadarAlpha[] = { { "50%", "0.5" }, { "70%", "0.7" }, { "85%", "0.85" }, { "100%", "1" } };
// hud_color: "r g b", the animated "rainbow" / "christmas", or "dynamic" (health and money by how much there is) (see
// HudColorRGB in hud.cpp)
const Choice kHudColors[] = {
	{ "Orange", "255 160 0" }, { "White", "255 255 255" }, { "Red", "255 0 0" }, { "Green", "0 255 0" },
	{ "Yellow", "255 255 0" }, { "Cyan", "0 255 255" }, { "Crimson", "220 20 60" },
	{ "Rainbow", "rainbow" }, { "Dynamic", "dynamic" }, { "Christmas", "christmas" },
};
// cl_menubackground (menu_background.cpp): "auto" goes round the three, a minute each
const Choice kMenuBackgrounds[] = { { "Auto", "auto" }, { "Original", "original" }, { "Beta", "beta" }, { "Retail", "retail" } };
// cl_weather_type (z_weather.cpp): on every map, outdoors
const Choice kWeathers[] = { { "None", "0" }, { "Rain", "1" }, { "Snow", "2" } };
// cl_bobstyle (view.cpp): the weapon's bob - Half-Life's with the WON tilt, the Steam versions' without it, or Half-Life 2's
// with Source's weapon lag
const Choice kBobStyles[] = { { "WON", "won" }, { "Retail", "retail" }, { "Source", "source" } };
// cl_crosshair_color: the game's own five (the Multiplayer options' list), white and more; any other colour with the
// red / green / blue sliders
const Choice kCrosshairColors[] = {
	{ "Green", "50 250 50" }, { "Red", "250 50 50" }, { "Blue", "50 50 250" }, { "Yellow", "250 250 50" },
	{ "Light blue", "50 250 250" }, { "White", "255 255 255" }, { "Pink", "255 0 255" }, { "Purple", "160 0 255" },
	{ "Orange", "255 128 0" },
};

#define COUNT_OF( a ) ( (int)( sizeof( a ) / sizeof( ( a )[0] ) ) )

enum RowKind
{
	ROW_TOGGLE,		// check box
	ROW_CHOICE,		// list
	ROW_SLIDER,		// slider over whole numbers, the value shown beside it
	ROW_COLOR_PART,	// slider for the red, green or blue of a colour list on the same page (no cvar of its own)
	ROW_HEADER,		// a section's title, a line under it (no cvar; one-column tabs, the Crosshair tab's columns)
	ROW_NOTE,		// a dim line of text between the rows (no cvar; one-column tabs)
};

// how a slider shows its value
enum ValueText
{
	VALUE_PLAIN,	// 3
	VALUE_SIGNED,	// +3, -3
	VALUE_OFF_ZERO,	// 0 shows "Off"
	VALUE_ALPHA,	// 0..255 as a percentage; 0 shows "Fade" (cl_crosshair_alpha 0: the game's, fading while shooting)
};

struct Row
{
	RowKind kind;
	const char* text;
	const char* cvar;			// a colour part: the colour list's cvar
	const char* defaultValue;	// the cvar's registration value
	const Choice* choices;
	int count;
	bool words;					// the value is text ("r g b" or a word), written as it is; otherwise a number
	int needs;					// -1, or the row on the same page that must be on (check box ticked / choice not "0")
	int column;					// the Crosshair tab (LayoutBanner): 0 left, 1 middle, 2 right
	int minimum, maximum;		// slider
	ValueText valueText;		// slider
	int component;				// colour part: 0 red, 1 green, 2 blue
	const char* note;			// check box: a dim line under it, or none
	bool readout;				// check box: Auto-rate's readout under it instead of a note (autorate_panel.cpp)
	bool widescreenOnly;		// only on a screen wider than 4:3 (disabled otherwise)
};

Row MakeToggle( const char* text, const char* cvar, const char* defaultValue, int needs = -1 )
{
	Row row = {};
	row.kind = ROW_TOGGLE;
	row.text = text;
	row.cvar = cvar;
	row.defaultValue = defaultValue;
	row.needs = needs;
	return row;
}

Row MakeList( const char* text, const char* cvar, const char* defaultValue, const Choice* choices, int count, bool words,
	int needs = -1, int column = 0 )
{
	Row row = MakeToggle( text, cvar, defaultValue, needs );
	row.kind = ROW_CHOICE;
	row.choices = choices;
	row.count = count;
	row.words = words;
	row.column = column;
	return row;
}

Row MakeSlider( const char* text, const char* cvar, const char* defaultValue, int minimum, int maximum, ValueText valueText,
	int column )
{
	Row row = MakeToggle( text, cvar, defaultValue );
	row.kind = ROW_SLIDER;
	row.minimum = minimum;
	row.maximum = maximum;
	row.valueText = valueText;
	row.column = column;
	return row;
}

Row MakeColorPart( const char* text, const char* colorCvar, int component, int column )
{
	Row row = MakeSlider( text, colorCvar, "", 0, 255, VALUE_PLAIN, column );
	row.kind = ROW_COLOR_PART;
	row.component = component;
	return row;
}

// a row put in a column of a multi-column tab (a section header there too)
Row InColumn( Row row, int column )
{
	row.column = column;
	return row;
}

Row WithNote( Row row, const char* note )
{
	row.note = note;
	return row;
}

// a check box with Auto-rate's readout under it (autorate_panel.cpp)
Row WithReadout( Row row )
{
	row.readout = true;
	return row;
}

Row WidescreenOnly( Row row )
{
	row.widescreenOnly = true;
	return row;
}

// rows without a cvar (cvar "" so name lookups never meet a null)
Row MakeHeader( const char* text )
{
	Row row = MakeToggle( text, "", "" );
	row.kind = ROW_HEADER;
	return row;
}

Row MakeNote( const char* text )
{
	Row row = MakeToggle( text, "", "" );
	row.kind = ROW_NOTE;
	return row;
}

// does the row hold an option (a cvar of its own)?
bool IsOption( const Row& row )
{
	return row.kind != ROW_COLOR_PART && row.kind != ROW_HEADER && row.kind != ROW_NOTE;
}

// the needs index of the team colours is the check box's row (1, under the header)
const Row kModelRows[] = {
	MakeHeader( "Visibility" ),
	MakeToggle( "Solid team colors", "cl_teamcolors", "0" ),
	MakeList( "Terrorist color", "cl_teamcolor_t", "255 0 0", kColors, COUNT_OF( kColors ), true, 1 ),
	MakeList( "CT color", "cl_teamcolor_ct", "0 0 255", kColors, COUNT_OF( kColors ), true, 1 ),
	MakeList( "VIP color", "cl_teamcolor_vip", "255 255 0", kColors, COUNT_OF( kColors ), true, 1 ),
	MakeList( "Head color", "cl_headcolor", "0", kHeadColors, COUNT_OF( kHeadColors ), true, 1 ),	// with solid team colors
	MakeNote( "Players, weapons and bodies in one flat color per team." ),
	MakeHeader( "Style" ),
	MakeList( "Players", "cl_playermodels", "original", kPlayerSets, COUNT_OF( kPlayerSets ), true ),
	MakeList( "Weapons", "cl_weaponmodels", "original", kWeaponSets, COUNT_OF( kWeaponSets ), true ),
	// vismods.cpp: a server's own player and weapon models drawn as the game's (and so in the sets above)
	WithNote( MakeToggle( "Override server models", "cl_overridemodels", "0" ),
		"Keeps your chosen player and weapon models when a server forces its own custom ones." ),
};
const Row kRadarRows[] = {
	MakeList( "Radar", "cl_radar_square", "1", kRadarStyle, COUNT_OF( kRadarStyle ), false ),
	// these four only change the enhanced (square map) radar (radar2.cpp)
	MakeList( "Size", "cl_radar_size", "150", kRadarSize, COUNT_OF( kRadarSize ), false, 0 ),
	MakeList( "Range", "cl_radar_range", "1000", kRadarRange, COUNT_OF( kRadarRange ), false, 0 ),
	MakeList( "Map opacity", "cl_radar_alpha", RADAR_MAP_OPACITY_DEFAULT, kRadarAlpha, COUNT_OF( kRadarAlpha ), false, 0 ),
	MakeToggle( "Radar rotation", "cl_radar_rotate", "1", 0 ),
};
// the FX tab: the look of the HUD, menus and world
const Row kFxRows[] = {
	MakeList( "HUD color", "hud_color", HUD_COLOR_DEFAULT, kHudColors, COUNT_OF( kHudColors ), true ),
	MakeList( "Menu background", "cl_menubackground", "auto", kMenuBackgrounds, COUNT_OF( kMenuBackgrounds ), true ),
	MakeList( "Font style", "cl_fontstyle", "original", kFontStyles, COUNT_OF( kFontStyles ), true ),
	MakeList( "Weather", "cl_weather_type", "0", kWeathers, COUNT_OF( kWeathers ), false ),	// hud.cpp
	MakeList( "Bobbing style", "cl_bobstyle", "won", kBobStyles, COUNT_OF( kBobStyles ), true ),
	WithNote( MakeToggle( "No sky", "cl_nosky", "0" ), "No sky draws the sky solid black." ),
	// stretch_view.cpp: nothing to stretch on a 4:3 screen
	WithNote( WidescreenOnly( MakeToggle( "Stretched 4:3", "cl_stretch43", "0" ) ), "Enable 4:3 stretched feeling on 16:9" ),
};
// the Misc tab: Auto-rate (auto_rate.cpp: the network settings tuned to the server, its readout under it), the other
// radio and server plugins' texts off, demos recorded on their own and old ones deleted (misc_features.cpp;
// Auto-delete only with Auto-record on: needs row 6)
const Choice kDemoAges[] = { { "5 days", "5" }, { "15 days", "15" }, { "30 days", "30" }, { "60 days", "60" },
	{ "Never", "0" } };
const Row kMiscRows[] = {
	MakeHeader( "Network" ),
	WithReadout( MakeToggle( "Auto-rate", "cl_autorate", "0" ) ),
	MakeHeader( "Messages" ),
	WithNote( MakeToggle( "Disable radio", "cl_disableradio", "0" ), "Disables radio sounds and notifications." ),
	WithNote( MakeToggle( "Hide AMXX texts", "cl_hideamxx", "0" ),
		"Hides server plugins' adverts, repeated notices and info boards; replies to you stay." ),
	MakeHeader( "Demos" ),
	MakeToggle( "Auto-record demos", "cl_autorecord", "0" ),
	MakeList( "Auto-delete", "cl_autorecord_delete", "30", kDemoAges, COUNT_OF( kDemoAges ), false, 6 ),
};
// the crosshair (ammo.cpp, crosshair_draw.cpp); cl_dynamiccrosshair is registered in hud.cpp. Laid out under the
// preview banner in three columns (LayoutBanner): its shape, its colour, how it behaves.
const Row kCrosshairRows[] = {
	// left: the shape (its size is the length: cl_crosshair_size stays "auto", ammo.cpp)
	InColumn( MakeHeader( "Shape" ), 0 ),
	MakeSlider( "Length", "cl_crosshair_length", "0", -5, 15, VALUE_SIGNED, 0 ),
	MakeSlider( "Thickness", "cl_crosshair_thickness", "1", 1, 8, VALUE_PLAIN, 0 ),
	MakeSlider( "Gap", "cl_crosshair_gap", "0", -8, 16, VALUE_SIGNED, 0 ),
	MakeSlider( "Outline", "cl_crosshair_outline", "0", 0, 3, VALUE_OFF_ZERO, 0 ),
	MakeSlider( "Center dot", "cl_crosshair_dot", "0", 0, 8, VALUE_OFF_ZERO, 0 ),
	// middle: the colour
	InColumn( MakeHeader( "Color" ), 1 ),
	MakeList( "Preset", "cl_crosshair_rgb", "50 250 50", kCrosshairColors, COUNT_OF( kCrosshairColors ), true, -1, 1 ),
	MakeColorPart( "Red", "cl_crosshair_rgb", 0, 1 ),
	MakeColorPart( "Green", "cl_crosshair_rgb", 1, 1 ),
	MakeColorPart( "Blue", "cl_crosshair_rgb", 2, 1 ),
	MakeSlider( "Opacity", "cl_crosshair_alpha", "0", 0, 255, VALUE_ALPHA, 1 ),
	// right: how it behaves
	InColumn( MakeHeader( "Behavior" ), 2 ),
	InColumn( MakeToggle( "Translucent", "cl_crosshair_additive", "1" ), 2 ),
	InColumn( MakeToggle( "T shape", "cl_crosshair_t", "0" ), 2 ),
	InColumn( MakeToggle( "Widen when moving", "cl_dynamiccrosshair", "1" ), 2 ),
	InColumn( MakeToggle( "Widen when shooting", "cl_crosshair_recoil", "1" ), 2 ),
	InColumn( MakeToggle( "Sniper rifle crosshair", "cl_crosshair_sniper", "0" ), 2 ),
};

bool ParseColor( const char* text, int rgb[3] )
{
	return sscanf( text, "%d %d %d", &rgb[0], &rgb[1], &rgb[2] ) == 3;
}

bool SameColor( const char* a, const char* b )
{
	int x[3] = { -1, -1, -1 }, y[3] = { -2, -2, -2 };
	if ( !ParseColor( a, x ) || !ParseColor( b, y ) )
		return false;
	return x[0] == y[0] && x[1] == y[1] && x[2] == y[2];
}

bool SameNumber( const char* a, const char* b )
{
	return fabs( atof( a ) - atof( b ) ) < 0.001;
}

// 'text' as the row's own value when it is one of the row's aliases ("0" -> "auto")
const char* Canonical( const Row& row, const char* text )
{
	for ( int k = 0; k < row.count; k++ )
	{
		if ( row.choices[k].alias && !stricmp( text, row.choices[k].alias ) )
			return row.choices[k].value;
	}
	return text;
}

// true when the cvar string 'current' already means the choice 'value'
bool SameValue( const Row& row, const char* current, const char* value )
{
	current = Canonical( row, current );
	value = Canonical( row, value );
	if ( !stricmp( current, value ) )
		return true;
	return row.words ? SameColor( current, value ) : SameNumber( current, value );
}

const char* CvarString( const char* name )
{
	const char* value = gEngfuncs.pfnGetCvarString( (char*)name );
	return value ? value : "";
}

// (min / max are macros in this code base)
int Larger( int a, int b )
{
	return a > b ? a : b;
}

int Smaller( int a, int b )
{
	return a < b ? a : b;
}

int Clamp( int value, int low, int high )
{
	return value < low ? low : value > high ? high : value;
}

// 'text' in lines no wider than 'wide' in 'font', broken at spaces (a word wider than a line stays whole): a label then
// shows every line, where one line too long for it would end in "..." (and a label's own wrapping needs its size
// before it can say how tall it is)
std::string WrapText( vgui2::HFont font, const std::string& text, int wide )
{
	if ( !font || wide <= 0 )
		return text;
	auto widthOf = [font]( const std::string& s ) {
		int total = 0;
		for ( unsigned char ch : s )
		{
			int a = 0, b = 0, c = 0;
			vgui2::surface()->GetCharABCwide( font, ch, a, b, c );
			total += a + b + c;
		}
		return total;
	};
	std::string out, line;
	size_t i = 0;
	while ( i < text.size() )
	{
		if ( text[i] == '\n' )
		{
			out += line + "\n";
			line.clear();
			i++;
			continue;
		}
		size_t end = text.find_first_of( " \n", i );
		if ( end == std::string::npos )
			end = text.size();
		const std::string word = text.substr( i, end - i );
		const std::string tried = line.empty() ? word : line + " " + word;
		if ( !line.empty() && widthOf( tried ) > wide )
		{
			out += line + "\n";
			line = word;
		}
		else
			line = tried;
		i = end;
		while ( i < text.size() && text[i] == ' ' )
			i++;
	}
	return out + line;
}

void SetWrappedText( vgui2::Label* label, const std::string& text, int wide )
{
	if ( !label )
		return;
	const std::string wrapped = WrapText( label->GetFont(), text, wide );
	char shown[1024] = "";
	label->GetText( shown, sizeof( shown ) );
	if ( wrapped != shown )
		label->SetText( wrapped.c_str() );
}

// the pages' scroll bar arrows: the stock buttons (vgui2 ScrollBar.cpp's ScrollBarButton: its borders and colours,
// held down they keep scrolling) with the arrow a triangle drawn to the button's size, where the stock one is a small
// picture that sat in a corner of the scaled-up buttons
class CScrollArrow : public vgui2::Button
{
	typedef vgui2::Button BaseClass;

public:
	explicit CScrollArrow( bool up ) : BaseClass( NULL, NULL, "" ), m_bUp( up )
	{
		SetButtonActivationType( vgui2::Button::ACTIVATE_ONPRESSED );
	}

	void OnMouseFocusTicked() override
	{
		CallParentFunction( new KeyValues( "MouseFocusTicked" ) );	// the scroll bar keeps scrolling while held
	}

protected:
	void OnMousePressed( vgui2::MouseCode code ) override
	{
		if ( !IsEnabled() || !IsMouseClickEnabled( code ) || !IsUseCaptureMouseEnabled() )
			return;
		SetSelected( true );
		Repaint();
		vgui2::input()->SetMouseCapture( GetVPanel() );
	}

	void OnMouseReleased( vgui2::MouseCode code ) override
	{
		if ( !IsEnabled() || !IsMouseClickEnabled( code ) || !IsUseCaptureMouseEnabled() )
			return;
		SetSelected( false );
		Repaint();
		vgui2::input()->SetMouseCapture( 0 );
	}

	void ApplySchemeSettings( vgui2::IScheme* scheme ) override
	{
		BaseClass::ApplySchemeSettings( scheme );
		SetDefaultBorder( scheme->GetBorder( "ScrollBarButtonBorder" ) );
		SetDepressedBorder( scheme->GetBorder( "ScrollBarButtonDepressedBorder" ) );
		const Color background = GetBgColor();
		SetArmedColor( GetFgColor(), background );
		SetDepressedColor( GetFgColor(), background );
		SetDefaultColor( GetSchemeColor( "DimBaseText", scheme ), background );
		m_Arrow = GetSchemeColor( "DimBaseText", Color( 160, 170, 149, 255 ), scheme );
		m_Lit = GetSchemeColor( "BaseText", Color( 216, 222, 211, 255 ), scheme );
	}

	void Paint() override
	{
		BaseClass::Paint();
		int wide, tall;
		GetSize( wide, tall );
		// a right-angled tip, a pixel wider each side each row: rows = 3/10 of the button
		const int rows = ( wide < tall ? wide : tall ) * 3 / 10 > 2 ? ( wide < tall ? wide : tall ) * 3 / 10 : 2;
		const int cx = ( wide - 1 ) / 2, top = ( tall - rows ) / 2;
		vgui2::surface()->DrawSetColor( IsArmed() || IsDepressed() || IsSelected() ? m_Lit : m_Arrow );
		for ( int row = 0; row < rows; row++ )
		{
			const int y = m_bUp ? top + row : top + rows - 1 - row;
			vgui2::surface()->DrawFilledRect( cx - row, y, cx + row + 1, y + 1 );
		}
	}

private:
	bool m_bUp;
	Color m_Arrow = Color( 160, 170, 149, 255 );
	Color m_Lit = Color( 216, 222, 211, 255 );
};

const int kBannerTall = 120;	// the crosshair tab's banner at its tallest (proportional units)

class CAdvancedPage;
// every page, for lining up the lists of all tabs
std::vector<CAdvancedPage*>& AllPages()
{
	static std::vector<CAdvancedPage*> pages;
	return pages;
}

// one tab: rows of options, read from the cvars on open, written on OK / Apply
class CAdvancedPage : public vgui2::PropertyPage
{
	typedef vgui2::PropertyPage BaseClass;

public:
	CAdvancedPage( vgui2::Panel* parent, const char* name, const Row* rows, int count, const char* note )
		: vgui2::PropertyPage( parent, name, true ), m_pRows( rows ), m_iCount( count ), m_bLoading( false )
	{
		m_bTwoColumns = false;
		for ( int i = 0; i < count; i++ )
		{
			const Row& row = rows[i];
			if ( row.column == 1 )
				m_bTwoColumns = true;
			Control c;
			char control[32];
			snprintf( control, sizeof( control ), "%sRow%d", name, i );
			if ( row.kind == ROW_HEADER )
			{
				// a section's title in the bright text colour, the dialogs' thin line under it
				c.header = new vgui2::Label( this, control, row.text );
				c.header->SetTextColorState( vgui2::Label::CS_BRIGHT );
				c.header->SetContentAlignment( vgui2::Label::a_southwest );
				snprintf( control, sizeof( control ), "%sLine%d", name, i );
				c.divider = new vgui2::Divider( this, control );
			}
			else if ( row.kind == ROW_NOTE )
			{
				c.noteText = row.text;
				c.note = new vgui2::Label( this, control, row.text );
				c.note->SetTextColorState( vgui2::Label::CS_DULL );
				c.note->SetContentAlignment( vgui2::Label::a_northwest );
			}
			else if ( row.kind == ROW_TOGGLE )
			{
				c.toggle = new vgui2::CheckButton( this, control, row.text );
				if ( row.readout )
				{
					snprintf( control, sizeof( control ), "%sReadout%d", name, i );
					c.panel = new CAutoRatePanel( this, control );
				}
				else if ( row.note )
				{
					snprintf( control, sizeof( control ), "%sNote%d", name, i );
					c.noteText = row.note;
					c.note = new vgui2::Label( this, control, row.note );
					c.note->SetTextColorState( vgui2::Label::CS_DULL );
					c.note->SetContentAlignment( vgui2::Label::a_northwest );
				}
			}
			else
			{
				c.label = new vgui2::Label( this, control, row.text );
				c.label->SetContentAlignment( vgui2::Label::a_west );
				if ( row.kind == ROW_CHOICE )
				{
					snprintf( control, sizeof( control ), "%sChoice%d", name, i );
					c.combo = new vgui2::ComboBox( this, control, row.count + 2, false );
					c.label->SetAssociatedControl( c.combo );
				}
				else
				{
					snprintf( control, sizeof( control ), "%sSlider%d", name, i );
					c.slider = new vgui2::Slider( this, control );
					c.slider->SetRange( row.minimum, row.maximum );
					const int steps = row.maximum - row.minimum;
					c.slider->SetNumTicks( steps <= 16 ? ( steps > 0 ? steps : 1 ) : 8 );
					c.label->SetAssociatedControl( c.slider );
					snprintf( control, sizeof( control ), "%sValue%d", name, i );
					c.value = new vgui2::Label( this, control, "" );
					c.value->SetContentAlignment( vgui2::Label::a_east );
				}
			}
			m_controls.push_back( c );
		}
		m_pNote = NULL;
		if ( note )
		{
			m_strNote = note;
			m_pNote = new vgui2::Label( this, "Note", note );
			m_pNote->SetTextColorState( vgui2::Label::CS_DULL );
			m_pNote->SetContentAlignment( vgui2::Label::a_northwest );
		}
		// shown when the rows run past the bottom (PerformLayout)
		m_pScroll = new vgui2::ScrollBar( this, "Scroll", true );
		m_pScroll->SetVisible( false );
		m_pScroll->AddActionSignalTarget( this );
		for ( int i = 0; i < 2; i++ )
		{
			vgui2::Button* stock = m_pScroll->GetButton( i );
			m_pScroll->SetButton( new CScrollArrow( i == 0 ), i );
			if ( stock )
				stock->MarkForDeletion();
		}
		AllPages().push_back( this );
	}

	~CAdvancedPage()
	{
		std::vector<CAdvancedPage*>& pages = AllPages();
		pages.erase( std::remove( pages.begin(), pages.end(), this ), pages.end() );
	}

	// the player preview on the right (Models): the team whose colour box was last used (Terrorist at first), in its
	// solid colour or with the normal skins
	void AddPlayerPreview()
	{
		if ( m_pPreview )
			return;
		m_pPreview = new CPlayerPreview( this, "PlayerPreview" );
		for ( int i = 0; i < m_iCount && m_iPreviewRow < 0; i++ )
		{
			if ( m_pRows[i].cvar && !strnicmp( m_pRows[i].cvar, "cl_teamcolor_", 13 ) )
				m_iPreviewRow = i;
		}
		// under it, while the players list is in use: < and > through every player model, its name between them
		m_pPrevPlayer = new vgui2::Button( this, "PrevPlayer", "<" );
		m_pPrevPlayer->SetCommand( "PrevPlayer" );
		m_pPrevPlayer->AddActionSignalTarget( this );
		m_pNextPlayer = new vgui2::Button( this, "NextPlayer", ">" );
		m_pNextPlayer->SetCommand( "NextPlayer" );
		m_pNextPlayer->AddActionSignalTarget( this );
		m_pPlayerName = new vgui2::Label( this, "PlayerName", "" );
		m_pPlayerName->SetContentAlignment( vgui2::Label::a_center );
		ShowPlayerBrowsing();
		InvalidateLayout( false, false );
	}

	void OnCommand( const char* command ) override
	{
		if ( m_pPreview && ( !stricmp( command, "PrevPlayer" ) || !stricmp( command, "NextPlayer" ) ) )
		{
			m_pPreview->StepModel( !stricmp( command, "NextPlayer" ) ? 1 : -1 );
			ShowPlayerBrowsing();
			return;
		}
		BaseClass::OnCommand( command );
	}

	// the radar preview on the right (Radar): just the radar, as the controls stand
	void AddRadarPreview()
	{
		if ( m_pRadarPreview )
			return;
		m_pRadarPreview = new CRadarPreview( this, "RadarPreview" );
		InvalidateLayout( false, false );
	}

	// the crosshair preview (Crosshair), a banner across the top of the tab (LayoutBanner): the crosshair as the
	// controls stand, at its real size
	void AddCrosshairPreview()
	{
		if ( m_pCrosshairPreview )
			return;
		m_pCrosshairPreview = new CCrosshairPreview( this, "CrosshairPreview" );
		InvalidateLayout( false, false );
	}

	bool IsTwoColumns() const { return m_bTwoColumns; }

	// cvars -> controls (the dialog asks for this each time it opens)
	void OnResetData() override
	{
		m_bLoading = true;
		for ( int i = 0; i < m_iCount; i++ )
		{
			const Row& row = m_pRows[i];
			Control& c = m_controls[i];
			if ( !IsOption( row ) )
				continue;	// below, from its colour list
			const char* current = CvarString( row.cvar );
			if ( c.toggle )
			{
				c.toggle->SetSelected( atof( current ) != 0.0 );
				continue;
			}
			if ( c.slider )
			{
				SetSlider( i, (int)floor( atof( current ) + 0.5 ) );
				continue;
			}
			c.combo->DeleteAllItems();
			c.values.clear();
			c.customId = -1;
			int active = -1;
			for ( int k = 0; k < row.count; k++ )
			{
				std::string label = row.choices[k].label;
				if ( SameValue( row, row.defaultValue, row.choices[k].value ) )
					label += " (Default)";
				const int id = c.combo->AddItem( label.c_str(), NULL );
				SetItemValue( c, id, row.choices[k].value );
				if ( active < 0 && SameValue( row, current, row.choices[k].value ) )
					active = id;
			}
			if ( HasColorParts( i ) )
			{
				// any other colour, made with the sliders
				c.customId = c.combo->AddItem( "Custom", NULL );
				SetItemValue( c, c.customId, current[0] ? current : row.defaultValue );
				if ( active < 0 )
					active = c.customId;
			}
			else if ( active < 0 && current[0] )
			{
				// a value set in the console: keep it as the selected entry
				char text[96];
				snprintf( text, sizeof( text ), "Custom (%s)", current );
				active = c.combo->AddItem( text, NULL );
				SetItemValue( c, active, current );
			}
			if ( active >= 0 )
				c.combo->ActivateItem( active );
		}
		SyncColorParts();
		UpdateEnabled();
		UpdatePreview();
		m_bLoading = false;
	}

	// every option on this tab back to its default: shown at once, written on OK / Apply like any change
	void UseDefaults()
	{
		for ( int i = 0; i < m_iCount; i++ )
		{
			const Row& row = m_pRows[i];
			Control& c = m_controls[i];
			if ( !IsOption( row ) )
				continue;
			if ( c.toggle )
			{
				c.toggle->SetSelected( atof( row.defaultValue ) != 0.0 );
				continue;
			}
			if ( c.slider )
			{
				SetSlider( i, atoi( row.defaultValue ) );
				continue;
			}
			for ( int id = 0; id < (int)c.values.size(); id++ )
			{
				if ( id != c.customId && !c.values[id].empty() && SameValue( row, row.defaultValue, c.values[id].c_str() ) )
				{
					c.combo->ActivateItem( id );
					break;
				}
			}
		}
		SyncColorParts();
		UpdateEnabled();
		UpdatePreview();
		PostActionSignal( new KeyValues( "ApplyButtonEnable" ) );
	}

	// controls -> cvars (OK / Apply)
	void OnApplyChanges() override
	{
		for ( int i = 0; i < m_iCount; i++ )
		{
			const Row& row = m_pRows[i];
			Control& c = m_controls[i];
			if ( !IsOption( row ) )
				continue;	// written as its colour list's value
			const char* current = CvarString( row.cvar );
			if ( c.toggle )
			{
				const bool want = c.toggle->IsSelected();
				if ( ( atof( current ) != 0.0 ) != want )
					gEngfuncs.Cvar_SetValue( (char*)row.cvar, want ? 1.0f : 0.0f );
				continue;
			}
			if ( c.slider )
			{
				const int want = c.slider->GetValue();
				if ( fabs( atof( current ) - want ) > 0.001 || !current[0] )
					gEngfuncs.Cvar_SetValue( (char*)row.cvar, (float)want );
				continue;
			}
			const std::string value = SelectedValue( i );
			if ( value.empty() || SameValue( row, current, value.c_str() ) )
				continue;
			if ( row.words )
				gEngfuncs.Cvar_Set( (char*)row.cvar, (char*)value.c_str() );
			else
				gEngfuncs.Cvar_SetValue( (char*)row.cvar, (float)atof( value.c_str() ) );
		}
	}

	// the longest option name on the one-column tabs, in pixels
	static int WidestLabel()
	{
		int widest = 0;
		for ( CAdvancedPage* page : AllPages() )
		{
			if ( !page->m_bTwoColumns )
				widest = Larger( widest, page->WidestLabelIn( -1 ) );
		}
		return widest;
	}

protected:
	void PerformLayout() override
	{
		BaseClass::PerformLayout();
		// at the full width; rows running past the bottom: again beside a scroll bar (narrower), moved up by its
		// position (the previews stay where they are)
		m_iLayoutWide = GetWide();
		LayOutRows();
		int bottom = RowsBottom();
		const bool scroll = bottom > GetTall();
		if ( scroll )
		{
			m_iLayoutWide = GetWide() - ScrollBarWide();
			LayOutRows();
			bottom = RowsBottom();
		}
		m_pScroll->SetVisible( scroll );
		if ( !scroll )
		{
			m_iScroll = 0;
			return;
		}
		m_pScroll->SetBounds( GetWide() - ScrollBarWide(), 0, ScrollBarWide(), GetTall() );
		m_pScroll->SetRange( 0, bottom );
		m_pScroll->SetRangeWindow( GetTall() );
		m_pScroll->SetButtonPressedScrollValue( GetProportionalScaledValue( 24 ) );
		m_iScroll = Clamp( m_iScroll, 0, bottom - GetTall() );
		if ( m_pScroll->GetValue() != m_iScroll )
			m_pScroll->SetValue( m_iScroll );
		ShiftRows( -m_iScroll );
	}

	void LayOutRows()
	{
		if ( m_pCrosshairPreview )
			LayoutBanner();
		else if ( m_bTwoColumns )
			LayoutTwoColumns();
		else
			LayoutOneColumn();
	}

	// the wheel scrolls a tab that scrolls
	void OnMouseWheeled( int delta ) override
	{
		if ( !m_pScroll || !m_pScroll->IsVisible() )
		{
			BaseClass::OnMouseWheeled( delta );
			return;
		}
		m_pScroll->SetValue( m_pScroll->GetValue() - delta * GetProportionalScaledValue( 30 ) );	// posts ScrollBarSliderMoved
	}

	void OnMessage( const KeyValues* message, vgui2::VPANEL sender ) override
	{
		const char* name = message->GetName();
		if ( !strcmp( name, "ScrollBarSliderMoved" ) )
		{
			if ( m_pScroll && m_pScroll->GetValue() != m_iScroll )
			{
				m_iScroll = m_pScroll->GetValue();
				InvalidateLayout( false, false );
			}
			return;
		}
		if ( !m_bLoading && !strcmp( name, "SliderMoved" ) )
		{
			for ( int i = 0; i < m_iCount; i++ )
			{
				Control& c = m_controls[i];
				if ( !c.slider || c.slider->GetVPanel() != sender )
					continue;
				ShowSliderValue( i );
				if ( m_pRows[i].kind == ROW_COLOR_PART )
					ColorPartMoved( i );
				break;
			}
			UpdatePreview();
			PostActionSignal( new KeyValues( "ApplyButtonEnable" ) );
		}
		else if ( !m_bLoading && ( !strcmp( name, "TextChanged" ) || !strcmp( name, "CheckButtonChecked" ) ) )
		{
			for ( int i = 0; i < m_iCount; i++ )
			{
				const Control& c = m_controls[i];
				vgui2::Panel* control = c.combo ? (vgui2::Panel*)c.combo : (vgui2::Panel*)c.toggle;
				if ( !control || control->GetVPanel() != sender )
					continue;
				// a team colour just picked: the preview shows that one; the weapons: the preview shows a weapon of
				// the set in first person, any other option the player again
				if ( !strnicmp( m_pRows[i].cvar, "cl_teamcolor_", 13 ) )
					m_iPreviewRow = i;
				m_bPreviewWeapons = !stricmp( m_pRows[i].cvar, "cl_weaponmodels" );
				m_bPreviewPlayers = !stricmp( m_pRows[i].cvar, "cl_playermodels" );
			}
			SyncColorParts();
			UpdateEnabled();
			UpdatePreview();
			PostActionSignal( new KeyValues( "ApplyButtonEnable" ) );
		}
		BaseClass::OnMessage( message, sender );
	}

	// a team colour box clicked into (or its list open): the preview shows that team's player
	void OnThink() override
	{
		BaseClass::OnThink();
		if ( !m_pPreview || !IsVisible() )
			return;
		const vgui2::VPANEL focus = vgui2::input()->GetFocus();
		if ( !focus )
			return;
		// a list clicked into: the weapons show a weapon in first person, the players one player model at a time to
		// step through, a team colour that team's player, any other list the players
		for ( int i = 0; i < m_iCount; i++ )
		{
			vgui2::ComboBox* combo = m_controls[i].combo;
			if ( !combo )
				continue;
			const vgui2::VPANEL box = combo->GetVPanel();
			if ( focus != box && !vgui2::ipanel()->HasParent( focus, box ) )
				continue;
			const bool weapons = !stricmp( m_pRows[i].cvar, "cl_weaponmodels" );
			const bool players = !stricmp( m_pRows[i].cvar, "cl_playermodels" );
			const bool team = !strnicmp( m_pRows[i].cvar, "cl_teamcolor_", 13 );
			if ( weapons != m_bPreviewWeapons || players != m_bPreviewPlayers || ( team && i != m_iPreviewRow ) )
			{
				m_bPreviewWeapons = weapons;
				m_bPreviewPlayers = players;
				if ( team )
					m_iPreviewRow = i;
				UpdatePreview();
			}
			break;
		}
	}

private:
	struct Control
	{
		vgui2::Label* label = nullptr;
		vgui2::CheckButton* toggle = nullptr;
		vgui2::ComboBox* combo = nullptr;
		vgui2::Slider* slider = nullptr;
		vgui2::Label* value = nullptr;		// a slider's value
		vgui2::Label* note = nullptr;		// a check box's dim line under it, or a note row's text
		vgui2::Label* header = nullptr;		// a header row's title
		vgui2::Divider* divider = nullptr;	// and the line under it
		std::vector<std::string> values;	// per combo item id
		int customId = -1;					// a colour list's "Custom" item (the sliders' colour)
		CAutoRatePanel* panel = nullptr;	// a check box's readout under it instead of a note (Auto-rate)
		std::string noteText;				// the note as written (laid out in lines that fit)
	};

	static void SetItemValue( Control& c, int id, const char* value )
	{
		if ( id < 0 )
			return;
		if ( id >= (int)c.values.size() )
			c.values.resize( id + 1 );
		c.values[id] = value;
	}

	// the longest name in a column (-1: every row) of this page, in pixels
	int WidestLabelIn( int column )
	{
		int widest = 0;
		for ( int i = 0; i < m_iCount; i++ )
		{
			if ( !m_controls[i].label || ( column >= 0 && m_pRows[i].column != column ) )
				continue;
			int wide = 0, tall = 0;
			m_controls[i].label->GetContentSize( wide, tall );
			widest = Larger( widest, wide );
		}
		return widest;
	}

	// the preview box this page's rows leave room for, on a page 'wide' x 'tall' (pages are laid out one at a time, so
	// the size is passed in)
	int PreviewRoomFor( int wide, int tall )
	{
		const int margin = GetProportionalScaledValue( 12 );
		int room;
		if ( m_bTwoColumns )
			room = wide - 3 * margin - TwoColumnNeed();
		else
			room = wide - 2 * margin - ( OneColumnControlX() + GetProportionalScaledValue( 200 ) );
		const int below = m_pPrevPlayer ? BrowseGap() + BrowseTall() : 0;	// the player buttons
		return Smaller( tall - 2 * margin - below, room );
	}

	int BrowseGap()
	{
		return GetProportionalScaledValue( 4 );
	}

	int BrowseTall()
	{
		return GetProportionalScaledValue( 24 );
	}

	// the preview box of every tab: the smallest any tab with a preview allows, so they all match
	static int SharedPreviewSide( CAdvancedPage* layingOut )
	{
		int side = 1 << 30;
		for ( CAdvancedPage* page : AllPages() )
		{
			if ( page->m_pPreview || page->m_pRadarPreview )	// the crosshair's is a banner (LayoutBanner)
				side = Smaller( side, page->PreviewRoomFor( layingOut->GetWide(), layingOut->GetTall() ) );
		}
		return Larger( side, layingOut->GetProportionalScaledValue( 80 ) );
	}

	// the preview (if any) in the top right corner; returns where the rows must end
	int PlacePreview()
	{
		const int margin = GetProportionalScaledValue( 12 );
		vgui2::Panel* preview = m_pPreview;
		if ( !preview )
			preview = m_pRadarPreview;
		if ( !preview )
			return LayoutWide() - margin;
		const int side = SharedPreviewSide( this );
		const int previewX = LayoutWide() - margin - side;
		preview->SetBounds( previewX, margin, side, side );
		if ( preview == m_pPreview && m_pPrevPlayer )
		{
			// < at the left edge, > at the right, the name between them
			const int tall = BrowseTall(), wide = tall * 3 / 2, y = margin + side + BrowseGap();
			m_pPrevPlayer->SetBounds( previewX, y, wide, tall );
			m_pNextPlayer->SetBounds( previewX + side - wide, y, wide, tall );
			m_pPlayerName->SetBounds( previewX + wide, y, Larger( side - 2 * wide, 0 ), tall );
		}
		return previewX - margin;
	}

	int OneColumnControlX()
	{
		return GetProportionalScaledValue( 12 ) + WidestLabel() + GetProportionalScaledValue( 12 );
	}

	// the width the two columns need at the least (names, smallest sliders with their values, the gaps)
	int TwoColumnNeed()
	{
		const int gap = GetProportionalScaledValue( 10 );
		const int minimumControl = GetProportionalScaledValue( 80 ) + GetProportionalScaledValue( 6 ) + ValueWidth();
		return WidestLabelIn( 0 ) + WidestLabelIn( 1 ) + 2 * ( gap + minimumControl ) + GetProportionalScaledValue( 18 );
	}

	// names at the left edge, the lists right after the longest name of any one-column tab, check boxes at the left
	// edge, the preview on the right
	void LayoutOneColumn()
	{
		const int margin = GetProportionalScaledValue( 12 );
		const int labelX = margin;
		const int controlX = OneColumnControlX();
		const int comboW = GetProportionalScaledValue( 200 );
		const int controlTall = GetProportionalScaledValue( 24 );
		const int textRight = PlacePreview();
		int y = GetProportionalScaledValue( m_iCount > 0 && m_pRows[0].kind == ROW_HEADER ? 10 : 16 );
		for ( int i = 0; i < m_iCount; i++ )
		{
			Control& c = m_controls[i];
			if ( c.header )
			{
				// a section: a little room above it when something comes before, the title, the line under it
				if ( i > 0 )
					y += GetProportionalScaledValue( 6 );
				c.header->SetBounds( labelX, y, textRight - labelX, controlTall );
				c.divider->SetBounds( labelX, y + controlTall + GetProportionalScaledValue( 1 ), textRight - labelX, 2 );
				y += controlTall + GetProportionalScaledValue( 8 );
				continue;
			}
			if ( m_pRows[i].kind == ROW_NOTE )
			{
				SetWrappedText( c.note, c.noteText, textRight - labelX );
				int noteWide = 0, noteTall = 0;
				c.note->GetContentSize( noteWide, noteTall );
				c.note->SetBounds( labelX, y, textRight - labelX, noteTall );
				y += noteTall + GetProportionalScaledValue( 8 );
				continue;
			}
			if ( c.toggle )
			{
				c.toggle->SetBounds( labelX, y, textRight - labelX, controlTall );
				y += GetProportionalScaledValue( 30 ) + PlaceNote( c, labelX, y + controlTall, textRight - labelX );
				continue;
			}
			c.label->SetBounds( labelX, y, controlX - labelX, controlTall );
			PlaceControl( c, controlX, y, comboW, controlTall );
			y += GetProportionalScaledValue( 34 );
		}
		// the dim note: right under the last option, wrapping before the preview / margin
		PlacePageNote( labelX, y + GetProportionalScaledValue( 2 ), textRight - labelX );
	}

	// two columns of named rows (each with its own name width), the check boxes two to a row under both, the preview
	// on the right
	void LayoutTwoColumns()
	{
		const int margin = GetProportionalScaledValue( 12 );
		const int gap = GetProportionalScaledValue( 10 );
		const int columnGap = GetProportionalScaledValue( 18 );
		const int controlTall = GetProportionalScaledValue( 24 );
		const int pitch = GetProportionalScaledValue( 30 );
		const int labelW[2] = { WidestLabelIn( 0 ), WidestLabelIn( 1 ) };
		const int textRight = PlacePreview();
		const int columnWide = ( textRight - margin - columnGap ) / 2;
		const int columnX[2] = { margin, margin + columnWide + columnGap };
		int y[2] = { GetProportionalScaledValue( 14 ), GetProportionalScaledValue( 14 ) };
		std::vector<int> toggles;
		for ( int i = 0; i < m_iCount; i++ )
		{
			Control& c = m_controls[i];
			if ( c.toggle )
			{
				toggles.push_back( i );
				continue;
			}
			if ( !c.label )
				continue;	// headers and notes are for one-column tabs
			const int column = m_pRows[i].column == 1 ? 1 : 0;
			const int x = columnX[column];
			c.label->SetBounds( x, y[column], labelW[column] + gap, controlTall );
			const int controlX = x + labelW[column] + gap;
			PlaceControl( c, controlX, y[column], x + columnWide - controlX, controlTall );
			y[column] += pitch;
		}
		int toggleY = Larger( y[0], y[1] ) + GetProportionalScaledValue( 4 );
		const int half = ( textRight - margin ) / 2;
		int noteTall = 0;	// the taller note of the pair
		for ( size_t k = 0; k < toggles.size(); k++ )
		{
			const int x = ( k & 1 ) ? margin + half : margin;
			Control& c = m_controls[toggles[k]];
			c.toggle->SetBounds( x, toggleY, half, controlTall );
			noteTall = Larger( noteTall, PlaceNote( c, x, toggleY + controlTall, half ) );
			if ( k & 1 )
			{
				toggleY += GetProportionalScaledValue( 28 ) + noteTall;
				noteTall = 0;
			}
		}
		if ( toggles.size() & 1 )
			toggleY += GetProportionalScaledValue( 28 ) + noteTall;
		PlacePageNote( margin, toggleY + GetProportionalScaledValue( 2 ), textRight - margin );
	}

	int ValueWidth()
	{
		return GetProportionalScaledValue( 34 );
	}

	// the crosshair tab: the preview a banner across the top, the rows in three columns under it (row.column 0, 1, 2;
	// each column's header over it). The banner is as tall as it may be, up to kBannerTall, while all the rows fit.
	void LayoutBanner()
	{
		const int margin = GetProportionalScaledValue( 12 );
		const int gap = GetProportionalScaledValue( 10 );
		const int columnGap = GetProportionalScaledValue( 18 );
		const int controlTall = GetProportionalScaledValue( 24 );
		const int pitch = GetProportionalScaledValue( 30 );
		const int togglePitch = GetProportionalScaledValue( 28 );
		const int headerPitch = controlTall + GetProportionalScaledValue( 8 );
		const int bannerTop = GetProportionalScaledValue( 6 );
		const int bannerGap = GetProportionalScaledValue( 10 );
		// the rows' height: the tallest column
		int need[3] = { 0, 0, 0 };
		for ( int i = 0; i < m_iCount; i++ )
		{
			const Control& c = m_controls[i];
			const int column = Clamp( m_pRows[i].column, 0, 2 );
			need[column] += c.header ? headerPitch : c.toggle ? togglePitch : c.label ? pitch : 0;
		}
		const int rowsTall = Larger( need[0], Larger( need[1], need[2] ) );
		int bannerTall = Clamp( GetTall() - bannerTop - bannerGap - rowsTall - margin, GetProportionalScaledValue( 60 ),
			GetProportionalScaledValue( kBannerTall ) );
		bannerTall = Smaller( Larger( bannerTall, 2 * m_iCrosshairReach + GetProportionalScaledValue( 8 ) ),
			CCrosshairPreview::MostTall() );
		m_pCrosshairPreview->SetBounds( GetProportionalScaledValue( 2 ), bannerTop, LayoutWide() - GetProportionalScaledValue( 4 ),
			bannerTall );
		// the three columns
		const int top = bannerTop + bannerTall + bannerGap;
		const int columnWide = ( LayoutWide() - 2 * margin - 2 * columnGap ) / 3;
		const int labelW[3] = { WidestLabelIn( 0 ), WidestLabelIn( 1 ), WidestLabelIn( 2 ) };
		int y[3] = { top, top, top };
		for ( int i = 0; i < m_iCount; i++ )
		{
			Control& c = m_controls[i];
			const int column = Clamp( m_pRows[i].column, 0, 2 );
			const int x = margin + column * ( columnWide + columnGap );
			if ( c.header )
			{
				c.header->SetBounds( x, y[column], columnWide, controlTall );
				c.divider->SetBounds( x, y[column] + controlTall + GetProportionalScaledValue( 1 ), columnWide, 2 );
				y[column] += headerPitch;
			}
			else if ( c.toggle )
			{
				c.toggle->SetBounds( x, y[column], columnWide, controlTall );
				y[column] += togglePitch + PlaceNote( c, x, y[column] + controlTall, columnWide );
			}
			else if ( c.label )
			{
				c.label->SetBounds( x, y[column], labelW[column] + gap, controlTall );
				const int controlX = x + labelW[column] + gap;
				PlaceControl( c, controlX, y[column], x + columnWide - controlX, controlTall );
				y[column] += pitch;
			}
		}
		PlacePageNote( margin, Larger( y[0], Larger( y[1], y[2] ) ) + GetProportionalScaledValue( 2 ),
			LayoutWide() - 2 * margin );
	}

	// the page's dim note at x, y in lines that fit 'wide'
	void PlacePageNote( int x, int y, int wide )
	{
		if ( !m_pNote )
			return;
		SetWrappedText( m_pNote, m_strNote, wide );
		int noteWide = 0, noteTall = 0;
		m_pNote->GetContentSize( noteWide, noteTall );
		m_pNote->SetBounds( x, y, wide, noteTall );
	}

	// the width the rows are laid out at: the page's, less the scroll bar when there is one
	int LayoutWide()
	{
		return m_iLayoutWide > 0 ? m_iLayoutWide : GetWide();
	}

	int ScrollBarWide()
	{
		return Larger( GetProportionalScaledValue( 10 ), 14 );
	}

	// every part of every row and the page's note (not the previews, which stay where they are)
	template <typename F>
	void ForEachRowPart( F f )
	{
		for ( Control& c : m_controls )
		{
			vgui2::Panel* parts[] = { c.label, c.toggle, c.combo, c.slider, c.value, c.note, c.header, c.divider, c.panel };
			for ( vgui2::Panel* part : parts )
			{
				if ( part )
					f( part );
			}
		}
		if ( m_pNote )
			f( m_pNote );
	}

	// where the rows end, with the margin under them
	int RowsBottom()
	{
		int bottom = 0;
		ForEachRowPart( [&bottom]( vgui2::Panel* part ) {
			if ( part->IsVisible() )
			{
				int x, y, wide, tall;
				part->GetBounds( x, y, wide, tall );
				bottom = Larger( bottom, y + tall );
			}
		} );
		return bottom + GetProportionalScaledValue( 12 );
	}

	void ShiftRows( int dy )
	{
		if ( !dy )
			return;
		ForEachRowPart( [dy]( vgui2::Panel* part ) {
			int x, y;
			part->GetPos( x, y );
			part->SetPos( x, y + dy );
		} );
	}

	// a check box's note or readout (if any) at 'top', lined up with the box's name, within the box's 'wide' from
	// 'x'; returns the height it takes (0 without one)
	int PlaceNote( Control& c, int x, int top, int wide )
	{
		if ( !c.note && !c.panel )
			return 0;
		// the name starts after the box picture (and the text inset): the box's content minus its text
		int contentWide = 0, contentTall = 0, textWide = 0, textTall = 0;
		c.toggle->GetContentSize( contentWide, contentTall );
		c.toggle->GetTextImage()->GetContentSize( textWide, textTall );
		const int noteX = x + Clamp( contentWide - textWide, 0, wide );
		if ( c.panel )
		{
			const int gap = GetProportionalScaledValue( 4 );
			const int panelTall = c.panel->WantedTall();
			c.panel->SetBounds( noteX, top + gap, Larger( x + wide - noteX, 1 ), panelTall );
			return gap + panelTall + gap;
		}
		SetWrappedText( c.note, c.noteText, Larger( x + wide - noteX, 1 ) );
		int noteWide = 0, noteTall = 0;
		c.note->GetContentSize( noteWide, noteTall );
		c.note->SetBounds( noteX, top, Larger( x + wide - noteX, 1 ), noteTall );
		return noteTall;
	}

	// a list across 'wide', or a slider with its value at the right end
	void PlaceControl( Control& c, int x, int y, int wide, int tall )
	{
		if ( c.combo )
			c.combo->SetBounds( x, y, wide, tall );
		if ( c.slider )
		{
			const int valueWide = ValueWidth();
			c.slider->SetBounds( x, y, Larger( wide - valueWide - GetProportionalScaledValue( 6 ), GetProportionalScaledValue( 40 ) ), tall );
			c.value->SetBounds( x + wide - valueWide, y, valueWide, tall );
		}
	}

	void SetSlider( int index, int value )
	{
		Control& c = m_controls[index];
		c.slider->SetValue( Clamp( value, m_pRows[index].minimum, m_pRows[index].maximum ), false );
		ShowSliderValue( index );
	}

	void ShowSliderValue( int index )
	{
		Control& c = m_controls[index];
		const int value = c.slider->GetValue();
		char text[16];
		if ( m_pRows[index].valueText == VALUE_OFF_ZERO && value == 0 )
			snprintf( text, sizeof( text ), "Off" );
		else if ( m_pRows[index].valueText == VALUE_ALPHA )
		{
			if ( value <= 0 )
				snprintf( text, sizeof( text ), "Fade" );
			else
				snprintf( text, sizeof( text ), "%d%%", Larger( ( value * 100 + 127 ) / 255, 1 ) );
		}
		else if ( m_pRows[index].valueText == VALUE_SIGNED && value > 0 )
			snprintf( text, sizeof( text ), "+%d", value );
		else
			snprintf( text, sizeof( text ), "%d", value );
		c.value->SetText( text );
	}

	// does a colour list have red / green / blue sliders on this page?
	bool HasColorParts( int index )
	{
		for ( int i = 0; i < m_iCount; i++ )
		{
			if ( m_pRows[i].kind == ROW_COLOR_PART && !stricmp( m_pRows[i].cvar, m_pRows[index].cvar ) )
				return true;
		}
		return false;
	}

	// the colour list a colour part belongs to
	int ColorListOf( int part )
	{
		for ( int i = 0; i < m_iCount; i++ )
		{
			if ( m_pRows[i].kind == ROW_CHOICE && !stricmp( m_pRows[i].cvar, m_pRows[part].cvar ) )
				return i;
		}
		return -1;
	}

	// the sliders show their colour list's colour
	void SyncColorParts()
	{
		for ( int i = 0; i < m_iCount; i++ )
		{
			if ( m_pRows[i].kind != ROW_COLOR_PART )
				continue;
			const int list = ColorListOf( i );
			int rgb[3] = { 0, 0, 0 };
			if ( list < 0 || !ParseColor( SelectedValue( list ).c_str(), rgb ) )
				continue;
			SetSlider( i, rgb[Clamp( m_pRows[i].component, 0, 2 )] );
		}
	}

	// a red / green / blue slider moved: its list shows the colour (a ready one if it is one, else "Custom")
	void ColorPartMoved( int part )
	{
		const int list = ColorListOf( part );
		if ( list < 0 )
			return;
		int rgb[3] = { 0, 0, 0 };
		ParseColor( SelectedValue( list ).c_str(), rgb );
		for ( int i = 0; i < m_iCount; i++ )
		{
			if ( m_pRows[i].kind == ROW_COLOR_PART && ColorListOf( i ) == list )
				rgb[Clamp( m_pRows[i].component, 0, 2 )] = m_controls[i].slider->GetValue();
		}
		char color[32];
		snprintf( color, sizeof( color ), "%d %d %d", rgb[0], rgb[1], rgb[2] );
		Control& c = m_controls[list];
		int pick = -1;
		for ( int id = 0; id < (int)c.values.size() && pick < 0; id++ )
		{
			if ( id != c.customId && SameColor( c.values[id].c_str(), color ) )
				pick = id;
		}
		if ( pick < 0 )
		{
			pick = c.customId;
			SetItemValue( c, pick, color );
		}
		if ( pick >= 0 && c.combo->GetActiveItem() != pick )
			c.combo->ActivateItem( pick );
	}

	std::string SelectedValue( int index )
	{
		Control& c = m_controls[index];
		const int id = c.combo ? c.combo->GetActiveItem() : -1;
		if ( id < 0 || id >= (int)c.values.size() )
			return std::string();
		return c.values[id];
	}

	// is row 'index' on, as the controls stand now (not yet applied)?
	bool RowOn( int index )
	{
		Control& c = m_controls[index];
		if ( c.toggle )
			return c.toggle->IsSelected();
		if ( c.slider )
			return c.slider->GetValue() != 0;
		const std::string value = SelectedValue( index );
		return !value.empty() && atof( value.c_str() ) != 0.0;
	}

	// disable (box and name) the options that do nothing with the current choices (or on this screen)
	void UpdateEnabled()
	{
		int screenWide = 0, screenTall = 0;
		vgui2::surface()->GetScreenSize( screenWide, screenTall );
		const bool widescreen = StretchView_WorksOn( screenWide, screenTall );
		for ( int i = 0; i < m_iCount; i++ )
		{
			const bool on = ( m_pRows[i].needs < 0 || RowOn( m_pRows[i].needs ) ) && ( !m_pRows[i].widescreenOnly || widescreen );
			Control& c = m_controls[i];
			vgui2::Panel* parts[5] = { c.toggle, c.label, c.combo, c.slider, c.value };
			for ( vgui2::Panel* part : parts )
			{
				if ( part && part->IsEnabled() != on )
					part->SetEnabled( on );
			}
		}
	}

	// the row of a cvar on this page (not a colour part), -1 when it has none
	int RowOf( const char* cvar )
	{
		for ( int i = 0; i < m_iCount; i++ )
		{
			if ( m_pRows[i].kind != ROW_COLOR_PART && !stricmp( m_pRows[i].cvar, cvar ) )
				return i;
		}
		return -1;
	}

	// a number row's value (list or slider) as the controls stand now
	float RowNumber( const char* cvar, float fallback )
	{
		const int row = RowOf( cvar );
		if ( row >= 0 && m_controls[row].slider )
			return (float)m_controls[row].slider->GetValue();
		const std::string value = row >= 0 ? SelectedValue( row ) : std::string();
		return value.empty() ? fallback : (float)atof( value.c_str() );
	}

	// a check box row as the controls stand now
	bool RowToggle( const char* cvar, bool fallback )
	{
		const int row = RowOf( cvar );
		return row >= 0 ? RowOn( row ) : fallback;
	}

	// the previews as the controls stand now (not yet applied)
	void UpdatePreview()
	{
		if ( m_pRadarPreview )
		{
			const int style = RowOf( "cl_radar_square" ), rotate = RowOf( "cl_radar_rotate" );
			m_pRadarPreview->SetSettings( style < 0 || RowOn( style ), RowNumber( "cl_radar_size", 150.0f ),
				RowNumber( "cl_radar_range", 1000.0f ), RowNumber( "cl_radar_alpha", RADAR_MAP_OPACITY_DEFAULT_VALUE ),
				rotate < 0 || RowOn( rotate ) );
		}
		if ( m_pCrosshairPreview )
		{
			CrosshairSettings settings;
			memset( &settings, 0, sizeof( settings ) );
			int rgb[3] = { 50, 250, 50 };
			const int color = RowOf( "cl_crosshair_rgb" );
			if ( color >= 0 && !ParseColor( SelectedValue( color ).c_str(), rgb ) )
			{
				rgb[0] = 50;
				rgb[1] = 250;
				rgb[2] = 50;
			}
			settings.r = Clamp( rgb[0], 0, 255 );
			settings.g = Clamp( rgb[1], 0, 255 );
			settings.b = Clamp( rgb[2], 0, 255 );
			settings.size = 0;	// always "auto" (ammo.cpp): the length sizes it
			settings.alpha = (int)RowNumber( "cl_crosshair_alpha", 0.0f );
			settings.thickness = (int)RowNumber( "cl_crosshair_thickness", 1.0f );
			settings.length = (int)RowNumber( "cl_crosshair_length", 0.0f );
			settings.gap = (int)RowNumber( "cl_crosshair_gap", 0.0f );
			settings.outline = (int)RowNumber( "cl_crosshair_outline", 0.0f );
			settings.dot = (int)RowNumber( "cl_crosshair_dot", 0.0f );
			settings.translucent = RowToggle( "cl_crosshair_additive", true );
			settings.tshape = RowToggle( "cl_crosshair_t", false );
			settings.dynamic = RowToggle( "cl_dynamiccrosshair", true );
			settings.recoil = RowToggle( "cl_crosshair_recoil", true );
			settings.sniper = RowToggle( "cl_crosshair_sniper", false );
			m_pCrosshairPreview->SetSettings( settings );
			int screenWide = 0, screenTall = 0;
			vgui2::surface()->GetScreenSize( screenWide, screenTall );
			const int reach = Crosshair_PreviewReach( settings, screenWide );
			if ( reach != m_iCrosshairReach )
			{
				m_iCrosshairReach = reach;	// the banner as tall as the crosshair reaches (LayoutBanner)
				InvalidateLayout( false, false );
			}
		}
		// the player preview: solid team colours on or off and each team's colour
		if ( !m_pPreview )
			return;
		bool solid = false;
		for ( int i = 0; i < m_iCount; i++ )
		{
			if ( !stricmp( m_pRows[i].cvar, "cl_teamcolors" ) )
				solid = RowOn( i );
		}
		// each team's colour (the players stand side by side, each in its own)
		int colors[3][3] = { { 255, 0, 0 }, { 0, 0, 255 }, { 255, 255, 0 } };	// by CPlayerPreview::Team
		const char* const teamRows[3] = { "cl_teamcolor_t", "cl_teamcolor_ct", "cl_teamcolor_vip" };
		for ( int team = 0; team < 3; team++ )
		{
			const int row = RowOf( teamRows[team] );
			int rgb[3];
			if ( row >= 0 && sscanf( SelectedValue( row ).c_str(), "%d %d %d", &rgb[0], &rgb[1], &rgb[2] ) == 3 )
				memcpy( colors[team], rgb, sizeof( rgb ) );
		}
		m_pPreview->SetLook( solid, colors );
		// and the head colour over it
		int head[3] = { 0, 0, 0 };
		const int headRow = RowOf( "cl_headcolor" );
		const bool headOn = solid && headRow >= 0 && ParseColor( SelectedValue( headRow ).c_str(), head );	// only with solid team colours
		m_pPreview->SetHeadColor( headOn, head[0], head[1], head[2] );
		// the weapon models' set (in the player's hand, and the first-person weapon while the weapons list is in use)
		const int setRow = RowOf( "cl_weaponmodels" );
		const std::string set = setRow >= 0 ? SelectedValue( setRow ) : std::string();
		m_pPreview->SetWeapons( !stricmp( set.c_str(), "retail" ) ? 1 : !stricmp( set.c_str(), "beta" ) ? 2 : 0,
			m_bPreviewWeapons );
		// the player models' set (the player shown)
		const int playerRow = RowOf( "cl_playermodels" );
		const std::string players = playerRow >= 0 ? SelectedValue( playerRow ) : std::string();
		m_pPreview->SetPlayers( !stricmp( players.c_str(), "retail" ) ? 1 : !stricmp( players.c_str(), "beta" ) ? 2 : 0 );
		ShowPlayerBrowsing();
	}

	// one player model at a time with the < > buttons and its name while the players list is in use, else neither
	void ShowPlayerBrowsing()
	{
		if ( !m_pPreview || !m_pPrevPlayer )
			return;
		const bool browsing = m_bPreviewPlayers && !m_bPreviewWeapons;
		m_pPreview->SetBrowsing( browsing );
		m_pPrevPlayer->SetVisible( browsing );
		m_pNextPlayer->SetVisible( browsing );
		m_pPlayerName->SetVisible( browsing );
		char name[64];
		m_pPreview->GetBrowsedName( name, sizeof( name ) );
		m_pPlayerName->SetText( name );
	}

	// the player a team colour row shows
	static CPlayerPreview::Team TeamOf( const char* cvar )
	{
		if ( !stricmp( cvar, "cl_teamcolor_ct" ) )
			return CPlayerPreview::TEAM_CT;
		if ( !stricmp( cvar, "cl_teamcolor_vip" ) )
			return CPlayerPreview::TEAM_VIP;
		return CPlayerPreview::TEAM_T;
	}

	const Row* m_pRows;
	int m_iCount;
	bool m_bLoading;
	bool m_bTwoColumns;
	std::vector<Control> m_controls;
	vgui2::Label* m_pNote;
	CPlayerPreview* m_pPreview = nullptr;
	int m_iPreviewRow = -1;	// the team colour row the preview shows
	bool m_bPreviewWeapons = false;	// the preview shows a weapon in first person (the weapons list is in use)
	bool m_bPreviewPlayers = false;	// the preview steps through the player models (the players list is in use)
	vgui2::Button* m_pPrevPlayer = nullptr;
	vgui2::Button* m_pNextPlayer = nullptr;
	vgui2::Label* m_pPlayerName = nullptr;
	CRadarPreview* m_pRadarPreview = nullptr;
	CCrosshairPreview* m_pCrosshairPreview = nullptr;
	int m_iCrosshairReach = 0;			// how far the preview's crosshair reaches from its middle (pixels)
	std::string m_strNote;				// the page's note as written (laid out in lines that fit)
	vgui2::ScrollBar* m_pScroll = nullptr;
	int m_iScroll = 0;					// how far the rows are scrolled up (pixels)
	int m_iLayoutWide = 0;				// the width the rows are laid out at (the page's less the scroll bar's)
};

// the dialog's size in its HD-proportional units (the Options window is 522 x 444): its first size (the Crosshair tab's
// banner at its tallest over its three columns), and the smallest it can be made (the columns under a lower banner and
// the other tabs' rows beside a smaller preview); both kept inside the screen
const int kDialogWide = 760;
const int kDialogTall = 444;
const int kMinimumWide = 640;
const int kMinimumTall = 400;

cvar_t* g_pSizeCvar;	// cl_advanced_size: the window's size in those units, "wide tall" ("" = the first size)

class CAdvancedDialog : public vgui2::PropertyDialog
{
	typedef vgui2::PropertyDialog BaseClass;

public:
	CAdvancedDialog() : vgui2::PropertyDialog( NULL, "ShadowsAdvancedDialog" )
	{
		// the main menu's scheme and the Options window's sizing (OptionsDialog.cpp: HD proportional)
		SetScheme( "BaseUI" );
		SetHDProportional( true );
		SetBounds( 0, 0, GetProportionalScaledValue( kDialogWide ), GetProportionalScaledValue( kDialogTall ) );
		SetTitle( "Advanced", true );
		SetCloseSound( "sound/UI/launch_dnmenu1.wav" );	// the X, like the game's own windows (OptionsDialog.cpp)
		SetDeleteSelfOnClose( false );
		SetSizeable( true );
		// before AddPage: each tab takes the width it is created with as its minimum
		GetPropertySheet()->SetTabWidth( GetProportionalScaledValue( 72 ) );

		m_pPages[0] = new CAdvancedPage( this, "Models", kModelRows, COUNT_OF( kModelRows ), NULL );	// its note is a row
		m_pPages[1] = new CAdvancedPage( this, "Radar", kRadarRows, COUNT_OF( kRadarRows ),
			"Size, range, opacity and rotation are for the enhanced radar." );
		m_pPages[2] = new CAdvancedPage( this, "FX", kFxRows, COUNT_OF( kFxRows ), NULL );
		m_pPages[3] = new CAdvancedPage( this, "Crosshair", kCrosshairRows, COUNT_OF( kCrosshairRows ), NULL );
		m_pPages[4] = new CAdvancedPage( this, "Misc", kMiscRows, COUNT_OF( kMiscRows ),
			"Auto-delete removes only auto-recorded demos." );
		m_pAbout = new CAboutPage( this, "About" );	// the client and its author (about_page.cpp)
		m_pPages[0]->AddPlayerPreview();
		m_pPages[1]->AddRadarPreview();
		m_pPages[3]->AddCrosshairPreview();
		// the tabs in their order (the first is the one shown when the window opens)
		AddPage( m_pPages[3], "Crosshair" );
		AddPage( m_pPages[0], "Models" );
		AddPage( m_pPages[1], "Radar" );
		AddPage( m_pPages[2], "FX" );
		AddPage( m_pPages[4], "Misc" );
		AddPage( m_pAbout, "About" );
		SetApplyButtonVisible( true );

		// bottom left, level with OK / Cancel / Apply
		m_pUseDefaults = new vgui2::Button( this, "UseDefaults", "Use Defaults" );
		m_pUseDefaults->SetCommand( "UseDefaults" );
		m_pUseDefaults->AddActionSignalTarget( this );
	}

	// fresh values from the cvars, the kept size (resolution may have changed since), centred, on top
	void Open()
	{
		int screenWide = 0, screenTall = 0;
		vgui2::surface()->GetScreenSize( screenWide, screenTall );
		const int roomWide = Larger( screenWide - GetProportionalScaledValue( 16 ), 1 );
		const int roomTall = Larger( screenTall - GetProportionalScaledValue( 16 ), 1 );
		SetMinimumSize( Smaller( GetProportionalScaledValue( kMinimumWide ), roomWide ),
			Smaller( GetProportionalScaledValue( kMinimumTall ), roomTall ) );
		int wide = kDialogWide, tall = kDialogTall;
		if ( g_pSizeCvar && g_pSizeCvar->string )
		{
			int keptWide = 0, keptTall = 0;
			if ( sscanf( g_pSizeCvar->string, "%d %d", &keptWide, &keptTall ) == 2 && keptWide >= kMinimumWide
				 && keptTall >= kMinimumTall )
			{
				wide = keptWide;
				tall = keptTall;
			}
		}
		SetSize( Smaller( GetProportionalScaledValue( wide ), roomWide ), Smaller( GetProportionalScaledValue( tall ), roomTall ) );
		Crosshair_TakeOverCvars();	// the crosshair's own colour cvars filled before they are read
		ResetAllData();
		EnableApplyButton( false );
		SetVisible( true );
		Activate();
		MoveToCenterOfScreen();
		MoveToFront();
		RequestFocus( 0 );
	}

protected:
	void PerformLayout() override
	{
		BaseClass::PerformLayout();
		if ( !m_pUseDefaults )
			return;
		// same row and height as PropertyDialog's OK / Cancel / Apply, at the sheet's left edge
		int x, y, wide, tall;
		GetClientArea( x, y, wide, tall );
		m_pUseDefaults->SetBounds( x, y + tall - GetProportionalScaledValue( 28 ), GetProportionalScaledValue( 100 ),
			GetProportionalScaledValue( 24 ) );
	}

	// keep the size (in the units, so it fits other resolutions too)
	void OnClose() override
	{
		const double unit = GetProportionalScaledValue( 1000 ) / 1000.0;
		if ( g_pSizeCvar && unit > 0.0 )
		{
			char size[32];
			snprintf( size, sizeof( size ), "%d %d", (int)( GetWide() / unit + 0.5 ), (int)( GetTall() / unit + 0.5 ) );
			if ( strcmp( size, g_pSizeCvar->string ? g_pSizeCvar->string : "" ) )
				gEngfuncs.Cvar_Set( (char*)"cl_advanced_size", size );
		}
		BaseClass::OnClose();
	}

	void OnCommand( const char* command ) override
	{
		if ( !stricmp( command, "UseDefaults" ) )
		{
			for ( int i = 0; i < kPages; i++ )
			{
				if ( m_pPages[i] )
					m_pPages[i]->UseDefaults();
			}
			return;
		}
		BaseClass::OnCommand( command );
	}

	void OnKeyCodeTyped( vgui2::KeyCode code ) override
	{
		if ( code == vgui2::KEY_ESCAPE )
		{
			Close();
			return;
		}
		BaseClass::OnKeyCodeTyped( code );
	}

private:
	static const int kPages = 5;
	CAdvancedPage* m_pPages[kPages] = {};
	CAboutPage* m_pAbout = nullptr;
	vgui2::Button* m_pUseDefaults = nullptr;
};

CAdvancedDialog* g_pAdvancedDialog;

// one of the engine's top-level panels ("BaseGameUIPanel" holds the main menu, "BaseClientPanel" the game's)
vgui2::VPANEL FindEnginePanel( const char* name )
{
	const vgui2::VPANEL root = vgui2::surface()->GetEmbeddedPanel();
	if ( !root )
		return 0;
	const int count = vgui2::ipanel()->GetChildCount( root );
	for ( int i = 0; i < count; i++ )
	{
		const vgui2::VPANEL child = vgui2::ipanel()->GetChild( root, i );
		const char* childName = child ? vgui2::ipanel()->GetName( child ) : NULL;
		if ( childName && !strcmp( childName, name ) )
			return child;
	}
	return 0;
}

void CmdAdvanced()
{
	// the window belongs to the main menu: the engine hides the game's panels while the menu is open
	const vgui2::VPANEL parent = FindEnginePanel( "BaseGameUIPanel" );
	if ( !parent )
	{
		gEngfuncs.Con_Printf( "Advanced: the main menu panel was not found\n" );
		return;
	}
	if ( !g_pAdvancedDialog )
	{
		g_pAdvancedDialog = new CAdvancedDialog();
		g_pAdvancedDialog->SetParent( parent );
		g_pAdvancedDialog->MakePopup( false, false );
	}
	else
		g_pAdvancedDialog->SetParent( parent );
	if ( !vgui2::ipanel()->IsVisible( parent ) && g_pBaseUI )
		g_pBaseUI->ActivateGameUI();
	if ( g_pAdvancedDialog->IsVisible() )
		g_pAdvancedDialog->MoveToFront();	// open already (the menu item opens it before its queued cl_advanced runs)
	else
		g_pAdvancedDialog->Open();
	if ( !vgui2::ipanel()->IsVisible( parent ) )
		gEngfuncs.Con_Printf( "Advanced: open the main menu (Esc) to see the window\n" );
}

} // namespace

// The game menu's "Advanced" item (GameMenu.res: "engine cl_advanced"). In a game GameUI hides its menu right after it
// queues the command, which runs only the next frame: one frame of the bare game showed between the menu and the window
// (a flicker). The engine tells the client when its menu hides (CounterStrikeViewport::ActivateClientUI), before that
// frame is drawn: when a click on that item (GameUI names it after its label) is what hid it, the menu comes straight
// back and the window opens now. True when it did.
bool AdvancedOptions_OpenFromGameMenu()
{
	vgui2::IInput* input = vgui2::input();
	if ( !input || !vgui2::ipanel() )
		return false;
	if ( !input->WasMousePressed( vgui2::MOUSE_LEFT ) && !input->WasMouseReleased( vgui2::MOUSE_LEFT )
		&& !input->IsMouseDown( vgui2::MOUSE_LEFT ) )
		return false;	// not a click (Esc, a key)
	// the panel under the mouse or a parent of it (the item's text is a part of it) is the item
	vgui2::VPANEL over = input->GetMouseOver();
	for ( int depth = 0; over && depth < 3; depth++, over = vgui2::ipanel()->GetParent( over ) )
	{
		const char* name = vgui2::ipanel()->GetName( over );
		if ( name && !strcmp( name, "Advanced" ) )
		{
			CmdAdvanced();
			return true;
		}
	}
	return false;
}

void AdvancedOptions_Init()
{
	g_pSizeCvar = gEngfuncs.pfnRegisterVariable( (char*)"cl_advanced_size", (char*)"", FCVAR_ARCHIVE );
	gEngfuncs.pfnAddCommand( (char*)"cl_advanced", CmdAdvanced );
}
