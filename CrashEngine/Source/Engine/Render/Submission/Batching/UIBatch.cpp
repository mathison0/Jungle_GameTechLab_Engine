#include "Render/Submission/Batching/UIBatch.h"

#include "Render/Scene/Proxies/UI/UIProxy.h"

#include <cmath>

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
    const FVector2 LayoutSize = Proxy.GetScreenLayoutSize(ViewportWidth, ViewportHeight);
    if (ViewportWidth <= 0.0f || ViewportHeight <= 0.0f ||
        LayoutSize.X == 0.0f || LayoutSize.Y == 0.0f)
    {
        return Range;
    }

    const FVector2 PivotPosition = Proxy.GetScreenPivotPosition(ViewportWidth, ViewportHeight);
    const float Left   = -Proxy.Pivot.X * LayoutSize.X;
    const float Top    = -Proxy.Pivot.Y * LayoutSize.Y;
    const float Right  = Left + LayoutSize.X;
    const float Bottom = Top + LayoutSize.Y;

    const float Radians = Proxy.RotationDegrees * 3.14159265358979323846f / 180.0f;
    const float CosTheta = std::cos(Radians);
    const float SinTheta = std::sin(Radians);

    auto TransformCorner = [&](float X, float Y) -> FVector2
    {
        return FVector2(
            PivotPosition.X + X * CosTheta - Y * SinTheta,
            PivotPosition.Y + X * SinTheta + Y * CosTheta);
    };

    const FVector2 TopLeft = TransformCorner(Left, Top);
    const FVector2 TopRight = TransformCorner(Right, Top);
    const FVector2 BottomLeft = TransformCorner(Left, Bottom);
    const FVector2 BottomRight = TransformCorner(Right, Bottom);

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

    const float U0 = Proxy.SubUVRect.X;
    const float V0 = Proxy.SubUVRect.Y;
    const float U1 = Proxy.SubUVRect.X + Proxy.SubUVRect.Z;
    const float V1 = Proxy.SubUVRect.Y + Proxy.SubUVRect.W;

    Buffer.Vertices.push_back({ FVector(PixelToClipX(TopLeft.X), PixelToClipY(TopLeft.Y), 0.0f), FVector2(U0, V0), Proxy.TintColor });
    Buffer.Vertices.push_back({ FVector(PixelToClipX(TopRight.X), PixelToClipY(TopRight.Y), 0.0f), FVector2(U1, V0), Proxy.TintColor });
    Buffer.Vertices.push_back({ FVector(PixelToClipX(BottomLeft.X), PixelToClipY(BottomLeft.Y), 0.0f), FVector2(U0, V1), Proxy.TintColor });
    Buffer.Vertices.push_back({ FVector(PixelToClipX(BottomRight.X), PixelToClipY(BottomRight.Y), 0.0f), FVector2(U1, V1), Proxy.TintColor });

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
