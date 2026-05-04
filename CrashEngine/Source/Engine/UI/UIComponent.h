#pragma once

#include "Component/SceneComponent.h"
#include "Object/FName.h"

class FUIProxy;

enum class EUIRenderSpace : int32
{
    ScreenSpace,
    WorldSpace
};

enum class EUIGeometryType : int32
{
    Quad,
    Mesh
};

class UUIComponent : public USceneComponent
{
public:
    DECLARE_CLASS(UUIComponent, USceneComponent)

    ~UUIComponent() override;

    void CreateRenderState() override;
    void DestroyRenderState() override;

    void Serialize(FArchive& Ar) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

    virtual FUIProxy* CreateUIProxy();
    FUIProxy* GetUIProxy() const { return UIProxy; }

    void SetRenderSpace(EUIRenderSpace InSpace);
    EUIRenderSpace GetRenderSpace() const { return RenderSpace; }

    void SetGeometryType(EUIGeometryType InType);
    EUIGeometryType GetGeometryType() const { return GeometryType; }

    void SetScreenPosition(const FVector2& InPosition);
    const FVector2& GetScreenPosition() const { return ScreenPosition; }

    void SetSize(const FVector2& InSize);
    const FVector2& GetSize() const { return Size; }

    void SetPivot(const FVector2& InPivot);
    const FVector2& GetPivot() const { return Pivot; }

    void SetLayer(int32 InLayer);
    int32 GetLayer() const { return Layer; }

    void SetZOrder(int32 InZOrder);
    int32 GetZOrder() const { return ZOrder; }

    void SetVisibility(bool bNewVisible);
    bool IsVisible() const { return bVisible; }

    void SetHitTestVisible(bool bNewHitTestVisible);
    bool IsHitTestVisible() const { return bHitTestVisible; }

    void SetTintColor(const FVector4& InColor);
    const FVector4& GetTintColor() const { return TintColor; }

    void SetTexture(const FName& InTextureName);
    const FName& GetTextureName() const { return TextureName; }

    void MarkUIRenderStateDirty();
    void MarkUILayoutDirty();
    void MarkUIStyleDirty();
    void MarkUIVisibilityDirty();

protected:
    void OnTransformDirty() override;

    FUIProxy* UIProxy = nullptr;

    EUIRenderSpace RenderSpace = EUIRenderSpace::ScreenSpace;
    EUIGeometryType GeometryType = EUIGeometryType::Quad;
    FVector2 ScreenPosition = { 0.0f, 0.0f };
    FVector2 Size = { 100.0f, 100.0f };
    FVector2 Pivot = { 0.5f, 0.5f };
    int32 Layer = 0;
    int32 ZOrder = 0;
    bool bVisible = true;
    bool bHitTestVisible = true;
    FVector4 TintColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    FName TextureName = FName::None;
};
