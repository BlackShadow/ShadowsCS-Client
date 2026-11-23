// menu_background.h - the main menu's background picture (cl_menubackground auto / original / beta / retail).
//
// The game's menu (GameUI) draws its background from the tiles listed in resource/BackgroundLayout.txt. This finds
// the tiles' textures and puts one of the Shadows pictures into them (gfx/shadows/menubg, the 4:3 or 16:9 version
// that suits the screen), drawn over the whole screen the way the menu places its tiles. "auto" goes round the
// original, beta and retail pictures, a minute each, cross-fading. While the menu layer is up (main menu, loading screen, Esc
// menu); in game the menu layer's panel also keeps the client's picture effects going under the menu (stretched
// 4:3, the weapon models' draw animation), as the engine calls no HUD_Redraw while its menu is up.
#pragma once

void MenuBackground_Init();
void MenuBackground_Frame();
void MenuBackground_Shutdown();	// waits for the worker thread
