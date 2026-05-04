#include "TankActor.h"

#include "Collision/CollisionChannels.h"
#include "Core/Logging/LogMacros.h"
#include "Engine/Component/Collision/CircleCollider2DComponent.h"
#include "Engine/Component/ScriptComponent.h"
#include "Engine/Component/StaticMeshComponent.h"
#include "Engine/Runtime/Engine.h"
#include "Engine/Scripting/LuaEngineBinding.h"
#include "Engine/Scripting/LuaScriptTypes.h"
#include "GameFramework/ActorPoolManager.h"
#include "GameFramework/HomingMissileActor.h"
#include "GameFramework/World.h"

#include <algorithm>
#include <string>

IMPLEMENT_CLASS(ATankActor, AActor)

namespace
{
FString MakeIndexedComponentName(const FString& Prefix, int32 Index)
{
    return Prefix + std::to_string(Index > 0 ? Index : 0);
}

FString MakeMachineTurretComponentName(const FString& Prefix, int32 Index)
{
    return Prefix + "_MachineTurret_" + std::to_string(Index > 0 ? Index : 0);
}

int32 MaxInt32(int32 A, int32 B)
{
    return A > B ? A : B;
}

sol::object FindFirstTableArgument(sol::variadic_args Args)
{
    for (const sol::object& Arg : Args)
    {
        if (Arg.is<sol::table>())
        {
            return Arg;
        }
    }
    return sol::object();
}
}

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

    ScriptComponent.BindFunction("EquipWeaponVisual",
        [this](const std::string& WeaponId, int32 Level)
        {
            EquipWeaponVisual(FString(WeaponId), Level);
        });

    ScriptComponent.BindFunction("FireLinearProjectile",
        [this](const std::string& WeaponId, sol::object ParamsObject, int32 MuzzleIndex)
        {
            const FTankWeaponAttackParams Params = ReadWeaponAttackParamsFromLua(ParamsObject);
            FireLinearProjectile(FString(WeaponId), Params, MuzzleIndex);
        });

    ScriptComponent.BindFunction("FireHomingMissile",
        [this](const std::string& WeaponId, sol::object ParamsObject, int32 MuzzleIndex, const FLuaActorHandle& TargetActor)
        {
            const FTankWeaponAttackParams Params = ReadWeaponAttackParamsFromLua(ParamsObject);
            FireHomingMissile(FString(WeaponId), Params, MuzzleIndex, TargetActor.Resolve());
        });

    ScriptComponent.BindFunction("ApplyTargetDamage",
        [this](const std::string& WeaponId, sol::object ParamsObject, const FLuaActorHandle& TargetActor)
        {
            const FTankWeaponAttackParams Params = ReadWeaponAttackParamsFromLua(ParamsObject);
            ApplyTargetDamage(FString(WeaponId), Params, TargetActor.Resolve());
        });

    ScriptComponent.BindFunction("NotifyInstantHit",
        [this](const std::string& WeaponId, sol::object ParamsObject, int32 SlotIndex, const FLuaActorHandle& TargetActor)
        {
            const FTankWeaponAttackParams Params = ReadWeaponAttackParamsFromLua(ParamsObject);
            NotifyInstantHit(FString(WeaponId), Params, SlotIndex, TargetActor.Resolve());
        });

    ScriptComponent.BindFunction("ApplyAreaDamage",
        [this](const std::string& WeaponId, sol::object ParamsObject, sol::object CenterObject, float Radius)
        {
            const FTankWeaponAttackParams Params = ReadWeaponAttackParamsFromLua(ParamsObject);
            FVector Center = GetActorLocation();
            ReadLuaVec3(CenterObject, Center);
            ApplyAreaDamage(FString(WeaponId), Params, Center, Radius);
        });

    ScriptComponent.BindFunction("SpawnInstallable",
        [this](const std::string& WeaponId, sol::object ParamsObject, sol::object PositionObject)
        {
            const FTankWeaponAttackParams Params = ReadWeaponAttackParamsFromLua(ParamsObject);
            FVector Position = GetActorLocation();
            ReadLuaVec3(PositionObject, Position);
            SpawnInstallable(FString(WeaponId), Params, Position);
        });

    ScriptComponent.BindFunction("SpawnSummon",
        [this](const std::string& WeaponId, sol::object ParamsObject, int32 MuzzleIndex)
        {
            const FTankWeaponAttackParams Params = ReadWeaponAttackParamsFromLua(ParamsObject);
            SpawnSummon(FString(WeaponId), Params, MuzzleIndex);
        });

    ScriptComponent.BindFunction("FireHeadMainGun",
        [this](sol::variadic_args Args)
        {
            const FTankWeaponAttackParams Params = ReadWeaponAttackParamsFromLua(FindFirstTableArgument(Args));
            FireHeadMainGun(Params);
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
    Script->SetScriptPath(TankScriptPath);

    UStaticMeshComponent* HeadMainGun = AddComponent<UStaticMeshComponent>();
    RootComponent->AddChild(HeadMainGun);
    HeadMainGun->SetFName(HeadMainGunName);
    HeadMainGun->SetRelativeLocation(FVector(1.2f, 0.0f, 0.5f));
    HeadMainGun->SetRelativeScale(FVector(1.0f, 0.35f, 0.35f));
    HeadMainGun->SetStaticMesh(GetBasicMesh("Models/_Basic/Cube.OBJ"));

    HeadGunProjectile = GetBasicMesh("Models/_Basic/Sphere.OBJ");

    CacheComponentIndices();
}

FTankWeaponAttackParams ATankActor::ReadWeaponAttackParamsFromLua(sol::object ParamsObject) const
{
    FTankWeaponAttackParams Params;

    if (!ParamsObject.valid() || ParamsObject == sol::nil || !ParamsObject.is<sol::table>())
    {
        return Params;
    }

    sol::table Table = ParamsObject.as<sol::table>();

    Params.Damage = Table.get_or("Damage", 10.0f);

    Params.ProjectileSpeed = Table.get_or("ProjectileSpeed", 20.0f);
    Params.ProjectileScale = Table.get_or("ProjectileScale", 1.0f);
    Params.ColliderSize = Table.get_or("ColliderSize", Params.ProjectileScale);
    Params.Pierce = Table.get_or("Pierce", 0);

    Params.TurnSpeed = Table.get_or("TurnSpeed", 120.0f);
    Params.ImpactRadius = Table.get_or("ImpactRadius", 0.0f);

    Params.Range = Table.get_or("Range", 10000.0f);
    Params.Radius = Table.get_or("Radius", 0.0f);
    Params.TickInterval = Table.get_or("TickInterval", 1.0f);
    Params.TargetCount = Table.get_or("TargetCount", 1);

    Params.Duration = Table.get_or("Duration", 0.0f);

    return Params;
}

void ATankActor::EquipWeaponVisual(const FString& WeaponId, int32 Level)
{
    if (WeaponId == "MainCannon")
    {
        EnsureWeaponVisualComponent(HeadMainGunName, "Models/_Basic/Cube.OBJ", FVector(1.2f, 0.0f, 0.5f), FVector(1.0f, 0.35f, 0.35f));
        CacheComponentIndices();
        UE_LOG(Tank, Info, "EquipWeaponVisual MainCannon Level=%d", Level);
        return;
    }

    if (WeaponId == "MachineTurret")
    {
        const int32 VisualCount = Level >= 3 ? 4 : MaxInt32(1, Level);
        for (int32 Index = 0; Index < VisualCount; ++Index)
        {
            const float Lane = static_cast<float>(Index) - static_cast<float>(VisualCount - 1) * 0.5f;
            EnsureWeaponVisualComponent(
                MakeMachineTurretComponentName("Visual", Index),
                "Models/_Basic/Cube.OBJ",
                FVector(0.7f, Lane * 0.45f, 0.65f),
                FVector(0.35f, 0.25f, 0.25f));
            EnsureWeaponVisualComponent(
                MakeMachineTurretComponentName("Muzzle", Index),
                "Models/_Basic/Sphere.OBJ",
                FVector(1.0f, Lane * 0.45f, 0.65f),
                FVector(0.12f, 0.12f, 0.12f));
        }
        UE_LOG(Tank, Info, "EquipWeaponVisual MachineTurret Level=%d", Level);
        return;
    }

    if (WeaponId == "Aura")
    {
        EnsureWeaponVisualComponent("AuraVisual", "Models/_Basic/Sphere.OBJ", FVector(0.0f, 0.0f, 0.05f), FVector(0.25f, 0.25f, 0.25f));
        UE_LOG(Tank, Info, "EquipWeaponVisual Aura Level=%d", Level);
        return;
    }

    if (WeaponId == "HomingMissile")
    {
        GetWeaponMuzzle(WeaponId, 0);
        UE_LOG(Tank, Info, "EquipWeaponVisual HomingMissile Level=%d", Level);
        return;
    }

    if (WeaponId == "SatelliteBeam")
    {
        EnsureWeaponVisualComponent("SatelliteBeamVisual", "Models/_Basic/Sphere.OBJ", FVector(0.0f, 0.0f, 1.2f), FVector(0.35f, 0.35f, 0.35f));
        UE_LOG(Tank, Info, "EquipWeaponVisual SatelliteBeam Level=%d", Level);
        return;
    }

    UE_LOG(Tank, Warning, "Unknown WeaponId in EquipWeaponVisual: %s", WeaponId.c_str());
}

void ATankActor::FireLinearProjectile(const FString& WeaponId, const FTankWeaponAttackParams& Params, int32 MuzzleIndex)
{
    USceneComponent* Muzzle = GetWeaponMuzzle(WeaponId, MuzzleIndex);
    if (!Muzzle)
    {
        UE_LOG(Tank, Warning, "FireLinearProjectile failed. Weapon=%s Muzzle=%d", WeaponId.c_str(), MuzzleIndex);
        return;
    }

    AProjectileActor* Projectile = AcquireProjectile();
    if (!Projectile)
    {
        UE_LOG(Tank, Warning, "FireLinearProjectile failed. Projectile acquire failed.");
        return;
    }

    const FVector FireDir = Muzzle->GetForwardVector();
    const FVector SpawnLocation = Muzzle->GetWorldLocation() + FireDir * 3.0f;

    Projectile->SetProjectileSetting({
        GetProjectileMeshForWeapon(WeaponId),
        Params.ColliderSize,
        SpawnLocation,
        Muzzle->GetWorldRotation().ToRotator(),
        FireDir,
        Params.ProjectileSpeed
    });

    Projectile->SetActorScale(FVector(Params.ProjectileScale, Params.ProjectileScale, Params.ProjectileScale));
    Projectile->SetDamage(Params.Damage);
    Projectile->SetPierceCount(Params.Pierce);

    UE_LOG(Tank, Info, "[%s] FireLinearProjectile Muzzle=%d Damage=%.2f Speed=%.2f",
           WeaponId.c_str(), MuzzleIndex, Params.Damage, Params.ProjectileSpeed);

    Projectile->Fire();
}

void ATankActor::FireHomingMissile(const FString& WeaponId, const FTankWeaponAttackParams& Params, int32 MuzzleIndex, AActor* TargetActor)
{
    if (!TargetActor)
    {
        UE_LOG(Tank, Warning, "FireHomingMissile failed. Target is null.");
        return;
    }

    USceneComponent* Muzzle = GetWeaponMuzzle(WeaponId, MuzzleIndex);
    if (!Muzzle)
    {
        UE_LOG(Tank, Warning, "FireHomingMissile failed. Muzzle not found.");
        return;
    }

    AHomingMissileActor* Missile = AcquireHomingMissile();
    if (!Missile)
    {
        UE_LOG(Tank, Warning, "FireHomingMissile failed. Missile acquire failed.");
        return;
    }

    Missile->SetActorLocation(Muzzle->GetWorldLocation() + Muzzle->GetForwardVector() * 3.0f);
    Missile->SetActorRotation(Muzzle->GetWorldRotation().ToRotator());
    Missile->SetActorScale(FVector(Params.ProjectileScale, Params.ProjectileScale, Params.ProjectileScale));
    Missile->SetTargetActor(TargetActor);
    Missile->SetDamage(Params.Damage);
    Missile->SetImpactRadius(Params.ImpactRadius);
    Missile->SetTurnSpeed(Params.TurnSpeed);
    Missile->SetSpeed(Params.ProjectileSpeed);

    UE_LOG(Tank, Info, "[%s] FireHomingMissile Target=%s Damage=%.2f",
           WeaponId.c_str(), TargetActor->GetFName().ToString().c_str(), Params.Damage);

    Missile->Fire();
}

void ATankActor::ApplyTargetDamage(const FString& WeaponId, const FTankWeaponAttackParams& Params, AActor* TargetActor)
{
    if (!TargetActor)
    {
        return;
    }

    UE_LOG(Tank, Info, "[%s] ApplyTargetDamage Damage=%.2f Target=%s",
           WeaponId.c_str(), Params.Damage, TargetActor->GetFName().ToString().c_str());
}

void ATankActor::NotifyInstantHit(const FString& WeaponId, const FTankWeaponAttackParams& Params, int32 SlotIndex, AActor* TargetActor)
{
    if (!TargetActor)
    {
        return;
    }

    UE_LOG(Tank, Info, "[%s] NotifyInstantHit Slot=%d Damage=%.2f Target=%s",
           WeaponId.c_str(), SlotIndex, Params.Damage, TargetActor->GetFName().ToString().c_str());
}

void ATankActor::ApplyAreaDamage(const FString& WeaponId, const FTankWeaponAttackParams& Params, const FVector& Center, float Radius)
{
    UE_LOG(Tank, Info, "[%s] ApplyAreaDamage Damage=%.2f Center=(%.2f, %.2f, %.2f) Radius=%.2f",
           WeaponId.c_str(), Params.Damage, Center.X, Center.Y, Center.Z, Radius);
}

void ATankActor::SpawnInstallable(const FString& WeaponId, const FTankWeaponAttackParams& Params, const FVector& Position)
{
    UE_LOG(Tank, Info, "[%s] SpawnInstallable Damage=%.2f Position=(%.2f, %.2f, %.2f)",
           WeaponId.c_str(), Params.Damage, Position.X, Position.Y, Position.Z);
}

void ATankActor::SpawnSummon(const FString& WeaponId, const FTankWeaponAttackParams& Params, int32 MuzzleIndex)
{
    USceneComponent* Muzzle = GetWeaponMuzzle(WeaponId, MuzzleIndex);
    const FVector Position = Muzzle ? Muzzle->GetWorldLocation() : GetActorLocation();
    UE_LOG(Tank, Info, "[%s] SpawnSummon Damage=%.2f Muzzle=%d Position=(%.2f, %.2f, %.2f)",
           WeaponId.c_str(), Params.Damage, MuzzleIndex, Position.X, Position.Y, Position.Z);
}

void ATankActor::FireHeadMainGun(const FTankWeaponAttackParams& Params)
{
    FireLinearProjectile("MainCannon", Params, 0);
}

void ATankActor::FireHeadMainGun()
{
    FireHeadMainGun(FTankWeaponAttackParams{});
}

void ATankActor::CacheComponentIndices()
{
    HeadMainGunIndex = -1;

    for (int32 i = 0; i < static_cast<int32>(OwnedComponents.size()); ++i)
    {
        UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(OwnedComponents[i]);
        if (StaticMeshComponent && StaticMeshComponent->GetFName().ToString() == HeadMainGunName)
        {
            HeadMainGunIndex = i;
            return;
        }
    }
}

UStaticMeshComponent* ATankActor::GetHeadMainGun()
{
    if (HeadMainGunIndex >= 0 && HeadMainGunIndex < static_cast<int32>(OwnedComponents.size()))
    {
        UStaticMeshComponent* HeadMainGun = Cast<UStaticMeshComponent>(OwnedComponents[HeadMainGunIndex]);
        if (HeadMainGun && HeadMainGun->GetFName().ToString() == HeadMainGunName)
        {
            return HeadMainGun;
        }
    }

    CacheComponentIndices();
    if (HeadMainGunIndex >= 0 && HeadMainGunIndex < static_cast<int32>(OwnedComponents.size()))
    {
        return Cast<UStaticMeshComponent>(OwnedComponents[HeadMainGunIndex]);
    }

    return nullptr;
}

UStaticMesh* ATankActor::GetHeadGunProjectile()
{
    if (!HeadGunProjectile)
    {
        HeadGunProjectile = GetBasicMesh("Models/_Basic/Sphere.OBJ");
    }
    return HeadGunProjectile;
}

UStaticMesh* ATankActor::GetBasicMesh(const FString& RelativePath)
{
    if (!GEngine)
    {
        return nullptr;
    }

    ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
    if (!Device)
    {
        return nullptr;
    }

    return FObjManager::LoadObjStaticMesh(FPaths::ContentRelativePath(RelativePath), Device);
}

UStaticMesh* ATankActor::GetProjectileMeshForWeapon(const FString& WeaponId)
{
    (void)WeaponId;
    return GetHeadGunProjectile();
}

AProjectileActor* ATankActor::GetProjectileActor()
{
    return AcquireProjectile();
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
        if (AProjectileActor* Projectile = World->GetPoolManager()->Acquire<AProjectileActor>())
        {
            return Projectile;
        }
    }

    return World->SpawnActor<AProjectileActor>();
}

AHomingMissileActor* ATankActor::AcquireHomingMissile()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(Tank, Warning, "AcquireHomingMissile failed: World is null");
        return nullptr;
    }

    if (World->GetPoolManager())
    {
        if (AHomingMissileActor* Missile = World->GetPoolManager()->Acquire<AHomingMissileActor>())
        {
            return Missile;
        }
    }

    return World->SpawnActor<AHomingMissileActor>();
}

USceneComponent* ATankActor::GetWeaponMuzzle(const FString& WeaponId, int32 MuzzleIndex)
{
    if (WeaponId == "MainCannon")
    {
        return GetHeadMainGun();
    }

    const int32 SafeIndex = MaxInt32(0, MuzzleIndex);
    FString ComponentName;
    FVector RelativeLocation(1.4f, 0.0f, 0.5f);
    FVector RelativeScale(0.35f, 0.35f, 0.35f);
    FString MeshPath = "Models/_Basic/Cube.OBJ";

    if (WeaponId == "MachineTurret")
    {
        ComponentName = MakeMachineTurretComponentName("Muzzle", SafeIndex);
        const float Lane = static_cast<float>(SafeIndex) - 2.5f;
        RelativeLocation = FVector(0.7f, Lane * 0.35f, 0.65f);
        RelativeScale = FVector(0.12f, 0.12f, 0.12f);
        MeshPath = "Models/_Basic/Sphere.OBJ";
    }
    else if (WeaponId == "HomingMissile")
    {
        ComponentName = MakeIndexedComponentName("HomingMissileMuzzle", SafeIndex);
        RelativeLocation = FVector(0.35f, SafeIndex == 0 ? 0.55f : -0.55f, 0.75f);
        RelativeScale = FVector(0.35f, 0.35f, 0.35f);
    }
    else
    {
        ComponentName = MakeIndexedComponentName(WeaponId + "Muzzle", SafeIndex);
        RelativeLocation = FVector(1.0f, 0.0f, 0.55f);
    }

    return EnsureWeaponVisualComponent(ComponentName, MeshPath, RelativeLocation, RelativeScale);
}

UStaticMeshComponent* ATankActor::FindStaticMeshComponentByName(const FString& ComponentName) const
{
    for (UActorComponent* Component : OwnedComponents)
    {
        UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(Component);
        if (MeshComponent && MeshComponent->GetFName().ToString() == ComponentName)
        {
            return MeshComponent;
        }
    }
    return nullptr;
}

UStaticMeshComponent* ATankActor::EnsureWeaponVisualComponent(
    const FString& ComponentName,
    const FString& MeshPath,
    const FVector& RelativeLocation,
    const FVector& RelativeScale)
{
    if (UStaticMeshComponent* Existing = FindStaticMeshComponentByName(ComponentName))
    {
        Existing->SetRelativeLocation(RelativeLocation);
        Existing->SetRelativeScale(RelativeScale);
        return Existing;
    }

    if (!RootComponent)
    {
        return nullptr;
    }

    UStaticMeshComponent* Component = AddComponent<UStaticMeshComponent>();
    RootComponent->AddChild(Component);
    Component->SetFName(ComponentName);
    Component->SetRelativeLocation(RelativeLocation);
    Component->SetRelativeScale(RelativeScale);
    Component->SetStaticMesh(GetBasicMesh(MeshPath));
    return Component;
}
