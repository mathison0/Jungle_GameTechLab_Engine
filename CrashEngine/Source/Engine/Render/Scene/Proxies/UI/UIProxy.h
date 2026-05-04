#pragma once

#include "Render/Scene/Proxies/SceneProxy.h"
#include "UI/UIComponent.h"

struct ID3D11ShaderResourceView;

class FUIProxy : public FSceneProxy
{
public:
    explicit FUIProxy(UUIComponent* InOwner);

    virtual void UpdateLayout();
    virtual void UpdateStyle();
    virtual void UpdateVisibility();

    UUIComponent* Owner = nullptr;

    EUIRenderSpace RenderSpace = EUIRenderSpace::ScreenSpace;
    EUIGeometryType GeometryType = EUIGeometryType::Quad;

    FVector2 ScreenPosition = { 0.0f, 0.0f };
    FVector2 Size = { 100.0f, 100.0f };
    FVector2 Pivot = { 0.5f, 0.5f };

    FMatrix WorldMatrix;
    int32 Layer = 0;
    int32 ZOrder = 0;

    bool bVisible = true;
    bool bHitTestVisible = true;

    FVector4 TintColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    FName TextureName = FName::None;
    ID3D11ShaderResourceView* TextureSRV = nullptr;
    bool bUseTexture = false;
};
