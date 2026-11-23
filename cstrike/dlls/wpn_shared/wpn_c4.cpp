/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
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

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"

#ifndef CLIENT_DLL
#include "bot/bot.h"
#include "career_tasks.h"
#include "GameEvent.h"
#include "hltv.h"

extern int gmsgBombDrop;
void Broadcast( const char *sentence );
#else
extern bool g_bInBombZone;
#endif

enum c4_e
{
	C4_IDLE1 = 0,
	C4_DRAW,
	C4_DROP,
	C4_ARM,
};

LINK_ENTITY_TO_CLASS( weapon_c4, CC4 );

void CC4::Spawn( void )
{
	SET_MODEL( ENT( pev ), "models/w_backpack.mdl" );

	pev->frame = 0;
	pev->body = 3;
	pev->sequence = 0;
	pev->framerate = 0;

	m_iId = WEAPON_C4;
	m_iDefaultAmmo = 1;

	m_bStartedArming = false;
	m_fArmedTime = 0;

	if ( pev->targetname )
	{
		pev->effects |= EF_NODRAW;
		DROP_TO_FLOOR( ENT( pev ) );
		return;
	}

	FallInit();
	SetThink( &CBasePlayerItem::FallThink );

	pev->nextthink = 0.1f;
}

void CC4::Precache( void )
{
	PRECACHE_MODEL( "models/v_c4.mdl" );
	PRECACHE_MODEL( "models/w_backpack.mdl" );

	PRECACHE_SOUND( "weapons/c4_click.wav" );
}

int CC4::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "C4";
	p->iMaxAmmo1 = 1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 4;
	p->iPosition = 3;
	p->iId = m_iId = WEAPON_C4;
	p->iWeight = 3;
	p->iFlags = ITEM_FLAG_LIMITINWORLD | ITEM_FLAG_EXHAUSTIBLE;

	return 1;
}

BOOL CC4::Deploy( void )
{
	pev->body = 0;
	m_bStartedArming = false;
	m_fArmedTime = 0;

	if ( m_pPlayer->HasShield() )
	{
		m_bHasShield = true;
		m_pPlayer->pev->gamestate = 1;
	}

	return DefaultDeploy( "models/v_c4.mdl", "models/p_c4.mdl", C4_DRAW, "c4", UseDecrement() ? 1 : 0 );
}


void CC4::Holster( int skiplocal /* = 0 */ )
{
	m_pPlayer->m_flNextAttack = 0.5f;

	m_bStartedArming = false;

	if ( !m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
	{
		m_pPlayer->pev->weapons &= ~( 1 << WEAPON_C4 );
		DestroyItem();
	}

	if ( m_bHasShield )
	{
		m_pPlayer->pev->gamestate = 0;
		m_bHasShield = false;
	}
}


void CC4::PrimaryAttack( void )
{
	BOOL PlaceBomb = FALSE;

	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
		return;

	int onGround = ( m_pPlayer->pev->flags & FL_ONGROUND );

#ifndef CLIENT_DLL
	int inBombZone = ( m_pPlayer->m_signals.GetState() & SIGNAL_BOMB );
#else
	int inBombZone = g_bInBombZone;
#endif

	if ( !m_bStartedArming )
	{
		if ( inBombZone && onGround )
		{
			m_bStartedArming = true;
			m_fArmedTime = gpGlobals->time + 3.0;
			m_bBombPlacedAnimation = false;
			SendWeaponAnim( C4_ARM, UseDecrement() ? 1 : 0 );

#ifndef CLIENT_DLL
			// freeze the player in place while planting
			g_engfuncs.pfnSetClientMaxspeed( m_pPlayer->edict(), 1 );

			// player "arming bomb" animation
			m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

			m_pPlayer->SetProgressBarTime( 3 );
#endif
		}
		else
		{
			if ( !inBombZone )
			{
				ClientPrint( m_pPlayer->pev, HUD_PRINTCENTER, "#C4_Plant_At_Bomb_Spot" );
			}
			else
			{
				ClientPrint( m_pPlayer->pev, HUD_PRINTCENTER, "#C4_Plant_Must_Be_On_Ground" );
			}

			m_flNextPrimaryAttack = GetNextAttackDelay( 1 );
			return;
		}
	}
	else
	{
		if ( !onGround || !inBombZone )
		{
			if ( !inBombZone )
			{
				ClientPrint( m_pPlayer->pev, HUD_PRINTCENTER, "#C4_Arming_Cancelled" );
			}
			else
			{
				ClientPrint( m_pPlayer->pev, HUD_PRINTCENTER, "#C4_Plant_Must_Be_On_Ground" );
			}

			m_flNextPrimaryAttack = GetNextAttackDelay( 1.5f );
			m_bStartedArming = false;

#ifndef CLIENT_DLL
			// release the player from being frozen, we've somehow left the bomb zone
			m_pPlayer->ResetMaxSpeed();

			m_pPlayer->SetProgressBarTime( 0 );

			m_pPlayer->SetAnimation( PLAYER_HOLDBOMB );
#endif

			if ( m_bBombPlacedAnimation ) //this means the placement animation is canceled
			{
				SendWeaponAnim( C4_DRAW, UseDecrement() ? 1 : 0 );
			}
			else
			{
				SendWeaponAnim( C4_IDLE1, UseDecrement() ? 1 : 0 );
			}

			return;
		}
		else
		{
			if ( gpGlobals->time >= m_fArmedTime ) //the c4 is ready to be armed
			{
				PlaceBomb = TRUE;
			}
			else if ( gpGlobals->time >= m_fArmedTime - 0.75 && !m_bBombPlacedAnimation )
			{
				//call the c4 Placement animation
				m_bBombPlacedAnimation = true;
				SendWeaponAnim( C4_DROP, UseDecrement() ? 1 : 0 );

#ifndef CLIENT_DLL
				// player "place" animation
				m_pPlayer->SetAnimation( PLAYER_HOLDBOMB );
#endif
			}
		}
	}

	if ( PlaceBomb )
	{
		m_bStartedArming = false;
		m_fArmedTime = 0;

#ifndef CLIENT_DLL
		Broadcast( "BOMBPL" );
		m_pPlayer->m_bHasC4 = false;

		if ( pev->speed != 0 && g_pGameRules )
			( (CHalfLifeMultiplay *)g_pGameRules )->m_iC4Timer = (int)pev->speed;

		CGrenade *pBomb = CGrenade::ShootSatchelCharge( m_pPlayer->pev, m_pPlayer->pev->origin, Vector( 0, 0, 0 ) );

		// send director message, that something important happened here
		MESSAGE_BEGIN( MSG_SPEC, SVC_DIRECTOR );
			WRITE_BYTE( 9 );	// command length in bytes
			WRITE_BYTE( DRC_CMD_EVENT );	// bomb placed
			WRITE_SHORT( ENTINDEX( m_pPlayer->edict() ) );
			WRITE_SHORT( 0 );
			WRITE_LONG( DRC_FLAG_FACEPLAYER | 11 );	// eventflags (priority and flags)
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_ALL, gmsgBombDrop );
			WRITE_COORD( pBomb->pev->origin.x );
			WRITE_COORD( pBomb->pev->origin.y );
			WRITE_COORD( pBomb->pev->origin.z );
			WRITE_BYTE( 1 );	// bomb was planted
		MESSAGE_END();

		UTIL_ClientPrintAll( HUD_PRINTCENTER, "#Bomb_Planted" );

		// tell bots the bomb has been planted
		TheBots->OnEvent( EVENT_BOMB_PLANTED, m_pPlayer, pBomb );

		if ( ( (CHalfLifeMultiplay *)g_pGameRules )->IsCareer() && !m_pPlayer->IsBot() )
		{
			TheCareerTasks->HandleEvent( EVENT_BOMB_PLANTED, m_pPlayer, NULL );
		}

		UTIL_LogPrintf( "\"%s<%i><%s><TERRORIST>\" triggered \"Planted_The_Bomb\"\n",
			STRING( m_pPlayer->pev->netname ),
			GETPLAYERUSERID( m_pPlayer->edict() ),
			GETPLAYERAUTHID( m_pPlayer->edict() ) );

		g_pGameRules->m_bBombDropped = false;

		// Play the plant sound.
		EMIT_SOUND( edict(), CHAN_WEAPON, "weapons/c4_plant.wav", 1, ATTN_NORM );
		m_pPlayer->pev->body = 0;

		// release the player from being frozen
		m_pPlayer->ResetMaxSpeed();

		// Remove the C4 icon from the HUD
		m_pPlayer->SetBombIcon( false );
#endif

		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;

		if ( !m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
		{
			// No more c4!
			RetireWeapon();
			return;
		}
	}

	m_flNextPrimaryAttack = GetNextAttackDelay( 0.3f );
	m_flTimeWeaponIdle = RANDOM_FLOAT( 10, 15 );
}

void CC4::WeaponIdle( void )
{
	if ( m_bStartedArming )
	{
		m_bStartedArming = false; //if the player releases the attack button cancel the arming sequence

#ifndef CLIENT_DLL
		// release the player from being frozen
		m_pPlayer->ResetMaxSpeed();

		m_flNextPrimaryAttack = GetNextAttackDelay( 1 );

		m_pPlayer->SetProgressBarTime( 0 );
#endif

		if ( m_bBombPlacedAnimation ) //this means the placement animation is canceled
			SendWeaponAnim( C4_DRAW, UseDecrement() ? 1 : 0 );
		else
			SendWeaponAnim( C4_IDLE1, UseDecrement() ? 1 : 0 );
	}

	if ( m_flTimeWeaponIdle <= 0 )
	{
		if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] > 0 )
		{
			SendWeaponAnim( C4_DRAW, UseDecrement() ? 1 : 0 );
		}
		else
		{
			RetireWeapon();
			return;
		}

		SendWeaponAnim( C4_IDLE1, UseDecrement() ? 1 : 0 );
		m_flTimeWeaponIdle = 20;
	}
}

void CC4::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "detonatedelay" ) )
	{
		pev->speed = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "detonatetarget" ) )
	{
		pev->noise1 = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "defusetarget" ) )
	{
		pev->target = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CC4::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( m_pPlayer )
		return;
	CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( 1 );
	if ( !pPlayer )
		return;
	edict_t *m_pentOldCurBombTarget = pPlayer->m_pentCurBombTarget;
	pPlayer->m_pentCurBombTarget = NULL;

	if ( pev->speed != 0 && g_pGameRules )
		( (CHalfLifeMultiplay *)g_pGameRules )->m_iC4Timer = (int)pev->speed;

	EMIT_SOUND( edict(), CHAN_WEAPON, "weapons/c4_plant.wav", 1, ATTN_NORM );
	CGrenade::ShootSatchelCharge( pPlayer->pev, pev->origin, Vector( 0, 0, 0 ) );

	CGrenade *pC4 = (CGrenade *)UTIL_FindEntityByClassname( NULL, "grenade" );
	while ( pC4 && !( pC4->m_bIsC4 && pC4->m_flNextFreq == gpGlobals->time ) )
		pC4 = (CGrenade *)UTIL_FindEntityByClassname( pC4, "grenade" );
	if ( pC4 )
	{
		pC4->pev->target = pev->target;
		pC4->pev->noise1 = pev->noise1;
	}

	pPlayer->m_pentCurBombTarget = m_pentOldCurBombTarget;

	SUB_Remove();
}
