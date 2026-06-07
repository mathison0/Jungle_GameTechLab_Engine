#pragma once

#include "Component/ActorComponent.h"
#include "Core/Delegate.h"
#include "Core/Types/EngineTypes.h"
#include "Game/Crowd/CrowdAIManager.h"
#include "Game/Crowd/CrowdCombatManager.h"
#include "Game/Crowd/CrowdEngagementManager.h"
#include "Game/Crowd/CrowdGroundQuery.h"
#include "Game/Crowd/CrowdMovementManager.h"
#include "Game/Crowd/CrowdSpatialPartition.h"
#include "Game/Crowd/CrowdUnitStore.h"
#include "Game/Crowd/CrowdVisualPool.h"
#include "Object/Ptr/SoftObjectPtr.h"
#include "Object/Ptr/SubclassOf.h"

#include "Source/Game/Crowd/LargeScaleUnitManagerComponent.generated.h"

struct FMusouAttackEvent;
class APawn;
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
	FCrowdCombatSettings BuildCombatSettings() const;
	FCrowdEngagementSettings BuildEngagementSettings() const;
	FCrowdVisualDesc BuildVisualDesc(EUnitTeam Team, EUnitCombatType CombatType) const;

	void ActivateUnit(FUnitHandle Handle, EUnitTeam Team, const FUnitArchetype& Archetype, const FVector& Position);
	void FlushPendingSpawns();
	void FlushPendingDespawns();
	void RemoveUnitAndReleaseVisual(FUnitHandle Handle);

	void HandleAttackEvent(const FMusouAttackEvent& Event);
	void EnsureGroundQueryBuilt();
	APawn* ResolvePlayerPawn() const;
	void UpdateCrowdLOD(float DeltaTime);
	FVector ResolveCrowdLODReferenceLocation(bool& bOutHasReference) const;
	ECrowdUnitLOD SelectCrowdLOD(ECrowdUnitLOD CurrentLOD, const FVector& UnitPosition, const FVector& ReferenceLocation) const;
	float GetCrowdLODUpdateInterval(ECrowdUnitLOD LOD) const;
	void DrawDebugUnits();

	float NextRandom01();
	float RandomThinkInterval();
	FColor GetTeamDebugColor(EUnitTeam Team) const;
	FColor GetLODDebugColor(ECrowdUnitLOD LOD) const;

private:
	FCrowdUnitStore UnitStore;
	FCrowdSpatialPartition SpatialPartition;
	FCrowdEngagementManager EngagementManager;
	FCrowdAIManager AIManager;
	FCrowdMovementManager MovementManager;
	FCrowdCombatManager CombatManager;
	FCrowdVisualPool VisualPool;
	FCrowdGroundQuery GroundQuery;
	FDelegateHandle AttackListenerHandle;

	bool bIsUpdating = false;
	bool bGroundQueryDirty = true;
	uint32 RandomState = 0x12345678u;
	uint32 LODUpdateCursor = 0;

	UPROPERTY(Edit, Save, Category="Crowd", DisplayName="Debug Draw")
	bool bDebugDrawEnabled = true;

	UPROPERTY(Edit, Save, Category="Crowd", DisplayName="Debug Draw Max Units", Min=0, Max=5000, Speed=10)
	int32 DebugDrawMaxUnits = 300;

	UPROPERTY(Edit, Save, Category="Crowd|Visual", DisplayName="Enable Skeletal Visuals")
	bool bEnableSkeletalVisuals = true;

	UPROPERTY(Edit, Save, Category="Crowd|Visual|Ally|Melee", DisplayName="Ally Melee Skeletal Mesh", AssetType="SkeletalMesh")
	FSoftObjectPtr AllyMeleeVisualSkeletalMeshPath = "None";

	UPROPERTY(Edit, Save, Category="Crowd|Visual|Ally|Melee", DisplayName="Ally Melee Anim Instance Class", Type=ClassRef, AllowedClass=UAnimInstance)
	TSubclassOf<UAnimInstance> AllyMeleeVisualAnimInstanceClass;

	UPROPERTY(Edit, Save, Category="Crowd|Visual|Ally|Melee", DisplayName="Ally Melee Scale")
	FVector AllyMeleeVisualScale = FVector(1.0f, 1.0f, 1.0f);

	UPROPERTY(Edit, Save, Category="Crowd|Visual|Ally|Ranged", DisplayName="Ally Ranged Skeletal Mesh", AssetType="SkeletalMesh")
	FSoftObjectPtr AllyRangedVisualSkeletalMeshPath = "None";

	UPROPERTY(Edit, Save, Category="Crowd|Visual|Ally|Ranged", DisplayName="Ally Ranged Anim Instance Class", Type=ClassRef, AllowedClass=UAnimInstance)
	TSubclassOf<UAnimInstance> AllyRangedVisualAnimInstanceClass;

	UPROPERTY(Edit, Save, Category="Crowd|Visual|Ally|Ranged", DisplayName="Ally Ranged Scale")
	FVector AllyRangedVisualScale = FVector(1.0f, 1.0f, 1.0f);

	UPROPERTY(Edit, Save, Category="Crowd|Visual|Enemy|Melee", DisplayName="Enemy Melee Skeletal Mesh", AssetType="SkeletalMesh")
	FSoftObjectPtr EnemyMeleeVisualSkeletalMeshPath = "None";

	UPROPERTY(Edit, Save, Category="Crowd|Visual|Enemy|Melee", DisplayName="Enemy Melee Anim Instance Class", Type=ClassRef, AllowedClass=UAnimInstance)
	TSubclassOf<UAnimInstance> EnemyMeleeVisualAnimInstanceClass;

	UPROPERTY(Edit, Save, Category="Crowd|Visual|Enemy|Melee", DisplayName="Enemy Melee Scale")
	FVector EnemyMeleeVisualScale = FVector(1.0f, 1.0f, 1.0f);

	UPROPERTY(Edit, Save, Category="Crowd|Visual|Enemy|Ranged", DisplayName="Enemy Ranged Skeletal Mesh", AssetType="SkeletalMesh")
	FSoftObjectPtr EnemyRangedVisualSkeletalMeshPath = "None";

	UPROPERTY(Edit, Save, Category="Crowd|Visual|Enemy|Ranged", DisplayName="Enemy Ranged Anim Instance Class", Type=ClassRef, AllowedClass=UAnimInstance)
	TSubclassOf<UAnimInstance> EnemyRangedVisualAnimInstanceClass;

	UPROPERTY(Edit, Save, Category="Crowd|Visual|Enemy|Ranged", DisplayName="Enemy Ranged Scale")
	FVector EnemyRangedVisualScale = FVector(1.0f, 1.0f, 1.0f);

	UPROPERTY(Edit, Save, Category="Crowd|Visual", DisplayName="Visual Turn Speed Deg/Sec", Min=1.0f, Max=3600.0f, Speed=10.0f)
	float VisualTurnSpeedDegreesPerSecond = 540.0f;

	UPROPERTY(Edit, Save, Category="Crowd", DisplayName="Cell Size", Min=0.5f, Max=100.0f, Speed=0.1f)
	float CellSize = 4.0f;

	UPROPERTY(Edit, Save, Category="Crowd|LOD", DisplayName="Enable Crowd LOD")
	bool bEnableCrowdLOD = true;

	UPROPERTY(Edit, Save, Category="Crowd|LOD", DisplayName="Full LOD Distance", Min=0.0f, Max=10000.0f, Speed=1.0f)
	float FullLODDistance = 30.0f;

	UPROPERTY(Edit, Save, Category="Crowd|LOD", DisplayName="Simple LOD Distance", Min=0.0f, Max=10000.0f, Speed=1.0f)
	float SimpleLODDistance = 60.0f;

	UPROPERTY(Edit, Save, Category="Crowd|LOD", DisplayName="Formation LOD Distance", Min=0.0f, Max=10000.0f, Speed=1.0f)
	float FormationLODDistance = 100.0f;

	UPROPERTY(Edit, Save, Category="Crowd|LOD", DisplayName="LOD Hysteresis", Min=0.0f, Max=1000.0f, Speed=0.5f)
	float LODHysteresis = 5.0f;

	UPROPERTY(Edit, Save, Category="Crowd|LOD", DisplayName="Simple Update Interval", Min=0.0f, Max=10.0f, Speed=0.01f)
	float SimpleUpdateInterval = 0.20f;

	UPROPERTY(Edit, Save, Category="Crowd|LOD", DisplayName="Formation Update Interval", Min=0.0f, Max=10.0f, Speed=0.01f)
	float FormationUpdateInterval = 0.50f;

	UPROPERTY(Edit, Save, Category="Crowd|Player Engagement", DisplayName="Enable Player Engagement")
	bool bEnablePlayerEngagement = true;

	UPROPERTY(Edit, Save, Category="Crowd|Player Engagement", DisplayName="Player Engagement Radius", Min=0.0f, Max=1000.0f, Speed=0.5f)
	float PlayerEngagementRadius = 18.0f;

	UPROPERTY(Edit, Save, Category="Crowd|Player Engagement", DisplayName="Player Proxy Radius", Min=0.0f, Max=100.0f, Speed=0.05f)
	float PlayerProxyRadius = 0.6f;

	UPROPERTY(Edit, Save, Category="Crowd|Player Engagement", DisplayName="Enable Player Separation")
	bool bEnablePlayerSeparation = true;

	UPROPERTY(Edit, Save, Category="Crowd|Player Engagement", DisplayName="Player Separation Padding", Min=0.0f, Max=100.0f, Speed=0.05f)
	float PlayerSeparationPadding = 0.15f;

	UPROPERTY(Edit, Save, Category="Crowd|Player Engagement", DisplayName="Player Separation Weight", Min=0.0f, Max=20.0f, Speed=0.05f)
	float PlayerSeparationWeight = 2.0f;

	UPROPERTY(Edit, Save, Category="Crowd|Player Engagement", DisplayName="Melee Combat Slot Count", Min=0, Max=64, Speed=1)
	int32 MeleeCombatSlotCount = 8;

	UPROPERTY(Edit, Save, Category="Crowd|Player Engagement", DisplayName="Ranged Combat Slot Count", Min=0, Max=64, Speed=1)
	int32 RangedCombatSlotCount = 8;

	UPROPERTY(Edit, Save, Category="Crowd|Player Engagement", DisplayName="Melee Slot Radius", Min=0.0f, Max=100.0f, Speed=0.1f)
	float MeleeSlotRadius = 2.2f;

	UPROPERTY(Edit, Save, Category="Crowd|Player Engagement", DisplayName="Ranged Slot Radius", Min=0.0f, Max=100.0f, Speed=0.1f)
	float RangedSlotRadius = 7.5f;

	UPROPERTY(Edit, Save, Category="Crowd|Player Engagement", DisplayName="Melee Attack Token Count", Min=0, Max=64, Speed=1)
	int32 MeleeAttackTokenCount = 2;

	UPROPERTY(Edit, Save, Category="Crowd|Player Engagement", DisplayName="Ranged Attack Token Count", Min=0, Max=64, Speed=1)
	int32 RangedAttackTokenCount = 1;

	UPROPERTY(Edit, Save, Category="Crowd|Player Engagement", DisplayName="Slot Arrive Tolerance", Min=0.0f, Max=100.0f, Speed=0.05f)
	float SlotArriveTolerance = 0.5f;

	UPROPERTY(Edit, Save, Category="Crowd|Player Engagement", DisplayName="Circle Around Speed Scale", Min=0.0f, Max=5.0f, Speed=0.05f)
	float CircleAroundSpeedScale = 0.75f;

	UPROPERTY(Edit, Save, Category="Crowd|Player Engagement", DisplayName="Circle Around Radius Tolerance", Min=0.0f, Max=100.0f, Speed=0.05f)
	float CircleAroundRadiusTolerance = 0.75f;

	UPROPERTY(Edit, Save, Category="Crowd|Player Engagement", DisplayName="Circle Around Radial Correction Weight", Min=0.0f, Max=10.0f, Speed=0.05f)
	float CircleAroundRadialCorrectionWeight = 0.65f;

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

	UPROPERTY(Edit, Save, Category="Crowd|Unit", DisplayName="Lose Target Range", Min=0.0f, Max=1000.0f, Speed=0.5f)
	float DefaultLoseTargetRange = 24.0f;

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

	UPROPERTY(Edit, Save, Category="Crowd|Unit|Ranged", DisplayName="Ranged Lose Target Range", Min=0.0f, Max=1000.0f, Speed=0.5f)
	float RangedLoseTargetRange = 32.0f;

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

	UPROPERTY(Edit, Save, Category="Crowd|Unit|State", DisplayName="Hit State Duration", Min=0.0f, Max=10.0f, Speed=0.01f)
	float HitStateDuration = 0.18f;

	UPROPERTY(Edit, Save, Category="Crowd|Unit|State", DisplayName="KnockDown State Duration", Min=0.0f, Max=10.0f, Speed=0.01f)
	float KnockDownStateDuration = 0.65f;

	UPROPERTY(Edit, Save, Category="Crowd|Unit|State", DisplayName="Dead State Duration", Min=0.0f, Max=10.0f, Speed=0.01f)
	float DeadStateDuration = 1.0f;

	UPROPERTY(Edit, Save, Category="Crowd|Unit|State", DisplayName="KnockDown Min Knockback Distance", Min=0.0f, Max=100.0f, Speed=0.1f)
	float KnockDownMinKnockbackDistance = 4.0f;

	UPROPERTY(Edit, Save, Category="Crowd|Unit", DisplayName="Wait When Chase Blocked")
	bool bWaitWhenChaseBlocked = true;

	UPROPERTY(Edit, Save, Category="Crowd|Unit", DisplayName="Chase Blocked Probe Distance", Min=0.0f, Max=10.0f, Speed=0.05f)
	float ChaseBlockedProbeDistance = 0.25f;

	UPROPERTY(Edit, Save, Category="Crowd|Unit", DisplayName="Chase Blocked Clearance Padding", Min=0.0f, Max=10.0f, Speed=0.01f)
	float ChaseBlockedClearancePadding = 0.05f;
};
