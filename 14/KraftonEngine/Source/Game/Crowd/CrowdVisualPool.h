#pragma once

#include "Game/Crowd/CrowdUnitStore.h"
#include "Object/Ptr/SoftObjectPtr.h"
#include "Object/Ptr/SubclassOf.h"

class ACrowdUnitVisualActor;
class UAnimInstance;
class UClass;
class ULargeScaleUnitManagerComponent;
class UMaterialInterface;
class USkeletalMesh;
class UWorld;

struct FCrowdVisualDesc
{
	FSoftObjectPtr SkeletalMeshPath = "None";
	TArray<FSoftObjectPtr> MaterialSlots;
	TSubclassOf<UAnimInstance> AnimInstanceClass;
	FVector Scale = FVector(1.0f, 1.0f, 1.0f);
	FCrowdMeleeAnimationSet MeleeAnimations;
};

class FCrowdVisualPool
{
public:
	using FResolveVisualDescFunc = TFunction<FCrowdVisualDesc(const FUnitRenderData&)>;

	const TArray<FUnitRenderData>& GetRenderData() const { return RenderData; }

	void ClearRenderData();
	void BuildRenderData(const FCrowdUnitStore& UnitStore);
	void SyncVisualActors(
		ULargeScaleUnitManagerComponent* Manager,
		UWorld* World,
		bool bEnableSkeletalVisuals,
		const FResolveVisualDescFunc& ResolveVisualDesc);

	void ReleaseVisualActorForHandle(FUnitHandle Handle);
	void DeactivateAllVisualActors();
	void DestroyVisualActors(UWorld* World, bool bDestroyWorldActors);

private:
	ACrowdUnitVisualActor* AcquireVisualActor(UWorld* World);
	USkeletalMesh* ResolveVisualSkeletalMesh(const FSoftObjectPtr& SkeletalMeshPath);
	TArray<UMaterialInterface*> ResolveVisualMaterials(const TArray<FSoftObjectPtr>& MaterialSlots);
	UClass* ResolveVisualAnimClass(const TSubclassOf<UAnimInstance>& AnimInstanceClass) const;

private:
	TArray<FUnitRenderData> RenderData;
	TArray<ACrowdUnitVisualActor*> VisualActors;
	TArray<ACrowdUnitVisualActor*> FreeVisualActors;
	TMap<uint32, ACrowdUnitVisualActor*> ActiveVisualActors;
	TMap<FString, USkeletalMesh*> CachedVisualSkeletalMeshes;
	TMap<FString, UMaterialInterface*> CachedVisualMaterials;
};
