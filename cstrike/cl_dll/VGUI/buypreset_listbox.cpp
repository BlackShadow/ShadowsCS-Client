//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose:
//
//=============================================================================

#include "buypreset_listbox.h"
#include "EditablePanel.h"
#include "ScrollBar.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "vgui_controls/Controls.h"
#include "minmax.h"

#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"cstrike\\cl_dll\\VGUI\\buypreset_listbox.cpp" )

using namespace vgui2;

//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
BuyPresetListBox::BuyPresetListBox( Panel *parent, const char *panelName ) : Panel( parent, panelName )
{
	m_visibleIndex = 0;
	m_lastSize = 0;

	SetBounds( 0, 0, 100, 100 );

	m_vbar = new ScrollBar( this, "PanelListPanelVScroll", true );
	m_vbar->SetBounds( 0, 0, 20, 20 );
	m_vbar->SetVisible( false );
	m_vbar->AddActionSignalTarget( this );

	m_pPanelEmbedded = new EditablePanel( this, "PanelListEmbedded" );
	m_pPanelEmbedded->SetBounds( 0, 0, 20, 20 );
	m_pPanelEmbedded->SetPaintBackgroundEnabled( false );
	m_pPanelEmbedded->SetPaintBorderEnabled( false );

	if ( IsProportional() )
	{
		int width, height;
		int sw, sh;
		surface()->GetProportionalBase( width, height );
		surface()->GetScreenSize( sw, sh );

		// resize scrollbar, etc
		m_iScrollbarSize = (int)( (float)SCROLLBAR_SIZE * ( (float)sw / (float)width ) );
		m_iDefaultHeight = (int)( (float)DEFAULT_HEIGHT * ( (float)sw / (float)width ) );
		m_iPanelBuffer = (int)( (float)PANELBUFFER * ( (float)sw / (float)width ) );
	}
	else
	{
		m_iScrollbarSize = SCROLLBAR_SIZE;
		m_iDefaultHeight = DEFAULT_HEIGHT;
		m_iPanelBuffer = PANELBUFFER;
	}
}

//--------------------------------------------------------------------------------------------------------------
BuyPresetListBox::~BuyPresetListBox( void )
{
	// free data from table
	DeleteAllItems();
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Passes commands up to the parent
 */
void BuyPresetListBox::OnCommand( const char *command )
{
	GetParent()->OnCommand( command );
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Scrolls the list according to the mouse wheel movement
 */
void BuyPresetListBox::OnMouseWheeled( int delta )
{
	int scale = 3;
	if ( m_items.size() )
	{
		Panel *panel = m_items[0].panel;
		if ( panel )
		{
			scale = panel->GetTall() + m_iPanelBuffer;
		}
	}
	int val = m_vbar->GetValue();
	val -= ( delta * scale );
	m_vbar->SetValue( val );
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Computes vertical pixels needed by listbox contents
 */
int BuyPresetListBox::computeVPixelsNeeded( void )
{
	int pixels = 0;

	int i;
	for ( i = 0; i < (int)m_items.size(); i++ )
	{
		Panel *panel = m_items[i].panel;
		if ( !panel )
			continue;

		int w, h;
		panel->GetSize( w, h );

		pixels += m_iPanelBuffer; // add in buffer. between items.
		pixels += h;
	}

	pixels += m_iPanelBuffer; // add in buffer below last item

	return pixels;
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Adds an item to the end of the listbox.  UserData is assumed to be a pointer that can be freed by the listbox if non-NULL.
 */
int BuyPresetListBox::AddItem( Panel *panel, void *userData )
{
	ASSERT_AT( panel, 140 );

	DataItem item = { panel, userData };

	panel->SetParent( m_pPanelEmbedded );

	m_items.push_back( item );

	InvalidateLayout( false, false );
	return (int)m_items.size();
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Exchanges two items in the listbox
 */
void BuyPresetListBox::SwapItems( int index1, int index2 )
{
	if ( index1 < 0 || index2 < 0 || index1 >= (int)m_items.size() || index2 >= (int)m_items.size() )
	{
		return;
	}

	DataItem temp = m_items[index1];
	m_items[index1] = m_items[index2];
	m_items[index2] = temp;

	InvalidateLayout( false, false );
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Returns the number of items in the listbox
 */
int BuyPresetListBox::GetItemCount( void ) const
{
	return (int)m_items.size();
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Returns the panel in the given index, or NULL
 */
Panel *BuyPresetListBox::GetItemPanel( int index ) const
{
	if ( index < 0 || index >= (int)m_items.size() )
		return NULL;

	return m_items[index].panel;
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Returns the userData in the given index, or NULL
 */
void *BuyPresetListBox::GetItemUserData( int index )
{
	if ( index < 0 || index >= (int)m_items.size() )
	{
		return NULL;
	}

	return m_items[index].userData;
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Sets the userData in the given index
 */
void BuyPresetListBox::SetItemUserData( int index, void *userData )
{
	if ( index < 0 || index >= (int)m_items.size() )
		return;

	m_items[index].userData = userData;
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Removes an item from the table (changing the indices of all following items), deleting the panel and userData
 */
void BuyPresetListBox::RemoveItem( int index )
{
	if ( index < 0 || index >= (int)m_items.size() )
		return;

	DataItem item = m_items[index];
	if ( item.panel )
	{
		item.panel->MarkForDeletion();
	}
	if ( item.userData )
	{
		::operator delete( item.userData );
	}

	m_items.erase( m_items.begin() + index );

	InvalidateLayout( false, false );
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  clears the listbox, deleting all panels and userData
 */
void BuyPresetListBox::DeleteAllItems( void )
{
	while ( m_items.size() )
	{
		RemoveItem( 0 );
	}

	// move the scrollbar to the top of the list
	m_vbar->SetValue( 0 );
	InvalidateLayout( false, false );
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Handles Count changes
 */
void BuyPresetListBox::OnSizeChanged( int wide, int tall )
{
	Panel::OnSizeChanged( wide, tall );
	InvalidateLayout( false, false );
}

#pragma warning( push )
#pragma warning( disable : 4456 )
//--------------------------------------------------------------------------------------------------------------
/**
 *  Positions listbox items, etc after internal changes
 */
void BuyPresetListBox::PerformLayout( void )
{
	int wide, tall;
	GetSize( wide, tall );

	int vpixels = computeVPixelsNeeded();

	int visibleIndex = m_visibleIndex;

	//!! need to make it recalculate scroll positions
	m_vbar->SetVisible( true );
	m_vbar->SetEnabled( false );
	m_vbar->SetRange( 0, ( max( 0, vpixels - tall + m_iDefaultHeight ) ) );
	m_vbar->SetRangeWindow( m_iDefaultHeight );
	m_vbar->SetButtonPressedScrollValue( m_iDefaultHeight ); // standard height of labels/buttons etc.
	m_vbar->SetPos( wide - m_iScrollbarSize, 1 );
	m_vbar->SetSize( m_iScrollbarSize, tall - 2 );

	m_visibleIndex = visibleIndex;

	int top = max( 0, m_vbar->GetValue() );

	m_pPanelEmbedded->SetPos( 1, -top );
	m_pPanelEmbedded->SetSize( wide - m_iScrollbarSize - 2, vpixels );

	m_vbar->GetValue();

	// Now lay out the controls on the embedded panel
	int y = 0;
	int h = 0;
	int totalh = 0;

	int i;
	for ( i = 0; i < (int)m_items.size(); i++, y += h )
	{
		// add in a little buffer between panels
		y += m_iPanelBuffer;
		DataItem item = m_items[i];

		h = item.panel->GetTall();

		totalh += h;
		item.panel->SetBounds( 8, y, wide - m_iScrollbarSize - 8 - 8, h );
		item.panel->InvalidateLayout( false, false );
	}

	if ( m_visibleIndex >= 0 && m_visibleIndex < (int)m_items.size() )
	{
		int vpos = 0;

		int wide, tall;
		GetSize( wide, tall );

		int vtop, vbottom;
		m_vbar->GetRange( vtop, vbottom );

		int top = max( 0, m_vbar->GetValue() ); // top pixel in the embedded panel
		int bottom = top + tall - 2;

		m_vbar->GetValue();
		int itemTop, itemLeft, itemBottom, itemRight;
		m_items[m_visibleIndex].panel->GetBounds( itemLeft, itemTop, itemRight, itemBottom );
		itemBottom += itemTop;
		itemRight += itemLeft;

		if ( itemTop < top )
		{
			// item's top is too high
			vpos -= ( top - itemTop );

			m_vbar->SetValue( vpos );
			OnSliderMoved( vpos );
		}
		else if ( itemBottom > bottom )
		{
			// item's bottom is too low
			vpos += ( itemBottom - bottom );

			m_vbar->SetValue( vpos );
			OnSliderMoved( vpos );
		}
	}

	if ( m_lastSize == vpixels )
	{
		m_visibleIndex = -1;
	}
	m_lastSize = vpixels;
}
#pragma warning( pop )

//--------------------------------------------------------------------------------------------------------------
/**
 *  Try to ensure that the given index is visible
 */
void BuyPresetListBox::MakeItemVisible( int index )
{
	m_visibleIndex = index;
	m_lastSize = 0;
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Loads colors, fonts, etc
 */
void BuyPresetListBox::ApplySchemeSettings( IScheme *pScheme )
{
	Panel::ApplySchemeSettings( pScheme );

	SetBgColor( GetSchemeColor( "LabelBgColor", GetSchemeColor( "WindowDisabledBgColor", pScheme ), pScheme ) );

	SetBorder( pScheme->GetBorder( "BrowserBorder" ) );
	m_vbar->SetBorder( pScheme->GetBorder( "BrowserBorder" ) );

	PerformLayout();
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Handles slider being dragged
 */
void BuyPresetListBox::OnSliderMoved( int )
{
	InvalidateLayout( false, false );
	Repaint();
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Moves slider to the top
 */
void BuyPresetListBox::MoveScrollBarToTop( void )
{
	m_vbar->SetValue( 0 );
	OnSliderMoved( 0 );
}

//--------------------------------------------------------------------------------------------------------------
