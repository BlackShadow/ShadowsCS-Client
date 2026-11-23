// picture_file.h - the pixels of a PNG or JPEG file, read with Windows' own image decoder (WIC): the client's own big
// pictures (the menu backgrounds, the Advanced window's backdrop and banner) are PNGs - as TGAs they were three times
// the size. No engine headers; may be called from any thread.
#pragma once

#include <stddef.h>
#include <vector>

// The picture in 'file' (a whole file's bytes) as RGBA, four bytes a pixel, top row first. False when the bytes are
// not a picture the decoder knows (a TGA is not: the callers have their own reader for those) or it is over 4096
// pixels a side.
bool PictureFile_Decode( const unsigned char* file, size_t length, std::vector<unsigned char>& rgba, int& wide, int& tall );
