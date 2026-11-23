// chatwindow.h - the chat as a real window (see chatwindow.cpp)
#pragma once

void ChatWindow_Init();

// HUD_Key_Event hook: true when the key opened the chat window (the engine must not act on it)
bool ChatWindow_KeyEvent( int down, int keynum, const char* binding );

// every engine frame, also while the main menu is up
void ChatWindow_Frame();

bool ChatWindow_IsOpen();
void ChatWindow_Close();

// one chat line, given in pieces: colour is r g b in 0..1, or NULL for the normal chat colour (con_color)
void ChatWindow_BeginLine();
void ChatWindow_AddText( const wchar_t* text, int length, const float* color );
void ChatWindow_EndLine();
