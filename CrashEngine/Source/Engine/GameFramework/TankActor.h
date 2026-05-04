#pragma once

#include "GameFramework/AActor.h"
#include "GameFramework/ProjectileActor/ProjectileActor.h"
#include "sol.hpp"

class AHomingMissileActor;
class USceneComponent;
class UScriptComponent;
class UStaticMesh;
class UStaticMeshComponent;

struct FTankWeaponAttackParams
{
    float Damage = 10.0f;

    float ProjectileSpeed = 20.0f;
    float ProjectileScale = 1.0f;
    float ColliderSize = 1.0f;
    int32 Pierce = 0;

    float TurnSpeed = 120.0f;
    float ImpactRadius = 0.0f;

    float Range = 10000.0f;
    float Radius = 0.0f;
    float TickInterval = 1.0f;
    int32 TargetCount = 1;

    float Duration = 0.0f;
};

using FTankMainGunFireParams = FTankWeaponAttackParams;

class ATankActor : public AActor
{
public:
    DECLARE_CLASS(ATankActor, AActor)

    void BeginPlay() override;
    void BindScriptFunctions(UScriptComponent& ScriptComponent) override;
    void InitDefaultComponents() override;

    FTankWeaponAttackParams ReadWeaponAttackParamsFromLua(sol::object ParamsObject) const;

    void EquipWeaponVisual(const FString& WeaponId, int32 Level);

    void FireLinearProjectile(const FString& WeaponId, const FTankWeaponAttackParams& Params, int32 MuzzleIndex);
    void FireHomingMissile(const FString& WeaponId, const FTankWeaponAttackParams& Params, int32 MuzzleIndex, AActor* TargetActor);
    void ApplyTargetDamage(const FString& WeaponId, const FTankWeaponAttackParams& Params, AActor* TargetActor);
    void ApplyAreaDamage(const FString& WeaponId, const FTankWeaponAttackParams& Params, const FVector& Center, float Radius);
    void SpawnInstallable(const FString& WeaponId, const FTankWeaponAttackParams& Params, const FVector& Position);
    void SpawnSummon(const FString& WeaponId, const FTankWeaponAttackParams& Params, int32 MuzzleIndex);

    AProjectileActor* GetProjectileActor();

    void FireHeadMainGun(const FTankWeaponAttackParams& Params);
    void FireHeadMainGun();

private:
    void CacheComponentIndices();

    UStaticMeshComponent* GetHeadMainGun();
    UStaticMesh* GetHeadGunProjectile();
    UStaticMesh* GetBasicMesh(const FString& RelativePath);
    UStaticMesh* GetProjectileMeshForWeapon(const FString& WeaponId);

    AProjectileActor* AcquireProjectile();
    AHomingMissileActor* AcquireHomingMissile();

    USceneComponent* GetWeaponMuzzle(const FString& WeaponId, int32 MuzzleIndex);
    UStaticMeshComponent* FindStaticMeshComponentByName(const FString& ComponentName) const;
    UStaticMeshComponent* EnsureWeaponVisualComponent(
        const FString& ComponentName,
        const FString& MeshPath,
        const FVector& RelativeLocation,
        const FVector& RelativeScale);

private:
    UStaticMesh* HeadGunProjectile = nullptr;

    int32 HeadMainGunIndex = -1;

    static constexpr const char* TankScriptPath = "TankScript.lua";
    static constexpr const char* HeadMainGunName = "HeadMainGun";
};
