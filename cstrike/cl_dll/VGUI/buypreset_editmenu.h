#pragma once

#include "basecareermenu.h"
#include "buy_presets.h"

class BuyPresetEditPanel;
namespace vgui2 { class Button; class Label; class TextEntry; }

enum { NUM_PRESET_CHOICES = 5 };

class CBuyPresetEditMenu : public CCSCareerBaseMenu
{
public:
	CBuyPresetEditMenu( vgui2::Panel *parent );
	virtual ~CBuyPresetEditMenu( void );
	void Update( void );
	void SetDirty( void );

private:
	void FinishEditingWeaponSet( void );
	void UpdateButtons( void );
	virtual void OnCommand( const char *command );
	virtual void ApplySchemeSettings( vgui2::IScheme *pScheme );
	void EditFallback( int fallbackIndex );
	void DeleteWeaponSet( void );

	vgui2::TextEntry *m_pPresetName;
	BuyPresetEditPanel *m_pChoicePanel[NUM_PRESET_CHOICES];
	int m_numValidChoices;
	int m_selectedIndex;
	Color m_selectedFgColor;
	Color m_selectedBgColor;
	Color m_normalFgColor;
	Color m_normalBgColor;
	Color m_alternateFgColor;
	Color m_alternateBgColor;
	WeaponSet m_weaponSet;
	BuyPreset m_originalBuyPreset;
	vgui2::Button *m_pAcceptButton;
	vgui2::Label *m_pTitleLabel;
	bool m_isDirty;
};

void ShowBuyPresetEditMenu( int presetIndex );
