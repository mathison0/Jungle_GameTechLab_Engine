#include "Game/Crowd/LargeScaleUnitManagerComponent.h"

#include "Debug/DrawDebugHelpers.h"
#include "Game/Musou/Combat/AttackTypes.h"
#include "Game/Musou/GameMode/MusouGameMode.h"
#include "GameFramework/World.h"
#include "Object/FName.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr float TwoPi = 6.28318530717958647692f;

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
	bGroundQueryDirty = true;

	if (AMusouGameMode* GameMode = GetMusouGameModeFor(this))
	{
		AttackListenerHandle = GameMode->OnAttackPerformed.AddUObject(this, &ULargeScaleUnitManagerComponent::HandleAttackEvent);
	}
}

void ULargeScaleUnitManagerComponent::EndPlay()
{
	VisualPool.DestroyVisualActors(GetWorld(), GetWorld() && GetWorld()->HasBegunPlay());

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

FUnitArchetype ULargeScaleUnitManagerComponent::BuildUnitArchetype(EUnitCombatType CombatType) const
{
	FUnitArchetype Archetype;
	Archetype.CombatType = CombatType;

	if (CombatType == EUnitCombatType::Ranged)
	{
		Archetype.MaxHP = RangedMaxHP;
		Archetype.MoveSpeed = RangedMoveSpeed;
		Archetype.DetectRange = RangedDetectRange;
		Archetype.AttackRange = RangedAttackRange;
		Archetype.AttackDamage = RangedAttackDamage;
		Archetype.AttackCooldown = RangedAttackCooldown;
		Archetype.Radius = RangedUnitRadius;
		Archetype.SeparationRadius = RangedSeparationRadius;
		Archetype.SeparationWeight = RangedSeparationWeight;
		return Archetype;
	}

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

FCrowdMovementSettings ULargeScaleUnitManagerComponent::BuildMovementSettings() const
{
	FCrowdMovementSettings Settings;
	Settings.bSurfaceFollowingEnabled = bSurfaceFollowingEnabled;
	Settings.VisualTurnSpeedDegreesPerSecond = VisualTurnSpeedDegreesPerSecond;
	Settings.bWaitWhenChaseBlocked = bWaitWhenChaseBlocked;
	Settings.ChaseBlockedProbeDistance = ChaseBlockedProbeDistance;
	Settings.ChaseBlockedClearancePadding = ChaseBlockedClearancePadding;
	Settings.GroundTraceUp = GroundTraceUp;
	Settings.GroundTraceDown = GroundTraceDown;
	Settings.GroundHeightOffset = GroundHeightOffset;
	Settings.GroundMissToleranceFrames = GroundMissToleranceFrames;
	return Settings;
}

FUnitHandle ULargeScaleUnitManagerComponent::SpawnUnit(EUnitTeam Team, const FVector& Position)
{
	return SpawnUnit(Team, EUnitCombatType::Melee, Position);
}

FUnitHandle ULargeScaleUnitManagerComponent::SpawnUnit(EUnitTeam Team, EUnitCombatType CombatType, const FVector& Position)
{
	const FUnitHandle Handle = UnitStore.AllocateUnitSlot();
	const FUnitArchetype Archetype = BuildUnitArchetype(CombatType);

	if (bIsUpdating)
	{
		UnitStore.QueueSpawn(Handle, Team, Archetype, Position);
	}
	else
	{
		ActivateUnit(Handle, Team, Archetype, Position);
	}

	return Handle;
}

void ULargeScaleUnitManagerComponent::SpawnUnits(EUnitTeam Team, const FVector& Center, int32 Count, float Radius)
{
	SpawnUnits(Team, EUnitCombatType::Melee, Center, Count, Radius);
}

void ULargeScaleUnitManagerComponent::SpawnUnits(EUnitTeam Team, EUnitCombatType CombatType, const FVector& Center, int32 Count, float Radius)
{
	if (Count <= 0)
	{
		return;
	}

	const float SpawnRadius = (std::max)(Radius, 0.0f);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float Angle = NextRandom01() * TwoPi;
		const float Dist = std::sqrt(NextRandom01()) * SpawnRadius;
		const FVector Offset(std::cos(Angle) * Dist, std::sin(Angle) * Dist, 0.0f);
		SpawnUnit(Team, CombatType, Center + Offset);
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
		UnitStore.QueueDespawn(Handle);
	}
	else
	{
		RemoveUnitAndReleaseVisual(Handle);
	}
}

void ULargeScaleUnitManagerComponent::ClearUnits()
{
	UnitStore.Clear();
	CombatManager.ClearDamageEvents();
	VisualPool.DestroyVisualActors(GetWorld(), true);
	SpatialPartition.Clear();
}

void ULargeScaleUnitManagerComponent::RebuildGroundQuery()
{
	FCrowdGroundBuildParams Params;
	Params.GroundActorTag = FName(GroundActorTag);
	Params.bAllowFallbackWithoutTag = bAllowGroundFallbackWithoutTag;
	Params.CellSize = GroundSampleCellSize;
	Params.WalkableSlopeAngle = WalkableSlopeAngle;

	GroundQuery.Rebuild(GetWorld(), Params);
	bGroundQueryDirty = false;
}

void ULargeScaleUnitManagerComponent::ApplyRadialDamage(const FVector& Center, float Radius, float Damage, EUnitTeam TargetTeam)
{
	if (SpatialPartition.IsEmpty())
	{
		SpatialPartition.Rebuild(UnitStore.GetUnits(), CellSize);
	}

	CombatManager.ApplyRadialDamage(Center, Radius, Damage, TargetTeam, UnitStore, SpatialPartition);
}

int32 ULargeScaleUnitManagerComponent::GetAliveCount() const
{
	return UnitStore.GetAliveCount();
}

int32 ULargeScaleUnitManagerComponent::GetTeamAliveCount(EUnitTeam Team) const
{
	return UnitStore.GetTeamAliveCount(Team);
}

int32 ULargeScaleUnitManagerComponent::GetTeamCombatTypeAliveCount(EUnitTeam Team, EUnitCombatType CombatType) const
{
	return UnitStore.GetTeamCombatTypeAliveCount(Team, CombatType);
}

bool ULargeScaleUnitManagerComponent::IsUnitAlive(FUnitHandle Handle) const
{
	return UnitStore.IsValidUnitHandle(Handle);
}

FVector ULargeScaleUnitManagerComponent::GetUnitPosition(FUnitHandle Handle) const
{
	const FCrowdUnit* Unit = UnitStore.ResolveUnit(Handle);
	return Unit ? Unit->Position : FVector::ZeroVector;
}

EUnitCombatType ULargeScaleUnitManagerComponent::GetUnitCombatType(FUnitHandle Handle) const
{
	const FCrowdUnit* Unit = UnitStore.ResolveUnit(Handle);
	return Unit ? Unit->Archetype.CombatType : EUnitCombatType::Melee;
}

void ULargeScaleUnitManagerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UActorComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	FlushPendingSpawns();
	FlushPendingDespawns();

	if (bSurfaceFollowingEnabled)
	{
		EnsureGroundQueryBuilt();
	}

	const FCrowdMovementSettings MovementSettings = BuildMovementSettings();

	SpatialPartition.Rebuild(UnitStore.GetUnits(), CellSize);

	bIsUpdating = true;
	AIManager.Update(DeltaTime, UnitStore, SpatialPartition, [this]()
	{
		return RandomThinkInterval();
	});
	MovementManager.Update(DeltaTime, UnitStore, SpatialPartition, GroundQuery, MovementSettings);
	CombatManager.UpdateCombat(DeltaTime, UnitStore);

	TArray<FUnitHandle> RemovedHandles;
	CombatManager.ProcessDamageEvents(UnitStore, GetMusouGameModeFor(this), RemovedHandles);
	for (FUnitHandle Handle : RemovedHandles)
	{
		VisualPool.ReleaseVisualActorForHandle(Handle);
	}

	bIsUpdating = false;

	FlushPendingSpawns();
	FlushPendingDespawns();

	VisualPool.BuildRenderData(UnitStore);
	VisualPool.SyncVisualActors(
		this,
		GetWorld(),
		bEnableSkeletalVisuals,
		VisualSkeletalMeshPath,
		VisualAnimInstanceClass,
		VisualScale);
	DrawDebugUnits();
}

void ULargeScaleUnitManagerComponent::ActivateUnit(
	FUnitHandle Handle,
	EUnitTeam Team,
	const FUnitArchetype& Archetype,
	const FVector& Position)
{
	UnitStore.ActivateUnit(
		Handle,
		Team,
		Position,
		Archetype,
		RandomThinkInterval(),
		NextRandom01() * Archetype.AttackCooldown);

	if (FCrowdUnit* Unit = UnitStore.ResolveUnit(Handle))
	{
		if (bSurfaceFollowingEnabled)
		{
			EnsureGroundQueryBuilt();
		}
		MovementManager.ApplySurfaceFollowing(*Unit, GroundQuery, BuildMovementSettings());
	}
}

void ULargeScaleUnitManagerComponent::FlushPendingSpawns()
{
	UnitStore.FlushPendingSpawns([this](FUnitHandle Handle, EUnitTeam Team, const FUnitArchetype& Archetype, const FVector& Position)
	{
		ActivateUnit(Handle, Team, Archetype, Position);
	});
}

void ULargeScaleUnitManagerComponent::FlushPendingDespawns()
{
	UnitStore.FlushPendingDespawns([this](FUnitHandle Handle)
	{
		RemoveUnitAndReleaseVisual(Handle);
	});
}

void ULargeScaleUnitManagerComponent::RemoveUnitAndReleaseVisual(FUnitHandle Handle)
{
	if (UnitStore.RemoveUnit(Handle))
	{
		VisualPool.ReleaseVisualActorForHandle(Handle);
	}
}

void ULargeScaleUnitManagerComponent::HandleAttackEvent(const FMusouAttackEvent& Event)
{
	SpatialPartition.Rebuild(UnitStore.GetUnits(), CellSize);
	CombatManager.HandleAttackEvent(Event, UnitStore, SpatialPartition, GetMusouGameModeFor(this));
}

void ULargeScaleUnitManagerComponent::EnsureGroundQueryBuilt()
{
	if (!bGroundQueryDirty)
	{
		return;
	}

	RebuildGroundQuery();
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

	const TArray<FCrowdUnit>& Units = UnitStore.GetUnits();
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

		if (const FCrowdUnit* Target = UnitStore.ResolveUnit(Unit.Target))
		{
			DrawDebugLine(World, Unit.Position, Target->Position, FColor::Yellow(), 0.0f);
			if (Unit.State == EUnitState::Attack)
			{
				DrawDebugSphere(World, Unit.Position, Unit.Archetype.AttackRange + Unit.Radius + Target->Radius, 12, FColor::Yellow(), 0.0f);
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
