#include "TankActor.h"

#include "Collision/CollisionChannels.h"
#include "Core/Logging/LogMacros.h"
#include "Engine/Component/Collision/CircleCollider2DComponent.h"
#include "Engine/Component/PrimitiveComponent.h"
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
FString MakeWeaponIndexedComponentName(const FString& Prefix, const FString& WeaponId, int32 Index)
{
    return Prefix + "_" + WeaponId + "_" + std::to_string(Index > 0 ? Index : 0);
}

bool StartsWith(const FString& Value, const FString& Prefix)
{
    return Value.size() >= Prefix.size() && Value.compare(0, Prefix.size(), Prefix) == 0;
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
    GetHeadGunProjectile();

    AActor::BeginPlay();
}

void ATankActor::BindScriptFunctions(UScriptComponent& ScriptComponent)
{
    if (ScriptComponent.GetScriptPath() != TankScriptPath)
    {
        return;
    }

    ScriptComponent.BindFunction("EquipWeaponVisual",
        [this](const std::string& WeaponId, int32 Level, sol::object LayoutObject)
        {
            if (!LayoutObject.valid() || LayoutObject == sol::nil || !LayoutObject.is<sol::table>())
            {
                UE_LOG(Tank, Warning, "EquipWeaponVisual failed: layout is not table. Weapon=%s Level=%d",
                       WeaponId.c_str(), Level);
                return;
            }

            EquipWeaponVisualFromLua(FString(WeaponId), Level, LayoutObject.as<sol::table>());
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
    
    ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
    UStaticMesh* Asset = FObjManager::LoadObjStaticMesh(FPaths::ContentRelativePath("Models/Tank/Tank_body.obj"), Device);
    auto BodyMesh = AddComponent<UStaticMeshComponent>();
    BodyMesh->SetFName("BodyMesh");
    BodyMesh->SetStaticMesh(Asset);
    BodyMesh->SetRelativeScale(FVector(0.5f));
    BodyMesh->AttachToComponent(Collider);

    auto Script = AddComponent<UScriptComponent>();
    Script->SetScriptPath(TankScriptPath);

    HeadGunProjectile = GetBasicMesh("Models/Bullet/DefaultBullet.OBJ");
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
    UE_LOG(Tank, Warning, "EquipWeaponVisual(WeaponId, Level) is deprecated. Weapon=%s Level=%d",
           WeaponId.c_str(), Level);
}

void ATankActor::EquipWeaponVisualFromLua(const FString& WeaponId, int32 Level, sol::table LayoutTable)
{
    HideWeaponVisuals(WeaponId);

    sol::object VisualsObject = LayoutTable["Visuals"];
    if (!VisualsObject.valid() || VisualsObject == sol::nil || !VisualsObject.is<sol::table>())
    {
        UE_LOG(Tank, Warning, "EquipWeaponVisualFromLua failed: Visuals missing. Weapon=%s Level=%d",
               WeaponId.c_str(), Level);
        return;
    }

    sol::table Visuals = VisualsObject.as<sol::table>();
    for (const auto& Pair : Visuals)
    {
        sol::object VisualObject = Pair.second;
        if (!VisualObject.valid() || VisualObject == sol::nil || !VisualObject.is<sol::table>())
        {
            continue;
        }

        sol::table VisualDef = VisualObject.as<sol::table>();
        const FString Name = ReadLuaStringOrDefault(VisualDef["Name"]);
        const FString Mesh = ReadLuaStringOrDefault(VisualDef["Mesh"]);
        const FString Parent = ReadLuaStringOrDefault(VisualDef["Parent"], "RootComponent");

        if (Name.empty() || Mesh.empty())
        {
            UE_LOG(Tank, Warning, "Invalid visual def. Weapon=%s", WeaponId.c_str());
            continue;
        }

        UStaticMeshComponent* Visual = GetOrCreateWeaponVisualComponent(Name, Mesh, Parent);
        if (!Visual)
        {
            continue;
        }

        const FVector Location = ReadLuaVec3OrDefault(VisualDef["Location"], FVector::ZeroVector);
        const FRotator Rotation = ReadLuaRotatorOrDefault(VisualDef["Rotation"], FRotator::ZeroRotator);
        const FVector Scale = ReadLuaVec3OrDefault(VisualDef["Scale"], FVector(1.0f, 1.0f, 1.0f));

        Visual->SetRelativeLocation(Location);
        Visual->SetRelativeRotation(Rotation);
        Visual->SetRelativeScale(Scale);
        Visual->SetVisibility(true);

        const FString MuzzleName = ReadLuaStringOrDefault(VisualDef["MuzzleName"]);
        if (!MuzzleName.empty())
        {
            USceneComponent* Muzzle = GetOrCreateMuzzleComponent(MuzzleName, Visual);
            if (Muzzle)
            {
                const FVector MuzzleLocation = ReadLuaVec3OrDefault(VisualDef["MuzzleLocation"], FVector::ZeroVector);
                const FRotator MuzzleRotation = ReadLuaRotatorOrDefault(VisualDef["MuzzleRotation"], FRotator::ZeroRotator);
                Muzzle->SetRelativeLocation(MuzzleLocation);
                Muzzle->SetRelativeRotation(MuzzleRotation);
                Muzzle->SetActive(true);
            }
        }
    }

    UE_LOG(Tank, Info, "EquipWeaponVisualFromLua success. Weapon=%s Level=%d", WeaponId.c_str(), Level);
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
    const FVector SpawnLocation = Muzzle->GetWorldLocation();

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

    Missile->SetActorLocation(Muzzle->GetWorldLocation());
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
        UE_LOG(Tank, Warning, "[%s] ApplyTargetDamage skipped: target is null. Damage is handled by Lua.",
               WeaponId.c_str());
        return;
    }

    UE_LOG(Tank, Info, "[%s] ApplyTargetDamage effect hook only. Damage=%.2f Target=%s",
           WeaponId.c_str(), Params.Damage, TargetActor->GetFName().ToString().c_str());
}

void ATankActor::NotifyInstantHit(const FString& WeaponId, const FTankWeaponAttackParams& Params, int32 SlotIndex, AActor* TargetActor)
{
    if (!TargetActor)
    {
        UE_LOG(Tank, Warning, "[%s] NotifyInstantHit skipped: target is null. Slot=%d",
               WeaponId.c_str(), SlotIndex);
        return;
    }

    USceneComponent* Origin = GetWeaponMuzzle(WeaponId, SlotIndex);
    if (!Origin)
    {
        Origin = FindSceneComponentByName(MakeWeaponIndexedComponentName("Visual", WeaponId, SlotIndex));
    }

    if (!Origin)
    {
        UE_LOG(Tank, Warning, "[%s] NotifyInstantHit failed: visual/muzzle not found. Slot=%d",
               WeaponId.c_str(), SlotIndex);
        return;
    }

    const FVector OriginLocation = Origin->GetWorldLocation();
    UE_LOG(Tank, Info, "[%s] NotifyInstantHit effect hook only. Slot=%d Damage=%.2f Origin=(%.2f, %.2f, %.2f) Target=%s",
           WeaponId.c_str(), SlotIndex, Params.Damage,
           OriginLocation.X, OriginLocation.Y, OriginLocation.Z,
           TargetActor->GetFName().ToString().c_str());
}

void ATankActor::ApplyAreaDamage(const FString& WeaponId, const FTankWeaponAttackParams& Params, const FVector& Center, float Radius)
{
    UE_LOG(Tank, Warning, "[%s] ApplyAreaDamage is a stub. Damage=%.2f Center=(%.2f, %.2f, %.2f) Radius=%.2f. Implement radius query or use Lua DamageSystem.",
           WeaponId.c_str(), Params.Damage, Center.X, Center.Y, Center.Z, Radius);
}

void ATankActor::SpawnInstallable(const FString& WeaponId, const FTankWeaponAttackParams& Params, const FVector& Position)
{
    UE_LOG(Tank, Warning, "[%s] SpawnInstallable is not implemented yet. Damage=%.2f Position=(%.2f, %.2f, %.2f)",
           WeaponId.c_str(), Params.Damage, Position.X, Position.Y, Position.Z);
}

void ATankActor::SpawnSummon(const FString& WeaponId, const FTankWeaponAttackParams& Params, int32 MuzzleIndex)
{
    USceneComponent* Muzzle = GetWeaponMuzzle(WeaponId, MuzzleIndex);
    if (!Muzzle)
    {
        UE_LOG(Tank, Warning, "[%s] SpawnSummon failed: muzzle not found. Muzzle=%d",
               WeaponId.c_str(), MuzzleIndex);
        return;
    }

    const FVector Position = Muzzle->GetWorldLocation();
    UE_LOG(Tank, Warning, "[%s] SpawnSummon is not implemented yet. Damage=%.2f Muzzle=%d Position=(%.2f, %.2f, %.2f)",
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

UStaticMesh* ATankActor::GetHeadGunProjectile()
{
    if (!HeadGunProjectile)
    {
        HeadGunProjectile = GetBasicMesh("Models/Bullet/DefaultBullet.OBJ");
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
    const int32 SafeIndex = MaxInt32(0, MuzzleIndex);
    USceneComponent* Muzzle = FindSceneComponentByName(MakeWeaponIndexedComponentName("Muzzle", WeaponId, SafeIndex));
    if (Muzzle)
    {
        return Muzzle;
    }

    UE_LOG(Tank, Warning, "Muzzle not found. Weapon=%s Index=%d", WeaponId.c_str(), MuzzleIndex);
    return nullptr;
}

void ATankActor::HideWeaponVisuals(const FString& WeaponId)
{
    const FString VisualPrefix = "Visual_" + WeaponId + "_";
    const FString MuzzlePrefix = "Muzzle_" + WeaponId + "_";

    for (UActorComponent* Component : OwnedComponents)
    {
        if (!Component)
        {
            continue;
        }

        const FString ComponentName = Component->GetFName().ToString();
        if (!StartsWith(ComponentName, VisualPrefix) && !StartsWith(ComponentName, MuzzlePrefix))
        {
            continue;
        }

        Component->SetActive(false);
        if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
        {
            Primitive->SetVisibility(false);
        }
    }
}

USceneComponent* ATankActor::FindSceneComponentByName(const FString& ComponentName) const
{
    for (UActorComponent* Component : OwnedComponents)
    {
        USceneComponent* SceneComponent = Cast<USceneComponent>(Component);
        if (SceneComponent && SceneComponent->GetFName().ToString() == ComponentName)
        {
            return SceneComponent;
        }
    }
    return nullptr;
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

UStaticMeshComponent* ATankActor::GetOrCreateWeaponVisualComponent(const FString& Name, const FString& MeshPath, const FString& ParentName)
{
    if (UStaticMeshComponent* Existing = FindStaticMeshComponentByName(Name))
    {
        Existing->SetStaticMesh(GetBasicMesh(MeshPath));
        Existing->SetActive(true);
        Existing->SetVisibility(true);
        if (USceneComponent* Parent = FindSceneComponentByName(ParentName))
        {
            Existing->AttachToComponent(Parent);
        }
        return Existing;
    }

    USceneComponent* Parent = FindSceneComponentByName(ParentName);
    if (!Parent)
    {
        UE_LOG(Tank, Warning, "Weapon visual parent not found: %s", ParentName.c_str());
        return nullptr;
    }

    UStaticMeshComponent* Visual = AddComponent<UStaticMeshComponent>();
    if (!Visual)
    {
        return nullptr;
    }

    Visual->SetFName(Name);
    Parent->AddChild(Visual);
    Visual->SetStaticMesh(GetBasicMesh(MeshPath));
    Visual->SetActive(true);
    Visual->SetVisibility(true);
    return Visual;
}

USceneComponent* ATankActor::GetOrCreateMuzzleComponent(const FString& Name, USceneComponent* Parent)
{
    if (USceneComponent* Existing = FindSceneComponentByName(Name))
    {
        Existing->SetActive(true);
        if (Parent)
        {
            Existing->AttachToComponent(Parent);
        }
        return Existing;
    }

    if (!Parent)
    {
        return nullptr;
    }

    USceneComponent* Muzzle = AddComponent<USceneComponent>();
    if (!Muzzle)
    {
        return nullptr;
    }

    Muzzle->SetFName(Name);
    Parent->AddChild(Muzzle);
    Muzzle->SetActive(true);
    return Muzzle;
}

FString ATankActor::ReadLuaStringOrDefault(sol::object Object, const FString& DefaultValue) const
{
    if (!Object.valid() || Object == sol::nil)
    {
        return DefaultValue;
    }

    if (Object.is<std::string>())
    {
        return FString(Object.as<std::string>());
    }

    if (Object.is<FString>())
    {
        return Object.as<FString>();
    }

    return DefaultValue;
}

FVector ATankActor::ReadLuaVec3OrDefault(sol::object Object, const FVector& DefaultValue) const
{
    FVector Value = DefaultValue;
    if (ReadLuaVec3(Object, Value))
    {
        return Value;
    }
    return DefaultValue;
}

FRotator ATankActor::ReadLuaRotatorOrDefault(sol::object Object, const FRotator& DefaultValue) const
{
    const FVector DefaultEuler(DefaultValue.Roll, DefaultValue.Pitch, DefaultValue.Yaw);
    const FVector Euler = ReadLuaVec3OrDefault(Object, DefaultEuler);
    return FRotator(Euler.Y, Euler.Z, Euler.X);
}
