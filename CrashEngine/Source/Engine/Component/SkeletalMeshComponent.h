#pragma once

#include "Component/SkinnedMeshComponent.h"
#include "Render/RHI/D3D11/Buffers/SkeletalMeshBuffer.h"

#include <memory>

class USkeletalMeshComponent : public USkinnedMeshComponent
{
public:
    DECLARE_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)
    USkeletalMeshComponent() = default;

	FPrimitiveProxy* CreateSceneProxy() override;
    FSkeletalMeshBuffer* GetMeshBuffer() const override;
    FMeshDataView GetMeshDataView() const override;
    ~USkeletalMeshComponent() override = default;

    void CreateTemporaryPreviewMesh(ID3D11Device* Device);

	void Serialize(FArchive& Ar) override;
    void PostDuplicate() override;

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;

private:
    TArray<FVertexPNCT_T> TemporaryPreviewVertices;
    TArray<uint32> TemporaryPreviewIndices;
    std::unique_ptr<FSkeletalMeshBuffer> TemporaryPreviewBuffer;
};

/*
= scene proxy 생성
= 에디터 property 노출
= serialize/reload
= tick 시 pose 갱신 호출
= 나중에 animation instance 연결
*/
