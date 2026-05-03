#pragma once
#include "GameFramework/AActor.h"

class UCircleCollider2DComponent;
class UStaticMeshComponent;
class UScriptComponent;

class AEnemyBaseActor : public AActor
{
public:
    DECLARE_CLASS(AEnemyBaseActor, AActor)
    void BeginPlay() override;
    void Tick(float DeltaTime) override;
    void InitDefaultComponents() override;

private:
    UCircleCollider2DComponent* ColliderComponent;
    UStaticMeshComponent* MeshComponent;
    UScriptComponent* EnemyScriptComponent;
};
