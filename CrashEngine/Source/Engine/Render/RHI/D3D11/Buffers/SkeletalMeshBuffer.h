#pragma once

#include "Core/CoreTypes.h"
#include "Render/RHI/D3D11/Buffers/VertexTypes.h"
#include "Render/RHI/D3D11/Buffers/MeshBuffer.h"
#include "Render/RHI/D3D11/Buffers/DynamicVertexBuffer.h"
#include "Render/RHI/D3D11/Buffers/IndexBuffer.h"

// FMeshBuffer는 GPU 버퍼 리소스의 생성과 바인딩을 관리합니다.
class FSkeletalMeshBuffer : public FMeshBuffer
{
public:
    FSkeletalMeshBuffer() = default;
    ~FSkeletalMeshBuffer() { Release(); }
    FSkeletalMeshBuffer(const FSkeletalMeshBuffer&)    = delete;
    FSkeletalMeshBuffer& operator=(const FSkeletalMeshBuffer&) = delete;
    FSkeletalMeshBuffer(FSkeletalMeshBuffer&&)                 = default;
    FSkeletalMeshBuffer& operator=(FSkeletalMeshBuffer&&)      = default;

    template <typename VertexType>
    void Create(ID3D11Device* InDevice, const TMeshData<VertexType>& InMeshData)
    {
        Release();
        if (InMeshData.Vertices.empty())
        {
            return;
        }

        uint32 VertexCount     = static_cast<uint32>(InMeshData.Vertices.size());
        uint32 VertexByteWidth = VertexCount * sizeof(VertexType);
        VertexBuffer.Create(InDevice, InMeshData.Vertices.data(), VertexCount, VertexByteWidth, sizeof(VertexType));

        if (!InMeshData.Indices.empty())
        {
            uint32 IndexCount     = static_cast<uint32>(InMeshData.Indices.size());
            uint32 IndexByteWidth = IndexCount * sizeof(uint32);
            IndexBuffer.Create(InDevice, InMeshData.Indices.data(), IndexCount, IndexByteWidth);
        }
    }

    bool                  UpdateVertex(ID3D11DeviceContext* Context, const void* Data, uint32 Count);
    void                  Release() override;
    FDynamicVertexBuffer& GetVertexBuffer() { return VertexBuffer; }
    FIndexBuffer&         GetIndexBuffer() { return IndexBuffer; }
    const FDynamicVertexBuffer& GetVertexBuffer() const { return VertexBuffer; }
    const FIndexBuffer&         GetIndexBuffer() const { return IndexBuffer; }
    bool                  IsValid() const override { return VertexBuffer.GetBuffer() != nullptr && VertexBuffer.GetVertexCount() > 0; }
    ID3D11Buffer*         GetVertexBufferRaw() const override { return VertexBuffer.GetBuffer(); }
    uint32                GetVertexStride() const override { return VertexBuffer.GetStride(); }
    uint32                GetVertexCount() const override { return VertexBuffer.GetVertexCount(); }
    ID3D11Buffer*         GetIndexBufferRaw() const override { return IndexBuffer.GetBuffer(); }
    uint32                GetIndexCount() const override { return IndexBuffer.GetIndexCount(); }

private:
    FDynamicVertexBuffer VertexBuffer;
    FIndexBuffer  IndexBuffer;
};
