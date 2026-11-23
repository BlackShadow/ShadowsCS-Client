#pragma once

#include "vgui/IScheme.h"
#include "Frame.h"

class CCSCareerBaseMenu : public vgui2::Frame
{
public:
	CCSCareerBaseMenu( vgui2::Panel *parent, const char *name, bool showTaskBar );
	virtual ~CCSCareerBaseMenu( void );
	virtual void Activate( void );
	virtual void ActivateBuildMode( void );
	void GetOffset( int &x, int &y ) { x = m_offsetX; y = m_offsetY; }
	void UpdateScheme( void );

protected:
	int m_offsetX;
	int m_offsetY;
};

int GetAlternateProportionalValueFromScaled( int scaledValue );
void RepositionControl( vgui2::Panel *pPanel );
