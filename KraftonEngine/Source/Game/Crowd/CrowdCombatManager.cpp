#include "Game/Crowd/CrowdCombatManager.h"

#include "Game/Musou/Combat/BattleComponent.h"
#include "Game/Musou/Combat/AttackTypes.h"
#include "Game/Musou/Combat/HitTypes.h"
#include "Game/Musou/GameMode/MusouGameMode.h"
#include "GameFramework/Pawn/Pawn.h"

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

	int32 ReactionPriority(EUnitState State)
	{
		switch (State)
		{
		case EUnitState::Dead:
			return 3;
		case EUnitState::KnockDown:
			return 2;
		case EUnitState::Hit:
			return 1;
		default:
			return 0;
		}
	}

	bool ApplyTimedReactionState(
		FCrowdUnit& Unit,
		EUnitState NewState,
		float Duration,
		const FDamageEvent& Event)
	{
		const int32 CurrentPriority = ReactionPriority(Unit.State);
		const int32 NewPriority = ReactionPriority(NewState);
		if (CurrentPriority > NewPriority)
		{
			return false;
		}

		const EUnitState PreviousState = Unit.State;
		Unit.State = NewState;
		Unit.StateTimeRemaining = (std::max)(Duration, 0.0f);
		Unit.AnimState = static_cast<uint16>(Unit.State);
		Unit.AnimTime = 0.0f;

		if (NewState == EUnitState::Dead)
		{
			Unit.Target = {};
			Unit.Velocity = FVector::ZeroVector;
			Unit.KnockbackTimeRemaining = 0.0f;
			Unit.KnockbackVelocity = FVector::ZeroVector;
			return true;
		}

		const FVector KnockbackDirection = NormalizedXY(Event.HitDirection);
		const bool bHasKnockback = Event.KnockbackDistance > 0.0f
			&& Event.KnockbackDuration > 0.0f
			&& !KnockbackDirection.IsNearlyZero();
		if (bHasKnockback)
		{
			Unit.KnockbackTimeRemaining = Event.KnockbackDuration;
			Unit.KnockbackVelocity = KnockbackDirection * (Event.KnockbackDistance / Event.KnockbackDuration);
		}
		else if (PreviousState != NewState)
		{
			Unit.KnockbackTimeRemaining = 0.0f;
			Unit.KnockbackVelocity = FVector::ZeroVector;
			Unit.Velocity = FVector::ZeroVector;
		}

		return true;
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
	const float RadiusSq = Radius * Radius;

	auto QueueDamage = [&](uint32 UnitIndex, const FCrowdUnit& Unit)
	{
		DamageEvents.push_back({
			FUnitHandle{ UnitIndex, Unit.Generation },
			Damage,
			NormalizedXY(Unit.Position - Center)
		});
	};

	for (uint32 UnitIndex : Candidates)
	{
		if (UnitIndex >= Units.size())
		{
			continue;
		}

		const FCrowdUnit& Unit = Units[UnitIndex];
		if (!IsCrowdUnitAliveForGameplay(Unit) || Unit.Team != TargetTeam)
		{
			continue;
		}

		QueueDamage(UnitIndex, Unit);
	}

	for (uint32 UnitIndex = 0; UnitIndex < static_cast<uint32>(Units.size()); ++UnitIndex)
	{
		const FCrowdUnit& Unit = Units[UnitIndex];
		if (Unit.LOD != ECrowdUnitLOD::Dormant
			|| !IsCrowdUnitAliveForGameplay(Unit)
			|| Unit.Team != TargetTeam
			|| DistanceSquaredXY(Unit.Position, Center) > RadiusSq)
		{
			continue;
		}

		QueueDamage(UnitIndex, Unit);
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
	auto QueuePlayerHit = [&](uint32 UnitIndex, const FCrowdUnit& Unit)
	{
		if (GameMode)
		{
			FMusouHitEvent Hit;
			Hit.Attack = &Event;
			Hit.UnitHandle = FUnitHandle{ UnitIndex, Unit.Generation };
			Hit.HitLocation = Unit.Position;
			Hit.HitDirection = NormalizedXY(Unit.Position - Event.Origin);
			Hit.Damage = Event.Damage;
			GameMode->NotifyHitConfirmed(Hit);
		}

		DamageEvents.push_back({
			FUnitHandle{ UnitIndex, Unit.Generation },
			Event.Damage,
			NormalizedXY(Unit.Position - Event.Origin),
			true,
			true,
			Event.Spec.KnockbackDist,
			Event.Spec.KnockbackDur
		});
		++HitCount;
	};

	for (uint32 UnitIndex : Candidates)
	{
		if (UnitIndex >= Units.size())
		{
			continue;
		}

		const FCrowdUnit& Unit = Units[UnitIndex];
		if (!IsCrowdUnitAliveForGameplay(Unit) || Unit.Team != EUnitTeam::Enemy || !Event.IsInVolume(Unit.Position))
		{
			continue;
		}

		QueuePlayerHit(UnitIndex, Unit);
	}

	for (uint32 UnitIndex = 0; UnitIndex < static_cast<uint32>(Units.size()); ++UnitIndex)
	{
		const FCrowdUnit& Unit = Units[UnitIndex];
		if (Unit.LOD != ECrowdUnitLOD::Dormant
			|| !IsCrowdUnitAliveForGameplay(Unit)
			|| Unit.Team != EUnitTeam::Enemy
			|| !Event.IsInVolume(Unit.Position))
		{
			continue;
		}

		QueuePlayerHit(UnitIndex, Unit);
	}

	if (HitCount > 0 && GameMode)
	{
		GameMode->NotifyAttackComboHits(Event, HitCount);
		GameMode->NotifyAttackHitFeedback(Event, HitCount);
	}
}

void FCrowdCombatManager::UpdateStateTimers(
	float DeltaTime,
	FCrowdUnitStore& UnitStore,
	TArray<FUnitHandle>& OutRemovedHandles)
{
	OutRemovedHandles.clear();

	TArray<FCrowdUnit>& Units = UnitStore.GetUnits();
	const float TimeStep = (std::max)(DeltaTime, 0.0f);
	for (uint32 Index = 0; Index < static_cast<uint32>(Units.size()); ++Index)
	{
		FCrowdUnit& Unit = Units[Index];
		if (!Unit.bAlive || !IsCrowdUnitControlLocked(Unit.State))
		{
			continue;
		}

		Unit.StateTimeRemaining = (std::max)(Unit.StateTimeRemaining - TimeStep, 0.0f);
		if (Unit.StateTimeRemaining > 0.0f)
		{
			continue;
		}

		if (Unit.State == EUnitState::Dead)
		{
			FUnitHandle Handle{ Index, Unit.Generation };
			if (UnitStore.RemoveUnit(Handle))
			{
				OutRemovedHandles.push_back(Handle);
			}
			continue;
		}

		Unit.State = EUnitState::Idle;
		Unit.StateTimeRemaining = 0.0f;
		Unit.KnockbackTimeRemaining = 0.0f;
		Unit.KnockbackVelocity = FVector::ZeroVector;
		Unit.Velocity = FVector::ZeroVector;
		Unit.AnimState = static_cast<uint16>(Unit.State);
		Unit.AnimTime = 0.0f;
	}
}

void FCrowdCombatManager::UpdateCombat(float DeltaTime, FCrowdUnitStore& UnitStore, APawn* PlayerPawn, float PlayerProxyRadius)
{
	TArray<FCrowdUnit>& Units = UnitStore.GetUnits();
	for (uint32 Index = 0; Index < static_cast<uint32>(Units.size()); ++Index)
	{
		FCrowdUnit& Unit = Units[Index];
		if (!ShouldSimulateCrowdUnitThisFrame(Unit))
		{
			continue;
		}

		const float UnitDeltaTime = Unit.SimulationDeltaTime > 0.0f ? Unit.SimulationDeltaTime : DeltaTime;
		Unit.AttackCooldownRemaining = (std::max)(Unit.AttackCooldownRemaining - UnitDeltaTime, 0.0f);
		Unit.AnimTime += UnitDeltaTime;
		Unit.AnimState = static_cast<uint16>(Unit.State);

		if (Unit.TargetKind == ECrowdTargetKind::Player)
		{
			if (!Unit.bHasAttackToken || Unit.State != EUnitState::Attack || Unit.AttackCooldownRemaining > 0.0f)
			{
				continue;
			}

			if (!PlayerPawn)
			{
				Unit.State = EUnitState::Idle;
				continue;
			}

			UBattleComponent* PlayerBattle = PlayerPawn->GetComponentByClass<UBattleComponent>();
			if (!PlayerBattle || PlayerBattle->IsDead())
			{
				Unit.State = EUnitState::Idle;
				continue;
			}

			const FUnitArchetype& Archetype = Unit.Archetype;
			const float AttackRange = (std::max)(Archetype.AttackRange + Unit.Radius + (std::max)(PlayerProxyRadius, 0.0f), 0.0f);
			if (DistanceSquaredXY(Unit.Position, PlayerPawn->GetActorLocation()) > AttackRange * AttackRange)
			{
				Unit.State = EUnitState::Chase;
				continue;
			}

			PlayerBattle->ApplyDamage(Archetype.AttackDamage, nullptr);
			Unit.AttackCooldownRemaining = Archetype.AttackCooldown;
			continue;
		}

		if (Unit.State != EUnitState::Attack || Unit.AttackCooldownRemaining > 0.0f)
		{
			continue;
		}

		const FUnitArchetype& Archetype = Unit.Archetype;
		const FCrowdUnit* Target = UnitStore.ResolveUnit(Unit.Target);
		if (!Target || !IsCrowdUnitCombatActive(*Target))
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
	const FCrowdCombatSettings& Settings,
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
		if (!Target || !IsCrowdUnitAliveForGameplay(*Target) || Event.Damage <= 0.0f)
		{
			continue;
		}

		Target->HP -= Event.Damage;
		if (Target->HP <= 0.0f)
		{
			Target->HP = 0.0f;
			if (Event.bCountAsPlayerKill && Target->Team == EUnitTeam::Enemy)
			{
				++PlayerKillCount;
			}

			ApplyTimedReactionState(*Target, EUnitState::Dead, Settings.DeadStateDuration, Event);
			if (Settings.DeadStateDuration <= 0.0f && UnitStore.RemoveUnit(Event.Target))
			{
				OutRemovedHandles.push_back(Event.Target);
			}
			continue;
		}

		const bool bShouldKnockDown = Event.bCanKnockDown
			&& Event.KnockbackDistance >= (std::max)(Settings.KnockDownMinKnockbackDistance, 0.0f);
		if (bShouldKnockDown)
		{
			ApplyTimedReactionState(*Target, EUnitState::KnockDown, Settings.KnockDownStateDuration, Event);
		}
		else
		{
			ApplyTimedReactionState(*Target, EUnitState::Hit, Settings.HitStateDuration, Event);
		}
	}

	if (PlayerKillCount > 0 && GameMode)
	{
		GameMode->NotifyEnemiesKilled(PlayerKillCount);
	}
}
