#include "Game/Crowd/CrowdUnitStore.h"

#include <utility>

FUnitHandle FCrowdUnitStore::AllocateUnitSlot()
{
	uint32 Index = UINT32_MAX;

	if (!FreeUnitIndices.empty())
	{
		Index = FreeUnitIndices.back();
		FreeUnitIndices.pop_back();
	}
	else
	{
		Index = static_cast<uint32>(Units.size());
		Units.emplace_back();
		Units.back().Generation = 1;
	}

	FCrowdUnit& Unit = Units[Index];
	if (Unit.Generation == 0)
	{
		Unit.Generation = 1;
	}

	return FUnitHandle{ Index, Unit.Generation };
}

void FCrowdUnitStore::QueueSpawn(FUnitHandle Handle, EUnitTeam Team, const FUnitArchetype& Archetype, const FVector& Position)
{
	PendingSpawns.push_back({ Handle, Team, Archetype, Position });
}

void FCrowdUnitStore::ActivateUnit(
	FUnitHandle Handle,
	EUnitTeam Team,
	const FVector& Position,
	const FUnitArchetype& Archetype,
	float ThinkInterval,
	float AttackCooldownRemaining)
{
	if (Handle.Index >= Units.size())
	{
		return;
	}

	FCrowdUnit& Unit = Units[Handle.Index];
	Unit = FCrowdUnit();
	Unit.Generation = Handle.Generation != 0 ? Handle.Generation : 1;
	Unit.bAlive = true;
	Unit.Team = Team;
	Unit.State = EUnitState::Idle;
	Unit.Archetype = Archetype;
	Unit.Position = Position;
	Unit.SpawnZ = Position.Z;
	Unit.HP = Archetype.MaxHP;
	Unit.Radius = Archetype.Radius;
	Unit.ThinkTimer = ThinkInterval;
	Unit.AttackCooldownRemaining = AttackCooldownRemaining;
}

void FCrowdUnitStore::QueueDespawn(FUnitHandle Handle)
{
	if (Handle.IsValid())
	{
		PendingDespawns.push_back(Handle);
	}
}

bool FCrowdUnitStore::RemoveUnit(FUnitHandle Handle)
{
	if (!IsValidUnitHandle(Handle))
	{
		return false;
	}

	FCrowdUnit& Unit = Units[Handle.Index];
	Unit.bAlive = false;
	Unit.State = EUnitState::Dead;
	Unit.Target = {};
	Unit.Generation++;
	if (Unit.Generation == 0)
	{
		Unit.Generation = 1;
	}

	FreeUnitIndices.push_back(Handle.Index);
	return true;
}

void FCrowdUnitStore::Clear()
{
	Units.clear();
	FreeUnitIndices.clear();
	PendingSpawns.clear();
	PendingDespawns.clear();
}

void FCrowdUnitStore::FlushPendingSpawns(const FActivateSpawnFunc& ActivateSpawn)
{
	if (PendingSpawns.empty())
	{
		return;
	}

	TArray<FPendingSpawn> Spawns = std::move(PendingSpawns);
	PendingSpawns.clear();

	for (const FPendingSpawn& Spawn : Spawns)
	{
		if (ActivateSpawn)
		{
			ActivateSpawn(Spawn.Handle, Spawn.Team, Spawn.Archetype, Spawn.Position);
		}
	}
}

void FCrowdUnitStore::FlushPendingDespawns(const FRemoveUnitFunc& RemoveUnitFunc)
{
	if (PendingDespawns.empty())
	{
		return;
	}

	TArray<FUnitHandle> Despawns = std::move(PendingDespawns);
	PendingDespawns.clear();

	for (FUnitHandle Handle : Despawns)
	{
		if (RemoveUnitFunc)
		{
			RemoveUnitFunc(Handle);
		}
	}
}

bool FCrowdUnitStore::IsValidUnitHandle(FUnitHandle Handle) const
{
	return Handle.IsValid()
		&& Handle.Index < Units.size()
		&& Units[Handle.Index].bAlive
		&& Units[Handle.Index].Generation == Handle.Generation;
}

FCrowdUnit* FCrowdUnitStore::ResolveUnit(FUnitHandle Handle)
{
	return IsValidUnitHandle(Handle) ? &Units[Handle.Index] : nullptr;
}

const FCrowdUnit* FCrowdUnitStore::ResolveUnit(FUnitHandle Handle) const
{
	return IsValidUnitHandle(Handle) ? &Units[Handle.Index] : nullptr;
}

int32 FCrowdUnitStore::GetAliveCount() const
{
	int32 Count = 0;
	for (const FCrowdUnit& Unit : Units)
	{
		if (Unit.bAlive)
		{
			++Count;
		}
	}
	return Count;
}

int32 FCrowdUnitStore::GetTeamAliveCount(EUnitTeam Team) const
{
	int32 Count = 0;
	for (const FCrowdUnit& Unit : Units)
	{
		if (Unit.bAlive && Unit.Team == Team)
		{
			++Count;
		}
	}
	return Count;
}

int32 FCrowdUnitStore::GetTeamCombatTypeAliveCount(EUnitTeam Team, EUnitCombatType CombatType) const
{
	int32 Count = 0;
	for (const FCrowdUnit& Unit : Units)
	{
		if (Unit.bAlive && Unit.Team == Team && Unit.Archetype.CombatType == CombatType)
		{
			++Count;
		}
	}
	return Count;
}
