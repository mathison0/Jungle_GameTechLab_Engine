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
	bool bEnableChaseBlockedSideStep = true;
	float ChaseBlockedSideStepSpeedScale = 0.65f;
	bool bTreatFriendlyChaseBlockersAsSoft = true;
	float FriendlyChaseBlockScoreScale = 0.25f;
	float CircleAroundSpeedScale = 0.75f;
	float CircleAroundRadiusTolerance = 0.75f;
	float CircleAroundRadialCorrectionWeight = 0.65f;
	bool bEnableAttackSeparation = true;
	float AttackSeparationSpeedScale = 0.35f;
	float SeparationDeadZone = 0.04f;
	float SeparationOnlySpeedScale = 0.35f;
	bool bEnableSeparationVelocitySmoothing = true;
	float SeparationVelocityBlendSpeed = 10.0f;
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

	// 띄우기(launcher) 공중 중력 (m/s²) — 플레이어 공중 콤보와 겹치도록 실제 중력보다
	// 약하게 잡아 체공을 늘린다. FCrowdUnit::bAirborne 동안 Z 포물선에 사용.
	float LaunchGravity = 12.0f;
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
