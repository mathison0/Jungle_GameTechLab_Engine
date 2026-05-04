#include "UIComponent.h"

#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Object/ObjectFactory.h"
#include "Render/Scene/Proxies/UI/UIProxy.h"
#include "Render/Scene/Scene.h"
#include "Serialization/Archive.h"

#include <cstring>

IMPLEMENT_CLASS(UUIComponent, USceneComponent)

namespace
{
constexpr FEnumPropertyOption GUIRenderSpaceOptions[] = {
    { "ScreenSpace", static_cast<int32_t>(EUIRenderSpace::ScreenSpace) },
    { "WorldSpace", static_cast<int32_t>(EUIRenderSpace::WorldSpace) },
};

constexpr FEnumPropertyMeta GUIRenderSpaceMeta = {
    "EUIRenderSpace",
    GUIRenderSpaceOptions,
    static_cast<int32_t>(sizeof(GUIRenderSpaceOptions) / sizeof(GUIRenderSpaceOptions[0])),
};

constexpr FEnumPropertyOption GUIGeometryTypeOptions[] = {
    { "Quad", static_cast<int32_t>(EUIGeometryType::Quad) },
    { "Mesh", static_cast<int32_t>(EUIGeometryType::Mesh) },
};

constexpr FEnumPropertyMeta GUIGeometryTypeMeta = {
    "EUIGeometryType",
    GUIGeometryTypeOptions,
    static_cast<int32_t>(sizeof(GUIGeometryTypeOptions) / sizeof(GUIGeometryTypeOptions[0])),
};
} // namespace

UUIComponent::~UUIComponent()
{
    DestroyRenderState();
}

void UUIComponent::CreateRenderState()
{
    if (UIProxy || !Owner || !Owner->GetWorld())
    {
        return;
    }

    UIProxy = Owner->GetWorld()->GetScene().AddUI(this);
}

void UUIComponent::DestroyRenderState()
{
    if (!UIProxy)
    {
        return;
    }

    if (Owner)
    {
        if (UWorld* World = Owner->GetWorld())
        {
            World->GetScene().RemoveUI(UIProxy);
        }
    }

    UIProxy = nullptr;
}

void UUIComponent::Serialize(FArchive& Ar)
{
    USceneComponent::Serialize(Ar);
    Ar << RenderSpace;
    Ar << GeometryType;
    Ar << ScreenPosition;
    Ar << Size;
    Ar << Pivot;
    Ar << Layer;
    Ar << ZOrder;
    Ar << bVisible;
    Ar << bHitTestVisible;
    Ar << TintColor;
    Ar << TextureName;
}

void UUIComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    USceneComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Render Space", EPropertyType::Enum, &RenderSpace, 0.0f, 0.0f, 0.1f, &GUIRenderSpaceMeta });
    OutProps.push_back({ "Geometry Type", EPropertyType::Enum, &GeometryType, 0.0f, 0.0f, 0.1f, &GUIGeometryTypeMeta });
    OutProps.push_back({ "Screen Position", EPropertyType::Vec2, &ScreenPosition });
    OutProps.push_back({ "Size", EPropertyType::Vec2, &Size });
    OutProps.push_back({ "Pivot", EPropertyType::Vec2, &Pivot });
    OutProps.push_back({ "Layer", EPropertyType::Int, &Layer });
    OutProps.push_back({ "Z Order", EPropertyType::Int, &ZOrder });
    OutProps.push_back({ "Visible", EPropertyType::Bool, &bVisible });
    OutProps.push_back({ "Hit Test Visible", EPropertyType::Bool, &bHitTestVisible });
    OutProps.push_back({ "Tint Color", EPropertyType::Color4, &TintColor });
    OutProps.push_back({ "Texture Name", EPropertyType::Name, &TextureName });
}

void UUIComponent::PostEditProperty(const char* PropertyName)
{
    USceneComponent::PostEditProperty(PropertyName);

    if (std::strcmp(PropertyName, "Visible") == 0 ||
        std::strcmp(PropertyName, "Hit Test Visible") == 0)
    {
        MarkUIVisibilityDirty();
        return;
    }

    if (std::strcmp(PropertyName, "Render Space") == 0 ||
        std::strcmp(PropertyName, "Geometry Type") == 0 ||
        std::strcmp(PropertyName, "Screen Position") == 0 ||
        std::strcmp(PropertyName, "Size") == 0 ||
        std::strcmp(PropertyName, "Pivot") == 0 ||
        std::strcmp(PropertyName, "Layer") == 0 ||
        std::strcmp(PropertyName, "Z Order") == 0)
    {
        MarkUILayoutDirty();
        return;
    }

    if (std::strcmp(PropertyName, "Tint Color") == 0 ||
        std::strcmp(PropertyName, "Texture Name") == 0)
    {
        MarkUIStyleDirty();
    }
}

FUIProxy* UUIComponent::CreateUIProxy()
{
    return new FUIProxy(this);
}

void UUIComponent::SetRenderSpace(EUIRenderSpace InSpace)
{
    if (RenderSpace == InSpace)
    {
        return;
    }

    RenderSpace = InSpace;
    MarkUILayoutDirty();
}

void UUIComponent::SetGeometryType(EUIGeometryType InType)
{
    if (GeometryType == InType)
    {
        return;
    }

    GeometryType = InType;
    MarkUILayoutDirty();
}

void UUIComponent::SetScreenPosition(const FVector2& InPosition)
{
    if (ScreenPosition.X == InPosition.X && ScreenPosition.Y == InPosition.Y)
    {
        return;
    }

    ScreenPosition = InPosition;
    MarkUILayoutDirty();
}

void UUIComponent::SetSize(const FVector2& InSize)
{
    if (Size.X == InSize.X && Size.Y == InSize.Y)
    {
        return;
    }

    Size = InSize;
    MarkUILayoutDirty();
}

void UUIComponent::SetPivot(const FVector2& InPivot)
{
    if (Pivot.X == InPivot.X && Pivot.Y == InPivot.Y)
    {
        return;
    }

    Pivot = InPivot;
    MarkUILayoutDirty();
}

void UUIComponent::SetLayer(int32 InLayer)
{
    if (Layer == InLayer)
    {
        return;
    }

    Layer = InLayer;
    MarkUILayoutDirty();
}

void UUIComponent::SetZOrder(int32 InZOrder)
{
    if (ZOrder == InZOrder)
    {
        return;
    }

    ZOrder = InZOrder;
    MarkUILayoutDirty();
}

void UUIComponent::SetVisibility(bool bNewVisible)
{
    if (bVisible == bNewVisible)
    {
        return;
    }

    bVisible = bNewVisible;
    MarkUIVisibilityDirty();
}

void UUIComponent::SetHitTestVisible(bool bNewHitTestVisible)
{
    if (bHitTestVisible == bNewHitTestVisible)
    {
        return;
    }

    bHitTestVisible = bNewHitTestVisible;
    MarkUIVisibilityDirty();
}

void UUIComponent::SetTintColor(const FVector4& InColor)
{
    if (TintColor.X == InColor.X &&
        TintColor.Y == InColor.Y &&
        TintColor.Z == InColor.Z &&
        TintColor.W == InColor.W)
    {
        return;
    }

    TintColor = InColor;
    MarkUIStyleDirty();
}

void UUIComponent::SetTexture(const FName& InTextureName)
{
    if (TextureName == InTextureName)
    {
        return;
    }

    TextureName = InTextureName;
    MarkUIStyleDirty();
}

void UUIComponent::MarkUIRenderStateDirty()
{
    DestroyRenderState();
    CreateRenderState();
}

void UUIComponent::MarkUILayoutDirty()
{
    if (!UIProxy || !Owner || !Owner->GetWorld())
    {
        return;
    }

    Owner->GetWorld()->GetScene().MarkUIProxyDirty(UIProxy, ESceneProxyDirtyFlag::Transform);
}

void UUIComponent::MarkUIStyleDirty()
{
    if (!UIProxy || !Owner || !Owner->GetWorld())
    {
        return;
    }

    Owner->GetWorld()->GetScene().MarkUIProxyDirty(UIProxy, ESceneProxyDirtyFlag::Material);
}

void UUIComponent::MarkUIVisibilityDirty()
{
    if (!UIProxy || !Owner || !Owner->GetWorld())
    {
        return;
    }

    Owner->GetWorld()->GetScene().MarkUIProxyDirty(UIProxy, ESceneProxyDirtyFlag::Visibility);
}

void UUIComponent::OnTransformDirty()
{
    USceneComponent::OnTransformDirty();
    MarkUILayoutDirty();
}
