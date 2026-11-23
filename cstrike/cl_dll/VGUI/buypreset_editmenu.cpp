#include "buypreset_editmenu.h"
#include "buypreset_mainmenu.h"
#include "WeaponSetLabel.h"
#include "CareerBox.h"
#include "Button.h"
#include "Label.h"
#include "TextEntry.h"
#include "game_controls/iviewport.h"
#include "vgui/IScheme.h"
#include "cl_util.h"
#include "shared_util.h"
#include <stdio.h>
#include <stdlib.h>

using namespace vgui2;

static CBuyPresetEditMenu *pMenu;
static int thePresetIndex;

//--------------------------------------------------------------------------------------------------------------
void ShowBuyPresetEditMenu( int presetIndex )
{
	if ( gViewPortInterface )
	{
		thePresetIndex = presetIndex;

		MaintainBuyPresetInputFocus();

		gViewPortInterface->HideVGUIMenu( 38 );
		gViewPortInterface->ShowVGUIMenu( 39 );
	}
}

//--------------------------------------------------------------------------------------------------------------
class PresetNameTextEntry : public TextEntry
{
public:
	PresetNameTextEntry( CBuyPresetEditMenu *parent, const char *name ) : TextEntry( parent, name )
	{
		m_pMenu = parent;
	}

	virtual void FireActionSignal( void )
	{
		TextEntry::FireActionSignal();
		m_pMenu->SetDirty();
	}

private:
	CBuyPresetEditMenu *m_pMenu;
};

//--------------------------------------------------------------------------------------------------------------
CBuyPresetEditMenu::CBuyPresetEditMenu( Panel *parent ) : CCSCareerBaseMenu( parent, "BuyPresetEditOutfitListMenu", false )
{
	m_isDirty = false;

	SetTitle( "#Cstrike_BuyPresetEditPresets", true );
	m_pTitleLabel = new Label( this, "Title", "" );
	m_pAcceptButton = new Button( this, "ok", "" );

	m_pPresetName = new PresetNameTextEntry( this, "PresetName" );
	for ( int i = 0; i < NUM_PRESET_CHOICES; ++i )
	{
		m_pChoicePanel[i] = new BuyPresetEditPanel( this, SharedVarArgs( "choice%d", i ), "Resource/UI/BuyPresetEditPanel.res", i );
	}

	LoadControlSettings( "Resource/UI/BuyPresetEditSubMenu.res", NULL );
}

//--------------------------------------------------------------------------------------------------------------
CBuyPresetEditMenu::~CBuyPresetEditMenu( void )
{
	if ( this == pMenu )
	{
		pMenu = NULL;
	}
}

//--------------------------------------------------------------------------------------------------------------
void CBuyPresetEditMenu::ApplySchemeSettings( IScheme *pScheme )
{
	Frame::ApplySchemeSettings( pScheme );

	m_selectedFgColor = GetSchemeColor( "BrightControlText", pScheme );
	m_selectedBgColor = GetSchemeColor( "SelectionBG", pScheme );
	m_normalFgColor = GetSchemeColor( "BaseText", pScheme );
	m_alternateFgColor = GetSchemeColor( "BaseText", pScheme );
	m_normalBgColor = GetSchemeColor( "ControlDarkBG", pScheme );
	m_alternateBgColor = GetSchemeColor( "ControlDarkBG", pScheme );

	UpdateButtons();
}

//--------------------------------------------------------------------------------------------------------------
void CBuyPresetEditMenu::Update( void )
{
	gEngfuncs.pfnClientCmd( "setpause\n" );
	gViewPortInterface->CloseTutorTextWindow();

	const BuyPreset *preset = TheBuyPresets->GetEditPreset( thePresetIndex );
	if ( preset )
	{
		m_pPresetName->SetText( preset->GetName() );
		m_originalBuyPreset = *preset;
	}
	else
	{
		BuyPreset tmp;
		m_originalBuyPreset = tmp;
	}

	m_isDirty = false;
	m_selectedIndex = -1;
	UpdateButtons();
}

//--------------------------------------------------------------------------------------------------------------
void CBuyPresetEditMenu::SetDirty( void )
{
	m_isDirty = true;

	UpdateButtons();
}

//--------------------------------------------------------------------------------------------------------------
void CBuyPresetEditMenu::UpdateButtons( void )
{
	if ( m_isDirty )
	{
		m_pTitleLabel->SetText( "#Cstrike_BuyPresetEditPresetTitleDirty" );
		m_pAcceptButton->SetEnabled( true );
	}
	else
	{
		m_pTitleLabel->SetText( "#Cstrike_BuyPresetEditPresetTitle" );
		m_pAcceptButton->SetEnabled( false );
	}

	bool sawBlank = false;
	m_numValidChoices = 0;

	const BuyPreset *preset = TheBuyPresets->GetEditPreset( thePresetIndex );
	if ( !preset )
	{
		sawBlank = true;
	}
	for ( int i = 0; i < NUM_PRESET_CHOICES; ++i )
	{
		const WeaponSet *weaponSet = NULL;
		if ( preset )
			weaponSet = preset->GetSet( i );

		if ( !weaponSet )
		{
			m_pChoicePanel[i]->SetWeaponSet( NULL, false );
		}
		else
		{
			m_pChoicePanel[i]->SetWeaponSet( weaponSet, false );
		}

		m_pChoicePanel[i]->SetVisible( !sawBlank );

		if ( m_selectedIndex == i )
		{
			m_pChoicePanel[i]->SetPanelBgColor( m_selectedBgColor );
		}
		else
		{
			if ( i & 1 )
			{
				m_pChoicePanel[i]->SetPanelBgColor( m_normalBgColor );
			}
			else
			{
				m_pChoicePanel[i]->SetPanelBgColor( m_alternateBgColor );
			}
		}

		if ( !weaponSet )
		{
			sawBlank = true;
		}

		if ( !sawBlank )
		{
			++m_numValidChoices;
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
void CBuyPresetEditMenu::FinishEditingWeaponSet( void )
{
	BuyPreset *preset = TheBuyPresets->GetEditPreset( thePresetIndex );
	if ( preset )
		preset->ReplaceSet( m_selectedIndex, m_weaponSet );

	if ( !m_weaponSet.m_armor &&
		!m_weaponSet.m_defuser &&
		!m_weaponSet.m_flashbangs &&
		!m_weaponSet.m_HEGrenade &&
		!m_weaponSet.m_helmet &&
		!m_weaponSet.m_nightvision &&
		!m_weaponSet.m_smokeGrenade &&
		m_weaponSet.m_primaryWeapon.GetWeaponID() == WEAPON_NONE &&
		!m_weaponSet.m_primaryWeapon.GetAmmoAmount() &&
		m_weaponSet.m_secondaryWeapon.GetWeaponID() == WEAPON_NONE &&
		!m_weaponSet.m_secondaryWeapon.GetAmmoAmount() )
	{
		DeleteWeaponSet();
	}

	m_selectedIndex = -1;

	UpdateButtons();
}

//--------------------------------------------------------------------------------------------------------------
void CBuyPresetEditMenu::EditFallback( int fallbackIndex )
{
	m_selectedIndex = fallbackIndex;
	BuyPreset *preset = TheBuyPresets->GetEditPreset( thePresetIndex );
	if ( !preset )
		return;

	const WeaponSet *weaponSet = preset->GetSet( m_selectedIndex );
	if ( weaponSet )
	{
		m_weaponSet = *weaponSet;
	}
	else if ( m_selectedIndex == preset->GetNumSets() )
	{
		m_weaponSet.Reset();
	}
	UpdateButtons();
}

//--------------------------------------------------------------------------------------------------------------
void CBuyPresetEditMenu::DeleteWeaponSet( void )
{
	BuyPreset *preset = TheBuyPresets->GetEditPreset( thePresetIndex );
	if ( preset )
		preset->DeleteSet( m_selectedIndex );
	m_selectedIndex = -1;

	UpdateButtons();
}

//--------------------------------------------------------------------------------------------------------------
void CBuyPresetEditMenu::OnCommand( const char *command )
{
	const int BufLen = 32;
	char commandBuf[BufLen] = { 0 };

	const int MaxArgs = 2;
	char argBuf[MaxArgs][BufLen];
	int numArgs = 0;

	const char *remainder = SharedParse( command );
	const char *token;

	if ( remainder )
	{
		token = SharedGetToken();
		_snprintf( commandBuf, BufLen, "%s", token );
		remainder = SharedParse( remainder );
	}
	for ( int i = 0; i < MaxArgs; ++i )
	{
		if ( remainder )
		{
			token = SharedGetToken();
			_snprintf( argBuf[numArgs++], BufLen, "%s", token );
			remainder = SharedParse( remainder );
		}
	}

	BuyPreset *preset = TheBuyPresets->GetEditPreset( thePresetIndex );
	if ( !preset && stricmp( command, "Cancel" ) )
	{
		command = "vguicancel";
	}

	if ( !stricmp( commandBuf, "Cancel" ) )
	{
		if ( numArgs == 1 && !stricmp( argBuf[0], "clear" ) )
		{
			m_selectedIndex = -1;
		}
		UpdateButtons();
		Frame::OnCommand( command );
		return;
	}

	if ( !stricmp( command, "close_with_save_check" ) )
	{
		if ( m_isDirty )
		{
			CCareerQueryBox *pBox = new CCareerQueryBox( this, "BuyBoxQuitNoSave" );
			pBox->AddActionSignalTarget( this );
			pBox->ShowWindow();
		}
		else
		{
			if ( preset )
				*preset = m_originalBuyPreset;
			ShowBuyPresetMainMenu( false, REOPEN_DONTCHANGE );
		}
		Frame::OnCommand( command );
		return;
	}

	if ( !stricmp( commandBuf, "open_edit_main_menu" ) )
	{
		if ( numArgs == 1 && !stricmp( argBuf[0], "save" ) )
		{
			wchar_t nameBuf[MaxBuyPresetName];
			m_pPresetName->GetText( nameBuf, MaxBuyPresetName );
			preset->SetName( nameBuf );
		}
		else
		{
			if ( preset )
				*preset = m_originalBuyPreset;
		}
		ShowBuyPresetMainMenu( false, REOPEN_DONTCHANGE );
		Frame::OnCommand( command );
		return;
	}

	if ( !stricmp( commandBuf, "primary" ) && numArgs == 1 )
	{
		EditFallback( atoi( argBuf[0] ) );
		CCareerBaseBox *pBox = new CWeaponSelectBox( this, &m_weaponSet, false );
		pBox->AddActionSignalTarget( this );
		pBox->ShowWindow();
	}
	else if ( !stricmp( commandBuf, "secondary" ) && numArgs == 1 )
	{
		EditFallback( atoi( argBuf[0] ) );
		CCareerBaseBox *pBox = new CWeaponSelectBox( this, &m_weaponSet, true );
		pBox->AddActionSignalTarget( this );
		pBox->ShowWindow();
	}
	else if ( !stricmp( commandBuf, "grenades" ) && numArgs == 1 )
	{
		EditFallback( atoi( argBuf[0] ) );
		CCareerBaseBox *pBox = new CGrenadeSelectBox( this, &m_weaponSet );
		pBox->AddActionSignalTarget( this );
		pBox->ShowWindow();
	}
	else if ( !stricmp( commandBuf, "equipment" ) && numArgs == 1 )
	{
		EditFallback( atoi( argBuf[0] ) );
		CCareerBaseBox *pBox = new CEquipmentSelectBox( this, &m_weaponSet );
		pBox->AddActionSignalTarget( this );
		pBox->ShowWindow();
	}
	else if ( !stricmp( commandBuf, "move" ) && numArgs == 2 )
	{
		int sourceIndex = atoi( argBuf[1] );

		int numSets = preset->GetNumSets();
		if ( sourceIndex >= 0 && sourceIndex < numSets )
		{
			if ( !stricmp( argBuf[0], "up" ) )
			{
				if ( m_selectedIndex != 0 )
				{
					m_isDirty = true;
					preset->SwapSet( sourceIndex, sourceIndex - 1 );
					m_selectedIndex = -1;
				}
			}
			else if ( !stricmp( argBuf[0], "down" ) )
			{
				if ( sourceIndex < numSets - 1 )
				{
					m_isDirty = true;
					preset->SwapSet( sourceIndex, sourceIndex + 1 );
					m_selectedIndex = -1;
				}
			}
			UpdateButtons();
		}
	}
	else if ( !stricmp( command, "popup_ok" ) )
	{
		m_isDirty = true;
		FinishEditingWeaponSet();
	}

	if ( !stricmp( command, "vguicancel" ) )
	{
		*preset = m_originalBuyPreset;
		ShowBuyPresetMainMenu( false, REOPEN_DONTCHANGE );
	}
	Frame::OnCommand( command );
}
