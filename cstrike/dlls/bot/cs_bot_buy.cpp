//========= Copyright (c) 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "cs_bot.h"
#include "cs_bot_manager.h"
#include "gamerules.h"
#include "debug.h"

//--------------------------------------------------------------------------------------------------------------
static bool HasDefaultPistol( CCSBot *me )
{
	CBasePlayerItem *pistol = me->m_rgpPlayerItems[PISTOL_SLOT];

	if ( pistol == NULL )
		return false;

	if ( me->m_iTeam == TERRORIST && pistol->m_iId == WEAPON_GLOCK18 )
		return true;

	if ( me->m_iTeam == CT && pistol->m_iId == WEAPON_USP )
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Buy weapons, armor, etc.
 */
void BuyState::OnEnter( CCSBot *me )
{
	m_retries = 0;
	m_prefRetries = 0;
	m_prefIndex = 0;

	m_doneBuying = false;

	m_isInitialDelay = true;

	me->EquipBestWeapon( false );

	m_buyDefuseKit = false;
	m_buyShield = false;

	if ( me->m_iTeam == CT )
	{
		if ( TheCSBots()->GetScenario() == CCSBotManager::SCENARIO_DEFUSE_BOMB )
		{
			// CT's sometimes buy defuse kits in the bomb scenario (except in career mode, where the player should defuse)
			if ( ((CHalfLifeMultiplay *)g_pGameRules)->IsCareer() == false )
			{
				if ( RANDOM_FLOAT( 0.0f, 100.0f ) < 50.0f )
				{
					m_buyDefuseKit = true;
				}
			}
		}

		// determine if we want a tactical shield
		if ( !me->m_bHasPrimary && cv_bot_allow_shield.value > 0.0f )
		{
			if ( me->m_iAccount > 2500 )
			{
				if ( me->m_iAccount < 4000 )
					m_buyShield = (RANDOM_FLOAT( 0.0f, 100.0f ) < 33.3f) ? true : false;
				else
					m_buyShield = (RANDOM_FLOAT( 0.0f, 100.0f ) < 10.0f) ? true : false;
			}
		}
	}

	if ( cv_bot_allow_grenades.value > 0.0f )
	{
		m_buyGrenade = (RANDOM_FLOAT( 0.0f, 100.0f ) < 33.3f) ? true : false;
	}
	else
	{
		m_buyGrenade = false;
	}

	m_buyPistol = false;
	if ( cv_bot_allow_pistols.value > 0.0f )
	{
		// check if we have a pistol
		if ( me->m_rgpPlayerItems[PISTOL_SLOT] )
		{
			// if we have our default pistol, think about buying a different one
			if ( HasDefaultPistol( me ) )
			{
				// if everything other than pistols is disallowed, buy a pistol
				if ( !(cv_bot_allow_shotguns.value > 0.0f) &&
					!(cv_bot_allow_sub_machine_guns.value > 0.0f) &&
					!(cv_bot_allow_rifles.value > 0.0f) &&
					!(cv_bot_allow_machine_guns.value > 0.0f) &&
					!(cv_bot_allow_shield.value > 0.0f) &&
					!(cv_bot_allow_snipers.value > 0.0f) )
				{
					m_buyPistol = (RANDOM_FLOAT( 0.0f, 100.0f ) < 75.0f);
				}
				else if ( me->m_iAccount < 1000 )
				{
					// if we're low on cash, buy a pistol
					m_buyPistol = (RANDOM_FLOAT( 0.0f, 100.0f ) < 75.0f);
				}
				else
				{
					m_buyPistol = (RANDOM_FLOAT( 0.0f, 100.0f ) < 33.3f);
				}
			}
		}
		else
		{
			// we dont have a pistol - buy one
			m_buyPistol = true;
		}
	}
}

enum WeaponType
{
	PISTOL,
	SHOTGUN,
	SUB_MACHINE_GUN,
	RIFLE,
	MACHINE_GUN,
	SNIPER_RIFLE,
	GRENADE,

	NUM_WEAPON_TYPES
};

struct BuyInfo
{
	WeaponType type;
	bool preferred;			///< more challenging bots prefer these weapons
	const char *buyAlias;	///< the buy alias for this equipment
};

#define PRIMARY_WEAPON_BUY_COUNT 13
#define SECONDARY_WEAPON_BUY_COUNT 3

/**
 * These tables MUST be kept in sync with the CT and T buy aliases
 */

static const BuyInfo primaryWeaponBuyInfoCT[ PRIMARY_WEAPON_BUY_COUNT ] =
{
	{ SHOTGUN,			false, "m3" },			// WEAPON_M3
	{ SHOTGUN,			false, "xm1014" },		// WEAPON_XM1014
	{ SUB_MACHINE_GUN,	false, "tmp" },			// WEAPON_TMP
	{ SUB_MACHINE_GUN,	false, "mp5" },			// WEAPON_MP5N
	{ SUB_MACHINE_GUN,	false, "ump45" },		// WEAPON_UMP45
	{ SUB_MACHINE_GUN,	false, "p90" },			// WEAPON_P90
	{ RIFLE,			true,  "famas" },		// WEAPON_FAMAS
	{ SNIPER_RIFLE,		false, "scout" },		// WEAPON_SCOUT
	{ RIFLE,			true,  "m4a1" },		// WEAPON_M4A1
	{ RIFLE,			false, "aug" },			// WEAPON_AUG
	{ SNIPER_RIFLE,		true,  "sg550" },		// WEAPON_SG550
	{ SNIPER_RIFLE,		true,  "awp" },			// WEAPON_AWP
	{ MACHINE_GUN,		false, "m249" }			// WEAPON_M249
};

static const BuyInfo secondaryWeaponBuyInfoCT[ SECONDARY_WEAPON_BUY_COUNT ] =
{
//	{ PISTOL,	false,	"glock" },
//	{ PISTOL,	false,	"usp" },
	{ PISTOL, true,		"p228" },
	{ PISTOL, true,		"deagle" },
	{ PISTOL, true,		"fn57" }
};


static const BuyInfo primaryWeaponBuyInfoT[ PRIMARY_WEAPON_BUY_COUNT ] =
{
	{ SHOTGUN,			false, "m3" },			// WEAPON_M3
	{ SHOTGUN,			false, "xm1014" },		// WEAPON_XM1014
	{ SUB_MACHINE_GUN,	false, "mac10" },		// WEAPON_MAC10
	{ SUB_MACHINE_GUN,	false, "mp5" },			// WEAPON_MP5N
	{ SUB_MACHINE_GUN,	false, "ump45" },		// WEAPON_UMP45
	{ SUB_MACHINE_GUN,	false, "p90" },			// WEAPON_P90
	{ RIFLE,			true,  "galil" },		// WEAPON_GALIL
	{ RIFLE,			true,  "ak47" },		// WEAPON_AK47
	{ SNIPER_RIFLE,		false, "scout" },		// WEAPON_SCOUT
	{ RIFLE,			true,  "sg552" },		// WEAPON_SG552
	{ SNIPER_RIFLE,		true,  "awp" },			// WEAPON_AWP
	{ SNIPER_RIFLE,		true,  "g3sg1" },		// WEAPON_G3SG1
	{ MACHINE_GUN,		false, "m249" }			// WEAPON_M249
};

static const BuyInfo secondaryWeaponBuyInfoT[ SECONDARY_WEAPON_BUY_COUNT ] =
{
//	{ PISTOL,	false,	"glock" },
//	{ PISTOL,	false,	"usp" },
	{ PISTOL, true,		"p228" },
	{ PISTOL, true,		"deagle" },
	{ PISTOL, true,		"elites" }
};

/**
 * Given a weapon alias, return the kind of weapon it is
 */
inline WeaponType GetWeaponType( const char *alias )
{
	int i;

	for ( i = 0; i < PRIMARY_WEAPON_BUY_COUNT; ++i )
	{
		if ( !stricmp( alias, primaryWeaponBuyInfoCT[i].buyAlias ) )
			return primaryWeaponBuyInfoCT[i].type;

		if ( !stricmp( alias, primaryWeaponBuyInfoT[i].buyAlias ) )
			return primaryWeaponBuyInfoT[i].type;
	}

	for ( i = 0; i < SECONDARY_WEAPON_BUY_COUNT; ++i )
	{
		if ( !stricmp( alias, secondaryWeaponBuyInfoCT[i].buyAlias ) )
			return secondaryWeaponBuyInfoCT[i].type;

		if ( !stricmp( alias, secondaryWeaponBuyInfoT[i].buyAlias ) )
			return secondaryWeaponBuyInfoT[i].type;
	}

	return NUM_WEAPON_TYPES;
}

//--------------------------------------------------------------------------------------------------------------
void BuyState::OnUpdate( CCSBot *me )
{
	// wait for a Navigation Mesh
	if ( TheNavAreaList.empty() )
		return;

	// apparently we cant buy things in the first few seconds, so wait a bit
	if ( m_isInitialDelay )
	{
		const float waitToBuyTime = 2.0f;
		if ( gpGlobals->time - me->m_stateTimestamp < waitToBuyTime )
			return;

		m_isInitialDelay = false;
	}

	// if we're done buying and still in the freeze period, wait
	if ( m_doneBuying )
	{
		if ( g_pGameRules->IsMultiplayer() && g_pGameRules->IsFreezePeriod() )
			return;

		me->Idle();
	}

	if ( !(me->m_signals.GetState() & SIGNAL_BUY) )
	{
		m_doneBuying = true;
		UTIL_DPrintf( "%s bot spawned outside of a buy zone (%d, %d, %d)\n",
						(me->m_iTeam == CT) ? "CT" : "Terrorist",
						(int)me->pev->origin.x,
						(int)me->pev->origin.y,
						(int)me->pev->origin.z );
		return;
	}

	// try to buy some weapons
	const float buyInterval = 0.2f;
	if ( gpGlobals->time - me->m_stateTimestamp > buyInterval )
	{
		me->m_stateTimestamp = gpGlobals->time;

		bool isPreferredAllDisallowed = true;

		// try to buy our preferred weapons first
		if ( m_prefIndex < me->GetProfile()->GetWeaponPreferenceCount() )
		{
			// need to retry because sometimes first buy fails??
			const int maxPrefRetries = 2;
			if ( m_prefRetries >= maxPrefRetries )
			{
				// try to buy next preferred weapon
				++m_prefIndex;
				m_prefRetries = 0;
				return;
			}

			int weaponPreference = me->GetProfile()->GetWeaponPreference( m_prefIndex );

			// don't buy it again if we still have one from last round
			if ( me->m_pActiveItem && me->m_pActiveItem->m_iId == weaponPreference )
			{
				// done with buying preferred weapon
				m_prefIndex = 9999;
				return;
			}

			if ( me->HasShield() && weaponPreference == WEAPON_SHIELDGUN )
			{
				// done with buying preferred weapon
				m_prefIndex = 9999;
				return;
			}

			const char *buyAlias = NULL;

			if ( weaponPreference == WEAPON_SHIELDGUN )
			{
				if ( cv_bot_allow_shield.value > 0.0f )
					buyAlias = "shield";
			}
			else
			{
				buyAlias = WeaponIDToAlias( weaponPreference );
				WeaponType type = GetWeaponType( buyAlias );
				switch ( type )
				{
					case PISTOL:
						if ( !(cv_bot_allow_pistols.value > 0.0f) )
							buyAlias = NULL;
						break;

					case SHOTGUN:
						if ( !(cv_bot_allow_shotguns.value > 0.0f) )
							buyAlias = NULL;
						break;

					case SUB_MACHINE_GUN:
						if ( !(cv_bot_allow_sub_machine_guns.value > 0.0f) )
							buyAlias = NULL;
						break;

					case RIFLE:
						if ( !(cv_bot_allow_rifles.value > 0.0f) )
							buyAlias = NULL;
						break;

					case MACHINE_GUN:
						if ( !(cv_bot_allow_machine_guns.value > 0.0f) )
							buyAlias = NULL;
						break;

					case SNIPER_RIFLE:
						if ( !(cv_bot_allow_snipers.value > 0.0f) )
							buyAlias = NULL;
						break;
				}
			}

			if ( buyAlias )
			{
				me->ClientCommand( buyAlias );
				me->PrintIfWatched( "Tried to buy preferred weapon %s.\n", buyAlias );
				isPreferredAllDisallowed = false;
			}

			++m_prefRetries;

			// bail out so we dont waste money on other equipment
			// unless everything we prefer has been disallowed, then buy at random
			if ( isPreferredAllDisallowed == false )
				return;
		}

		if ( !me->m_bHasPrimary )
		{
			if ( m_buyShield )
			{
				// buy a shield
				me->ClientCommand( "shield" );
				me->PrintIfWatched( "Tried to buy a shield.\n" );
			}
			else
			{
				// build list of allowable weapons to buy
				const BuyInfo *masterPrimary = (me->m_iTeam == TERRORIST) ? primaryWeaponBuyInfoT : primaryWeaponBuyInfoCT;
				const BuyInfo *stockPrimary[ PRIMARY_WEAPON_BUY_COUNT ];
				int stockPrimaryCount = 0;

				// dont choose sniper rifles as often
				float sniperChance = RANDOM_FLOAT( 0.0f, 100.0f );

				for ( int i = 0; i < PRIMARY_WEAPON_BUY_COUNT; ++i )
				{
					if ( (masterPrimary[i].type == SHOTGUN && cv_bot_allow_shotguns.value > 0.0f) ||
						(masterPrimary[i].type == SUB_MACHINE_GUN && cv_bot_allow_sub_machine_guns.value > 0.0f) ||
						(masterPrimary[i].type == RIFLE && cv_bot_allow_rifles.value > 0.0f) ||
						(masterPrimary[i].type == SNIPER_RIFLE && cv_bot_allow_snipers.value > 0.0f && sniperChance < 50.0f) ||
						(masterPrimary[i].type == MACHINE_GUN && cv_bot_allow_machine_guns.value > 0.0f) )
					{
						stockPrimary[ stockPrimaryCount++ ] = &masterPrimary[i];
					}
				}

				if ( stockPrimaryCount )
				{
					// buy primary weapon if we don't have one
					int which;

					// on hard difficulty levels, bots try to buy preferred weapons on the first pass
					if ( m_retries == 0 && !(cv_bot_difficulty.value < 0.9f) && !(cv_bot_difficulty.value < 1.9f) )
					{
						// count up available preferred weapons
						int prefCount = 0;
						for ( which = 0; which < stockPrimaryCount; ++which )
							if ( stockPrimary[which]->preferred )
								++prefCount;

						if ( prefCount )
						{
							int whichPref = RANDOM_LONG( 0, prefCount - 1 );
							for ( which = 0; which < stockPrimaryCount; ++which )
								if ( stockPrimary[which]->preferred && whichPref-- == 0 )
									break;
						}
						else
						{
							// no preferred weapons available, just pick randomly
							which = RANDOM_LONG( 0, stockPrimaryCount - 1 );
						}
					}
					else
					{
						which = RANDOM_LONG( 0, stockPrimaryCount - 1 );
					}

					me->ClientCommand( stockPrimary[ which ]->buyAlias );
					me->PrintIfWatched( "Tried to buy %s.\n", stockPrimary[ which ]->buyAlias );
				}
			}
		}

		//
		// If we now have a weapon, or have tried for too long, we're done
		//
		if ( me->m_bHasPrimary || m_retries++ > 5 )
		{
			// primary ammo
			if ( me->m_bHasPrimary )
			{
				me->ClientCommand( "primammo" );
			}

			// buy armor last, to make sure we bought a weapon first
			me->ClientCommand( "vesthelm" );
			me->ClientCommand( "vest" );

			// pistols - if we have no preferred pistol, buy at random
			if ( cv_bot_allow_pistols.value > 0.0f && !me->GetProfile()->HasPistolPreference() )
			{
				if ( m_buyPistol )
				{
					int which = RANDOM_LONG( 0, SECONDARY_WEAPON_BUY_COUNT - 1 );

					const char *what = NULL;

					if ( me->m_iTeam == TERRORIST )
						what = secondaryWeaponBuyInfoT[ which ].buyAlias;
					else
						what = secondaryWeaponBuyInfoCT[ which ].buyAlias;

					me->ClientCommand( what );

					// only buy one pistol
					m_buyPistol = false;
				}

				// make sure we have enough pistol ammo
				me->ClientCommand( "secammo" );
			}

			// buy a grenade if we wish, and we don't already have one
			if ( m_buyGrenade && !me->HasGrenade() )
			{
				if ( UTIL_IsTeamAllBots( me->m_iTeam ) )
				{
					// only allow Flashbangs if everyone on the team is a bot (dont want to blind our friendly humans)
					float rnd = RANDOM_FLOAT( 0.0f, 100.0f );

					if ( rnd < 10.0f )
					{
						me->ClientCommand( "sgren" );	// smoke grenade
					}
					else if ( rnd < 35.0f )
					{
						me->ClientCommand( "flash" );	// flashbang
					}
					else
					{
						me->ClientCommand( "hegren" );	// he grenade
					}
				}
				else
				{
					if ( RANDOM_FLOAT( 0.0f, 100.0f ) < 10.0f )
					{
						me->ClientCommand( "sgren" );	// smoke grenade
					}
					else
					{
						me->ClientCommand( "hegren" );	// he grenade
					}
				}
			}

			if ( m_buyDefuseKit )
			{
				me->ClientCommand( "defuser" );
			}

			m_doneBuying = true;
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
void BuyState::OnExit( CCSBot *me )
{
	me->ResetStuckMonitor();
	me->EquipBestWeapon( false );
}

const char *BuyState::GetName() const
{
	return "Buy";
}
