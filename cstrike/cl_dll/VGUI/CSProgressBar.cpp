/*** 
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

#include "CSProgressBar.h"
#include "hud.h"
#include "GameUI_Interfaces.h"
#include "GameUI/ICareerUI.h"
#include "BitmapImagePanel.h"
#include "Label.h"
#include "vgui_controls/Controls.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "r_studioint.h"

using namespace vgui2;

extern engine_studio_api_t IEngineStudio;


RoundedCornerPanel::RoundedCornerPanel( Panel *parent ) : Panel( parent, "RoundedCornerBackground" )
{
	m_pTopLeftPanel = new CBitmapImagePanel( this, "TopLeftPanel", "gfx/vgui/round_corner_nw" );
	m_pTopRightPanel = new CBitmapImagePanel( this, "TopRightPanel", "gfx/vgui/round_corner_ne" );
	m_pBottomLeftPanel = new CBitmapImagePanel( this, "BottomLeftPanel", "gfx/vgui/round_corner_sw" );
	m_pBottomRightPanel = new CBitmapImagePanel( this, "BottomRightPanel", "gfx/vgui/round_corner_se" );

	m_pTopLeftBorderPanel = new CBitmapImagePanel( this, "TopLeftBorderPanel", "gfx/vgui/round_corner_small_nw" );
	m_pTopRightBorderPanel = new CBitmapImagePanel( this, "TopRightBorderPanel", "gfx/vgui/round_corner_small_ne" );
	m_pBottomLeftBorderPanel = new CBitmapImagePanel( this, "BottomLeftBorderPanel", "gfx/vgui/round_corner_small_sw" );
	m_pBottomRightBorderPanel = new CBitmapImagePanel( this, "BottomRightBorderPanel", "gfx/vgui/round_corner_small_se" );

	m_bgColor = m_borderColor = Color( 0, 0, 0, 0 );
}

RoundedCornerPanel::~RoundedCornerPanel( void )
{
}


void RoundedCornerPanel::ApplySchemeSettings( IScheme *pScheme )
{
	Panel::ApplySchemeSettings( pScheme );

	m_bgColor = GetSchemeColor( "BuddyListBgColor", GetBgColor(), pScheme );
	m_borderColor = pScheme->GetColor( "BorderBright", Color( 0, 0, 0, 0 ) );

	Color imageColor( 255, 255, 255, m_bgColor[3] );
	m_pTopLeftPanel->m_bgColor = imageColor;
	m_pTopRightPanel->m_bgColor = imageColor;
	m_pBottomLeftPanel->m_bgColor = imageColor;
	m_pBottomRightPanel->m_bgColor = imageColor;

	m_pTopLeftBorderPanel->m_bgColor = m_borderColor;
	m_pTopRightBorderPanel->m_bgColor = m_borderColor;
	m_pBottomLeftBorderPanel->m_bgColor = m_borderColor;
	m_pBottomRightBorderPanel->m_bgColor = m_borderColor;
}


void RoundedCornerPanel::PerformLayout( void )
{
	int cornerSize = 10;
	if ( IEngineStudio.IsHardware() != 1 )
		cornerSize = 0;

	bool showCorners = IEngineStudio.IsHardware && IEngineStudio.IsHardware();
	m_pTopLeftPanel->SetVisible( showCorners );
	m_pTopRightPanel->SetVisible( showCorners );
	m_pBottomLeftPanel->SetVisible( showCorners );
	m_pBottomRightPanel->SetVisible( showCorners );
	m_pTopLeftBorderPanel->SetVisible( showCorners );
	m_pTopRightBorderPanel->SetVisible( showCorners );
	m_pBottomLeftBorderPanel->SetVisible( showCorners );
	m_pBottomRightBorderPanel->SetVisible( showCorners );

	int w = GetWide();
	int h = GetTall();

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
}


void RoundedCornerPanel::Draw( void )
{
	int x1, x2, y1, y2;
	surface()->DrawSetColor( m_bgColor );

	if ( IEngineStudio.IsHardware() != 1 )
	{
		m_pTopLeftPanel->GetPos( x1, y1 );
		m_pBottomRightPanel->GetPos( x2, y2 );
		x2 += m_pBottomRightPanel->GetWide();
		y2 += m_pBottomRightPanel->GetTall();
		surface()->DrawFilledRect( x1, y1, x2, y2 );

		surface()->DrawSetColor( m_borderColor );
		surface()->DrawOutlinedRect( x1, y1, x2, y2 );
		return;
	}

	m_pTopLeftPanel->GetPos( x1, y1 );
	m_pBottomLeftPanel->GetPos( x2, y2 );
	y1 += m_pTopLeftPanel->GetTall();
	x2 += m_pBottomLeftPanel->GetWide();
	surface()->DrawFilledRect( x1, y1, x2, y2 );

	m_pTopLeftPanel->GetPos( x1, y1 );
	m_pBottomRightPanel->GetPos( x2, y2 );
	x1 += m_pTopLeftPanel->GetWide();
	y2 += m_pBottomRightPanel->GetTall();
	surface()->DrawFilledRect( x1, y1, x2, y2 );

	m_pTopRightPanel->GetPos( x1, y1 );
	m_pBottomRightPanel->GetPos( x2, y2 );
	y1 += m_pTopRightPanel->GetTall();
	x2 += m_pBottomRightPanel->GetWide();
	surface()->DrawFilledRect( x1, y1, x2, y2 );

	surface()->DrawSetColor( m_borderColor );
	surface()->DrawSetTextColor( m_borderColor );

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


CCSProgressBar::CCSProgressBar( Panel *parent ) : Frame( parent, "CSProgressBar", true, false )
{
	m_isTimeBased = false;
	m_startTime = 0;
	m_numTicks = 0;
	m_startTick = 0;
	m_currentTick = 0;

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

	m_pTitle = new Label( this, "TitleLabel", "" );
	m_pText = new Label( this, "TextLabel", "" );

	m_pProgressBarBorder = new Panel( this, "ProgressBarBorder" );
	m_pProgressBar = new Panel( this, "ProgressBar" );
	m_pProgressBarSizer = new Panel( this, "ProgressBarSizer" );
	m_pBackgroundPanel = new RoundedCornerPanel( this );

	LoadControlSettings( "Resource/UI/CSProgressBar.res", NULL );
}


CCSProgressBar::~CCSProgressBar( void )
{
}


void CCSProgressBar::ApplySchemeSettings( IScheme *pScheme )
{
	Frame::ApplySchemeSettings( pScheme );

	SetBgColor( Color( 0, 0, 0, 0 ) );
	SetBorder( NULL );

	m_pProgressBarSizer->SetVisible( false );
	m_pBackgroundPanel->SetVisible( true );

	m_pProgressBarBorder->SetBorder( pScheme->GetBorder( "ButtonDepressedBorder" ) );
	m_pProgressBarBorder->SetBgColor( Color( 0, 0, 0, 0 ) );

	m_pProgressBar->SetBorder( pScheme->GetBorder( "ButtonBorder" ) );
	m_pProgressBar->SetBgColor( pScheme->GetColor( "SelectionBG", Color( 0, 0, 0, 0 ) ) );
}


void CCSProgressBar::PerformLayout( void )
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

	if ( m_numTicks )
	{
#pragma warning( suppress : 4456 )
		int w = m_pProgressBarSizer->GetWide();
		w = w * m_currentTick / m_numTicks;
		m_pProgressBar->SetWide( w );
	}

	m_pBackgroundPanel->InvalidateLayout( false, false );
}


void CCSProgressBar::PaintBackground( void )
{
	m_pBackgroundPanel->Draw();
}


void CCSProgressBar::OnThink( void )
{
}


void CCSProgressBar::Init( const char *title, int numTicks, int startTick, bool isTimeBased )
{
	m_pTitle->SetText( title );
	m_pText->SetText( "" );

	m_numTicks = numTicks;
	m_startTick = startTick;
	m_isTimeBased = isTimeBased;
	m_currentTick = startTick;

	InvalidateLayout( false, false );
}


void CCSProgressBar::Update( const char *statusText, int tick )
{
	m_pText->SetText( statusText );

	m_currentTick = tick;

	InvalidateLayout( false, false );
}
