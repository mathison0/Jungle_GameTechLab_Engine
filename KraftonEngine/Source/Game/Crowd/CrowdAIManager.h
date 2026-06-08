#pragma once

#include "Game/Crowd/CrowdSpatialPartition.h"
#include "Game/Crowd/CrowdUnitStore.h"

struct FCrowdAISettings
{
	bool bHasPlayerTarget = false;
	FVector PlayerLocation = FVector::ZeroVector;
	FVector PlayerForward = FVector::ForwardVector;
	float PlayerProxyRadius = 0.6f;
	float SlotArriveTolerance = 0.5f;
	float CircleAroundRadiusTolerance = 0.75f;
	float AttackStateExitHysteresis = 0.35f;
	float CircleAroundStateHysteresis = 0.35f;
	bool bEnableAllyFollowPlayer = true;
	float AllyFollowDistance = 3.5f;
	float AllyFollowColumnSpacing = 1.2f;
	float AllyFollowRowSpacing = 1.5f;
	int32 AllyFollowColumnCount = 3;
	float AllyFollowArriveTolerance = 0.5f;
	float AllyFollowResumeDistance = 1.25f;
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
	bool UpdateAllyFollowPlayerState(FCrowdUnit& Unit, const FCrowdAISettings& Settings, int32 FollowSlotIndex) const;
	void UpdatePlayerTargetState(FCrowdUnit& Unit, const FCrowdAISettings& Settings) const;
	FUnitHandle FindNearestHostile(
		const FCrowdUnitStore& UnitStore,
		const FCrowdSpatialPartition& SpatialPartition,
		uint32 UnitIndex,
		float MaxRange) const;
};
