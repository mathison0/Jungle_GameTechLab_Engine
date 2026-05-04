#include "Render/Submission/Batching/UIBatch.h"

#include "Render/Scene/Proxies/UI/UIProxy.h"

void FUIBatch::Create(ID3D11Device* InDevice)
{
    Buffer.Create(InDevice, 256, 384);
    SolidParamsCB.Create(InDevice, sizeof(FUIParamsCBData));
    TextureParamsCB.Create(InDevice, sizeof(FUIParamsCBData));
}

void FUIBatch::Release()
{
    Clear();
    SolidParamsCB.Release();
    TextureParamsCB.Release();
    Buffer.Release();
}

void FUIBatch::Clear()
{
    Buffer.Clear();
}

FUIBatchRange FUIBatch::AddScreenQuad(const FUIProxy& Proxy, float ViewportWidth, float ViewportHeight)
{
    FUIBatchRange Range = {};
    if (ViewportWidth <= 0.0f || ViewportHeight <= 0.0f ||
        Proxy.Size.X == 0.0f || Proxy.Size.Y == 0.0f)
    {
        return Range;
    }

    const float Left   = Proxy.ScreenPosition.X - Proxy.Pivot.X * Proxy.Size.X;
    const float Top    = Proxy.ScreenPosition.Y - Proxy.Pivot.Y * Proxy.Size.Y;
    const float Right  = Left + Proxy.Size.X;
    const float Bottom = Top + Proxy.Size.Y;

    auto PixelToClipX = [ViewportWidth](float X) -> float
    {
        return (X / ViewportWidth) * 2.0f - 1.0f;
    };

    auto PixelToClipY = [ViewportHeight](float Y) -> float
    {
        return 1.0f - (Y / ViewportHeight) * 2.0f;
    };

    const uint32 BaseVertex = static_cast<uint32>(Buffer.Vertices.size());
    const uint32 BaseIndex  = static_cast<uint32>(Buffer.Indices.size());

    Buffer.Vertices.push_back({ FVector(PixelToClipX(Left), PixelToClipY(Top), 0.0f), FVector2(0.0f, 0.0f), Proxy.TintColor });
    Buffer.Vertices.push_back({ FVector(PixelToClipX(Right), PixelToClipY(Top), 0.0f), FVector2(1.0f, 0.0f), Proxy.TintColor });
    Buffer.Vertices.push_back({ FVector(PixelToClipX(Left), PixelToClipY(Bottom), 0.0f), FVector2(0.0f, 1.0f), Proxy.TintColor });
    Buffer.Vertices.push_back({ FVector(PixelToClipX(Right), PixelToClipY(Bottom), 0.0f), FVector2(1.0f, 1.0f), Proxy.TintColor });

    Buffer.Indices.push_back(BaseVertex);
    Buffer.Indices.push_back(BaseVertex + 1);
    Buffer.Indices.push_back(BaseVertex + 2);
    Buffer.Indices.push_back(BaseVertex + 1);
    Buffer.Indices.push_back(BaseVertex + 3);
    Buffer.Indices.push_back(BaseVertex + 2);

    Range.FirstIndex = BaseIndex;
    Range.IndexCount = 6;
    return Range;
}

bool FUIBatch::UploadBuffers(ID3D11DeviceContext* Context)
{
    return Buffer.Upload(Context);
}

void FUIBatch::UpdateParams(ID3D11DeviceContext* Context)
{
    if (!Context)
    {
        return;
    }

    FUIParamsCBData SolidParams = {};
    SolidParams.Flags = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
    SolidParamsCB.Update(Context, &SolidParams, sizeof(SolidParams));

    FUIParamsCBData TextureParams = {};
    TextureParams.Flags = FVector4(1.0f, 0.0f, 0.0f, 0.0f);
    TextureParamsCB.Update(Context, &TextureParams, sizeof(TextureParams));
}
