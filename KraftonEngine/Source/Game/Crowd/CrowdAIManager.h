#pragma once

#include "Game/Crowd/CrowdSpatialPartition.h"
#include "Game/Crowd/CrowdUnitStore.h"

struct FCrowdAISettings
{
	bool bHasPlayerTarget = false;
	FVector PlayerLocation = FVector::ZeroVector;
	float PlayerProxyRadius = 0.6f;
	float SlotArriveTolerance = 0.5f;
	float CircleAroundRadiusTolerance = 0.75f;
	float AttackStateExitHysteresis = 0.35f;
	float CircleAroundStateHysteresis = 0.35f;
};

class FCrowdAIManager
{
public:
	void Update(
		float DeltaTime,
		FCrowdUnitStore& UnitStore,
		const FCrowdSpatialPartition& SpatialPartition,
		const FCrowdAISettings& Settings,
		const TFunction<float()>& RandomThinkInterval) const;

private:
	static bool IsHostile(EUnitTeam A, EUnitTeam B) { return A != B; }
	void UpdatePlayerTargetState(FCrowdUnit& Unit, const FCrowdAISettings& Settings) const;
	FUnitHandle FindNearestHostile(
		const FCrowdUnitStore& UnitStore,
		const FCrowdSpatialPartition& SpatialPartition,
		uint32 UnitIndex,
		float MaxRange) const;
};
