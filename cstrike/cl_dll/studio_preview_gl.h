// studio_preview_gl.h - draws a posed player (and the weapon in its hand) with plain OpenGL into a rectangle of the
// screen (the Advanced window's player preview). No engine headers: the same code runs in an offline test.
#pragma once

class CStudioPreviewModel;

// The player posed 'time' seconds into its animations with 'weapon' (a p_ model, may be null) in its hand, turned
// 'yaw' degrees about its vertical axis and moved 'shift' world units to the right, standing in the rectangle x, y,
// wide, tall of a screenWide x screenTall screen (menu coordinates, origin top left). solid: one flat colour (0..1
// each) for player and weapon, as solid team colours draw players; otherwise the skins, lit. headColor: the player's
// head over that in one flat colour (0..1 each), as the head colour draws it in game; NULL for none. viewModel: 'player'
// is a first-person weapon model (v_) seen as the game shows it, the eye at its origin with an 80 degree view (the part
// of it on the weapon's side that fits the rectangle; weapon, yaw and shift are not used), mirror: left and right swapped (the game
// mirrors view models for the hand they are not made for), leftHanded: the weapon shows on the left (the view keeps
// to that side). Every GL setting it changes is put back afterwards.
void StudioPreview_Draw( const CStudioPreviewModel& player, const CStudioPreviewModel* weapon, double time, int x, int y,
	int wide, int tall, int screenWide, int screenTall, float yaw, float shift, bool solid, const float color[3],
	const float* headColor = 0, bool viewModel = false, bool mirror = false, bool leftHanded = false );

// (A player with a plan set - CStudioPreviewModel::SetFit - is drawn grown towards its hit boxes.)

// the 'shift' that centres the player's body (its weapon kept inside the rectangle too), turned 'yaw' degrees, in a
// rectangle 'aspect' (width / height) wide, at the start of their animations
float StudioPreview_CentreShift( const CStudioPreviewModel& player, const CStudioPreviewModel* weapon, float yaw, float aspect );
