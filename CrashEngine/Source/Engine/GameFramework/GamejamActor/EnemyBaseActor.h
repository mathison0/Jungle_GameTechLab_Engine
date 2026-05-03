#pragma once
#include "GameFramework/AActor.h"

class UCircleCollider2DComponent;
class UStaticMeshComponent;
class UScriptComponent;

class AEnemyBaseActor : public AActor
{
public:
    DECLARE_CLASS(AEnemyBaseActor, AActor)
    AEnemyBaseActor() = default;
    void BeginPlay() override;
    void Tick(float DeltaTime) override;
    void InitDefaultComponents() override;

protected:
    UCircleCollider2DComponent* ColliderComponent = nullptr;
    UStaticMeshComponent* MeshComponent = nullptr;
    UScriptComponent* EnemyScriptComponent = nullptr;
};
