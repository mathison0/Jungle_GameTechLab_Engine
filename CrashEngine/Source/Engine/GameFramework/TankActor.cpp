#include "TankActor.h"
#include "Collision/CollisionChannels.h"
#include "Core/Logging/LogMacros.h"
#include "Engine/Component/Collision/CircleCollider2DComponent.h"
#include "Engine/Component/StaticMeshComponent.h"
#include "Engine/Runtime/Engine.h"
#include "Engine/Component/ScriptComponent.h"
#include "GameFramework/ActorPoolManager.h"
#include "GameFramework/World.h"

IMPLEMENT_CLASS(ATankActor, AActor)

void ATankActor::BeginPlay()
{
    CacheComponentIndices();
    GetHeadGunProjectile();

    AActor::BeginPlay();
}

void ATankActor::BindScriptFunctions(UScriptComponent& ScriptComponent)
{
    if (ScriptComponent.GetScriptPath() != TankScriptPath)
    {
        return;
    }

    CacheComponentIndices();

    ScriptComponent.BindFunction("FireHeadMainGun",
        [this](sol::object ParamsObject)
        {
            const FTankMainGunFireParams Params = ReadMainGunParamsFromLua(ParamsObject);
            UE_LOG(Tank, Info,
                   "FireHeadMainGun params: Damage=%.2f Speed=%.2f Scale=%.2f ColliderSize=%.2f Pierce=%d",
                   Params.Damage,
                   Params.ProjectileSpeed,
                   Params.ProjectileScale,
                   Params.ColliderSize,
                   Params.Pierce);
            FireHeadMainGun(Params);
        });
}

void ATankActor::InitDefaultComponents()
{
    SetFName("Tank");
    SetActorTag(FName("Player"));

    auto Collider = AddComponent<UCircleCollider2DComponent>();
    SetRootComponent(Collider);
    Collider->SetFName("RootComponent");
    Collider->SetCollisionChannel(ECollisionChannel::Player);

    auto Script = AddComponent<UScriptComponent>();
    Script->SetScriptPath(TankScriptPath);

    auto HeadMainGun = AddComponent<UStaticMeshComponent>();
    RootComponent->AddChild(HeadMainGun);
    HeadMainGun->SetFName(HeadMainGunName);

    if (!GEngine)
    {
        CacheComponentIndices();
        return;
    }

    ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
    if (!Device)
    {
        CacheComponentIndices();
        return;
    }

    UStaticMesh* HeadAsset =
        FObjManager::LoadObjStaticMesh(
            FPaths::ContentRelativePath("Models/_Basic/Cube.OBJ"),
            Device);

    HeadMainGun->SetStaticMesh(HeadAsset);

    HeadGunProjectile =
        FObjManager::LoadObjStaticMesh(
            FPaths::ContentRelativePath("Models/_Basic/Sphere.OBJ"),
            Device);

    CacheComponentIndices();
}

void ATankActor::CacheComponentIndices()
{
    HeadMainGunIndex = -1;

    for (int32 i = 0; i < static_cast<int32>(OwnedComponents.size()); ++i)
    {
        UActorComponent* Component = OwnedComponents[i];
        if (!Component)
        {
            continue;
        }

        UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Component);
        if (!StaticMeshComponent)
        {
            continue;
        }

        if (StaticMeshComponent->GetFName().ToString() == HeadMainGunName)
        {
            HeadMainGunIndex = i;
            break;
        }
    }
}

UStaticMeshComponent* ATankActor::GetHeadMainGun()
{
    const auto IsHeadMainGunComponent = [](UActorComponent* Component) -> bool
    {
        UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Component);
        return Mesh && Mesh->GetFName().ToString() == HeadMainGunName;
    };

    if (HeadMainGunIndex >= 0 &&
        HeadMainGunIndex < static_cast<int32>(OwnedComponents.size()) &&
        IsHeadMainGunComponent(OwnedComponents[HeadMainGunIndex]))
    {
        return Cast<UStaticMeshComponent>(OwnedComponents[HeadMainGunIndex]);
    }

    CacheComponentIndices();

    if (HeadMainGunIndex >= 0 &&
        HeadMainGunIndex < static_cast<int32>(OwnedComponents.size()) &&
        IsHeadMainGunComponent(OwnedComponents[HeadMainGunIndex]))
    {
        return Cast<UStaticMeshComponent>(OwnedComponents[HeadMainGunIndex]);
    }

    return nullptr;
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
    if (!Device)
    {
        return nullptr;
    }

    HeadGunProjectile =
        FObjManager::LoadObjStaticMesh(
            FPaths::ContentRelativePath("Models/_Basic/Sphere.OBJ"),
            Device);

    return HeadGunProjectile;
}

AProjectileActor* ATankActor::AcquireProjectile()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(Tank, Warning, "AcquireProjectile failed: World is null");
        return nullptr;
    }

    if (World->GetPoolManager())
    {
        AProjectileActor* Projectile = World->GetPoolManager()->Acquire<AProjectileActor>();
        if (Projectile)
        {
            UE_LOG(Tank, Info, "AcquireProjectile succeeded from pool");
            return Projectile;
        }
    }

    AProjectileActor* Projectile = World->SpawnActor<AProjectileActor>();
    if (!Projectile)
    {
        UE_LOG(Tank, Warning, "AcquireProjectile failed: spawn failed");
        return nullptr;
    }

    UE_LOG(Tank, Info, "AcquireProjectile succeeded by spawn");
    return Projectile;
}

FTankMainGunFireParams ATankActor::ReadMainGunParamsFromLua(sol::object ParamsObject) const
{
    FTankMainGunFireParams Params;

    if (!ParamsObject.valid() || !ParamsObject.is<sol::table>())
    {
        return Params;
    }

    sol::table Table = ParamsObject.as<sol::table>();

    Params.Damage = Table.get_or("Damage", 10.0f);
    Params.ProjectileSpeed = Table.get_or("ProjectileSpeed", 20.0f);
    Params.ProjectileScale = Table.get_or("ProjectileScale", 1.0f);
    Params.ColliderSize = Table.get_or("ColliderSize", Params.ProjectileScale);
    Params.Pierce = Table.get_or("Pierce", 0);

    return Params;
}

void ATankActor::FireHeadMainGun(const FTankMainGunFireParams& Params)
{
    UStaticMeshComponent* HeadMainGun = GetHeadMainGun();
    if (!HeadMainGun)
    {
        UE_LOG(Tank, Warning, "FireHeadMainGun failed: HeadMainGun not found");
        return;
    }

    AProjectileActor* Projectile = AcquireProjectile();
    if (!Projectile)
    {
        UE_LOG(Tank, Warning, "FireHeadMainGun failed: projectile acquire failed");
        return;
    }

    const FVector FireDir = HeadMainGun->GetForwardVector();
    const FVector SpawnLocation = HeadMainGun->GetWorldLocation() + FireDir * 3.0f;

    Projectile->SetProjectileSetting({
        GetHeadGunProjectile(),
        Params.ColliderSize,
        SpawnLocation,
        HeadMainGun->GetWorldRotation().ToRotator(),
        FireDir,
        Params.ProjectileSpeed
    });

    Projectile->SetActorScale(FVector(
        Params.ProjectileScale,
        Params.ProjectileScale,
        Params.ProjectileScale));

    Projectile->SetDamage(Params.Damage);
    Projectile->SetPierceCount(Params.Pierce);
    UE_LOG(Tank, Info, "Projectile setting applied");

    Projectile->Fire();
}
