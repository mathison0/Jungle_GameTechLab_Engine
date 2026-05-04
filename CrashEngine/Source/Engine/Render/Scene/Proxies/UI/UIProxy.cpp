#include "Render/Scene/Proxies/UI/UIProxy.h"

#include "Core/ResourceTypes.h"
#include "Resource/ResourceManager.h"

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
    ScreenPosition = Owner->GetScreenPosition();
    Size = Owner->GetSize();
    Pivot = Owner->GetPivot();
    WorldMatrix = Owner->GetWorldMatrix();
    Layer = Owner->GetLayer();
    ZOrder = Owner->GetZOrder();
}

void FUIProxy::UpdateStyle()
{
    if (!Owner)
    {
        TintColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
        TextureName = FName::None;
        TextureSRV = nullptr;
        bUseTexture = false;
        return;
    }

    TintColor = Owner->GetTintColor();
    TextureName = Owner->GetTextureName();

    const FTextureResource* Texture = TextureName.IsValid()
        ? FResourceManager::Get().FindTexture(TextureName)
        : nullptr;

    TextureSRV = (Texture && Texture->IsLoaded()) ? Texture->SRV : nullptr;
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
