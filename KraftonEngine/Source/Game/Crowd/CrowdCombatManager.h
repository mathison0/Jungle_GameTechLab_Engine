#pragma once

#include "Game/Crowd/CrowdSpatialPartition.h"
#include "Game/Crowd/CrowdUnitStore.h"

class AMusouGameMode;
class APawn;
struct FMusouAttackEvent;

struct FCrowdCombatSettings
{
	float HitStateDuration = 0.18f;
	float KnockDownStateDuration = 0.65f;
	float DeadStateDuration = 1.0f;
	float KnockDownMinKnockbackDistance = 4.0f;
};

class FCrowdCombatManager
{
public:
	void ClearDamageEvents();

	void ApplyRadialDamage(
		const FVector& Center,
		float Radius,
		float Damage,
		EUnitTeam TargetTeam,
		const FCrowdUnitStore& UnitStore,
		const FCrowdSpatialPartition& SpatialPartition);

	void HandleAttackEvent(
		const FMusouAttackEvent& Event,
		const FCrowdUnitStore& UnitStore,
		const FCrowdSpatialPartition& SpatialPartition,
		AMusouGameMode* GameMode);

	void UpdateStateTimers(
		float DeltaTime,
		FCrowdUnitStore& UnitStore,
		TArray<FUnitHandle>& OutRemovedHandles);

	void UpdateCombat(float DeltaTime, FCrowdUnitStore& UnitStore, APawn* PlayerPawn, float PlayerProxyRadius);
	void ProcessDamageEvents(
		FCrowdUnitStore& UnitStore,
		AMusouGameMode* GameMode,
		const FCrowdCombatSettings& Settings,
		TArray<FUnitHandle>& OutRemovedHandles);

private:
	TArray<FDamageEvent> DamageEvents;
};
