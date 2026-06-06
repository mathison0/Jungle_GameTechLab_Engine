#pragma once

#include "Game/Crowd/CrowdSpatialPartition.h"
#include "Game/Crowd/CrowdUnitStore.h"

class AMusouGameMode;
struct FMusouAttackEvent;

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

	void UpdateCombat(float DeltaTime, FCrowdUnitStore& UnitStore);
	void ProcessDamageEvents(FCrowdUnitStore& UnitStore, AMusouGameMode* GameMode, TArray<FUnitHandle>& OutRemovedHandles);

private:
	TArray<FDamageEvent> DamageEvents;
};
