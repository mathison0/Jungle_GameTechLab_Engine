#include "Render/Scene/Proxies/UI/UIProxy.h"

#include "Engine/Runtime/Engine.h"
#include "Texture/Texture2D.h"

FUIProxy::FUIProxy(UUIComponent* InOwner)
    : Owner(InOwner)
{
    UpdateLayout();
    UpdateStyle();
    UpdateVisibility();
}

void FUIProxy::UpdateLayout()
{
    if (!Owner)
    {
        return;
    }

    RenderSpace = Owner->GetRenderSpace();
    GeometryType = Owner->GetGeometryType();
    AnchorMin = Owner->GetAnchorMin();
    AnchorMax = Owner->GetAnchorMax();
    AnchoredPosition = Owner->GetAnchoredPosition();
    SizeDelta = Owner->GetSizeDelta();
    Pivot = Owner->GetPivot();
    RotationDegrees = Owner->GetRotationDegrees();
    SubUVRect = Owner->GetSubUVRect();
    WorldMatrix = Owner->GetWorldMatrix();
    Layer = Owner->GetLayer();
    ZOrder = Owner->GetZOrder();
}

FVector2 FUIProxy::GetScreenLayoutSize(float ViewportWidth, float ViewportHeight) const
{
    const FVector2 AnchorSize(
        (AnchorMax.X - AnchorMin.X) * ViewportWidth,
        (AnchorMax.Y - AnchorMin.Y) * ViewportHeight);

    return FVector2(
        AnchorSize.X + SizeDelta.X,
        AnchorSize.Y + SizeDelta.Y);
}

FVector2 FUIProxy::GetScreenPivotPosition(float ViewportWidth, float ViewportHeight) const
{
    const FVector2 AnchorMinPixels(
        AnchorMin.X * ViewportWidth,
        AnchorMin.Y * ViewportHeight);
    const FVector2 AnchorSize(
        (AnchorMax.X - AnchorMin.X) * ViewportWidth,
        (AnchorMax.Y - AnchorMin.Y) * ViewportHeight);

    return FVector2(
        AnchorMinPixels.X + AnchorSize.X * Pivot.X + AnchoredPosition.X,
        AnchorMinPixels.Y + AnchorSize.Y * Pivot.Y + AnchoredPosition.Y);
}

void FUIProxy::UpdateStyle()
{
    if (!Owner)
    {
        TintColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
        TexturePath.clear();
        Texture = nullptr;
        TextureSRV = nullptr;
        bUseTexture = false;
        return;
    }

    TintColor = Owner->GetTintColor();
    TexturePath = Owner->GetTexturePath();
    Texture = nullptr;

    if (!TexturePath.empty() && GEngine)
    {
        ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
        Texture = UTexture2D::LoadFromFile(TexturePath, Device);
    }

    TextureSRV = (Texture && Texture->IsLoaded()) ? Texture->GetSRV() : nullptr;
    bUseTexture = TextureSRV != nullptr;
}

void FUIProxy::UpdateVisibility()
{
    if (!Owner)
    {
        bVisible = false;
        bHitTestVisible = false;
        return;
    }

    bVisible = Owner->IsVisible();
    bHitTestVisible = Owner->IsHitTestVisible();
}
