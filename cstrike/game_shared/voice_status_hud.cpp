//========= Copyright 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

// There are hud.h's coming out of the woodwork so this ensures that we get the right one.
#include "voice_status_hud.h"
#include "vgui_controls/Controls.h"
#include "game_controls/iviewport.h"
#include "vgui/ILocalize.h"
#include "vgui/IImage.h"
#include "vgui/IScheme.h"
#include "vgui/IPanel.h"
#include "vgui/ISurface.h"
#include "entity_types.h"
#include "shared_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

extern int cam_thirdperson;



// ---------------------------------------------------------------------- //
// The voice manager for the client.
// ---------------------------------------------------------------------- //
CVoiceStatusHud g_VoiceStatusHud;

IVoiceHud* GetClientVoiceHud()
{
	return &g_VoiceStatusHud;
}

// The panel the speaker labels and the talk icon hang under: the one the game's menus and scoreboard hang under (the
// client's top panel), shown the whole time the game is. Not GetViewPortPanel() itself, as in the original code: that
// is the dark background of the team / buy menus and is hidden whenever none of them is open, so the labels and the
// icon only showed while a menu was open.
static vgui2::VPANEL VoicePanelParent()
{
	const vgui2::VPANEL background = gViewPortInterface ? gViewPortInterface->GetViewPortPanel() : 0;
	const vgui2::VPANEL top = background ? vgui2::ipanel()->GetParent( background ) : 0;
	return top ? top : background;
}

// The labels and the talk icon grow with the screen like the HUD: their pictures (16 and 32 pixels) and the stock
// font are made for 640 x 480 and were tiny on big screens.
static float VoiceScale()
{
	const float scale = gHUD.m_scrinfo.iHeight / 480.0f * 0.8f;
	return scale > 1.0f ? scale : 1.0f;
}

static int VoiceScaled( int value )
{
	return (int)( value * VoiceScale() + 0.5f );
}

// the speaker's name: a bold font sized for the screen (made again when the resolution changes; 0 = the stock one)
static vgui2::HFont VoiceFont()
{
	static vgui2::HFont font = 0;
	static int madeFor = 0;
	if ( madeFor == gHUD.m_scrinfo.iHeight )
		return font;
	madeFor = gHUD.m_scrinfo.iHeight;
	const vgui2::HFont made = vgui2::surface()->CreateFont();
	if ( made && vgui2::surface()->AddGlyphSetToFont( made, "Verdana", VoiceScaled( 13 ), 700, 0, 0,
		vgui2::ISurface::FONTFLAG_ANTIALIAS, 0, 0 ) )
		font = made;
	return font;
}

CVoiceLabel::CVoiceLabel()
{
	m_pLabel = new VoiceVGUILabel( NULL, "VoiceLabel", "" );
	m_pLabel->SetParent( VoicePanelParent() );
	m_pLabel->SetProportional( true );
	m_pLabel->SetScheme( "ClientScheme" );
	m_pLabel->MakeReadyForUse();
	m_clientindex = -1;
	m_locationString = NULL;
	m_playerName = NULL;
}

CVoiceLabel::~CVoiceLabel()
{
	m_pLabel->MarkForDeletion();
}

void CVoiceLabel::GetContentSize( int& width, int& height )
{
	m_pLabel->GetContentSize( width, height );
	const int minimum = VoiceScaled( 32 );
	if ( height < minimum )
		height = minimum;
}

void CVoiceLabel::SetBounds( int x, int y )
{
	m_pLabel->SetPos( x, y );
	int width, height;
	m_pLabel->GetContentSize( width, height );
	m_pLabel->SetSize( width, height );
}

void CVoiceLabel::Rescale()
{
	m_pLabel->ApplyScale();
}

void CVoiceLabel::VoiceVGUILabel::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	Label::ApplySchemeSettings( scheme );
	SetTextImageIndex( 1 );
	ApplyScale();
}

// the speaker picture (filtered, so it stays smooth when grown), the gaps and the name's font for this screen
void CVoiceLabel::VoiceVGUILabel::ApplyScale()
{
	vgui2::IImage* speaker = vgui2::scheme()->GetImage( "gfx/vgui/speaker4", true );
	if ( speaker )
	{
		const int side = VoiceScaled( 16 );
		speaker->SetSize( side, side );
	}
	SetImageAtIndex( 0, speaker, VoiceScaled( 1 ) );
	SetImagePreOffset( 1, VoiceScaled( 2 ) );
	const vgui2::HFont font = VoiceFont();
	if ( font )
		SetFont( font );
	InvalidateLayout( false, false );
}



// ---------------------------------------------------------------------- //
// CVoiceLabel.
// ---------------------------------------------------------------------- //
void CVoiceLabel::SetLocation( const char* location )
{
	if ( !location || !*location )
	{
		if ( m_locationString )
		{
			delete[] m_locationString;
			m_locationString = NULL;
			RebuildLabelText();
		}
		return;
	}
	const wchar_t* localized = vgui2::localize()->Find( location );
	if ( localized )
	{
		// localized version
		if ( m_locationString && wcscmp( localized, m_locationString ) )
		{
			delete[] m_locationString;
			m_locationString = NULL;
		}
		if ( !m_locationString )
		{
			m_locationString = CloneWString( localized );
			RebuildLabelText();
		}
	}
	else
	{
		wchar_t* temporary = new wchar_t[strlen( location ) + 1];
		vgui2::localize()->ConvertANSIToUnicode( location, temporary, sizeof( temporary ) );
		if ( m_locationString && wcscmp( temporary, m_locationString ) )
		{
			delete[] m_locationString;
			m_locationString = NULL;
		}
		if ( !m_locationString )
		{
			m_locationString = CloneWString( temporary );
			RebuildLabelText();
		}
		delete[] temporary;
	}
}

void CVoiceLabel::SetPlayerName( const char* name )
{
	if ( m_playerName )
	{
		delete[] m_playerName;
		m_playerName = NULL;
	}
	if ( name )
		m_playerName = CloneString( name );
	RebuildLabelText();
}

void CVoiceLabel::RebuildLabelText()
{
	wchar_t text[512] = L"";
	if ( m_playerName )
	{
		wchar_t playerName[512] = L"";
		vgui2::localize()->ConvertANSIToUnicode( m_playerName, playerName, sizeof( playerName ) );
		const wchar_t* format = L"%ls   ";
		if ( m_locationString )
		{
			format = vgui2::localize()->Find( "#Voice_Location" );
			if ( !format )
				format = L"%ls/%ls   ";
		}
#ifdef _WIN32
		_snwprintf( text, 512, format, playerName, m_locationString );
#else
		swprintf( text, 512, format, playerName, m_locationString );
#endif
	}
	m_pLabel->SetText( text );
}

#ifdef _MSC_VER



// ---------------------------------------------------------------------- //
// CVoiceStatus.
// ---------------------------------------------------------------------- //

#pragma warning( suppress : 26495 )
#endif
CVoiceStatusHud::CVoiceStatusHud()
{
}

CVoiceStatusHud::~CVoiceStatusHud()
{
}

int CVoiceStatusHud::Init( IVoiceStatusHelper* helper, IVoiceStatus* status )
{
	m_VoiceHeadModel = 0;
	m_pHelper = helper;
	m_pStatus = status;
	gHUD.AddHudElem( this );
	m_iFlags = HUD_ACTIVE;
	m_pLocalPlayerTalkIcon = new vgui2::ImagePanel( NULL, "LocalPlayerIcon" );
	m_pLocalPlayerTalkIcon->SetParent( VoicePanelParent() );
	m_pLocalPlayerTalkIcon->SetVisible( false );
	// scaled to the panel's size (set for the screen when shown), filtered so it stays smooth
	m_pLocalPlayerTalkIcon->SetImage( vgui2::scheme()->GetImage( "gfx/vgui/icntlk_pl", true ), true );
	return 1;
}

int CVoiceStatusHud::VidInit()
{
	// Figure out the voice head model height.
	m_VoiceHeadModelHeight = 45;
	char* file = (char*)gEngfuncs.COM_LoadFile( "scripts/voicemodel.txt", 5, NULL );
	if ( file )
	{
		char token[4096];
		gEngfuncs.COM_ParseFile( file, token );
		if ( token[0] >= '0' && token[0] <= '9' )
			m_VoiceHeadModelHeight = (float)atof( token );
		gEngfuncs.COM_FreeFile( file );
	}
	m_VoiceHeadModel = gEngfuncs.pfnSPR_Load( "sprites/voiceicon.spr" );
	// the resolution may have changed: labels sized for the screen again
	for ( int index = 0; index < m_Labels.Count(); ++index )
		m_Labels[index]->Rescale();
	RepositionLabels();
	return 1;
}

void CVoiceStatusHud::CreateEntities()
{
	if ( !m_VoiceHeadModel )
		return;
	cl_entity_s* localPlayer = gEngfuncs.GetLocalPlayer();
	int modelIndex = 0;
	for ( int player = 0; player < VOICE_MAX_PLAYERS; ++player )
	{
		if ( m_pStatus->GetSpeakerStatus( player ) != CVoiceStatus::VOICE_TALKING )
			continue;
		cl_entity_s* client = gEngfuncs.GetEntityByIndex( player + 1 );
		if ( !client || client->curstate.messagenum < localPlayer->curstate.messagenum )
			continue;

		// Don't show an icon for dead or spectating players (ie: invisible entities).
		if ( client->curstate.effects & EF_NODRAW )
			continue;

		// Don't show an icon for the local player unless we're in thirdperson mode.
		if ( client == localPlayer && !cam_thirdperson )
			continue;
		cl_entity_s* entity = &m_VoiceHeadModels[modelIndex++];
		memset( entity, 0, sizeof( *entity ) );
		entity->curstate.rendermode = kRenderTransAdd;
		entity->curstate.renderamt = 255;
		entity->baseline.renderamt = 255;
		entity->curstate.renderfx = kRenderFxNoDissipation;
		entity->curstate.framerate = 1;
		entity->curstate.frame = 0;
		entity->model = (model_s*)gEngfuncs.GetSpritePointer( m_VoiceHeadModel );
		entity->angles[0] = entity->angles[1] = entity->angles[2] = 0;
		entity->curstate.scale = 0.5f;
		entity->origin[0] = 0;
		entity->origin[1] = 0;
		entity->origin[2] = 45;
		entity->origin[0] += client->origin[0];
		entity->origin[1] += client->origin[1];
		entity->origin[2] += client->origin[2];
		gEngfuncs.CL_CreateVisibleEntity( ET_NORMAL, entity );
	}
}

void CVoiceStatusHud::UpdateLocation( int player, const char* location )
{
	if ( player - 1 < 0 )
		return;
	CVoiceLabel* label = FindVoiceLabel( player - 1 );
	if ( !label )
		return;
	label->SetLocation( location );
	RepositionLabels();
}

CVoiceLabel* CVoiceStatusHud::FindVoiceLabel( int player )
{
	for ( int index = 0; index < m_Labels.Count(); ++index )
	{
		if ( m_Labels[index]->GetClientIndex() == player )
			return m_Labels[index];
	}
	return NULL;
}

CVoiceLabel* CVoiceStatusHud::GetFreeVoiceLabel()
{
	CVoiceLabel* label = FindVoiceLabel( -1 );
	if ( !label )
	{
		label = new CVoiceLabel;
		m_Labels.AddToTail( label );
	}
	return label;
}

void CVoiceStatusHud::RepositionLabels()
{
	int y = gHUD.m_scrinfo.iHeight / 2;
	for ( int index = 0; index < m_Labels.Count(); ++index )
	{
		CVoiceLabel* label = m_Labels[index];
		int width, height;
		label->GetContentSize( width, height );
		label->SetBounds( gHUD.m_scrinfo.iWidth - width - VoiceScaled( 8 ), y );
		y += height + VoiceScaled( 2 );
	}
}

void CVoiceStatusHud::UpdateSpeakerStatus( int player, bool talking )
{
	if ( player == -2 )  // this is the local player
	{
		if ( talking )
		{
			// made at start-up, maybe before the menus' panels were hung up: under the right panel now
			const vgui2::VPANEL parent = VoicePanelParent();
			if ( parent && vgui2::ipanel()->GetParent( m_pLocalPlayerTalkIcon->GetVPanel() ) != parent )
				m_pLocalPlayerTalkIcon->SetParent( parent );
			if ( m_pLocalPlayerTalkIcon->GetImage() )
			{
				// the 32 pixel picture grown with the screen (the panel scales its picture to its size)
				const int side = VoiceScaled( 32 );
				m_pLocalPlayerTalkIcon->SetSize( side, side );
				m_pLocalPlayerTalkIcon->SetPos( gHUD.m_scrinfo.iWidth - side - VoiceScaled( 10 ),
					gHUD.m_scrinfo.iHeight - m_pHelper->GetAckIconHeight() - side );
				m_pLocalPlayerTalkIcon->SetVisible( true );
			}
		}
		else
		{
			m_pLocalPlayerTalkIcon->SetVisible( false );
		}
	}
	else if ( player >= 0 && player <= 64 )
	{
		if ( player == 0 )
			return;
		CVoiceLabel* label = FindVoiceLabel( player - 1 );
		if ( talking )
		{
			// If we don't have a label for this guy yet, then create one.
			if ( !label )
			{
				label = GetFreeVoiceLabel();
				if ( label )
				{
					// Get the name from the engine.
					hud_player_info_t info;
					memset( &info, 0, sizeof( info ) );
					gEngfuncs.pfnGetPlayerInfo( player, &info );
					int color[3];
					m_pHelper->GetPlayerTextColor( player, color );
					label->SetFgColor( Color( 255, 255, 255, 255 ) );
					label->SetBgColor( Color( color[0], color[1], color[2], 180 ) );
					label->SetPlayerName( info.name );
					label->SetLocation( m_pHelper->GetPlayerLocation( player ) );
					label->SetClientIndex( player - 1 );
					if ( !m_pHelper || m_pHelper->CanShowSpeakerLabels() )
						label->SetVisible( true );
				}
			}
		}
		else if ( label )
		{
			label->SetVisible( false );
			label->SetClientIndex( -1 );
		}
	}
	RepositionLabels();
}
