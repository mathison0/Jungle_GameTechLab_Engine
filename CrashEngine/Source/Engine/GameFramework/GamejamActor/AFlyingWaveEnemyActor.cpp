#include "AFlyingWaveEnemyActor.h"
#include "Component/Collision/CircleCollider2DComponent.h"
#include "GameFramework/World.h"

IMPLEMENT_CLASS(AFlyingWaveEnemyActor, AEnemyBaseActor)

void AFlyingWaveEnemyActor::Tick(float DeltaTime)
{
    if (UWorld* World = GetWorld())
    {
        if (World->IsGameplayPaused())
        {
            return;
        }
    }

    AEnemyBaseActor::Tick(DeltaTime);

    AddActorWorldOffset(MoveDirection * MoveSpeed * DeltaTime);

    ElapsedTime += DeltaTime;
    if (ElapsedTime >= LifeTime)
    {
        RequestReturnToPool();
    }
}

void AFlyingWaveEnemyActor::InitDefaultComponents()
{
    Super::InitDefaultComponents();
    ColliderComponent->SetCollisionChannel(ECollisionChannel::FlyingEnemy);
    ColliderComponent->SetGenerateOverlapEvents(true);
    ColliderComponent->SetGenerateHitEvents(false);
}

void AFlyingWaveEnemyActor::InitWave(const FVector& NewDirection, float NewSpeed, float InLifeTime)
{
    MoveDirection = NewDirection;
    MoveSpeed = NewSpeed;
    LifeTime = InLifeTime;
    ElapsedTime = 0.0f;
}
