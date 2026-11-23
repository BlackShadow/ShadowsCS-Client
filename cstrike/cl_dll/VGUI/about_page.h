// about_page.h - the Advanced window's About tab: the client's name ("Shadow's", animated: a shadow that swings as if
// its light moved, a gleam passing over the letters now and then), what it is and its build number, and its author as
// in game credits - "CREATED BY" between two fading lines, name and GitHub link on the left, the GitHub picture on the
// right (author_avatar.cpp keeps it current) with a slowly breathing glow
#pragma once

#include "PropertyPage.h"

namespace vgui2
{
class Label;
class URLLabel;
}
class CAnimatedTitle;
class CAuthorPicture;

class CAboutPage : public vgui2::PropertyPage
{
	typedef vgui2::PropertyPage BaseClass;

public:
	CAboutPage( vgui2::Panel* parent, const char* name );

protected:
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override;
	void PerformLayout() override;
	void Paint() override;

private:
	CAnimatedTitle* m_pTitle;
	vgui2::Label* m_pTagline;
	vgui2::Label* m_pBuild;
	vgui2::Label* m_pCreatedBy;
	vgui2::Label* m_pAuthorName;
	vgui2::URLLabel* m_pAuthorLink;
	CAuthorPicture* m_pPicture;
	unsigned long m_hNameFont = 0;		// vgui2::HFont
	int m_iNameTall = 0;
	int m_iCaptionY = 0;				// the middle of "CREATED BY", for its lines
	int m_iCaptionHalf = 0;				// half its width
	int m_AccentColor[4] = { 196, 181, 80, 150 };
};
