#include "EnemySpawnerActor.h"
#include "GameFramework/GamejamActor/EnemyBaseActor.h"
#include "GameFramework/World.h"
#include "GameFramework/ActorPoolManager.h"

IMPLEMENT_CLASS(AEnemySpawnerActor, AActor)

static const TArray<FEnemySpawnPhase> SpawnTable = {
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
        PoolManager->Acquire<AEnemyBaseActor>();
        CurrentEnemyCount++;
        SpawnBudget -= 1.0f;
    }
}

void AEnemySpawnerActor::EndPlay()
{
    PoolManager = nullptr;
}

uint32 AEnemySpawnerActor::GetDesiredEnemyCount(float Time) const
{
    if (Time < 30.0f)
        return 30;
    if (Time < 60.0f)
        return 60;
    if (Time < 120.0f)
        return 100;
    return 150;
}

float AEnemySpawnerActor::GetSpawnRate(float Time) const
{
    if (Time < 30.0f)
        return 5.0f;
    if (Time < 60.0f)
        return 10.0f;
    if (Time < 120.0f)
        return 15.0f;
    return 20.0f;
}

