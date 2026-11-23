#pragma once

#include "basecareermenu.h"

class BuyPresetEditPanel;
namespace vgui2 { class Label; class Button; }

class CBuyPresetEditMainMenu : public CCSCareerBaseMenu
{
public:
	CBuyPresetEditMainMenu( vgui2::Panel *parent );
	virtual ~CBuyPresetEditMainMenu( void );

	void Update( void );
	void EditPreset( int index );

private:
	void UpdateButtons( void );
	virtual void OnCommand( const char *command );
	virtual void ApplySchemeSettings( vgui2::IScheme *pScheme );
	void SetDirty( bool dirty );

	vgui2::Label *m_pTitleLabel;
	vgui2::Button *m_pSaveButton;

	BuyPresetEditPanel *m_pWeaponSetLabel[4];

	bool m_dirty;
};

void MaintainBuyPresetInputFocus( void );
void ShowBuyPresetMainMenu( bool runUpdate, int reopenBuyMenu );
