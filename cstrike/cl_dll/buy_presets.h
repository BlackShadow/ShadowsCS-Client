//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose:
//
//=============================================================================

#ifndef BUY_PRESETS_H
#define BUY_PRESETS_H

#include "../dlls/weapontype.h"
#include "wrect.h"
#include "ammo.h"
#include <vector>

class KeyValues;

//--------------------------------------------------------------------------------------------------------------
enum BuyPresetStringSizes
{
	BUY_PRESET_COMMAND_LEN = 256,
	MaxBuyPresetName = 64,
	MaxBuyPresetImageFname = 64
};

enum AmmoSizeType
{
	AMMO_PERCENT,
	AMMO_CLIPS,
	AMMO_ROUNDS
};

enum { NUM_PRESETS = 4 };

//--------------------------------------------------------------------------------------------------------------
/**
 *  A BuyPresetWeapon stores the mp.dll version of the WeaponID, the increment for buying ammo (clips, rounds, etc),
 *  the number of increments to buy, etc.  This is the basic info for buying a weapon that is present in an
 *  item set.
 */
class BuyPresetWeapon
{
public:
	BuyPresetWeapon( void );
	BuyPresetWeapon( WeaponIdType weaponID );
	BuyPresetWeapon &operator=( const BuyPresetWeapon &other );

	const wchar_t *GetName( void ) const { return m_name; }			///< Returns the display name corresponding to the weapon ID
	WeaponIdType GetWeaponID( void ) const { return m_weaponID; }	///< Returns the WEAPON_* weapon ID
	void SetWeaponID( WeaponIdType weaponID );						///< Sets the WEAPON_* weapon ID (and resets ammo, etc)

	void SetAmmoType( AmmoSizeType ammoType ) { m_ammoType = ammoType; }		///< Sets the ammo type - percent (unused), clips, or rounds (unused)
	void SetAmmoAmount( int ammoAmount ) { m_ammoAmount = ammoAmount; }		///< Sets the amount of ammo, in units relevant to the ammo type
	void SetFillAmmo( bool fill ) { m_fillAmmo = fill; }						///< Sets whether the weapon's ammo should be topped off

	AmmoSizeType GetAmmoType( void ) const { return m_ammoType; }	///< Returns ammo type - percent (unused), clips, or rounds (unused)
	int GetAmmoAmount( void ) const { return m_ammoAmount; }		///< Returns the amount of ammo, in units relevant to the ammo type
	bool GetFillAmmo( void ) const { return m_fillAmmo; }			///< Returns true if the weapon's ammo should be topped off

protected:
	const wchar_t	*m_name;
	WeaponIdType	m_weaponID;
	AmmoSizeType	m_ammoType;
	int				m_ammoAmount;
	bool			m_fillAmmo;
};

typedef std::vector<BuyPresetWeapon> BuyPresetWeaponList;

//--------------------------------------------------------------------------------------------------------------
/**
 *  A WeaponSet is part of a buy preset.  Each buy preset is composed of a series of (fallback) WeaponSets.
 *  The WeaponSet contains a snapshot of a set of weapons and equipment that should be bought.
 */
class WeaponSet
{
public:
	WeaponSet( void );

	void GetCurrent( int &cost, WeaponSet &ws, bool withCareer ) const;			///< Generates a WeaponSet containing only items that would be bought right now
	void GetFromScratch( int &cost, WeaponSet &ws, bool full ) const;			///< Generates a WeaponSet containing everything that would be bought from scratch

	void GenerateBuyCommands( char *command ) const;							///< Generates a list of commands to buy the current WeaponSet.

	int FullCost( void ) const;													///< Calculates the full cost of the WeaponSet, including all optional items

	void Reset( void );

	const BuyPresetWeapon &GetPrimaryWeapon( void ) const { return m_primaryWeapon; }
	const BuyPresetWeapon &GetSecondaryWeapon( void ) const { return m_secondaryWeapon; }

	BuyPresetWeapon m_primaryWeapon;
	BuyPresetWeapon m_secondaryWeapon;

	bool	m_isRebuy;

	// Equipment --------------------------------
	int		m_armor;
	bool	m_helmet;
	bool	m_smokeGrenade;
	bool	m_HEGrenade;
	int		m_flashbangs;
	bool	m_defuser;
	bool	m_nightvision;
	int		m_extraCash;

	bool	m_isSecondaryOptional;
	bool	m_isArmorOptional;
	bool	m_isSmokeOptional;
	bool	m_isHEGrenadeOptional;
	bool	m_isFlashbangOptional;
	bool	m_isDefuserOptional;
	bool	m_isNightvisionOptional;
	bool	m_isCashOptional;
};

typedef std::vector<WeaponSet> WeaponSetList;
typedef WeaponSetList::iterator WeaponSetListIt;

//--------------------------------------------------------------------------------------------------------------
/**
 *  The BuyPreset is a complete representation of a buy preset.  Essentially, it consists of the preset name,
 *  whether or not the preset is used for autobuy, and a list of fallback WeaponSets.
 */
class BuyPreset
{
public:
	BuyPreset( void );
	~BuyPreset( void );

	void SetName( const wchar_t *name );
	const wchar_t *GetName( void ) const { return m_name; }

	void Parse( KeyValues *data );								///< Populates data from a KeyValues structure, for loading
	void Save( KeyValues *data );								///< Fills in a KeyValues structure with data, for saving

	int GetNumSets( void ) const { return (int)m_weaponList.size(); }	///< Returns the number of fallback WeaponSets
	const WeaponSet *GetSet( int index ) const;					///< Returns the specified fallback WeaponSet, or NULL if it doesn't exist

	int FullCost( void ) const;									///< Calculates the full cost of the preset, which is exactly the full cost of the first fallback WeaponSet

	// editing functions --------------------
	void DeleteSet( int index );								///< Deletes a fallback
	void SwapSet( int firstIndex, int secondIndex );			///< Switches the order of two fallbacks
	void ReplaceSet( int index, const WeaponSet &weaponSet );	///< Replaces a fallback with the supplied WeaponSet

	bool IsUsedForAutobuy( void ) const { return m_isAutobuy; }
	void SetAutobuy( bool autobuy ) { m_isAutobuy = autobuy; }

private:
	wchar_t m_name[MaxBuyPresetName];
	bool m_isAutobuy;

	WeaponSetList m_weaponList;
};

typedef std::vector<BuyPreset> BuyPresetList;
typedef BuyPresetList::iterator BuyPresetListIt;

//--------------------------------------------------------------------------------------------------------------
/**
 *  The BuyPresetManager singleton manages saving/loading/buying the individual BuyPresets.  It also tracks the
 *  ownership of some items that aren't explicitly known by the client (defuser, nightvision).
 *
 *  Two sets of BuyPresets are maintained - the live set used for purchasing, and a set that is acted upon for
 *  editing.  This provides the basic save changes/abandon changes ability when editing presets.
 */
class BuyPresetManager
{
public:
	BuyPresetManager( void );

	void Reset( void );
	void ResetItems( void ) { m_hasNightvision = false; m_hasDefuser = false; }
	void Save( void );

	void PurchasePreset( int presetIndex, bool withCareer );		///< Generates and sends buy commands to buy a specific preset

	int GetNumPresets( void ) const { return (int)m_presets.size(); }
	const BuyPreset *GetPreset( int index ) const;					///< Returns the specified "live" preset, or NULL if it doesn't exist

	int GetAutobuyPresetIndex( void ) const;

	void SetNightvision( bool value ) { m_hasNightvision = value; }
	void SetDefuser( bool value ) { m_hasDefuser = value; }

	bool HasNightvision( void ) const { return m_hasNightvision; }
	bool HasDefuser( void ) const { return m_hasDefuser; }

	void SetPresets( const BuyPresetList &presets ) { m_presets = presets; }

	void SetEditPresets( const BuyPresetList &presets ) { m_editPresets = presets; }
	int GetNumEditPresets( void ) const { return (int)m_editPresets.size(); }

	BuyPreset *GetEditPreset( int index );							///< Returns the specified editing preset, or NULL if it doesn't exist
	const BuyPresetList &GetEditPresets( void ) const { return m_editPresets; }

	void ResetEditPresets( void ) { m_editPresets = m_presets; }	///< Resets the editing presets to the live presets (e.g. when starting editing)
	void ResetEditToDefaults( void );								///< Loads the default presets into the editing presets (e.g. when hitting the "Use Defaults" button)

	const char *GetRebuyString( void );

private:
	BuyPresetList	m_presets;		///< Current (live) presets
	BuyPresetList	m_editPresets;	///< BuyPresets used when editing

	bool m_hasNightvision;
	bool m_hasDefuser;

	char *m_rebuyString;
};

//--------------------------------------------------------------------------------------------------------------
extern BuyPresetManager *TheBuyPresets;

bool IsASMapType( void );
bool IsDEMapType( void );

ClientWeaponIdType GetClientWeaponID( bool primary );

//--------------------------------------------------------------------------------------------------------------
/**
 *  Functions for controlling the display of the various buy preset editing menus.  Opening one closes any
 *  others open.
 */
enum { REOPEN_YES, REOPEN_NO, REOPEN_DONTCHANGE };					///< Determines if we need to re-open the buy menu when done editing.
void ShowBuyPresetMainMenu( bool runUpdate, int reopenBuyMenu );	///< Open the main preset editing menu
void ShowBuyPresetEditMenu( int presetIndex );						///< Open the menu for editing the list of fallbacks for an individual preset

//--------------------------------------------------------------------------------------------------------------
/**
 *  Returns the image filename for a given WEAPON_* weapon ID.  Primary is specified because WEAPON_NONE returns
 *  separate images for primary and pistol, to facilitate showing an outline version for "Current Weapon".
 */
const char *ImageFnameFromWeaponID( WeaponIdType weaponID, bool isPrimary );

//--------------------------------------------------------------------------------------------------------------
void MaintainBuyPresetInputFocus( void );

//--------------------------------------------------------------------------------------------------------------
/**
 *  Constructs a list of weapons that will satisfy the any unfinished weapon-specific career tasks:
 *   1.  If there are no unfinished career tasks, the source list is returned
 *   2.  If there are unfinished career tasks, all weapons that can be used for the tasks are added
 *       to the front of the list.  This list of career weapons is sorted according to the order of
 *       weapons in the source list, so that if any weapons in the source list can be used for career
 *       tasks, they will be.
 */
BuyPresetWeaponList CareerWeaponList( const BuyPresetWeaponList &source, bool isPrimary, ClientWeaponIdType currentClientID );

//--------------------------------------------------------------------------------------------------------------
/**
 *  Returns the number of clips that will have to be bought for the specified BuyPresetWeapon
 */
int CalcClipsNeeded( const BuyPresetWeapon *pWeapon, const WeaponInfoStruct *pInfo, const int *ammo );

//--------------------------------------------------------------------------------------------------------------
/**
 *  Fills the ammo array with the ammo currently owned by the local player
 */
void FillClientAmmo( int *ammo );

//--------------------------------------------------------------------------------------------------------------
/**
 *  Returns true if the client can currently buy the weapon according to class/gamemode restrictions.  Returns
 *  true also if the player owns the weapon already.
 */
bool CanBuyWeapon( ClientWeaponIdType currentPrimaryID, ClientWeaponIdType currentSecondaryID, WeaponIdType weaponID );

WeaponIdType ClientWeaponIDToGameWeaponID( ClientWeaponIdType id );

//--------------------------------------------------------------------------------------------------------------
/**
 *  Returns the display name for a weapon, based on it's weaponID
 */
const wchar_t *WeaponIDToDisplayName( WeaponIdType weaponID );

//--------------------------------------------------------------------------------------------------------------
// If our weapon is NONE, we want to be able to buy up to this many clips for the current gun we're holding
enum { NUM_CLIPS_FOR_CURRENT = 4 };

#endif // BUY_PRESETS_H
