#pragma once

enum DebugOutputType
{
	DEBUG_NONE		= 0,
	DEBUG_BOT		= 1,
	DEBUG_CAREER	= 2,
	DEBUG_TUTOR		= 4,
	DEBUG_STATS		= 8,
	DEBUG_HOSTAGE	= 16,
	DEBUG_ALL		= 0xFFFFFFFF
};

void UTIL_DPrintf( DebugOutputType outputType, char *pszMsg, ... );
void UTIL_DPrintf( char *pszMsg, ... );
void UTIL_BotDPrintf( char *pszMsg, ... );
void UTIL_CareerDPrintf( char *pszMsg, ... );
void UTIL_TutorDPrintf( char *pszMsg, ... );
void UTIL_StatsDPrintf( char *pszMsg, ... );
void UTIL_HostageDPrintf( char *pszMsg, ... );
void PrintDebugFlag( const char *flagStr );
void UTIL_SetDprintfFlags( const char *flagStr );
