#pragma once
#include "GameFramework/AActor.h"
#include "Platform/Paths.h"
#include "Engine/Component/Collision/CircleCollider2DComponent.h"
#include "Engine/Mesh/StaticMesh.h"

struct ProjectileInfo
{
    UStaticMesh* MeshAsset;
    float ColliderSize;
    FVector Position;
    FRotator Rotation;
    FVector Dir;
    float Speed;
    float Size;
    ProjectileInfo(UStaticMesh* InMeshAsset,
                   float InColliderSize,
                   FVector InPosition,
                   FRotator InRotation,
                   FVector InDir,
                   float InSpeed)
    {
        MeshAsset = InMeshAsset;
        ColliderSize = InColliderSize;
        Position = InPosition;
        Rotation = InRotation;
        Dir = InDir;
        Speed = InSpeed;
    }
};

class AProjectileActor : public AActor
{
public:
    DECLARE_CLASS(AProjectileActor, AActor)

    void InitDefaultComponents();
    void SetDirection(const FVector& InDirection) { Direction = InDirection.Normalized(); }
    void SetSpeed(float InSpeed) { Speed = InSpeed; }
    virtual FString GetMeshPath() { return FPaths::ContentRelativePath("Models/_Basic/Sphere.OBJ"); }
    void SetColiderSize(float Size)
    {
        if (ColliderComponentIndex < 0 || ColliderComponentIndex >= static_cast<int>(OwnedComponents.size()))
        {
            return;
        }

        UCircleCollider2DComponent* Collider = GetComponent<UCircleCollider2DComponent>(ColliderComponentIndex);
        if (Collider)
        {
            Collider->SetRadius(Size);
        }
    }

    void Fire();

    void SetProjectileSetting(const ProjectileInfo& InProjectileInfo);

private:
    FVector Direction = { 0.f, 0.f, 0.f };
    float Speed = 0;

    int ColliderComponentIndex = -1;
    int MovementComponentIndex = -1;
    int StaticMeshComponentIndex = -1;
};
