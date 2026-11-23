// fontstyle.h - cl_fontstyle (the Advanced window's FX tab: Font style): "original", the game's own fonts, or "won", the
// fonts of the WON Half-Life engine CS 1.5 ran on - the menus in CS 1.5's Arial sizes and the chat in the engine's
// console lettering, from resource/ClientSchemeWON.res and resource/font/won*.ttf (made by tools/fonts/won_fonts.py)
#pragma once

void FontStyle_Init();					// registers cl_fontstyle (CHud::Init)
void FontStyle_LoadScheme();			// loads ClientSchemeWON.res as "ClientSchemeWON" (the viewport's Initialize)
bool FontStyle_IsWON();					// cl_fontstyle is "won" and the scheme loaded
const char* FontStyle_MenuScheme();		// the menus' scheme: "ClientSchemeWON" or the game's "ClientScheme"
unsigned long FontStyle_ChatFont();		// the chat's WON lettering (a vgui2::HFont), 0 for the game's font
