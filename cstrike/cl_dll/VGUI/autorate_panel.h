// autorate_panel.h - the Advanced window's Auto-rate readout (Misc tab, under its check box): the connection as
// measured, the last half minute's ping as a graph, the settings now and what Auto-rate is doing
#pragma once

#include "Panel.h"
#include "../auto_rate.h"

class CAutoRatePanel : public vgui2::Panel
{
	typedef vgui2::Panel BaseClass;

public:
	CAutoRatePanel( vgui2::Panel* parent, const char* name );

	// how tall it draws (the page's layout gives it this; it draws across whatever width it gets)
	int WantedTall();

protected:
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override;
	void OnThink() override;
	void Paint() override;

private:
	void EnsureFonts();
	int P( int units ) { return GetProportionalScaledValue( units ); }

	AutoRateReadout m_read;
	double m_flNextRead = 0.0;
	vgui2::HFont m_hCaption = 0;	// the small captions over the numbers
	vgui2::HFont m_hValue = 0;		// the numbers
	vgui2::HFont m_hUnit = 0;		// ms, %, /s after them
	vgui2::HFont m_hText = 0;		// what Auto-rate is doing
	int m_iFontScale = 0;			// the proportional scale the fonts were made at
	Color m_Bright = Color( 196, 181, 80, 255 );
	Color m_Text = Color( 216, 222, 211, 255 );
	Color m_Dim = Color( 160, 170, 149, 255 );
};
