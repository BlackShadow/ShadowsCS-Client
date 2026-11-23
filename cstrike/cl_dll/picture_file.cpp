// picture_file.cpp - see picture_file.h. (user: "tga's are like uncompressed")
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincodec.h>
#include "picture_file.h"

#pragma comment( lib, "windowscodecs.lib" )
#pragma comment( lib, "ole32.lib" )

namespace
{

const UINT kMaxSide = 4096;

template <class T> void Release( T*& p )
{
	if ( p )
		p->Release();
	p = NULL;
}

} // namespace

bool PictureFile_Decode( const unsigned char* file, size_t length, std::vector<unsigned char>& rgba, int& wide, int& tall )
{
	if ( !file || length < 8 || length > 0x7fffffff )
		return false;
	// the decoder is a COM object: the thread needs COM (a thread that has it in another mode keeps its own)
	const HRESULT com = CoInitializeEx( NULL, COINIT_MULTITHREADED );
	IWICImagingFactory* factory = NULL;
	IWICStream* stream = NULL;
	IWICBitmapDecoder* decoder = NULL;
	IWICBitmapFrameDecode* frame = NULL;
	IWICFormatConverter* converter = NULL;
	bool ok = false;
	UINT w = 0, h = 0;
	if ( SUCCEEDED( CoCreateInstance( CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS( &factory ) ) )
		 && SUCCEEDED( factory->CreateStream( &stream ) )
		 && SUCCEEDED( stream->InitializeFromMemory( (BYTE*)file, (DWORD)length ) )
		 && SUCCEEDED( factory->CreateDecoderFromStream( stream, NULL, WICDecodeMetadataCacheOnLoad, &decoder ) )
		 && SUCCEEDED( decoder->GetFrame( 0, &frame ) )
		 && SUCCEEDED( factory->CreateFormatConverter( &converter ) )
		 && SUCCEEDED( converter->Initialize( frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, NULL, 0.0,
			 WICBitmapPaletteTypeCustom ) )
		 && SUCCEEDED( converter->GetSize( &w, &h ) ) && w > 0 && h > 0 && w <= kMaxSide && h <= kMaxSide )
	{
		rgba.resize( (size_t)w * h * 4 );
		ok = SUCCEEDED( converter->CopyPixels( NULL, w * 4, (UINT)rgba.size(), rgba.data() ) );
		wide = (int)w;
		tall = (int)h;
	}
	Release( converter );
	Release( frame );
	Release( decoder );
	Release( stream );
	Release( factory );
	if ( SUCCEEDED( com ) )
		CoUninitialize();
	if ( !ok )
		rgba.clear();
	return ok;
}
