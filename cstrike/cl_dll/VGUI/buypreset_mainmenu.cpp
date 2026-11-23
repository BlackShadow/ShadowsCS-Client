#include "buypreset_mainmenu.h"
#include "buypreset_editmenu.h"
#include "WeaponSetLabel.h"
#include "CareerBox.h"
#include "buy_presets.h"
#include "Button.h"
#include "Label.h"
#include "game_controls/iviewport.h"
#include "vgui/ILocalize.h"
#include "vgui/IVGui.h"
#include "vgui_controls/Controls.h"
#include "shared_util.h"
#include "cl_util.h"
#include "net_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

using namespace vgui2;

static CBuyPresetEditMainMenu *pMainMenu;
static bool runUpdate = true;
static bool reopenBuyMenu;

//--------------------------------------------------------------------------------------------------------------
class MouseFocusPanel : public Frame
{
	DECLARE_CLASS_SIMPLE_NAMED( MouseFocusPanel, Frame, "vgui::Frame" );

public:
	MouseFocusPanel( void );
	virtual void OnTick( void );
	virtual void OnClose( void );
	virtual void Paint( void ) {}
	virtual void PaintBackground( void ) {}

private:
	int m_framesLeft;
};

//--------------------------------------------------------------------------------------------------------------
MouseFocusPanel::MouseFocusPanel( void ) : Frame( NULL, "MouseFocusPanel", true, false )
{
	SetMoveable( false );
	SetSizeable( false );
	SetTitleBarVisible( false );
	m_framesLeft = 2;
	ivgui()->AddTickSignal( GetVPanel(), 0 );
}

//--------------------------------------------------------------------------------------------------------------
void MouseFocusPanel::OnTick( void )
{
	--m_framesLeft;
	if ( m_framesLeft <= 0 )
	{
		OnClose();
	}
}

//--------------------------------------------------------------------------------------------------------------
void MouseFocusPanel::OnClose( void )
{
	Frame::OnClose();
	MarkForDeletion();
}

//--------------------------------------------------------------------------------------------------------------
void MaintainBuyPresetInputFocus( void )
{
	MouseFocusPanel *pPanel = new MouseFocusPanel;
	pPanel->SetParent( gViewPortInterface->GetViewPortPanel() );
	pPanel->Activate();
}

#pragma warning( push )
#pragma warning( disable : 4459 )
//--------------------------------------------------------------------------------------------------------------
void ShowBuyPresetMainMenu( bool runUpdate, int reopenBuyMenu )
{
	net_status_t status;
	gEngfuncs.pNetAPI->Status( &status );
	if ( status.connected )
	{
		if ( gViewPortInterface )
		{
			::runUpdate = runUpdate;
			if ( reopenBuyMenu == REOPEN_YES )
			{
				::reopenBuyMenu = true;
			}
			else if ( reopenBuyMenu == REOPEN_NO )
			{
				::reopenBuyMenu = false;
			}
			MaintainBuyPresetInputFocus();
			gViewPortInterface->HideVGUIMenu( 39 );
			gViewPortInterface->ShowVGUIMenu( 38 );
		}
	}
}
#pragma warning( pop )

//--------------------------------------------------------------------------------------------------------------
#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
CBuyPresetEditMainMenu::CBuyPresetEditMainMenu( Panel *parent ) : CCSCareerBaseMenu( parent, "BuyPresetEditMainMenu", false )
{
	SetTitle( "#Cstrike_BuyPresetEditPresets", true );

	m_pSaveButton = new Button( this, "save", "" );
	m_pTitleLabel = new Label( this, "Title", "" );

	for ( int i = 0; i < 4; ++i )
	{
		m_pWeaponSetLabel[i] = new BuyPresetEditPanel( this, SharedVarArgs( "LabelPreset%dChoice1", i + 1 ), "Resource/UI/BuyPresetViewPanel.res", i );
	}

	LoadControlSettings( "Resource/UI/BuyPresetEditMainMenu.res", NULL );
}

//--------------------------------------------------------------------------------------------------------------
CBuyPresetEditMainMenu::~CBuyPresetEditMainMenu( void )
{
	if ( this == pMainMenu )
	{
		pMainMenu = NULL;
	}
}

//--------------------------------------------------------------------------------------------------------------
void CBuyPresetEditMainMenu::Update( void )
{
	m_dirty = false;

	gEngfuncs.pfnClientCmd( "setpause\n" );
	gViewPortInterface->CloseTutorTextWindow();

	UpdateScheme();

	pMainMenu = this;

	if ( runUpdate )
	{
		TheBuyPresets->ResetEditPresets();
		SetDirty( false );
	}
	UpdateButtons();
}

//--------------------------------------------------------------------------------------------------------------
void CBuyPresetEditMainMenu::ApplySchemeSettings( IScheme *pScheme )
{
	Frame::ApplySchemeSettings( pScheme );

	UpdateButtons();
}

//--------------------------------------------------------------------------------------------------------------
void CBuyPresetEditMainMenu::UpdateButtons( void )
{
	int i;
	m_dirty = false;
	if ( TheBuyPresets->GetNumEditPresets() != TheBuyPresets->GetNumPresets() )
	{
		m_dirty = true;
	}
	else
	{
		for ( i = 0; i < TheBuyPresets->GetNumEditPresets(); ++i )
		{
			const BuyPreset *pPreset = TheBuyPresets->GetPreset( i );
			const BuyPreset *pEditPreset = TheBuyPresets->GetEditPreset( i );

			if ( wcscmp( pPreset->GetName(), pEditPreset->GetName() ) )
			{
				m_dirty = true;
				break;
			}

			if ( pPreset->GetNumSets() != pEditPreset->GetNumSets() )
			{
				m_dirty = true;
				break;
			}

			for ( int choiceIndex = 0; choiceIndex < pPreset->GetNumSets(); ++choiceIndex )
			{
				const WeaponSet *pSet = pPreset->GetSet( choiceIndex );
				const WeaponSet *pEditSet = pEditPreset->GetSet( choiceIndex );
				if ( !pPreset || !pEditPreset )
				{
					m_dirty = true;
					break;
				}

				if ( pSet->m_armor != pEditSet->m_armor
					|| pSet->m_helmet != pEditSet->m_helmet
					|| pSet->m_smokeGrenade != pEditSet->m_smokeGrenade
					|| pSet->m_HEGrenade != pEditSet->m_HEGrenade
					|| pSet->m_flashbangs != pEditSet->m_flashbangs
					|| pSet->m_defuser != pEditSet->m_defuser
					|| pSet->m_nightvision != pEditSet->m_nightvision
					|| pSet->m_extraCash != pEditSet->m_extraCash
					|| pSet->m_primaryWeapon.GetWeaponID() != pEditSet->m_primaryWeapon.GetWeaponID()
					|| pSet->m_primaryWeapon.GetAmmoAmount() != pEditSet->m_primaryWeapon.GetAmmoAmount()
					|| pSet->m_primaryWeapon.GetAmmoType() != pEditSet->m_primaryWeapon.GetAmmoType()
					|| pSet->m_primaryWeapon.GetFillAmmo() != pEditSet->m_primaryWeapon.GetFillAmmo()
					|| pSet->m_secondaryWeapon.GetWeaponID() != pEditSet->m_secondaryWeapon.GetWeaponID()
					|| pSet->m_secondaryWeapon.GetAmmoAmount() != pEditSet->m_secondaryWeapon.GetAmmoAmount()
					|| pSet->m_secondaryWeapon.GetAmmoType() != pEditSet->m_secondaryWeapon.GetAmmoType()
					|| pSet->m_secondaryWeapon.GetFillAmmo() != pEditSet->m_secondaryWeapon.GetFillAmmo() )
				{
					m_dirty = true;
					break;
				}
			}

			if ( m_dirty )
				break;
		}
	}

	SetDirty( m_dirty );

	for ( i = 0; i < 4; ++i )
	{
		BuyPreset *preset = TheBuyPresets->GetEditPreset( i );

		if ( preset )
		{
			wchar_t buffer[128];
			localize()->ConstructString( buffer, sizeof( buffer ), localize()->Find( "#Cstrike_BuyPresetPresetButtonLabel" ), 2, preset->GetName(), NumAsWString( preset->FullCost() ) );

			const WeaponSet *weaponSet = preset->GetSet( 0 );
			m_pWeaponSetLabel[i]->SetVisible( weaponSet != NULL );
			if ( weaponSet )
			{
				m_pWeaponSetLabel[i]->SetWeaponSet( weaponSet, false );
			}
			else
			{
				m_pWeaponSetLabel[i]->SetWeaponSet( NULL, false );
			}
			m_pWeaponSetLabel[i]->SetText( buffer );
		}
		else
		{
			m_pWeaponSetLabel[i]->SetWeaponSet( NULL, false );
			m_pWeaponSetLabel[i]->SetText( localize()->Find( "#Cstrike_BuyPresetBlank" ) );
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
void CBuyPresetEditMainMenu::SetDirty( bool dirty )
{
	m_dirty = dirty;
	if ( dirty )
	{
		m_pTitleLabel->SetText( "#Cstrike_BuyPresetEditPresetsDirty" );
		m_pSaveButton->SetEnabled( true );
	}
	else
	{
		m_pTitleLabel->SetText( "#Cstrike_BuyPresetEditPresets" );
		m_pSaveButton->SetEnabled( false );
	}
}

//--------------------------------------------------------------------------------------------------------------
void CBuyPresetEditMainMenu::EditPreset( int index )
{
	ShowBuyPresetEditMenu( index );
}

//--------------------------------------------------------------------------------------------------------------
void CBuyPresetEditMainMenu::OnCommand( const char *command )
{
	const int BufLen = 32;
	char commandBuf[BufLen] = { 0 };
	char argBuf[BufLen] = { 0 };

	const char *remainder = SharedParse( command );
	const char *token;

	if ( remainder )
	{
		token = SharedGetToken();
		_snprintf( commandBuf, BufLen, "%s", token );
		remainder = SharedParse( remainder );

		if ( remainder )
		{
			token = SharedGetToken();
			_snprintf( argBuf, BufLen, "%s", token );
		}
	}

	if ( !stricmp( command, "Cancel" ) )
	{
		Frame::OnCommand( command );
		return;
	}

	if ( !stricmp( command, "save_presets" ) )
	{
		TheBuyPresets->SetPresets( TheBuyPresets->GetEditPresets() );
		TheBuyPresets->Save();
		SetDirty( false );
		command = "vguicancel";
	}
	else if ( !stricmp( command, "close_with_save_check" ) )
	{
		if ( m_dirty )
		{
			CCareerQueryBox *pBox = new CCareerQueryBox( this, "BuyBoxQuitNoSave" );
			pBox->AddActionSignalTarget( this );
			pBox->ShowWindow();
			Frame::OnCommand( command );
			return;
		}
		else
		{
			command = "vguicancel";
		}
	}
	else if ( !stricmp( command, "use_defaults" ) )
	{
		CCareerQueryBox *pBox = new CCareerQueryBox( this, "BuyBoxUseDefaults" );
		pBox->AddActionSignalTarget( this );
		pBox->ShowWindow();
		Frame::OnCommand( command );
		return;
	}
	else if ( !stricmp( command, "use_defaults_ok" ) )
	{
		TheBuyPresets->ResetEditToDefaults();
		SetDirty( true );
		UpdateButtons();
		Frame::OnCommand( command );
		return;
	}
	else if ( !stricmp( commandBuf, "edit_preset" ) && argBuf[0] )
	{
		EditPreset( atoi( argBuf ) );
		Frame::OnCommand( command );
		return;
	}

	if ( !stricmp( command, "vguicancel" ) )
	{
		if ( reopenBuyMenu )
		{
			MaintainBuyPresetInputFocus();
			gViewPortInterface->HideBackGround();
			OnClose();
			gViewPortInterface->HideVGUIMenu( 38 );
			gViewPortInterface->HideVGUIMenu( 39 );
			gViewPortInterface->ShowVGUIMenu( 28 );
		}
		else
		{
			OnClose();
		}
	}
	Frame::OnCommand( command );
}
