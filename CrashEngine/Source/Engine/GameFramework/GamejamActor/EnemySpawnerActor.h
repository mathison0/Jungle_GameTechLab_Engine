#pragma once
#include "GameFramework/AActor.h"

class FActorPoolManager;
class UScriptComponent;

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
    AEnemySpawnerActor() = default;

    void BeginPlay() override;
    void Tick(float DeltaTime) override;
    void EndPlay() override;
    void InitDefaultComponents() override;

    uint32 GetDesiredEnemyCount(float Time) const;
    float GetSpawnRate(float Time) const;

    static const TArray<FEnemySpawnPhase> SpawnTable;

private:
    const FEnemySpawnPhase& GetPhase(float Time) const;
    FVector GetSpawnPosition(const FVector& PlayerPosition);

private:
    UScriptComponent* ScriptComponent = nullptr;

    FActorPoolManager* PoolManager = nullptr;
    float GameTime = 0.0f;
    float SpawnBudget;

    uint32 DesiredEnemyCount;

    float SpawnRate;
    float MinSpawnRadius = 30.0f;
    float MaxSpawnRadius = 50.0f;
};
