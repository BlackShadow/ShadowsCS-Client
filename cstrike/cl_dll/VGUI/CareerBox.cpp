//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose:
//
//=============================================================================

// Author: Matthew D. Campbell (matt@turtlerockstudios.com), 2003

#include "CareerBox.h"
#include "hud.h"
#include "GameUI_Interfaces.h"
#include "GameUI/ICareerUI.h"
#include "IBaseUI.h"
#include "BitmapImagePanel.h"
#include "Button.h"
#include "Label.h"
#include "vgui_controls/TextEntry.h"
#include "BuildGroup.h"
#include "vgui_controls/ComboBox.h"
#include "Menu.h"
#include "TextImage.h"
#include "WeaponSetLabel.h"
#include "buypreset_listbox.h"
#include "buy_presets.h"
#include "shared_util.h"
#include "vgui/KeyValues.h"
#include "vgui_controls/Controls.h"
#include "vgui/IInput.h"
#include "vgui/ISurface.h"
#include "vgui/ILocalize.h"
#include "vgui/IImage.h"
#include "r_studioint.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#ifdef PostMessage
#undef PostMessage
#endif

extern engine_studio_api_t IEngineStudio;

using namespace vgui2;

//--------------------------------------------------------------------------------------------------------------
CCareerBaseBox *CCareerBaseBox::s_currentBox;

//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
CCareerBaseBox::CCareerBaseBox( Panel *parent, const char *panelName, bool loadResources ) : Frame( parent, panelName, false, false )
{
	CloseOpenBoxes( true );
	s_currentBox = this;

	if ( g_pCareerUI && g_pCareerUI->IsPlayingMatch() )
	{
		SetScheme( "CareerClientScheme" );
	}
	else
	{
		SetScheme( "ClientScheme" );
	}

	SetProportional( true );

	SetMoveable( false );
	SetSizeable( false );

	m_pTopLeftPanel = new CBitmapImagePanel( this, "TopLeftPanel", "gfx/vgui/round_corner_nw" );
	m_pTopRightPanel = new CBitmapImagePanel( this, "TopRightPanel", "gfx/vgui/round_corner_ne" );
	m_pBottomLeftPanel = new CBitmapImagePanel( this, "BottomLeftPanel", "gfx/vgui/round_corner_sw" );
	m_pBottomRightPanel = new CBitmapImagePanel( this, "BottomRightPanel", "gfx/vgui/round_corner_se" );

	m_pTopLeftBorderPanel = new CBitmapImagePanel( this, "TopLeftBorderPanel", "gfx/vgui/round_corner_small_nw" );
	m_pTopRightBorderPanel = new CBitmapImagePanel( this, "TopRightBorderPanel", "gfx/vgui/round_corner_small_ne" );
	m_pBottomLeftBorderPanel = new CBitmapImagePanel( this, "BottomLeftBorderPanel", "gfx/vgui/round_corner_small_sw" );
	m_pBottomRightBorderPanel = new CBitmapImagePanel( this, "BottomRightBorderPanel", "gfx/vgui/round_corner_small_se" );

	m_pGapPanel = new Panel( this, "GapPanel" );
	m_pIconPanel = new CBitmapImagePanel( this, "IconPanel", "gfx/vgui/icon_!" );

	m_pTextLabel = new Label( this, "TextLabel", "" );

	m_pOkButton = new Button( this, "OkButton", "" );
	m_pOkButton->SetVisible( false );
	new Button( this, "CancelButton", "" );
	m_shouldAutoClose = true;

	if ( loadResources )
	{
		char *buf = new char[strlen( panelName ) + 17];
		strcpy( buf, "resource/UI/" );
		strcat( buf, panelName );
		strcat( buf, ".res" );
		LoadControlSettings( buf, NULL );
		delete[] buf;
	}
}

//--------------------------------------------------------------------------------------------------------------
CCareerBaseBox::~CCareerBaseBox( void )
{
	if ( this == s_currentBox )
	{
		s_currentBox = NULL;
	}
}

//--------------------------------------------------------------------------------------------------------------
void CCareerBaseBox::ActivateBuildMode( void )
{
	SetProportional( false );
	SetScheme( "TrackerScheme" );
	InvalidateLayout( false, true );
	if ( GetBuildGroup() && GetBuildGroup()->GetResourceName() )
	{
		char *tmpStr = CloneString( GetBuildGroup()->GetResourceName() );
		LoadControlSettings( tmpStr, NULL );
		delete[] tmpStr;
	}
	EditablePanel::ActivateBuildMode();
}

//--------------------------------------------------------------------------------------------------------------
void CCareerBaseBox::FriendLoadResources( const char *resources )
{
	if ( resources )
		LoadControlSettings( resources, NULL );
}

//--------------------------------------------------------------------------------------------------------------
void CCareerBaseBox::CloseOpenBoxes( bool force )
{
	if ( !s_currentBox )
		return;
	if ( force || s_currentBox->m_shouldAutoClose )
	{
		s_currentBox->Close();
		s_currentBox = NULL;
	}
}

//--------------------------------------------------------------------------------------------------------------
void CCareerBaseBox::BringToFront( void )
{
	if ( !s_currentBox || !s_currentBox->IsVisible() )
		return;

	s_currentBox->MoveToFront();
	s_currentBox->DoModal();
}

//--------------------------------------------------------------------------------------------------------------
void CCareerBaseBox::SetLabelText( const wchar_t *text )
{
	m_pTextLabel->SetText( text );
}

//--------------------------------------------------------------------------------------------------------------
void CCareerBaseBox::SetLabelText( const char *text )
{
	m_pTextLabel->SetText( text );
}

//--------------------------------------------------------------------------------------------------------------
void CCareerBaseBox::ApplySchemeSettings( IScheme *pScheme )
{
	Frame::ApplySchemeSettings( pScheme );

	m_bgColor = GetSchemeColor( "CareerBoxBG", GetSchemeColor( "ControlBG", pScheme ), pScheme );
	m_borderColor = GetSchemeColor( "BorderBuyPreset", pScheme );

	if ( !IEngineStudio.IsHardware() )
		m_bgColor[3] = 255;

	Color imageColor( 255, 255, 255, m_bgColor[3] );
	m_pTopLeftPanel->m_bgColor = imageColor;
	m_pTopRightPanel->m_bgColor = imageColor;
	m_pBottomLeftPanel->m_bgColor = imageColor;
	m_pBottomRightPanel->m_bgColor = imageColor;
	m_pIconPanel->m_bgColor = GetSchemeColor( "BaseText", pScheme );

	m_borderColor = GetSchemeColor( "BorderBuyPreset", pScheme );
	m_pTopLeftBorderPanel->m_bgColor = m_borderColor;
	m_pTopRightBorderPanel->m_bgColor = m_borderColor;
	m_pBottomLeftBorderPanel->m_bgColor = m_borderColor;
	m_pBottomRightBorderPanel->m_bgColor = m_borderColor;

	SetBorder( NULL );
}

//--------------------------------------------------------------------------------------------------------------
void CCareerBaseBox::PerformLayout( void )
{
	Frame::PerformLayout();

	int x, y, w, h;
	GetBounds( x, y, w, h );

	int screenWide, screenTall;
	surface()->GetScreenSize( screenWide, screenTall );
	if ( w / 2 + x != screenWide / 2 )
	{
		SetPos( screenWide / 2 - w / 2, screenTall / 2 - h / 2 );
		GetBounds( x, y, w, h );
	}

	int cornerSize = 10;
	if ( IEngineStudio.IsHardware() != 1 )
		cornerSize = 0;

	const int offset = 1;

	m_pTopLeftPanel->SetBounds( 0, 0, cornerSize, cornerSize );
	m_pTopRightPanel->SetBounds( w - cornerSize - offset, 0, cornerSize, cornerSize );
	m_pBottomLeftPanel->SetBounds( 0, h - cornerSize - offset, cornerSize, cornerSize );
	m_pBottomRightPanel->SetBounds( w - cornerSize - offset, h - cornerSize - offset, cornerSize, cornerSize );

	m_pTopLeftBorderPanel->SetBounds( 0, 0, cornerSize, cornerSize );
	m_pTopRightBorderPanel->SetBounds( w - cornerSize - offset, 0, cornerSize, cornerSize );
	m_pBottomLeftBorderPanel->SetBounds( 0, h - cornerSize - offset, cornerSize, cornerSize );
	m_pBottomRightBorderPanel->SetBounds( w - cornerSize - offset, h - cornerSize - offset, cornerSize, cornerSize );

	m_pTopLeftBorderPanel->MoveToFront();
	m_pTopRightBorderPanel->MoveToFront();
	m_pBottomLeftBorderPanel->MoveToFront();
	m_pBottomRightBorderPanel->MoveToFront();

	int gapWide, gapTall, gapX, gapY;
	m_pGapPanel->GetBounds( gapX, gapY, gapWide, gapTall );
	if ( gapTall > gapWide )
	{
		m_pGapPanel->SetBounds( gapX, 0, gapWide, h );
	}
	else if ( gapTall < gapWide )
	{
		m_pGapPanel->SetBounds( 0, gapY, w, gapTall );
	}
}

//--------------------------------------------------------------------------------------------------------------
void CCareerBaseBox::Paint( void )
{
	Frame::Paint();

	surface()->DrawSetColor( m_borderColor );
	surface()->DrawSetTextColor( m_borderColor );
	int x1, x2, y1, y2;

	m_pTopLeftPanel->GetPos( x1, y1 );
	m_pBottomLeftPanel->GetPos( x2, y2 );
	y1 += m_pTopLeftPanel->GetTall();
	surface()->DrawFilledRect( x1, y1, x2 + 1, y2 + 1 );

	m_pTopLeftPanel->GetPos( x1, y1 );
	m_pTopRightPanel->GetPos( x2, y2 );
	x1 += m_pTopLeftPanel->GetWide();
	surface()->DrawFilledRect( x1, y1, x2 + 1, y2 + 1 );

	m_pBottomLeftPanel->GetPos( x1, y1 );
	m_pBottomRightPanel->GetPos( x2, y2 );
	x1 += m_pBottomLeftPanel->GetWide();
	y1 += m_pBottomLeftPanel->GetTall();
	y2 += m_pBottomRightPanel->GetTall();
	surface()->DrawFilledRect( x1, y1, x2 + 1, y2 + 1 );

	m_pTopRightPanel->GetPos( x1, y1 );
	m_pBottomRightPanel->GetPos( x2, y2 );
	y1 += m_pTopRightPanel->GetTall();
	x1 += m_pTopRightPanel->GetWide();
	x2 += m_pBottomRightPanel->GetWide();
	surface()->DrawFilledRect( x1, y1, x2 + 1, y2 + 1 );
}

//--------------------------------------------------------------------------------------------------------------
void CCareerBaseBox::PaintBackground( void )
{
	int x1, x2, y1, y2;
	surface()->DrawSetColor( m_bgColor );

	if ( m_pGapPanel->GetTall() > m_pGapPanel->GetWide() )
	{
		m_pTopLeftPanel->GetPos( x1, y1 );
		m_pBottomLeftPanel->GetPos( x2, y2 );
		y1 += m_pTopLeftPanel->GetTall();
		x2 += m_pBottomLeftPanel->GetWide();
		surface()->DrawFilledRect( x1, y1, x2, y2 );

		m_pTopLeftPanel->GetPos( x1, y1 );
		m_pGapPanel->GetPos( x2, y2 );
		x1 += m_pTopLeftPanel->GetWide();
		y2 += m_pGapPanel->GetTall();
		surface()->DrawFilledRect( x1, y1, x2, y2 );

		m_pGapPanel->GetPos( x1, y1 );
		m_pBottomRightPanel->GetPos( x2, y2 );
		x1 += m_pGapPanel->GetWide();
		y2 += m_pBottomRightPanel->GetTall();
		surface()->DrawFilledRect( x1, y1, x2, y2 );

		m_pTopRightPanel->GetPos( x1, y1 );
		m_pBottomRightPanel->GetPos( x2, y2 );
		y1 += m_pTopRightPanel->GetTall();
		x2 += m_pBottomRightPanel->GetWide();
		surface()->DrawFilledRect( x1, y1, x2, y2 );
	}
	else
	{
		m_pTopLeftPanel->GetPos( x1, y1 );
		m_pTopRightPanel->GetPos( x2, y2 );
		x1 += m_pTopLeftPanel->GetWide();
		y2 += m_pTopRightPanel->GetTall();
		surface()->DrawFilledRect( x1, y1, x2, y2 );

		m_pTopLeftPanel->GetPos( x1, y1 );
		m_pGapPanel->GetPos( x2, y2 );
		y1 += m_pTopLeftPanel->GetTall();
		x2 += m_pGapPanel->GetWide();
		surface()->DrawFilledRect( x1, y1, x2, y2 );

		m_pGapPanel->GetPos( x1, y1 );
		m_pBottomRightPanel->GetPos( x2, y2 );
		y1 += m_pGapPanel->GetTall();
		x2 += m_pBottomRightPanel->GetWide();
		surface()->DrawFilledRect( x1, y1, x2, y2 );

		m_pBottomLeftPanel->GetPos( x1, y1 );
		m_pBottomRightPanel->GetPos( x2, y2 );
		x1 += m_pBottomLeftPanel->GetWide();
		y2 += m_pBottomRightPanel->GetTall();
		surface()->DrawFilledRect( x1, y1, x2, y2 );
	}
}

//--------------------------------------------------------------------------------------------------------------
void CCareerBaseBox::ShowWindow( void )
{
	SetVisible( true );
	SetEnabled( true );
	MoveToFront();
}

//--------------------------------------------------------------------------------------------------------------
void CCareerBaseBox::DoModal( void )
{
	ShowWindow();
	input()->SetAppModalSurface( GetVPanel() );
}

//--------------------------------------------------------------------------------------------------------------
void CCareerBaseBox::UndoModal( void )
{
	if ( s_currentBox && s_currentBox->GetVPanel() == input()->GetAppModalSurface() )
		input()->SetAppModalSurface( 0 );
}

//--------------------------------------------------------------------------------------------------------------
void CCareerBaseBox::OnKeyCodeTyped( KeyCode code )
{
	if ( code == KEY_ESCAPE )
	{
		OnCommand( "Cancel" );
		if ( g_pBaseUI )
		{
			g_pBaseUI->HideGameUI();
		}
	}
	else if ( code == KEY_ENTER )
	{
		OnCommand( "Ok" );
		return;
	}

	Frame::OnKeyCodeTyped( code );
}

//--------------------------------------------------------------------------------------------------------------
void CCareerBaseBox::OnCommand( const char *command )
{
	if ( !_stricmp( command, "Ok" ) )
	{
	}

	if ( !strncmp( command, "engine ", 7 ) )
	{
		gEngfuncs.pfnClientCmd( SharedVarArgs( "%s\n", command + 7 ) );
	}
	else if ( _stricmp( command, "close" ) )
	{
		PostActionSignal( new KeyValues( "Command", "command", command ) );
	}

	Frame::OnCommand( command );

	Close();
}

//--------------------------------------------------------------------------------------------------------------
void CCareerBaseBox::OnClose( void )
{
	if ( s_currentBox == this )
	{
		s_currentBox = NULL;
	}
	Frame::OnClose();
	MarkForDeletion();
}

//--------------------------------------------------------------------------------------------------------------
CCareerQueryBox::CCareerQueryBox( const char *, const char *, const char *panelName, Panel *parent ) : CCareerBaseBox( parent, panelName, true )
{
}

//--------------------------------------------------------------------------------------------------------------
CCareerQueryBox::CCareerQueryBox( const wchar_t *, const wchar_t *, const char *panelName, Panel *parent ) : CCareerBaseBox( parent, panelName, true )
{
}

//--------------------------------------------------------------------------------------------------------------
CCareerQueryBox::CCareerQueryBox( Panel *parent, const char *panelName ) : CCareerBaseBox( parent, panelName, true )
{
}

//--------------------------------------------------------------------------------------------------------------
CCareerQueryBox::~CCareerQueryBox( void )
{
}

//--------------------------------------------------------------------------------------------------------------
CCareerTextEntryBox::CCareerTextEntryBox( Panel *parent,
										 const char *title,
										 const char *labelText,
										 const char *command,
										 TextEntryCallback *callback ) : CCareerBaseBox( parent, "TextEntryBox", false )
{
	Label *pTitle = NULL;
	if ( title )
	{
		pTitle = new Label( this, "TitleLabel", L"" );
	}

	m_pEntry = new TextEntry( this, "TextEntry" );
	LoadControlSettings( "resource/UI/TextEntryBox.res", NULL );
	m_callback = callback;
	m_command = NULL;
	SetCommand( command );

	if ( pTitle )
	{
		pTitle->SetText( title );
	}

	if ( labelText )
	{
		SetLabelText( labelText );
	}

	m_pEntry->SetText( L"" );
}

//--------------------------------------------------------------------------------------------------------------
CCareerTextEntryBox::~CCareerTextEntryBox( void )
{
}

//--------------------------------------------------------------------------------------------------------------
void CCareerTextEntryBox::ShowWindow( void )
{
	CCareerBaseBox::ShowWindow();

	m_pEntry->RequestFocus( 0 );
	InvalidateLayout( false, false );
}

//--------------------------------------------------------------------------------------------------------------
void CCareerTextEntryBox::SetCommand( const char *command )
{
	if ( m_command )
	{
		delete[] m_command;
		m_command = NULL;
	}
	if ( command )
	{
		m_command = CloneString( command );
	}
}

//--------------------------------------------------------------------------------------------------------------
void CCareerTextEntryBox::SetCallback( TextEntryCallback *callback )
{
	m_callback = callback;
}

//--------------------------------------------------------------------------------------------------------------
TextEntry *CCareerTextEntryBox::GetTextEntry( void )
{
	return m_pEntry;
}

//--------------------------------------------------------------------------------------------------------------
void CCareerTextEntryBox::OnCommand( const char *command )
{
	if ( !_stricmp( command, "Ok" ) )
	{
		if ( m_callback )
		{
			m_callback( m_pEntry );
		}
		if ( m_command )
		{
			CCareerBaseBox::OnCommand( m_command );
		}
		else
		{
			CCareerBaseBox::OnCommand( "Ok" );
		}
	}
	else
	{
		CCareerBaseBox::OnCommand( command );
	}
}

//--------------------------------------------------------------------------------------------------------------
void CCareerTextEntryBox::OnKeyCodeTyped( KeyCode code )
{
	if ( code == KEY_ESCAPE )
	{
		OnCommand( "Cancel" );
		if ( g_pBaseUI )
		{
			g_pBaseUI->HideGameUI();
		}
	}
	else if ( code == KEY_ENTER )
	{
		OnCommand( "Ok" );
	}
	else
	{
		CCareerBaseBox::OnKeyCodeTyped( code );
	}
}

// sorted order weapon list -----------------------------------------------------------------------
const int NUM_SECONDARY_WEAPONS = 7;
static WeaponIdType s_secondaryWeapons[NUM_SECONDARY_WEAPONS] =
{
	WEAPON_NONE,
	WEAPON_USP,
	WEAPON_GLOCK18,
	WEAPON_DEAGLE,
	WEAPON_ELITE,
	WEAPON_P228,
	WEAPON_FIVESEVEN,
};

const int NUM_PRIMARY_WEAPONS = 24;
static WeaponIdType s_primaryWeapons[NUM_PRIMARY_WEAPONS] =
{
	WEAPON_NONE,

	// Assault Rifles
	(WeaponIdType)-WEAPONCLASS_RIFLE,
	WEAPON_SG552,
	WEAPON_AUG,
	WEAPON_AK47,
	WEAPON_M4A1,
	WEAPON_GALIL,
	WEAPON_FAMAS,

	// Snipers
	(WeaponIdType)-WEAPONCLASS_SNIPERRIFLE,
	WEAPON_AWP,
	WEAPON_SG550,
	WEAPON_G3SG1,
	WEAPON_SCOUT,

	// SMG
	(WeaponIdType)-WEAPONCLASS_SUBMACHINEGUN,
	WEAPON_P90,
	WEAPON_UMP45,
	WEAPON_MP5N,
	WEAPON_MAC10,
	WEAPON_TMP,

	// Heavy
	(WeaponIdType)-WEAPONCLASS_SHOTGUN,
	WEAPON_M249,
	WEAPON_XM1014,
	WEAPON_M3,
	WEAPON_SHIELDGUN,
};

//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
class CWeaponButton : public Button
{
public:
	CWeaponButton( BuyPresetListBox *pParent, WeaponIdType weaponID, bool isPrimary ) : Button( pParent->GetParent(), SharedVarArgs( "WeaponButton%d", weaponID ), "" )
	{
		m_isPrimary = isPrimary;
		m_pImage = NULL;
		m_pTitleImage = new TextImage( WeaponIDToDisplayName( weaponID ) );
		m_pCostImage = new TextImage( L"" );

		m_pListBox = pParent;
		BuyPresetWeapon weapon( weaponID );
		SetWeapon( weapon );
	}

	virtual ~CWeaponButton( void ) { delete m_pTitleImage; delete m_pCostImage; }

	virtual void ApplySchemeSettings( IScheme *pScheme );

	virtual Color GetBgColor( void ) { return ( this == s_current ) ? m_selectedBgColor : Panel::GetBgColor(); }
	virtual Color GetFgColor( void ) { return ( this == s_current ) ? m_selectedFgColor : Label::GetFgColor(); }

	void SetCurrent( void ) { s_current = this; }
	bool IsCurrent( void ) const { return this == s_current; }

	void SetWeapon( const BuyPresetWeapon &weapon );
	const BuyPresetWeapon &GetWeapon( void ) { return m_weapon; }

	virtual void Paint( void );
	virtual void PerformLayout( void );

	virtual void OnMousePressed( MouseCode code )
	{
		if ( code == MOUSE_LEFT )
		{
			SetCurrent();
			m_pListBox->GetParent()->OnCommand( "select_weapon" );
		}
		Button::OnMousePressed( code );
	}

	virtual void OnMouseDoublePressed( MouseCode code )
	{
		if ( code == MOUSE_LEFT )
		{
			SetCurrent();
			m_pListBox->GetParent()->OnCommand( "select_weapon" );
			m_pListBox->GetParent()->OnCommand( "popup_ok" );
		}
		Button::OnMouseDoublePressed( code );
	}

protected:
	static CWeaponButton *s_current;
	BuyPresetListBox *m_pListBox;
	BuyPresetWeapon m_weapon;

	IImage *m_pImage;
	TextImage *m_pTitleImage;
	TextImage *m_pCostImage;
	int m_imageWide;
	int m_imageTall;
	int m_imageX;
	int m_imageY;

	Color m_selectedBgColor;
	Color m_selectedFgColor;

	bool m_isPrimary;
};

//--------------------------------------------------------------------------------------------------------------
CWeaponButton *CWeaponButton::s_current;

//--------------------------------------------------------------------------------------------------------------
void CWeaponButton::ApplySchemeSettings( IScheme *pScheme )
{
	CWeaponButton *current = s_current; // save off current so Button::ApplySchemeSettings() can get the right colors
	s_current = NULL;
	Button::ApplySchemeSettings( pScheme );
	m_selectedBgColor = GetSchemeColor( "ButtonArmedBgColor", Panel::GetBgColor(), pScheme );
	m_selectedFgColor = GetSchemeColor( "ButtonArmedFgColor", Label::GetFgColor(), pScheme );
	s_current = current;
	m_pTitleImage->SetColor( pScheme->GetColor( "BaseText", Color( 0, 0, 0, 0 ) ) );
	m_pTitleImage->SetFont( pScheme->GetFont( "Default", IsProportional() ) );
	m_pTitleImage->SetWrap( true );
	m_pCostImage->SetColor( pScheme->GetColor( "BaseText", Color( 0, 0, 0, 0 ) ) );
	m_pCostImage->SetFont( pScheme->GetFont( "Default", IsProportional() ) );
	m_pCostImage->SetWrap( true );
	SetWeapon( m_weapon );
}

//--------------------------------------------------------------------------------------------------------------
void CWeaponButton::SetWeapon( const BuyPresetWeapon &weapon )
{
	m_weapon = weapon;
	if ( m_weapon.GetName() )
	{
		// weapon string
		const wchar_t *name = m_weapon.GetName();
		m_pTitleImage->SetText( name );

		// cost string
		const WeaponInfoStruct *info = GetWeaponInfo( m_weapon.GetWeaponID() );
		if ( info )
		{
			const int BufLen = 256;
			wchar_t wbuf[BufLen];
			localize()->ConstructString( wbuf, sizeof( wbuf ),
				SharedFindString( "#Cstrike_BuyPresetPlainCost" ),
				1, NumAsWString( info->cost ) );
			m_pCostImage->SetText( wbuf );
		}
		else
		{
			m_pCostImage->SetText( L"" );
		}
	}
	else
	{
		m_pTitleImage->SetText( L"" );
		m_pCostImage->SetText( L"" );
	}

	m_pTitleImage->ResizeImageToContent();
	m_pCostImage->ResizeImageToContent();

	InvalidateLayout( true, false );

	m_pImage = scheme()->GetImage( ImageFnameFromWeaponID( m_weapon.GetWeaponID(), m_isPrimary ), true );
}

//--------------------------------------------------------------------------------------------------------------
void CWeaponButton::PerformLayout( void )
{
	Label::PerformLayout();

	// Calculate some sizes
	int oldWide, oldTall;
	GetSize( oldWide, oldTall );

	const float IMAGE_PERCENT = 0.42f;
	const float TITLE_PERCENT = 0.45f;
	const float COST_PERCENT = 1.0f - TITLE_PERCENT - IMAGE_PERCENT;

	int maxTitleWide = (int)( TITLE_PERCENT * oldWide );
	int maxImageWide = (int)( IMAGE_PERCENT * oldWide );
	int maxCostWide = (int)( COST_PERCENT * oldWide );
	int textTall = 2 * surface()->GetFontTall( m_pTitleImage->GetFont() );
	if ( textTall != oldTall )
	{
		oldTall = textTall;
		SetSize( oldWide, textTall );
		m_pListBox->InvalidateLayout( false, false );
	}

	// Position the weapon name
	{
		m_pTitleImage->SetSize( maxTitleWide, oldTall );
		m_pTitleImage->RecalculateNewLinePositions();
		m_pTitleImage->ResizeImageToContent();

		int textContentWide, textContentTall;
		m_pTitleImage->GetSize( textContentWide, textContentTall );

		if ( textTall > textContentTall )
		{
			m_pTitleImage->SetSize( maxTitleWide, textContentTall );
			m_pTitleImage->SetPos( maxImageWide, ( textTall - textContentTall ) / 2 );
		}
		else
		{
			m_pTitleImage->SetPos( maxImageWide, 0 );
		}
	}

	// Position the weapon cost
	{
		m_pCostImage->SetSize( maxCostWide, oldTall );
		m_pCostImage->RecalculateNewLinePositions();
		m_pCostImage->ResizeImageToContent();

		int textContentWide, textContentTall;
		m_pCostImage->GetSize( textContentWide, textContentTall );

		if ( textTall > textContentTall )
		{
			m_pCostImage->SetSize( maxCostWide, textContentTall );
			m_pCostImage->SetPos( oldWide - maxCostWide, ( textTall - textContentTall ) / 2 );
		}
		else
		{
			m_pCostImage->SetPos( oldWide - maxCostWide, 0 );
		}
	}

	// Position the weapon image
	m_pImage = scheme()->GetImage( ImageFnameFromWeaponID( m_weapon.GetWeaponID(), m_isPrimary ), true );

	int maxImageTall = textTall;

	m_pImage->GetContentSize( m_imageWide, m_imageTall );
	if ( maxImageTall < m_imageTall )
	{
		m_imageWide = (int)( (float)m_imageWide * maxImageTall / m_imageTall );
		m_imageTall = maxImageTall;
	}
	if ( maxImageWide < m_imageWide )
	{
		m_imageTall = (int)( (float)m_imageTall * maxImageWide / m_imageWide );
		m_imageWide = maxImageWide;
	}
	m_imageY = ( textTall - m_imageTall ) / 2;
	m_imageX = (int)( ( IMAGE_PERCENT * oldWide - m_imageWide ) * 0.5 );
}

//--------------------------------------------------------------------------------------------------------------
void CWeaponButton::Paint( void )
{
	if ( m_pImage )
	{
		m_pImage->SetSize( m_imageWide, m_imageTall );
		m_pImage->SetPos( m_imageX, m_imageY );
		m_pImage->Paint();
		m_pImage->SetSize( 0, 0 );
	}
	m_pTitleImage->Paint();
	m_pCostImage->Paint();

	Label::Paint();
}


//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
class WeaponComboBox : public ComboBox
{
public:
	WeaponComboBox( CWeaponSelectBox *parent, const char *name, int numEntries, bool editable )
		: ComboBox( parent, name, numEntries, editable )
	{
		m_pBox = parent;
	}

	virtual void OnSetText( const wchar_t *newText )
	{
		ComboBox::OnSetText( newText );
		if ( m_pBox )
			m_pBox->UpdateClips();
	}

private:
	CWeaponSelectBox *m_pBox;
};

#pragma optimize( "", off )
//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
CWeaponSelectBox::CWeaponSelectBox( Panel *parent, WeaponSet *pWeaponSet, bool isSecondary ) : CCareerBaseBox( parent, "BuyBoxSelectWeapon", false )
{
	m_pWeaponSet = pWeaponSet;
	m_isSecondary = isSecondary;

	m_numWeapons = NUM_PRIMARY_WEAPONS;
	m_weaponIDs = s_primaryWeapons;

	BuyPresetWeapon *weapon;
	if ( !m_isSecondary )
	{
		weapon = &m_pWeaponSet->m_primaryWeapon;
		if ( !IsPrimaryWeapon( weapon->GetWeaponID() ) && weapon->GetWeaponID() != WEAPON_NONE )
		{
			BuyPresetWeapon tmp( WEAPON_NONE );
			m_pWeaponSet->m_primaryWeapon = tmp;
		}
	}
	else
	{
		m_numWeapons = NUM_SECONDARY_WEAPONS;
		m_weaponIDs = s_secondaryWeapons;
		weapon = &m_pWeaponSet->m_secondaryWeapon;
		if ( !IsSecondaryWeapon( weapon->GetWeaponID() ) && weapon->GetWeaponID() != WEAPON_NONE )
		{
			BuyPresetWeapon tmp( WEAPON_NONE );
			*weapon = tmp;
		}
	}

	int maxClips = 0;
	const WeaponInfoStruct *info = GetWeaponInfo( weapon->GetWeaponID() );
	if ( info )
		maxClips = (int)( ( info->buyClipSize > 0 ) ? ceil( (float)info->maxRounds / (float)info->buyClipSize ) : 0 );
	else
		maxClips = NUM_CLIPS_FOR_CURRENT; // so we can buy ammo for our current gun

	m_pClips = new WeaponComboBox( this, "Clips", 2 * maxClips + 1, false );
	m_pClips->SetOpenDirection( ComboBox::UP );
	m_pListBox = new BuyPresetListBox( this, "WeaponListBox" );
	m_pBullets = new Label( this, "bullets", "" );

	int selectedWeaponIndex = -1;
	int i;
	for ( i = 0; i < m_numWeapons; ++i )
	{
		if ( (int)m_weaponIDs[i] < 0 )
		{
			const char *text = "";
			switch ( (int)m_weaponIDs[i] )
			{
			case -WEAPONCLASS_RIFLE:
				text = "#Cstrike_BuyPresetCategoryRifle";
				break;
			case -WEAPONCLASS_SNIPERRIFLE:
				text = "#Cstrike_BuyPresetCategorySniper";
				break;
			case -WEAPONCLASS_SUBMACHINEGUN:
				text = "#Cstrike_BuyPresetCategorySMG";
				break;
			case -WEAPONCLASS_SHOTGUN:
				text = "#Cstrike_BuyPresetCategoryHeavy";
				break;
			}
			Label *pLabel = new Label( m_pListBox, SharedVarArgs( "weaponlabel%d", i ), text );
			m_pListBox->AddItem( pLabel, NULL );
		}
		else
		{
			CWeaponButton *pWeaponButton = new CWeaponButton( m_pListBox, m_weaponIDs[i], !m_isSecondary );
			m_pListBox->AddItem( pWeaponButton, NULL );
			if ( m_weaponIDs[i] == weapon->GetWeaponID() )
			{
				pWeaponButton->SetCurrent();
				selectedWeaponIndex = i;
			}
		}
	}
	m_pListBox->MakeItemVisible( selectedWeaponIndex );

	LoadControlSettings( "resource/UI/BuyBoxSelectWeapon.res", NULL );
	PopulateControls();
}

//--------------------------------------------------------------------------------------------------------------
CWeaponSelectBox::~CWeaponSelectBox( void )
{
}

//--------------------------------------------------------------------------------------------------------------
void CWeaponSelectBox::ActivateBuildMode( void )
{
	SetClipsVisible( true );
	m_pListBox->DeleteAllItems();
	CCareerBaseBox::ActivateBuildMode();
}

//--------------------------------------------------------------------------------------------------------------
void CWeaponSelectBox::SetClipsVisible( bool visible )
{
	SetLabelVisible( visible );
	m_pClips->SetVisible( visible );
}

//--------------------------------------------------------------------------------------------------------------
void CWeaponSelectBox::PopulateControls( void )
{
	BuyPresetWeapon *weapon;
	if ( !m_isSecondary )
	{
		weapon = &m_pWeaponSet->m_primaryWeapon;
	}
	else
	{
		weapon = &m_pWeaponSet->m_secondaryWeapon;
	}

	int i;
	int maxClips = 0;
	const WeaponInfoStruct *info = GetWeaponInfo( weapon->GetWeaponID() );
	if ( info )
		maxClips = (int)( ( info->buyClipSize > 0 ) ? ceil( (float)info->maxRounds / (float)info->buyClipSize ) : 0 );
	else
		maxClips = NUM_CLIPS_FOR_CURRENT;
	m_pClips->SetNumberOfEditLines( 2 * maxClips + 1 );

	SetClipsVisible( maxClips != 0 );

	m_pClips->DeleteAllItems();

	// populate clips combo box
	m_pClips->AddItem( "#Cstrike_BuyPresetEditWeaponFullClips", NULL );

	const int BufLen = 64;
	wchar_t buf[BufLen];
	for ( i = maxClips - 1; i >= 0; --i )
	{
		char *clipsOrMore = "#Cstrike_BuyPresetEditClipsOrMore";
		char *clips = "#Cstrike_BuyPresetEditClips";
		if ( i == 1 )
		{
			clipsOrMore = "#Cstrike_BuyPresetEditClipOrMore";
			clips = "#Cstrike_BuyPresetEditClip";
		}
		localize()->ConstructString( buf, sizeof( buf ),
			SharedFindString( clipsOrMore ),
			1, NumAsWString( i ) );
		m_pClips->AddItem( buf, NULL );
		localize()->ConstructString( buf, sizeof( buf ),
			SharedFindString( clips ),
			1, NumAsWString( i ) );
		m_pClips->AddItem( buf, NULL );
	}

	// now select the proper entry
	int clipIndexToSelect = weapon->GetFillAmmo();
	clipIndexToSelect += 2 * weapon->GetAmmoAmount();
	clipIndexToSelect = 2 * maxClips - clipIndexToSelect;
	m_pClips->ActivateItem( clipIndexToSelect );

	if ( m_isSecondary )
	{
		Panel *pPanel = FindChildByName( "TitleLabel", false );
		if ( pPanel )
		{
			const wchar_t *title = localize()->Find( "#Cstrike_BuyPresetWizardSecondary" );
			if ( title )
				PostMessage( pPanel, new KeyValues( "SetText", "text", title ), 0 );
		}
	}

	UpdateClips();
}
#pragma optimize( "", on )

//--------------------------------------------------------------------------------------------------------------
void CWeaponSelectBox::UpdateClips( void )
{
	if ( !m_pClips )
		return;

	int numEntries = m_pClips->GetItemCount();
	int activeID = m_pClips->GetActiveItem();
	int combined = numEntries - activeID + 1;
	int numClips = combined / 2 - 1;
	//bool isFill = (combined%2) != 0;

	BuyPresetWeapon *weapon;
	if ( !m_isSecondary )
	{
		weapon = &m_pWeaponSet->m_primaryWeapon;
	}
	else
	{
		weapon = &m_pWeaponSet->m_secondaryWeapon;
	}

	const WeaponInfoStruct *info = GetWeaponInfo( weapon->GetWeaponID() );
	if ( info )
	{
		m_pBullets->SetVisible( true );

		const int BufLen = 64;
		wchar_t buf[BufLen];
		localize()->ConstructString( buf, sizeof( buf ),
			localize()->Find( "#Cstrike_BuyPresetsBullets" ),
			2, NumAsWString( min( info->maxRounds, numClips * info->buyClipSize ) ), NumAsWString( info->maxRounds ) );
		m_pBullets->SetText( buf );
	}
	else
	{
		m_pBullets->SetVisible( false );
	}
}

//--------------------------------------------------------------------------------------------------------------
void CWeaponSelectBox::ApplySchemeSettings( IScheme *pScheme )
{
	CCareerBaseBox::ApplySchemeSettings( pScheme );

	HFont font = pScheme->GetFont( "Default", IsProportional() );
	int tall = surface()->GetFontTall( font ) + 2;
	if ( font != 0 )
	{
		Menu *pMenu = m_pClips->GetMenu();
		pMenu->SetMenuItemHeight( tall );
	}

	for ( int i = 0; i < m_pListBox->GetItemCount(); ++i )
	{
		if ( (int)m_weaponIDs[i] >= 0 )
		{
			CWeaponButton *pButton = (CWeaponButton *)m_pListBox->GetItemPanel( i );
			if ( pButton && pButton->IsCurrent() )
			{
				m_pListBox->MakeItemVisible( i );
			}
		}
		else
		{
			// it's a caption label
			Label *pLabel = (Label *)m_pListBox->GetItemPanel( i );
			if ( pLabel )
			{
				pLabel->SetContentAlignment( Label::a_center );
				pLabel->SetBorder( pScheme->GetBorder( "ButtonBorder" ) );
				pLabel->SetBgColor( pScheme->GetColor( "ControlDarkBG", Color( 0, 0, 0, 255 ) ) );
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
WeaponIdType CWeaponSelectBox::GetSelectedWeaponID( void )
{
	int selectedIndex = -1;
	for ( int i = 0; i < m_pListBox->GetItemCount(); ++i )
	{
		if ( (int)m_weaponIDs[selectedIndex] >= 0 )
		{
			CWeaponButton *pButton = (CWeaponButton *)m_pListBox->GetItemPanel( i );
			if ( pButton && pButton->IsCurrent() )
			{
				selectedIndex = i;
			}
		}
	}
	if ( selectedIndex >= 0 )
	{
		return m_weaponIDs[selectedIndex];
	}

	return WEAPON_NONE;
}

//--------------------------------------------------------------------------------------------------------------
void CWeaponSelectBox::OnCommand( const char *command )
{
	if ( !_stricmp( command, "select_weapon" ) )
	{
		WeaponIdType weaponID = GetSelectedWeaponID();
		BuyPresetWeapon weapon( weaponID );
		if ( m_isSecondary )
		{
			m_pWeaponSet->m_secondaryWeapon = weapon;
		}
		else
		{
			m_pWeaponSet->m_primaryWeapon = weapon;
		}

		PopulateControls();
		return;
	}

	if ( !_stricmp( command, "popup_ok" ) )
	{
		if ( !m_pClips )
			return;

		int numEntries = m_pClips->GetItemCount();
		int activeID = m_pClips->GetActiveItem();
		int combined = numEntries - activeID + 1;
		int numClips = combined / 2 - 1;
		bool isFill = ( combined & 1 ) != 0;

		BuyPresetWeapon weapon( GetSelectedWeaponID() );
		weapon.SetAmmoType( AMMO_CLIPS );
		weapon.SetAmmoAmount( numClips );
		weapon.SetFillAmmo( isFill );

		if ( m_isSecondary )
		{
			m_pWeaponSet->m_secondaryWeapon = weapon;
		}
		else
		{
			m_pWeaponSet->m_primaryWeapon = weapon;
		}
		CCareerBaseBox::OnCommand( command );
	}
	else
	{
		CCareerBaseBox::OnCommand( command );
	}
}

//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
class EquipmentComboBox : public ComboBox
{
public:
	EquipmentComboBox( CBaseSelectBox *parent, const char *name, int numEntries, bool editable )
		: ComboBox( parent, name, numEntries, editable )
	{
		m_pBox = parent;
	}

	virtual void OnSetText( const wchar_t *newText )
	{
		ComboBox::OnSetText( newText );
		m_pBox->OnControlChanged();
	}

private:
	CBaseSelectBox *m_pBox;
};

//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
CGrenadeSelectBox::CGrenadeSelectBox( Panel *parent, WeaponSet *pWeaponSet ) : CBaseSelectBox( parent, "BuyBoxSelectGrenades", false )
{
	m_pWeaponSet = pWeaponSet;

	// Equipment controls
	m_pHEGrenade = new EquipmentComboBox( this, "hegrenade", 2, false );
	m_pSmokeGrenade = new EquipmentComboBox( this, "smokegrenade", 2, false );
	m_pFlashbangs = new EquipmentComboBox( this, "flashbangs", 3, false );

	// Equipment images
	m_pHEGrenadeImage = new EquipmentLabel( this, "HEGrenadeImage", NULL );
	m_pSmokeGrenadeImage = new EquipmentLabel( this, "SmokeGrenadeImage", NULL );
	m_pFlashbangImage = new EquipmentLabel( this, "FlashbangImage", NULL );

	m_pHELabel = new Label( this, "HECost", "" );
	m_pSmokeLabel = new Label( this, "SmokeCost", "" );
	m_pFlashLabel = new Label( this, "FlashCost", "" );

	LoadControlSettings( "Resource/UI/BuyBoxWizardGrenades.res", NULL );

	// Add entries to the combo boxes
	m_pHEGrenade->AddItem( "0", NULL );
	m_pHEGrenade->AddItem( "1", NULL );

	m_pSmokeGrenade->AddItem( "0", NULL );
	m_pSmokeGrenade->AddItem( "1", NULL );

	m_pFlashbangs->AddItem( "0", NULL );
	m_pFlashbangs->AddItem( "1", NULL );
	m_pFlashbangs->AddItem( "2", NULL );

	// populate the data
	m_pHEGrenade->ActivateItem( m_pWeaponSet->m_HEGrenade );
	m_pSmokeGrenade->ActivateItem( m_pWeaponSet->m_smokeGrenade );
	m_pFlashbangs->ActivateItem( m_pWeaponSet->m_flashbangs );

	m_pHEGrenadeImage->SetItem( "gfx/vgui/buy_presets/hegrenade", 1 );
	m_pSmokeGrenadeImage->SetItem( "gfx/vgui/buy_presets/smokegrenade", 1 );
	m_pFlashbangImage->SetItem( "gfx/vgui/buy_presets/flashbang", 1 );

	OnControlChanged();
}

//--------------------------------------------------------------------------------------------------------------
void CGrenadeSelectBox::OnControlChanged( void )
{
	const int BufLen = 256;
	wchar_t wbuf[BufLen];

	int numGrenades = m_pHEGrenade->GetActiveItem();
	localize()->ConstructString( wbuf, sizeof( wbuf ),
		SharedFindString( "#Cstrike_BuyPresetPlainCost" ),
		1, NumAsWString( 300 * numGrenades ) );
	m_pHELabel->SetText( wbuf );

	numGrenades = m_pSmokeGrenade->GetActiveItem();
	localize()->ConstructString( wbuf, sizeof( wbuf ),
		SharedFindString( "#Cstrike_BuyPresetPlainCost" ),
		1, NumAsWString( 300 * numGrenades ) );
	m_pSmokeLabel->SetText( wbuf );

	numGrenades = m_pFlashbangs->GetActiveItem();
	localize()->ConstructString( wbuf, sizeof( wbuf ),
		SharedFindString( "#Cstrike_BuyPresetPlainCost" ),
		1, NumAsWString( 200 * numGrenades ) );
	m_pFlashLabel->SetText( wbuf );
}

//--------------------------------------------------------------------------------------------------------------
void CGrenadeSelectBox::OnCommand( const char *command )
{
	if ( !_stricmp( command, "popup_ok" ) )
	{
		// stuff values back in m_pWeaponSet
		m_pWeaponSet->m_HEGrenade = m_pHEGrenade->GetActiveItem() != 0;
		m_pWeaponSet->m_smokeGrenade = m_pSmokeGrenade->GetActiveItem() != 0;
		m_pWeaponSet->m_flashbangs = m_pFlashbangs->GetActiveItem();
	}
	CCareerBaseBox::OnCommand( command );
}

//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
CEquipmentSelectBox::CEquipmentSelectBox( Panel *parent, WeaponSet *pWeaponSet ) : CBaseSelectBox( parent, "BuyBoxSelectEquipment", false )
{
	m_pWeaponSet = pWeaponSet;

	// Equipment controls
	m_pKevlar = new EquipmentComboBox( this, "kevlar", 2, false );
	m_pHelmet = new EquipmentComboBox( this, "helmet", 2, false );
	m_pDefuser = new EquipmentComboBox( this, "defuser", 2, false );
	m_pNightvision = new EquipmentComboBox( this, "nightvision", 2, false );

	// Equipment labels
	m_pKevlarLabel = new Label( this, "kevlarCost", "" );
	m_pHelmetLabel = new Label( this, "helmetCost", "" );
	m_pDefuserLabel = new Label( this, "defuserCost", "" );
	m_pNightvisionLabel = new Label( this, "nightvisionCost", "" );

	// Equipment images
	m_pKevlarImage = new EquipmentLabel( this, "KevlarImage", NULL );
	m_pHelmetImage = new EquipmentLabel( this, "HelmetImage", NULL );
	m_pDefuserImage = new EquipmentLabel( this, "DefuserImage", NULL );
	m_pNightvisionImage = new EquipmentLabel( this, "NightvisionImage", NULL );

	LoadControlSettings( "Resource/UI/BuyBoxWizardEquipment.res", NULL );

	// Add entries to the combo boxes
	m_pKevlar->AddItem( "0", NULL );
	m_pKevlar->AddItem( "1", NULL );

	m_pHelmet->AddItem( "0", NULL );
	m_pHelmet->AddItem( "1", NULL );

	m_pDefuser->AddItem( "0", NULL );
	m_pDefuser->AddItem( "1", NULL );

	m_pNightvision->AddItem( "0", NULL );
	m_pNightvision->AddItem( "1", NULL );

	// populate the data
	m_pKevlar->ActivateItem( m_pWeaponSet->m_armor > 0 );
	m_pHelmet->ActivateItem( m_pWeaponSet->m_armor != 0 && m_pWeaponSet->m_helmet );
	m_pDefuser->ActivateItem( m_pWeaponSet->m_defuser );
	m_pNightvision->ActivateItem( m_pWeaponSet->m_nightvision );

	m_pKevlarImage->SetItem( "gfx/vgui/buy_presets/kevlar", 1 );
	m_pHelmetImage->SetItem( "gfx/vgui/buy_presets/helmet", 1 );
	m_pDefuserImage->SetItem( "gfx/vgui/buy_presets/defuser", 1 );
	m_pNightvisionImage->SetItem( "gfx/vgui/buy_presets/nightvision", 1 );

	OnControlChanged();
}

//--------------------------------------------------------------------------------------------------------------
void CEquipmentSelectBox::OnControlChanged( void )
{
	const int BufLen = 256;
	wchar_t wbuf[BufLen];

	int count = m_pKevlar->GetActiveItem();
	localize()->ConstructString( wbuf, sizeof( wbuf ),
		SharedFindString( "#Cstrike_BuyPresetPlainCost" ),
		1, NumAsWString( 650 * count ) );
	m_pKevlarLabel->SetText( wbuf );

	m_pHelmet->SetEnabled( count != 0 );
	if ( count == 0 && m_pHelmet->GetActiveItem() != 0 )
		m_pHelmet->ActivateItem( 0 );

	count = m_pHelmet->GetActiveItem();
	localize()->ConstructString( wbuf, sizeof( wbuf ),
		SharedFindString( "#Cstrike_BuyPresetPlainCost" ),
		1, NumAsWString( 350 * count ) );
	m_pHelmetLabel->SetText( wbuf );

	count = m_pDefuser->GetActiveItem();
	localize()->ConstructString( wbuf, sizeof( wbuf ),
		SharedFindString( "#Cstrike_BuyPresetPlainCost" ),
		1, NumAsWString( 200 * count ) );
	m_pDefuserLabel->SetText( wbuf );

	count = m_pNightvision->GetActiveItem();
	localize()->ConstructString( wbuf, sizeof( wbuf ),
		SharedFindString( "#Cstrike_BuyPresetPlainCost" ),
		1, NumAsWString( 1250 * count ) );
	m_pNightvisionLabel->SetText( wbuf );
}

//--------------------------------------------------------------------------------------------------------------
void CEquipmentSelectBox::OnCommand( const char *command )
{
	if ( !_stricmp( command, "popup_ok" ) )
	{
		// stuff values back in m_pWeaponSet
		m_pWeaponSet->m_armor = m_pKevlar->GetActiveItem() != 0 ? 100 : 0;
		m_pWeaponSet->m_helmet = m_pWeaponSet->m_armor != 0 && m_pHelmet->GetActiveItem() != 0;
		m_pWeaponSet->m_defuser = m_pDefuser->GetActiveItem() != 0;
		m_pWeaponSet->m_nightvision = m_pNightvision->GetActiveItem() != 0;
	}
	CCareerBaseBox::OnCommand( command );
}
