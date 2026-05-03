#include "TankActor.h"
#include "Engine/Component/Collision/CircleCollider2DComponent.h"
#include "Engine/Component/StaticMeshComponent.h"
#include "Engine/Runtime/Engine.h"
#include "Engine/Component/ScriptComponent.h"
#include "GameFramework/World.h"
#include "Object/ObjectFactory.h"

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
    SetFName("Tank");
    auto Collider = AddComponent<UCircleCollider2DComponent>();
    SetRootComponent(Collider);
    Collider->SetFName({ "RootComponent" });

    auto Script = AddComponent<UScriptComponent>();
    Script->SetScriptPath("TankScript.lua");

    auto HeadMainGun = AddComponent<UStaticMeshComponent>();
    RootComponent->AddChild(HeadMainGun);
    ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
    UStaticMesh* Asset = FObjManager::LoadObjStaticMesh(FPaths::ContentRelativePath("Models/_Basic/Cube.OBJ"), Device);
    HeadMainGun->SetStaticMesh(Asset);
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
    ULevel* Level = GetLevel();
    if (!World || !Level)
    {
        return nullptr;
    }

    AProjectileActor* Projectile = UObjectManager::Get().CreateObject<AProjectileActor>(Level);
    if (!Projectile)
    {
        return nullptr;
    }

    Projectile->InitDefaultComponents();
    World->AddActor(Projectile);
    return Projectile;
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
