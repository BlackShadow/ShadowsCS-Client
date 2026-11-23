//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: BuyPresetWeapon implementation, and misc knowledge relating to weapons
//
//=============================================================================

#include "buy_presets.h"
#include "hud.h"
#include "ammohistory.h"
#include "vgui/ILocalize.h"
#include "vgui_controls/Controls.h"
#include "VGUI/GameUI_Interfaces.h"
#include "GameUI/ICareerUI.h"
#include "buy_preset_debug.h"
#include <math.h>
#include <string.h>

//--------------------------------------------------------------------------------------------------------------
WeaponIdType ClientWeaponIDToGameWeaponID( ClientWeaponIdType id )
{
	switch ( id )
	{
	case ID_NONE:
		return WEAPON_NONE;
	case ID_P228:
		return WEAPON_P228;
	case ID_SHIELD:
		return WEAPON_SHIELDGUN;
	case ID_SCOUT:
		return WEAPON_SCOUT;
	case ID_HEGRENADE:
		return WEAPON_NONE;
	case ID_XM1014:
		return WEAPON_XM1014;
	case ID_C4:
		return WEAPON_C4;
	case ID_MAC10:
		return WEAPON_MAC10;
	case ID_AUG:
		return WEAPON_AUG;
	case ID_SMOKEGRENADE:
		return WEAPON_NONE;
	case ID_ELITES:
		return WEAPON_ELITE;
	case ID_FIVESEVEN:
		return WEAPON_FIVESEVEN;
	case ID_UMP45:
		return WEAPON_UMP45;
	case ID_SG550:
		return WEAPON_SG550;
	case ID_GALIL:
		return WEAPON_GALIL;
	case ID_FAMAS:
		return WEAPON_FAMAS;
	case ID_USP:
		return WEAPON_USP;
	case ID_GLOCK18:
		return WEAPON_GLOCK18;
	case ID_AWP:
		return WEAPON_AWP;
	case ID_MP5N:
		return WEAPON_MP5N;
	case ID_M249:
		return WEAPON_M249;
	case ID_M3:
		return WEAPON_M3;
	case ID_M4A1:
		return WEAPON_M4A1;
	case ID_TMP:
		return WEAPON_TMP;
	case ID_G3SG1:
		return WEAPON_G3SG1;
	case ID_FLASHBANG:
		return WEAPON_NONE;
	case ID_DEAGLE:
		return WEAPON_DEAGLE;
	case ID_SG552:
		return WEAPON_SG552;
	case ID_AK47:
		return WEAPON_AK47;
	case ID_KNIFE:
		return WEAPON_NONE;
	case ID_P90:
		return WEAPON_P90;
	}
	return WEAPON_NONE;
}

//--------------------------------------------------------------------------------------------------------------
struct WeaponDisplayNameInfo
{
	WeaponIdType id;
	const char *displayName;
};

//--------------------------------------------------------------------------------------------------------------
// NOTE: Array must be NULL-terminated
static const WeaponDisplayNameInfo weaponDisplayNameInfo[] =
{
	{ WEAPON_P228,		"#Cstrike_TitlesTXT_P228" },
	{ WEAPON_GLOCK,		"#Cstrike_TitlesTXT_Glock18" },
	{ WEAPON_GLOCK18,	"#Cstrike_TitlesTXT_Glock18" },
	{ WEAPON_SCOUT,		"#Cstrike_TitlesTXT_Scout" },
	{ WEAPON_XM1014,	"#Cstrike_TitlesTXT_AutoShotgun" },
	{ WEAPON_MAC10,		"#Cstrike_TitlesTXT_Mac10_Short" },
	{ WEAPON_AUG,		"#Cstrike_TitlesTXT_Aug" },
	{ WEAPON_ELITE,		"#Cstrike_TitlesTXT_Beretta96G" },
	{ WEAPON_FIVESEVEN,	"#Cstrike_TitlesTXT_ESFiveSeven" },
	{ WEAPON_UMP45,		"#Cstrike_TitlesTXT_KMUMP45" },
	{ WEAPON_SG550,		"#Cstrike_TitlesTXT_SG550" },
	{ WEAPON_GALIL,		"#Cstrike_TitlesTXT_Galil" },
	{ WEAPON_FAMAS,		"#Cstrike_TitlesTXT_Famas" },
	{ WEAPON_USP,		"#Cstrike_TitlesTXT_USP45" },
	{ WEAPON_AWP,		"#Cstrike_TitlesTXT_ArcticWarfareMagnum" },
	{ WEAPON_MP5N,		"#Cstrike_TitlesTXT_mp5navy" },
	{ WEAPON_M249,		"#Cstrike_TitlesTXT_ESM249" },
	{ WEAPON_M3,		"#Cstrike_TitlesTXT_Leone12" },
	{ WEAPON_M4A1,		"#Cstrike_TitlesTXT_M4A1_Short" },
	{ WEAPON_TMP,		"#Cstrike_TitlesTXT_tmp" },
	{ WEAPON_G3SG1,		"#Cstrike_TitlesTXT_G3SG1" },
	{ WEAPON_DEAGLE,	"#Cstrike_TitlesTXT_DesertEagle" },
	{ WEAPON_SG552,		"#Cstrike_TitlesTXT_SG552" },
	{ WEAPON_AK47,		"#Cstrike_TitlesTXT_AK47" },
	{ WEAPON_P90,		"#Cstrike_TitlesTXT_ESC90" },
	{ WEAPON_SHIELDGUN,	"#Cstrike_TitlesTXT_TactShield" },

	{ WEAPON_NONE,		"#Cstrike_CurrentWeapon" }
};

//--------------------------------------------------------------------------------------------------------------
/**
 *  Returns the display name for a weapon, based on it's weaponID
 */
const wchar_t *WeaponIDToDisplayName( WeaponIdType weaponID )
{
	for ( int i = 0; weaponDisplayNameInfo[i].displayName; ++i )
		if ( weaponDisplayNameInfo[i].id == weaponID )
			return vgui2::localize()->Find( weaponDisplayNameInfo[i].displayName );

	return NULL;
}

//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
BuyPresetWeapon::BuyPresetWeapon( void )
{
	m_name = NULL;
	m_weaponID = WEAPON_NONE;
	m_ammoType = AMMO_CLIPS;
	m_ammoAmount = 0;
	m_fillAmmo = true;
}

//--------------------------------------------------------------------------------------------------------------
BuyPresetWeapon::BuyPresetWeapon( WeaponIdType weaponID )
{
	m_name = WeaponIDToDisplayName( weaponID );
	m_weaponID = weaponID;
	m_ammoType = AMMO_CLIPS;
	m_ammoAmount = weaponID != WEAPON_NONE;
	m_fillAmmo = true;
}

//--------------------------------------------------------------------------------------------------------------
BuyPresetWeapon &BuyPresetWeapon::operator=( const BuyPresetWeapon &other )
{
	m_name = other.m_name;
	m_weaponID = other.m_weaponID;
	m_ammoType = other.m_ammoType;
	m_ammoAmount = other.m_ammoAmount;
	m_fillAmmo = other.m_fillAmmo;

	return *this;
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Sets the WEAPON_* weapon ID (and resets ammo, etc)
 */
void BuyPresetWeapon::SetWeaponID( WeaponIdType weaponID )
{
	m_name = WeaponIDToDisplayName( weaponID );
	m_weaponID = weaponID;
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Returns the number of clips that will have to be bought for the specified BuyPresetWeapon
 */
int CalcClipsNeeded( const BuyPresetWeapon *pWeapon, const WeaponInfoStruct *pInfo, const int *ammo )
{
	int numClips = 0;
	if ( pInfo && pWeapon && pInfo->buyClipSize && pInfo->ammoType >= 0 )
	{
		switch ( pWeapon->GetAmmoType() )
		{
		case AMMO_CLIPS:
			numClips = pWeapon->GetAmmoAmount();
			if ( pWeapon->GetWeaponID() == WEAPON_NONE && numClips >= NUM_CLIPS_FOR_CURRENT )
			{
				numClips = (int)ceilf( (float)pInfo->maxRounds / pInfo->buyClipSize );
			}
			numClips = min( (int)ceilf( (float)pInfo->maxRounds / pInfo->buyClipSize ), numClips );

			numClips -= ammo[pInfo->ammoType] / pInfo->buyClipSize;
			if ( numClips < 0 || ammo[pInfo->ammoType] == pInfo->maxRounds )
			{
				numClips = 0;
			}
			break;
		case AMMO_ROUNDS:
			{
				int roundsNeeded = pWeapon->GetAmmoAmount() - ammo[pInfo->ammoType];
				if ( roundsNeeded > 0 )
				{
					numClips = (int)( ceil( (double)roundsNeeded ) / pInfo->buyClipSize );
				}
				else
				{
					numClips = 0;
				}
			}
			break;
		case AMMO_PERCENT:
			{
				int roundsNeeded = pWeapon->GetAmmoAmount() * pInfo->maxRounds - ammo[pInfo->ammoType];
				roundsNeeded = (int)( roundsNeeded * 0.01f );
				if ( roundsNeeded > 0 )
				{
					numClips = (int)ceilf( (float)roundsNeeded / pInfo->buyClipSize );
				}
				else
				{
					numClips = 0;
				}
			}
			break;
		}
	}
	return numClips;
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Returns true if the client can currently buy the weapon according to class/gamemode restrictions.  Returns
 *  true also if the player owns the weapon already.
 */
bool CanBuyWeapon( ClientWeaponIdType currentPrimaryID, ClientWeaponIdType currentSecondaryID, WeaponIdType weaponID )
{
	if ( currentPrimaryID == ID_SHIELD && weaponID == WEAPON_ELITE )
	{
		return false;
	}

	if ( currentSecondaryID == ID_ELITES && weaponID == WEAPON_SHIELDGUN )
	{
		return false;
	}

	return CanBuyWeaponByMaptype( g_iTeamNumber, weaponID, IsASMapType() );
}

//--------------------------------------------------------------------------------------------------------------
static bool IsPrimaryWeaponClassID( WeaponClassType classId )
{
	switch ( classId )
	{
	case WEAPONCLASS_SUBMACHINEGUN:
	case WEAPONCLASS_SHOTGUN:
	case WEAPONCLASS_MACHINEGUN:
	case WEAPONCLASS_RIFLE:
	case WEAPONCLASS_SNIPERRIFLE:
		return true;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Returns true if the weapon ID is for a primary weapon
 */
static bool IsPrimaryWeaponID( WeaponIdType id )
{
	if ( id == WEAPON_SHIELDGUN )
		return true;

	const char *alias = WeaponIDToAlias( id );
	if ( !alias )
	{
		return false;
	}

	WeaponClassType classId = AliasToWeaponClass( alias );
	return IsPrimaryWeaponClassID( classId );
}

//--------------------------------------------------------------------------------------------------------------
static bool IsSecondaryWeaponClassID( WeaponClassType classId )
{
	return ( classId == WEAPONCLASS_PISTOL );
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Returns true if the weapon ID is for a secondary weapon
 */
static bool IsSecondaryWeaponID( WeaponIdType id )
{
	if ( id == WEAPON_SHIELDGUN )
		return false;

	const char *alias = WeaponIDToAlias( id );
	if ( !alias )
	{
		return false;
	}

	WeaponClassType classId = AliasToWeaponClass( alias );
	return IsSecondaryWeaponClassID( classId );
}

//--------------------------------------------------------------------------------------------------------------
static bool CurrentWeaponMatches( ClientWeaponIdType currentClientID, WeaponIdType weaponID, WeaponClassType classID )
{
	WeaponIdType currentID = ClientWeaponIDToGameWeaponID( currentClientID );

	if ( weaponID != WEAPON_NONE && weaponID == currentID )
		return true;

	const char *alias = WeaponIDToAlias( currentID );
	if ( !alias )
		return false;

	WeaponClassType currentClassID = AliasToWeaponClass( alias );
	return ( currentClassID == classID && classID != WEAPONCLASS_NONE );
}

//--------------------------------------------------------------------------------------------------------------
static BuyPresetWeaponList CareerWeaponListForClass( WeaponClassType weaponClassID )
{
	BuyPresetWeaponList classWeapons;
	switch ( weaponClassID )
	{
	case WEAPONCLASS_PISTOL:
		{
			ClientWeaponIdType secondaryClientID = GetClientWeaponID( false );
			WeaponIdType secondaryID = ClientWeaponIDToGameWeaponID( secondaryClientID );
			if ( secondaryID != WEAPON_NONE )
			{
				classWeapons.push_back( BuyPresetWeapon( secondaryID ) );
			}
			if ( g_iTeamNumber == 2 )
			{
				classWeapons.push_back( BuyPresetWeapon( WEAPON_USP ) );
				classWeapons.push_back( BuyPresetWeapon( WEAPON_GLOCK18 ) );
			}
			else
			{
				classWeapons.push_back( BuyPresetWeapon( WEAPON_GLOCK18 ) );
				classWeapons.push_back( BuyPresetWeapon( WEAPON_USP ) );
			}
			classWeapons.push_back( BuyPresetWeapon( WEAPON_DEAGLE ) );
			classWeapons.push_back( BuyPresetWeapon( WEAPON_ELITE ) );
			classWeapons.push_back( BuyPresetWeapon( WEAPON_FIVESEVEN ) );
			classWeapons.push_back( BuyPresetWeapon( WEAPON_P228 ) );
		}
		break;
	case WEAPONCLASS_SNIPERRIFLE:
		classWeapons.push_back( BuyPresetWeapon( WEAPON_AWP ) );
		classWeapons.push_back( BuyPresetWeapon( WEAPON_SG550 ) );
		classWeapons.push_back( BuyPresetWeapon( WEAPON_G3SG1 ) );
		classWeapons.push_back( BuyPresetWeapon( WEAPON_SCOUT ) );
		break;
	case WEAPONCLASS_SHOTGUN:
		classWeapons.push_back( BuyPresetWeapon( WEAPON_XM1014 ) );
		classWeapons.push_back( BuyPresetWeapon( WEAPON_M3 ) );
		break;
	case WEAPONCLASS_SUBMACHINEGUN:
		classWeapons.push_back( BuyPresetWeapon( WEAPON_P90 ) );
		classWeapons.push_back( BuyPresetWeapon( WEAPON_UMP45 ) );
		classWeapons.push_back( BuyPresetWeapon( WEAPON_MP5N ) );
		classWeapons.push_back( BuyPresetWeapon( WEAPON_TMP ) );
		classWeapons.push_back( BuyPresetWeapon( WEAPON_MAC10 ) );
		break;
	case WEAPONCLASS_MACHINEGUN:
		classWeapons.push_back( BuyPresetWeapon( WEAPON_M249 ) );
		break;
	case WEAPONCLASS_RIFLE:
		classWeapons.push_back( BuyPresetWeapon( WEAPON_SG552 ) );
		classWeapons.push_back( BuyPresetWeapon( WEAPON_AUG ) );
		classWeapons.push_back( BuyPresetWeapon( WEAPON_AK47 ) );
		classWeapons.push_back( BuyPresetWeapon( WEAPON_M4A1 ) );
		classWeapons.push_back( BuyPresetWeapon( WEAPON_GALIL ) );
		classWeapons.push_back( BuyPresetWeapon( WEAPON_FAMAS ) );
		break;
	}

	return classWeapons;
}

//--------------------------------------------------------------------------------------------------------------
static BuyPresetWeaponList CareerWeaponListOrganized( const BuyPresetWeaponList &careerSource, const BuyPresetWeaponList &source )
{
	BuyPresetWeaponList classWeapons = careerSource;

	int sourceIndex;
	for ( sourceIndex = (int)source.size() - 1; sourceIndex >= 0; --sourceIndex )
	{
		for ( int classIndex = 1; classIndex < (int)classWeapons.size(); ++classIndex )
		{
			if ( source[sourceIndex].GetWeaponID() == classWeapons[classIndex].GetWeaponID() )
			{
				classWeapons.insert( classWeapons.begin(), source[sourceIndex] );
				classWeapons.erase( classWeapons.begin() + classIndex + 1 );
				break;
			}
		}
	}

	for ( sourceIndex = 0; sourceIndex < (int)source.size(); ++sourceIndex )
	{
		bool found = false;
		for ( int classIndex = 0; classIndex < (int)classWeapons.size(); ++classIndex )
		{
			if ( source[sourceIndex].GetWeaponID() == classWeapons[classIndex].GetWeaponID() )
			{
				found = true;
				break;
			}
		}
		if ( !found )
		{
			classWeapons.push_back( source[sourceIndex] );
		}
	}

	if ( IsPresetCareerDebuggingEnabled() ) gEngfuncs.Con_DPrintf( "CareerWeaponListWithClass() - weapons are:\n" );
	int i;
	for ( i = 0; i < (int)classWeapons.size(); ++i )
	{
		if ( IsPresetCareerDebuggingEnabled() ) gEngfuncs.Con_DPrintf( "\t%s + %d%s clips\n",
			WeaponIDToAlias( classWeapons[i].GetWeaponID() ),
			classWeapons[i].GetAmmoAmount(),
			( classWeapons[i].GetFillAmmo() ) ? "+" : "" );
	}

	return classWeapons;
}

//--------------------------------------------------------------------------------------------------------------
BuyPresetWeaponList CareerWeaponList( const BuyPresetWeaponList &source, bool isPrimary, ClientWeaponIdType currentClientID )
{
	if ( !g_pCareerUI->IsPlayingMatch() )
		return source;

	ITaskVec *tv = g_pCareerUI->GetCurrentTaskVec();
	if ( !tv )
		return source;

	ITaskVec taskVector;
	ITaskVecIt it;
	for ( it = tv->begin(); it != tv->end(); ++it )
	{
		ICareerTask *task = *it;
		const char *weaponName = task->GetWeaponName();
		WeaponIdType weaponID = AliasToWeaponID( weaponName );
		WeaponClassType weaponClassID = AliasToWeaponClass( weaponName );
		if ( weaponID != WEAPON_NONE )
		{
			weaponClassID = WEAPONCLASS_NONE;
		}
		bool isWeaponID = ( isPrimary ) ? IsPrimaryWeaponID( weaponID ) : IsSecondaryWeaponID( weaponID );
		bool isWeaponClassID = ( isPrimary ) ? IsPrimaryWeaponClassID( weaponClassID ) : IsSecondaryWeaponClassID( weaponClassID );
		if ( task->IsComplete() )
			continue;
		if ( !isWeaponClassID && !isWeaponID )
			continue;

		if ( CurrentWeaponMatches( currentClientID, weaponID, weaponClassID ) )
		{
			if ( isWeaponClassID )
			{
				return CareerWeaponListOrganized( CareerWeaponListForClass( weaponClassID ), source );
			}

			BuyPresetWeaponList weapons = source;
			weapons.insert( weapons.begin(), BuyPresetWeapon( weaponID ) );
			return weapons;
		}
		taskVector.push_back( task );
	}

	BuyPresetWeaponList weapons;
	for ( it = taskVector.begin(); it != taskVector.end(); ++it )
	{
		ICareerTask *task = *it;
		const char *weaponName = task->GetWeaponName();
		WeaponIdType weaponID = AliasToWeaponID( weaponName );
		WeaponClassType weaponClassID = AliasToWeaponClass( weaponName );
		if ( weaponID != WEAPON_NONE )
		{
			weapons.push_back( BuyPresetWeapon( weaponID ) );
		}
		else if ( weaponClassID != WEAPONCLASS_NONE )
		{
			BuyPresetWeaponList classWeapons = CareerWeaponListForClass( weaponClassID );
			for ( int i = 0; i < (int)classWeapons.size(); ++i )
			{
				weapons.push_back( classWeapons[i] );
			}
		}
	}

	return CareerWeaponListOrganized( weapons, source );
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Fills the ammo array with the ammo currently owned by the local player
 */
void FillClientAmmo( int *ammo )
{
	int i;
	for ( i = 0; i < AMMO_MAX_TYPES; ++i )
	{
		ammo[i] = 0;
	}

	for ( i = 0; i <= ID_MAX; ++i )
	{
		WeaponIdType gameWeaponID = ClientWeaponIDToGameWeaponID( (ClientWeaponIdType)i );
		if ( gameWeaponID == WEAPON_NONE )
			continue;

		const WeaponInfoStruct *info = GetWeaponInfo( gameWeaponID );
		int clientAmmoType = gWR.GetWeapon( i )->iAmmoType;
		int clientAmmoCount = gWR.CountAmmo( clientAmmoType );
		if ( clientAmmoCount > 0 && info )
		{
			ammo[info->ammoType] = max( ammo[info->ammoType], clientAmmoCount );
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
ClientWeaponIdType GetClientWeaponID( bool primary )
{
	int i;
	for ( i = 0; i <= ID_MAX; ++i )
	{
		int iSlot = gWR.GetWeapon( i )->iSlot;
		int iSlotPos = gWR.GetWeapon( i )->iSlotPos;
		WEAPON *pWeapon = gWR.GetWeaponSlot( iSlot, iSlotPos );
		if ( pWeapon )
		{
			if ( primary )
			{
				switch ( i )
				{
				case ID_SHIELD:
				case ID_SCOUT:
				case ID_XM1014:
				case ID_MAC10:
				case ID_AUG:
				case ID_UMP45:
				case ID_SG550:
				case ID_GALIL:
				case ID_FAMAS:
				case ID_AWP:
				case ID_MP5N:
				case ID_M249:
				case ID_M3:
				case ID_M4A1:
				case ID_TMP:
				case ID_G3SG1:
				case ID_SG552:
				case ID_AK47:
				case ID_P90:
					return (ClientWeaponIdType)i;
				}
			}
			else
			{
				switch ( i )
				{
				case ID_P228:
				case ID_ELITES:
				case ID_FIVESEVEN:
				case ID_USP:
				case ID_GLOCK18:
				case ID_DEAGLE:
					return (ClientWeaponIdType)i;
				}
			}
		}
	}
	return ID_NONE;
}

//--------------------------------------------------------------------------------------------------------------
