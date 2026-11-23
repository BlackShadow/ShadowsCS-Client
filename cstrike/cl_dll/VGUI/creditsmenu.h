#pragma once

#include "vgui/IScheme.h"
#include "Frame.h"
#include <vector>

namespace vgui2
{
class Label;
}

typedef std::vector<vgui2::Label *> LabelVector;
typedef LabelVector::iterator LabelVectorIter;

class CCSCreditsMenu : public vgui2::Frame
{
public:
	CCSCreditsMenu( vgui2::Panel *parent );
	virtual ~CCSCreditsMenu( void );
	virtual void ApplySchemeSettings( vgui2::IScheme *pScheme );
	virtual void Activate( void );
	virtual void OnThink( void );
	virtual void OnCommand( const char *command );
	static void SetDisconnect( bool disconnectAfter );

protected:
	virtual void PerformLayout( void );
	void ReadCreditsFile( void );

	vgui2::Panel *m_panel;

	LabelVector m_labelVector;

	float m_panelSpeed;
	float m_panelOffscreenStart;
	float m_panelHideTime;
	float m_startTime;
	static bool m_shouldDisconnect;
};

void ShowCreditsMenu( bool disconnectAfter );
