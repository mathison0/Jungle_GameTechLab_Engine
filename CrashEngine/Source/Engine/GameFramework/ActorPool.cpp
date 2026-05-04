#include "GameFramework/ActorPool.h"
#include "GameFramework/World.h"
#include "GameFramework/AActor.h"
#include <algorithm>

FActorPool::FActorPool(UWorld* InWorld, const FString& InClassName)
    : World(InWorld), ActorClassName(InClassName)
{
}

FActorPool::~FActorPool()
{
    DestroyAll();
}

void FActorPool::Warmup(uint32 Count, const FActorCallback& Initialize)
{
    for (uint32 Index = 0; Index < Count; ++Index)
    {
        AActor* Actor = CreateActor();
        if (!Actor)
        {
            return;
        }

        if (Initialize)
        {
            Initialize(Actor);
        }

        DeactivateActor(Actor);
        Available.push_back(Actor);
    }
}

AActor* FActorPool::Acquire(const FActorCallback& Activate)
{
    AActor* Actor = nullptr;
    if (!Available.empty())
    {
        Actor = Available.back();
        Available.pop_back();
    }
    else
    {
        Actor = CreateActor();
    }

    if (!Actor)
    {
        return nullptr;
    }

    ActivateActor(Actor);
    if (Activate)
    {
        Activate(Actor);
    }

    Active.push_back(Actor);
    return Actor;
}

void FActorPool::Release(AActor* Actor, const FActorCallback& Deactivate)
{
    if (!Actor)
    {
        return;
    }

    auto It = std::find(Active.begin(), Active.end(), Actor);
    if (It == Active.end())
    {
        return;
    }

    *It = Active.back();
    Active.pop_back();

    if (Deactivate)
    {
        Deactivate(Actor);
    }

    DeactivateActor(Actor);
    Available.push_back(Actor);
}

void FActorPool::ReleaseAll(const FActorCallback& Deactivate)
{
    TArray<AActor*> ActorsToRelease = Active;
    for (AActor* Actor : ActorsToRelease)
    {
        Release(Actor, Deactivate);
    }
}

void FActorPool::ForgetActor(AActor* Actor)
{
    if (!Actor)
    {
        return;
    }

    auto ActiveIt = std::remove(Active.begin(), Active.end(), Actor);
    Active.erase(ActiveIt, Active.end());

    auto AvailableIt = std::remove(Available.begin(), Available.end(), Actor);
    Available.erase(AvailableIt, Available.end());
}

void FActorPool::DestroyAll()
{
    if (!World)
    {
        Available.clear();
        Active.clear();
        return;
    }

    Active.clear();
    Available.clear();
}

AActor* FActorPool::CreateActor()
{
    if (!World)
    {
        return nullptr;
    }

    AActor* Actor = World->SpawnActorByClass(ActorClassName);
    if (Actor)
    {
        DeactivateActor(Actor);
    }
    return Actor;
}

void FActorPool::ActivateActor(AActor* Actor)
{
    if (Actor)
    {
        Actor->Activate();
    }
}

void FActorPool::DeactivateActor(AActor* Actor)
{
    if (Actor)
    {
        Actor->Deactivate();
        // 비활성화된 액터가 쿼리나 물리에 잡히지 않도록 아주 먼 곳으로 이동
        Actor->SetActorLocation(FVector(9999.0f, 0.0f, 0.0f));
    }
}
