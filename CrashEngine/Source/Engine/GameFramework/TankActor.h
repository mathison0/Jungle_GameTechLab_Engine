#pragma once
#include "GameFramework/AActor.h"
#include "GameFramework/ProjectileActor/ProjectileActor.h"

class UScriptComponent;

class ATankActor : public AActor
{
public:
    DECLARE_CLASS(ATankActor, AActor)

    virtual void BeginPlay() override;
    void BindScriptFunctions(UScriptComponent& ScriptComponent) override;
    void InitDefaultComponents();


    // 이 부분 풀에서 가져오기로
    AProjectileActor* GetProjectileActor();


    void FireHeadMainGun();

private:
    void CacheHeadMainGunIndex();
    UStaticMesh* GetHeadGunProjectile();

private:
    UStaticMesh* HeadGunProjectile = nullptr;
    int HeadMainGunIndex = -1;
};
