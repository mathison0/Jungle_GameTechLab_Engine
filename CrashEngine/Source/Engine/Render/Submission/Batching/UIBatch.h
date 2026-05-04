#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"
#include "Render/RHI/D3D11/Buffers/Buffers.h"
#include "Render/RHI/D3D11/Buffers/VertexTypes.h"
#include "Render/Submission/Batching/BatchBuffer.h"

class FUIProxy;

struct FUIBatchRange
{
    uint32 FirstIndex = 0;
    uint32 IndexCount = 0;
};

struct FUIParamsCBData
{
    FVector4 Flags = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
};

class FUIBatch
{
public:
    void Create(ID3D11Device* InDevice);
    void Release();
    void Clear();

    FUIBatchRange AddScreenQuad(const FUIProxy& Proxy, float ViewportWidth, float ViewportHeight);
    bool UploadBuffers(ID3D11DeviceContext* Context);
    void UpdateParams(ID3D11DeviceContext* Context);

    ID3D11Buffer* GetVBBuffer() const { return Buffer.GetVBBuffer(); }
    uint32        GetVBStride() const { return Buffer.GetVBStride(); }
    ID3D11Buffer* GetIBBuffer() const { return Buffer.GetIBBuffer(); }
    uint32        GetIndexCount() const { return Buffer.GetIndexCount(); }

    FConstantBuffer* GetSolidParamsCB() { return &SolidParamsCB; }
    FConstantBuffer* GetTextureParamsCB() { return &TextureParamsCB; }

private:
    TBatchBuffer<FUIVertex> Buffer;
    FConstantBuffer SolidParamsCB;
    FConstantBuffer TextureParamsCB;
};
