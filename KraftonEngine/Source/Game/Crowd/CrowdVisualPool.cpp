#include "Game/Crowd/CrowdVisualPool.h"

#include "Animation/AnimInstance.h"
#include "Game/Crowd/CrowdUnitAnimInstance.h"
#include "Game/Crowd/CrowdUnitVisualActor.h"
#include "Game/Crowd/LargeScaleUnitManagerComponent.h"
#include "GameFramework/World.h"
#include "Mesh/MeshManager.h"
#include "Mesh/Skeletal/SkeletalMesh.h"
#include "Runtime/Engine.h"

void FCrowdVisualPool::ClearRenderData()
{
	RenderData.clear();
}

void FCrowdVisualPool::BuildRenderData(const FCrowdUnitStore& UnitStore)
{
	RenderData.clear();
	RenderData.reserve(UnitStore.GetAliveCount());

	const TArray<FCrowdUnit>& Units = UnitStore.GetUnits();
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
			Unit.Archetype.CombatType,
			Unit.State,
			Unit.Position,
			Unit.Rotation,
			Unit.AnimState,
			Unit.AnimTime,
			Unit.Velocity.Length(),
			true
		});
	}
}

void FCrowdVisualPool::SyncVisualActors(
	ULargeScaleUnitManagerComponent* Manager,
	UWorld* World,
	bool bEnableSkeletalVisuals,
	const FSoftObjectPtr& VisualSkeletalMeshPath,
	const TSubclassOf<UAnimInstance>& VisualAnimInstanceClass,
	const FVector& VisualScale)
{
	if (!bEnableSkeletalVisuals)
	{
		DeactivateAllVisualActors();
		return;
	}

	USkeletalMesh* VisualMesh = ResolveVisualSkeletalMesh(bEnableSkeletalVisuals, VisualSkeletalMeshPath);
	if (!VisualMesh)
	{
		DeactivateAllVisualActors();
		return;
	}

	UClass* AnimClass = ResolveVisualAnimClass(VisualAnimInstanceClass);
	TSet<uint32> SeenUnitIndices;
	SeenUnitIndices.reserve(RenderData.size());

	for (const FUnitRenderData& Data : RenderData)
	{
		if (!Data.bVisible || !Data.Handle.IsValid())
		{
			continue;
		}

		SeenUnitIndices.insert(Data.Handle.Index);

		ACrowdUnitVisualActor* VisualActor = nullptr;
		auto ActiveIt = ActiveVisualActors.find(Data.Handle.Index);
		if (ActiveIt != ActiveVisualActors.end())
		{
			VisualActor = ActiveIt->second;
			if (!VisualActor || VisualActor->GetUnitHandle().Generation != Data.Handle.Generation)
			{
				ReleaseVisualActorForHandle(FUnitHandle{ Data.Handle.Index, 0 });
				VisualActor = nullptr;
			}
		}

		if (!VisualActor)
		{
			VisualActor = AcquireVisualActor(World);
			if (!VisualActor)
			{
				continue;
			}
			ActiveVisualActors[Data.Handle.Index] = VisualActor;
		}

		VisualActor->InitializeVisual(Manager, VisualMesh, AnimClass);
		VisualActor->SetActorScale(VisualScale);
		VisualActor->ApplyRenderData(Data);
	}

	TArray<uint32> StaleUnitIndices;
	for (const auto& Pair : ActiveVisualActors)
	{
		if (SeenUnitIndices.find(Pair.first) == SeenUnitIndices.end())
		{
			StaleUnitIndices.push_back(Pair.first);
		}
	}

	for (uint32 UnitIndex : StaleUnitIndices)
	{
		ReleaseVisualActorForHandle(FUnitHandle{ UnitIndex, 0 });
	}
}

void FCrowdVisualPool::ReleaseVisualActorForHandle(FUnitHandle Handle)
{
	auto It = ActiveVisualActors.find(Handle.Index);
	if (It == ActiveVisualActors.end())
	{
		return;
	}

	ACrowdUnitVisualActor* Actor = It->second;
	ActiveVisualActors.erase(It);

	if (!Actor)
	{
		return;
	}

	Actor->DeactivateVisual();
	FreeVisualActors.push_back(Actor);
}

void FCrowdVisualPool::DeactivateAllVisualActors()
{
	for (auto& Pair : ActiveVisualActors)
	{
		if (Pair.second)
		{
			Pair.second->DeactivateVisual();
			FreeVisualActors.push_back(Pair.second);
		}
	}
	ActiveVisualActors.clear();
}

void FCrowdVisualPool::DestroyVisualActors(UWorld* World, bool bDestroyWorldActors)
{
	for (ACrowdUnitVisualActor* Actor : VisualActors)
	{
		if (!Actor)
		{
			continue;
		}

		Actor->DeactivateVisual();
		if (bDestroyWorldActors && World)
		{
			World->DestroyActor(Actor);
		}
	}

	RenderData.clear();
	VisualActors.clear();
	FreeVisualActors.clear();
	ActiveVisualActors.clear();
}

ACrowdUnitVisualActor* FCrowdVisualPool::AcquireVisualActor(UWorld* World)
{
	if (!FreeVisualActors.empty())
	{
		ACrowdUnitVisualActor* Actor = FreeVisualActors.back();
		FreeVisualActors.pop_back();
		return Actor;
	}

	if (!World)
	{
		return nullptr;
	}

	ACrowdUnitVisualActor* Actor = World->SpawnActor<ACrowdUnitVisualActor>();
	if (Actor)
	{
		VisualActors.push_back(Actor);
	}
	return Actor;
}

USkeletalMesh* FCrowdVisualPool::ResolveVisualSkeletalMesh(
	bool bEnableSkeletalVisuals,
	const FSoftObjectPtr& VisualSkeletalMeshPath)
{
	if (!bEnableSkeletalVisuals)
	{
		return nullptr;
	}

	const FString MeshPath = VisualSkeletalMeshPath.ToString();
	if (MeshPath.empty() || MeshPath == "None")
	{
		CachedVisualSkeletalMesh = nullptr;
		CachedVisualSkeletalMeshPath.clear();
		return nullptr;
	}

	if (CachedVisualSkeletalMesh && CachedVisualSkeletalMeshPath == MeshPath)
	{
		return CachedVisualSkeletalMesh;
	}

	CachedVisualSkeletalMesh = nullptr;
	CachedVisualSkeletalMeshPath = MeshPath;

	ID3D11Device* Device = GEngine ? GEngine->GetRenderer().GetFD3DDevice().GetDevice() : nullptr;
	if (!Device)
	{
		return nullptr;
	}

	CachedVisualSkeletalMesh = FMeshManager::LoadSkeletalMesh(MeshPath, Device);
	return CachedVisualSkeletalMesh;
}

UClass* FCrowdVisualPool::ResolveVisualAnimClass(const TSubclassOf<UAnimInstance>& VisualAnimInstanceClass) const
{
	if (UClass* Class = VisualAnimInstanceClass.Get())
	{
		return Class;
	}

	return UCrowdUnitAnimInstance::StaticClass();
}
