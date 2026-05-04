#include "UIComponent.h"

#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Object/ObjectFactory.h"
#include "Render/Scene/Proxies/UI/UIProxy.h"
#include "Render/Scene/Scene.h"
#include "Serialization/Archive.h"

#include <algorithm>
#include <cmath>
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
    Ar << AnchorMin;
    Ar << AnchorMax;
    Ar << AnchoredPosition;
    Ar << SizeDelta;
    Ar << Pivot;
    Ar << RotationDegrees;
    Ar << Layer;
    Ar << ZOrder;
    Ar << bVisible;
    Ar << bHitTestVisible;
    Ar << TintColor;
    Ar << TexturePath;
    Ar << SubUVRect;
    Ar << SpriteColumns;
    Ar << SpriteRows;
    Ar << SpriteFrameIndex;
    Ar << SpriteFPS;
    Ar << bSpriteAnimation;
    Ar << bSpriteLoop;
    Ar << bSpritePlaying;
    Ar << bSpriteFinished;
    Ar << SpriteTimeAccumulator;
}

void UUIComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    USceneComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Render Space", EPropertyType::Enum, &RenderSpace, 0.0f, 0.0f, 0.1f, &GUIRenderSpaceMeta });
    OutProps.push_back({ "Geometry Type", EPropertyType::Enum, &GeometryType, 0.0f, 0.0f, 0.1f, &GUIGeometryTypeMeta });
    OutProps.push_back({ "Anchor Min", EPropertyType::Vec2, &AnchorMin, 0.0f, 1.0f, 0.01f });
    OutProps.push_back({ "Anchor Max", EPropertyType::Vec2, &AnchorMax, 0.0f, 1.0f, 0.01f });
    OutProps.push_back({ "Anchored Position", EPropertyType::Vec2, &AnchoredPosition });
    OutProps.push_back({ "Size Delta", EPropertyType::Vec2, &SizeDelta });
    OutProps.push_back({ "Pivot", EPropertyType::Vec2, &Pivot });
    OutProps.push_back({ "Rotation Degrees", EPropertyType::Float, &RotationDegrees, -360.0f, 360.0f, 1.0f });
    OutProps.push_back({ "Layer", EPropertyType::Int, &Layer });
    OutProps.push_back({ "Z Order", EPropertyType::Int, &ZOrder });
    OutProps.push_back({ "Visible", EPropertyType::Bool, &bVisible });
    OutProps.push_back({ "Hit Test Visible", EPropertyType::Bool, &bHitTestVisible });
    OutProps.push_back({ "Tint Color", EPropertyType::Color4, &TintColor });
    OutProps.push_back({ "Texture Path", EPropertyType::String, &TexturePath });
    OutProps.push_back({ "SubUV Rect", EPropertyType::Vec4, &SubUVRect, 0.0f, 1.0f, 0.01f });
    OutProps.push_back({ "Sprite Columns", EPropertyType::Int, &SpriteColumns, 1.0f, 64.0f, 1.0f });
    OutProps.push_back({ "Sprite Rows", EPropertyType::Int, &SpriteRows, 1.0f, 64.0f, 1.0f });
    OutProps.push_back({ "Sprite Frame", EPropertyType::Int, &SpriteFrameIndex, 0.0f, 4096.0f, 1.0f });
    OutProps.push_back({ "Sprite FPS", EPropertyType::Float, &SpriteFPS, 0.0f, 120.0f, 1.0f });
    OutProps.push_back({ "Sprite Animation", EPropertyType::Bool, &bSpriteAnimation });
    OutProps.push_back({ "Sprite Loop", EPropertyType::Bool, &bSpriteLoop });
    OutProps.push_back({ "Sprite Playing", EPropertyType::Bool, &bSpritePlaying });
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
        std::strcmp(PropertyName, "Anchor Min") == 0 ||
        std::strcmp(PropertyName, "Anchor Max") == 0 ||
        std::strcmp(PropertyName, "Anchored Position") == 0 ||
        std::strcmp(PropertyName, "Size Delta") == 0 ||
        std::strcmp(PropertyName, "Pivot") == 0 ||
        std::strcmp(PropertyName, "Rotation Degrees") == 0 ||
        std::strcmp(PropertyName, "Layer") == 0 ||
        std::strcmp(PropertyName, "Z Order") == 0 ||
        std::strcmp(PropertyName, "SubUV Rect") == 0)
    {
        MarkUILayoutDirty();
        return;
    }

    if (std::strcmp(PropertyName, "Sprite Columns") == 0 ||
        std::strcmp(PropertyName, "Sprite Rows") == 0 ||
        std::strcmp(PropertyName, "Sprite Frame") == 0 ||
        std::strcmp(PropertyName, "Sprite Animation") == 0)
    {
        ClampSpriteSheetState();
        ApplySpriteFrameToSubUVRect();
        MarkUILayoutDirty();
        return;
    }

    if (std::strcmp(PropertyName, "Sprite FPS") == 0)
    {
        SpriteFPS = std::max(SpriteFPS, 0.0f);
        return;
    }

    if (std::strcmp(PropertyName, "Sprite Playing") == 0)
    {
        if (bSpritePlaying)
        {
            bSpriteFinished = false;
        }
        return;
    }

    if (std::strcmp(PropertyName, "Tint Color") == 0 ||
        std::strcmp(PropertyName, "Texture Path") == 0)
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

void UUIComponent::SetAnchorMin(const FVector2& InAnchorMin)
{
    if (AnchorMin.X == InAnchorMin.X && AnchorMin.Y == InAnchorMin.Y)
    {
        return;
    }

    AnchorMin = InAnchorMin;
    MarkUILayoutDirty();
}

void UUIComponent::SetAnchorMax(const FVector2& InAnchorMax)
{
    if (AnchorMax.X == InAnchorMax.X && AnchorMax.Y == InAnchorMax.Y)
    {
        return;
    }

    AnchorMax = InAnchorMax;
    MarkUILayoutDirty();
}

void UUIComponent::SetAnchoredPosition(const FVector2& InPosition)
{
    if (AnchoredPosition.X == InPosition.X && AnchoredPosition.Y == InPosition.Y)
    {
        return;
    }

    AnchoredPosition = InPosition;
    MarkUILayoutDirty();
}

void UUIComponent::SetSizeDelta(const FVector2& InSizeDelta)
{
    if (SizeDelta.X == InSizeDelta.X && SizeDelta.Y == InSizeDelta.Y)
    {
        return;
    }

    SizeDelta = InSizeDelta;
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

void UUIComponent::SetRotationDegrees(float InDegrees)
{
    if (RotationDegrees == InDegrees)
    {
        return;
    }

    RotationDegrees = InDegrees;
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

void UUIComponent::SetTexturePath(const FString& InTexturePath)
{
    if (TexturePath == InTexturePath)
    {
        return;
    }

    TexturePath = InTexturePath;
    MarkUIStyleDirty();
}

void UUIComponent::SetSubUVRect(const FVector4& InRect)
{
    if (SubUVRect.X == InRect.X &&
        SubUVRect.Y == InRect.Y &&
        SubUVRect.Z == InRect.Z &&
        SubUVRect.W == InRect.W)
    {
        return;
    }

    SubUVRect = InRect;
    MarkUILayoutDirty();
}

void UUIComponent::SetSubUVFrame(int32 Columns, int32 Rows, int32 Index)
{
    SpriteColumns = Columns;
    SpriteRows = Rows;
    SpriteFrameIndex = Index;
    ClampSpriteSheetState();
    ApplySpriteFrameToSubUVRect();
    MarkUILayoutDirty();
}

void UUIComponent::ResetSubUVRect()
{
    SetSubUVRect(FVector4(0.0f, 0.0f, 1.0f, 1.0f));
}

void UUIComponent::SetSpriteSheet(int32 Columns, int32 Rows)
{
    if (SpriteColumns == Columns && SpriteRows == Rows)
    {
        return;
    }

    SpriteColumns = Columns;
    SpriteRows = Rows;
    ClampSpriteSheetState();
    ApplySpriteFrameToSubUVRect();
    MarkUILayoutDirty();
}

void UUIComponent::SetSpriteFrame(int32 Index)
{
    if (SpriteFrameIndex == Index)
    {
        return;
    }

    SpriteFrameIndex = Index;
    ClampSpriteSheetState();
    ApplySpriteFrameToSubUVRect();
    MarkUILayoutDirty();
}

void UUIComponent::SetSpriteFPS(float InFPS)
{
    SpriteFPS = std::max(InFPS, 0.0f);
}

void UUIComponent::SetSpriteLoop(bool bInLoop)
{
    bSpriteLoop = bInLoop;
}

void UUIComponent::SetSpriteAnimationEnabled(bool bEnabled)
{
    if (bSpriteAnimation == bEnabled)
    {
        return;
    }

    bSpriteAnimation = bEnabled;
    bSpriteFinished = false;
    SpriteTimeAccumulator = 0.0f;
    ClampSpriteSheetState();
    ApplySpriteFrameToSubUVRect();
    MarkUILayoutDirty();
}

void UUIComponent::PlaySpriteAnimation(bool bRestart)
{
    bSpriteAnimation = true;
    bSpritePlaying = true;
    bSpriteFinished = false;
    SpriteTimeAccumulator = 0.0f;

    if (bRestart)
    {
        SpriteFrameIndex = 0;
    }

    ClampSpriteSheetState();
    ApplySpriteFrameToSubUVRect();
    MarkUILayoutDirty();
}

void UUIComponent::PauseSpriteAnimation()
{
    bSpritePlaying = false;
}

void UUIComponent::StopSpriteAnimation()
{
    bSpritePlaying = false;
    bSpriteFinished = false;
    SpriteTimeAccumulator = 0.0f;
    SpriteFrameIndex = 0;
    ApplySpriteFrameToSubUVRect();
    MarkUILayoutDirty();
}

FVector2 UUIComponent::GetScreenLayoutSize(float ViewportWidth, float ViewportHeight) const
{
    const FVector2 AnchorSize(
        (AnchorMax.X - AnchorMin.X) * ViewportWidth,
        (AnchorMax.Y - AnchorMin.Y) * ViewportHeight);

    return FVector2(
        AnchorSize.X + SizeDelta.X,
        AnchorSize.Y + SizeDelta.Y);
}

FVector2 UUIComponent::GetScreenPivotPosition(float ViewportWidth, float ViewportHeight) const
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

bool UUIComponent::HitTestScreenPoint(const FVector2& ScreenPoint, float ViewportWidth, float ViewportHeight) const
{
    const FVector2 LayoutSize = GetScreenLayoutSize(ViewportWidth, ViewportHeight);
    if (!bVisible || !bHitTestVisible ||
        RenderSpace != EUIRenderSpace::ScreenSpace ||
        GeometryType != EUIGeometryType::Quad ||
        LayoutSize.X == 0.0f || LayoutSize.Y == 0.0f)
    {
        return false;
    }

    const FVector2 PivotPosition = GetScreenPivotPosition(ViewportWidth, ViewportHeight);
    const float LocalLeft   = -Pivot.X * LayoutSize.X;
    const float LocalTop    = -Pivot.Y * LayoutSize.Y;
    const float LocalRight  = LocalLeft + LayoutSize.X;
    const float LocalBottom = LocalTop + LayoutSize.Y;

    const float DeltaX = ScreenPoint.X - PivotPosition.X;
    const float DeltaY = ScreenPoint.Y - PivotPosition.Y;
    const float Radians = RotationDegrees * 3.14159265358979323846f / 180.0f;
    const float CosTheta = std::cos(Radians);
    const float SinTheta = std::sin(Radians);

    const float LocalX = DeltaX * CosTheta + DeltaY * SinTheta;
    const float LocalY = -DeltaX * SinTheta + DeltaY * CosTheta;

    return LocalX >= LocalLeft && LocalX < LocalRight &&
           LocalY >= LocalTop && LocalY < LocalBottom;
}

bool UUIComponent::HandleUIPointerEvent(const FViewportPointerEvent& Event)
{
    (void)Event;
    return false;
}

void UUIComponent::OnUIPointerEnter(const FViewportPointerEvent& Event)
{
    (void)Event;
}

void UUIComponent::OnUIPointerLeave(const FViewportPointerEvent& Event)
{
    (void)Event;
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

void UUIComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
    (void)TickType;
    (void)ThisTickFunction;

    if (!bSpriteAnimation || !bSpritePlaying || bSpriteFinished || SpriteFPS <= 0.0f)
    {
        return;
    }

    const int32 FrameCount = GetSpriteFrameCount();
    if (FrameCount <= 1)
    {
        return;
    }

    SpriteTimeAccumulator += DeltaTime;
    const float FrameDuration = 1.0f / SpriteFPS;
    bool bFrameChanged = false;

    while (SpriteTimeAccumulator >= FrameDuration)
    {
        SpriteTimeAccumulator -= FrameDuration;

        if (SpriteFrameIndex < FrameCount - 1)
        {
            ++SpriteFrameIndex;
            bFrameChanged = true;
            continue;
        }

        if (bSpriteLoop)
        {
            SpriteFrameIndex = 0;
            bFrameChanged = true;
        }
        else
        {
            bSpritePlaying = false;
            bSpriteFinished = true;
            SpriteTimeAccumulator = 0.0f;
            break;
        }
    }

    if (bFrameChanged)
    {
        ApplySpriteFrameToSubUVRect();
        MarkUILayoutDirty();
    }
}

int32 UUIComponent::GetSpriteFrameCount() const
{
    return std::max(SpriteColumns, 1) * std::max(SpriteRows, 1);
}

void UUIComponent::ClampSpriteSheetState()
{
    SpriteColumns = std::max(SpriteColumns, 1);
    SpriteRows = std::max(SpriteRows, 1);

    const int32 FrameCount = GetSpriteFrameCount();
    SpriteFrameIndex = std::max(0, std::min(SpriteFrameIndex, FrameCount - 1));
    SpriteFPS = std::max(SpriteFPS, 0.0f);
}

void UUIComponent::ApplySpriteFrameToSubUVRect()
{
    ClampSpriteSheetState();

    const float CellWidth = 1.0f / static_cast<float>(SpriteColumns);
    const float CellHeight = 1.0f / static_cast<float>(SpriteRows);
    const int32 Column = SpriteFrameIndex % SpriteColumns;
    const int32 Row = SpriteFrameIndex / SpriteColumns;

    SubUVRect = FVector4(
        static_cast<float>(Column) * CellWidth,
        static_cast<float>(Row) * CellHeight,
        CellWidth,
        CellHeight);
}
