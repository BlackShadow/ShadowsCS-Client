#include "creditsmenu.h"
#include "hud.h"
#include "GameUI_Interfaces.h"
#include "GameUI/ICareerUI.h"
#include "GameUI/IMusicManager.h"
#include "IBaseUI.h"
#include "Label.h"
#include "Button.h"
#include "FileSystem.h"
#include "game_controls/iviewport.h"
#include "vgui_controls/Controls.h"
#include "vgui/ISurface.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

const int StyleLen = 64;
class CreditsLabel : public vgui2::Label
{
public:
	CreditsLabel( vgui2::Panel *parent, const char *panelName, const char *text );

	void SetStyle( const char *style );
	virtual void ApplySchemeSettings( vgui2::IScheme *pScheme );

private:
	char m_style[StyleLen];
};

CreditsLabel::CreditsLabel( vgui2::Panel *parent, const char *panelName, const char *text ) : vgui2::Label( parent, panelName, text )
{
	strcpy( m_style, "Normal" );
}

void CreditsLabel::SetStyle( const char *style )
{
	_snprintf( m_style, sizeof( m_style ), "%s", style );
}

void CreditsLabel::ApplySchemeSettings( vgui2::IScheme *pScheme )
{
	vgui2::Label::ApplySchemeSettings( pScheme );

	const int BufLen = 128;
	char buf[BufLen];

	_snprintf( buf, sizeof( buf ), "Credits%sFG", m_style );
	SetFgColor( pScheme->GetColor( buf, Color( 255, 255, 255, 255 ) ) );

	_snprintf( buf, sizeof( buf ), "Credits%sBG", m_style );
	SetBgColor( pScheme->GetColor( buf, Color( 0, 0, 0, 0 ) ) );

	SetBorder( NULL );

	_snprintf( buf, sizeof( buf ), "Credits%s", m_style );
	SetFont( pScheme->GetFont( buf, true ) );

	SetContentAlignment( vgui2::Label::a_center );
}

CCSCreditsMenu::CCSCreditsMenu( vgui2::Panel *parent ) : vgui2::Frame( parent, "Credits", false, false )
{
	if ( g_pCareerUI && g_pCareerUI->IsPlayingMatch() )
	{
		SetScheme( "CareerClientScheme" );
	}
	else
	{
		SetScheme( "ClientScheme" );
	}
	SetProportional( true );

	int screenW, screenH;
	vgui2::surface()->GetScreenSize( screenW, screenH );

	SetSize( screenW, screenH );

	m_panel = new vgui2::Panel( this );

	SetMenuButtonVisible( false );
	SetMinimizeButtonVisible( false );
	SetCloseButtonVisible( false );
	SetTitle( "", false );

	vgui2::Button *pButton = new vgui2::Button( this, "Button", "" );
	pButton->SetBounds( 0, 0, screenW, screenH );
	pButton->SetPaintBackgroundEnabled( false );
	pButton->SetPaintBorderEnabled( false );
	pButton->MoveToFront();
	pButton->SetCommand( "Close" );
	pButton->AddActionSignalTarget( this );

	ReadCreditsFile();
}

CCSCreditsMenu::~CCSCreditsMenu( void )
{
}

void CCSCreditsMenu::ReadCreditsFile( void )
{
	FileHandle_t file = vgui2::filesystem()->Open( "credits.txt", "r", NULL );
	if ( !file )
		return;

	char line[256];
	char style[64];
	int i = 1;

	while ( vgui2::filesystem()->ReadLine( line, 255, file ) )
	{
		char labelName[32];
		sprintf( labelName, "CreditsLine%d", i );

		char *end = line + strlen( line ) - 1;
		while ( end > line && end && !isprint( *end ) )
		{
			*end-- = 0;
		}

		if ( !strnicmp( line, "@STYLE ", 6 ) && strlen( line ) > 7 )
		{
			_snprintf( style, sizeof( style ), "%s", line + 7 );
			continue;
		}
		if ( line[0] == '\n' || line[0] == '\r' )
		{
			line[0] = 0;
		}
		CreditsLabel *label = new CreditsLabel( m_panel, labelName, line );
		label->SetStyle( style );
		label->SetParent( m_panel );

		m_labelVector.push_back( label );

		++i;
	}

	vgui2::filesystem()->Close( file );
}

void CCSCreditsMenu::PerformLayout( void )
{
	int screenWidth, screenHeight;
	vgui2::surface()->GetScreenSize( screenWidth, screenHeight );

	int currentHeight = 0;

	LabelVectorIter it = m_labelVector.begin();
	while ( it != m_labelVector.end() )
	{
		vgui2::Label *pLabel = *it;

		pLabel->SizeToContents();

		int labelWidth, labelHeight;
		pLabel->GetSize( labelWidth, labelHeight );

		currentHeight += 2;

		pLabel->SetPos( screenWidth / 2 - labelWidth / 2, currentHeight );

		currentHeight += labelHeight;

		++it;
	}

	m_panel->SetSize( screenWidth, currentHeight );

	m_panel->SetVisible( gEngfuncs.GetAbsoluteTime() >= m_startTime );
}


void CCSCreditsMenu::ApplySchemeSettings( vgui2::IScheme *pScheme )
{
	vgui2::Frame::ApplySchemeSettings( pScheme );

	SetBgColor( Color( 0, 0, 0, 255 ) );
}

void CCSCreditsMenu::Activate( void )
{
	vgui2::Frame::Activate();

	if ( g_pMusicManager->IsPlayingTrack( TRACKTYPE_CREDITS ) )
	{
		return;
	}

	if ( g_pBaseUI )
	{
		g_pBaseUI->HideGameUI();
	}

	int screenWidth, screenHeight;
	vgui2::surface()->GetScreenSize( screenWidth, screenHeight );

	m_panelSpeed = 0.1f;
	m_panelOffscreenStart = 0;
	m_startTime = (float)( gEngfuncs.GetAbsoluteTime() + 1 );
	m_panelHideTime = 1;

	g_pMusicManager->StartTrack( TRACKTYPE_CREDITS, true );

	gEngfuncs.pfnClientCmd( "setpause\n" );
	gViewPortInterface->CloseTutorTextWindow();
}

void CCSCreditsMenu::OnThink( void )
{
	int screenWidth, screenHeight;
	vgui2::surface()->GetScreenSize( screenWidth, screenHeight );

	float elapsedTime = (float)( gEngfuncs.GetAbsoluteTime() - m_startTime );
	float elapsedPercent = elapsedTime * m_panelSpeed;

	int panelTop, panelLeft, panelWide, panelTall;
	m_panel->GetBounds( panelLeft, panelTop, panelWide, panelTall );

	panelTop = screenHeight - (int)( screenHeight * elapsedPercent );

	if ( m_panelOffscreenStart && gEngfuncs.GetAbsoluteTime() > m_panelOffscreenStart + m_panelHideTime )
	{
		g_pMusicManager->FadeOutTrack();
		if ( m_shouldDisconnect )
		{
			gEngfuncs.pfnClientCmd( "disconnect\n" );
			g_pCareerUI->OnMatchEndMenuClose( false );
		}
		else
		{
			gEngfuncs.pfnClientCmd( "unpause\n" );
		}
		Close();
	}

	if ( panelTop + panelTall < 0 && m_panelOffscreenStart == 0 )
	{
		m_panelOffscreenStart = (float)gEngfuncs.GetAbsoluteTime();
	}

	m_panel->SetPos( panelLeft, panelTop );
	InvalidateLayout( false, false );
}

void ShowCreditsMenu( bool disconnectAfter )
{
	if ( gViewPortInterface )
	{
		CCSCreditsMenu::SetDisconnect( disconnectAfter );
		gViewPortInterface->ShowVGUIMenu( 37 );
	}
}

bool CCSCreditsMenu::m_shouldDisconnect;

void CCSCreditsMenu::SetDisconnect( bool disconnectAfter )
{
	m_shouldDisconnect = disconnectAfter;
}

void CCSCreditsMenu::OnCommand( const char *command )
{
	float elapsedTime = (float)( gEngfuncs.GetAbsoluteTime() - m_startTime );
	if ( elapsedTime < 3 )
		return;

	g_pMusicManager->FadeOutTrack();
	if ( !stricmp( command, "close" ) )
	{
		if ( m_shouldDisconnect )
		{
			gEngfuncs.pfnClientCmd( "disconnect\n" );
			g_pCareerUI->OnMatchEndMenuClose( false );
		}
		else
		{
			gEngfuncs.pfnClientCmd( "unpause\n" );
		}
		Close();
		OnCommand( "CreditsDone" );
	}
	else
		vgui2::Frame::OnCommand( command );
}
