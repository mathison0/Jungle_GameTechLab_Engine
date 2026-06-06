#include "Game/Crowd/CrowdAIManager.h"

#include <algorithm>

namespace
{
	float DistanceSquaredXY(const FVector& A, const FVector& B)
	{
		const float DX = A.X - B.X;
		const float DY = A.Y - B.Y;
		return DX * DX + DY * DY;
	}
}

void FCrowdAIManager::Update(
	float DeltaTime,
	FCrowdUnitStore& UnitStore,
	const FCrowdSpatialPartition& SpatialPartition,
	const TFunction<float()>& RandomThinkInterval) const
{
	TArray<FCrowdUnit>& Units = UnitStore.GetUnits();
	for (uint32 Index = 0; Index < static_cast<uint32>(Units.size()); ++Index)
	{
		FCrowdUnit& Unit = Units[Index];
		if (!Unit.bAlive)
		{
			continue;
		}

		Unit.ThinkTimer -= DeltaTime;
		if (Unit.ThinkTimer > 0.0f)
		{
			continue;
		}

		Unit.ThinkTimer = RandomThinkInterval ? RandomThinkInterval() : 0.1f;

		const FCrowdUnit* Target = UnitStore.ResolveUnit(Unit.Target);
		if (Target && !IsHostile(Unit.Team, Target->Team))
		{
			Target = nullptr;
			Unit.Target = {};
		}

		if (!Target)
		{
			Unit.Target = FindNearestHostile(UnitStore, SpatialPartition, Index, Unit.Archetype.DetectRange);
			Target = UnitStore.ResolveUnit(Unit.Target);
		}

		if (!Target)
		{
			Unit.State = EUnitState::Idle;
			continue;
		}

		const float AttackRange = (std::max)(Unit.Archetype.AttackRange + Unit.Radius + Target->Radius, 0.0f);
		const bool bInAttackRange = DistanceSquaredXY(Unit.Position, Target->Position) <= AttackRange * AttackRange;
		Unit.State = bInAttackRange ? EUnitState::Attack : EUnitState::Chase;
	}
}

FUnitHandle FCrowdAIManager::FindNearestHostile(
	const FCrowdUnitStore& UnitStore,
	const FCrowdSpatialPartition& SpatialPartition,
	uint32 UnitIndex,
	float MaxRange) const
{
	const TArray<FCrowdUnit>& Units = UnitStore.GetUnits();
	if (UnitIndex >= Units.size() || MaxRange <= 0.0f)
	{
		return {};
	}

	const FCrowdUnit& Unit = Units[UnitIndex];
	if (!Unit.bAlive)
	{
		return {};
	}

	TArray<uint32> Candidates;
	SpatialPartition.QueryUnitsInRadius(Units, Unit.Position, MaxRange, Candidates);

	float BestDistanceSq = MaxRange * MaxRange;
	FUnitHandle BestTarget;

	for (uint32 CandidateIndex : Candidates)
	{
		if (CandidateIndex == UnitIndex || CandidateIndex >= Units.size())
		{
			continue;
		}

		const FCrowdUnit& Candidate = Units[CandidateIndex];
		if (!Candidate.bAlive || !IsHostile(Unit.Team, Candidate.Team))
		{
			continue;
		}

		const float DistSq = DistanceSquaredXY(Unit.Position, Candidate.Position);
		if (DistSq < BestDistanceSq)
		{
			BestDistanceSq = DistSq;
			BestTarget = FUnitHandle{ CandidateIndex, Candidate.Generation };
		}
	}

	return BestTarget;
}
