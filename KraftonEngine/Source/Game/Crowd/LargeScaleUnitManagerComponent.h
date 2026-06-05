#pragma once

#include "Component/ActorComponent.h"
#include "Core/Delegate.h"
#include "Core/Types/EngineTypes.h"
#include "Game/Crowd/CrowdGroundQuery.h"
#include "Game/Crowd/CrowdUnitTypes.h"
#include "Object/Ptr/SoftObjectPtr.h"
#include "Object/Ptr/SubclassOf.h"

#include "Source/Game/Crowd/LargeScaleUnitManagerComponent.generated.h"

class ACrowdUnitVisualActor;
struct FMusouAttackEvent;
class UAnimInstance;
class UClass;
class USkeletalMesh;

UCLASS()
class ULargeScaleUnitManagerComponent : public UActorComponent
{
public:
	GENERATED_BODY()

	ULargeScaleUnitManagerComponent();

	void BeginPlay() override;
	void EndPlay() override;

	FUnitHandle SpawnUnit(EUnitTeam Team, const FVector& Position);
	void SpawnUnits(EUnitTeam Team, const FVector& Center, int32 Count, float Radius);

	void DespawnUnit(FUnitHandle Handle);
	void ClearUnits();

	void ApplyRadialDamage(const FVector& Center, float Radius, float Damage, EUnitTeam TargetTeam);

	int32 GetAliveCount() const;
	int32 GetTeamAliveCount(EUnitTeam Team) const;

	const TArray<FUnitRenderData>& GetRenderData() const { return RenderData; }

	void SetDebugDrawEnabled(bool bEnabled) { bDebugDrawEnabled = bEnabled; }
	bool IsDebugDrawEnabled() const { return bDebugDrawEnabled; }

	bool IsUnitAlive(FUnitHandle Handle) const;
	FVector GetUnitPosition(FUnitHandle Handle) const;

	void SetSurfaceFollowingEnabled(bool bEnabled) { bSurfaceFollowingEnabled = bEnabled; }
	bool IsSurfaceFollowingEnabled() const { return bSurfaceFollowingEnabled; }
	void RebuildGroundQuery();

	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

private:
	struct FPendingSpawn
	{
		FUnitHandle Handle;
		EUnitTeam Team = EUnitTeam::Enemy;
		FVector Position = FVector::ZeroVector;
	};

	static bool IsHostile(EUnitTeam A, EUnitTeam B) { return A != B; }
	static int64 MakeCellKey(int32 CellX, int32 CellY);

	FUnitArchetype BuildDefaultArchetype() const;
	FUnitHandle AllocateUnitSlot();
	void ActivateUnit(FUnitHandle Handle, EUnitTeam Team, const FVector& Position);
	void RemoveUnitInternal(FUnitHandle Handle);

	bool IsValidUnitHandle(FUnitHandle Handle) const;
	FCrowdUnit* ResolveUnit(FUnitHandle Handle);
	const FCrowdUnit* ResolveUnit(FUnitHandle Handle) const;

	void ProcessPendingSpawns();
	void ProcessPendingDespawns();
	void RebuildSpatialGrid();
	void QueryUnitsInRadius(const FVector& Center, float Radius, TArray<uint32>& OutIndices) const;

	FUnitHandle FindNearestHostile(uint32 UnitIndex, float MaxRange) const;
	void UpdateAI(float DeltaTime);
	void UpdateMovement(float DeltaTime);
	void UpdateCombat(float DeltaTime);
	void ProcessDamageEvents();
	void HandleAttackEvent(const FMusouAttackEvent& Event);
	void EnsureGroundQueryBuilt();
	void ApplySurfaceFollowing(FCrowdUnit& Unit);

	void BuildRenderData();
	void DrawDebugUnits();
	void SyncVisualActors();
	void DeactivateAllVisualActors();
	void ReleaseVisualActorForHandle(FUnitHandle Handle);
	void DestroyVisualActors(bool bDestroyWorldActors);
	ACrowdUnitVisualActor* AcquireVisualActor();
	USkeletalMesh* ResolveVisualSkeletalMesh();
	UClass* ResolveVisualAnimClass() const;

	float NextRandom01();
	float RandomThinkInterval();
	int32 GetCellCoord(float Value) const;
	FColor GetTeamDebugColor(EUnitTeam Team) const;

private:
	TArray<FCrowdUnit> Units;
	TArray<uint32> FreeUnitIndices;
	TArray<FPendingSpawn> PendingSpawns;
	TArray<FUnitHandle> PendingDespawns;
	TArray<FDamageEvent> DamageEvents;
	TArray<FUnitRenderData> RenderData;
	TMap<int64, TArray<uint32>> SpatialGrid;
	TArray<ACrowdUnitVisualActor*> VisualActors;
	TArray<ACrowdUnitVisualActor*> FreeVisualActors;
	TMap<uint32, ACrowdUnitVisualActor*> ActiveVisualActors;
	FDelegateHandle AttackListenerHandle;
	FCrowdGroundQuery GroundQuery;
	USkeletalMesh* CachedVisualSkeletalMesh = nullptr;
	FString CachedVisualSkeletalMeshPath;

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

	UPROPERTY(Edit, Save, Category="Crowd|Unit", DisplayName="Wait When Chase Blocked")
	bool bWaitWhenChaseBlocked = true;

	UPROPERTY(Edit, Save, Category="Crowd|Unit", DisplayName="Chase Blocked Probe Distance", Min=0.0f, Max=10.0f, Speed=0.05f)
	float ChaseBlockedProbeDistance = 0.25f;

	UPROPERTY(Edit, Save, Category="Crowd|Unit", DisplayName="Chase Blocked Clearance Padding", Min=0.0f, Max=10.0f, Speed=0.01f)
	float ChaseBlockedClearancePadding = 0.05f;
};
