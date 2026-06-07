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
		if (!IsCrowdUnitVisibleForRender(Unit))
		{
			continue;
		}

		RenderData.push_back({
			FUnitHandle{ Index, Unit.Generation },
			Unit.Team,
			Unit.Archetype.CombatType,
			Unit.State,
			Unit.LOD,
			Unit.Position,
			Unit.Rotation,
			Unit.Velocity,
			Unit.AnimState,
			Unit.AnimTime,
			Unit.Velocity.Length(),
			Unit.CircleAroundDirectionSign,
			Unit.bKnockDownGettingUp,
			true
		});
	}
}

void FCrowdVisualPool::SyncVisualActors(
	ULargeScaleUnitManagerComponent* Manager,
	UWorld* World,
	bool bEnableSkeletalVisuals,
	const FResolveVisualDescFunc& ResolveVisualDesc)
{
	if (!bEnableSkeletalVisuals)
	{
		DeactivateAllVisualActors();
		return;
	}

	TSet<uint32> SeenUnitIndices;
	SeenUnitIndices.reserve(RenderData.size());

	for (const FUnitRenderData& Data : RenderData)
	{
		if (!Data.bVisible || !Data.Handle.IsValid())
		{
			continue;
		}

		const FCrowdVisualDesc VisualDesc = ResolveVisualDesc ? ResolveVisualDesc(Data) : FCrowdVisualDesc();
		USkeletalMesh* VisualMesh = ResolveVisualSkeletalMesh(VisualDesc.SkeletalMeshPath);
		if (!VisualMesh)
		{
			continue;
		}

		UClass* AnimClass = ResolveVisualAnimClass(VisualDesc.AnimInstanceClass);
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

		VisualActor->InitializeVisual(Manager, VisualMesh, AnimClass, VisualDesc.MeleeAnimations);
		VisualActor->SetActorScale(VisualDesc.Scale);
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
	CachedVisualSkeletalMeshes.clear();
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

USkeletalMesh* FCrowdVisualPool::ResolveVisualSkeletalMesh(const FSoftObjectPtr& SkeletalMeshPath)
{
	const FString MeshPath = SkeletalMeshPath.ToString();
	if (MeshPath.empty() || MeshPath == "None")
	{
		return nullptr;
	}

	auto CachedIt = CachedVisualSkeletalMeshes.find(MeshPath);
	if (CachedIt != CachedVisualSkeletalMeshes.end())
	{
		return CachedIt->second;
	}

	ID3D11Device* Device = GEngine ? GEngine->GetRenderer().GetFD3DDevice().GetDevice() : nullptr;
	if (!Device)
	{
		return nullptr;
	}

	USkeletalMesh* VisualMesh = FMeshManager::LoadSkeletalMesh(MeshPath, Device);
	if (VisualMesh)
	{
		CachedVisualSkeletalMeshes[MeshPath] = VisualMesh;
	}
	return VisualMesh;
}

UClass* FCrowdVisualPool::ResolveVisualAnimClass(const TSubclassOf<UAnimInstance>& AnimInstanceClass) const
{
	if (UClass* Class = AnimInstanceClass.Get())
	{
		return Class;
	}

	return UCrowdUnitAnimInstance::StaticClass();
}
