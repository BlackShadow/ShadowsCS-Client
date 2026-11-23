/*** 
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

#include "careerroundendmenu.h"
#include "careermenus.h"
#include "CareerBox.h"
#include "hud.h"
#include "GameUI_Interfaces.h"
#include "GameUI/ICareerUI.h"
#include "GameUI/IMusicManager.h"
#include "Button.h"
#include "Label.h"
#include "game_controls/iviewport.h"
#include "vgui_controls/Controls.h"
#include "vgui/ILocalize.h"
#include "shared_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

CareerMatchStatus TheCareerMatchStatus;


void ShowCareerRoundEndMenu( int ctWins, int tWins, int neededWins, int winBy, int roundResult )
{
	TheCareerMatchStatus.ctWins = ctWins;
	TheCareerMatchStatus.tWins = tWins;
	TheCareerMatchStatus.neededWins = neededWins;
	TheCareerMatchStatus.winBy = winBy;
	TheCareerMatchStatus.roundResult = roundResult;
	if ( gViewPortInterface != NULL )
	{
		CCareerBaseBox::CloseOpenBoxes( false );
		for ( int i = 1; i < 40; ++i )
		{
			if ( i != 2 && i != 27 && i != 26 )
			{
				gViewPortInterface->HideVGUIMenu( i );
			}
		}
		gViewPortInterface->ShowVGUIMenu( 35 );
	}
}


void ShowCareerMatchEndMenu( int ctWins, int tWins, int neededWins, int winBy, int roundResult )
{
	TheCareerMatchStatus.ctWins = ctWins;
	TheCareerMatchStatus.tWins = tWins;
	TheCareerMatchStatus.neededWins = neededWins;
	TheCareerMatchStatus.winBy = winBy;
	TheCareerMatchStatus.roundResult = roundResult;
	if ( gViewPortInterface != NULL )
	{
		CCareerBaseBox::CloseOpenBoxes( false );
		gViewPortInterface->HideAllVGUIMenu();
		gViewPortInterface->ShowVGUIMenu( 36 );
	}
}


void OnRoundEndMenuOpen( bool didWin )
{
	if ( g_pCareerUI != NULL && g_pCareerUI->IsPlayingMatch() )
	{
		g_pCareerUI->OnRoundEndMenuOpen( didWin );
		return;
	}

	if ( g_pMusicManager != NULL )
	{
		if ( didWin )
		{
			g_pMusicManager->StartTrack( TRACKTYPE_ROUNDWIN, false );
		}
		else
		{
			g_pMusicManager->StartTrack( TRACKTYPE_ROUNDLOSE, false );
		}
	}
}


void OnRoundEndMenuClose( bool stillPlaying )
{
	if ( g_pCareerUI != NULL && g_pCareerUI->IsPlayingMatch() )
	{
		g_pCareerUI->OnRoundEndMenuClose( stillPlaying );
		return;
	}

	if ( g_pMusicManager != NULL )
	{
		g_pMusicManager->FadeOutTrack();
	}
}


wchar_t *GetTaskStatusString( ICareerTask *task )
{
	const int BufLen = 512;
	static wchar_t partialStr[BufLen];
	char ascii[BufLen];
	wchar_t tmpBuf[BufLen];
	partialStr[0] = 0;

	if ( task->IsComplete() )
	{
		vgui2::localize()->ConstructString( partialStr, sizeof( partialStr ), vgui2::localize()->Find( "#Career_Task_Complete" ), 0 );
	}
	else
	{
		int numLeft = task->GetRepeat() - task->GetPartial();
		if ( numLeft > 0 && task->GetRepeat() > 1 )
		{
			if ( task->IsMaxRoundTime() )
			{
				vgui2::localize()->ConstructString( partialStr, sizeof( partialStr ), vgui2::localize()->Find( "#Career_Task_PartialIncomplete" ), 0 );
			}
			else
			{
				sprintf( ascii, "%d", numLeft );
				vgui2::localize()->ConvertANSIToUnicode( ascii, tmpBuf, sizeof( tmpBuf ) );
				vgui2::localize()->ConstructString( partialStr, sizeof( partialStr ), vgui2::localize()->Find( "#Career_Task_PartialRemaining" ), 1, tmpBuf );
			}
		}
		else if ( task->MustSurvive() && numLeft <= 0 )
		{
			vgui2::localize()->ConstructString( partialStr, sizeof( partialStr ), vgui2::localize()->Find( "#Career_Task_PartialMustLive" ), 0 );
		}
		else
		{
			vgui2::localize()->ConstructString( partialStr, sizeof( partialStr ), vgui2::localize()->Find( "#Career_Task_PartialIncomplete" ), 0 );
		}
	}

	return partialStr;
}


class TaskLabel : public vgui2::Label
{
public:
	TaskLabel( vgui2::Panel *parent, const char *panelName, const char *text );

	void SetTaskStatus( bool complete );
	virtual void OnThink( void );
	virtual void ApplySchemeSettings( vgui2::IScheme *pScheme );

private:
	Color m_flashColor;
	Color m_completeColor;
	Color m_incompleteColor;
	float m_interval;
	float m_lastTransition;
	int m_transitionsLeft;
	bool m_isComplete;
	bool m_isFlashColor;
};


TaskLabel::TaskLabel( vgui2::Panel *parent, const char *panelName, const char *text ) : vgui2::Label( parent, panelName, text )
{
	m_isComplete = m_isFlashColor = false;
	m_transitionsLeft = 0;
}


void TaskLabel::ApplySchemeSettings( vgui2::IScheme *pScheme )
{
	m_completeColor = pScheme->GetColor( "TaskComplete", Color( 0, 0, 255, 255 ) );
	m_incompleteColor = pScheme->GetColor( "TaskIncomplete", Color( 255, 0, 0, 255 ) );
	m_flashColor = pScheme->GetColor( "TaskFlash", Color( 255, 255, 255, 0 ) );
	m_interval = (float)atof( pScheme->GetResourceString( "TaskFlashPeriod" ) );
	SetFont( pScheme->GetFont( "Task", IsProportional() ) );

	if ( m_transitionsLeft < 0 )
	{
		m_transitionsLeft = atoi( pScheme->GetResourceString( "TaskNumFlashes" ) );
	}
	if ( m_isComplete )
	{
		SetFgColor( m_completeColor );
	}
}


void TaskLabel::OnThink( void )
{
	if ( m_isComplete )
		return;
	float now = (float)gEngfuncs.GetAbsoluteTime();
	if ( now >= m_lastTransition + m_interval && m_transitionsLeft > 0 )
	{
		m_lastTransition = now;
		if ( m_isFlashColor )
		{
			SetFgColor( m_flashColor );
		}
		else
		{
			SetFgColor( m_incompleteColor );
		}
		m_isFlashColor = !m_isFlashColor;
		--m_transitionsLeft;
	}
	else if ( m_transitionsLeft <= 0 )
	{
		SetFgColor( m_flashColor );
		m_isFlashColor = true;
	}
}


void TaskLabel::SetTaskStatus( bool complete )
{
	m_isComplete = complete;
	m_isFlashColor = false;
	m_lastTransition = (float)gEngfuncs.GetAbsoluteTime();
	m_transitionsLeft = -1;

	if ( complete )
	{
		SetFgColor( m_completeColor );
	}
	else
	{
		SetFgColor( m_incompleteColor );
	}
}


void PopulateTaskLabels( TaskLabelVector &taskLabel, TaskLabelVector &statusLabel, vgui2::Label *pSummary, bool showTasksRemaining )
{
	int numTaskLabels = taskLabel.Count();

	int i;
	for ( i = 0; i < taskLabel.Count(); ++i )
	{
		taskLabel[i]->SetText( "" );
		statusLabel[i]->SetText( "" );
	}
	for ( i = 0; i < statusLabel.Count(); ++i )
	{
		taskLabel[i]->SetText( "" );
		statusLabel[i]->SetText( "" );
	}
	if ( pSummary != NULL )
		pSummary->SetText( "" );
	if ( g_pCareerUI == NULL )
		return;

	if ( numTaskLabels != statusLabel.Count() )
		return;

	int tasksRemaining = 0;

	wchar_t tmpBuf[128];
	wchar_t cbuf[1024];
	wchar_t ibuf[1024];
	cbuf[0] = 0;
	ibuf[0] = 0;

	ITaskVec *tv = g_pCareerUI->GetCurrentTaskVec();
	int taskNum = 0;
	for ( ITaskVecIt it = tv->begin(); it != tv->end(); ++it )
	{
		ICareerTask *task = *it;

		const wchar_t *unicodeTaskName = task->GetLocalizedTaskName();
		if ( unicodeTaskName != NULL )
		{
			if ( !task->IsComplete() )
			{
				++tasksRemaining;
			}
			taskLabel[taskNum]->SetTaskStatus( task->IsComplete() );
			taskLabel[taskNum]->SetText( unicodeTaskName );
			statusLabel[taskNum]->SetTaskStatus( task->IsComplete() );
			statusLabel[taskNum]->SetText( GetTaskStatusString( task ) );
			++taskNum;
		}
	}
	if ( pSummary != NULL )
	{
		if ( tasksRemaining && showTasksRemaining )
		{
			const char *lookup = ( tasksRemaining == 1 ) ?
				"#Career_TaskStatusUnfinishedSingle" :
				"#Career_TaskStatusUnfinished";
			char ascii[64];
			_snprintf( ascii, sizeof( ascii ), "%d", tasksRemaining );
			vgui2::localize()->ConvertANSIToUnicode( ascii, tmpBuf, sizeof( tmpBuf ) );
			vgui2::localize()->ConstructString( ibuf, sizeof( ibuf ), vgui2::localize()->Find( lookup ), 1, tmpBuf );
			pSummary->SetText( ibuf );
		}
		else if ( !tasksRemaining )
		{
			pSummary->SetText( vgui2::localize()->Find( "#Career_TaskStatusDone" ) );
		}
	}
}


void PopulateWinsAndLosses( bool playAsCT, vgui2::Label *pWinsLabel, vgui2::Label *pLossesLabel )
{
	wchar_t string1[1024];
	wchar_t wInt[2][6];
	if ( playAsCT )
	{
		_snwprintf( wInt[0], 6, L"%d", TheCareerMatchStatus.ctWins );
		vgui2::localize()->ConstructString( string1, 1024, vgui2::localize()->Find( "#Career_RoundWinsCT" ), 1, wInt[0] );
		pWinsLabel->SetText( string1 );

		_snwprintf( wInt[0], 6, L"%d", TheCareerMatchStatus.tWins );
		vgui2::localize()->ConstructString( string1, 1024, vgui2::localize()->Find( "#Career_RoundWinsT" ), 1, wInt[0] );
		pLossesLabel->SetText( string1 );
	}
	else
	{
		_snwprintf( wInt[0], 6, L"%d", TheCareerMatchStatus.ctWins );
		vgui2::localize()->ConstructString( string1, 1024, vgui2::localize()->Find( "#Career_RoundWinsCT" ), 1, wInt[0] );
		pLossesLabel->SetText( string1 );

		_snwprintf( wInt[0], 6, L"%d", TheCareerMatchStatus.tWins );
		vgui2::localize()->ConstructString( string1, 1024, vgui2::localize()->Find( "#Career_RoundWinsT" ), 1, wInt[0] );
		pWinsLabel->SetText( string1 );
	}
}


#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
CCSCareerRoundEndMenu::CCSCareerRoundEndMenu( vgui2::Panel *parent ) : CCSCareerBaseMenu( parent, "CSCareerRoundEnd", false )
{
	vgui2::Frame::SetTitle( "#Career_Round_End_Menu", true );

	m_pWinsLabel = new vgui2::Label( this, "Wins", "" );
	m_pLossesLabel = new vgui2::Label( this, "Losses", "" );

	m_pNeededLabel = new vgui2::Label( this, "Needed", "" );
	m_pRoundResultLabel = new vgui2::Label( this, "RoundResult", "" );

	m_pImminentVictoryLabel = new vgui2::Label( this, "ImminentVictory", "" );
	m_pTaskNoticeLabel = new vgui2::Label( this, "TaskNotice", "" );

	m_pContinueButton = new vgui2::Button( this, "ok", "" );
	m_pSurrenderButton = new vgui2::Button( this, "surrender", "" );

	LoadControlSettings( "Resource/UI/CSCareerRoundEnd.res", NULL );

	int i = 0;
	while ( true )
	{
		TaskLabel *taskLabel = NULL;
		TaskLabel *statusLabel = NULL;
		taskLabel = dynamic_cast<TaskLabel *>( FindChildByName( SharedVarArgs( (char *)"Task%d", i ), true ) );
		statusLabel = dynamic_cast<TaskLabel *>( FindChildByName( SharedVarArgs( (char *)"TaskStatus%d", i ), true ) );
		if ( taskLabel == NULL || statusLabel == NULL )
			break;
		m_pTaskLabels.AddToTail( taskLabel );
		m_pTaskStatusLabels.AddToTail( statusLabel );
		++i;
	}
}


vgui2::Panel *CCSCareerRoundEndMenu::CreateControlByName( const char *controlName )
{
	if ( !_stricmp( "TaskLabel", controlName ) )
	{
		TaskLabel *pLabel = new TaskLabel( this, NULL, "" );
		return pLabel;
	}
	else
	{
		return vgui2::EditablePanel::CreateControlByName( controlName );
	}
}


CCSCareerRoundEndMenu::~CCSCareerRoundEndMenu( void )
{
}


extern void IN_AttackUp( void );
extern void IN_ScoreUp( void );
void CCSCareerRoundEndMenu::Update( void )
{
	IN_AttackUp();
	IN_ScoreUp();

	gEngfuncs.pfnClientCmd( (char *)"-score; -attack; -attack2; -voicerecord\n" );

	if ( g_pCareerUI == NULL || !g_pCareerUI->IsPlayingMatch() )
	{
		m_pSurrenderButton->SetVisible( false );
	}
	else
	{
		m_pSurrenderButton->SetVisible( true );
	}

	bool isCT = true;
	if ( g_pCareerUI != NULL && !g_pCareerUI->PlayAsCT() )
		isCT = false;

	PopulateWinsAndLosses( isCT, m_pWinsLabel, m_pLossesLabel );

	wchar_t string1[1024];
	wchar_t wInt[2][6];
	_snwprintf( wInt[0], 6, L"%d", TheCareerMatchStatus.neededWins );
	_snwprintf( wInt[1], 6, L"%d", TheCareerMatchStatus.winBy );
	vgui2::localize()->ConstructString( string1, 2048, vgui2::localize()->Find( "#Career_MatchTasks" ), 2, wInt[0], wInt[1] );
	m_pNeededLabel->SetText( string1 );

	switch ( TheCareerMatchStatus.roundResult )
	{
	case 1:
		if ( isCT )
		{
			m_pRoundResultLabel->SetText( "#Career_RoundWin" );
			OnRoundEndMenuOpen( true );
		}
		else
		{
			m_pRoundResultLabel->SetText( "#Career_RoundLose" );
			OnRoundEndMenuOpen( false );
		}
		break;
	case 2:
		if ( isCT )
		{
			m_pRoundResultLabel->SetText( "#Career_RoundLose" );
			OnRoundEndMenuOpen( false );
		}
		else
		{
			m_pRoundResultLabel->SetText( "#Career_RoundWin" );
			OnRoundEndMenuOpen( true );
		}
		break;
	default:
		m_pRoundResultLabel->SetText( "#Career_RoundDraw" );
		break;
	}
	PopulateTaskLabels( m_pTaskLabels, m_pTaskStatusLabels, m_pTaskNoticeLabel, true );

	int wins = isCT ? TheCareerMatchStatus.ctWins : TheCareerMatchStatus.tWins;
	int losses = isCT ? TheCareerMatchStatus.tWins : TheCareerMatchStatus.ctWins;
	if ( wins + 1 >= TheCareerMatchStatus.neededWins && wins - losses >= TheCareerMatchStatus.winBy - 1 )
	{
		m_pImminentVictoryLabel->SetText( "#Career_ImminentVictory" );
	}
	else if ( losses + 1 >= TheCareerMatchStatus.neededWins && losses - wins >= TheCareerMatchStatus.winBy - 1 )
	{
		m_pImminentVictoryLabel->SetText( "#Career_ImminentDefeat" );
	}
	else
	{
		m_pImminentVictoryLabel->SetText( L"" );
	}

	m_pContinueButton->RequestFocus( 0 );

	gEngfuncs.pfnClientCmd( (char *)"setpause\n" );

	gViewPortInterface->CloseTutorTextWindow();
}


void CCSCareerRoundEndMenu::OnCommand( const char *command )
{
	if ( !_stricmp( command, "disconnect" ) )
	{
		CCareerQueryBox *pBox = new CCareerQueryBox( "#Career_Surrender", "#GameUI_QuitConfirmationText", "CareerBoxSurrender", this );
		pBox->AddActionSignalTarget( this );
		pBox->DoModal();
		return;
	}
	if ( !_stricmp( command, "surrender" ) )
	{
		command = "disconnect";
	}

	gEngfuncs.pfnClientCmd( (char *)"unpause\n" );

	if ( _stricmp( command, "vguicancel" ) )
	{
		gEngfuncs.pfnClientCmd( (char *)command );
	}
	else
	{
		gViewPortInterface->HideBackGround();
	}
	OnClose();
	vgui2::Frame::OnCommand( command );

	OnRoundEndMenuClose( _stricmp( command, "disconnect" ) != 0 );
}
