#pragma once

#include "Game/Crowd/CrowdUnitStore.h"
#include "Object/Ptr/SoftObjectPtr.h"
#include "Object/Ptr/SubclassOf.h"

class ACrowdUnitVisualActor;
class UAnimInstance;
class UClass;
class ULargeScaleUnitManagerComponent;
class USkeletalMesh;
class UWorld;

class FCrowdVisualPool
{
public:
	const TArray<FUnitRenderData>& GetRenderData() const { return RenderData; }

	void ClearRenderData();
	void BuildRenderData(const FCrowdUnitStore& UnitStore);
	void SyncVisualActors(
		ULargeScaleUnitManagerComponent* Manager,
		UWorld* World,
		bool bEnableSkeletalVisuals,
		const FSoftObjectPtr& VisualSkeletalMeshPath,
		const TSubclassOf<UAnimInstance>& VisualAnimInstanceClass,
		const FVector& VisualScale);

	void ReleaseVisualActorForHandle(FUnitHandle Handle);
	void DeactivateAllVisualActors();
	void DestroyVisualActors(UWorld* World, bool bDestroyWorldActors);

private:
	ACrowdUnitVisualActor* AcquireVisualActor(UWorld* World);
	USkeletalMesh* ResolveVisualSkeletalMesh(bool bEnableSkeletalVisuals, const FSoftObjectPtr& VisualSkeletalMeshPath);
	UClass* ResolveVisualAnimClass(const TSubclassOf<UAnimInstance>& VisualAnimInstanceClass) const;

private:
	TArray<FUnitRenderData> RenderData;
	TArray<ACrowdUnitVisualActor*> VisualActors;
	TArray<ACrowdUnitVisualActor*> FreeVisualActors;
	TMap<uint32, ACrowdUnitVisualActor*> ActiveVisualActors;
	USkeletalMesh* CachedVisualSkeletalMesh = nullptr;
	FString CachedVisualSkeletalMeshPath;
};
