#pragma once

#include "basecareermenu.h"
#include "utlvector.h"

class TaskLabel;
namespace vgui2
{
class Button;
class Label;
}

typedef CUtlVector<TaskLabel *> TaskLabelVector;

class CCSCareerRoundEndMenu : public CCSCareerBaseMenu
{
public:
	CCSCareerRoundEndMenu( vgui2::Panel *parent );
	virtual ~CCSCareerRoundEndMenu( void );
	void Update( void );
	virtual vgui2::Panel *CreateControlByName( const char *controlName );

private:
	virtual void OnCommand( const char *command );

	vgui2::Label *m_pWinsLabel;
	vgui2::Label *m_pLossesLabel;
	vgui2::Label *m_pNeededLabel;
	vgui2::Label *m_pWinByLabel;
	vgui2::Label *m_pRoundResultLabel;
	vgui2::Label *m_pImminentVictoryLabel;
	vgui2::Button *m_pContinueButton;
	vgui2::Button *m_pSurrenderButton;

	TaskLabelVector m_pTaskLabels;
	TaskLabelVector m_pTaskStatusLabels;
	vgui2::Label *m_pTaskNoticeLabel;
};
