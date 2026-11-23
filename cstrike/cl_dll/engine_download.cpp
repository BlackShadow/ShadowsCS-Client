// engine_download.cpp - what the engine is downloading right now, read from hw.dll's memory.
//
// The engine keeps the list of files to download and their progress to itself: the client interface has nothing for
// it. Each value is found once by searching the engine's code for the few instructions that use it, then read
// directly. Every search must find exactly one place and pass a check; otherwise that part stays off. So an engine
// update that moves things only turns the extra download info off, it cannot make the client misread memory.
// Nothing of the engine is changed on disk; the one change in memory is the number of files it fetches at once from
// a web address (EngineDownload_SetWebParallel).
//
// Places searched (Half-Life 25th anniversary hw.dll, 2024-10-08 build; source reference HL25-Engine/engine):
//   CL_StartResourceDownloading (cl_parse.cpp):
//     lea eax,[ebp-24h]; push eax; push offset cl.resourcesneeded; call COM_SizeofResourceList;
//     mov esi,[cl.resourcesneeded.pNext]; add esp,8; xor edi,edi; mov [cls.dl.nTotalSize],eax; cmp esi,offset ...
//   CL_ReadPackets (cl_main.cpp):
//     movss xmm0,[scr_downloading.value]; xorps xmm1,xmm1; add esp,4; comiss xmm0,xmm1; jbe ..;
//     mov al,[gDownloadFile]; test al,al; je ..; cmp al,'!'
//   DownloadManager::StartNewDownload (download.cpp): cmp word ptr [ebx+5Eh],5 (at most 5 requests); jae ..;
//     cmp dword ptr [ebx+44h],0 - and CL_HTTPUpdate's "mov ecx,offset TheDownloadManager; jmp StartNewDownload"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <unordered_map>
#include "engine_download.h"

namespace
{

// the engine's resource_t (common/custom.h)
struct EngResource
{
	char szFileName[64];
	int type;
	int nIndex;
	int nDownloadSize;
	unsigned char ucFlags;
	unsigned char rgucMD5_hash[16];
	unsigned char playernum;
	unsigned char rguc_reserved[32];
	EngResource* pNext;
	EngResource* pPrev;
};
static_assert( offsetof( EngResource, nDownloadSize ) == 0x48, "resource_t layout" );
static_assert( offsetof( EngResource, ucFlags ) == 0x4C, "resource_t layout" );
static_assert( offsetof( EngResource, pNext ) == 0x80, "resource_t layout" );
static_assert( sizeof( EngResource ) == 0x88, "resource_t layout" );

const int kResSound = 0;						// t_sound: listed without "sound/"
// t_decal: other players' spray logos, never game files. hw CL_EstimateNeededResources marks every one carrying RES_CUSTOM
// as missing (cached in custom.hpk or not) and CL_ParseCustomization adds each player's uncached logo to the list too,
// asked of the server by its "!MD5..." name: counted, they showed "Downloaded 0 of N files" on a plain connect
const int kResDecal = 3;
const int kResGeneric = 4;						// t_generic: CL_ProcessFile matches those by the full name
const unsigned char kResWasMissing = 1 << 1;	// RES_WASMISSING: not on disk, being downloaded
const int kMaxListWalk = 8192;

// DownloadManager (download.cpp), 32-bit layout
const int kMgrQueueMemory = 0x38;		// CUtlVector<RequestContext*> m_queuedRequests: m_Memory.m_pMemory
const int kMgrQueueAlloc = 0x3C;		//   m_Memory.m_nAllocationCount
const int kMgrQueueCount = 0x44;		//   m_Size
const int kMgrActiveMemory = 0x4C;		// CUtlLinkedList<RequestContext*, unsigned short> m_activeRequest: m_pMemory
const int kMgrActiveAlloc = 0x50;		//   m_nAllocationCount
const int kMgrActiveHead = 0x58;		//   m_Head
const int kMgrActiveCount = 0x5E;		//   m_ElementCount
const int kMgrTotalRequests = 0x6C;		// m_totalRequests
// RequestContext
const int kReqStatus = 0x0;				// HTTPStatus
const int kReqGamePath = 0x408;			// char gamePath[1024]
const int kReqHandle = 0xC08;			// HTTPRequestHandle hOpenResource
const int kHttpDone = 2;
const int kHttpError = 4;
const unsigned short kInvalidIndex = 0xFFFF;

struct ListElem
{
	const unsigned char* request;
	unsigned short previous;
	unsigned short next;
};
static_assert( sizeof( ListElem ) == 8, "CUtlLinkedList element layout" );

typedef void* ( __cdecl* SteamHttpFn )();
typedef bool ( __cdecl* ProgressFn )( void* http, unsigned int request, float* percent );

struct Engine
{
	bool searched;
	const EngResource* needed;		// &cl.resourcesneeded, the head of a ring of the files still to come
	const float* downloading;		// &scr_downloading.value: percent of the file coming through the game connection
	const char* downloadFile;		// gDownloadFile: its name
	const unsigned char* manager;	// &TheDownloadManager: web downloads (FastDL)
	unsigned char* parallelLimit;	// the 5 of StartNewDownload's "at most 5 requests at once"
	unsigned char engineParallel;
	SteamHttpFn steamHttp;
	ProgressFn progress;
};
Engine g_engine;

// web: file sizes from the resource list (the requests only carry names), by lower-case game path
std::unordered_map<std::string, int> g_sizes;
const EngResource* g_sizesFirst;
int g_sizesCount = -1;

struct Image
{
	uintptr_t base;
	size_t size;
	const unsigned char* text;
	size_t textSize;
};

bool GetImage( const char* name, Image& image )
{
	const HMODULE module = GetModuleHandleA( name );
	if ( !module )
		return false;
	const unsigned char* base = reinterpret_cast<const unsigned char*>( module );
	const IMAGE_DOS_HEADER* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>( base );
	if ( dos->e_magic != IMAGE_DOS_SIGNATURE )
		return false;
	const IMAGE_NT_HEADERS* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>( base + dos->e_lfanew );
	if ( nt->Signature != IMAGE_NT_SIGNATURE )
		return false;
	image.base = reinterpret_cast<uintptr_t>( base );
	image.size = nt->OptionalHeader.SizeOfImage;
	const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION( nt );
	for ( int i = 0; i < nt->FileHeader.NumberOfSections; i++, section++ )
	{
		if ( !memcmp( section->Name, ".text", 6 ) )
		{
			image.text = base + section->VirtualAddress;
			image.textSize = section->Misc.VirtualSize;
			return true;
		}
	}
	return false;
}

int HexDigit( char c )
{
	if ( c >= '0' && c <= '9' )
		return c - '0';
	if ( c >= 'A' && c <= 'F' )
		return c - 'A' + 10;
	if ( c >= 'a' && c <= 'f' )
		return c - 'a' + 10;
	return -1;
}

// pattern "8D 45 ?? 50": hex bytes, ?? = any byte. The match when exactly one place matches, NULL otherwise.
const unsigned char* FindUnique( const Image& image, const char* pattern )
{
	int bytes[64];
	int count = 0;
	for ( const char* p = pattern; p[0] && p[1] && count < 64; )
	{
		if ( p[0] == ' ' )
		{
			p++;
			continue;
		}
		if ( p[0] == '?' )
			bytes[count++] = -1;
		else
		{
			const int high = HexDigit( p[0] ), low = HexDigit( p[1] );
			if ( high < 0 || low < 0 )
				return NULL;
			bytes[count++] = high * 16 + low;
		}
		p += 2;
	}
	if ( count == 0 || bytes[0] < 0 || image.textSize < (size_t)count )
		return NULL;
	const unsigned char* found = NULL;
	const size_t last = image.textSize - count;
	for ( size_t i = 0; i <= last; i++ )
	{
		const unsigned char* at = image.text + i;
		if ( at[0] != bytes[0] )
			continue;
		int k = 1;
		while ( k < count && ( bytes[k] < 0 || at[k] == bytes[k] ) )
			k++;
		if ( k < count )
			continue;
		if ( found )
			return NULL;	// more than one: not sure which, use none
		found = at;
	}
	return found;
}

uintptr_t ReadAddress( const unsigned char* at )
{
	uint32_t value;
	memcpy( &value, at, sizeof( value ) );
	return value;
}

bool InImage( const Image& image, uintptr_t address, size_t bytes )
{
	return address >= image.base && address + bytes <= image.base + image.size;
}

void Search()
{
	g_engine.searched = true;
	Image image;
	if ( !GetImage( "hw.dll", image ) && !GetImage( "sw.dll", image ) )
		return;

	// the files still to come
	const unsigned char* at = FindUnique( image,
		"8D 45 ?? 50 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 35 ?? ?? ?? ?? 83 C4 08 33 FF A3 ?? ?? ?? ?? 81 FE" );
	if ( at )
	{
		const uintptr_t list = ReadAddress( at + 5 );
		if ( InImage( image, list, sizeof( EngResource ) ) && ReadAddress( at + 16 ) == list + offsetof( EngResource, pNext )
			 && ReadAddress( at + 32 ) == list )
			g_engine.needed = reinterpret_cast<const EngResource*>( list );
	}

	// the file coming through the game connection and its percent
	at = FindUnique( image, "F3 0F 10 05 ?? ?? ?? ?? 0F 57 C9 83 C4 04 0F 2F C1 76 ?? A0 ?? ?? ?? ?? 84 C0 74 ?? 3C 21" );
	if ( at )
	{
		const uintptr_t percent = ReadAddress( at + 4 ), file = ReadAddress( at + 20 );
		if ( InImage( image, percent, sizeof( float ) ) && InImage( image, file, 64 ) )
		{
			g_engine.downloading = reinterpret_cast<const float*>( percent );
			g_engine.downloadFile = reinterpret_cast<const char*>( file );
		}
	}

	// web downloads: StartNewDownload's limit, then the manager object CL_HTTPUpdate hands it
	at = FindUnique( image, "66 83 7B 5E ?? 0F 83 ?? ?? ?? ?? 83 7B 44 00" );
	if ( at )
	{
		const unsigned char* start = NULL;
		for ( size_t back = 0; back < 0x100 && at - back > image.text + 1; back++ )
		{
			const unsigned char* p = at - back;
			if ( p[-1] == 0xCC && p[0] == 0x55 && p[1] == 0x8B && p[2] == 0xEC )	// int3 padding, push ebp; mov ebp,esp
			{
				start = p;
				break;
			}
		}
		const unsigned char* manager = NULL;
		int found = 0;
		for ( size_t i = 0; start && i + 10 <= image.textSize; i++ )
		{
			const unsigned char* p = image.text + i;
			if ( p[0] != 0xB9 || ( p[5] != 0xE9 && p[5] != 0xE8 ) )	// mov ecx,imm32; jmp/call rel32
				continue;
			int32_t relative;
			memcpy( &relative, p + 6, sizeof( relative ) );
			if ( p + 10 + relative != start )
				continue;
			const uintptr_t object = ReadAddress( p + 1 );
			if ( !InImage( image, object, kMgrTotalRequests + sizeof( int ) ) )
				continue;
			if ( found && reinterpret_cast<const unsigned char*>( object ) == manager )
				continue;
			manager = reinterpret_cast<const unsigned char*>( object );
			found++;
		}
		if ( found == 1 && at[4] > 0 && at[4] < 64 )
		{
			g_engine.manager = manager;
			g_engine.parallelLimit = const_cast<unsigned char*>( at + 4 );
			g_engine.engineParallel = at[4];
		}
	}

	// Steam's progress of a web request
	const HMODULE steam = GetModuleHandleA( "steam_api.dll" );
	if ( steam )
	{
		g_engine.steamHttp = reinterpret_cast<SteamHttpFn>( reinterpret_cast<void*>( GetProcAddress( steam, "SteamAPI_SteamHTTP_v003" ) ) );
		g_engine.progress = reinterpret_cast<ProgressFn>(
			reinterpret_cast<void*>( GetProcAddress( steam, "SteamAPI_ISteamHTTP_GetHTTPDownloadProgressPct" ) ) );
	}
}

void CopyName( char* out, size_t size, const char* name )
{
	strncpy( out, name, size - 1 );
	out[size - 1] = 0;
}

std::string SizeKey( const char* path )
{
	std::string key;
	for ( const char* p = path; *p && key.size() < 1024; p++ )
	{
		char c = *p;
		if ( c == '\\' )
			c = '/';
		else if ( c >= 'A' && c <= 'Z' )
			c = (char)( c - 'A' + 'a' );
		key += c;
	}
	return key;
}

// the size table for the web requests, rebuilt when the resource list changes
void UpdateSizes()
{
	const EngResource* head = g_engine.needed;
	int count = 0;
	for ( const EngResource* r = head->pNext; r && r != head && count < kMaxListWalk; r = r->pNext )
		count++;
	if ( head->pNext == g_sizesFirst && count == g_sizesCount )
		return;
	g_sizesFirst = head->pNext;
	g_sizesCount = count;
	g_sizes.clear();
	int walked = 0;
	for ( const EngResource* r = head->pNext; r && r != head && walked < kMaxListWalk; r = r->pNext, walked++ )
	{
		char name[sizeof( r->szFileName ) + 1];
		memcpy( name, r->szFileName, sizeof( r->szFileName ) );
		name[sizeof( r->szFileName )] = 0;
		const std::string key = r->type == kResSound ? SizeKey( ( std::string( "sound/" ) + name ).c_str() ) : SizeKey( name );
		g_sizes[key] = r->nDownloadSize;
	}
}

void AddWebFile( EngineDownloadStatus& status, const unsigned char* request, bool active, void* http )
{
	char path[256];
	CopyName( path, sizeof( path ), reinterpret_cast<const char*>( request + kReqGamePath ) );
	const auto size = g_sizes.find( SizeKey( path ) );
	const double bytes = size != g_sizes.end() && size->second > 0 ? (double)size->second : 0.0;
	status.filesLeft++;
	status.bytesLeft += bytes;
	if ( !active )
		return;
	if ( !status.currentFile[0] )
		CopyName( status.currentFile, sizeof( status.currentFile ), path );
	int state;
	memcpy( &state, request + kReqStatus, sizeof( state ) );
	if ( state == kHttpDone || state == kHttpError )
	{
		status.bytesArrived += bytes;	// finished, leaves the list next frame
		return;
	}
	unsigned int handle;
	memcpy( &handle, request + kReqHandle, sizeof( handle ) );
	float percent = 0.0f;
	if ( http && handle && g_engine.progress && g_engine.progress( http, handle, &percent ) )
	{
		if ( percent < 0.0f )
			percent = 0.0f;
		else if ( percent > 100.0f )
			percent = 100.0f;
		status.bytesArrived += bytes * percent / 100.0;
	}
}

bool ReadWeb( EngineDownloadStatus& status )
{
	const unsigned char* manager = g_engine.manager;
	int queued, queueAlloc, activeAlloc;
	unsigned short active, index;
	memcpy( &queued, manager + kMgrQueueCount, sizeof( queued ) );
	memcpy( &queueAlloc, manager + kMgrQueueAlloc, sizeof( queueAlloc ) );
	memcpy( &active, manager + kMgrActiveCount, sizeof( active ) );
	memcpy( &activeAlloc, manager + kMgrActiveAlloc, sizeof( activeAlloc ) );
	memcpy( &index, manager + kMgrActiveHead, sizeof( index ) );
	if ( queued <= 0 && active == 0 )
		return false;
	if ( queued < 0 || queued > queueAlloc || queued > 65536 || active > activeAlloc )
		return false;	// not what this code expects: leave it alone
	UpdateSizes();
	status.web = true;
	memcpy( &status.webFilesTotal, manager + kMgrTotalRequests, sizeof( status.webFilesTotal ) );
	void* http = g_engine.steamHttp ? g_engine.steamHttp() : NULL;
	const unsigned char* const* queue;
	memcpy( &queue, manager + kMgrQueueMemory, sizeof( queue ) );
	for ( int i = 0; queue && i < queued; i++ )
	{
		if ( queue[i] )
			AddWebFile( status, queue[i], false, http );
	}
	const ListElem* elements;
	memcpy( &elements, manager + kMgrActiveMemory, sizeof( elements ) );
	for ( int steps = 0; elements && index != kInvalidIndex && index < activeAlloc && steps < active; steps++ )
	{
		if ( elements[index].request )
			AddWebFile( status, elements[index].request, true, http );
		index = elements[index].next;
	}
	return status.filesLeft > 0;
}

bool ReadGameConnection( EngineDownloadStatus& status )
{
	char current[64] = "";
	float percent = 0.0f;
	if ( g_engine.downloadFile && g_engine.downloading && *g_engine.downloading > 0.0f )
	{
		CopyName( current, sizeof( current ), g_engine.downloadFile );
		percent = *g_engine.downloading > 100.0f ? 100.0f : *g_engine.downloading;
	}
	const char* withoutSound = !_strnicmp( current, "sound/", 6 ) ? current + 6 : current;
	const EngResource* head = g_engine.needed;
	int walked = 0;
	for ( const EngResource* r = head->pNext; r && r != head && walked < kMaxListWalk; r = r->pNext, walked++ )
	{
		if ( !( r->ucFlags & kResWasMissing ) || r->type == kResDecal )
			continue;	// not missing, or a player's spray logo (kResDecal)
		status.filesLeft++;
		status.bytesLeft += r->nDownloadSize > 0 ? r->nDownloadSize : 0;
		if ( current[0] && !status.currentFile[0]
			 && ( !_strnicmp( r->szFileName, current, sizeof( r->szFileName ) )
				  || ( r->type != kResGeneric && !_strnicmp( r->szFileName, withoutSound, sizeof( r->szFileName ) ) ) ) )
		{
			CopyName( status.currentFile, sizeof( status.currentFile ), current );
			status.bytesArrived = ( r->nDownloadSize > 0 ? r->nDownloadSize : 0 ) * (double)percent / 100.0;
		}
	}
	return status.filesLeft > 0;
}

} // namespace

bool EngineDownload_Read( EngineDownloadStatus& status )
{
	memset( &status, 0, sizeof( status ) );
	if ( !g_engine.searched )
		Search();
	if ( !g_engine.needed || !g_engine.needed->pNext )
		return false;
	if ( g_engine.manager && ReadWeb( status ) )
		return true;
	memset( &status, 0, sizeof( status ) );
	return ReadGameConnection( status );
}

// ---- the loading box's status line ----
// IGameUI (HL25-Engine/engine/IGameUI.h): slot 0 the IBaseInterface destructor, then Initialize, Start, Shutdown,
// ActivateGameUI, ActivateDemoUI, HasExclusiveInput, RunFrame, ConnectToServer, DisconnectFromServer, HideGameUI,
// IsGameUIActive, LoadingStarted, LoadingFinished, StartProgressBar, ContinueProgressBar, StopProgressBar,
// SetProgressBarStatusText = 17 (hw.dll's SetLoadingProgressBarStatusText calls [vtable+44h]).
namespace
{
const int kStatusTextSlot = 17;
typedef int( __fastcall* StatusTextFn )( void* self, void* unused, const char* text );	// a thiscall
StatusTextFn g_pfnStatusText;
void** g_pStatusSlot;
void* g_pHookedGameUI;
char g_szStatusText[128];
char g_szEngineText[128];	// the last status text the engine sent (shown again when ours comes off)

int __fastcall HookStatusText( void* self, void* unused, const char* text )
{
	if ( text )
	{
		strncpy( g_szEngineText, text, sizeof( g_szEngineText ) - 1 );
		g_szEngineText[sizeof( g_szEngineText ) - 1] = 0;
	}
	return g_pfnStatusText( self, unused, g_szStatusText[0] ? g_szStatusText : text );
}

bool WriteSlot( void** slot, void* value )
{
	DWORD protection;
	if ( !VirtualProtect( slot, sizeof( void* ), PAGE_READWRITE, &protection ) )
		return false;
	*slot = value;
	VirtualProtect( slot, sizeof( void* ), protection, &protection );
	return true;
}

// the function must lie in the module that holds the object's vtable (GameUI.dll), or this is not what we think it is
bool InModuleOf( const void* object, const void* code )
{
	HMODULE module = NULL;
	if ( !GetModuleHandleExA( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			 reinterpret_cast<LPCSTR>( *reinterpret_cast<void* const*>( object ) ), &module ) || !module )
		return false;
	const unsigned char* base = reinterpret_cast<const unsigned char*>( module );
	const IMAGE_NT_HEADERS* nt =
		reinterpret_cast<const IMAGE_NT_HEADERS*>( base + reinterpret_cast<const IMAGE_DOS_HEADER*>( base )->e_lfanew );
	const uintptr_t address = reinterpret_cast<uintptr_t>( code );
	return address >= reinterpret_cast<uintptr_t>( base ) && address < reinterpret_cast<uintptr_t>( base ) + nt->OptionalHeader.SizeOfImage;
}

bool HookStatus( void* gameUI )
{
	if ( g_pfnStatusText )
		return g_pHookedGameUI == gameUI;
	if ( !gameUI )
		return false;
	void** vtable = *reinterpret_cast<void***>( gameUI );
	void** slot = &vtable[kStatusTextSlot];
	if ( *slot == reinterpret_cast<void*>( &HookStatusText ) || !InModuleOf( gameUI, *slot ) )
		return false;
	StatusTextFn original = reinterpret_cast<StatusTextFn>( *slot );
	if ( !WriteSlot( slot, reinterpret_cast<void*>( &HookStatusText ) ) )
		return false;
	g_pfnStatusText = original;
	g_pStatusSlot = slot;
	g_pHookedGameUI = gameUI;
	return true;
}
} // namespace

void EngineDownload_SetStatusText( void* gameUI, const char* text )
{
	if ( !text || !text[0] )
	{
		const bool ours = g_szStatusText[0] != 0;
		g_szStatusText[0] = 0;
		// the engine's own line back at once: after a web download the engine reconnects ("retry", hw.so's
		// DownloadManager::CheckActiveDownload) and its first texts for that could have come while ours was up - the
		// count then stayed on while the reconnect's bar filled from the start
		if ( ours && g_pfnStatusText && g_pHookedGameUI && g_szEngineText[0] )
			g_pfnStatusText( g_pHookedGameUI, NULL, g_szEngineText );
		return;
	}
	if ( !HookStatus( gameUI ) )
		return;
	if ( !strcmp( g_szStatusText, text ) )
		return;
	strncpy( g_szStatusText, text, sizeof( g_szStatusText ) - 1 );
	g_szStatusText[sizeof( g_szStatusText ) - 1] = 0;
	g_pfnStatusText( gameUI, NULL, g_szStatusText );	// show it now (does nothing when there is no loading box)
}

void EngineDownload_Unhook()
{
	g_szStatusText[0] = 0;
	if ( g_pStatusSlot && g_pfnStatusText && *g_pStatusSlot == reinterpret_cast<void*>( &HookStatusText ) )
		WriteSlot( g_pStatusSlot, reinterpret_cast<void*>( g_pfnStatusText ) );
	g_pStatusSlot = NULL;
	g_pfnStatusText = NULL;
	g_pHookedGameUI = NULL;
}

void EngineDownload_Describe( char* out, int size )
{
	if ( !g_engine.searched )
		Search();
	_snprintf( out, size - 1, "file list %s, current file %s, web downloads %s (at once: %d, engine %d), web progress %s",
		g_engine.needed ? "found" : "NOT FOUND", g_engine.downloadFile ? "found" : "NOT FOUND",
		g_engine.manager ? "found" : "NOT FOUND", g_engine.parallelLimit ? *g_engine.parallelLimit : 0,
		g_engine.engineParallel, g_engine.steamHttp && g_engine.progress ? "found" : "NOT FOUND" );
	out[size - 1] = 0;
}

void EngineDownload_SetWebParallel( int count )
{
	if ( !g_engine.searched )
		Search();
	unsigned char* limit = g_engine.parallelLimit;
	if ( !limit )
		return;
	const unsigned char wanted = count > 0 && count < 64 ? (unsigned char)count : g_engine.engineParallel;
	if ( *limit == wanted )
		return;
	DWORD protection;
	if ( !VirtualProtect( limit, 1, PAGE_EXECUTE_READWRITE, &protection ) )
		return;
	*limit = wanted;
	VirtualProtect( limit, 1, protection, &protection );
	FlushInstructionCache( GetCurrentProcess(), limit, 1 );
}
