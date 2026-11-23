#pragma once

namespace vgui2
{
class Label;
}

void PopulateWinsAndLosses( bool playAsCT, vgui2::Label *pWinsLabel, vgui2::Label *pLossesLabel );

void ShowCareerMatchEndMenu( int ctWins, int tWins, int neededWins, int winBy, int roundResult );

void ShowCareerRoundEndMenu( int ctWins, int tWins, int neededWins, int winBy, int roundResult );

struct CareerMatchStatus
{
	int ctWins;
	int tWins;
	int neededWins;
	int winBy;
	int roundResult;
};

extern CareerMatchStatus TheCareerMatchStatus;
