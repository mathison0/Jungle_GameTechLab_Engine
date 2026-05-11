#pragma once
#include "MeshSceneProxy.h"
#include "Render/RHI/D3D11/Buffers/SkeletalMeshBuffer.h"
#include "Render/Scene/Debug/SkeletalDebug.h"

class USkeletalMeshComponent;

class FSkeletalMeshSceneProxy : public FMeshSceneProxy
{
public:
    FSkeletalMeshSceneProxy(USkeletalMeshComponent* InComponent);
    void UpdateMesh() override;
    void UpdateShadow() override;
    void UpdateLOD(uint32 LODLevel) override { /* SkeletalMesh does not support LOD for now */ };
    void BuildSkeletalDebugInstance(FSkeletalDebugInstance& OutInstance) const;
    bool UpdateSkinnedSubMeshVertices(uint32 SubMeshIndex, ID3D11DeviceContext* Context, const FVertexPNCT_T* Vertices, uint32 VertexCount);

protected:
    UMeshComponent* GetMeshComponent() const override;
    void            RebuildSectionRenderData() override;

    TArray<std::unique_ptr<FSkeletalMeshBuffer>> SkinnedSubMeshBuffers;
};