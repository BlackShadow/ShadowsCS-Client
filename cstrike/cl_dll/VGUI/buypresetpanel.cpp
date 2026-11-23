//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose:
//
//=============================================================================

#include "WeaponSetLabel.h"
#include "buy_presets.h"
#include "Button.h"
#include "Label.h"
#include "vgui/ILocalize.h"
#include "vgui/IScheme.h"
#include "vgui/KeyValues.h"
#include "vgui_controls/Controls.h"
#include "shared_util.h"
#include <string.h>

#ifdef GetClassName
#undef GetClassName
#endif

using namespace vgui2;

//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
BuyPresetEditPanel::BuyPresetEditPanel( Panel *parent, const char *panelName, const char *resourceFilename, int fallbackIndex ) : EditablePanel( parent, panelName )
{
	SetProportional( parent->IsProportional() );
	if ( IsProportional() )
	{
		m_baseWide = m_baseTall = scheme()->GetProportionalScaledValue( 100 );
	}
	else
	{
		m_baseWide = m_baseTall = 100;
	}
	SetSize( m_baseWide, m_baseTall );

	m_fallbackIndex = fallbackIndex;

	m_pTitleLabel = new Label( this, "title", "" );
	m_pCostLabel = new Label( this, "cost", "" );

	m_pPrimaryWeapon = new WeaponLabel( this, "primary" );
	m_pSecondaryWeapon = new WeaponLabel( this, "secondary" );

	m_pHEGrenade = new EquipmentLabel( this, "hegrenade", NULL );
	m_pSmokeGrenade = new EquipmentLabel( this, "smokegrenade", NULL );
	m_pFlashbangs = new EquipmentLabel( this, "flashbang", NULL );

	m_pDefuser = new EquipmentLabel( this, "defuser", NULL );
	m_pNightvision = new EquipmentLabel( this, "nightvision", NULL );

	m_pArmor = new EquipmentLabel( this, "armor", NULL );

	if ( resourceFilename )
	{
		LoadControlSettings( resourceFilename, NULL );
	}

	m_pBgPanel = FindChildByName( "mainBackground", false );
	if ( m_pBgPanel )
	{
		int x, y, w, h;
		m_pBgPanel->GetBounds( x, y, w, h );

		m_baseWide = x + w;
		m_baseTall = y + h;
	}
	SetSize( m_baseWide, m_baseTall );

	Reset();
}

//--------------------------------------------------------------------------------------------------------------
BuyPresetEditPanel::~BuyPresetEditPanel( void )
{
}

//--------------------------------------------------------------------------------------------------------------
void BuyPresetEditPanel::SetWeaponSet( const WeaponSet *pWeaponSet, bool )
{
	// set to empty state
	Reset();

	// now fill in items
	if ( pWeaponSet )
	{
		m_pTitleLabel->SetText( SharedVarArgs( "#Cstrike_BuyPresetChoice%d", m_fallbackIndex ) );

		const int BufLen = 256;
		wchar_t wbuf[BufLen];
		localize()->ConstructString( wbuf, sizeof( wbuf ),
			SharedFindString( "#Cstrike_BuyPresetPlainCost" ),
			1, NumAsWString( pWeaponSet->FullCost() ) );
		m_pCostLabel->SetText( wbuf );

		m_pPrimaryWeapon->SetWeapon( &pWeaponSet->m_primaryWeapon, true, true );
		m_pSecondaryWeapon->SetWeapon( &pWeaponSet->m_secondaryWeapon, false, true );

		if ( pWeaponSet->m_HEGrenade )
			m_pHEGrenade->SetItem( "gfx/vgui/buy_presets/hegrenade", 1 );
		if ( pWeaponSet->m_smokeGrenade )
			m_pSmokeGrenade->SetItem( "gfx/vgui/buy_presets/smokegrenade", 1 );
		if ( pWeaponSet->m_flashbangs )
			m_pFlashbangs->SetItem( "gfx/vgui/buy_presets/flashbang", pWeaponSet->m_flashbangs );

		if ( pWeaponSet->m_defuser )
			m_pDefuser->SetItem( "gfx/vgui/buy_presets/defuser", 1 );
		if ( pWeaponSet->m_nightvision )
			m_pNightvision->SetItem( "gfx/vgui/buy_presets/nightvision", 1 );

		if ( pWeaponSet->m_armor )
		{
			if ( pWeaponSet->m_helmet )
				m_pArmor->SetItem( "gfx/vgui/buy_presets/kevlar_helmet", 1 );
			else
				m_pArmor->SetItem( "gfx/vgui/buy_presets/kevlar", 1 );
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
void BuyPresetEditPanel::SetText( const wchar_t *text )
{
	if ( !text )
		text = L"";
	m_pTitleLabel->SetText( text );
	InvalidateLayout( false, false );
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Handle command callbacks
 */
void BuyPresetEditPanel::OnCommand( const char *command )
{
	if ( stricmp( command, "close" ) )
	{
		PostActionSignal( new KeyValues( "Command", "command", SharedVarArgs( "%s %d", command, m_fallbackIndex ) ) );
	}

	Panel::OnCommand( command );
}

//--------------------------------------------------------------------------------------------------------------
void BuyPresetEditPanel::ApplySchemeSettings( IScheme *pScheme )
{
	Panel::ApplySchemeSettings( pScheme );

	IBorder *pBorder = NULL;

	int i;

	for ( i = 0; i < GetChildCount(); i++ )
	{
		// perform auto-layout on the child panel
		Panel *child = GetChild( i );
		if ( !child )
			continue;

		if ( !stricmp( "button", child->GetClassName() ) )
		{
			Button *pButton = dynamic_cast<Button *>( child );
			if ( pButton )
			{
				pButton->SetDefaultBorder( pBorder );
				pButton->SetDepressedBorder( pBorder );
				pButton->SetKeyFocusBorder( pBorder );
			}
		}
	}

	pBorder = pScheme->GetBorder( "BuyPresetButtonBorder" );

	const int NumButtons = 4;
	const char *buttonNames[] = { "editPrimary", "editSecondary", "editGrenades", "editEquipment" };
	for ( i = 0; i < NumButtons; ++i )
	{
		Panel *pPanel = FindChildByName( buttonNames[i], false );
		if ( pPanel )
		{
			pPanel->SetBorder( pBorder );
			if ( !stricmp( "button", pPanel->GetClassName() ) )
			{
				Button *pButton = dynamic_cast<Button *>( pPanel );
				if ( pButton )
				{
					pButton->SetDefaultBorder( pBorder );
					pButton->SetDepressedBorder( pBorder );
					pButton->SetKeyFocusBorder( pBorder );

					Color fgColor, bgColor;
					fgColor = GetSchemeColor( "LabelDimText", GetFgColor(), pScheme );
					bgColor = GetSchemeColor( "ButtonBgColor", GetFgColor(), pScheme );
					pButton->SetDefaultColor( fgColor, bgColor );
				}
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Overrides EditablePanel's resizing of children to scale them proportionally to the main panel's change.
 */
void BuyPresetEditPanel::OnSizeChanged( int wide, int tall )
{
	if ( !m_baseWide )
		m_baseWide = 1;
	if ( !m_baseTall )
		m_baseTall = 1;

	Panel::OnSizeChanged( wide, tall );
	InvalidateLayout( false, false );

	if ( wide == m_baseWide && tall == m_baseTall )
	{
		Repaint();
		return;
	}

	float xScale = (float)wide / m_baseWide;
	float yScale = (float)tall / m_baseTall;

	for ( int i = 0; i < GetChildCount(); i++ )
	{
		// perform auto-layout on the child panel
		Panel *child = GetChild( i );
		if ( !child )
			continue;

		int x, y, w, t;
		child->GetBounds( x, y, w, t );

		int newX = (int)( x * xScale );
		int newY = (int)( y * yScale );
		int newW = (int)( ( x + w ) * xScale - newX );
		int newT = (int)( t * yScale );

		// make sure the child isn't too big...
		if ( newX + newW > wide )
		{
			continue;
		}

		if ( newY + newT > tall )
		{
			continue;
		}

		child->SetBounds( newX, newY, newW, newT );
		child->InvalidateLayout( false, false );
	}
	Repaint();

	// update the baselines
	m_baseWide = wide;
	m_baseTall = tall;
}

//--------------------------------------------------------------------------------------------------------------
void BuyPresetEditPanel::Reset( void )
{
	m_pTitleLabel->SetText( "#Cstrike_BuyPresetNewChoice" );
	m_pCostLabel->SetText( "" );

	BuyPresetWeapon weapon;
	m_pPrimaryWeapon->SetWeapon( &weapon, true, false );
	m_pSecondaryWeapon->SetWeapon( &weapon, false, false );

	m_pHEGrenade->SetItem( "gfx/vgui/buy_presets/hegrenade_none", 1 );
	m_pSmokeGrenade->SetItem( "gfx/vgui/buy_presets/smokegrenade_none", 1 );
	m_pFlashbangs->SetItem( "gfx/vgui/buy_presets/flashbang_none", 1 );

	m_pDefuser->SetItem( "gfx/vgui/buy_presets/defuser_none", 1 );
	m_pNightvision->SetItem( "gfx/vgui/buy_presets/nightvision_none", 1 );

	m_pArmor->SetItem( "gfx/vgui/buy_presets/kevlar_none", 1 );
}
