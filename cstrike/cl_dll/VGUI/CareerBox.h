//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose:
//
//=============================================================================

// Author: Matthew D. Campbell (matt@turtlerockstudios.com), 2003

#pragma once

#include "vgui/IScheme.h"
#include "Frame.h"
#include "Label.h"
#include "weapontype.h"

class WeaponSet;
class BuyPresetListBox;
namespace vgui2
{
class Button;
class Label;
class ComboBox;
class TextEntry;
class CBitmapImagePanel;
}
class EquipmentLabel;

//--------------------------------------------------------------------------------------------------------------
/**
 *  Base class for career popup dialogs (handles custom backgrounds, etc)
 */
class CCareerBaseBox : public vgui2::Frame
{
public:
	CCareerBaseBox( vgui2::Panel *parent, const char *panelName, bool loadResources );
	virtual ~CCareerBaseBox( void );
	virtual void ShowWindow( void );
	virtual void DoModal( void );
	virtual void ApplySchemeSettings( vgui2::IScheme *pScheme );
	virtual void PaintBackground( void );
	virtual void Paint( void );
	virtual void PerformLayout( void );
	virtual void OnClose( void );
	virtual void OnCommand( const char *command );
	void SetLabelText( const wchar_t *text );
	void SetLabelText( const char *text );
	static void CloseOpenBoxes( bool force );
	static void BringToFront( void );
	static void UndoModal( void );
	virtual void ActivateBuildMode( void );

	void SetAutoClose( bool autoClose ) { m_shouldAutoClose = autoClose; }

	void FriendLoadResources( const char *resources );

	static bool IsBoxOpen( void ) { return s_currentBox != NULL; }

private:
	vgui2::Button *m_pOkButton;

	vgui2::CBitmapImagePanel *m_pTopLeftPanel;
	vgui2::CBitmapImagePanel *m_pTopRightPanel;
	vgui2::CBitmapImagePanel *m_pBottomLeftPanel;
	vgui2::CBitmapImagePanel *m_pBottomRightPanel;

	vgui2::CBitmapImagePanel *m_pTopLeftBorderPanel;
	vgui2::CBitmapImagePanel *m_pTopRightBorderPanel;
	vgui2::CBitmapImagePanel *m_pBottomLeftBorderPanel;
	vgui2::CBitmapImagePanel *m_pBottomRightBorderPanel;

	Color m_bgColor;
	Color m_borderColor;

	vgui2::Label *m_pTextLabel;
	vgui2::Panel *m_pGapPanel;
	vgui2::CBitmapImagePanel *m_pIconPanel;

protected:
	virtual void OnKeyCodeTyped( vgui2::KeyCode code );

	void SetLabelVisible( bool visible ) { m_pTextLabel->SetVisible( visible ); }

	bool m_shouldAutoClose;
	static CCareerBaseBox *s_currentBox;
};

//--------------------------------------------------------------------------------------------------------------
/**
 *  Popup dialog with functionality similar to QueryBox, bot with different layout
 */
class CCareerQueryBox : public CCareerBaseBox
{
public:
	CCareerQueryBox( const char *title, const char *labelText, const char *panelName, vgui2::Panel *parent );
	CCareerQueryBox( const wchar_t *title, const wchar_t *labelText, const char *panelName, vgui2::Panel *parent );
	CCareerQueryBox( vgui2::Panel *parent, const char *panelName );

	virtual ~CCareerQueryBox( void );
};

//--------------------------------------------------------------------------------------------------------------
typedef void TextEntryCallback( vgui2::TextEntry * );
class CCareerTextEntryBox : public CCareerBaseBox
{
public:
	CCareerTextEntryBox( vgui2::Panel *parent, const char *title, const char *labelText, const char *command, TextEntryCallback *callback );

	virtual void ShowWindow( void );
	virtual ~CCareerTextEntryBox( void );

	void SetCommand( const char *command );
	void SetCallback( TextEntryCallback *callback );
	vgui2::TextEntry *GetTextEntry( void );

protected:
	virtual void OnCommand( const char *command );
	virtual void OnKeyCodeTyped( vgui2::KeyCode code );

private:
	vgui2::TextEntry *m_pEntry;
	char *m_command;
	TextEntryCallback *m_callback;
};

//--------------------------------------------------------------------------------------------------------------
/**
 *  Popup dialog for selecting and editing a primary weapon (ammo to buy, side-availability)
 */
class CWeaponSelectBox : public CCareerBaseBox
{
public:
	CWeaponSelectBox( vgui2::Panel *parent, WeaponSet *pWeaponSet, bool isSecondary );

	virtual ~CWeaponSelectBox( void );

	virtual void ActivateBuildMode( void );

	void UpdateClips( void );

protected:
	virtual void OnCommand( const char *command );
	virtual void ApplySchemeSettings( vgui2::IScheme *pScheme );

private:
	void PopulateControls( void );
	void SetClipsVisible( bool visible );

	WeaponIdType GetSelectedWeaponID( void );

	vgui2::ComboBox *m_pClips;	///< Number of clips to purchase
	vgui2::Label *m_pBullets;	///< Label showing "M of N Bullets"

	WeaponSet *m_pWeaponSet;	///< WeaponSet being edited
	bool m_isSecondary;			///< is this weapon primary or secondary?
	BuyPresetListBox *m_pListBox;	///< List of weapons from which to choose

	int m_numWeapons;
	WeaponIdType *m_weaponIDs;
};

//--------------------------------------------------------------------------------------------------------------
/**
 *  Base class for editing grenades and equipment
 */
class CBaseSelectBox : public CCareerBaseBox
{
public:
	CBaseSelectBox( vgui2::Panel *parent, const char *panelName, bool loadResources ) : CCareerBaseBox( parent, panelName, loadResources ) {}

	virtual void OnControlChanged( void ) = 0;
};

//--------------------------------------------------------------------------------------------------------------
/**
 *  Popup dialog for editing grenades in a buy preset fallback
 */
class CGrenadeSelectBox : public CBaseSelectBox
{
public:
	CGrenadeSelectBox( vgui2::Panel *parent, WeaponSet *pWeaponSet );

	virtual void OnControlChanged( void );		///< Updates item costs

private:
	WeaponSet *m_pWeaponSet;		///< WeaponSet being edited

	// Equipment controls
	vgui2::ComboBox		*m_pHEGrenade;
	EquipmentLabel		*m_pHEGrenadeImage;
	vgui2::Label		*m_pHELabel;

	vgui2::ComboBox		*m_pSmokeGrenade;
	EquipmentLabel		*m_pSmokeGrenadeImage;
	vgui2::Label		*m_pSmokeLabel;

	vgui2::ComboBox		*m_pFlashbangs;
	EquipmentLabel		*m_pFlashbangImage;
	vgui2::Label		*m_pFlashLabel;

	virtual void OnCommand( const char *command );
};

//--------------------------------------------------------------------------------------------------------------
/**
 *  Popup dialog for selecting an equipment set
 */
class CEquipmentSelectBox : public CBaseSelectBox
{
public:
	CEquipmentSelectBox( vgui2::Panel *parent, WeaponSet *pWeaponSet );

	virtual void OnControlChanged( void );

private:
	WeaponSet *m_pWeaponSet;		///< WeaponSet being edited

	// Equipment controls
	vgui2::ComboBox		*m_pKevlar;
	vgui2::Label		*m_pKevlarLabel;
	EquipmentLabel		*m_pKevlarImage;

	vgui2::ComboBox		*m_pHelmet;
	vgui2::Label		*m_pHelmetLabel;
	EquipmentLabel		*m_pHelmetImage;

	vgui2::ComboBox		*m_pDefuser;
	vgui2::Label		*m_pDefuserLabel;
	EquipmentLabel		*m_pDefuserImage;

	vgui2::ComboBox		*m_pNightvision;
	vgui2::Label		*m_pNightvisionLabel;
	EquipmentLabel		*m_pNightvisionImage;

	virtual void OnCommand( const char *command );
};
