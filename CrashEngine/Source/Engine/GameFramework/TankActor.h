#pragma once
#include "GameFramework/AActor.h"
#include "GameFramework/ProjectileActor/ProjectileActor.h"
#include "sol.hpp"

class UScriptComponent;
class UStaticMeshComponent;

struct FTankMainGunFireParams
{
    float Damage = 10.0f;
    float ProjectileSpeed = 20.0f;
    float ProjectileScale = 1.0f;
    float ColliderSize = 1.0f;
    int32 Pierce = 0;
};

class ATankActor : public AActor
{
public:
    DECLARE_CLASS(ATankActor, AActor)

    virtual void BeginPlay() override;
    virtual void BindScriptFunctions(UScriptComponent& ScriptComponent) override;

    void InitDefaultComponents();

    void FireHeadMainGun(const FTankMainGunFireParams& Params);

private:
    void CacheComponentIndices();

    UStaticMeshComponent* GetHeadMainGun();
    UStaticMesh* GetHeadGunProjectile();

    AProjectileActor* AcquireProjectile();

    FTankMainGunFireParams ReadMainGunParamsFromLua(sol::object ParamsObject) const;

private:
    UStaticMesh* HeadGunProjectile = nullptr;

    int32 HeadMainGunIndex = -1;

    static constexpr const char* TankScriptPath = "TankScript.lua";
    static constexpr const char* HeadMainGunName = "HeadMainGun";
};
