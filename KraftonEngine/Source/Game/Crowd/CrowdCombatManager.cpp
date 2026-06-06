#include "Game/Crowd/CrowdCombatManager.h"

#include "Game/Musou/Combat/AttackTypes.h"
#include "Game/Musou/GameMode/MusouGameMode.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
	FVector NormalizedXY(const FVector& V)
	{
		const float LenSq = V.X * V.X + V.Y * V.Y;
		if (LenSq <= 1.e-6f)
		{
			return FVector::ZeroVector;
		}

		const float InvLen = 1.0f / std::sqrt(LenSq);
		return FVector(V.X * InvLen, V.Y * InvLen, 0.0f);
	}

	float DistanceSquaredXY(const FVector& A, const FVector& B)
	{
		const float DX = A.X - B.X;
		const float DY = A.Y - B.Y;
		return DX * DX + DY * DY;
	}
}

void FCrowdCombatManager::ClearDamageEvents()
{
	DamageEvents.clear();
}

void FCrowdCombatManager::ApplyRadialDamage(
	const FVector& Center,
	float Radius,
	float Damage,
	EUnitTeam TargetTeam,
	const FCrowdUnitStore& UnitStore,
	const FCrowdSpatialPartition& SpatialPartition)
{
	if (Radius <= 0.0f || Damage <= 0.0f)
	{
		return;
	}

	const TArray<FCrowdUnit>& Units = UnitStore.GetUnits();
	TArray<uint32> Candidates;
	SpatialPartition.QueryUnitsInRadius(Units, Center, Radius, Candidates);

	for (uint32 UnitIndex : Candidates)
	{
		if (UnitIndex >= Units.size())
		{
			continue;
		}

		const FCrowdUnit& Unit = Units[UnitIndex];
		if (!Unit.bAlive || Unit.Team != TargetTeam)
		{
			continue;
		}

		DamageEvents.push_back({
			FUnitHandle{ UnitIndex, Unit.Generation },
			Damage,
			NormalizedXY(Unit.Position - Center)
		});
	}
}

void FCrowdCombatManager::HandleAttackEvent(
	const FMusouAttackEvent& Event,
	const FCrowdUnitStore& UnitStore,
	const FCrowdSpatialPartition& SpatialPartition,
	AMusouGameMode* GameMode)
{
	if (!Event.bFromPlayer || Event.Damage <= 0.0f || Event.Spec.Range <= 0.0f)
	{
		return;
	}

	const TArray<FCrowdUnit>& Units = UnitStore.GetUnits();
	TArray<uint32> Candidates;
	SpatialPartition.QueryUnitsInRadius(Units, Event.Origin, Event.Spec.Range, Candidates);

	int32 HitCount = 0;
	for (uint32 UnitIndex : Candidates)
	{
		if (UnitIndex >= Units.size())
		{
			continue;
		}

		const FCrowdUnit& Unit = Units[UnitIndex];
		if (!Unit.bAlive || Unit.Team != EUnitTeam::Enemy || !Event.IsInVolume(Unit.Position))
		{
			continue;
		}

		DamageEvents.push_back({
			FUnitHandle{ UnitIndex, Unit.Generation },
			Event.Damage,
			NormalizedXY(Unit.Position - Event.Origin),
			true
		});
		++HitCount;
	}

	if (HitCount > 0 && GameMode)
	{
		GameMode->NotifyAttackHits(Event, HitCount);
	}
}

void FCrowdCombatManager::UpdateCombat(float DeltaTime, FCrowdUnitStore& UnitStore)
{
	TArray<FCrowdUnit>& Units = UnitStore.GetUnits();
	for (uint32 Index = 0; Index < static_cast<uint32>(Units.size()); ++Index)
	{
		FCrowdUnit& Unit = Units[Index];
		if (!Unit.bAlive)
		{
			continue;
		}

		Unit.AttackCooldownRemaining = (std::max)(Unit.AttackCooldownRemaining - DeltaTime, 0.0f);
		Unit.AnimTime += DeltaTime;
		Unit.AnimState = static_cast<uint16>(Unit.State);

		if (Unit.State != EUnitState::Attack || Unit.AttackCooldownRemaining > 0.0f)
		{
			continue;
		}

		const FUnitArchetype& Archetype = Unit.Archetype;
		const FCrowdUnit* Target = UnitStore.ResolveUnit(Unit.Target);
		if (!Target)
		{
			Unit.State = EUnitState::Idle;
			continue;
		}

		const float AttackRange = (std::max)(Archetype.AttackRange + Unit.Radius + Target->Radius, 0.0f);
		if (DistanceSquaredXY(Unit.Position, Target->Position) > AttackRange * AttackRange)
		{
			Unit.State = EUnitState::Chase;
			continue;
		}

		DamageEvents.push_back({
			Unit.Target,
			Archetype.AttackDamage,
			NormalizedXY(Target->Position - Unit.Position)
		});
		Unit.AttackCooldownRemaining = Archetype.AttackCooldown;
	}
}

void FCrowdCombatManager::ProcessDamageEvents(
	FCrowdUnitStore& UnitStore,
	AMusouGameMode* GameMode,
	TArray<FUnitHandle>& OutRemovedHandles)
{
	OutRemovedHandles.clear();
	if (DamageEvents.empty())
	{
		return;
	}

	TArray<FDamageEvent> Events = std::move(DamageEvents);
	DamageEvents.clear();

	int32 PlayerKillCount = 0;
	for (const FDamageEvent& Event : Events)
	{
		FCrowdUnit* Target = UnitStore.ResolveUnit(Event.Target);
		if (!Target || Event.Damage <= 0.0f)
		{
			continue;
		}

		Target->HP -= Event.Damage;
		if (Target->HP <= 0.0f)
		{
			if (Event.bCountAsPlayerKill && Target->Team == EUnitTeam::Enemy)
			{
				++PlayerKillCount;
			}

			if (UnitStore.RemoveUnit(Event.Target))
			{
				OutRemovedHandles.push_back(Event.Target);
			}
		}
	}

	if (PlayerKillCount > 0 && GameMode)
	{
		GameMode->NotifyEnemiesKilled(PlayerKillCount);
	}
}
