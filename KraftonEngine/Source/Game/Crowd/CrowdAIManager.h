#pragma once

#include "Game/Crowd/CrowdSpatialPartition.h"
#include "Game/Crowd/CrowdUnitStore.h"

class FCrowdAIManager
{
public:
	void Update(
		float DeltaTime,
		FCrowdUnitStore& UnitStore,
		const FCrowdSpatialPartition& SpatialPartition,
		const TFunction<float()>& RandomThinkInterval) const;

private:
	static bool IsHostile(EUnitTeam A, EUnitTeam B) { return A != B; }
	FUnitHandle FindNearestHostile(
		const FCrowdUnitStore& UnitStore,
		const FCrowdSpatialPartition& SpatialPartition,
		uint32 UnitIndex,
		float MaxRange) const;
};
