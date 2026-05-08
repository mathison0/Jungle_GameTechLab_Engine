#pragma once

#include "Component/MeshComponent.h"

class USkeletalMesh;

class USkinnedMeshComponent : public UMeshComponent
{
public:
    DECLARE_CLASS(USkinnedMeshComponent, UMeshComponent)
    USkinnedMeshComponent() = default;

    void SetSkeletalMesh(USkeletalMesh* InMesh);
    USkeletalMesh* GetSkeletalMesh() const { return SkeletalMesh; }

protected:
    void CacheLocalBounds() override;

    USkeletalMesh* SkeletalMesh = nullptr;
};
