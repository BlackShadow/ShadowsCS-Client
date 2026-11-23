#include "hostage/hostage.h"
#include "animation.h"

void HostageAnimateState::Reset( void )
{
	m_sequenceCount = 0;
	m_performance = None;
}


void HostageAnimateState::StartSequence( CHostageImprov *improv, const SeqInfo *seqInfo )
{
	if ( seqInfo->seqID >= 0 )
	{
		CHostage *hostage = improv->GetEntity();

		hostage->pev->sequence = seqInfo->seqID;
		hostage->ResetSequenceInfo();

		hostage->m_fSequenceLoops = seqInfo->holdTime > 0;

		hostage->pev->frame = seqInfo->rate >= 0 ? 0 : 255;

		hostage->pev->framerate *= seqInfo->rate;
	}

	m_isHolding = false;
}


void HostageAnimateState::AddSequence( CHostageImprov *improv, const char *seqName, float holdTime, float rate )
{
	if ( m_sequenceCount >= MAX_SEQUENCES )
		return;

	CHostage *hostage = improv->GetEntity();

	m_sequence[m_sequenceCount].seqID = seqName ? hostage->LookupSequence( seqName ) : -1;
	m_sequence[m_sequenceCount].holdTime = holdTime;
	m_sequence[m_sequenceCount].rate = rate;
	++m_sequenceCount;

	m_currentSequence = 0;
	StartSequence( improv, &m_sequence[0] );
}


void HostageAnimateState::AddSequence( CHostageImprov *improv, int activity, float holdTime, float rate )
{
	CHostage *hostage = improv->GetEntity();

	void *model = GET_MODEL_PTR( hostage->edict() );
	if ( !model )
		return;

	m_sequence[m_sequenceCount].seqID = LookupActivity( model, hostage->pev, activity );
	m_sequence[m_sequenceCount].holdTime = holdTime;
	m_sequence[m_sequenceCount].rate = rate;
	++m_sequenceCount;

	m_currentSequence = 0;
	StartSequence( improv, &m_sequence[0] );
}


bool HostageAnimateState::IsDoneHolding( void )
{
	if ( m_sequence[m_currentSequence].holdTime < 0 )
		return true;

	if ( m_isHolding )
	{
		if ( m_holdTimer.IsElapsed() )
			return true;
	}
	else
	{
		m_isHolding = true;
		m_holdTimer.Start( m_sequence[m_currentSequence].holdTime );
	}

	return false;
}


void HostageAnimateState::OnEnter( CHostageImprov *improv )
{
}


void HostageAnimateState::OnUpdate( CHostageImprov *improv )
{
	if ( !m_sequenceCount )
		return;

	CHostage *hostage = improv->GetEntity();

	if ( !hostage->m_fSequenceFinished && m_sequence[m_currentSequence].seqID >= 0 )
		return;

	if ( IsDoneHolding() )
	{
		++m_currentSequence;

		if ( m_currentSequence == m_sequenceCount )
		{
			m_sequenceCount = 0;

			m_performance = None;
		}
		else
		{
			StartSequence( improv, &m_sequence[m_currentSequence] );
		}
	}
}


void HostageAnimateState::OnExit( CHostageImprov *improv )
{
}


bool HostageAnimateState::IsPlaying( CHostageImprov *improv, const char *seqName ) const
{
	if ( !m_sequenceCount )
		return false;

	CHostage *hostage = improv->GetEntity();

	int id = hostage->LookupSequence( seqName );

	return m_sequence[m_currentSequence].seqID == id;
}
