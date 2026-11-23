// author_avatar.h - the author's GitHub picture for the Advanced window's About tab (VGUI/about_page.cpp). Asked for,
// it is read from its copy on disk at once (also when offline) and downloaded again from GitHub on a thread of its
// own, once per game, so a new GitHub picture shows from the next game on.
#pragma once

#include <vector>

// starts the reading and download, the first time only; cacheFile: where the downloaded picture is kept
void AuthorAvatar_Request( const char* cacheFile );
// the picture's version: 0 while there is none, one more each time a picture arrives
int AuthorAvatar_Version();
// the latest picture as RGBA (wide x tall); false while there is none
bool AuthorAvatar_Copy( std::vector<unsigned char>& rgba, int& wide, int& tall );
// a download still running is cut off and waited for (HUD_Shutdown: the thread must end before client.dll unloads)
void AuthorAvatar_Shutdown();
