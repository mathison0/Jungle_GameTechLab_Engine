#pragma once
#include "GameFramework/AActor.h"

class FActorPoolManager;

struct FEnemySpawnPhase
{
    float TimeThreshold;
    uint32 DesiredCount;
    float SpawnRate;
};

class AEnemySpawnerActor : public AActor
{
public:
    DECLARE_CLASS(AEnemySpawnerActor, AActor)
    void BeginPlay();
    void Tick(float DeltaTime);
    void EndPlay();

    uint32 GetDesiredEnemyCount(float Time) const;
    float GetSpawnRate(float Time) const;

	static const TArray<FEnemySpawnPhase> SpawnTable;

private:
    void DeclineEnemyCount(AActor* ReturnActor);

private:


    FActorPoolManager* PoolManager = nullptr;
    float GameTime = 0.0f;
    float SpawnBudget;

    uint32 DesiredEnemyCount;

    float SpawnRate;
    float MinSpawnRadius;
    float MaxSpawnRadius;
};
