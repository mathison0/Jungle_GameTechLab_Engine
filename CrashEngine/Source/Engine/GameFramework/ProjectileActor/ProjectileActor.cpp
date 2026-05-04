#include "ProjectileActor.h"
#include "Engine/Component/Collision/CircleCollider2DComponent.h"
#include "Engine/Component/StaticMeshComponent.h"
#include "Engine/Runtime/Engine.h"
#include "Engine/Component/ProjectileComponent.h"
#include "Engine/Component/ScriptComponent.h"
#include "GameFramework/ActorPoolManager.h"

IMPLEMENT_CLASS(AProjectileActor, AActor)

void AProjectileActor::InitDefaultComponents()
{
    auto ColliderComponent = AddComponent<UCircleCollider2DComponent>();
    ColliderComponent->SetCollisionChannel(ECollisionChannel::Projectile);
    ColliderComponent->SetGenerateOverlapEvents(true);
    SetRootComponent(ColliderComponent);
    auto StaticMeshComponent = AddComponent<UStaticMeshComponent>();
    RootComponent->AddChild(StaticMeshComponent);

    auto MovementComponent = AddComponent<UProjectileMovementComponent2>();
    MovementComponent->SetUpdatedComponent(GetRootComponent());

    for (uint32 i = 0; i < OwnedComponents.size(); i++)
    {
        if (OwnedComponents[i] != ColliderComponent)
            continue;
        ColliderComponentIndex = i;
    }

    for (uint32 i = 0; i < OwnedComponents.size(); i++)
    {
        if (OwnedComponents[i] != MovementComponent)
            continue;
        MovementComponentIndex = i;
    }

    for (uint32 i = 0; i < OwnedComponents.size(); i++)
    {
        if (OwnedComponents[i] != StaticMeshComponent)
            continue;
        StaticMeshComponentIndex = i;
    }

    auto ScriptComponent = AddComponent<UScriptComponent>();
    ScriptComponent->SetScriptPath("ProjectileScript.lua");

	ScriptComponent = AddComponent<UScriptComponent>();
    ScriptComponent->SetScriptPath("Test/TestBullet.lua");
}

void AProjectileActor::BindScriptFunctions(UScriptComponent& ScriptComponent)
{
    ScriptComponent.BindFunction("GetProjectileDamage",
        [this]() -> float
        {
            return Damage;
        });

    ScriptComponent.BindFunction("GetProjectilePierceCount",
        [this]() -> int32
        {
            return PierceCount;
        });

    ScriptComponent.BindFunction("ConsumeProjectilePierce",
        [this]() -> bool
        {
            return ConsumePierce();
        });
}

bool AProjectileActor::ConsumePierce()
{
    if (PierceCount <= 0)
    {
        return false;
    }

    --PierceCount;
    return true;
}


void AProjectileActor::Fire()
{
    if (MovementComponentIndex < 0 || MovementComponentIndex >= static_cast<int>(OwnedComponents.size()))
    {
        return;
    }

    auto movementCompoent = Cast<UProjectileMovementComponent2>(OwnedComponents[MovementComponentIndex]);
    if (!movementCompoent)
    {
        return;
    }

    movementCompoent->SetInitialVelocity(Direction * Speed);
}

void AProjectileActor::SetProjectileSetting(const ProjectileInfo& InProjectileInfo)
{
    if (StaticMeshComponentIndex < 0 || StaticMeshComponentIndex >= static_cast<int>(OwnedComponents.size()) ||
        ColliderComponentIndex < 0 || ColliderComponentIndex >= static_cast<int>(OwnedComponents.size()))
    {
        return;
    }

    UStaticMeshComponent* StaticMeshComponent = GetComponent<UStaticMeshComponent>(StaticMeshComponentIndex);
    UCircleCollider2DComponent* ColliderComponent = GetComponent<UCircleCollider2DComponent>(ColliderComponentIndex);
    if (!StaticMeshComponent || !ColliderComponent)
    {
        return;
    }

    StaticMeshComponent->SetStaticMesh(InProjectileInfo.MeshAsset);

    ColliderComponent->SetRadius(InProjectileInfo.ColliderSize);

    SetActorLocation(InProjectileInfo.Position);
    SetActorRotation(InProjectileInfo.Rotation);

    SetDirection(InProjectileInfo.Dir);
    SetSpeed(InProjectileInfo.Speed);

	SetColiderSize(InProjectileInfo.ColliderSize);
}
