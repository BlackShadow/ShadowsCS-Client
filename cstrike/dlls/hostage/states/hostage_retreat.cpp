#include "hostage/hostage.h"

void HostageRetreatState::OnEnter( CHostageImprov *improv )
{
	improv->Walk();
	improv->MoveTo( improv->GetEntity()->GetHomePosition() );
}


void HostageRetreatState::OnUpdate( CHostageImprov *improv )
{
	if ( improv->IsAtHome() )
	{
		improv->Stop();
		improv->Idle();
		return;
	}

	CBasePlayer *player = improv->GetClosestVisiblePlayer( UNASSIGNED );
	if ( player )
	{
		const float farRange = 400.0f;
		if ( ( player->pev->origin - improv->GetCentroid() ).IsLengthGreaterThan( farRange ) )
		{
			if ( player->m_iTeam == CT )
			{
				if ( !improv->IsScared() )
				{
					improv->Stop();
					improv->Idle();
					return;
				}
			}
		}
		else
		{
			if ( player->m_iTeam == CT )
			{
				improv->Stop();
				improv->Idle();
				return;
			}
		}
	}

	if ( improv->IsScared() && improv->GetScareIntensity() == CHostageImprov::TERRIFIED )
		improv->Run();
	else
		improv->Walk();
}


void HostageRetreatState::OnExit( CHostageImprov *improv )
{
}
