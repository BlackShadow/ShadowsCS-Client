//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose:
//
//=============================================================================

#include "buy_presets.h"
#include "hud.h"
#include "cl_util.h"
#include "buy_preset_debug.h"
#include "game_controls/iviewport.h"
#include "vgui/KeyValues.h"
#include "vgui_controls/Controls.h"
#include "parsemsg.h"
#include "shared_util.h"
#include <stdlib.h>
#include <string.h>

BuyPresetManager *TheBuyPresets;

cvar_t *cl_preset_debug;

//--------------------------------------------------------------------------------------------------------------
int __MsgFunc_ItemStatus( const char *, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	int items = READ_BYTE();
	TheBuyPresets->SetNightvision( ( items & 1 ) != 0 );
	TheBuyPresets->SetDefuser( ( items & 2 ) != 0 );

	return 0;
}

//--------------------------------------------------------------------------------------------------------------
void __CmdFunc_BuyPresetEdit( void )
{
	if ( gViewPortInterface
		&& gViewPortInterface->GetCurrentMenuID() != 38
		&& gViewPortInterface->GetCurrentMenuID() != 39 )
	{
		ShowBuyPresetMainMenu( true, REOPEN_NO );
	}
}

//--------------------------------------------------------------------------------------------------------------
void __CmdFunc_BuyPreset( void )
{
	if ( gEngfuncs.Cmd_Argc() != 2 )
	{
		if ( IsPresetDebuggingEnabled() ) gEngfuncs.Con_DPrintf( "buy_preset: no preset specified\n" );
		if ( TheBuyPresets->GetNumPresets() )
			gEngfuncs.Con_Printf( "usage:  buy_preset <1...%d>\n", TheBuyPresets->GetNumPresets() );
		else
			gEngfuncs.Con_Printf( "buy_preset: no presets are defined\n" );
		PlaySound( "common/wpn_select.wav", 1 );
		return;
	}

	int presetIndex = atoi( gEngfuncs.Cmd_Argv( 1 ) ) - 1;
	if ( presetIndex < 0 || presetIndex >= TheBuyPresets->GetNumPresets() )
	{
		if ( IsPresetDebuggingEnabled() ) gEngfuncs.Con_DPrintf( "buy_preset: preset %d doesn't exist\n", presetIndex );
		if ( TheBuyPresets->GetNumPresets() )
			gEngfuncs.Con_Printf( "usage:  buy_preset <1...%d>\n", TheBuyPresets->GetNumPresets() );
		else
			gEngfuncs.Con_Printf( "buy_preset: no presets are defined\n" );
		PlaySound( "common/wpn_select.wav", 1 );
		return;
	}

	TheBuyPresets->PurchasePreset( presetIndex, false );
}

//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
/**
 *  Creates the BuyPresetManager singleton
 */
BuyPresetManager::BuyPresetManager( void )
{
	m_hasNightvision = false;
	m_hasDefuser = false;

	m_rebuyString = NULL;

	Reset();

	gEngfuncs.pfnHookUserMsg( "ItemStatus", __MsgFunc_ItemStatus );

	if ( gHUD.IsGame( "czero" ) )
	{
		gEngfuncs.pfnAddCommand( "buy_preset", __CmdFunc_BuyPreset );
		gEngfuncs.pfnAddCommand( "buy_preset_edit", __CmdFunc_BuyPresetEdit );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Resets the BuyPresetManager, loading BuyPresets from disk.  If no presets are defined, it loads the
 *  default presets instead.
 */
void BuyPresetManager::Reset( void )
{
	m_presets.clear();
	if ( m_rebuyString )
	{
		delete[] m_rebuyString;
		m_rebuyString = NULL;
	}

	KeyValues *data;
	KeyValues *presetKey;
	m_presets.clear();
	data = new KeyValues( "Presets" );
	data->LoadFromFile( vgui2::filesystem(), "BuyPresets.vdf", NULL );

	presetKey = data->GetFirstSubKey();
	while ( presetKey )
	{
		BuyPreset preset;
		preset.Parse( presetKey );
		m_presets.push_back( preset );

		presetKey = presetKey->GetNextKey();
	}

	if ( !m_presets.size() )
	{
		ResetEditToDefaults();
		SetPresets( GetEditPresets() );
	}

	// Guarantee we have at least this many presets, even if they are blank
	while ( m_presets.size() < NUM_PRESETS )
	{
		BuyPreset preset;
		m_presets.push_back( preset );
	}

	data->deleteThis();
	m_editPresets = m_presets;
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Loads the default presets into the editing presets (e.g. when hitting the "Use Defaults" button)
 */
void BuyPresetManager::ResetEditToDefaults( void )
{
	m_editPresets.clear();
	KeyValues *data;
	KeyValues *presetKey;
	data = new KeyValues( "Presets" );
	data->LoadFromFile( vgui2::filesystem(), "BuyPresetsDefault.vdf", NULL );

	presetKey = data->GetFirstSubKey();
	while ( presetKey )
	{
		BuyPreset preset;
		preset.Parse( presetKey );
		m_editPresets.push_back( preset );

		presetKey = presetKey->GetNextKey();
	}

	data->deleteThis();
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Saves the current BuyPresets to disk
 */
void BuyPresetManager::Save( void )
{
	KeyValues *data;

	data = new KeyValues( "Presets" );

	data = new KeyValues( "Presets" );
	for ( BuyPresetListIt it = m_presets.begin(); it != m_presets.end(); ++it )
	{
		it->Save( data );
	}
	data->SaveToFile( vgui2::filesystem(), "BuyPresets.vdf", NULL );
	data->deleteThis();
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Returns the specified "live" preset, or NULL if it doesn't exist
 */
const BuyPreset *BuyPresetManager::GetPreset( int index ) const
{
	if ( index < 0 || (unsigned int)index >= m_presets.size() )
	{
		return NULL;
	}

	return &( m_presets[index] );
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Returns the specified editing preset, or NULL if it doesn't exist
 */
BuyPreset *BuyPresetManager::GetEditPreset( int index )
{
	if ( index < 0 || (unsigned int)index >= m_editPresets.size() )
	{
		return NULL;
	}

	return &( m_editPresets[index] );
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Generates and sends buy commands to buy a specific preset
 */
void BuyPresetManager::PurchasePreset( int presetIndex, bool withCareer )
{
	if ( presetIndex >= 0 && (unsigned int)presetIndex < m_presets.size() )
	{
		const BuyPreset *preset = &( m_presets[presetIndex] );

		int setIndex;
		for ( setIndex = 0; setIndex < preset->GetNumSets(); ++setIndex )
		{
			// Try to buy this weapon set.
			const WeaponSet *itemSet = preset->GetSet( setIndex );
			if ( itemSet )
			{
				int currentCost;
				WeaponSet currentSet;
				itemSet->GetCurrent( currentCost, currentSet, withCareer );
				if ( currentCost > 0 )
				{
					if ( IsPresetDebuggingEnabled() ) gEngfuncs.Con_DPrintf( "buy_preset: buying %ls for a total of $%d.\n",
						preset->GetName(),
						currentCost );
					char buf[BUY_PRESET_COMMAND_LEN];
					currentSet.GenerateBuyCommands( buf );

					// Send completed string
					if ( IsPresetDebuggingEnabled() ) gEngfuncs.Con_DPrintf( "%s\n", buf );
					gEngfuncs.pfnClientCmd( buf );
					return;
				}
				else if ( currentCost == 0 )
				{
					// We have everything already.  Let the player know.
					if ( setIndex == 0 )
					{
						if ( IsPresetDebuggingEnabled() ) gEngfuncs.Con_DPrintf( "buy_preset: already have a complete %ls set.\n", preset->GetName() );
					}
					else
					{
						if ( IsPresetDebuggingEnabled() ) gEngfuncs.Con_DPrintf( "buy_preset: can't afford anything better from %ls.\n", preset->GetName() );
					}
					PlaySound( "common/wpn_moveselect.wav", 1 );
					return;
				}
			}
		}
		if ( IsPresetDebuggingEnabled() ) gEngfuncs.Con_DPrintf( "buy_preset: can't afford anything better from %ls.\n", preset->GetName() );
		PlaySound( "common/wpn_moveselect.wav", 1 );
		return;
	}

	// We failed to buy anything.  Let the player know.
	if ( IsPresetDebuggingEnabled() ) gEngfuncs.Con_DPrintf( "buy_preset: preset %d doesn't exist.\n", presetIndex );
	PlaySound( "common/wpn_select.wav", 1 );
}

//--------------------------------------------------------------------------------------------------------------
const char *BuyPresetManager::GetRebuyString( void )
{
	if ( m_rebuyString )
		return m_rebuyString;

	const int BufLen = 1024;

	char *fileBuffer = (char *)gEngfuncs.COM_LoadFile( "rebuy.txt", 5, NULL );
	char token[BufLen];
	if ( !fileBuffer )
	{
		return "";
	}

	char *remainder = fileBuffer;

	char rebuystring[BufLen] = { 0 };
	remainder = gEngfuncs.COM_ParseFile( remainder, token );

	bool first = true;

	while ( remainder )
	{
		if ( first )
		{
			first = false;
		}
		else
		{
			strncat( rebuystring, " ", BufLen );
		}

		strncat( rebuystring, token, BufLen );

		remainder = gEngfuncs.COM_ParseFile( remainder, token );
	}

	gEngfuncs.COM_FreeFile( fileBuffer );

	m_rebuyString = CloneString( rebuystring );

	return m_rebuyString;
}

//--------------------------------------------------------------------------------------------------------------
int BuyPresetManager::GetAutobuyPresetIndex( void ) const
{
	return -1;
}

//--------------------------------------------------------------------------------------------------------------
