#pragma once

#include "Component/ActorComponent.h"
#include "Core/Delegate.h"
#include "Core/Types/EngineTypes.h"
#include "Game/Crowd/CrowdAIManager.h"
#include "Game/Crowd/CrowdCombatManager.h"
#include "Game/Crowd/CrowdGroundQuery.h"
#include "Game/Crowd/CrowdMovementManager.h"
#include "Game/Crowd/CrowdSpatialPartition.h"
#include "Game/Crowd/CrowdUnitStore.h"
#include "Game/Crowd/CrowdVisualPool.h"
#include "Object/Ptr/SoftObjectPtr.h"
#include "Object/Ptr/SubclassOf.h"

#include "Source/Game/Crowd/LargeScaleUnitManagerComponent.generated.h"

struct FMusouAttackEvent;
class UAnimInstance;

UCLASS()
class ULargeScaleUnitManagerComponent : public UActorComponent
{
public:
	GENERATED_BODY()

	ULargeScaleUnitManagerComponent();

	void BeginPlay() override;
	void EndPlay() override;

	FUnitHandle SpawnUnit(EUnitTeam Team, const FVector& Position);
	FUnitHandle SpawnUnit(EUnitTeam Team, EUnitCombatType CombatType, const FVector& Position);
	void SpawnUnits(EUnitTeam Team, const FVector& Center, int32 Count, float Radius);
	void SpawnUnits(EUnitTeam Team, EUnitCombatType CombatType, const FVector& Center, int32 Count, float Radius);

	void DespawnUnit(FUnitHandle Handle);
	void ClearUnits();

	void ApplyRadialDamage(const FVector& Center, float Radius, float Damage, EUnitTeam TargetTeam);

	int32 GetAliveCount() const;
	int32 GetTeamAliveCount(EUnitTeam Team) const;
	int32 GetTeamCombatTypeAliveCount(EUnitTeam Team, EUnitCombatType CombatType) const;

	const TArray<FUnitRenderData>& GetRenderData() const { return VisualPool.GetRenderData(); }

	void SetDebugDrawEnabled(bool bEnabled) { bDebugDrawEnabled = bEnabled; }
	bool IsDebugDrawEnabled() const { return bDebugDrawEnabled; }

	bool IsUnitAlive(FUnitHandle Handle) const;
	FVector GetUnitPosition(FUnitHandle Handle) const;
	EUnitCombatType GetUnitCombatType(FUnitHandle Handle) const;

	void SetSurfaceFollowingEnabled(bool bEnabled) { bSurfaceFollowingEnabled = bEnabled; }
	bool IsSurfaceFollowingEnabled() const { return bSurfaceFollowingEnabled; }
	void RebuildGroundQuery();

	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

private:
	FUnitArchetype BuildUnitArchetype(EUnitCombatType CombatType) const;
	FCrowdMovementSettings BuildMovementSettings() const;

	void ActivateUnit(FUnitHandle Handle, EUnitTeam Team, const FUnitArchetype& Archetype, const FVector& Position);
	void FlushPendingSpawns();
	void FlushPendingDespawns();
	void RemoveUnitAndReleaseVisual(FUnitHandle Handle);

	void HandleAttackEvent(const FMusouAttackEvent& Event);
	void EnsureGroundQueryBuilt();
	void DrawDebugUnits();

	float NextRandom01();
	float RandomThinkInterval();
	FColor GetTeamDebugColor(EUnitTeam Team) const;

private:
	FCrowdUnitStore UnitStore;
	FCrowdSpatialPartition SpatialPartition;
	FCrowdAIManager AIManager;
	FCrowdMovementManager MovementManager;
	FCrowdCombatManager CombatManager;
	FCrowdVisualPool VisualPool;
	FCrowdGroundQuery GroundQuery;
	FDelegateHandle AttackListenerHandle;

	bool bIsUpdating = false;
	bool bGroundQueryDirty = true;
	uint32 RandomState = 0x12345678u;

	UPROPERTY(Edit, Save, Category="Crowd", DisplayName="Debug Draw")
	bool bDebugDrawEnabled = true;

	UPROPERTY(Edit, Save, Category="Crowd", DisplayName="Debug Draw Max Units", Min=0, Max=5000, Speed=10)
	int32 DebugDrawMaxUnits = 300;

	UPROPERTY(Edit, Save, Category="Crowd|Visual", DisplayName="Enable Skeletal Visuals")
	bool bEnableSkeletalVisuals = true;

	UPROPERTY(Edit, Save, Category="Crowd|Visual", DisplayName="Visual Skeletal Mesh", AssetType="SkeletalMesh")
	FSoftObjectPtr VisualSkeletalMeshPath = "Content/Data/GameJam/Barbarian/Barbarian_SkeletalMesh.uasset";

	UPROPERTY(Edit, Save, Category="Crowd|Visual", DisplayName="Visual Anim Instance Class", Type=ClassRef, AllowedClass=UAnimInstance)
	TSubclassOf<UAnimInstance> VisualAnimInstanceClass;

	UPROPERTY(Edit, Save, Category="Crowd|Visual", DisplayName="Visual Scale")
	FVector VisualScale = FVector(1.0f, 1.0f, 1.0f);

	UPROPERTY(Edit, Save, Category="Crowd|Visual", DisplayName="Visual Turn Speed Deg/Sec", Min=1.0f, Max=3600.0f, Speed=10.0f)
	float VisualTurnSpeedDegreesPerSecond = 540.0f;

	UPROPERTY(Edit, Save, Category="Crowd", DisplayName="Cell Size", Min=0.5f, Max=100.0f, Speed=0.1f)
	float CellSize = 4.0f;

	UPROPERTY(Edit, Save, Category="Crowd|Ground", DisplayName="Surface Following")
	bool bSurfaceFollowingEnabled = true;

	UPROPERTY(Edit, Save, Category="Crowd|Ground", DisplayName="Ground Actor Tag")
	FString GroundActorTag = "CrowdGround";

	UPROPERTY(Edit, Save, Category="Crowd|Ground", DisplayName="Allow Fallback Without Tag")
	bool bAllowGroundFallbackWithoutTag = true;

	UPROPERTY(Edit, Save, Category="Crowd|Ground", DisplayName="Ground Trace Up", Min=0.0f, Max=1000.0f, Speed=0.1f)
	float GroundTraceUp = 5.0f;

	UPROPERTY(Edit, Save, Category="Crowd|Ground", DisplayName="Ground Trace Down", Min=0.0f, Max=1000.0f, Speed=0.1f)
	float GroundTraceDown = 50.0f;

	UPROPERTY(Edit, Save, Category="Crowd|Ground", DisplayName="Ground Height Offset", Min=-100.0f, Max=100.0f, Speed=0.01f)
	float GroundHeightOffset = 0.0f;

	UPROPERTY(Edit, Save, Category="Crowd|Ground", DisplayName="Ground Sample Cell Size", Min=0.5f, Max=100.0f, Speed=0.1f)
	float GroundSampleCellSize = 4.0f;

	UPROPERTY(Edit, Save, Category="Crowd|Ground", DisplayName="Walkable Slope Angle", Min=0.0f, Max=89.0f, Speed=1.0f)
	float WalkableSlopeAngle = 60.0f;

	UPROPERTY(Edit, Save, Category="Crowd|Ground", DisplayName="Ground Miss Tolerance Frames", Min=0, Max=30, Speed=1)
	int32 GroundMissToleranceFrames = 2;

	UPROPERTY(Edit, Save, Category="Crowd|Unit", DisplayName="Max HP", Min=1.0f, Max=10000.0f, Speed=1.0f)
	float DefaultMaxHP = 100.0f;

	UPROPERTY(Edit, Save, Category="Crowd|Unit", DisplayName="Move Speed", Min=0.0f, Max=100.0f, Speed=0.1f)
	float DefaultMoveSpeed = 6.0f;

	UPROPERTY(Edit, Save, Category="Crowd|Unit", DisplayName="Detect Range", Min=0.0f, Max=1000.0f, Speed=0.5f)
	float DefaultDetectRange = 18.0f;

	UPROPERTY(Edit, Save, Category="Crowd|Unit", DisplayName="Attack Range", Min=0.0f, Max=100.0f, Speed=0.1f)
	float DefaultAttackRange = 1.4f;

	UPROPERTY(Edit, Save, Category="Crowd|Unit", DisplayName="Attack Damage", Min=0.0f, Max=10000.0f, Speed=0.5f)
	float DefaultAttackDamage = 10.0f;

	UPROPERTY(Edit, Save, Category="Crowd|Unit", DisplayName="Attack Cooldown", Min=0.01f, Max=60.0f, Speed=0.05f)
	float DefaultAttackCooldown = 1.0f;

	UPROPERTY(Edit, Save, Category="Crowd|Unit", DisplayName="Unit Radius", Min=0.01f, Max=50.0f, Speed=0.05f)
	float DefaultUnitRadius = 0.45f;

	UPROPERTY(Edit, Save, Category="Crowd|Unit", DisplayName="Separation Radius", Min=0.0f, Max=50.0f, Speed=0.05f)
	float DefaultSeparationRadius = 1.1f;

	UPROPERTY(Edit, Save, Category="Crowd|Unit", DisplayName="Separation Weight", Min=0.0f, Max=20.0f, Speed=0.05f)
	float DefaultSeparationWeight = 1.4f;

	UPROPERTY(Edit, Save, Category="Crowd|Unit|Ranged", DisplayName="Ranged Max HP", Min=1.0f, Max=10000.0f, Speed=1.0f)
	float RangedMaxHP = 70.0f;

	UPROPERTY(Edit, Save, Category="Crowd|Unit|Ranged", DisplayName="Ranged Move Speed", Min=0.0f, Max=100.0f, Speed=0.1f)
	float RangedMoveSpeed = 5.0f;

	UPROPERTY(Edit, Save, Category="Crowd|Unit|Ranged", DisplayName="Ranged Detect Range", Min=0.0f, Max=1000.0f, Speed=0.5f)
	float RangedDetectRange = 24.0f;

	UPROPERTY(Edit, Save, Category="Crowd|Unit|Ranged", DisplayName="Ranged Attack Range", Min=0.0f, Max=100.0f, Speed=0.1f)
	float RangedAttackRange = 8.0f;

	UPROPERTY(Edit, Save, Category="Crowd|Unit|Ranged", DisplayName="Ranged Attack Damage", Min=0.0f, Max=10000.0f, Speed=0.5f)
	float RangedAttackDamage = 8.0f;

	UPROPERTY(Edit, Save, Category="Crowd|Unit|Ranged", DisplayName="Ranged Attack Cooldown", Min=0.01f, Max=60.0f, Speed=0.05f)
	float RangedAttackCooldown = 1.4f;

	UPROPERTY(Edit, Save, Category="Crowd|Unit|Ranged", DisplayName="Ranged Unit Radius", Min=0.01f, Max=50.0f, Speed=0.05f)
	float RangedUnitRadius = 0.45f;

	UPROPERTY(Edit, Save, Category="Crowd|Unit|Ranged", DisplayName="Ranged Separation Radius", Min=0.0f, Max=50.0f, Speed=0.05f)
	float RangedSeparationRadius = 1.1f;

	UPROPERTY(Edit, Save, Category="Crowd|Unit|Ranged", DisplayName="Ranged Separation Weight", Min=0.0f, Max=20.0f, Speed=0.05f)
	float RangedSeparationWeight = 1.4f;

	UPROPERTY(Edit, Save, Category="Crowd|Unit", DisplayName="Wait When Chase Blocked")
	bool bWaitWhenChaseBlocked = true;

	UPROPERTY(Edit, Save, Category="Crowd|Unit", DisplayName="Chase Blocked Probe Distance", Min=0.0f, Max=10.0f, Speed=0.05f)
	float ChaseBlockedProbeDistance = 0.25f;

	UPROPERTY(Edit, Save, Category="Crowd|Unit", DisplayName="Chase Blocked Clearance Padding", Min=0.0f, Max=10.0f, Speed=0.01f)
	float ChaseBlockedClearancePadding = 0.05f;
};
