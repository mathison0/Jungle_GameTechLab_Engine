#pragma once

#include "Game/Crowd/CrowdUnitStore.h"

struct FCrowdEngagementSettings
{
	bool bEnablePlayerEngagement = true;
	float PlayerEngagementRadius = 18.0f;
	float PlayerEngagementExitHysteresis = 2.0f;
	float PlayerProxyRadius = 0.6f;
	int32 MeleeCombatSlotCount = 8;
	int32 RangedCombatSlotCount = 8;
	float MeleeSlotRadius = 2.2f;
	float RangedSlotRadius = 7.5f;
	int32 MeleeAttackTokenCount = 2;
	int32 RangedAttackTokenCount = 1;
	float SlotArriveTolerance = 0.5f;
};

class FCrowdEngagementManager
{
public:
	void Update(
		FCrowdUnitStore& UnitStore,
		const FCrowdEngagementSettings& Settings,
		const FVector& PlayerLocation,
		bool bHasPlayer) const;

private:
	void ResetPlayerEngagement(FCrowdUnit& Unit) const;
	void AssignCombatTypeSlots(
		TArray<FCrowdUnit>& Units,
		TArray<uint32>& CandidateIndices,
		const FVector& PlayerLocation,
		int32 SlotCount,
		float SlotRadius,
		int32 AttackTokenCount,
		float SlotAngleOffset) const;
};
