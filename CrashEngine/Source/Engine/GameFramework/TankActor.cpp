#include "TankActor.h"
#include "Engine/Component/Collision/CircleCollider2DComponent.h"
#include "Engine/Component/StaticMeshComponent.h"
#include "Engine/Runtime/Engine.h"
#include "Engine/Component/ScriptComponent.h"
#include "GameFramework/ActorPoolManager.h"
#include "GameFramework/World.h"

IMPLEMENT_CLASS(ATankActor, AActor)

void ATankActor::BeginPlay()
{
    CacheHeadMainGunIndex();
    GetHeadGunProjectile();
    AActor::BeginPlay();
}

void ATankActor::BindScriptFunctions(UScriptComponent& ScriptComponent)
{
    if (ScriptComponent.GetScriptPath() != "TankScript.lua")
    {
        return;
    }

    CacheHeadMainGunIndex();
    ScriptComponent.BindFunction("FireHeadMainGun",
        [this](sol::variadic_args)
        {
            FireHeadMainGun();
        });
}

void ATankActor::InitDefaultComponents()
{
    SetActorTag("Player");
    auto Collider = AddComponent<UCircleCollider2DComponent>();
    SetRootComponent(Collider);
    Collider->SetFName({ "RootComponent" });
    Collider->SetCollisionChannel(ECollisionChannel::Player);
    Collider->SetGenerateOverlapEvents(true);

    auto PickupSensor = AddComponent<UCircleCollider2DComponent>();
    PickupSensor->SetFName("PickupSensor");
    PickupSensor->SetRadius(Collider->GetRadius() * 3.0f);
    PickupSensor->SetCollisionChannel(ECollisionChannel::PickupSensor);
    PickupSensor->SetGenerateOverlapEvents(true);
    PickupSensor->AttachToComponent(Collider);

    auto Script = AddComponent<UScriptComponent>();
    Script->SetScriptPath("TankScript.lua");

    auto HeadMainGun = AddComponent<UStaticMeshComponent>();
    RootComponent->AddChild(HeadMainGun);
    ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
    UStaticMesh* Asset = FObjManager::LoadObjStaticMesh(FPaths::ContentRelativePath("Models/_Basic/Cube.OBJ"), Device);
    HeadMainGun->SetStaticMesh(Asset);
    HeadMainGun->SetReceivesDecals(false);
    HeadMainGun->SetFName("HeadMainGun");

    HeadGunProjectile = FObjManager::LoadObjStaticMesh(FPaths::ContentRelativePath("Models/_Basic/Sphere.OBJ"), Device);

    CacheHeadMainGunIndex();
}

void ATankActor::CacheHeadMainGunIndex()
{
    HeadMainGunIndex = -1;

    for (int i = 0; i < static_cast<int>(OwnedComponents.size()); i++)
    {
        UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(OwnedComponents[i]);
        if (!StaticMeshComponent || StaticMeshComponent->GetFName().ToString() != "HeadMainGun")
        {
            continue;
        }

        HeadMainGunIndex = i;
        break;
    }
}

UStaticMesh* ATankActor::GetHeadGunProjectile()
{
    if (HeadGunProjectile)
    {
        return HeadGunProjectile;
    }

    if (!GEngine)
    {
        return nullptr;
    }

    ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
    HeadGunProjectile = FObjManager::LoadObjStaticMesh(FPaths::ContentRelativePath("Models/_Basic/Sphere.OBJ"), Device);
    return HeadGunProjectile;
}

AProjectileActor* ATankActor::GetProjectileActor()
{
    UWorld* World = GetWorld();
    if (!World || !World->GetPoolManager())
    {
        return nullptr;
    }

    return World->GetPoolManager()->Acquire<AProjectileActor>();
}

void ATankActor::FireHeadMainGun()
{
    if (HeadMainGunIndex < 0 || HeadMainGunIndex >= static_cast<int>(OwnedComponents.size()))
    {
        return;
    }

    auto HeadMainGun = GetComponent<UStaticMeshComponent>(HeadMainGunIndex);
    if (!HeadMainGun)
    {
        return;
    }

    auto Projectile = GetProjectileActor();
    if (!Projectile)
    {
        return;
    }

    Projectile->SetProjectileSetting({ GetHeadGunProjectile(),
                                       1.f,
                                       HeadMainGun->GetWorldLocation() + HeadMainGun->GetForwardVector() * 3.f,
                                       HeadMainGun->GetWorldRotation().ToRotator(),
                                       HeadMainGun->GetForwardVector(),
                                       10.f

    });
    Projectile->Fire();
}
