// author_avatar.cpp - see author_avatar.h. The picture is https://github.com/BlackShadow.png?size=128 (GitHub sends
// whatever picture the account has now, PNG or JPEG), fetched with WinINet and decoded with Windows' own image decoder
// (WIC). The file is kept as it came; a picture that does not decode is not shown and not kept.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wininet.h>
#include <wincodec.h>
#include <stdio.h>
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "author_avatar.h"

#pragma comment( lib, "wininet.lib" )
#pragma comment( lib, "windowscodecs.lib" )
#pragma comment( lib, "ole32.lib" )

namespace
{

const char kPictureURL[] = "https://github.com/BlackShadow.png?size=128";
const size_t kMaxFileBytes = 4 << 20;
const UINT kMaxSide = 1024;
const DWORD kTimeoutMs = 10000;

std::mutex g_lock;
std::vector<unsigned char> g_rgba;
int g_wide, g_tall;
std::atomic<int> g_version( 0 );
std::atomic<bool> g_started( false );
std::atomic<bool> g_stop( false );
std::thread g_thread;
std::atomic<HINTERNET> g_net( NULL );

template <class T> void Release( T*& p )
{
	if ( p )
		p->Release();
	p = NULL;
}

bool Decode( const std::vector<unsigned char>& file, std::vector<unsigned char>& rgba, int& wide, int& tall )
{
	if ( file.empty() )
		return false;
	IWICImagingFactory* factory = NULL;
	IWICStream* stream = NULL;
	IWICBitmapDecoder* decoder = NULL;
	IWICBitmapFrameDecode* frame = NULL;
	IWICFormatConverter* converter = NULL;
	bool ok = false;
	UINT w = 0, h = 0;
	if ( SUCCEEDED( CoCreateInstance( CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS( &factory ) ) )
		&& SUCCEEDED( factory->CreateStream( &stream ) )
		&& SUCCEEDED( stream->InitializeFromMemory( (BYTE*)file.data(), (DWORD)file.size() ) )
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
	return ok;
}

void Publish( std::vector<unsigned char>& rgba, int wide, int tall )
{
	std::lock_guard<std::mutex> hold( g_lock );
	g_rgba.swap( rgba );
	g_wide = wide;
	g_tall = tall;
	g_version++;
}

bool ReadWhole( const std::string& path, std::vector<unsigned char>& out )
{
	FILE* f = fopen( path.c_str(), "rb" );
	if ( !f )
		return false;
	out.clear();
	unsigned char buffer[16384];
	size_t n;
	while ( ( n = fread( buffer, 1, sizeof( buffer ), f ) ) > 0 && out.size() < kMaxFileBytes )
		out.insert( out.end(), buffer, buffer + n );
	fclose( f );
	return !out.empty();
}

void WriteWhole( const std::string& path, const std::vector<unsigned char>& data )
{
	const std::string temp = path + ".part";
	FILE* f = fopen( temp.c_str(), "wb" );
	if ( !f )
		return;
	const bool ok = fwrite( data.data(), 1, data.size(), f ) == data.size();
	fclose( f );
	if ( !ok || !MoveFileExA( temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING ) )
		DeleteFileA( temp.c_str() );
}

// the picture GitHub has now (redirects followed, nothing taken from or put in the system's web cache)
bool Download( std::vector<unsigned char>& out )
{
	HINTERNET net = InternetOpenA( "Shadows", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0 );
	if ( !net )
		return false;
	g_net = net;
	DWORD timeout = kTimeoutMs;
	InternetSetOptionA( net, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof( timeout ) );
	InternetSetOptionA( net, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof( timeout ) );
	bool ok = false;
	HINTERNET url = g_stop ? NULL : InternetOpenUrlA( net, kPictureURL, NULL, 0, INTERNET_FLAG_RELOAD |
		INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_NO_COOKIES | INTERNET_FLAG_NO_UI | INTERNET_FLAG_SECURE, 0 );
	if ( url )
	{
		DWORD status = 0, size = sizeof( status );
		if ( HttpQueryInfoA( url, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &status, &size, NULL ) && status == 200 )
		{
			out.clear();
			unsigned char buffer[16384];
			DWORD got = 0;
			while ( !g_stop && InternetReadFile( url, buffer, sizeof( buffer ), &got ) && got > 0 && out.size() < kMaxFileBytes )
				out.insert( out.end(), buffer, buffer + got );
			ok = !g_stop && !out.empty() && got == 0;	// read to the end
		}
		InternetCloseHandle( url );
	}
	HINTERNET mine = g_net.exchange( NULL );	// NULL when AuthorAvatar_Shutdown closed it already
	if ( mine )
		InternetCloseHandle( mine );
	return ok;
}

void Worker( std::string cacheFile )
{
	const HRESULT com = CoInitializeEx( NULL, COINIT_MULTITHREADED );
	std::vector<unsigned char> file, rgba;
	int wide = 0, tall = 0;
	// the copy on disk first: shown at once, also when there is no connection
	if ( ReadWhole( cacheFile, file ) && Decode( file, rgba, wide, tall ) )
		Publish( rgba, wide, tall );
	// then the one GitHub has now
	if ( !g_stop && Download( file ) && Decode( file, rgba, wide, tall ) )
	{
		Publish( rgba, wide, tall );
		WriteWhole( cacheFile, file );
	}
	if ( SUCCEEDED( com ) )
		CoUninitialize();
}

} // namespace

void AuthorAvatar_Request( const char* cacheFile )
{
	bool expected = false;
	if ( !cacheFile || !g_started.compare_exchange_strong( expected, true ) )
		return;
	g_thread = std::thread( Worker, std::string( cacheFile ) );
}

int AuthorAvatar_Version()
{
	return g_version;
}

bool AuthorAvatar_Copy( std::vector<unsigned char>& rgba, int& wide, int& tall )
{
	std::lock_guard<std::mutex> hold( g_lock );
	if ( g_rgba.empty() )
		return false;
	rgba = g_rgba;
	wide = g_wide;
	tall = g_tall;
	return true;
}

void AuthorAvatar_Shutdown()
{
	g_stop = true;
	HINTERNET net = g_net.exchange( NULL );
	if ( net )
		InternetCloseHandle( net );	// cuts off a read or connect still waiting
	if ( g_thread.joinable() )
		g_thread.join();
}
