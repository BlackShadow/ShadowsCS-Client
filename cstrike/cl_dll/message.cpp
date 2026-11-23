/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
// Message.cpp
//
// implementation of CHudMessage class
//

#include <string.h>
#include <stdlib.h>
#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "misc_features.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "../../engine/controls.h"
#include "../../vgui2/controls/controls.h"

DECLARE_MESSAGE( m_Message, HudText );
DECLARE_MESSAGE( m_Message, HudTextPro );
DECLARE_MESSAGE( m_Message, HudTextArgs );
DECLARE_MESSAGE( m_Message, GameTitle );

vgui2::HFont _font;
vgui2::HFont _Newfont;
static cvar_t* hud_message_scale;

// Server text (AMX Mod X hud messages, director messages) uses the scheme's fixed-size fonts, which are tiny on
// big screens. These are bigger copies of the same fonts: screen height / 1080 (hud_message_scale overrides it,
// 1 = original size). Below 1080 lines nothing changes.
static vgui2::HFont MessageFont( bool legacy )
{
	const vgui2::HFont base = legacy ? _font : _Newfont;
	if ( !base )
		return base;
	float scale = hud_message_scale ? hud_message_scale->value : 0.0f;
	if ( scale <= 0.0f )
		scale = ScreenHeight / 1080.0f;
	if ( scale > 4.0f )
		scale = 4.0f;
	if ( scale < 1.05f )
		return base;
	const int baseTall = vgui2::surface()->GetFontTall( base );
	const int tall = (int)( ( baseTall > 0 ? baseTall : 18 ) * scale + 0.5f );
	struct Entry
	{
		bool legacy;
		int tall;
		vgui2::HFont font;
	};
	static Entry cache[8];
	static int count = 0;
	static int nextReuse = 0;
	for ( int i = 0; i < count; i++ )
	{
		if ( cache[i].legacy == legacy && cache[i].tall == tall )
			return cache[i].font ? cache[i].font : base;
	}
	// same face and weight as Legacy_CreditsFont / CreditsFont
	vgui2::HFont font = vgui2::surface()->CreateFont();
	if ( font && !vgui2::surface()->AddGlyphSetToFont( font, "Trebuchet MS", tall, legacy ? 700 : 600, 0, 0,
		vgui2::ISurface::FONTFLAG_ANTIALIAS, 0, 0 ) )
		font = 0;
	// a full cache used to make a brand new font for every message from then on; reuse the oldest slot instead
	// (fonts are never freed, so messages still using the old one keep working)
	int slot = count;
	if ( count < 8 )
		count++;
	else
	{
		slot = nextReuse;
		nextReuse = ( nextReuse + 1 ) % 8;
	}
	cache[slot].legacy = legacy;
	cache[slot].tall = tall;
	cache[slot].font = font;
	return font ? font : base;
}

int CHudMessage::Init()
{
	HOOK_MESSAGE( HudText );
	HOOK_MESSAGE( HudTextPro );
	HOOK_MESSAGE( HudTextArgs );
	HOOK_MESSAGE( GameTitle );
	hud_message_scale = gEngfuncs.pfnRegisterVariable( "hud_message_scale", "0", FCVAR_ARCHIVE );
	gHUD.AddHudElem( this );
	Reset();
	return 1;
}

void CHudMessage::Reset()
{
	memset( m_pMessages, 0, sizeof( m_pMessages ) );
	memset( m_startTime, 0, sizeof( m_startTime ) );
	memset( m_holdExtra, 0, sizeof( m_holdExtra ) );
	m_parms.holdExtra = 0;
	m_gameTitleTime = 0;
	m_pGameTitle = NULL;
}

int CHudMessage::VidInit()
{
	m_HUD_title_half = gHUD.GetSpriteIndex( "title_half" );
	m_HUD_title_life = gHUD.GetSpriteIndex( "title_life" );
	vgui2::IScheme* scheme = vgui2::scheme()->GetIScheme( vgui2::scheme()->GetDefaultScheme() );
	if ( !_font )
		_font = scheme->GetFont( "Legacy_CreditsFont", true );
	if ( !_Newfont )
		_Newfont = scheme->GetFont( "CreditsFont", true );
	return 1;
}

float CHudMessage::FadeBlend( float fadein, float fadeout, float hold, float time )
{
	if ( time >= 0 )
	{
		if ( fadein > time )
			return 1.0 - ( fadein - time ) / fadein;
		double stop = (double)fadein + hold;
		if ( time <= stop )
			return 1;
		if ( fadeout > 0 )
			return 1.0 - ( time - stop ) / fadeout;
	}
	return 0;
}

int CHudMessage::XPosition( float x, int width, int totalWidth )
{
	int position;
	if ( x == -1 )
		position = ( ScreenWidth - width ) / 2;
	else if ( x < 0 )
		position = (int)( ( x + 1.0 ) * ScreenWidth - totalWidth );
	else
	{
		float scaled = ScreenWidth * x;
		position = (int)scaled;
	}
	if ( position + width > ScreenWidth )
		return ScreenWidth - width;
	return position < 0 ? 0 : position;
}

int CHudMessage::YPosition( float y, int height )
{
	int position;
	if ( y == -1 )
		position = (int)( ( ScreenHeight - height ) * 0.5 );
	else if ( y < 0 )
		position = (int)( ( y + 1.0 ) * ScreenHeight - height );
	else
	{
		float scaled = ScreenHeight * y;
		position = (int)scaled;
	}
	if ( position + height > ScreenHeight )
		return ScreenHeight - height;
	return position < 0 ? 0 : position;
}

// When a message's display time is over (see Draw); false for effects Draw never shows or frees.
bool CHudMessage::MessageEndTime( int index, float& endTime ) const
{
	const client_textmessage_t* message = m_pMessages[index].pMessage;
	switch ( message->effect )
	{
	case 0:
	case 1:
		endTime = m_startTime[index] + message->fadein + message->fadeout + message->holdtime;
		break;
	case 2:
		if ( message->pMessage && *message->pMessage )
			endTime = message->fadein * strlen( message->pMessage ) + m_startTime[index] + message->fadeout + message->holdtime;
		else
			endTime = m_startTime[index] + message->fadein + message->fadeout + message->holdtime;
		break;
	default:
		return false;
	}
	endTime += m_holdExtra[index];
	return true;
}

// Servers (AMX Mod X plugins most of all) send the same hud message again and again, often several times a
// second and long before the copy on screen has faded. Each send used to take another of the 16 slots and draw
// the whole text again on top of the old copies, so one line could cost up to 16 times its drawing work every
// frame. Now a send that matches a message still on screen keeps that one and only moves its end to where the
// new copy would have ended. It looks the same: the old copy was already showing the new text (an engine
// channel is a single shared buffer) or exactly the same text at the same place in the same colour and effect.
// The only difference is that stacked copies no longer add up into brighter, thicker letters.
int CHudMessage::FindShownCopy( const client_textmessage_t* message, unsigned int font, float time )
{
	if ( !message )
		return -1;
	for ( int i = 0; i < 16; i++ )
	{
		const client_message_t& shown = m_pMessages[i];
		const client_textmessage_t* other = shown.pMessage;
		if ( !other )
			continue;
		float endTime;
		if ( MessageEndTime( i, endTime ) && time > endTime )
			continue;	// already over, Draw frees it
		if ( other == message )
			return i;

		// a different buffer (the next engine channel, the next director message slot): plain text only, identical
		// in every visible way; localised "#" titles can carry arguments, so those only ever match themselves
		if ( shown.font != font || shown.numArgs || !other->pMessage || !message->pMessage || *message->pMessage == '#'
			|| other->effect != message->effect || other->x != message->x || other->y != message->y
			|| other->r1 != message->r1 || other->g1 != message->g1 || other->b1 != message->b1
			|| other->fadein != message->fadein )
			continue;
		if ( message->effect == 2 && ( other->r2 != message->r2 || other->g2 != message->g2 || other->b2 != message->b2
			|| other->fxtime != message->fxtime ) )
			continue;
		if ( !strcmp( other->pMessage, message->pMessage ) )
			return i;
	}
	return -1;
}

void CHudMessage::ExtendShown( int index, unsigned int font, float time )
{
	const float extra = time - m_startTime[index];
	if ( extra > m_holdExtra[index] )
		m_holdExtra[index] = extra;
	m_pMessages[index].font = font;
}

int CHudMessage::MessageAdd( const char* name, float time, int hintMessage, unsigned int font )
{
	int index;
	client_textmessage_t* message;
	if ( *name == '#' )
	{
		message = TextMessageGet( name + 1 );
		if ( message && hintMessage )
		{
			message->effect = 2;
			message->r1 = 40;
			message->g1 = 255;
			message->b1 = 40;
			message->a1 = 200;
			message->r2 = 0;
			message->g2 = 255;
			message->b2 = 0;
			message->a2 = 200;
			message->x = -1;
			message->y = 0.7;
			message->fadein = 0.01;
			message->fadeout = 0.7;
			message->fxtime = 0.07;
			message->holdtime = 5;
			if ( !strcmp( name, "#Spec_Duck" ) )
				message->holdtime = 6;
			else if ( message->pMessage )
			{
				float duration = (float)( strlen( message->pMessage ) / 25 );
				message->holdtime = duration >= 1 ? duration : 1;
			}
			else
			{
				message->holdtime = 1;
			}
		}
	}
	else
	{
		message = TextMessageGet( name );
	}
	index = FindShownCopy( message, font, time );
	if ( index >= 0 )
	{
		ExtendShown( index, font, time );
		m_pMessages[index].pMessage = message;
		return index;
	}
	for ( index = 0; index < 16; index++ )
	{
		if ( !m_pMessages[index].pMessage )
			break;
	}
	if ( index == 16 )
		return -1;
	m_pMessages[index].pMessage = message;
	m_pMessages[index].font = font;
	m_pMessages[index].numArgs = 0;
	m_startTime[index] = time;
	m_holdExtra[index] = 0;
	return index;
}

void CHudMessage::MessageAdd( client_textmessage_t* message )
{
	if ( !gEngfuncs.IsSpectateOnly() && (unsigned int)( g_iTeamNumber - 1 ) <= 1
		&& !strcmp( message->pMessage, "#Spec_Duck" ) )
		return;
	m_parms.time = gHUD.m_flTime;
	m_iFlags |= HUD_ACTIVE;
	const unsigned int font = MessageFont( false );
	const int shown = FindShownCopy( message, font, gHUD.m_flTime );
	if ( shown >= 0 )
	{
		ExtendShown( shown, font, gHUD.m_flTime );
		m_pMessages[shown].pMessage = message;
		return;
	}
	for ( int i = 0; i < 16; i++ )
	{
		if ( !m_pMessages[i].pMessage )
		{
			m_pMessages[i].pMessage = message;
			m_pMessages[i].font = font;
			m_pMessages[i].numArgs = 0;
			m_startTime[i] = gHUD.m_flTime;
			m_holdExtra[i] = 0;
			return;
		}
	}
}

int CHudMessage::MsgFunc_HudText( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	const char* message = READ_STRING();
	int hint = READ_BYTE();
	if ( MiscFeatures_HidePluginScreenText( message ) )
		return 1;	// a server plugin's text on the screen, with Hide AMXX texts on (misc_features.cpp)
	MessageAdd( message, gHUD.m_flTime, hint, MessageFont( true ) );
	m_parms.time = gHUD.m_flTime;
	m_iFlags |= HUD_ACTIVE;
	return 1;
}

int CHudMessage::MsgFunc_HudTextPro( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	const char* message = READ_STRING();
	int hint = READ_BYTE();
	MessageAdd( message, gHUD.m_flTime, hint, MessageFont( false ) );
	m_parms.time = gHUD.m_flTime;
	m_iFlags |= HUD_ACTIVE;
	return 1;
}

int CHudMessage::MsgFunc_HudTextArgs( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	const char* name = READ_STRING();
	int hint = READ_BYTE();
	int index = MessageAdd( name, gHUD.m_flTime, hint, MessageFont( false ) );
	if ( index >= 0 )
	{
		client_message_t& message = m_pMessages[index];
		message.numArgs = READ_BYTE();
		if ( message.numArgs > MAX_MESSAGE_ARGS )
			message.numArgs = MAX_MESSAGE_ARGS;
		if ( message.numArgs < 0 )
			message.numArgs = 0;
		for ( int i = 0; i < message.numArgs; i++ )
		{
			const char* argument = READ_STRING();
			if ( !argument )
				argument = "";
			vgui2::localize()->ConvertANSIToUnicode( argument, message.args[i], sizeof( message.args[i] ) );
		}
	}
	m_parms.time = gHUD.m_flTime;
	m_iFlags |= HUD_ACTIVE;
	return 1;
}

int CHudMessage::MsgFunc_GameTitle( const char*, int, void* )
{
	m_pGameTitle = TextMessageGet( "GAMETITLE" );
	if ( m_pGameTitle )
	{
		m_gameTitleTime = gHUD.m_flTime;
		m_iFlags |= HUD_ACTIVE;
	}
	return 1;
}

void CHudMessage::MessageScanStart()
{
	client_textmessage_t* message = m_parms.pMessage;
	switch ( message->effect )
	{
	case 0:
	case 1:
		m_parms.fadeTime = message->fadein + message->holdtime + m_parms.holdExtra;
		if ( m_parms.time < message->fadein )
			m_parms.fadeBlend = (int)( ( message->fadein - m_parms.time ) * ( 1.0 / message->fadein ) * 255 );
		else if ( m_parms.time <= m_parms.fadeTime )
			m_parms.fadeBlend = 0;
		else if ( message->fadeout > 0 )
		{
			float blend = ( m_parms.time - m_parms.fadeTime ) / message->fadeout * 255;
			m_parms.fadeBlend = (int)blend;
		}
		else
			m_parms.fadeBlend = 255;	// Pure source (on)
		m_parms.charTime = 0;
		if ( message->effect == 1 && rand() % 100 < 10 )
			m_parms.charTime = 1;
		break;
	case 2:
		m_parms.fadeTime = message->fadein * m_parms.length + message->holdtime + m_parms.holdExtra;
		if ( m_parms.time > m_parms.fadeTime && message->fadeout > 0 )
		{
			float blend = ( m_parms.time - m_parms.fadeTime ) / message->fadeout * 255;
			m_parms.fadeBlend = (int)blend;
		}
		else
			m_parms.fadeBlend = 0;
		break;
	}
}

void CHudMessage::MessageScanNextChar( unsigned int font )
{
	client_textmessage_t* message = m_parms.pMessage;
	int red = message->r1;
	int green = message->g1;
	int blue = message->b1;
	int destRed = 0;
	int destGreen = 0;
	int destBlue = 0;
	int blend = 0;
	switch ( message->effect )
	{
	// Fade-in / Fade-out
	case 0:
	case 1:
		blend = m_parms.fadeBlend;
		break;
	case 2:
		m_parms.charTime += message->fadein;
		if ( m_parms.charTime > m_parms.time )
			red = green = blue = 0;
		else if ( m_parms.time > m_parms.fadeTime )
			blend = m_parms.fadeBlend;
		else
		{
			float elapsed = m_parms.time - m_parms.charTime;
			if ( elapsed <= message->fxtime )
			{
				destRed = message->r2;
				destGreen = message->g2;
				destBlue = message->b2;
				blend = (int)( 255.0 - ( 1.0 / message->fxtime * elapsed * 255.0 + 0.5 ) );
			}
		}
		break;
	}
	if ( blend > 255 )
		blend = 255;
	else if ( blend < 0 )
		blend = 0;
	m_parms.r = ( red * ( 255 - blend ) + destRed * blend ) >> 8;
	m_parms.g = ( green * ( 255 - blend ) + destGreen * blend ) >> 8;
	m_parms.b = ( blue * ( 255 - blend ) + destBlue * blend ) >> 8;
	if ( message->effect == 1 && m_parms.charTime != 0 && m_parms.x >= 0 && m_parms.y >= 0 )
		gEngfuncs.pfnVGUI2DrawCharacterAdd( m_parms.x, m_parms.y, m_parms.text,
			message->r2, message->g2, message->b2, font );
}

void CHudMessage::MessageDrawScan( client_message_t* clientMessage, float time, unsigned int font )
{
	client_textmessage_t* message = clientMessage->pMessage;
	if ( !message->pMessage || !*message->pMessage )
		return;
	wchar_t textBuffer[1024];
	wchar_t line[256];
	char token[256];
	int length = strlen( message->pMessage );
	wchar_t* text;
	if ( *message->pMessage == '#' )
	{
		strncpy( token, message->pMessage, sizeof( token ) - 1 );
		token[sizeof( token ) - 1] = 0;
		if ( ( message->pMessage[length - 1] == '\r' || message->pMessage[length - 1] == '\n' )
			&& (unsigned int)( length - 1 ) < sizeof( token ) - 1 )
			token[length - 1] = 0;
		text = vgui2::localize()->Find( token );
		if ( clientMessage->numArgs )
		{
			vgui2::localize()->ConstructString( textBuffer, sizeof( textBuffer ), text, clientMessage->numArgs,
				clientMessage->args[0], clientMessage->args[1], clientMessage->args[2], clientMessage->args[3] );
			text = textBuffer;
		}
		else if ( !text )
		{
			gEngfuncs.Con_DPrintf( "ERROR: Missing %s from the czero_english.txt file!\n", message->pMessage );
			return;
		}
	}
	else
	{
		vgui2::localize()->ConvertANSIToUnicode( message->pMessage, textBuffer, sizeof( textBuffer ) );
		text = textBuffer;
	}
	m_parms.lines = 1;
	m_parms.time = time;
	m_parms.pMessage = message;
	m_parms.totalWidth = 0;
	int width = 0;
	length = 0;
	int a, b, c;

	// each letter's width is looked up once here and reused below (it used to be looked up three times)
	int advance[1024];
	const int maxAdvance = ARRAYSIZE( advance );
	auto charWidth = [&]( int index, wchar_t ch )
	{
		if ( index < maxAdvance )
			return advance[index];
		int a1, b1, c1;
		vgui2::surface()->GetCharABCwide( font, ch, a1, b1, c1 );
		return a1 + b1 + c1;
	};
	for ( const wchar_t* scan = text; *scan; scan++, length++ )
	{
		if ( *scan == '\n' )
		{
			m_parms.lines++;
			if ( width > m_parms.totalWidth )
				m_parms.totalWidth = width;
			width = 0;
		}
		else
		{
			vgui2::surface()->GetCharABCwide( font, *scan, a, b, c );
			if ( length < maxAdvance )
				advance[length] = a + b + c;
			width += a + b + c;
		}
	}
	m_parms.length = length;
	const int fontTall = vgui2::surface()->GetFontTall( font );
	m_parms.totalHeight = fontTall * m_parms.lines;
	m_parms.y = YPosition( message->y, m_parms.totalHeight );
	m_parms.charTime = 0;
	MessageScanStart();
	int lineAdvance[ARRAYSIZE( line )];
	int position = 0;	// index into text, matching the first pass
	for ( int i = 0; i < m_parms.lines; i++ )
	{
		m_parms.lineLength = 0;
		m_parms.width = 0;
		while ( *text && *text != '\n' && m_parms.lineLength < ARRAYSIZE( line ) - 1 )
		{
			const int w = charWidth( position++, *text );
			lineAdvance[m_parms.lineLength] = w;
			line[m_parms.lineLength++] = *text++;
			m_parms.width += w;
		}
		text++;
		position++;
		line[m_parms.lineLength] = 0;
		m_parms.x = XPosition( message->x, m_parms.width, m_parms.totalWidth );
		for ( int j = 0; j < m_parms.lineLength; j++ )
		{
			m_parms.text = line[j];
			int next = m_parms.x + lineAdvance[j];
			MessageScanNextChar( font );
			// letters are added onto the screen, so a black one (not typed out yet, or fully faded) changes nothing:
			// skip its draw call
			if ( ( m_parms.r | m_parms.g | m_parms.b ) && m_parms.x >= 0 && m_parms.y >= 0 && next <= ScreenWidth )
				gEngfuncs.pfnVGUI2DrawCharacterAdd( m_parms.x, m_parms.y, m_parms.text,
					m_parms.r, m_parms.g, m_parms.b, font );
			m_parms.x = next;
		}
		m_parms.y += fontTall;
	}
}

int CHudMessage::Draw( float time )
{
	int drawn = 0;
	if ( m_gameTitleTime > 0 )
	{
		float elapsed = gHUD.m_flTime - m_gameTitleTime;

		// Maybe timer isn't set yet
		if ( m_gameTitleTime > gHUD.m_flTime )
			m_gameTitleTime = gHUD.m_flTime;
		if ( elapsed > m_pGameTitle->fadein + m_pGameTitle->holdtime + m_pGameTitle->fadeout )
			m_gameTitleTime = 0;
		else
		{
			float brightness = FadeBlend( m_pGameTitle->fadein, m_pGameTitle->fadeout, m_pGameTitle->holdtime, elapsed );
			wrect_t& half = gHUD.GetSpriteRect( m_HUD_title_half );
			wrect_t& life = gHUD.GetSpriteRect( m_HUD_title_life );
			int halfWidth = half.right - half.left;
			int width = halfWidth + life.right - life.left;
			int height = half.bottom - half.top;
			int x = XPosition( m_pGameTitle->x, width, width );
			int y = YPosition( m_pGameTitle->y, height );
			SPR_Set( gHUD.GetSprite( m_HUD_title_half ), (int)( brightness * m_pGameTitle->r1 ),
				(int)( brightness * m_pGameTitle->g1 ), (int)( brightness * m_pGameTitle->b1 ) );
			SPR_DrawAdditive( 0, x, y, &half );
			SPR_Set( gHUD.GetSprite( m_HUD_title_life ), (int)( brightness * m_pGameTitle->r1 ),
				(int)( brightness * m_pGameTitle->g1 ), (int)( brightness * m_pGameTitle->b1 ) );
			SPR_DrawAdditive( 0, x + halfWidth, y, &life );
			drawn = 1;
		}
	}
	// Fixup level transitions
	for ( int i = 0; i < 16; i++ )
	{
		if ( m_startTime[i] > gHUD.m_flTime || m_startTime[i] == 1 )
			m_startTime[i] = gHUD.m_flTime + m_parms.time - m_startTime[i] + 0.2;	// Server takes 0.2 seconds to spawn, adjust for this
	}
	for ( int i = 0; i < 16; i++ )
	{
		if ( !m_pMessages[i].pMessage )
			continue;
		float endTime;
		if ( !MessageEndTime( i, endTime ) )
			continue;
		if ( time <= endTime )
		{
			m_parms.holdExtra = m_holdExtra[i];
			MessageDrawScan( &m_pMessages[i], time - m_startTime[i], m_pMessages[i].font );
			drawn++;
		}
		else
		{
			m_pMessages[i].pMessage = NULL;
			m_pMessages[i].font = 0;
			m_pMessages[i].numArgs = 0;
			m_holdExtra[i] = 0;
		}
	}

	// Remember the time -- to fix up level transitions
	m_parms.time = gHUD.m_flTime;
	// Don't call until we get another message
	if ( !drawn )
		m_iFlags &= ~HUD_ACTIVE;
	return 1;
}
