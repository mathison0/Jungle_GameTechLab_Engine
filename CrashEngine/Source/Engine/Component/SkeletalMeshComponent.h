#pragma once

#include "Component/SkinnedMeshComponent.h"
#include "Render/RHI/D3D11/Buffers/SkeletalMeshBuffer.h"

class USkeletalMeshComponent : public USkinnedMeshComponent
{
public:
    DECLARE_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)
    USkeletalMeshComponent() = default;

	FPrimitiveProxy* CreateSceneProxy() override;
    FSkeletalMeshBuffer* GetMeshBuffer() const override;
};
