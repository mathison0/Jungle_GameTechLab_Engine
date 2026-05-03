#pragma once

#include "GameFramework/ActorPool.h"

#include <functional>
#include <memory>
#include <typeindex>
#include <type_traits>
#include <utility>

/*
    FActorPoolManager keeps one actor pool per concrete actor type.
    Pooled actors can call RequestReturnToPool(), and the manager will return
    them to the pool that acquired them.
*/
class FActorPoolManager
{
public:
    explicit FActorPoolManager(UWorld* InWorld)
        : World(InWorld)
    {
    }

	FActorPoolManager() = default;

    ~FActorPoolManager()
    {
        DestroyAll();
    }

    FActorPoolManager(const FActorPoolManager&) = delete;
    FActorPoolManager& operator=(const FActorPoolManager&) = delete;

    void SetWorld(UWorld* InWorld)
    {
        if (World == InWorld)
        {
            return;
        }

        DestroyAll();
        World = InWorld;
    }

    template <typename TActor>
    void Warmup(uint32 Count, const typename TActorPool<TActor>::FActorCallback& Initialize = nullptr)
    {
        GetOrCreateEntry<TActor>().Pool.Warmup(Count, Initialize);
    }

    template <typename TActor>
    TActor* Acquire(const typename TActorPool<TActor>::FActorCallback& Activate = nullptr)
    {
        TPoolEntry<TActor>& Entry = GetOrCreateEntry<TActor>();
        TActor* Actor = Entry.Pool.Acquire(Activate);
        RegisterActiveActor(Actor, &Entry);
        return Actor;
    }

    template <typename TActor, typename TActivate>
    TActor* AcquireWith(TActivate&& Activate)
    {
        return Acquire<TActor>(typename TActorPool<TActor>::FActorCallback(std::forward<TActivate>(Activate)));
    }

    template <typename TActor>
    void Release(TActor* Actor, const typename TActorPool<TActor>::FActorCallback& Deactivate = nullptr)
    {
        if (!Actor)
        {
            return;
        }

        TPoolEntry<TActor>& Entry = GetOrCreateEntry<TActor>();
        ActiveActorToPool.erase(Actor);
        Entry.ReleaseTyped(Actor, Deactivate);
    }

    void Release(AActor* Actor)
    {
        if (!Actor)
        {
            return;
        }

        auto It = ActiveActorToPool.find(Actor);
        if (It == ActiveActorToPool.end())
        {
            return;
        }

        IPoolEntry* Entry = It->second;
        ActiveActorToPool.erase(It);
        Entry->ReleaseActor(Actor);
    }

    template <typename TActor>
    void SetDefaultDeactivate(const typename TActorPool<TActor>::FActorCallback& Deactivate)
    {
        GetOrCreateEntry<TActor>().DefaultDeactivate = Deactivate;
    }

    void DestroyAll()
    {
        for (AActor* Actor : DelegateBoundActors)
        {
            if (Actor)
            {
                Actor->OnPoolReturnRequested.RemoveDynamic(this);
            }
        }

        DelegateBoundActors.clear();
        ActiveActorToPool.clear();

        for (auto& Pair : Pools)
        {
            if (Pair.second)
            {
                Pair.second->DestroyAll();
            }
        }

        Pools.clear();
    }

    template <typename TActor>
    uint32 GetActiveCount() const
    {
        const TPoolEntry<TActor>* Entry = FindEntry<TActor>();
        return Entry ? Entry->Pool.GetActiveCount() : 0;
    }

    template <typename TActor>
    uint32 GetAvailableCount() const
    {
        const TPoolEntry<TActor>* Entry = FindEntry<TActor>();
        return Entry ? Entry->Pool.GetAvailableCount() : 0;
    }

    template <typename TActor>
    uint32 GetTotalCount() const
    {
        const TPoolEntry<TActor>* Entry = FindEntry<TActor>();
        return Entry ? Entry->Pool.GetTotalCount() : 0;
    }

private:
    struct IPoolEntry
    {
        virtual ~IPoolEntry() = default;
        virtual void ReleaseActor(AActor* Actor) = 0;
        virtual void DestroyAll() = 0;
    };

    template <typename TActor>
    struct TPoolEntry final : IPoolEntry
    {
        explicit TPoolEntry(UWorld* InWorld)
            : Pool(InWorld)
        {
        }

        void ReleaseActor(AActor* Actor) override
        {
            ReleaseTyped(static_cast<TActor*>(Actor), nullptr);
        }

        void ReleaseTyped(TActor* Actor, const typename TActorPool<TActor>::FActorCallback& Deactivate)
        {
            if (Deactivate)
            {
                Pool.Release(Actor, Deactivate);
            }
            else
            {
                Pool.Release(Actor, DefaultDeactivate);
            }
        }

        void DestroyAll() override
        {
            Pool.DestroyAll();
        }

        TActorPool<TActor> Pool;
        typename TActorPool<TActor>::FActorCallback DefaultDeactivate = nullptr;
    };

    template <typename TActor>
    TPoolEntry<TActor>& GetOrCreateEntry()
    {
        static_assert(std::is_base_of_v<AActor, TActor>, "FActorPoolManager<TActor>: TActor must derive from AActor.");

        const std::type_index TypeKey(typeid(TActor));
        auto It = Pools.find(TypeKey);
        if (It == Pools.end())
        {
            auto Entry = std::make_unique<TPoolEntry<TActor>>(World);
            TPoolEntry<TActor>* RawEntry = Entry.get();
            Pools.emplace(TypeKey, std::move(Entry));
            return *RawEntry;
        }

        return *static_cast<TPoolEntry<TActor>*>(It->second.get());
    }

    template <typename TActor>
    const TPoolEntry<TActor>* FindEntry() const
    {
        const std::type_index TypeKey(typeid(TActor));
        auto It = Pools.find(TypeKey);
        if (It == Pools.end())
        {
            return nullptr;
        }

        return static_cast<const TPoolEntry<TActor>*>(It->second.get());
    }

    void RegisterActiveActor(AActor* Actor, IPoolEntry* Entry)
    {
        if (!Actor || !Entry)
        {
            return;
        }

        ActiveActorToPool[Actor] = Entry;
        if (DelegateBoundActors.insert(Actor).second)
        {
            Actor->OnPoolReturnRequested.AddDynamic(this, &FActorPoolManager::HandleActorPoolReturnRequested);
        }
    }

    void HandleActorPoolReturnRequested(AActor* Actor)
    {
        Release(Actor);
    }

    UWorld* World = nullptr;
    TMap<std::type_index, std::unique_ptr<IPoolEntry>> Pools;
    TMap<AActor*, IPoolEntry*> ActiveActorToPool;
    TSet<AActor*> DelegateBoundActors;
};
