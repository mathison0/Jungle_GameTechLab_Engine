#include "Game/Crowd/LargeScaleUnitManagerComponent.h"

#include "Animation/AnimInstance.h"
#include "Debug/DrawDebugHelpers.h"
#include "Game/Musou/Combat/AttackTypes.h"
#include "Game/Musou/GameMode/MusouGameMode.h"
#include "GameFramework/GameMode/PlayerController.h"
#include "GameFramework/Pawn/Pawn.h"
#include "GameFramework/World.h"
#include "Object/FName.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr float TwoPi = 6.28318530717958647692f;
	constexpr uint32 CrowdLODUpdateSliceCount = 4;

	float DistanceSquaredXY(const FVector& A, const FVector& B)
	{
		const float DX = A.X - B.X;
		const float DY = A.Y - B.Y;
		return DX * DX + DY * DY;
	}

	float Square(float Value)
	{
		return Value * Value;
	}

	void DrawDebugCircleXY(UWorld* World, const FVector& Center, float Radius, int32 Segments, const FColor& Color, float Duration)
	{
		if (!World || Radius <= 0.0f || Segments < 3)
		{
			return;
		}

		FVector Previous = Center + FVector(Radius, 0.0f, 0.0f);
		for (int32 Index = 1; Index <= Segments; ++Index)
		{
			const float Angle = TwoPi * static_cast<float>(Index) / static_cast<float>(Segments);
			const FVector Current = Center + FVector(std::cos(Angle) * Radius, std::sin(Angle) * Radius, 0.0f);
			DrawDebugLine(World, Previous, Current, Color, Duration);
			Previous = Current;
		}
	}

	AMusouGameMode* GetMusouGameModeFor(const UActorComponent* Component)
	{
		UWorld* World = Component ? Component->GetWorld() : nullptr;
		return World ? Cast<AMusouGameMode>(World->GetGameMode()) : nullptr;
	}

	FCrowdVisualDesc MakeVisualDesc(
		const FSoftObjectPtr& SkeletalMeshPath,
		const TSubclassOf<UAnimInstance>& AnimInstanceClass,
		const FVector& Scale)
	{
		FCrowdVisualDesc Desc;
		Desc.SkeletalMeshPath = SkeletalMeshPath;
		Desc.AnimInstanceClass = AnimInstanceClass;
		Desc.Scale = Scale;
		return Desc;
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
		Archetype.LoseTargetRange = RangedLoseTargetRange;
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
	Archetype.LoseTargetRange = DefaultLoseTargetRange;
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
	Settings.CircleAroundSpeedScale = CircleAroundSpeedScale;
	Settings.CircleAroundRadiusTolerance = CircleAroundRadiusTolerance;
	Settings.CircleAroundRadialCorrectionWeight = CircleAroundRadialCorrectionWeight;
	Settings.bEnablePlayerSeparation = bEnablePlayerSeparation;
	Settings.PlayerSeparationPadding = PlayerSeparationPadding;
	Settings.PlayerSeparationWeight = PlayerSeparationWeight;
	Settings.GroundTraceUp = GroundTraceUp;
	Settings.GroundTraceDown = GroundTraceDown;
	Settings.GroundHeightOffset = GroundHeightOffset;
	Settings.GroundMissToleranceFrames = GroundMissToleranceFrames;
	return Settings;
}

FCrowdCombatSettings ULargeScaleUnitManagerComponent::BuildCombatSettings() const
{
	FCrowdCombatSettings Settings;
	Settings.HitStateDuration = HitStateDuration;
	Settings.KnockDownStateDuration = KnockDownStateDuration;
	Settings.DeadStateDuration = DeadStateDuration;
	Settings.KnockDownMinKnockbackDistance = KnockDownMinKnockbackDistance;
	return Settings;
}

FCrowdEngagementSettings ULargeScaleUnitManagerComponent::BuildEngagementSettings() const
{
	FCrowdEngagementSettings Settings;
	Settings.bEnablePlayerEngagement = bEnablePlayerEngagement;
	Settings.PlayerEngagementRadius = PlayerEngagementRadius;
	Settings.PlayerProxyRadius = PlayerProxyRadius;
	Settings.MeleeCombatSlotCount = MeleeCombatSlotCount;
	Settings.RangedCombatSlotCount = RangedCombatSlotCount;
	Settings.MeleeSlotRadius = MeleeSlotRadius;
	Settings.RangedSlotRadius = RangedSlotRadius;
	Settings.MeleeAttackTokenCount = MeleeAttackTokenCount;
	Settings.RangedAttackTokenCount = RangedAttackTokenCount;
	Settings.SlotArriveTolerance = SlotArriveTolerance;
	return Settings;
}

FCrowdVisualDesc ULargeScaleUnitManagerComponent::BuildVisualDesc(EUnitTeam Team, EUnitCombatType CombatType) const
{
	switch (Team)
	{
	case EUnitTeam::Ally:
		if (CombatType == EUnitCombatType::Ranged)
		{
			return MakeVisualDesc(AllyRangedVisualSkeletalMeshPath, AllyRangedVisualAnimInstanceClass, AllyRangedVisualScale);
		}

		return MakeVisualDesc(AllyMeleeVisualSkeletalMeshPath, AllyMeleeVisualAnimInstanceClass, AllyMeleeVisualScale);
	case EUnitTeam::Enemy:
	default:
		if (CombatType == EUnitCombatType::Ranged)
		{
			return MakeVisualDesc(EnemyRangedVisualSkeletalMeshPath, EnemyRangedVisualAnimInstanceClass, EnemyRangedVisualScale);
		}

		return MakeVisualDesc(EnemyMeleeVisualSkeletalMeshPath, EnemyMeleeVisualAnimInstanceClass, EnemyMeleeVisualScale);
	}
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
	LODUpdateCursor = 0;
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
	return UnitStore.IsUnitAliveForGameplay(Handle);
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

	UpdateCrowdLOD(DeltaTime);

	FCrowdMovementSettings MovementSettings = BuildMovementSettings();
	const FCrowdCombatSettings CombatSettings = BuildCombatSettings();
	const FCrowdEngagementSettings EngagementSettings = BuildEngagementSettings();
	APawn* PlayerPawn = ResolvePlayerPawn();
	const bool bHasPlayerPawn = PlayerPawn != nullptr;
	const FVector PlayerLocation = PlayerPawn ? PlayerPawn->GetActorLocation() : FVector::ZeroVector;
	MovementSettings.bHasPlayerSeparationTarget = bHasPlayerPawn;
	MovementSettings.PlayerSeparationLocation = PlayerLocation;
	MovementSettings.PlayerProxyRadius = EngagementSettings.PlayerProxyRadius;

	TArray<FUnitHandle> RemovedHandles;
	CombatManager.UpdateStateTimers(DeltaTime, UnitStore, RemovedHandles);
	for (FUnitHandle Handle : RemovedHandles)
	{
		VisualPool.ReleaseVisualActorForHandle(Handle);
	}

	SpatialPartition.Rebuild(UnitStore.GetUnits(), CellSize);
	EngagementManager.Update(UnitStore, EngagementSettings, PlayerLocation, bHasPlayerPawn);
	bIsUpdating = true;
	FCrowdAISettings AISettings;
	AISettings.bHasPlayerTarget = bHasPlayerPawn;
	AISettings.PlayerLocation = PlayerLocation;
	AISettings.PlayerProxyRadius = EngagementSettings.PlayerProxyRadius;
	AISettings.SlotArriveTolerance = EngagementSettings.SlotArriveTolerance;
	AISettings.CircleAroundRadiusTolerance = CircleAroundRadiusTolerance;
	AIManager.Update(DeltaTime, UnitStore, SpatialPartition, AISettings, [this]()
	{
		return RandomThinkInterval();
	});
	MovementManager.Update(DeltaTime, UnitStore, SpatialPartition, GroundQuery, MovementSettings);
	CombatManager.UpdateCombat(DeltaTime, UnitStore, PlayerPawn, EngagementSettings.PlayerProxyRadius);

	CombatManager.ProcessDamageEvents(UnitStore, GetMusouGameModeFor(this), CombatSettings, RemovedHandles);
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
		[this](const FUnitRenderData& Data)
		{
			return BuildVisualDesc(Data.Team, Data.CombatType);
		});
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

APawn* ULargeScaleUnitManagerComponent::ResolvePlayerPawn() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const APlayerController* PlayerController = World->GetFirstPlayerController();
	return PlayerController ? PlayerController->GetPossessedPawn() : nullptr;
}

void ULargeScaleUnitManagerComponent::UpdateCrowdLOD(float DeltaTime)
{
	TArray<FCrowdUnit>& Units = UnitStore.GetUnits();
	if (Units.empty())
	{
		return;
	}

	bool bHasReference = false;
	const FVector ReferenceLocation = ResolveCrowdLODReferenceLocation(bHasReference);
	const bool bForceFullLOD = !bEnableCrowdLOD || !bHasReference;
	const float TimeStep = (std::max)(DeltaTime, 0.0f);
	const uint32 CurrentSlice = LODUpdateCursor % CrowdLODUpdateSliceCount;
	++LODUpdateCursor;

	for (uint32 Index = 0; Index < static_cast<uint32>(Units.size()); ++Index)
	{
		FCrowdUnit& Unit = Units[Index];
		if (!Unit.bAlive)
		{
			continue;
		}

		if (bForceFullLOD)
		{
			Unit.LOD = ECrowdUnitLOD::Full;
		}
		else if (IsCrowdUnitAliveForGameplay(Unit) && Index % CrowdLODUpdateSliceCount == CurrentSlice)
		{
			Unit.LOD = SelectCrowdLOD(Unit.LOD, Unit.Position, ReferenceLocation);
		}

		if (!IsCrowdUnitAliveForGameplay(Unit))
		{
			Unit.bSimulateThisFrame = false;
			Unit.SimulationDeltaTime = 0.0f;
			Unit.LODAccumulatedDeltaTime = 0.0f;
			Unit.LODUpdateTimeRemaining = 0.0f;
			continue;
		}

		const float UpdateInterval = GetCrowdLODUpdateInterval(Unit.LOD);
		if (Unit.LOD == ECrowdUnitLOD::Dormant)
		{
			Unit.bSimulateThisFrame = false;
			Unit.SimulationDeltaTime = 0.0f;
			Unit.LODAccumulatedDeltaTime = 0.0f;
			Unit.LODUpdateTimeRemaining = UpdateInterval;
			Unit.Velocity = FVector::ZeroVector;
			continue;
		}

		if (UpdateInterval <= 0.0f)
		{
			Unit.bSimulateThisFrame = true;
			Unit.SimulationDeltaTime = TimeStep;
			Unit.LODAccumulatedDeltaTime = 0.0f;
			Unit.LODUpdateTimeRemaining = 0.0f;
			continue;
		}

		Unit.LODAccumulatedDeltaTime += TimeStep;
		Unit.LODUpdateTimeRemaining -= TimeStep;
		if (Unit.LODUpdateTimeRemaining <= 0.0f)
		{
			Unit.bSimulateThisFrame = true;
			Unit.SimulationDeltaTime = (std::max)(Unit.LODAccumulatedDeltaTime, TimeStep);
			Unit.LODAccumulatedDeltaTime = 0.0f;
			Unit.LODUpdateTimeRemaining = UpdateInterval;
		}
		else
		{
			Unit.bSimulateThisFrame = false;
			Unit.SimulationDeltaTime = 0.0f;
		}
	}
}

FVector ULargeScaleUnitManagerComponent::ResolveCrowdLODReferenceLocation(bool& bOutHasReference) const
{
	bOutHasReference = false;

	const APawn* Pawn = ResolvePlayerPawn();
	if (!Pawn)
	{
		return FVector::ZeroVector;
	}

	bOutHasReference = true;
	return Pawn->GetActorLocation();
}

ECrowdUnitLOD ULargeScaleUnitManagerComponent::SelectCrowdLOD(
	ECrowdUnitLOD CurrentLOD,
	const FVector& UnitPosition,
	const FVector& ReferenceLocation) const
{
	const float FullDistance = (std::max)(FullLODDistance, 0.0f);
	const float SimpleDistance = (std::max)(SimpleLODDistance, FullDistance);
	const float FormationDistance = (std::max)(FormationLODDistance, SimpleDistance);
	const float Hysteresis = (std::max)(LODHysteresis, 0.0f);
	const float DistanceSq = DistanceSquaredXY(UnitPosition, ReferenceLocation);

	auto RawLOD = [DistanceSq, FullDistance, SimpleDistance, FormationDistance]()
	{
		if (DistanceSq <= Square(FullDistance))
		{
			return ECrowdUnitLOD::Full;
		}

		if (DistanceSq <= Square(SimpleDistance))
		{
			return ECrowdUnitLOD::Simple;
		}

		if (DistanceSq <= Square(FormationDistance))
		{
			return ECrowdUnitLOD::Formation;
		}

		return ECrowdUnitLOD::Dormant;
	};

	switch (CurrentLOD)
	{
	case ECrowdUnitLOD::Full:
		if (DistanceSq <= Square(FullDistance + Hysteresis))
		{
			return ECrowdUnitLOD::Full;
		}
		break;
	case ECrowdUnitLOD::Simple:
		if (DistanceSq > Square((std::max)(FullDistance - Hysteresis, 0.0f))
			&& DistanceSq <= Square(SimpleDistance + Hysteresis))
		{
			return ECrowdUnitLOD::Simple;
		}
		break;
	case ECrowdUnitLOD::Formation:
		if (DistanceSq > Square((std::max)(SimpleDistance - Hysteresis, 0.0f))
			&& DistanceSq <= Square(FormationDistance + Hysteresis))
		{
			return ECrowdUnitLOD::Formation;
		}
		break;
	case ECrowdUnitLOD::Dormant:
		if (DistanceSq > Square((std::max)(FormationDistance - Hysteresis, 0.0f)))
		{
			return ECrowdUnitLOD::Dormant;
		}
		break;
	default:
		break;
	}

	return RawLOD();
}

float ULargeScaleUnitManagerComponent::GetCrowdLODUpdateInterval(ECrowdUnitLOD LOD) const
{
	switch (LOD)
	{
	case ECrowdUnitLOD::Simple:
		return (std::max)(SimpleUpdateInterval, 0.0f);
	case ECrowdUnitLOD::Formation:
		return (std::max)(FormationUpdateInterval, 0.0f);
	case ECrowdUnitLOD::Full:
	case ECrowdUnitLOD::Dormant:
	default:
		return 0.0f;
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

	if (const APawn* PlayerPawn = ResolvePlayerPawn())
	{
		const FVector PlayerLocation = PlayerPawn->GetActorLocation() + FVector(0.0f, 0.0f, 0.05f);
		DrawDebugCircleXY(World, PlayerLocation, (std::max)(MeleeSlotRadius, 0.0f), 48, FColor(255, 160, 0), 0.0f);
		DrawDebugCircleXY(World, PlayerLocation, (std::max)(RangedSlotRadius, 0.0f), 64, FColor(0, 160, 255), 0.0f);
		if (bEnablePlayerSeparation)
		{
			const float MaxUnitRadius = (std::max)((std::max)(DefaultUnitRadius, RangedUnitRadius), 0.0f);
			const float SeparationRadius = (std::max)(PlayerProxyRadius + PlayerSeparationPadding + MaxUnitRadius, 0.0f);
			DrawDebugCircleXY(World, PlayerLocation, SeparationRadius, 40, FColor(80, 255, 120), 0.0f);
		}
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
		DrawDebugSphere(
			World,
			Unit.Position + FVector(0.0f, 0.0f, Unit.Radius + 0.1f),
			Unit.Radius * 0.45f,
			6,
			GetLODDebugColor(Unit.LOD),
			0.0f);

		if (Unit.TargetKind == ECrowdTargetKind::Player)
		{
			const bool bCircleAround = Unit.State == EUnitState::CircleAround && !Unit.bHasAttackToken;
			const FColor PlayerTargetColor = Unit.bHasAttackToken
				? FColor(255, 0, 255)
				: (bCircleAround ? FColor(255, 160, 0) : FColor(0, 255, 255));
			DrawDebugLine(World, Unit.Position, Unit.LookAtLocation, PlayerTargetColor, 0.0f);
			if (Unit.bHasCombatSlot)
			{
				DrawDebugSphere(World, Unit.MoveGoal, Unit.Radius * 0.5f, 6, FColor(0, 128, 255), 0.0f);
			}
			if (bCircleAround)
			{
				DrawDebugSphere(
					World,
					Unit.Position + FVector(0.0f, 0.0f, Unit.Radius + 0.2f),
					Unit.Radius * 0.55f,
					8,
					FColor(255, 160, 0),
					0.0f);
			}
			if (Unit.bHasAttackToken)
			{
				DrawDebugSphere(
					World,
					Unit.Position + FVector(0.0f, 0.0f, Unit.Radius + 0.35f),
					Unit.Radius * 0.7f,
					8,
					FColor(255, 0, 255),
					0.0f);
			}
			if (Unit.State == EUnitState::Attack)
			{
				DrawDebugSphere(
					World,
					Unit.Position,
					Unit.Archetype.AttackRange + Unit.Radius + PlayerProxyRadius,
					12,
					FColor(255, 0, 255),
					0.0f);
			}
		}
		else if (Unit.TargetKind == ECrowdTargetKind::Unit)
		{
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

FColor ULargeScaleUnitManagerComponent::GetLODDebugColor(ECrowdUnitLOD LOD) const
{
	switch (LOD)
	{
	case ECrowdUnitLOD::Full:
		return FColor::White();
	case ECrowdUnitLOD::Simple:
		return FColor(0, 255, 255);
	case ECrowdUnitLOD::Formation:
		return FColor::Blue();
	case ECrowdUnitLOD::Dormant:
	default:
		return FColor::Gray();
	}
}
