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

#include "basecareermenu.h"
#include "GameUI_Interfaces.h"
#include "GameUI/ICareerUI.h"
#include "BuildGroup.h"
#include "vgui_controls/Controls.h"
#include "vgui/ISurface.h"

CCSCareerBaseMenu::CCSCareerBaseMenu( vgui2::Panel *parent, const char *name, bool showTaskBar ) : vgui2::Frame( parent, name, showTaskBar, false )
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

	vgui2::Frame::SetTitle( "", true );

	vgui2::Frame::SetMoveable( false );
	vgui2::Frame::SetSizeable( false );

	vgui2::Frame::SetTitleBarVisible( false );

	m_offsetX = m_offsetY = 0;
}


CCSCareerBaseMenu::~CCSCareerBaseMenu( void )
{
}


void CCSCareerBaseMenu::ActivateBuildMode( void )
{
	vgui2::EditablePanel::ActivateBuildMode();
}


void CCSCareerBaseMenu::UpdateScheme( void )
{
	if ( g_pCareerUI && g_pCareerUI->IsPlayingMatch() )
	{
		SetScheme( "CareerClientScheme" );
	}
	else
	{
		SetScheme( "ClientScheme" );
	}
	InvalidateLayout( false, true );
}


void CCSCareerBaseMenu::Activate( void )
{
	SetPos( 0, 0 );
	vgui2::Frame::Activate();

	if ( !IsProportional() )
		return;
	int screenW, screenH;
	vgui2::surface()->GetScreenSize( screenW, screenH );

	int wide, tall;
	GetSize( wide, tall );

	if ( wide == screenW && tall == screenH )
		return;

	SetSize( screenW, screenH );

	wide = GetAlternateProportionalValueFromScaled( wide );
	tall = GetAlternateProportionalValueFromScaled( tall );

	m_offsetX = ( screenW - wide ) / 2;
	m_offsetY = ( screenH - tall ) / 2;

	if ( !GetBuildGroup() || !GetBuildGroup()->GetPanelList() )
		return;

	CUtlVector<vgui2::PHandle> *panelList = GetBuildGroup()->GetPanelList();
	CUtlVector<vgui2::Panel *> resizedPanels;
	CUtlVector<vgui2::Panel *> movedPanels;

	for ( int i = 0; i < panelList->Count(); ++i )
	{
		vgui2::PHandle handle = (*panelList)[i];

		vgui2::Panel *panel = handle.Get();

		bool found = false;
		for ( int j = 0; j < resizedPanels.Count(); ++j )
		{
			if ( panel == resizedPanels[j] )
				found = true;
		}

		if ( panel && !found )
		{
			resizedPanels.AddToTail( panel );

			if ( panel != this )
			{
				RepositionControl( panel );
			}
		}
	}

	for ( int i = 0; i < panelList->Count(); ++i )
	{
		vgui2::PHandle handle = (*panelList)[i];

		vgui2::Panel *panel = handle.Get();

		bool found = false;
		for ( int j = 0; j < movedPanels.Count(); ++j )
		{
			if ( panel == movedPanels[j] )
				found = true;
		}

		if ( panel && !found )
		{
			movedPanels.AddToTail( panel );

			if ( panel != this )
			{
				int x, y;

				panel->GetPos( x, y );
				panel->SetPos( m_offsetX + x, m_offsetY + y );
			}
		}
	}
}
