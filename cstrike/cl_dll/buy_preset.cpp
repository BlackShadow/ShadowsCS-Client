//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose:
//
//=============================================================================

#include "buy_presets.h"
#include "hud.h"
#include "ammohistory.h"
#include "net_api.h"
#include "buy_preset_debug.h"
#include "VGUI/GameUI_Interfaces.h"
#include "GameUI/ICareerUI.h"
#include "vgui/ILocalize.h"
#include "vgui/KeyValues.h"
#include "vgui_controls/Controls.h"
#include "shared_util.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

//--------------------------------------------------------------------------------------------------------------
static bool CareerIsFlashbangTask( void )
{
	if ( !g_pCareerUI->IsPlayingMatch() )
		return false;

	ITaskVec *tv = g_pCareerUI->GetCurrentTaskVec();
	if ( !tv )
		return false;

	for ( ITaskVecIt it = tv->begin(); it != tv->end(); ++it )
	{
		ICareerTask *task = *it;
		if ( task && !task->IsComplete() && !stricmp( task->GetTaskName(), "killblind" ) )
			return true;
	}

	return false;
}


//--------------------------------------------------------------------------------------------------------------
static bool CareerIsHEGrenadeTask( void )
{
	if ( !g_pCareerUI->IsPlayingMatch() )
		return false;

	ITaskVec *tv = g_pCareerUI->GetCurrentTaskVec();
	if ( !tv )
		return false;

	for ( ITaskVecIt it = tv->begin(); it != tv->end(); ++it )
	{
		ICareerTask *task = *it;
		if ( task && !task->IsComplete() && ( !stricmp( task->GetWeaponName(), "grenade" ) || !stricmp( task->GetWeaponName(), "hegren" ) ) )
			return true;
	}

	return false;
}


//--------------------------------------------------------------------------------------------------------------
static bool CanBuyDefuser( void )
{
	return IsDEMapType();
}

//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
WeaponSet::WeaponSet( void )
{
	m_isRebuy = false;
	Reset();
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Sets reasonable defaults for an empty weapon set: no primary/secondary weapons, no equipment, and
 *  everything optional.
 */
void WeaponSet::Reset( void )
{
	BuyPresetWeapon blank;
	m_primaryWeapon = blank;
	m_secondaryWeapon = blank;

	m_armor = 0;
	m_helmet = false;
	m_smokeGrenade = false;
	m_HEGrenade = false;
	m_flashbangs = 0;
	m_defuser = false;
	m_nightvision = false;
	m_extraCash = 0;

	m_isSecondaryOptional = false;
	m_isArmorOptional = false;
	m_isSmokeOptional = false;
	m_isHEGrenadeOptional = false;
	m_isFlashbangOptional = false;
	m_isDefuserOptional = false;
	m_isNightvisionOptional = false;
	m_isCashOptional = true;
}


#pragma warning( push )
#pragma warning( disable : 4456 )
//--------------------------------------------------------------------------------------------------------------
/**
 *  Constructs a WeaponSet containing everything that could be purchased right now.  The cost is also
 *  returned, and is -1 if the set is not purchasable (as opposed to completely purchased already, in
 *  which case cost is 0.)
 */
void WeaponSet::GetCurrent( int &cost, WeaponSet &ws, bool withCareer ) const
{
	cost = 0;
	ws.Reset();

	net_status_t status;
	gEngfuncs.pNetAPI->Status( &status );
	if ( status.connected )
	{
		cl_entity_t *player = gEngfuncs.GetLocalPlayer();
		if ( !player->player || ( g_iTeamNumber != 1 && g_iTeamNumber != 2 ) )
		{
			cost = -1;
			return;
		}
	}
	else
	{
		cost = -1;
		return;
	}

	if ( g_PlayerExtraInfo[gEngfuncs.GetLocalPlayer()->index].vip )
		return; // can't buy anything as the VIP

	//-------------------------------------------------------------------------
	// Armor
	if ( !m_isArmorOptional && m_armor > gHUD.m_Battery.m_iBat )
	{
		cost += KEVLAR_PRICE;
		ws.m_armor = 100;
	}

	if ( !m_isArmorOptional && m_helmet && m_armor > 0 && !gHUD.m_Battery.m_iArmorType )
	{
		cost += HELMET_PRICE;
		ws.m_helmet = true;
	}

	WEAPON *pWeapon;

	pWeapon = gWR.GetWeapon( ID_SMOKEGRENADE );
	bool hasSmokeGrenade = ( !pWeapon || gWR.HasAmmo( pWeapon ) );
	if ( pWeapon && !m_isSmokeOptional && m_smokeGrenade && !gWR.HasAmmo( pWeapon ) )
	{
		cost += SMOKEGRENADE_PRICE;
		ws.m_smokeGrenade = true;
		hasSmokeGrenade = true;
	}

	pWeapon = gWR.GetWeapon( ID_HEGRENADE );
	bool hasHEGrenade = ( !pWeapon || gWR.HasAmmo( pWeapon ) );
	if ( !m_isHEGrenadeOptional && m_HEGrenade && !hasHEGrenade )
	{
		cost += HEGRENADE_PRICE;
		ws.m_HEGrenade = true;
		hasHEGrenade = true;
	}

	pWeapon = gWR.GetWeapon( ID_FLASHBANG );
	int numFlashbangs = ( pWeapon ) ? gWR.CountAmmo( pWeapon->iAmmoType ) : 0;
	if ( pWeapon && !m_isFlashbangOptional && numFlashbangs < m_flashbangs && m_flashbangs )
	{
		int count = m_flashbangs - numFlashbangs;
		cost += FLASHBANG_PRICE * count;
		ws.m_flashbangs = count;
		numFlashbangs += count;
	}

	if ( !m_isDefuserOptional && m_defuser && g_iTeamNumber == 2 && !TheBuyPresets->HasDefuser() && CanBuyDefuser() )
	{
		cost += DEFUSEKIT_PRICE;
		ws.m_defuser = true;
	}

	if ( !m_isNightvisionOptional && m_nightvision && !TheBuyPresets->HasNightvision() )
	{
		cost += NVG_PRICE;
		ws.m_nightvision = true;
	}

	if ( !m_isCashOptional && m_extraCash )
	{
		cost += m_extraCash;
		ws.m_extraCash = m_extraCash;
	}

	if ( withCareer && !m_isRebuy && CareerIsFlashbangTask() && numFlashbangs < 2 )
	{
		int count = 2 - numFlashbangs;
		cost += FLASHBANG_PRICE * count;
		ws.m_flashbangs += count;
		numFlashbangs += count;
	}

	if ( withCareer && !m_isRebuy && CareerIsHEGrenadeTask() && !hasHEGrenade )
	{
		cost += HEGRENADE_PRICE;
		ws.m_HEGrenade = true;
		hasHEGrenade = true;
	}

	//-------------------------------------------------------------------------
	// Construct a list of weapons to buy from.
	ClientWeaponIdType primaryClientID = GetClientWeaponID( true );		///< The local player's current primary weapon
	ClientWeaponIdType secondaryClientID = GetClientWeaponID( false );	///< The local player's current pistol

	WeaponIdType primaryID = ClientWeaponIDToGameWeaponID( primaryClientID );
	WeaponIdType secondaryID = ClientWeaponIDToGameWeaponID( secondaryClientID );

	BuyPresetWeaponList primaryWeaponList;
	if ( m_isRebuy && primaryID != WEAPON_NONE && primaryID != m_primaryWeapon.GetWeaponID() )
	{
		BuyPresetWeapon weapon( primaryID );
		weapon.SetAmmoType( AMMO_ROUNDS );
		weapon.SetFillAmmo( false );
		weapon.SetAmmoAmount( m_primaryWeapon.GetAmmoAmount() );
		primaryWeaponList.push_back( weapon );
	}
	primaryWeaponList.push_back( m_primaryWeapon );
	BuyPresetWeaponList primaryWeapons = ( withCareer ) ? CareerWeaponList( primaryWeaponList, true, primaryClientID ) : primaryWeaponList;

	BuyPresetWeaponList secondaryWeaponList;
	secondaryWeaponList.push_back( m_secondaryWeapon );
	BuyPresetWeaponList secondaryWeapons = ( withCareer ) ? CareerWeaponList( secondaryWeaponList, false, secondaryClientID ) : secondaryWeaponList;

	/**
	 *  Determine best weapons & ammo to purchase
	 *
	 *  For each primary weapon in the list, do the following until one satisfies the current money situation:
	 *    1. buy primary weapon and its ammo (NEED TO WATCH FOR WEAPONS PLAYER CANNOT BUY!!!)
	 *    2. for each non-optional secondary weapon (if any),
	 *         try to buy it and its ammo
	 *    3. when a set can be purchased, add it to the total cost and stop processing
	 *  If we can't purchase the primary weapon, or a required secondary weapon, the WeaponSet can't be bought,
	 *  so we return -1.
	 *
	 *  To find if a weapon is owned, we can compare it's weaponID to either primaryID or secondaryID from above.
	 */
	int currentCost = cost;
	int ammo[AMMO_MAX_TYPES];
	memset( ammo, 0, sizeof( ammo ) );
	FillClientAmmo( ammo );
	const BuyPresetWeapon *primaryWeaponToBuy = NULL;
	const BuyPresetWeapon *secondaryWeaponToBuy = NULL;
	int primaryClipsToBuy = 0;
	int secondaryClipsToBuy = 0;
	int currentNonWeaponCost = currentCost;
	bool doneBuyingWeapons = false;

	int currentCash = gHUD.m_accountBalance.m_iAccount;

	if ( m_isRebuy )
		doneBuyingWeapons = true;

	//-------------------------------------------------------------------------
	// Try to buy the primary weapon
	for ( BuyPresetWeaponList::const_iterator primaryWeapon = primaryWeapons.begin(); primaryWeapon != primaryWeapons.end() && !doneBuyingWeapons; ++primaryWeapon )
	{
		primaryWeaponToBuy = NULL;
		int primaryClips = 0;
		primaryClipsToBuy = 0;
		currentCost = currentNonWeaponCost;
		FillClientAmmo( ammo );

		WeaponIdType weaponID = primaryWeapon->GetWeaponID();
		if ( weaponID == WEAPON_NONE )
			weaponID = primaryID;

		const WeaponInfoStruct *primaryInfo = GetWeaponInfo( weaponID );
		if ( primaryInfo )
		{
			primaryClips = CalcClipsNeeded( &(*primaryWeapon), primaryInfo, ammo );
			int primaryWeaponCost = ( weaponID != primaryID ) ? primaryInfo->cost : 0;
			currentCost += primaryWeaponCost;
			currentCost += primaryInfo->clipCost * primaryClips;
			ammo[primaryInfo->ammoType] += primaryClips * primaryInfo->buyClipSize;

			if ( IsPresetCurrentCostDebuggingEnabled() ) gEngfuncs.Con_DPrintf( "\t%5.5d (%s)\n", primaryWeaponCost, WeaponIDToAlias( weaponID ) );
			if ( IsPresetCurrentCostDebuggingEnabled() ) gEngfuncs.Con_DPrintf( "\t%5.5d (ammo)\n", primaryInfo->clipCost * primaryClips );
			if ( currentCash < currentCost )
			{
				currentCost = currentNonWeaponCost;	// can't afford it, so try the next weapon
				continue;
			}

			// check for as_* maps, and CTs buying AK47s, etc
			if ( !CanBuyWeapon( primaryClientID, secondaryClientID, weaponID ) && weaponID != primaryID )
			{
				currentCost = currentNonWeaponCost;	// can't buy it, so try the next weapon
				continue;
			}

			primaryWeaponToBuy = &(*primaryWeapon);
			primaryClipsToBuy = primaryClips;
		}

		if ( secondaryWeapons.empty() )
		{
			if ( IsPresetCurrentCostDebuggingEnabled() ) gEngfuncs.Con_DPrintf( "\t\t\tDone buying weapons (no secondary)\n" );
			break;
		}

		if ( m_isSecondaryOptional )
		{
			if ( IsPresetCurrentCostDebuggingEnabled() ) gEngfuncs.Con_DPrintf( "\t\t\tDone buying required weapons (secondary is optional)\n" );
			break;
		}

		//-------------------------------------------------------------------------
		// Try to buy a (required) secondary weapon to go with primaryWeaponToBuy.
		// If not, reset cost to not reflect either weapon, so we can look at buying
		// the next primary weapon in the list.
		for ( BuyPresetWeaponList::const_iterator secondaryWeapon = secondaryWeapons.begin(); secondaryWeapon != secondaryWeapons.end(); ++secondaryWeapon )
		{
			// reset ammo counts and cost
			secondaryWeaponToBuy = NULL;
			secondaryClipsToBuy = 0;
			currentCost = currentNonWeaponCost;
			FillClientAmmo( ammo );

			// add in ammo we're already buying for the primary weapon to the client's actual ammo
			if ( primaryInfo )
			{
				currentCost += ( weaponID != primaryID ) ? primaryInfo->cost : 0;
				currentCost += primaryInfo->clipCost * primaryClips;
				ammo[primaryInfo->ammoType] += primaryClips * primaryInfo->buyClipSize;
			}

			int currentCostWithoutSecondary = currentCost;

			WeaponIdType weaponID = secondaryWeapon->GetWeaponID();
			if ( weaponID == WEAPON_NONE )
				weaponID = secondaryID;

			const WeaponInfoStruct *secondaryInfo = GetWeaponInfo( weaponID );
			if ( !secondaryInfo )
			{
				currentCost = currentCostWithoutSecondary;
				continue;
			}

			int secondaryClips = CalcClipsNeeded( &(*secondaryWeapon), secondaryInfo, ammo );
			int secondaryWeaponCost = ( weaponID != secondaryID ) ? secondaryInfo->cost : 0;
			currentCost += secondaryWeaponCost;
			currentCost += secondaryInfo->clipCost * secondaryClips;
			ammo[secondaryInfo->ammoType] += secondaryClips * secondaryInfo->buyClipSize;

			if ( IsPresetCurrentCostDebuggingEnabled() ) gEngfuncs.Con_DPrintf( "\t%5.5d (%s)\n", secondaryWeaponCost, WeaponIDToAlias( weaponID ) );
			if ( IsPresetCurrentCostDebuggingEnabled() ) gEngfuncs.Con_DPrintf( "\t%5.5d (ammo)\n", secondaryInfo->clipCost * secondaryClips );
			if ( currentCash < currentCost )
			{
				currentCost = currentCostWithoutSecondary;	// can't afford it, so skip
				continue;
			}

			// check for as_* maps, and CTs buying elites, etc
			if ( !CanBuyWeapon( primaryClientID, secondaryClientID, weaponID ) && weaponID != secondaryID )
			{
				currentCost = currentCostWithoutSecondary;	// can't buy it, so skip
				continue;
			}

			// We found both a primary and secondary weapon to buy.  Stop looking for more.
			secondaryWeaponToBuy = &(*secondaryWeapon);
			secondaryClipsToBuy = secondaryClips;
			doneBuyingWeapons = true;
			if ( IsPresetCurrentCostDebuggingEnabled() ) gEngfuncs.Con_DPrintf( "\t\t\tDone buying weapons (primary && secondary)\n" );
			break;
		}
	}

	// Update our cost to reflect the weapons and ammo purchased
	cost = currentCost;

	if ( primaryWeaponToBuy )
	{
		BuyPresetWeapon weapon = *primaryWeaponToBuy;
		weapon.SetAmmoAmount( primaryClipsToBuy );
		weapon.SetAmmoType( AMMO_CLIPS );
		ws.m_primaryWeapon = weapon;
	}
	else if ( !m_isRebuy )
	{
		// If we failed to buy a primary weapon, and one was in the list, bail - the player can't afford this WeaponSet.
		for ( BuyPresetWeaponList::const_iterator primaryWeapon = primaryWeapons.begin(); primaryWeapon != primaryWeapons.end(); ++primaryWeapon )
		{
			int weaponID = primaryWeapon->GetWeaponID();
			if ( weaponID != WEAPON_NONE )
			{
				cost = -1;
				ws.Reset();
				return;
			}
		}
	}

	if ( secondaryWeaponToBuy )
	{
		BuyPresetWeapon weapon = *secondaryWeaponToBuy;
		weapon.SetAmmoAmount( secondaryClipsToBuy );
		weapon.SetAmmoType( AMMO_CLIPS );
		ws.m_secondaryWeapon = weapon;
	}
	else if ( !m_isSecondaryOptional )
	{
		if ( !m_isRebuy )
		{
			// If we failed to buy a required secondary weapon, and one was in the list, bail - the player can't afford this WeaponSet.
			for ( BuyPresetWeaponList::const_iterator secondaryWeapon = secondaryWeapons.begin(); secondaryWeapon != secondaryWeapons.end(); ++secondaryWeapon )
			{
				int weaponID = secondaryWeapon->GetWeaponID();
				if ( weaponID != WEAPON_NONE )
				{
					cost = -1;
					ws.Reset();
					return;
				}
			}
		}
	}

	//-------------------------------------------------------------------------
	// now any extra ammo, in rebuy order
	const char *remainder = SharedParse( TheBuyPresets->GetRebuyString() );
	const char *token;

	while ( remainder )
	{
		token = SharedGetToken();
		if ( !token || !*token )
			return;

		if ( !stricmp( token, "PrimaryWeapon" ) )
		{
			if ( m_isRebuy )
			{
				if ( !primaryWeaponToBuy )
				{
					int currentCost = cost;
					for ( BuyPresetWeaponList::const_iterator weapon = primaryWeapons.begin(); weapon != primaryWeapons.end(); ++weapon )
					{
						currentCost = cost;
						FillClientAmmo( ammo );

						if ( weapon->GetWeaponID() == WEAPON_NONE )
							continue;

						const WeaponInfoStruct *info = GetWeaponInfo( weapon->GetWeaponID() );
						BuyPresetWeapon weaponWithOneClip = *weapon;
						weaponWithOneClip.SetAmmoType( AMMO_CLIPS );
						weaponWithOneClip.SetAmmoAmount( 1 );
						weaponWithOneClip.SetFillAmmo( false );
						int clips = CalcClipsNeeded( &weaponWithOneClip, info, ammo );
						int weaponCost = info->cost;
						currentCost += weaponCost;
						currentCost += info->clipCost * clips;
						ammo[info->ammoType] += clips * info->buyClipSize;

						if ( IsPresetCurrentCostDebuggingEnabled() ) gEngfuncs.Con_DPrintf( "\t%5.5d (%s)\n", weaponCost, WeaponIDToAlias( weapon->GetWeaponID() ) );
						if ( IsPresetCurrentCostDebuggingEnabled() ) gEngfuncs.Con_DPrintf( "\t%5.5d (ammo)\n", info->clipCost * clips );
						if ( currentCash < currentCost )
						{
							continue;
						}

						if ( !CanBuyWeapon( primaryClientID, secondaryClientID, weapon->GetWeaponID() ) && primaryID != weapon->GetWeaponID() )
						{
							continue;
						}

						primaryWeaponToBuy = &(*weapon);
						primaryClipsToBuy = clips;
						if ( IsPresetCurrentCostDebuggingEnabled() ) gEngfuncs.Con_DPrintf( "\t\t\tDone buying weapons (optional primary)\n" );
						break;
					}
					if ( primaryWeaponToBuy )
					{
						cost = currentCost;
						BuyPresetWeapon weapon = *primaryWeaponToBuy;
						weapon.SetAmmoAmount( primaryClipsToBuy );
						weapon.SetAmmoType( AMMO_CLIPS );
						ws.m_primaryWeapon = weapon;
					}
				}
				if ( !primaryWeaponToBuy && primaryID != WEAPON_NONE )
				{
					BuyPresetWeapon weapon( primaryID );
					weapon.SetAmmoType( AMMO_CLIPS );
					weapon.SetAmmoAmount( 1 );
					weapon.SetFillAmmo( false );
					ws.m_primaryWeapon = weapon;

					primaryWeaponToBuy = &ws.m_primaryWeapon;
				}
			}
		}
		else if ( !stricmp( token, "PrimaryAmmo" ) )
		{
			if ( primaryWeaponToBuy )
			{
				int id = primaryWeaponToBuy->GetWeaponID();
				if ( id == WEAPON_NONE )
					id = primaryID;

				if ( m_isRebuy && primaryWeaponToBuy->GetAmmoType() == AMMO_ROUNDS )
				{
					const WeaponInfoStruct *info = GetWeaponInfo( id );
					if ( info )
					{
						int roundsLeft = primaryWeaponToBuy->GetAmmoAmount() - ammo[info->ammoType];
						if ( roundsLeft > 0 )
						{
							int clipsLeft = ( info->buyClipSize > 0 ) ? (int)ceilf( (float)roundsLeft / info->buyClipSize ) : 0;
							while ( clipsLeft > 0 && currentCash >= cost + info->clipCost )
							{
								ws.m_primaryWeapon.SetAmmoAmount( ws.m_primaryWeapon.GetAmmoAmount() + 1 );
								--clipsLeft;
								ammo[info->ammoType] += ( info->buyClipSize < info->maxRounds ) ? info->buyClipSize : info->maxRounds;
								cost += info->clipCost;
							}
						}
					}
				}
				else if ( primaryWeaponToBuy->GetFillAmmo() )
				{
					const WeaponInfoStruct *info = GetWeaponInfo( id );
					if ( info )
					{
						int roundsLeft = info->maxRounds - ammo[info->ammoType];
						int clipsLeft = ( info->buyClipSize > 0 ) ? (int)ceilf( (float)roundsLeft / info->buyClipSize ) : 0;
						while ( clipsLeft > 0 && currentCash >= cost + info->clipCost )
						{
							ws.m_primaryWeapon.SetAmmoAmount( ws.m_primaryWeapon.GetAmmoAmount() + 1 );
							--clipsLeft;
							ammo[info->ammoType] += ( info->buyClipSize < info->maxRounds ) ? info->buyClipSize : info->maxRounds;
							cost += info->clipCost;
						}
					}
				}
			}
		}
		else if ( !stricmp( token, "SecondaryWeapon" ) )
		{
			if ( ( m_isRebuy || m_isSecondaryOptional ) && !secondaryWeaponToBuy && !secondaryWeapons.empty() )
			{
				int currentCost = cost;
				const WeaponInfoStruct *primaryInfo = ( primaryWeaponToBuy ) ? GetWeaponInfo( primaryWeaponToBuy->GetWeaponID() ) : NULL;
				for ( BuyPresetWeaponList::const_iterator secondaryWeapon = secondaryWeapons.begin(); secondaryWeapon != secondaryWeapons.end(); ++secondaryWeapon )
				{
					currentCost = cost;
					FillClientAmmo( ammo );
					if ( primaryInfo )
						ammo[primaryInfo->ammoType] += ws.m_primaryWeapon.GetAmmoAmount() * primaryInfo->buyClipSize;

					WeaponIdType weaponID = secondaryWeapon->GetWeaponID();
					if ( weaponID == WEAPON_NONE )
						weaponID = secondaryID;

					const WeaponInfoStruct *secondaryInfo = GetWeaponInfo( weaponID );
					if ( !secondaryInfo )
						continue;

					int secondaryClips = CalcClipsNeeded( &(*secondaryWeapon), secondaryInfo, ammo );
					int secondaryWeaponCost = ( weaponID != secondaryID ) ? secondaryInfo->cost : 0;
					currentCost += secondaryWeaponCost;
					currentCost += secondaryInfo->clipCost * secondaryClips;
					ammo[secondaryInfo->ammoType] += secondaryClips * secondaryInfo->buyClipSize;

					if ( IsPresetCurrentCostDebuggingEnabled() ) gEngfuncs.Con_DPrintf( "\t%5.5d (%s)\n", secondaryWeaponCost, WeaponIDToAlias( weaponID ) );
					if ( IsPresetCurrentCostDebuggingEnabled() ) gEngfuncs.Con_DPrintf( "\t%5.5d (ammo)\n", secondaryInfo->clipCost * secondaryClips );
					if ( currentCash < currentCost )
					{
						continue;
					}

					if ( !CanBuyWeapon( primaryClientID, secondaryClientID, weaponID ) && weaponID != secondaryID )
					{
						continue;
					}

					secondaryWeaponToBuy = &(*secondaryWeapon);
					secondaryClipsToBuy = secondaryClips;
					if ( IsPresetCurrentCostDebuggingEnabled() ) gEngfuncs.Con_DPrintf( "\t\t\tDone buying weapons (optional secondary)\n" );
					break;
				}
				if ( secondaryWeaponToBuy )
				{
					cost = currentCost;
					BuyPresetWeapon weapon = *secondaryWeaponToBuy;
					weapon.SetAmmoAmount( secondaryClipsToBuy );
					weapon.SetAmmoType( AMMO_CLIPS );
					ws.m_secondaryWeapon = weapon;
				}
			}
		}
		else if ( !stricmp( token, "SecondaryAmmo" ) )
		{
			if ( secondaryWeaponToBuy )
			{
				if ( secondaryWeaponToBuy->GetFillAmmo() )
				{
					int weaponID = secondaryWeaponToBuy->GetWeaponID();
					if ( weaponID == WEAPON_NONE )
						weaponID = secondaryID;

					const WeaponInfoStruct *info = GetWeaponInfo( weaponID );
					if ( info )
					{
						int roundsLeft = info->maxRounds - ammo[info->ammoType];
						int clipsLeft = ( info->buyClipSize > 0 ) ? (int)ceilf( (float)roundsLeft / info->buyClipSize ) : 0;
						while ( clipsLeft > 0 && currentCash >= cost + info->clipCost )
						{
							ws.m_secondaryWeapon.SetAmmoAmount( ws.m_secondaryWeapon.GetAmmoAmount() + 1 );
							--clipsLeft;
							ammo[info->ammoType] += ( info->buyClipSize < info->maxRounds ) ? info->buyClipSize : info->maxRounds;
							cost += info->clipCost;
						}
					}
				}
			}
		}
		else if ( !stricmp( token, "Defuser" ) )
		{
			if ( !ws.m_defuser && m_defuser && g_iTeamNumber == 2 && !TheBuyPresets->HasDefuser() && CanBuyDefuser() && currentCash >= cost + DEFUSEKIT_PRICE )
			{
				cost += DEFUSEKIT_PRICE;
				ws.m_defuser = true;
			}
		}
		else if ( !stricmp( token, "Armor" ) )
		{
			if ( !ws.m_armor && m_armor > gHUD.m_Battery.m_iBat && currentCash >= cost + KEVLAR_PRICE )
			{
				cost += KEVLAR_PRICE;
				ws.m_armor = 100;
			}

			if ( !ws.m_helmet && m_helmet && m_armor > 0 && !gHUD.m_Battery.m_iArmorType && currentCash >= cost + KEVLAR_PRICE )
			{
				cost += HELMET_PRICE;
				ws.m_helmet = true;
			}
		}
		else if ( !stricmp( token, "HEGrenade" ) )
		{
			if ( m_HEGrenade && !hasHEGrenade && currentCash >= cost + HEGRENADE_PRICE )
			{
				cost += HEGRENADE_PRICE;
				ws.m_HEGrenade = true;
				hasHEGrenade = true;
			}
		}
		else if ( !stricmp( token, "Flashbang" ) )
		{
			int remainingFlashbangs = m_flashbangs - numFlashbangs;
			while ( remainingFlashbangs > 0 )
			{
				if ( currentCash >= cost + FLASHBANG_PRICE )
				{
					cost += FLASHBANG_PRICE;
					++ws.m_flashbangs;
					++numFlashbangs;
				}
				--remainingFlashbangs;
			}
		}
		else if ( !stricmp( token, "SmokeGrenade" ) )
		{
			if ( m_smokeGrenade && !hasSmokeGrenade && currentCash >= cost + SMOKEGRENADE_PRICE )
			{
				cost += SMOKEGRENADE_PRICE;
				ws.m_smokeGrenade = true;
				hasSmokeGrenade = true;
			}
		}
		else if ( !stricmp( token, "NightVision" ) )
		{
			if ( !ws.m_nightvision && m_nightvision && !TheBuyPresets->HasNightvision() && currentCash >= cost + NVG_PRICE )
			{
				cost += NVG_PRICE;
				ws.m_nightvision = true;
			}
		}
		else if ( !stricmp( token, "ExtraCash" ) )
		{
			if ( !ws.m_extraCash && m_extraCash )
			{
				int cashLeft = currentCash - cost;
				int reservedCash = ( cashLeft > m_extraCash ) ? m_extraCash : cashLeft;
				cost += reservedCash;
				ws.m_extraCash = reservedCash;
			}
		}

		remainder = SharedParse( remainder );
	}

	if ( cost > currentCash )
	{
		cost = -1; // can't buy it
		ws.Reset();
	}
	else
	{
		cost -= ws.m_extraCash;
	}
}
#pragma warning( pop )

//--------------------------------------------------------------------------------------------------------------
/**
 *  Constructs a WeaponSet containing everything that can be purchased.
 *  The cost (including extra cash) is also calculated and returned.
 */
void WeaponSet::GetFromScratch( int &cost, WeaponSet &ws, bool full ) const
{
	cost = 0;
	ws.Reset();

	int ammo[AMMO_MAX_TYPES];
	memset( ammo, 0, sizeof( ammo ) );

	//-------------------------------------------------------------------------
	// Primary weapon
	const WeaponInfoStruct *primaryInfo = GetWeaponInfo( m_primaryWeapon.GetWeaponID() );
	if ( primaryInfo )
	{
		int primaryClips = CalcClipsNeeded( &m_primaryWeapon, primaryInfo, ammo );
		int primaryWeaponCost = primaryInfo->cost;
		cost += primaryWeaponCost;
		cost += primaryInfo->clipCost * primaryClips;
		ammo[primaryInfo->ammoType] += primaryClips * primaryInfo->buyClipSize;

		ws.m_primaryWeapon = m_primaryWeapon;
		ws.m_primaryWeapon.SetAmmoAmount( primaryClips );
		ws.m_primaryWeapon.SetAmmoType( AMMO_CLIPS );
		ws.m_primaryWeapon.SetFillAmmo( false );
	}

	//-------------------------------------------------------------------------
	// secondary weapon
	if ( full || !m_isSecondaryOptional )
	{
		const BuyPresetWeapon *pSecondaryWeapon = &m_secondaryWeapon;
		const WeaponInfoStruct *secondaryInfo = GetWeaponInfo( pSecondaryWeapon->GetWeaponID() );
		if ( secondaryInfo )
		{
			int secondaryClips = CalcClipsNeeded( pSecondaryWeapon, secondaryInfo, ammo );
			int secondaryWeaponCost = secondaryInfo->cost;
			cost += secondaryWeaponCost;
			cost += secondaryInfo->clipCost * secondaryClips;
			ammo[secondaryInfo->ammoType] += secondaryClips * secondaryInfo->buyClipSize;

			ws.m_secondaryWeapon = *pSecondaryWeapon;
			ws.m_secondaryWeapon.SetAmmoAmount( secondaryClips );
			ws.m_secondaryWeapon.SetAmmoType( AMMO_CLIPS );
			ws.m_secondaryWeapon.SetFillAmmo( false );
		}
	}

	//-------------------------------------------------------------------------
	// equipment
	if ( full || !m_isArmorOptional )
	{
		if ( m_armor )
		{
			cost += ( m_helmet ) ? ASSAULTSUIT_PRICE : KEVLAR_PRICE;
			ws.m_armor = m_armor;
			ws.m_helmet = m_helmet;
		}
	}

	if ( full || !m_isSmokeOptional )
	{
		if ( m_smokeGrenade )
		{
			cost += SMOKEGRENADE_PRICE;
			ws.m_smokeGrenade = m_smokeGrenade;
		}
	}

	if ( full || !m_isHEGrenadeOptional )
	{
		if ( m_HEGrenade )
		{
			cost += HEGRENADE_PRICE;
			ws.m_HEGrenade = m_HEGrenade;
		}
	}

	if ( full || !m_isFlashbangOptional )
	{
		cost += FLASHBANG_PRICE * m_flashbangs;
		ws.m_flashbangs = m_flashbangs;
	}

	if ( full || !m_isDefuserOptional )
	{
		if ( m_defuser )
		{
			cost += DEFUSEKIT_PRICE;
			ws.m_defuser = m_defuser;
		}
	}

	if ( full || !m_isNightvisionOptional )
	{
		if ( m_nightvision )
		{
			cost += NVG_PRICE;
			ws.m_nightvision = m_nightvision;
		}
	}

	if ( full || !m_isCashOptional )
	{
		cost += m_extraCash;
		ws.m_extraCash = m_extraCash;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Convenience function that wraps GetFromScratch() and discards the generated WeaponSet.  Returns the cost
 *  to buy the full WeaponSet from scratch.
 */
int WeaponSet::FullCost( void ) const
{
	WeaponSet fullSet;
	int cost;
	GetFromScratch( cost, fullSet, true );
	return cost;
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Generates a list of buy commands that will buy the current WeaponSet.
 */
void WeaponSet::GenerateBuyCommands( char *command ) const
{
	command[0] = 0;
	char *tmp = command;
	int remainder = BUY_PRESET_COMMAND_LEN;
	int i;

	ClientWeaponIdType primaryClientID = GetClientWeaponID( true );		///< The local player's current primary weapon
	ClientWeaponIdType secondaryClientID = GetClientWeaponID( false );	///< The local player's current pistol

	WeaponIdType primaryID = ClientWeaponIDToGameWeaponID( primaryClientID );
	WeaponIdType secondaryID = ClientWeaponIDToGameWeaponID( secondaryClientID );

	//-------------------------------------------------------------------------
	// Primary weapon
	int weaponID = m_primaryWeapon.GetWeaponID();
	if ( weaponID == WEAPON_NONE )
		weaponID = primaryID;
	const WeaponInfoStruct *primaryInfo = GetWeaponInfo( weaponID );
	if ( primaryInfo )
	{
		if ( weaponID != primaryID )
		{
			tmp = BufPrintf( tmp, remainder, "%s\n", WeaponIDToAlias( weaponID ) );
		}
		for ( i = 0; i < m_primaryWeapon.GetAmmoAmount(); ++i )
		{
			tmp = BufPrintf( tmp, remainder, "buyammo1\n" );
		}
	}

	//-------------------------------------------------------------------------
	// secondary weapon
	weaponID = m_secondaryWeapon.GetWeaponID();
	if ( weaponID == WEAPON_NONE )
		weaponID = secondaryID;
	const WeaponInfoStruct *secondaryInfo = GetWeaponInfo( weaponID );
	if ( secondaryInfo )
	{
		if ( weaponID != secondaryID )
		{
			tmp = BufPrintf( tmp, remainder, "%s\n", WeaponIDToAlias( weaponID ) );
		}
		for ( i = 0; i < m_secondaryWeapon.GetAmmoAmount(); ++i )
		{
			tmp = BufPrintf( tmp, remainder, "buyammo2\n" );
		}
	}

	//-------------------------------------------------------------------------
	// equipment
	if ( m_armor )
	{
		if ( m_helmet )
		{
			tmp = BufPrintf( tmp, remainder, "vesthelm\n" );
		}
		else
		{
			tmp = BufPrintf( tmp, remainder, "vest\n" );
		}
	}

	if ( m_smokeGrenade )
	{
		tmp = BufPrintf( tmp, remainder, "sgren\n" );
	}

	if ( m_HEGrenade )
	{
		tmp = BufPrintf( tmp, remainder, "hegren\n" );
	}

	for ( i = 0; i < m_flashbangs; ++i )
	{
		tmp = BufPrintf( tmp, remainder, "flash\n" );
	}

	if ( m_defuser )
	{
		tmp = BufPrintf( tmp, remainder, "defuser\n" );
	}

	if ( m_nightvision )
	{
		tmp = BufPrintf( tmp, remainder, "nvgs\n" );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Return images out of a subdir, so when the buy preset system resizes the images, they don't resize in the
 *  main buy menu.
 */
const char *ImageFnameFromWeaponID( WeaponIdType weaponID, bool isPrimary )
{
	switch ( weaponID )
	{
	case WEAPON_NONE:
		if ( g_pCareerUI->IsPlayingMatch() )
		{
			if ( isPrimary )
			{
				return "gfx/vgui/buy_presets/defaultrifle_career";
			}
			else
			{
				return "gfx/vgui/buy_presets/defaultpistol_career";
			}
		}
		else
		{
			if ( isPrimary )
			{
				return "gfx/vgui/buy_presets/defaultrifle";
			}
			else
			{
				return "gfx/vgui/buy_presets/defaultpistol";
			}
		}
	case WEAPON_SCOUT:
		return "gfx/vgui/buy_presets/scout";
	case WEAPON_XM1014:
		return "gfx/vgui/buy_presets/xm1014";
	case WEAPON_MAC10:
		return "gfx/vgui/buy_presets/mac10";
	case WEAPON_AUG:
		return "gfx/vgui/buy_presets/aug";
	case WEAPON_UMP45:
		return "gfx/vgui/buy_presets/ump45";
	case WEAPON_SG550:
		return "gfx/vgui/buy_presets/sg550";
	case WEAPON_GALIL:
		return "gfx/vgui/buy_presets/galil";
	case WEAPON_FAMAS:
		return "gfx/vgui/buy_presets/famas";
	case WEAPON_AWP:
		return "gfx/vgui/buy_presets/awp";
	case WEAPON_MP5N:
		return "gfx/vgui/buy_presets/mp5";
	case WEAPON_M249:
		return "gfx/vgui/buy_presets/m249";
	case WEAPON_M3:
		return "gfx/vgui/buy_presets/m3";
	case WEAPON_M4A1:
		return "gfx/vgui/buy_presets/m4a1";
	case WEAPON_TMP:
		return "gfx/vgui/buy_presets/tmp";
	case WEAPON_G3SG1:
		return "gfx/vgui/buy_presets/g3sg1";
	case WEAPON_SG552:
		return "gfx/vgui/buy_presets/sg552";
	case WEAPON_AK47:
		return "gfx/vgui/buy_presets/ak47";
	case WEAPON_P90:
		return "gfx/vgui/buy_presets/p90";
	case WEAPON_SHIELDGUN:
		return "gfx/vgui/buy_presets/shield";

	case WEAPON_USP:
		return "gfx/vgui/buy_presets/usp45";
	case WEAPON_GLOCK18:
		return "gfx/vgui/buy_presets/glock18";
	case WEAPON_DEAGLE:
		return "gfx/vgui/buy_presets/deserteagle";
	case WEAPON_ELITE:
		return "gfx/vgui/buy_presets/elites";
	case WEAPON_P228:
		return "gfx/vgui/buy_presets/p228";
	case WEAPON_FIVESEVEN:
		return "gfx/vgui/buy_presets/fiveseven";

	default:
		return "";
	}
}

//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
BuyPreset::BuyPreset( void )
{
	SetName( L"" );
	m_isAutobuy = false;
}

//--------------------------------------------------------------------------------------------------------------
BuyPreset::~BuyPreset( void )
{
}

//--------------------------------------------------------------------------------------------------------------
void BuyPreset::SetName( const wchar_t *name )
{
	wcsncpy( m_name, name, MaxBuyPresetName );
	if ( m_name[0] == 0 )
	{
		const wchar_t *defaultName = vgui2::localize()->Find( "#Cstrike_BuyPresetBlank" );
		if ( defaultName )
		{
			wcsncpy( m_name, defaultName, MaxBuyPresetName );
		}
	}
	m_name[MaxBuyPresetName - 1] = 0;
}


//--------------------------------------------------------------------------------------------------------------
// Parse KeyValues string into a BuyPresetWeapon vector
static void ParseWeaponString( const char *str, BuyPresetWeaponList &weapons, bool isPrimary )
{
	weapons.clear();

	if ( !str )
		return;

	const char *remainder = SharedParse( str );
	const char *token;

	const int BufLen = 32;
	char tmpBuf[BufLen];
	char weaponBuf[BufLen];
	char clipModifier;
	int numClips;

	while ( remainder )
	{
		token = SharedGetToken();
		if ( !token || strlen( token ) >= BufLen )
			return;

		strncpy( tmpBuf, token, BufLen );
		tmpBuf[BufLen - 1] = 0;
		char *tmp = tmpBuf;
		while ( *tmp )
		{
			if ( *tmp == '/' )
			{
				*tmp = ' ';
			}
			++tmp;
		}
		// sscanf is safe, since the size of the string being scanned is at least as small as any individual destination buffer
		if ( sscanf( tmpBuf, "%s %d%c", weaponBuf, &numClips, &clipModifier ) != 3 )
			return;

		WeaponIdType weaponID = AliasToWeaponID( weaponBuf );
		const WeaponInfoStruct *info = GetWeaponInfo( weaponID );
		if ( info )
		{
			numClips = min( (int)ceil( (float)info->maxRounds / (float)info->buyClipSize ), max( 0, numClips ) );
			if ( IsPrimaryWeapon( weaponID ) != isPrimary )
				return;
		}
		else
		{
			numClips = min( NUM_CLIPS_FOR_CURRENT, max( 0, numClips ) );
		}

		BuyPresetWeapon weapon( weaponID );
		weapon.SetAmmoType( AMMO_CLIPS );
		weapon.SetAmmoAmount( numClips );
		weapon.SetFillAmmo( ( clipModifier == '+' ) );
		weapons.push_back( weapon );

		remainder = SharedParse( remainder );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Populates data from a KeyValues structure, for loading
 */
void BuyPreset::Parse( KeyValues *data )
{
	m_name[0] = 0;
	m_weaponList.clear();

	if ( !data )
		return;

	//-------------------------------------------------------------------------
	// Read name
	wcsncpy( m_name, data->GetWString( "PresetName", L"" ), MaxBuyPresetName );
	m_name[MaxBuyPresetName - 1] = 0;
	if ( IsPresetDebuggingEnabled() ) gEngfuncs.Con_DPrintf( "Parsing Buy Preset %ls\n", m_name );

	int version = data->GetInt( "Version", 0 );
	if ( version < 1 || version > 3 )
	{
		if ( IsPresetDebuggingEnabled() ) gEngfuncs.Con_DPrintf( "Invalid preset version %d\n", version );
		return;
	}

	m_isAutobuy = false;

	int nextVal = 1;
	while ( true )
	{
		KeyValues *weaponSetKey = data->FindKey( NumAsString( nextVal ), false );
		if ( !weaponSetKey )
			break;

		const char *primaryString = weaponSetKey->GetString( "Primary", NULL );
		const char *secondaryString = weaponSetKey->GetString( "Secondary", NULL );
		const char *equipmentString = weaponSetKey->GetString( "Equipment", NULL );

		BuyPresetWeaponList weapons;
		ParseWeaponString( primaryString, weapons, true );

		if ( weapons.size() != 1 )
			return;

		WeaponSet ws;
		if ( weapons.size() )
			ws.m_primaryWeapon = weapons[0];

		weapons.clear();
		ParseWeaponString( secondaryString, weapons, false );
		if ( weapons.size() )
			ws.m_secondaryWeapon = weapons[0];

		ws.m_isSecondaryOptional = false;

		// parse equipment
		if ( equipmentString )
		{
			const char *remainder = SharedParse( equipmentString );
			const char *token;

			const int BufLen = 32;
			char tmpBuf[BufLen];
			char itemBuf[BufLen];
			int intVal;

			while ( remainder )
			{
				token = SharedGetToken();
				if ( !token || strlen( token ) >= BufLen )
					break;

				strncpy( tmpBuf, token, BufLen );
				tmpBuf[BufLen - 1] = 0;
				char *tmp = tmpBuf;
				while ( *tmp )
				{
					if ( *tmp == '/' )
					{
						*tmp = ' ';
					}
					++tmp;
				}
				// sscanf is safe, since the size of the string being scanned is at least as small as any individual destination buffer
				if ( sscanf( tmpBuf, "%s %d", itemBuf, &intVal ) != 2 )
					break;

				if ( !strcmp( itemBuf, "vest" ) )
				{
					ws.m_armor = ( intVal > 0 ) ? 100 : 0;
					ws.m_helmet = false;
					ws.m_isArmorOptional = false;
				}
				else if ( !strcmp( itemBuf, "vesthelm" ) )
				{
					ws.m_armor = ( intVal > 0 ) ? 100 : 0;
					ws.m_helmet = true;
					ws.m_isArmorOptional = false;
				}
				else if ( !strcmp( itemBuf, "defuser" ) )
				{
					ws.m_defuser = ( intVal > 0 );
					ws.m_isDefuserOptional = false;
				}
				else if ( !strcmp( itemBuf, "nvgs" ) )
				{
					ws.m_nightvision = ( intVal > 0 );
					ws.m_isNightvisionOptional = false;
				}
				else if ( !strcmp( itemBuf, "sgren" ) )
				{
					ws.m_smokeGrenade = ( intVal > 0 );
					ws.m_isSmokeOptional = false;
				}
				else if ( !strcmp( itemBuf, "hegren" ) )
				{
					ws.m_HEGrenade = ( intVal > 0 );
					ws.m_isHEGrenadeOptional = false;
				}
				else if ( !strcmp( itemBuf, "flash" ) )
				{
					ws.m_flashbangs = min( 2, max( 0, intVal ) );
					ws.m_isFlashbangOptional = false;
				}

				remainder = SharedParse( remainder );
			}
		}

		m_weaponList.push_back( ws );

		++nextVal;
	}
}

//--------------------------------------------------------------------------------------------------------------
// Build a string of weapons+ammo for KeyValues saving/loading
static const char *ConstructWeaponString( const BuyPresetWeapon &weapon )
{
	const int WeaponLen = 1024;
	static char weaponString[WeaponLen];
	weaponString[0] = 0;
	int remainder = WeaponLen;
	char *tmp = weaponString;

	tmp = BufPrintf( tmp, remainder, "%s/%d%c",
		WeaponIDToAlias( weapon.GetWeaponID() ),
		weapon.GetAmmoAmount(),
		( weapon.GetFillAmmo() ) ? '+' : '=' );
	return weaponString;
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Fills in a KeyValues structure with data, for saving
 */
void BuyPreset::Save( KeyValues *data )
{
	//-------------------------------------------------------------------------
	// Save name and version
	KeyValues *presetKey = data->CreateNewKey();
	presetKey->SetWString( "PresetName", m_name );

	/**
	 *  Version History
	 *  ---------------
	 *
	 *  PRE-RELEASE
	 *  1.  2/2004
	 *      First-pass implementation.  Allowed for multiple weapons per fallback.
	 *      Individual items could be marked optional.  Cash reserve could be specified.
	 *  2.  3/2004
	 *      Second-pass implementation.  Removed multiple weapons per fallback.  The
	 *      pistol could be marked optional.  A preset could be marked as 'use for
	 *      Autobuy'.
	 *
	 *  CZ RELEASE
	 *  3.  4/22/2004
	 *      The first released version.  Removed optional/required status.  Also
	 *      removed the cash reserve and autobuy status.  Corresponds to streamlined
	 *      editing interface mocked up by Greg Coomer.
	 */
	presetKey->SetInt( "Version", 3 );
	for ( WeaponSetListIt it = m_weaponList.begin(); it != m_weaponList.end(); ++it )
	{
		//-------------------------------------------------------------------------
		// Save a fallback WeaponSet
		WeaponSet ws = *it;

		KeyValues *weaponSetKey = presetKey->CreateNewKey();

		weaponSetKey->SetString( "Primary", ConstructWeaponString( ws.m_primaryWeapon ) );
		weaponSetKey->SetString( "Secondary", ConstructWeaponString( ws.m_secondaryWeapon ) );

		weaponSetKey->SetString( "Equipment",
			SharedVarArgs( "vest%s/%d flash/%d sgren/%d hegren/%d defuser/%d nvgs/%d",
			( ws.m_helmet ) ? "helm" : "", ws.m_armor,
			ws.m_flashbangs,
			ws.m_smokeGrenade,
			ws.m_HEGrenade,
			ws.m_defuser,
			ws.m_nightvision
			) );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Calculates the full cost of the preset, which is exactly the full cost of the first fallback WeaponSet
 */
int BuyPreset::FullCost( void ) const
{
	if ( m_weaponList.empty() )
		return 0;

	return m_weaponList.front().FullCost();
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Returns the specified fallback WeaponSet, or NULL if it doesn't exist
 */
const WeaponSet *BuyPreset::GetSet( int index ) const
{
	if ( index < 0 || (unsigned int)index >= m_weaponList.size() )
		return NULL;

	return &( m_weaponList[index] );
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Deletes a fallback
 */
void BuyPreset::DeleteSet( int index )
{
	if ( index < 0 || (unsigned int)index >= m_weaponList.size() )
		return;

	m_weaponList.erase( m_weaponList.begin() + 1 );
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Switches the order of two fallbacks
 */
void BuyPreset::SwapSet( int firstIndex, int secondIndex )
{
	if ( firstIndex < 0 || (unsigned int)firstIndex >= m_weaponList.size() )
		return;

	if ( secondIndex < 0 || (unsigned int)secondIndex >= m_weaponList.size() )
		return;

	WeaponSet tmpSet = m_weaponList[firstIndex];
	m_weaponList[firstIndex] = m_weaponList[secondIndex];
	m_weaponList[secondIndex] = tmpSet;
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Replaces a fallback with the supplied WeaponSet
 */
void BuyPreset::ReplaceSet( int index, const WeaponSet &weaponSet )
{
	if ( index < 0 || (unsigned int)index > m_weaponList.size() )
		return;

	if ( (unsigned int)index == m_weaponList.size() )
	{
		m_weaponList.push_back( weaponSet );
	}
	else
	{
		m_weaponList[index] = weaponSet;
	}
}

//--------------------------------------------------------------------------------------------------------------
