#pragma once

#include "EnemyBaseActor.h"

class AFlyingWaveEnemyActor : public AEnemyBaseActor
{
public:
    DECLARE_CLASS(AFlyingWaveEnemyActor, AEnemyBaseActor)
    AFlyingWaveEnemyActor() = default;
    void BeginPlay() override;
    void Tick(float DeltaTime) override;
    void InitDefaultComponents() override;

private:
    FString MeshFilePath;
    FVector MoveDirection;
    float MoveSpeed;
    float LifeTime;
    float ElapsedTime;
};
