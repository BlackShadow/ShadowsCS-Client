//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#include "hud.h"
#include "counterstrikeviewport.h"
#include "BitmapImagePanel.h"
#include "BuildGroup.h"
#include "Label.h"
#include "vgui_controls/Controls.h"
#include "vgui/ISurface.h"
#include "vgui/IScheme.h"
#include "r_studioint.h"

extern engine_studio_api_t IEngineStudio;

using namespace vgui2;


#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
CounterStrikeViewport::CCSBackGroundPanel::CCSBackGroundPanel( Panel *parent ) : TeamFortressViewport::CBackGroundPanel( parent )
{
	m_pTopLeftPanel = new CBitmapImagePanel( this, "TopLeftPanel", "gfx/vgui/round_corner_nw" );
	m_pTopRightPanel = new CBitmapImagePanel( this, "TopRightPanel", "gfx/vgui/round_corner_ne" );
	m_pBottomLeftPanel = new CBitmapImagePanel( this, "BottomLeftPanel", "gfx/vgui/round_corner_sw" );
	m_pBottomRightPanel = new CBitmapImagePanel( this, "BottomRightPanel", "gfx/vgui/round_corner_se" );
	m_pGapPanel = new Panel( this, "GapPanel" );
	m_pTitleLabel = new Label( this, "CaptionLabel", "" );

	m_offsetX = m_offsetY = 0;

	LoadControlSettings( "Resource/UI/BackgroundPanel.res", NULL );
}


CounterStrikeViewport::CCSBackGroundPanel::~CCSBackGroundPanel( void )
{
}


void CounterStrikeViewport::CCSBackGroundPanel::ApplySchemeSettings( IScheme *pScheme )
{
	TeamFortressViewport::CBackGroundPanel::ApplySchemeSettings( pScheme );

	m_titleColor = pScheme->GetColor( "TitleIcon", Color( 0, 0, 0, 200 ) );
	m_pTitleLabel->SetFgColor( m_titleColor );

	m_bgColor = pScheme->GetColor( "ViewportBG", Color( 0, 0, 0, 200 ) );
	m_borderColor = pScheme->GetColor( "BorderBright", Color( 0, 0, 0, 0 ) );

	Color imageColor( 255, 255, 255, m_bgColor.a() );
	m_pTopLeftPanel->m_bgColor = imageColor;
	m_pTopRightPanel->m_bgColor = imageColor;
	m_pBottomLeftPanel->m_bgColor = imageColor;
	m_pBottomRightPanel->m_bgColor = imageColor;

	if ( IEngineStudio.IsHardware && !IEngineStudio.IsHardware() )
	{
		m_pTopLeftPanel->SetVisible( false );
		m_pTopRightPanel->SetVisible( false );
		m_pBottomLeftPanel->SetVisible( false );
		m_pBottomRightPanel->SetVisible( false );
	}
}


void CounterStrikeViewport::CCSBackGroundPanel::SetTitleText( const char *text )
{
	m_pTitleLabel->SetText( text );
}


int numPoints = 256;
void CounterStrikeViewport::CCSBackGroundPanel::PaintBackground( void )
{
	int x1, x2, y1, y2;
	surface()->DrawSetColor( m_bgColor );

	if ( !IEngineStudio.IsHardware() )
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
		int x3, y3;
		m_pTopRightPanel->GetPos( x3, y3 );
		y1 += m_pTopLeftPanel->GetTall();
		x2 = x3 + m_pTopRightPanel->GetWide();
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


int GetAlternateProportionalValueFromNormal( int normalizedValue )
{
	int wide, tall;
	surface()->GetScreenSize( wide, tall );
	int proH, proW;
	surface()->GetProportionalBase( proW, proH );
	double scaleH = (double)tall / (double)proH;
	double scaleW = (double)wide / (double)proW;
	double scale = ( scaleW < scaleH ) ? scaleW : scaleH;

	return (int)( normalizedValue * scale );
}


int GetAlternateProportionalValueFromScaled( int scaledValue )
{
	return GetAlternateProportionalValueFromNormal( scheme()->GetProportionalNormalizedValue( scaledValue ) );
}


void RepositionControl( Panel *pPanel )
{
	int x, y, w, h;
	pPanel->GetBounds( x, y, w, h );

	x = GetAlternateProportionalValueFromScaled( x );
	y = GetAlternateProportionalValueFromScaled( y );
	w = GetAlternateProportionalValueFromScaled( w );
	h = GetAlternateProportionalValueFromScaled( h );

	pPanel->SetBounds( x, y, w, h );
}


void CounterStrikeViewport::CCSBackGroundPanel::PerformLayout( void )
{
	Frame::PerformLayout();
}


void CounterStrikeViewport::CCSBackGroundPanel::Activate( void )
{
	SetPos( 0, 0 );
	Frame::Activate();

	if ( IsProportional() )
	{
		int screenW, screenH;
		surface()->GetScreenSize( screenW, screenH );

		int wide, tall;
		GetSize( wide, tall );

		if ( wide != screenW || tall != screenH )
		{
			SetSize( screenW, screenH );

			wide = GetAlternateProportionalValueFromScaled( wide );
			tall = GetAlternateProportionalValueFromScaled( tall );

			m_offsetX = ( screenW - wide ) / 2;
			m_offsetY = ( screenH - tall ) / 2;

			if ( !GetBuildGroup() || !GetBuildGroup()->GetPanelList() )
				return;

			CUtlVector<PHandle> *panelList = GetBuildGroup()->GetPanelList();
			CUtlVector<Panel *> resizedPanels;
			CUtlVector<Panel *> movedPanels;

			// Resize to account for 1.25 aspect ratio (1280x1024) screens
			{
				for ( int i = 0; i < panelList->Count(); ++i )
				{
					PHandle handle = ( *panelList )[i];

					Panel *panel = handle.Get();

					bool found = false;
					for ( int j = 0; j < resizedPanels.Count(); ++j )
					{
						if ( panel == resizedPanels[j] )
							found = true;
					}

					if ( !panel || found )
					{
						continue;
					}

					resizedPanels.AddToTail( panel ); // don't move a panel more than once

					if ( panel != this )
					{
						RepositionControl( panel );
					}
				}
			}

			// and now re-center them.  Woohoo!
			for ( int i = 0; i < panelList->Count(); ++i )
			{
				PHandle handle = ( *panelList )[i];

				Panel *panel = handle.Get();

				bool found = false;
				for ( int j = 0; j < movedPanels.Count(); ++j )
				{
					if ( panel == movedPanels[j] )
						found = true;
				}

				if ( !panel || found )
				{
					continue;
				}

				movedPanels.AddToTail( panel ); // don't move a panel more than once

				if ( panel != this )
				{
					int x, y;

					panel->GetPos( x, y );
					panel->SetPos( x + m_offsetX, y + m_offsetY );
				}
			}
		}
	}
}
