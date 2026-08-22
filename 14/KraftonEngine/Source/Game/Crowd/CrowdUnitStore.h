#pragma once

#include "Game/Crowd/CrowdUnitTypes.h"

class FCrowdUnitStore
{
public:
	using FActivateSpawnFunc = TFunction<void(FUnitHandle, EUnitTeam, const FUnitArchetype&, const FVector&, bool, const FVector&)>;
	using FRemoveUnitFunc = TFunction<void(FUnitHandle)>;

	FUnitHandle AllocateUnitSlot();
	void QueueSpawn(
		FUnitHandle Handle,
		EUnitTeam Team,
		const FUnitArchetype& Archetype,
		const FVector& Position,
		bool bHasInitialMoveGoal = false,
		const FVector& InitialMoveGoal = FVector::ZeroVector);
	void ActivateUnit(
		FUnitHandle Handle,
		EUnitTeam Team,
		const FVector& Position,
		const FUnitArchetype& Archetype,
		float ThinkInterval,
		float AttackCooldownRemaining);

	void QueueDespawn(FUnitHandle Handle);
	bool RemoveUnit(FUnitHandle Handle);
	void Clear();

	void FlushPendingSpawns(const FActivateSpawnFunc& ActivateSpawn);
	void FlushPendingDespawns(const FRemoveUnitFunc& RemoveUnit);

	bool IsValidUnitHandle(FUnitHandle Handle) const;
	bool IsUnitAliveForGameplay(FUnitHandle Handle) const;
	bool IsUnitCombatActive(FUnitHandle Handle) const;
	FCrowdUnit* ResolveUnit(FUnitHandle Handle);
	const FCrowdUnit* ResolveUnit(FUnitHandle Handle) const;

	int32 GetAliveCount() const;
	int32 GetTeamAliveCount(EUnitTeam Team) const;
	int32 GetTeamCombatTypeAliveCount(EUnitTeam Team, EUnitCombatType CombatType) const;

	TArray<FCrowdUnit>& GetUnits() { return Units; }
	const TArray<FCrowdUnit>& GetUnits() const { return Units; }

private:
	struct FPendingSpawn
	{
		FUnitHandle Handle;
		EUnitTeam Team = EUnitTeam::Enemy;
		FUnitArchetype Archetype;
		FVector Position = FVector::ZeroVector;
		bool bHasInitialMoveGoal = false;
		FVector InitialMoveGoal = FVector::ZeroVector;
	};

private:
	TArray<FCrowdUnit> Units;
	TArray<uint32> FreeUnitIndices;
	TArray<FPendingSpawn> PendingSpawns;
	TArray<FUnitHandle> PendingDespawns;
};
