// chatwindow.cpp - the chat as a real window.
//
// A key bound to "messagemode" (say), "messagemode2" (say_team) or "messagemode <command>" (admin chat binds and
// the like) opens this window over the chat area instead of the engine's one-line "say:" prompt:
//   - the chat history (the last 100 lines, in the same colours as on screen); scroll with the mouse wheel or the
//     scroll bar (its arrows drawn to the bar's size), select text with the mouse and copy it (Ctrl+C)
//   - a proper text box: cursor keys, Home/End, Ctrl+arrows, mouse selection, Ctrl+A/C/V/X/Z, any language's letters
//   - right click (history or text box): the chat's own Cut / Copy / Paste / Select all menu, in the chat's font
//   - Up/Down bring back what you sent before, Enter sends, Esc closes
// What is sent is exactly what the engine's prompt sent: <say|say_team|command> "<text>", ';' turned into ',',
// at most 119 bytes. While the window is closed the classic chat lines show as always.
// hud_chat_window 0 brings back the engine's prompt.
#include "hud.h"
#include "cl_util.h"
#include "vgui_controls/Controls.h"
#include "vgui/ISurface.h"
#include "vgui/IScheme.h"
#include "vgui/IInput.h"
#include "vgui/IPanel.h"
#include "vgui/ILocalize.h"
#include "vgui/ISystem.h"
#include "vgui/KeyCode.h"
#include "vgui/KeyValues.h"
#include "EditablePanel.h"
#include "RichText.h"
#include "ScrollBar.h"
#include "Button.h"
#include "TextEntry.h"
#include "IBaseUI.h"
#include "iviewport.h"
#include "../fontstyle.h"
#include "chatwindow.h"
#include "misc_features.h"

#include <stdio.h>
#include <string.h>
#include <wctype.h>
#include <deque>
#include <string>
#include <vector>

extern IBaseUI* g_pBaseUI;

namespace
{

const int kMaxHistoryLines = 100;
const int kMaxSentLines = 32;
const int kMaxChatBytes = 119;	// the engine's chat buffer (KEY_CHAT_BUFFER_BYTES 120, with the terminator)
const int kVisibleLines = 9;

struct ChatSegment
{
	int start;
	int length;
	Color color;
};

struct ChatLine
{
	std::wstring text;
	std::vector<ChatSegment> segments;
};

std::deque<ChatLine> g_history;
ChatLine g_building;
bool g_bBuilding;
std::vector<std::wstring> g_sent;	// oldest first
cvar_t* hud_chat_window;
cvar_t* con_color;
double g_flEscapeCloseTime = -1;

// the classic chat's colour for normal text
Color ChatTextColor()
{
	int r = 255, g = 180, b = 30;
	if ( !con_color )
		con_color = gEngfuncs.pfnGetCvarPointer( "con_color" );
	if ( con_color && con_color->string && sscanf( con_color->string, "%i %i %i", &r, &g, &b ) != 3 )
		r = g = b = 0;
	return Color( r, g, b, 255 );
}

// the classic chat's font (CHudVGUI2Print), so the window's text looks like the chat lines: the game's, or with the
// WON font style the WON lettering (fontstyle.cpp)
vgui2::HFont ChatFont()
{
	const vgui2::HFont won = FontStyle_ChatFont();
	if ( won )
		return won;
	vgui2::IScheme* scheme = vgui2::scheme()->GetIScheme( vgui2::scheme()->GetDefaultScheme() );
	return scheme ? scheme->GetFont( "EngineFont", true ) : 0;
}

// The WON lettering is light one-pixel strokes that the chat lines draw over a one-pixel black outline (the letters
// in black at the eight one-pixel offsets, hud_vgui2print.cpp); the window's text gets the same.
bool ChatOutline()
{
	return FontStyle_ChatFont() != 0;
}

// a line of the window's own text (the prompt, the menu's items) at x, y in the font and colour set - over the outline
// when the lettering has one; the text colour is left as it was given
void PrintChatText( int x, int y, Color color, const wchar_t* text )
{
	const int length = (int)wcslen( text );
	if ( ChatOutline() )
	{
		vgui2::surface()->DrawSetTextColor( Color( 0, 0, 0, color.a() ) );
		for ( int dy = -1; dy <= 1; dy++ )
		{
			for ( int dx = -1; dx <= 1; dx++ )
			{
				if ( !dx && !dy )
					continue;
				vgui2::surface()->DrawSetTextPos( x + dx, y + dy );
				vgui2::surface()->DrawPrintText( text, length );
			}
		}
	}
	vgui2::surface()->DrawSetTextColor( color );
	vgui2::surface()->DrawSetTextPos( x, y );
	vgui2::surface()->DrawPrintText( text, length );
}

// one of the engine's top-level panels ("BaseGameUIPanel" holds the main menu)
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

bool GameMenuVisible()
{
	const vgui2::VPANEL menu = FindEnginePanel( "BaseGameUIPanel" );
	return menu && vgui2::ipanel()->IsVisible( menu );
}

// The panel the window hangs under: the client's top panel, which is shown the whole time the game is (and hidden
// with the main menu). Not the viewport's "menu panel" (GetViewPortPanel): that is the background of the team and
// buy menus and is hidden whenever none of them is open - a window under it is never drawn.
vgui2::VPANEL ChatParentPanel()
{
	const vgui2::VPANEL client = FindEnginePanel( "BaseClientPanel" );
	if ( client )
		return client;
	if ( gViewPortInterface )
	{
		const vgui2::VPANEL menuPanel = gViewPortInterface->GetViewPortPanel();
		const vgui2::VPANEL parent = menuPanel ? vgui2::ipanel()->GetParent( menuPanel ) : 0;
		return parent ? parent : menuPanel;
	}
	return 0;
}

// visible itself and every panel above it visible too (so it is really drawn)
bool PanelFullyVisible( vgui2::VPANEL panel )
{
	for ( int depth = 0; panel && depth < 64; depth++ )
	{
		if ( !vgui2::ipanel()->IsVisible( panel ) )
			return false;
		panel = vgui2::ipanel()->GetParent( panel );
	}
	return true;
}

class CChatWindow;

// a scroll bar arrow button that draws its arrow to its own size (the stock ones show a small fixed picture, tiny on
// the chat's wide scroll bar); otherwise it works as the stock one
class CChatScrollButton : public vgui2::Button
{
	typedef vgui2::Button BaseClass;

public:
	explicit CChatScrollButton( bool up ) : vgui2::Button( NULL, NULL, "" ), m_bUp( up )
	{
		SetButtonActivationType( vgui2::Button::ACTIVATE_ONPRESSED );
	}
	// while it is held the bar keeps scrolling (the bar checks IsDepressed on each tick)
	void OnMouseFocusTicked() override
	{
		CallParentFunction( new KeyValues( "MouseFocusTicked" ) );
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
	}
	void Paint() override
	{
		BaseClass::Paint();
		int wide, tall;
		GetSize( wide, tall );
		// a filled triangle half the button wide, drawn row by row from its tip
		int side = ( wide < tall ? wide : tall ) / 2;
		if ( side < 4 )
			side = 4;
		const int rows = ( side + 1 ) / 2;
		const int cx = wide / 2, top = ( tall - rows ) / 2;
		const Color chat = ChatTextColor();
		vgui2::surface()->DrawSetColor( Color( chat.r(), chat.g(), chat.b(), IsEnabled() ? 255 : 90 ) );
		for ( int row = 0; row < rows; row++ )
		{
			const int step = m_bUp ? row : rows - 1 - row;	// 0 at the tip
			const int half = rows > 1 ? step * ( side / 2 ) / ( rows - 1 ) : side / 2;
			vgui2::surface()->DrawFilledRect( cx - half, top + row, cx + half + 1, top + row + 1 );
		}
	}

private:
	bool m_bUp;
};

// the chat's right-click menu: Cut / Copy / Paste / Select all in the chat's own look and font (the stock menus are
// tiny on big screens). On the history: Copy, Paste (into the text box), Select all; on the text box all four.
class CChatEditMenu : public vgui2::Panel
{
	typedef vgui2::Panel BaseClass;

public:
	enum Action
	{
		EDIT_CUT,
		EDIT_COPY,
		EDIT_PASTE,
		EDIT_SELECT_ALL,
	};

	CChatEditMenu( vgui2::Panel* parent, CChatWindow* window ) : vgui2::Panel( parent, "ChatEditMenu" ), m_pWindow( window )
	{
		SetVisible( false );
		SetPaintBackgroundEnabled( true );
		SetMouseInputEnabled( true );
		SetKeyBoardInputEnabled( false );
	}

	// at x, y of the window (the mouse pointer), kept on the screen
	void Open( bool history, int x, int y );
	void Close()
	{
		SetVisible( false );
		m_iHover = -1;
	}

protected:
	void PaintBackground() override;
	void Paint() override;
	void OnCursorMoved( int x, int y ) override;
	void OnCursorExited() override
	{
		m_iHover = -1;
	}
	void OnMousePressed( vgui2::MouseCode code ) override;

private:
	struct Item
	{
		Action action;
		const wchar_t* text;
		const wchar_t* keys;
		bool enabled;
	};

	int ItemUnderPointer();

	CChatWindow* m_pWindow;
	std::vector<Item> m_items;
	bool m_bHistory = false;
	int m_iHover = -1;
	int m_iPad = 4;
	int m_iItemTall = 20;
	int m_iKeysX = 0;
	vgui2::HFont m_hFont = 0;
};

// the history: rich text with the classic chat font, no box of its own (the window draws one)
class CChatHistory : public vgui2::RichText
{
	typedef vgui2::RichText BaseClass;

public:
	CChatHistory( vgui2::Panel* parent, CChatWindow* window )
		: vgui2::RichText( parent, "ChatHistory", false ), m_pWindow( window )
	{
		SetVerticalScrollbar( true );
	}
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override
	{
		BaseClass::ApplySchemeSettings( scheme );
		const vgui2::HFont font = ChatFont();
		if ( font )
			SetFont( font );
		SetFgColor( ChatTextColor() );
		SetBgColor( Color( 0, 0, 0, 0 ) );
		SetBorder( NULL );
		SetPaintBorderEnabled( false );
	}
	// typing while the history has the focus (after selecting text) goes on in the text box
	void OnKeyCodeTyped( vgui2::KeyCode code ) override;
	void OnKeyTyped( wchar_t ch ) override;
	// right click: the chat's own menu instead of the stock one
	void OnMousePressed( vgui2::MouseCode code ) override;

private:
	CChatWindow* m_pWindow;
};

class CChatEntry : public vgui2::TextEntry
{
	typedef vgui2::TextEntry BaseClass;

public:
	CChatEntry( vgui2::Panel* parent, CChatWindow* window )
		: vgui2::TextEntry( parent, "ChatEntry" ), m_pWindow( window )
	{
		SetAllowNonAsciiCharacters( true );
		SetMaximumCharCount( kMaxChatBytes );
		SetMultiline( false );
		SetCatchEnterKey( false );
	}
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override
	{
		BaseClass::ApplySchemeSettings( scheme );
		const vgui2::HFont font = ChatFont();
		if ( font )
			SetFont( font );
		SetFgColor( Color( 255, 255, 255, 255 ) );
		SetBgColor( Color( 0, 0, 0, 0 ) );
		SetBorder( NULL );
		SetPaintBorderEnabled( false );
	}
	void OnKeyCodeTyped( vgui2::KeyCode code ) override;
	void OnKeyTyped( wchar_t ch ) override;
	void OnMousePressed( vgui2::MouseCode code ) override;
	// the WON lettering over its outline (not under selected text: that stands on the selection's colour)
	int DrawChar( wchar_t ch, vgui2::HFont font, int index, int x, int y ) override
	{
		int from = -1, to = -1;
		GetSelectedRange( from, to );
		if ( ChatOutline() && iswprint( ch ) && !( index >= from && index < to ) )
		{
			vgui2::surface()->DrawSetTextColor( Color( 0, 0, 0, 255 ) );
			for ( int dy = -1; dy <= 1; dy++ )
			{
				for ( int dx = -1; dx <= 1; dx++ )
				{
					if ( !dx && !dy )
						continue;
					vgui2::surface()->DrawSetTextPos( x + dx, y + dy );
					vgui2::surface()->DrawUnicodeChar( ch );
				}
			}
			vgui2::surface()->DrawSetTextColor( GetFgColor() );
		}
		return BaseClass::DrawChar( ch, font, index, x, y );
	}

private:
	CChatWindow* m_pWindow;
};

class CChatWindow : public vgui2::EditablePanel
{
	typedef vgui2::EditablePanel BaseClass;

public:
	CChatWindow() : vgui2::EditablePanel( NULL, "ShadowsChatWindow" )
	{
		m_pHistory = new CChatHistory( this, this );
		m_pEntry = new CChatEntry( this, this );
		m_pEditMenu = new CChatEditMenu( this, this );	// made last: drawn over the rest
		// arrows drawn to the scroll bar's size
		if ( vgui2::ScrollBar* bar = m_pHistory->GetVerticalScrollBar() )
		{
			bar->SetButton( new CChatScrollButton( true ), 0 );
			bar->SetButton( new CChatScrollButton( false ), 1 );
		}
		m_hFont = 0;
		m_iPad = 4;
		m_iOpenKey = 0;
		m_flOpenTime = 0;
		m_iRecall = -1;
		m_bScrollToEnd = 0;
		m_bSwallowedOpenChar = false;
		SetPaintBorderEnabled( false );
		SetVisible( false );
	}

	// messageType: "say", "say_team" or the command of a "messagemode <command>" bind
	void Open( const char* messageType, int keynum )
	{
		strncpy( m_szMessageType, messageType, sizeof( m_szMessageType ) - 1 );
		m_szMessageType[sizeof( m_szMessageType ) - 1] = 0;
		if ( !strcmp( m_szMessageType, "say" ) )
			wcscpy( m_wszPrompt, L"Say:" );
		else if ( !strcmp( m_szMessageType, "say_team" ) )
			wcscpy( m_wszPrompt, L"Say (team):" );
		else
		{
			wchar_t command[64];
			vgui2::localize()->ConvertANSIToUnicode( m_szMessageType, command, sizeof( command ) );
			_snwprintf( m_wszPrompt, ARRAYSIZE( m_wszPrompt ) - 1, L"%s:", command );
			m_wszPrompt[ARRAYSIZE( m_wszPrompt ) - 1] = 0;
		}
		m_iOpenKey = keynum;
		m_flOpenTime = gEngfuncs.GetAbsoluteTime();
		m_bSwallowedOpenChar = false;
		m_iRecall = -1;
		m_draft.clear();
		const vgui2::VPANEL parent = ChatParentPanel();
		if ( parent )
			SetParent( parent );
		InvalidateLayout( false, true );	// fonts and colours from the scheme again (resolution may have changed)
		LayoutForScreen();
		Rebuild();
		m_pEntry->SetText( L"" );
		SetVisible( true );
		MoveToFront();
		m_pEntry->RequestFocus( 0 );
	}

	void Close( bool byEscape )
	{
		if ( !IsVisible() )
			return;
		CloseEditMenu();
		m_pEntry->SetText( L"" );
		SetVisible( false );
		if ( byEscape )
			g_flEscapeCloseTime = gEngfuncs.GetAbsoluteTime();
	}

	void Send()
	{
		wchar_t wide[kMaxChatBytes + 1];
		m_pEntry->GetText( wide, ARRAYSIZE( wide ) );
		if ( wide[0] )
		{
			RememberSent( wide );
			char text[kMaxChatBytes * 4 + 1];
			vgui2::localize()->ConvertUnicodeToANSI( wide, text, sizeof( text ) );
			// the engine's limit, cut at a whole UTF-8 character
			int length = (int)strlen( text );
			if ( length > kMaxChatBytes )
			{
				length = kMaxChatBytes;
				while ( length > 0 && ( (unsigned char)text[length] & 0xC0 ) == 0x80 )
					length--;
				text[length] = 0;
			}
			// as the engine's prompt: no ';' (it would end the command)
			for ( char* scan = text; *scan; scan++ )
			{
				if ( *scan == ';' )
					*scan = ',';
			}
			char command[sizeof( m_szMessageType ) + sizeof( text ) + 8];
			_snprintf( command, sizeof( command ) - 1, "%s \"%s\"", m_szMessageType, text );
			command[sizeof( command ) - 1] = 0;
			gEngfuncs.pfnClientCmd( command );
			MiscFeatures_PlayerAsked();	// plugin lines in the next seconds are replies (Hide AMXX texts)
		}
		Close( false );
	}

	// Up = older (direction 1), Down = newer (-1); past the newest the unsent text comes back
	void Recall( int direction )
	{
		if ( g_sent.empty() )
			return;
		const int count = (int)g_sent.size();
		if ( m_iRecall < 0 )
		{
			if ( direction < 0 )
				return;
			wchar_t current[kMaxChatBytes + 1];
			m_pEntry->GetText( current, ARRAYSIZE( current ) );
			m_draft = current;
			m_iRecall = count - 1;
		}
		else
		{
			m_iRecall -= direction;
			if ( m_iRecall < 0 )
			{
				m_iRecall = -1;
				m_pEntry->SetText( m_draft.c_str() );
				m_pEntry->GotoTextEnd();
				return;
			}
			if ( m_iRecall >= count )
				m_iRecall = 0;
		}
		m_pEntry->SetText( g_sent[m_iRecall].c_str() );
		m_pEntry->GotoTextEnd();
	}

	// the character typed by the key that opened the window must not end up in the text box
	bool SwallowOpeningChar( wchar_t ch )
	{
		if ( m_bSwallowedOpenChar || m_iOpenKey <= 0 || m_iOpenKey > 255 )
			return false;
		if ( gEngfuncs.GetAbsoluteTime() - m_flOpenTime > 0.15 )
			return false;
		if ( towlower( ch ) != towlower( (wchar_t)m_iOpenKey ) )
			return false;
		m_bSwallowedOpenChar = true;
		return true;
	}

	void FocusEntry() { m_pEntry->RequestFocus( 0 ); }
	CChatEntry* Entry() { return m_pEntry; }

	// the right-click menu, at the mouse pointer
	void OpenEditMenu( bool history )
	{
		int x = 0, y = 0;
		vgui2::input()->GetCursorPos( x, y );
		ScreenToLocal( x, y );
		m_pEditMenu->Open( history, x, y );
		m_pEditMenu->MoveToFront();
	}
	void CloseEditMenu()
	{
		if ( m_pEditMenu )
			m_pEditMenu->Close();
	}
	bool HistoryHasSelection() { return m_pHistory->HasSelection(); }
	bool EntryHasSelection()
	{
		int start = 0, end = 0;
		return m_pEntry->GetSelectedRange( start, end );
	}
	bool EntryHasText()
	{
		wchar_t text[kMaxChatBytes + 1];
		m_pEntry->GetText( text, ARRAYSIZE( text ) );
		return text[0] != 0;
	}
	static bool ClipboardHasText() { return vgui2::system()->GetClipboardTextCount() > 0; }
	void DoEditAction( int action, bool history )
	{
		switch ( action )
		{
		case CChatEditMenu::EDIT_CUT:
			FocusEntry();
			m_pEntry->CutSelected();
			break;
		case CChatEditMenu::EDIT_COPY:
			if ( history )
				m_pHistory->CopySelected();
			else
				m_pEntry->CopySelected();
			break;
		case CChatEditMenu::EDIT_PASTE:
			FocusEntry();	// pasted into the text box, wherever the menu was opened
			m_pEntry->Paste();
			break;
		case CChatEditMenu::EDIT_SELECT_ALL:
			if ( history )
			{
				m_pHistory->RequestFocus( 0 );
				m_pHistory->SelectAllText();
			}
			else
			{
				FocusEntry();
				m_pEntry->SelectAllText( true );
			}
			break;
		}
	}

	void AppendLine( const ChatLine& line, bool newLineBefore )
	{
		if ( newLineBefore )
			m_pHistory->InsertChar( L'\n' );
		for ( const ChatSegment& segment : line.segments )
		{
			m_pHistory->InsertColorChange( segment.color );
			m_pHistory->InsertString( line.text.substr( segment.start, segment.length ).c_str() );
		}
	}

	void AddLine( const ChatLine& line )
	{
		if ( !IsVisible() )
			return;	// the history is rebuilt when the window opens
		AppendLine( line, true );
		m_bScrollToEnd = 2;
	}

	void OnThink() override
	{
		BaseClass::OnThink();
		if ( !IsVisible() )
			return;
		// keep the keyboard here while the window is open (a click elsewhere would hand it back to the game keys)
		const vgui2::VPANEL focus = vgui2::input()->GetFocus();
		if ( !focus || ( focus != GetVPanel() && !vgui2::ipanel()->HasParent( focus, GetVPanel() ) ) )
			m_pEntry->RequestFocus( 0 );
		// scroll to the newest line once the rich text has laid the new text out
		if ( m_bScrollToEnd > 0 )
		{
			m_pHistory->GotoTextEnd();
			m_bScrollToEnd--;
		}
		if ( ScreenWidth != m_iLaidOutWide || ScreenHeight != m_iLaidOutTall )
			LayoutForScreen();
	}

	// a click on the window's empty (see-through) area closes the menu
	void OnMousePressed( vgui2::MouseCode code ) override
	{
		CloseEditMenu();
		BaseClass::OnMousePressed( code );
	}

	void OnKeyCodeTyped( vgui2::KeyCode code ) override
	{
		// Keys reach the window when a child does not use them (Shift, F-keys, ... are passed up by the text box) or
		// when the window's empty area has the focus. Never hand them back to the text box: it would pass them up
		// again, round and round until the game crashes. Esc / Enter still work; the rest is dropped here, so it
		// does not travel on to the game's panels either.
		if ( code == vgui2::KEY_ESCAPE )
			Close( true );
		else if ( code == vgui2::KEY_ENTER || code == vgui2::KEY_PAD_ENTER )
			Send();
	}

protected:
	// The window itself covers the whole screen (transparent), so the mouse pointer stays visible everywhere while
	// typing - over the game the engine's own panel hides the pointer and the mouse would turn the view instead.
	// Only the chat box (m_iBox*) is drawn.
	void PaintBackground() override
	{
		vgui2::surface()->DrawSetColor( Color( 0, 0, 0, 150 ) );
		vgui2::surface()->DrawFilledRect( m_iBoxX, m_iBoxY, m_iBoxX + m_iBoxWide, m_iBoxY + m_iBoxTall );
		vgui2::surface()->DrawSetColor( Color( 255, 255, 255, 30 ) );
		vgui2::surface()->DrawOutlinedRect( m_iBoxX, m_iBoxY, m_iBoxX + m_iBoxWide, m_iBoxY + m_iBoxTall );
		// the text box
		int x, y, w, h;
		m_pEntry->GetBounds( x, y, w, h );
		vgui2::surface()->DrawSetColor( Color( 0, 0, 0, 120 ) );
		vgui2::surface()->DrawFilledRect( x - m_iPad / 2, y, x + w, y + h );
		vgui2::surface()->DrawSetColor( Color( 255, 255, 255, 45 ) );
		vgui2::surface()->DrawOutlinedRect( x - m_iPad / 2, y, x + w, y + h );
	}

	void Paint() override
	{
		if ( !m_hFont )
			return;
		int x, y, w, h;
		m_pEntry->GetBounds( x, y, w, h );
		const int textTall = vgui2::surface()->GetFontTall( m_hFont );
		vgui2::surface()->DrawSetTextFont( m_hFont );
		PrintChatText( m_iBoxX + m_iPad, y + ( h - textTall ) / 2, ChatTextColor(), m_wszPrompt );
	}

private:
	void RememberSent( const wchar_t* text )
	{
		if ( !g_sent.empty() && g_sent.back() == text )
			return;
		g_sent.push_back( text );
		if ( (int)g_sent.size() > kMaxSentLines )
			g_sent.erase( g_sent.begin() );
	}

	void Rebuild()
	{
		m_pHistory->SetText( L"" );
		bool first = true;
		for ( const ChatLine& line : g_history )
		{
			AppendLine( line, !first );
			first = false;
		}
		m_bScrollToEnd = 2;
	}

	// bottom left, where the classic chat lines are, ending just above the HUD numbers
	void LayoutForScreen()
	{
		m_hFont = ChatFont();
		if ( m_hFont )
		{
			// (the font style may have changed since the boxes took their font)
			m_pHistory->SetFont( m_hFont );
			m_pEntry->SetFont( m_hFont );
			m_pHistory->SetTextOutline( ChatOutline() );
		}
		const int textTall = m_hFont ? vgui2::surface()->GetFontTall( m_hFont ) : 14;
		m_iPad = textTall / 3 > 4 ? textTall / 3 : 4;
		int wide = (int)( ScreenWidth * 0.45f );
		if ( wide < 360 )
			wide = 360;
		if ( wide > ScreenWidth - 20 )
			wide = ScreenWidth - 20;
		const int historyTall = textTall * kVisibleLines + m_iPad;
		const int entryTall = textTall + m_iPad;
		const int tall = m_iPad + historyTall + m_iPad + entryTall + m_iPad;
		int bottom;
		if ( ( g_iUser1 || gEngfuncs.IsSpectateOnly() ) && gViewPortInterface )
			bottom = ScreenHeight - gViewPortInterface->GetSpectatorBottomBarHeight() - 4;
		else
			bottom = ScreenHeight - (int)( gHUD.m_iFontHeight * 2.75 );
		int y = bottom - tall;
		if ( y < 0 )
			y = 0;
		SetBounds( 0, 0, ScreenWidth, ScreenHeight );
		m_iBoxX = 10;
		m_iBoxY = y;
		m_iBoxWide = wide;
		m_iBoxTall = tall;
		m_pHistory->SetBounds( m_iBoxX + m_iPad, m_iBoxY + m_iPad, wide - m_iPad * 2, historyTall );
		// the stock 18 pixel scroll bar is hard to grab at high resolutions: size it to the chat text
		const int scrollWide = textTall * 4 / 3;
		m_pHistory->SetScrollBarSize( scrollWide > 22 ? scrollWide : 22 );
		int promptWide = 0, promptTall = 0;
		if ( m_hFont )
			vgui2::surface()->GetTextSize( m_hFont, m_wszPrompt, promptWide, promptTall );
		const int entryX = m_iPad + promptWide + m_iPad;
		m_pEntry->SetBounds( m_iBoxX + entryX, m_iBoxY + m_iPad + historyTall + m_iPad, wide - entryX - m_iPad, entryTall );
		m_iLaidOutWide = ScreenWidth;
		m_iLaidOutTall = ScreenHeight;
	}

	CChatHistory* m_pHistory;
	CChatEntry* m_pEntry;
	CChatEditMenu* m_pEditMenu = nullptr;
	vgui2::HFont m_hFont;
	int m_iPad;
	int m_iLaidOutWide = 0, m_iLaidOutTall = 0;
	int m_iBoxX = 0, m_iBoxY = 0, m_iBoxWide = 0, m_iBoxTall = 0;
	char m_szMessageType[64] = "say";
	wchar_t m_wszPrompt[80] = L"Say:";
	int m_iOpenKey;
	double m_flOpenTime;
	bool m_bSwallowedOpenChar;
	int m_iRecall;
	std::wstring m_draft;
	int m_bScrollToEnd;
};

CChatWindow* g_pChat;

void CChatEditMenu::Open( bool history, int x, int y )
{
	m_bHistory = history;
	m_items.clear();
	const bool clipboard = CChatWindow::ClipboardHasText();
	if ( history )
	{
		m_items.push_back( { EDIT_COPY, L"Copy", L"Ctrl+C", m_pWindow->HistoryHasSelection() } );
		m_items.push_back( { EDIT_PASTE, L"Paste", L"Ctrl+V", clipboard } );
		m_items.push_back( { EDIT_SELECT_ALL, L"Select all", L"", true } );
	}
	else
	{
		const bool selection = m_pWindow->EntryHasSelection();
		m_items.push_back( { EDIT_CUT, L"Cut", L"Ctrl+X", selection } );
		m_items.push_back( { EDIT_COPY, L"Copy", L"Ctrl+C", selection } );
		m_items.push_back( { EDIT_PASTE, L"Paste", L"Ctrl+V", clipboard } );
		m_items.push_back( { EDIT_SELECT_ALL, L"Select all", L"Ctrl+A", m_pWindow->EntryHasText() } );
	}
	// sized to the chat's font
	m_hFont = ChatFont();
	const int textTall = m_hFont ? vgui2::surface()->GetFontTall( m_hFont ) : 14;
	m_iPad = textTall / 3 > 3 ? textTall / 3 : 3;
	m_iItemTall = textTall + m_iPad * 2;
	int textWide = 0, keysWide = 0;
	for ( const Item& item : m_items )
	{
		int wide = 0, tall = 0;
		if ( m_hFont )
			vgui2::surface()->GetTextSize( m_hFont, item.text, wide, tall );
		textWide = wide > textWide ? wide : textWide;
		wide = 0;
		if ( m_hFont && item.keys[0] )
			vgui2::surface()->GetTextSize( m_hFont, item.keys, wide, tall );
		keysWide = wide > keysWide ? wide : keysWide;
	}
	m_iKeysX = m_iPad * 3 + textWide + m_iPad * 4;
	const int wide = m_iKeysX + keysWide + m_iPad * 3;
	const int tall = (int)m_items.size() * m_iItemTall + m_iPad * 2;
	// on the screen (the window covers all of it)
	int parentWide = 0, parentTall = 0;
	GetParent()->GetSize( parentWide, parentTall );
	if ( x + wide > parentWide )
		x = parentWide - wide;
	if ( y + tall > parentTall )
		y = y - tall;
	x = x > 0 ? x : 0;
	y = y > 0 ? y : 0;
	SetBounds( x, y, wide, tall );
	m_iHover = -1;
	SetVisible( true );
}

int CChatEditMenu::ItemUnderPointer()
{
	int x = 0, y = 0;
	vgui2::input()->GetCursorPos( x, y );
	ScreenToLocal( x, y );
	if ( x < 0 || x >= GetWide() || y < m_iPad || m_iItemTall <= 0 )
		return -1;
	const int index = ( y - m_iPad ) / m_iItemTall;
	return index < (int)m_items.size() ? index : -1;
}

void CChatEditMenu::PaintBackground()
{
	int wide, tall;
	GetSize( wide, tall );
	vgui2::surface()->DrawSetColor( Color( 0, 0, 0, 225 ) );
	vgui2::surface()->DrawFilledRect( 0, 0, wide, tall );
	vgui2::surface()->DrawSetColor( Color( 255, 255, 255, 60 ) );
	vgui2::surface()->DrawOutlinedRect( 0, 0, wide, tall );
}

void CChatEditMenu::Paint()
{
	if ( !m_hFont )
		return;
	const int wide = GetWide();
	const int textTall = vgui2::surface()->GetFontTall( m_hFont );
	const Color chat = ChatTextColor();
	vgui2::surface()->DrawSetTextFont( m_hFont );
	for ( int i = 0; i < (int)m_items.size(); i++ )
	{
		const Item& item = m_items[i];
		const int y = m_iPad + i * m_iItemTall;
		if ( i == m_iHover && item.enabled )
		{
			vgui2::surface()->DrawSetColor( Color( chat.r(), chat.g(), chat.b(), 110 ) );
			vgui2::surface()->DrawFilledRect( 1, y, wide - 1, y + m_iItemTall );
		}
		const int textY = y + ( m_iItemTall - textTall ) / 2;
		PrintChatText( m_iPad * 3, textY, Color( 255, 255, 255, item.enabled ? 255 : 90 ), item.text );
		if ( item.keys[0] )
			PrintChatText( m_iKeysX, textY, Color( 255, 255, 255, item.enabled ? 130 : 60 ), item.keys );
	}
	vgui2::surface()->DrawFlushText();
}

void CChatEditMenu::OnCursorMoved( int, int )
{
	const int hover = ItemUnderPointer();
	if ( hover != m_iHover )
	{
		m_iHover = hover;
		Repaint();
	}
}

void CChatEditMenu::OnMousePressed( vgui2::MouseCode )
{
	const int index = ItemUnderPointer();
	if ( index >= 0 && !m_items[index].enabled )
		return;	// a greyed-out item: the menu stays
	Close();
	if ( index >= 0 )
		m_pWindow->DoEditAction( m_items[index].action, m_bHistory );
}

void CChatHistory::OnMousePressed( vgui2::MouseCode code )
{
	if ( code == vgui2::MOUSE_RIGHT )
	{
		m_pWindow->OpenEditMenu( true );
		return;
	}
	m_pWindow->CloseEditMenu();
	BaseClass::OnMousePressed( code );
}

void CChatEntry::OnMousePressed( vgui2::MouseCode code )
{
	if ( code == vgui2::MOUSE_RIGHT )
	{
		RequestFocus( 0 );
		m_pWindow->OpenEditMenu( false );
		return;
	}
	m_pWindow->CloseEditMenu();
	BaseClass::OnMousePressed( code );
}

void CChatHistory::OnKeyCodeTyped( vgui2::KeyCode code )
{
	const bool ctrl = vgui2::input()->IsKeyDown( vgui2::KEY_LCONTROL ) || vgui2::input()->IsKeyDown( vgui2::KEY_RCONTROL );
	if ( ctrl && ( code == vgui2::KEY_C || code == vgui2::KEY_INSERT ) )
	{
		BaseClass::OnKeyCodeTyped( code );	// copy the selected history text
		return;
	}
	if ( code == vgui2::KEY_ESCAPE )
	{
		m_pWindow->Close( true );
		return;
	}
	m_pWindow->FocusEntry();
	m_pWindow->Entry()->OnKeyCodeTyped( code );
}

void CChatHistory::OnKeyTyped( wchar_t ch )
{
	m_pWindow->FocusEntry();
	m_pWindow->Entry()->OnKeyTyped( ch );
}

void CChatEntry::OnKeyCodeTyped( vgui2::KeyCode code )
{
	m_pWindow->CloseEditMenu();
	switch ( code )
	{
	case vgui2::KEY_ENTER:
	case vgui2::KEY_PAD_ENTER:
		m_pWindow->Send();
		return;
	case vgui2::KEY_ESCAPE:
		m_pWindow->Close( true );
		return;
	case vgui2::KEY_UP:
		m_pWindow->Recall( 1 );
		return;
	case vgui2::KEY_DOWN:
		m_pWindow->Recall( -1 );
		return;
	case vgui2::KEY_TAB:
		return;	// no focus hopping
	default:
		BaseClass::OnKeyCodeTyped( code );
		return;
	}
}

void CChatEntry::OnKeyTyped( wchar_t ch )
{
	m_pWindow->CloseEditMenu();
	if ( m_pWindow->SwallowOpeningChar( ch ) )
		return;
	BaseClass::OnKeyTyped( ch );
}

// first word of a key binding and its argument: "messagemode amx_chat" -> "messagemode", "amx_chat"
bool ParseBinding( const char* binding, char* command, int commandSize, char* argument, int argumentSize )
{
	command[0] = argument[0] = 0;
	if ( !binding || strchr( binding, ';' ) || strchr( binding, '\n' ) )
		return false;	// several commands in one bind: leave it to the engine
	const char* scan = binding;
	char* outs[2] = { command, argument };
	const int sizes[2] = { commandSize, argumentSize };
	for ( int token = 0; token < 2; token++ )
	{
		while ( *scan == ' ' || *scan == '\t' )
			scan++;
		int length = 0;
		if ( *scan == '"' )
		{
			scan++;
			while ( *scan && *scan != '"' )
			{
				if ( length < sizes[token] - 1 )
					outs[token][length++] = *scan;
				scan++;
			}
			if ( *scan == '"' )
				scan++;
		}
		else
		{
			while ( *scan && *scan != ' ' && *scan != '\t' )
			{
				if ( length < sizes[token] - 1 )
					outs[token][length++] = *scan;
				scan++;
			}
		}
		outs[token][length] = 0;
	}
	return command[0] != 0;
}

} // namespace

void ChatWindow_Init()
{
	hud_chat_window = gEngfuncs.pfnRegisterVariable( (char*)"hud_chat_window", (char*)"1", FCVAR_ARCHIVE );
}

// open = really on screen; a window that is not drawn must never count (it would hide the classic chat)
bool ChatWindow_IsOpen()
{
	return g_pChat && g_pChat->IsVisible() && PanelFullyVisible( g_pChat->GetVPanel() );
}

void ChatWindow_Close()
{
	if ( g_pChat )
		g_pChat->Close( false );
}

bool ChatWindow_KeyEvent( int down, int keynum, const char* binding )
{
	if ( !down || ChatWindow_IsOpen() || !hud_chat_window || hud_chat_window->value == 0.0f || !gViewPortInterface
		|| !ChatParentPanel() )
		return false;
	char command[32], argument[64];
	if ( !ParseBinding( binding, command, sizeof( command ), argument, sizeof( argument ) ) )
		return false;
	const char* messageType;
	if ( !stricmp( command, "messagemode" ) )
		messageType = argument[0] ? argument : "say";	// as the engine: "messagemode <command>" sends <command>
	else if ( !stricmp( command, "messagemode2" ) )
		messageType = "say_team";
	else
		return false;
	if ( !g_pChat )
	{
		g_pChat = new CChatWindow();
		g_pChat->SetParent( ChatParentPanel() );
		g_pChat->MakePopup( false, false );
	}
	g_pChat->Open( messageType, keynum );
	if ( !ChatWindow_IsOpen() )
	{
		// could not be shown: hand the key back so the engine's own prompt opens instead
		g_pChat->Close( false );
		return false;
	}
	return true;
}

void ChatWindow_Frame()
{
	if ( !g_pChat )
		return;
	const double now = gEngfuncs.GetAbsoluteTime();
	// Esc in the text box also reaches the engine, which opens the main menu with it: close that menu again, so Esc
	// only closes the chat, as the engine's own prompt did. It is only ever the menu that this Esc press opened.
	if ( g_flEscapeCloseTime >= 0 )
	{
		if ( now - g_flEscapeCloseTime > 0.5 )
			g_flEscapeCloseTime = -1;
		else if ( GameMenuVisible() )
		{
			g_flEscapeCloseTime = -1;
			if ( g_pBaseUI )
				g_pBaseUI->HideGameUI();
		}
	}
	// the menu or console came up while typing (the engine saw that Esc first, or the console key): close the chat
	if ( g_pChat->IsVisible() && GameMenuVisible() )
	{
		const bool escape = vgui2::input()->IsKeyDown( vgui2::KEY_ESCAPE );
		g_pChat->Close( false );
		if ( escape && g_pBaseUI )
			g_pBaseUI->HideGameUI();
	}
	// marked open but not drawn (a panel above it got hidden): close it, so it does not keep the mouse cursor or
	// the keyboard while nobody can see it
	else if ( g_pChat->IsVisible() && !PanelFullyVisible( g_pChat->GetVPanel() ) )
		g_pChat->Close( false );
}

void ChatWindow_BeginLine()
{
	g_building.text.clear();
	g_building.segments.clear();
	g_bBuilding = true;
}

void ChatWindow_AddText( const wchar_t* text, int length, const float* color )
{
	if ( !g_bBuilding || !text )
		return;
	ChatSegment segment;
	segment.start = (int)g_building.text.size();
	segment.color = color ? Color( (int)( color[0] * 255.0f ), (int)( color[1] * 255.0f ), (int)( color[2] * 255.0f ), 255 )
		: ChatTextColor();
	for ( int i = 0; i < length && text[i]; i++ )
	{
		// colour codes and other control characters are not drawn by the classic chat either
		if ( text[i] >= 32 )
			g_building.text.push_back( text[i] );
	}
	segment.length = (int)g_building.text.size() - segment.start;
	if ( segment.length > 0 )
		g_building.segments.push_back( segment );
}

void ChatWindow_EndLine()
{
	if ( !g_bBuilding )
		return;
	g_bBuilding = false;
	if ( g_building.text.empty() )
		return;
	g_history.push_back( g_building );
	while ( (int)g_history.size() > kMaxHistoryLines )
		g_history.pop_front();
	if ( g_pChat )
		g_pChat->AddLine( g_history.back() );
}
