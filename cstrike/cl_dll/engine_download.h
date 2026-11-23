// engine_download.h - what the engine is downloading right now (read from hw.dll's memory, see engine_download.cpp)
#pragma once

struct EngineDownloadStatus
{
	bool web;				// the files come from the server's web address (FastDL), not through the game connection
	int filesLeft;			// files still to come, the ones arriving now included
	int webFilesTotal;		// web: all files of this round, as the engine counts them (0 otherwise)
	double bytesLeft;		// sizes of the files still to come, the ones arriving now counted whole
	double bytesArrived;	// how much of the files arriving now is here already
	char currentFile[64];	// a file arriving now ("" when none)
};

// false while nothing is downloading, or when the engine's data could not be found (other engine version)
bool EngineDownload_Read( EngineDownloadStatus& status );

// how many files the engine fetches at once from a web address (the engine's own number is 5); 0 = engine default
void EngineDownload_SetWebParallel( int count );

// which engine values were found, for the log ("list yes, file yes, web yes, parallel 8, steam yes")
void EngineDownload_Describe( char* out, int size );

// The loading box's status line (the "Verifying and downloading resources..." text), set through the game menu's own
// IGameUI::SetProgressBarStatusText. While a text is given here, every status text the engine sends is replaced by
// it; NULL gives the line back to the engine, showing again at once the last text the engine sent (or the line would
// keep ours until the engine's next one). Hooks that one IGameUI function (vtable slot) the first time.
void EngineDownload_SetStatusText( void* gameUI, const char* text );
// puts the IGameUI function back (client shutdown)
void EngineDownload_Unhook();
