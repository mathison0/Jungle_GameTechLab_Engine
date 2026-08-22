#pragma once

#include "PrimitiveComponent.h"
#include "Renderer/Material.h"
#include "Types/ObjectPtr.h"

#include <memory>

class FArchive;
class FMaterial;
class UTexture;

struct FDynamicMesh;

class ENGINE_API UBillboardComponent : public UPrimitiveComponent
{
public:
	DECLARE_RTTI(UBillboardComponent, UPrimitiveComponent)
	UBillboardComponent(const UBillboardComponent& Other);

	~UBillboardComponent() override;

	void PostConstruct() override;
	void OnRegister() override;
	void Serialize(FArchive& Ar) override;

	virtual FRenderMesh* GetRenderMesh() const override;
	virtual FBoxSphereBounds GetWorldBounds() const override;

	void SetSprite(UTexture* InSprite);
	UTexture* GetSprite() const { return Sprite.Get(); }

	void SetSize(const FVector2& InSize);
	const FVector2& GetSize() const { return Size; }

	void SetTintColor(const FVector4& InTintColor);
	const FVector4& GetTintColor() const { return TintColor; }

	void SetScreenSizeScaled(bool bInScreenSizeScaled) { bScreenSizeScaled = bInScreenSizeScaled; }
	bool IsScreenSizeScaled() const { return bScreenSizeScaled; }

	void SetScreenSize(float InScreenSize) { ScreenSize = InScreenSize; }
	float GetScreenSize() const { return ScreenSize; }

	FVector GetBillboardRenderScale(const FVector& CameraPosition) const;
	FMaterial* GetBillboardMaterial() const;
	bool EnsureRenderResources();
	virtual void DuplicateSubObjects() override;


private:
	void RebuildMesh();
	bool EnsureMaterial();
	bool LoadSpriteTexture();

private:
	TObjectPtr<UTexture> Sprite = nullptr;
	FVector2 Size = FVector2(1.0f, 1.0f);
	FVector4 TintColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	bool bScreenSizeScaled = false;
	float ScreenSize = 0.0025f;

	std::shared_ptr<FDynamicMesh> BillboardMesh;
	std::unique_ptr<FDynamicMaterial> BillboardMaterial;
	TObjectPtr<UTexture> LoadedSprite = nullptr;
};
