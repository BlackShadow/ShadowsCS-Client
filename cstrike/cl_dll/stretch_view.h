// stretch_view.h - "Stretched 4:3" (cl_stretch43): the world looks like a 4:3 picture stretched to fill a wider screen,
// so things look wider. A screen effect at the screen's own resolution: the middle 4:3 part of the drawn world is
// stretched over the whole screen before the HUD is drawn (the HUD stays as it is).
#pragma once

void StretchView_Init();	// registers cl_stretch43
void StretchView_Draw();	// the world is drawn, the HUD not yet: at the start of HUD_Redraw, or under the engine's menu
							// (menu_background.cpp: no HUD_Redraw while it is up)
bool StretchView_WorksOn( int wide, int tall );	// is a screen this size wider than 4:3 (something to stretch)?
