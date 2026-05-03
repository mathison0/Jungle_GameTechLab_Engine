#include "AFlyingWaveEnemyActor.h"
#include "Component/Collision/CircleCollider2DComponent.h"

IMPLEMENT_CLASS(AFlyingWaveEnemyActor, AActor);

void AFlyingWaveEnemyActor::Tick(float DeltaTime)
{
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
