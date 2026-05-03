#pragma once

#include "GameFramework/World.h"
#include "GameFramework/AActor.h"

#include <algorithm>
#include <functional>
#include <type_traits>
#include <utility>

/*
    TActorPool owns a reusable set of actors of one concrete type.
    Actors stay registered in the world; Release only deactivates and hides them.
*/
template <typename TActor>
class TActorPool
{
    static_assert(std::is_base_of_v<AActor, TActor>, "TActorPool<TActor>: TActor must derive from AActor.");

public:
    using FActorCallback = std::function<void(TActor*)>;

    explicit TActorPool(UWorld* InWorld)
        : World(InWorld)
    {
    }

    ~TActorPool()
    {
        DestroyAll();
    }

    TActorPool(const TActorPool&) = delete;
    TActorPool& operator=(const TActorPool&) = delete;

    void Warmup(uint32 Count, const FActorCallback& Initialize = nullptr)
    {
        for (uint32 Index = 0; Index < Count; ++Index)
        {
            TActor* Actor = CreateActor();
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

    TActor* Acquire(const FActorCallback& Activate = nullptr)
    {
        TActor* Actor = nullptr;
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

    template <typename TActivate>
    TActor* AcquireWith(TActivate&& Activate)
    {
        return Acquire(FActorCallback(std::forward<TActivate>(Activate)));
    }

    void Release(TActor* Actor, const FActorCallback& Deactivate = nullptr)
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

    void ReleaseAll(const FActorCallback& Deactivate = nullptr)
    {
        TArray<TActor*> ActorsToRelease = Active;
        for (TActor* Actor : ActorsToRelease)
        {
            Release(Actor, Deactivate);
        }
    }

    void DestroyAll()
    {
        if (!World)
        {
            Available.clear();
            Active.clear();
            return;
        }

        for (TActor* Actor : Active)
        {
            //World->DestroyActor(Actor);
        }
        for (TActor* Actor : Available)
        {
            //World->DestroyActor(Actor);
        }

        Active.clear();
        Available.clear();
    }

    uint32 GetActiveCount() const { return static_cast<uint32>(Active.size()); }
    uint32 GetAvailableCount() const { return static_cast<uint32>(Available.size()); }
    uint32 GetTotalCount() const { return GetActiveCount() + GetAvailableCount(); }

private:
    TActor* CreateActor()
    {
        if (!World)
        {
            return nullptr;
        }

        TActor* Actor = World->template SpawnActor<TActor>();
        DeactivateActor(Actor);
        return Actor;
    }

    static void ActivateActor(TActor* Actor)
    {
        if (!Actor)
        {
            return;
        }

		Actor->Activate();
    }

    static void DeactivateActor(TActor* Actor)
    {
        if (!Actor)
        {
            return;
        }
        Actor->Deactivate();
    }

    UWorld* World = nullptr;
    TArray<TActor*> Available;
    TArray<TActor*> Active;
};
