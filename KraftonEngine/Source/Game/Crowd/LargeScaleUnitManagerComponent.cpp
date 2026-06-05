#include "Game/Crowd/LargeScaleUnitManagerComponent.h"

#include "Debug/DrawDebugHelpers.h"
#include "GameFramework/World.h"
#include "Game/Musou/Combat/AttackTypes.h"
#include "Game/Musou/GameMode/MusouGameMode.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr float TwoPi = 6.28318530717958647692f;
	constexpr float RadToDeg = 57.295779513082320876f;

	FVector ToXY(const FVector& V)
	{
		return FVector(V.X, V.Y, 0.0f);
	}

	float LengthSquaredXY(const FVector& V)
	{
		return V.X * V.X + V.Y * V.Y;
	}

	FVector NormalizedXY(const FVector& V)
	{
		const float LenSq = LengthSquaredXY(V);
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

	FRotator RotationFromDirectionXY(const FVector& Direction)
	{
		if (LengthSquaredXY(Direction) <= 1.e-6f)
		{
			return FRotator::ZeroRotator;
		}

		return FRotator(0.0f, std::atan2(Direction.Y, Direction.X) * RadToDeg, 0.0f);
	}

	AMusouGameMode* GetMusouGameModeFor(const UActorComponent* Component)
	{
		UWorld* World = Component ? Component->GetWorld() : nullptr;
		return World ? Cast<AMusouGameMode>(World->GetGameMode()) : nullptr;
	}
}

ULargeScaleUnitManagerComponent::ULargeScaleUnitManagerComponent()
{
}

void ULargeScaleUnitManagerComponent::BeginPlay()
{
	UActorComponent::BeginPlay();

	if (AMusouGameMode* GameMode = GetMusouGameModeFor(this))
	{
		AttackListenerHandle = GameMode->OnAttackPerformed.AddUObject(this, &ULargeScaleUnitManagerComponent::HandleAttackEvent);
	}
}

void ULargeScaleUnitManagerComponent::EndPlay()
{
	if (AttackListenerHandle.IsValid())
	{
		if (AMusouGameMode* GameMode = GetMusouGameModeFor(this))
		{
			GameMode->OnAttackPerformed.Remove(AttackListenerHandle);
		}
		AttackListenerHandle.Reset();
	}

	UActorComponent::EndPlay();
}

FUnitArchetype ULargeScaleUnitManagerComponent::BuildDefaultArchetype() const
{
	FUnitArchetype Archetype;
	Archetype.MaxHP = DefaultMaxHP;
	Archetype.MoveSpeed = DefaultMoveSpeed;
	Archetype.DetectRange = DefaultDetectRange;
	Archetype.AttackRange = DefaultAttackRange;
	Archetype.AttackDamage = DefaultAttackDamage;
	Archetype.AttackCooldown = DefaultAttackCooldown;
	Archetype.Radius = DefaultUnitRadius;
	Archetype.SeparationRadius = DefaultSeparationRadius;
	Archetype.SeparationWeight = DefaultSeparationWeight;
	return Archetype;
}

FUnitHandle ULargeScaleUnitManagerComponent::SpawnUnit(EUnitTeam Team, const FVector& Position)
{
	const FUnitHandle Handle = AllocateUnitSlot();

	if (bIsUpdating)
	{
		PendingSpawns.push_back({ Handle, Team, Position });
	}
	else
	{
		ActivateUnit(Handle, Team, Position);
	}

	return Handle;
}

void ULargeScaleUnitManagerComponent::SpawnUnits(EUnitTeam Team, const FVector& Center, int32 Count, float Radius)
{
	if (Count <= 0)
	{
		return;
	}

	const float SpawnRadius = std::max(Radius, 0.0f);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float Angle = NextRandom01() * TwoPi;
		const float Dist = std::sqrt(NextRandom01()) * SpawnRadius;
		const FVector Offset(std::cos(Angle) * Dist, std::sin(Angle) * Dist, 0.0f);
		SpawnUnit(Team, Center + Offset);
	}
}

void ULargeScaleUnitManagerComponent::DespawnUnit(FUnitHandle Handle)
{
	if (!Handle.IsValid())
	{
		return;
	}

	if (bIsUpdating)
	{
		PendingDespawns.push_back(Handle);
	}
	else
	{
		RemoveUnitInternal(Handle);
	}
}

void ULargeScaleUnitManagerComponent::ClearUnits()
{
	Units.clear();
	FreeUnitIndices.clear();
	PendingSpawns.clear();
	PendingDespawns.clear();
	DamageEvents.clear();
	RenderData.clear();
	SpatialGrid.clear();
}

void ULargeScaleUnitManagerComponent::ApplyRadialDamage(const FVector& Center, float Radius, float Damage, EUnitTeam TargetTeam)
{
	if (Radius <= 0.0f || Damage <= 0.0f)
	{
		return;
	}

	if (SpatialGrid.empty())
	{
		RebuildSpatialGrid();
	}

	TArray<uint32> Candidates;
	QueryUnitsInRadius(Center, Radius, Candidates);

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

int32 ULargeScaleUnitManagerComponent::GetAliveCount() const
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

int32 ULargeScaleUnitManagerComponent::GetTeamAliveCount(EUnitTeam Team) const
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

bool ULargeScaleUnitManagerComponent::IsUnitAlive(FUnitHandle Handle) const
{
	return IsValidUnitHandle(Handle);
}

FVector ULargeScaleUnitManagerComponent::GetUnitPosition(FUnitHandle Handle) const
{
	const FCrowdUnit* Unit = ResolveUnit(Handle);
	return Unit ? Unit->Position : FVector::ZeroVector;
}

void ULargeScaleUnitManagerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UActorComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ProcessPendingSpawns();
	ProcessPendingDespawns();

	RebuildSpatialGrid();

	bIsUpdating = true;
	UpdateAI(DeltaTime);
	UpdateMovement(DeltaTime);
	UpdateCombat(DeltaTime);
	ProcessDamageEvents();
	bIsUpdating = false;

	ProcessPendingSpawns();
	ProcessPendingDespawns();

	BuildRenderData();
	DrawDebugUnits();
}

void ULargeScaleUnitManagerComponent::HandleAttackEvent(const FMusouAttackEvent& Event)
{
	if (!Event.bFromPlayer || Event.Damage <= 0.0f || Event.Spec.Range <= 0.0f)
	{
		return;
	}

	RebuildSpatialGrid();

	TArray<uint32> Candidates;
	QueryUnitsInRadius(Event.Origin, Event.Spec.Range, Candidates);

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

	if (HitCount > 0)
	{
		if (AMusouGameMode* GameMode = GetMusouGameModeFor(this))
		{
			GameMode->NotifyAttackHits(Event, HitCount);
		}
	}
}

FUnitHandle ULargeScaleUnitManagerComponent::AllocateUnitSlot()
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

void ULargeScaleUnitManagerComponent::ActivateUnit(FUnitHandle Handle, EUnitTeam Team, const FVector& Position)
{
	if (Handle.Index >= Units.size())
	{
		return;
	}

	const FUnitArchetype Archetype = BuildDefaultArchetype();
	FCrowdUnit& Unit = Units[Handle.Index];

	Unit = FCrowdUnit();
	Unit.Generation = Handle.Generation != 0 ? Handle.Generation : 1;
	Unit.bAlive = true;
	Unit.Team = Team;
	Unit.State = EUnitState::Idle;
	Unit.Position = Position;
	Unit.SpawnZ = Position.Z;
	Unit.HP = Archetype.MaxHP;
	Unit.Radius = Archetype.Radius;
	Unit.ThinkTimer = RandomThinkInterval();
	Unit.AttackCooldownRemaining = NextRandom01() * Archetype.AttackCooldown;
}

void ULargeScaleUnitManagerComponent::RemoveUnitInternal(FUnitHandle Handle)
{
	if (!IsValidUnitHandle(Handle))
	{
		return;
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
}

bool ULargeScaleUnitManagerComponent::IsValidUnitHandle(FUnitHandle Handle) const
{
	return Handle.IsValid()
		&& Handle.Index < Units.size()
		&& Units[Handle.Index].bAlive
		&& Units[Handle.Index].Generation == Handle.Generation;
}

FCrowdUnit* ULargeScaleUnitManagerComponent::ResolveUnit(FUnitHandle Handle)
{
	return IsValidUnitHandle(Handle) ? &Units[Handle.Index] : nullptr;
}

const FCrowdUnit* ULargeScaleUnitManagerComponent::ResolveUnit(FUnitHandle Handle) const
{
	return IsValidUnitHandle(Handle) ? &Units[Handle.Index] : nullptr;
}

void ULargeScaleUnitManagerComponent::ProcessPendingSpawns()
{
	if (PendingSpawns.empty())
	{
		return;
	}

	TArray<FPendingSpawn> Spawns = std::move(PendingSpawns);
	PendingSpawns.clear();

	for (const FPendingSpawn& Spawn : Spawns)
	{
		ActivateUnit(Spawn.Handle, Spawn.Team, Spawn.Position);
	}
}

void ULargeScaleUnitManagerComponent::ProcessPendingDespawns()
{
	if (PendingDespawns.empty())
	{
		return;
	}

	TArray<FUnitHandle> Despawns = std::move(PendingDespawns);
	PendingDespawns.clear();

	for (FUnitHandle Handle : Despawns)
	{
		RemoveUnitInternal(Handle);
	}
}

void ULargeScaleUnitManagerComponent::RebuildSpatialGrid()
{
	SpatialGrid.clear();
	SpatialGrid.reserve(Units.size());

	for (uint32 Index = 0; Index < static_cast<uint32>(Units.size()); ++Index)
	{
		const FCrowdUnit& Unit = Units[Index];
		if (!Unit.bAlive)
		{
			continue;
		}

		const int32 CellX = GetCellCoord(Unit.Position.X);
		const int32 CellY = GetCellCoord(Unit.Position.Y);
		SpatialGrid[MakeCellKey(CellX, CellY)].push_back(Index);
	}
}

void ULargeScaleUnitManagerComponent::QueryUnitsInRadius(const FVector& Center, float Radius, TArray<uint32>& OutIndices) const
{
	OutIndices.clear();

	if (Radius <= 0.0f)
	{
		return;
	}

	const int32 MinX = GetCellCoord(Center.X - Radius);
	const int32 MaxX = GetCellCoord(Center.X + Radius);
	const int32 MinY = GetCellCoord(Center.Y - Radius);
	const int32 MaxY = GetCellCoord(Center.Y + Radius);
	const float RadiusSq = Radius * Radius;

	for (int32 CellX = MinX; CellX <= MaxX; ++CellX)
	{
		for (int32 CellY = MinY; CellY <= MaxY; ++CellY)
		{
			auto It = SpatialGrid.find(MakeCellKey(CellX, CellY));
			if (It == SpatialGrid.end())
			{
				continue;
			}

			for (uint32 UnitIndex : It->second)
			{
				if (UnitIndex >= Units.size())
				{
					continue;
				}

				const FCrowdUnit& Unit = Units[UnitIndex];
				if (Unit.bAlive && DistanceSquaredXY(Unit.Position, Center) <= RadiusSq)
				{
					OutIndices.push_back(UnitIndex);
				}
			}
		}
	}
}

FUnitHandle ULargeScaleUnitManagerComponent::FindNearestHostile(uint32 UnitIndex, float MaxRange) const
{
	if (UnitIndex >= Units.size() || MaxRange <= 0.0f)
	{
		return {};
	}

	const FCrowdUnit& Unit = Units[UnitIndex];
	if (!Unit.bAlive)
	{
		return {};
	}

	TArray<uint32> Candidates;
	QueryUnitsInRadius(Unit.Position, MaxRange, Candidates);

	float BestDistanceSq = MaxRange * MaxRange;
	FUnitHandle BestTarget;

	for (uint32 CandidateIndex : Candidates)
	{
		if (CandidateIndex == UnitIndex || CandidateIndex >= Units.size())
		{
			continue;
		}

		const FCrowdUnit& Candidate = Units[CandidateIndex];
		if (!Candidate.bAlive || !IsHostile(Unit.Team, Candidate.Team))
		{
			continue;
		}

		const float DistSq = DistanceSquaredXY(Unit.Position, Candidate.Position);
		if (DistSq < BestDistanceSq)
		{
			BestDistanceSq = DistSq;
			BestTarget = FUnitHandle{ CandidateIndex, Candidate.Generation };
		}
	}

	return BestTarget;
}

void ULargeScaleUnitManagerComponent::UpdateAI(float DeltaTime)
{
	const FUnitArchetype Archetype = BuildDefaultArchetype();

	for (uint32 Index = 0; Index < static_cast<uint32>(Units.size()); ++Index)
	{
		FCrowdUnit& Unit = Units[Index];
		if (!Unit.bAlive)
		{
			continue;
		}

		Unit.ThinkTimer -= DeltaTime;
		if (Unit.ThinkTimer > 0.0f)
		{
			continue;
		}

		Unit.ThinkTimer = RandomThinkInterval();

		const FCrowdUnit* Target = ResolveUnit(Unit.Target);
		if (Target && !IsHostile(Unit.Team, Target->Team))
		{
			Target = nullptr;
			Unit.Target = {};
		}

		if (!Target)
		{
			Unit.Target = FindNearestHostile(Index, Archetype.DetectRange);
			Target = ResolveUnit(Unit.Target);
		}

		if (!Target)
		{
			Unit.State = EUnitState::Idle;
			continue;
		}

		const float AttackRange = std::max(Archetype.AttackRange + Unit.Radius + Target->Radius, 0.0f);
		const bool bInAttackRange = DistanceSquaredXY(Unit.Position, Target->Position) <= AttackRange * AttackRange;
		Unit.State = bInAttackRange ? EUnitState::Attack : EUnitState::Chase;
	}
}

void ULargeScaleUnitManagerComponent::UpdateMovement(float DeltaTime)
{
	if (DeltaTime <= 0.0f)
	{
		return;
	}

	const FUnitArchetype Archetype = BuildDefaultArchetype();
	TArray<uint32> Neighbors;

	for (uint32 Index = 0; Index < static_cast<uint32>(Units.size()); ++Index)
	{
		FCrowdUnit& Unit = Units[Index];
		if (!Unit.bAlive)
		{
			continue;
		}

		FVector Desired = FVector::ZeroVector;
		if (Unit.State == EUnitState::Chase || Unit.State == EUnitState::Attack)
		{
			if (const FCrowdUnit* Target = ResolveUnit(Unit.Target))
			{
				Desired = NormalizedXY(Target->Position - Unit.Position);
				Unit.Rotation = RotationFromDirectionXY(Desired);
			}
			else
			{
				Unit.State = EUnitState::Idle;
			}
		}

		FVector Separation = FVector::ZeroVector;
		if (Archetype.SeparationRadius > 0.0f && Archetype.SeparationWeight > 0.0f)
		{
			QueryUnitsInRadius(Unit.Position, Archetype.SeparationRadius, Neighbors);
			for (uint32 OtherIndex : Neighbors)
			{
				if (OtherIndex == Index || OtherIndex >= Units.size())
				{
					continue;
				}

				const FCrowdUnit& Other = Units[OtherIndex];
				if (!Other.bAlive)
				{
					continue;
				}

				const FVector Away = ToXY(Unit.Position - Other.Position);
				const float DistSq = LengthSquaredXY(Away);
				if (DistSq <= 1.e-6f)
				{
					continue;
				}

				const float Dist = std::sqrt(DistSq);
				const float Strength = std::max(Archetype.SeparationRadius - Dist, 0.0f) / Archetype.SeparationRadius;
				Separation += (Away / Dist) * Strength;
			}
		}

		FVector MoveDir = Desired;
		if (LengthSquaredXY(Separation) > 1.e-6f)
		{
			MoveDir += NormalizedXY(Separation) * Archetype.SeparationWeight;
		}

		MoveDir = NormalizedXY(MoveDir);
		if (LengthSquaredXY(MoveDir) <= 1.e-6f || Unit.State == EUnitState::Attack)
		{
			Unit.Velocity = FVector::ZeroVector;
			continue;
		}

		Unit.Velocity = MoveDir * Archetype.MoveSpeed;
		Unit.Position += Unit.Velocity * DeltaTime;
		Unit.Position.Z = Unit.SpawnZ;
		Unit.Rotation = RotationFromDirectionXY(MoveDir);
	}
}

void ULargeScaleUnitManagerComponent::UpdateCombat(float DeltaTime)
{
	const FUnitArchetype Archetype = BuildDefaultArchetype();

	for (uint32 Index = 0; Index < static_cast<uint32>(Units.size()); ++Index)
	{
		FCrowdUnit& Unit = Units[Index];
		if (!Unit.bAlive)
		{
			continue;
		}

		Unit.AttackCooldownRemaining = std::max(Unit.AttackCooldownRemaining - DeltaTime, 0.0f);
		Unit.AnimTime += DeltaTime;
		Unit.AnimState = static_cast<uint16>(Unit.State);

		if (Unit.State != EUnitState::Attack || Unit.AttackCooldownRemaining > 0.0f)
		{
			continue;
		}

		const FCrowdUnit* Target = ResolveUnit(Unit.Target);
		if (!Target)
		{
			Unit.State = EUnitState::Idle;
			continue;
		}

		const float AttackRange = std::max(Archetype.AttackRange + Unit.Radius + Target->Radius, 0.0f);
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

void ULargeScaleUnitManagerComponent::ProcessDamageEvents()
{
	if (DamageEvents.empty())
	{
		return;
	}

	TArray<FDamageEvent> Events = std::move(DamageEvents);
	DamageEvents.clear();

	int32 PlayerKillCount = 0;
	for (const FDamageEvent& Event : Events)
	{
		FCrowdUnit* Target = ResolveUnit(Event.Target);
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
			RemoveUnitInternal(Event.Target);
		}
	}

	if (PlayerKillCount > 0)
	{
		if (AMusouGameMode* GameMode = GetMusouGameModeFor(this))
		{
			GameMode->NotifyEnemiesKilled(PlayerKillCount);
		}
	}
}

void ULargeScaleUnitManagerComponent::BuildRenderData()
{
	RenderData.clear();
	RenderData.reserve(GetAliveCount());

	for (uint32 Index = 0; Index < static_cast<uint32>(Units.size()); ++Index)
	{
		const FCrowdUnit& Unit = Units[Index];
		if (!Unit.bAlive)
		{
			continue;
		}

		RenderData.push_back({
			FUnitHandle{ Index, Unit.Generation },
			Unit.Team,
			Unit.State,
			Unit.Position,
			Unit.Rotation,
			Unit.AnimState,
			Unit.AnimTime,
			true
		});
	}
}

void ULargeScaleUnitManagerComponent::DrawDebugUnits()
{
	if (!bDebugDrawEnabled)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FUnitArchetype Archetype = BuildDefaultArchetype();
	int32 Drawn = 0;
	for (uint32 Index = 0; Index < static_cast<uint32>(Units.size()); ++Index)
	{
		const FCrowdUnit& Unit = Units[Index];
		if (!Unit.bAlive)
		{
			continue;
		}

		if (DebugDrawMaxUnits >= 0 && Drawn >= DebugDrawMaxUnits)
		{
			break;
		}
		++Drawn;

		DrawDebugSphere(World, Unit.Position, Unit.Radius, 8, GetTeamDebugColor(Unit.Team), 0.0f);

		if (const FCrowdUnit* Target = ResolveUnit(Unit.Target))
		{
			DrawDebugLine(World, Unit.Position, Target->Position, FColor::Yellow(), 0.0f);
			if (Unit.State == EUnitState::Attack)
			{
				DrawDebugSphere(World, Unit.Position, Archetype.AttackRange + Unit.Radius + Target->Radius, 12, FColor::Yellow(), 0.0f);
			}
		}
	}
}

float ULargeScaleUnitManagerComponent::NextRandom01()
{
	RandomState = RandomState * 1664525u + 1013904223u;
	return static_cast<float>((RandomState >> 8) & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
}

float ULargeScaleUnitManagerComponent::RandomThinkInterval()
{
	return 0.1f + NextRandom01() * 0.15f;
}

int32 ULargeScaleUnitManagerComponent::GetCellCoord(float Value) const
{
	const float SafeCellSize = std::max(CellSize, 0.5f);
	return static_cast<int32>(std::floor(Value / SafeCellSize));
}

FColor ULargeScaleUnitManagerComponent::GetTeamDebugColor(EUnitTeam Team) const
{
	switch (Team)
	{
	case EUnitTeam::Ally:
		return FColor::Green();
	case EUnitTeam::Enemy:
	default:
		return FColor::Red();
	}
}

int64 ULargeScaleUnitManagerComponent::MakeCellKey(int32 CellX, int32 CellY)
{
	return (static_cast<int64>(static_cast<uint32>(CellX)) << 32)
		| static_cast<uint32>(CellY);
}
