// player_preview.h - the Advanced window's previews (the players' and the radar's are square boxes of the same size in
// the same place on their tabs): the players side by side - the CT (GIGN) on the left looking left, the Terrorist
// (Leet) on the right looking right - on the cs_assault backdrop, weapons in hand, drawn the way the game draws players:
// in one flat colour per team or with the normal skins; drag to turn them; or, while the Weapons list is in use, the
// chosen set's M4A1 firing in first person, or, while the Players list is in use, one player model at a time to step
// through; the radar; and the crosshair, on a banner across the top of its tab (a de_aztec screenshot, its ends fading
// out).
#pragma once

#include "Panel.h"
#include "../crosshair_draw.h"

class CPlayerPreview : public vgui2::Panel
{
	typedef vgui2::Panel BaseClass;

public:
	enum Team
	{
		TEAM_T,		// Leet with an AK-47
		TEAM_CT,	// GIGN with an M4A1
		TEAM_VIP,	// the VIP with a USP
	};

	CPlayerPreview( vgui2::Panel* parent, const char* name );

	// solid: each player in its team's flat colour (r g b, 0..255 each, by Team) as with solid team colours, otherwise
	// the normal skins
	void SetLook( bool solid, const int colors[3][3] );
	// the head colour (r g b, 0..255) over that, or none
	void SetHeadColor( bool on, int r, int g, int b );
	// the weapon models' set (0 the game's, 1 retail, 2 beta: cl_weaponmodels) for the weapon in the player's hand, and
	// showWeapon: the set's M4A1 firing in first person instead of the player
	void SetWeapons( int set, bool showWeapon );
	// the player models' set (0 the game's, 1 retail, 2 beta: cl_playermodels) for the player shown
	void SetPlayers( int set );
	// browsing: one player instead of the two side by side - every player model in turn (the CTs, the Terrorists, the
	// VIP), each with its team's weapon, always in its own skins (no team or head colour); StepModel goes to the next
	// (step 1) or the previous (-1)
	void SetBrowsing( bool on );
	void StepModel( int step );
	// the name of the player browsed to and the set its model comes from, as "GIGN (Beta)"
	void GetBrowsedName( char* buffer, int size );

protected:
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override;
	void PaintBackground() override;
	void Paint() override;
	void OnMousePressed( vgui2::MouseCode code ) override;
	void OnMouseReleased( vgui2::MouseCode code ) override;
	void OnCursorMoved( int x, int y ) override;

private:
	bool m_bSolid;
	float m_Colors[3][3];	// by Team
	bool m_bHead;
	float m_HeadColor[3];
	int m_iWeaponSet;
	int m_iPlayerSet;
	bool m_bWeaponView;
	bool m_bBrowsing;
	int m_iBrowsed;			// kBrowsed (player_preview.cpp)
	float m_flYaw;			// degrees the players are turned (dragged) from their start angles
	bool m_bDragging;
	int m_iDragX;
};

// the Advanced window's radar preview: just the radar as set, filling the box, over a sample team on de_dust2 (drawn
// by the HUD's own radar code, radar2.cpp)
class CRadarPreview : public vgui2::Panel
{
	typedef vgui2::Panel BaseClass;

public:
	CRadarPreview( vgui2::Panel* parent, const char* name );

	// square: the square map radar (size in HUD pixels, range in world units, map opacity 0..1, turning with the
	// view); otherwise the classic round radar
	void SetSettings( bool square, float size, float range, float mapAlpha, bool rotate );

protected:
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override;
	void Paint() override;

private:
	bool m_bSquare;
	float m_flSize;
	float m_flRange;
	float m_flAlpha;
	bool m_bRotate;
};

// the Advanced window's crosshair preview, a banner across the top of the tab: the crosshair as set, at its real size,
// on a de_aztec screenshot shown at its own pixel size (like the game's screen), both ends fading into the window. It
// runs a little routine (standing, running, a burst of shots) so the "wider while moving / shooting" options show.
class CCrosshairPreview : public vgui2::Panel
{
	typedef vgui2::Panel BaseClass;

public:
	CCrosshairPreview( vgui2::Panel* parent, const char* name );

	void SetSettings( const CrosshairSettings& settings );

	// the tallest banner the picture fills (pixels): as far above and below its aim point as it goes, twice the nearer
	static int MostTall();

protected:
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override;
	void PaintBackground() override;
	void Paint() override;

private:
	CrosshairSettings m_settings;
	CCrosshairRoutine m_routine;
};
