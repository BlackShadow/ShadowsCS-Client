#include "extdll.h"
#include "util.h"
#include "debug.h"
#include "shared_util.h"

struct DebugOutputLevel
{
	const char *name;
	DebugOutputType value;
};

static DebugOutputLevel outputLevel[] =
{
	{ "bot", DEBUG_BOT },
	{ "career", DEBUG_CAREER },
	{ "tutor", DEBUG_TUTOR },
	{ "stats", DEBUG_STATS },
	{ "hostage", DEBUG_HOSTAGE },
	{ "all", DEBUG_ALL }
};

static unsigned int theDebugOutputTypes;
static char theDebugBuffer[1024];

void UTIL_DPrintf( DebugOutputType outputType, char *pszMsg, ... )
{
	va_list arguments;
	va_start( arguments, pszMsg );
	if ( CVAR_GET_FLOAT( "developer" ) > 0 && (outputType & theDebugOutputTypes) )
	{
		vsprintf( theDebugBuffer, pszMsg, arguments );
		g_engfuncs.pfnServerPrint( theDebugBuffer );
	}
	va_end( arguments );
}

void UTIL_DPrintf( char *pszMsg, ... )
{
	va_list arguments;
	va_start( arguments, pszMsg );
	if ( CVAR_GET_FLOAT( "developer" ) > 0 )
	{
		vsprintf( theDebugBuffer, pszMsg, arguments );
		g_engfuncs.pfnServerPrint( theDebugBuffer );
	}
	va_end( arguments );
}

static void SetDebugFlag( const char *flagStr, bool state )
{
	for ( int i = 0; flagStr && i < ARRAYSIZE( outputLevel ); ++i )
	{
		if ( FStrEq( flagStr, outputLevel[i].name ) && outputLevel[i].value != DEBUG_NONE )
		{
			if ( state )
				theDebugOutputTypes |= outputLevel[i].value;
			else
				theDebugOutputTypes &= ~outputLevel[i].value;
			g_engfuncs.pfnServerPrint( SharedVarArgs( "mp_debug: %s is now %s\n", flagStr, state ? "on" : "off" ) );
			return;
		}
	}
	g_engfuncs.pfnServerPrint( SharedVarArgs( "mp_debug: unknown variable '%s'\n", flagStr ) );
}

void PrintDebugFlag( const char *flagStr )
{
	for ( int i = 0; flagStr && i < ARRAYSIZE( outputLevel ); ++i )
	{
		if ( FStrEq( flagStr, outputLevel[i].name ) && outputLevel[i].value != DEBUG_NONE )
		{
			g_engfuncs.pfnServerPrint( SharedVarArgs( "mp_debug: %s is %s\n", flagStr,
				(outputLevel[i].value & theDebugOutputTypes) ? "on" : "off" ) );
			return;
		}
	}
	g_engfuncs.pfnServerPrint( SharedVarArgs( "mp_debug: unknown variable '%s'\n", flagStr ) );
}

void UTIL_SetDprintfFlags( const char *flagStr )
{
	if ( !(CVAR_GET_FLOAT( "developer" ) > 0) )
		return;
	if ( flagStr && *flagStr )
	{
		if ( *flagStr == '+' )
			SetDebugFlag( flagStr + 1, true );
		else if ( *flagStr == '-' )
			SetDebugFlag( flagStr + 1, false );
		else
			PrintDebugFlag( flagStr );
		return;
	}
	int remaining = sizeof( theDebugBuffer );
	theDebugBuffer[0] = 0;
	char *output = BufPrintf( theDebugBuffer, remaining, "mp_debug:\n" );
	for ( int i = 0; i < ARRAYSIZE( outputLevel ) - 1; ++i )
		output = BufPrintf( output, remaining, "  %s: %s\n", outputLevel[i].name,
			(outputLevel[i].value & theDebugOutputTypes) ? "on" : "off" );
	g_engfuncs.pfnServerPrint( theDebugBuffer );
}

void UTIL_BotDPrintf( char *pszMsg, ... )
{
	va_list arguments;
	va_start( arguments, pszMsg );
	if ( CVAR_GET_FLOAT( "developer" ) > 0 && (theDebugOutputTypes & DEBUG_BOT) )
	{
		vsprintf( theDebugBuffer, pszMsg, arguments );
		g_engfuncs.pfnServerPrint( theDebugBuffer );
	}
	va_end( arguments );
}

void UTIL_CareerDPrintf( char *pszMsg, ... )
{
	va_list arguments;
	va_start( arguments, pszMsg );
	if ( CVAR_GET_FLOAT( "developer" ) > 0 && (theDebugOutputTypes & DEBUG_CAREER) )
	{
		vsprintf( theDebugBuffer, pszMsg, arguments );
		g_engfuncs.pfnServerPrint( theDebugBuffer );
	}
	va_end( arguments );
}

void UTIL_TutorDPrintf( char *pszMsg, ... )
{
	va_list arguments;
	va_start( arguments, pszMsg );
	if ( CVAR_GET_FLOAT( "developer" ) > 0 && (theDebugOutputTypes & DEBUG_TUTOR) )
	{
		vsprintf( theDebugBuffer, pszMsg, arguments );
		g_engfuncs.pfnServerPrint( theDebugBuffer );
	}
	va_end( arguments );
}

void UTIL_StatsDPrintf( char *pszMsg, ... )
{
	va_list arguments;
	va_start( arguments, pszMsg );
	if ( CVAR_GET_FLOAT( "developer" ) > 0 && (theDebugOutputTypes & DEBUG_STATS) )
	{
		vsprintf( theDebugBuffer, pszMsg, arguments );
		g_engfuncs.pfnServerPrint( theDebugBuffer );
	}
	va_end( arguments );
}

void UTIL_HostageDPrintf( char *pszMsg, ... )
{
	va_list arguments;
	va_start( arguments, pszMsg );
	if ( CVAR_GET_FLOAT( "developer" ) > 0 && (theDebugOutputTypes & DEBUG_HOSTAGE) )
	{
		vsprintf( theDebugBuffer, pszMsg, arguments );
		g_engfuncs.pfnServerPrint( theDebugBuffer );
	}
	va_end( arguments );
}
