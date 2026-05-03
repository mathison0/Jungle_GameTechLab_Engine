#include "EnemySpawnerActor.h"
#include "GameFramework/GamejamActor/EnemyBaseActor.h"
#include "GameFramework/World.h"
#include "GameFramework/ActorPoolManager.h"
#include "Component/ScriptComponent.h"
#include "Core/Random.h"

IMPLEMENT_CLASS(AEnemySpawnerActor, AActor)

const TArray<FEnemySpawnPhase> AEnemySpawnerActor::SpawnTable = {
	//TimeThreshold / DesiredCount / SpawnRate
    { 30.0f, 30, 5.0f },
    { 60.0f, 60, 10.0f },
    { 120.0f, 100, 15.0f },
    { FLT_MAX, 150, 20.0f }
};

void AEnemySpawnerActor::BeginPlay()
{
    AActor::BeginPlay();
	PoolManager = GetWorld()->GetPoolManager();
}

void AEnemySpawnerActor::Tick(float DeltaTime)
{
    GameTime += DeltaTime;

    DesiredEnemyCount = GetDesiredEnemyCount(GameTime);
    SpawnRate = GetSpawnRate(GameTime);
    uint32 CurrentEnemyCount = PoolManager->GetActiveCount<AEnemyBaseActor>();

    SpawnBudget += SpawnRate * DeltaTime;

    while (SpawnBudget >= 1.0f && CurrentEnemyCount < DesiredEnemyCount)
    {
        AEnemyBaseActor* NewEnemyActor = PoolManager->Acquire<AEnemyBaseActor>();
        FVector SpawnPosition = FVector(5,0,0);
        NewEnemyActor->SetActorLocation(SpawnPosition);

        CurrentEnemyCount++;
        SpawnBudget -= 1.0f;
    }
}

void AEnemySpawnerActor::EndPlay()
{
    PoolManager = nullptr;
}

void AEnemySpawnerActor::InitDefaultComponents()
{
    ScriptComponent = AddComponent<UScriptComponent>();
    ScriptComponent->SetScriptPath("EnemySpawner.lua");
}

uint32 AEnemySpawnerActor::GetDesiredEnemyCount(float Time) const
{
    return GetPhase(Time).DesiredCount;
}

float AEnemySpawnerActor::GetSpawnRate(float Time) const
{
    return GetPhase(Time).SpawnRate;
}

const FEnemySpawnPhase& AEnemySpawnerActor::GetPhase(float Time) const
{
    for (const auto& Phase : SpawnTable)
    {
        if (Time < Phase.TimeThreshold)
        {
            return Phase;
        }
    }

    return SpawnTable.back(); // 안전 fallback
}

FVector AEnemySpawnerActor::GetSpawnPosition(const FVector& PlayerPosition)
{
    float Angle = FRandom::Angle();
    float Distance = FRandom::Range(MinSpawnRadius, MaxSpawnRadius);

	FVector Offset(
        cos(Angle) * Distance,
        sin(Angle) * Distance,
        0.0f);

    return PlayerPosition + Offset;
}

