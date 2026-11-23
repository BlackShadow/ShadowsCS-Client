//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose:
//
//=============================================================================

#pragma once

#include "Panel.h"
#include <vector>

namespace vgui2 { class ScrollBar; }

//--------------------------------------------------------------------------------------------------------------
/**
 *  ListBox-style control with behavior needed by weapon lists for BuyPreset editing
 */
class BuyPresetListBox : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE_NAMED( BuyPresetListBox, vgui2::Panel, "vgui::Panel" );

public:
	BuyPresetListBox( vgui2::Panel *parent, const char *panelName );
	virtual ~BuyPresetListBox( void );

	virtual int AddItem( vgui2::Panel *panel, void *userData );	///< Adds an item to the end of the listbox.  UserData is assumed to be a pointer that can be freed by the listbox if non-NULL.
	virtual int GetItemCount( void ) const;						///< Returns the number of items in the listbox
	void SwapItems( int index1, int index2 );					///< Exchanges two items in the listbox
	void MakeItemVisible( int index );							///< Try to ensure that the given index is visible

	vgui2::Panel *GetItemPanel( int index ) const;				///< Returns the panel in the given index, or NULL
	void *GetItemUserData( int index );							///< Returns the userData in the given index, or NULL
	void SetItemUserData( int index, void *userData );			///< Sets the userData in the given index

	virtual void RemoveItem( int index );						///< Removes an item from the table (changing the indices of all following items), deleting the panel and userData
	virtual void DeleteAllItems( void );						///< clears the listbox, deleting all panels and userData

	// overrides
	virtual void OnSizeChanged( int wide, int tall );			////< Handles size changes
	MESSAGE_FUNC_INT( OnSliderMoved, "ScrollBarSliderMoved", position );	///< Handles slider being dragged
	virtual void OnMouseWheeled( int delta );					///< Scrolls the list according to the mouse wheel movement
	virtual void MoveScrollBarToTop( void );					///< Moves slider to the top

protected:

	virtual int computeVPixelsNeeded( void );					///< Computes vertical pixels needed by listbox contents

	virtual void PerformLayout( void );							///< Positions listbox items, etc after internal changes
	virtual void ApplySchemeSettings( vgui2::IScheme *pScheme );	///< Loads colors, fonts, etc

	virtual void OnCommand( const char *command );				///< Passes commands up to the parent

private:
	enum { SCROLLBAR_SIZE = 18, DEFAULT_HEIGHT = 24, PANELBUFFER = 5 };

	typedef struct dataitem_s
	{
		vgui2::Panel *panel;
		void *userData;
	} DataItem;
	std::vector<DataItem>	m_items;

	vgui2::ScrollBar		*m_vbar;
	vgui2::Panel			*m_pPanelEmbedded;

	int						m_iScrollbarSize;
	int						m_iDefaultHeight;
	int						m_iPanelBuffer;

	int						m_visibleIndex;
	int						m_lastSize;
};
