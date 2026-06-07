#pragma once

#include "Game/Crowd/CrowdGroundQuery.h"
#include "Game/Crowd/CrowdSpatialPartition.h"
#include "Game/Crowd/CrowdUnitStore.h"

struct FCrowdMovementSettings
{
	bool bSurfaceFollowingEnabled = true;
	float VisualTurnSpeedDegreesPerSecond = 540.0f;
	bool bWaitWhenChaseBlocked = true;
	float ChaseBlockedProbeDistance = 0.25f;
	float ChaseBlockedClearancePadding = 0.05f;
	float CircleAroundSpeedScale = 0.75f;
	float CircleAroundRadiusTolerance = 0.75f;
	float CircleAroundRadialCorrectionWeight = 0.65f;
	bool bHasPlayerSeparationTarget = false;
	FVector PlayerSeparationLocation = FVector::ZeroVector;
	float PlayerProxyRadius = 0.6f;
	bool bEnablePlayerSeparation = true;
	float PlayerSeparationPadding = 0.15f;
	float PlayerSeparationWeight = 2.0f;
	float GroundTraceUp = 5.0f;
	float GroundTraceDown = 50.0f;
	float GroundHeightOffset = 0.0f;
	int32 GroundMissToleranceFrames = 2;
};

class FCrowdMovementManager
{
public:
	void Update(
		float DeltaTime,
		FCrowdUnitStore& UnitStore,
		const FCrowdSpatialPartition& SpatialPartition,
		FCrowdGroundQuery& GroundQuery,
		const FCrowdMovementSettings& Settings) const;

	void ApplySurfaceFollowing(
		FCrowdUnit& Unit,
		FCrowdGroundQuery& GroundQuery,
		const FCrowdMovementSettings& Settings) const;
};
