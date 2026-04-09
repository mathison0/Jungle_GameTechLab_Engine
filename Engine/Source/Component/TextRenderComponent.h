#pragma once
#include "PrimitiveComponent.h"

#include "Renderer/MeshData.h"

class ENGINE_API UTextRenderComponent : public UPrimitiveComponent
{
public:
	DECLARE_RTTI(UTextRenderComponent, UPrimitiveComponent)

	void PostConstruct() override;

	virtual FBoxSphereBounds GetWorldBounds() const override;

	void SetText(const FString& InText);
	const FString& GetText() const { return Text; }

	virtual FString GetDisplayText() const { return Text; }

	void SetTextColor(const FVector4& InColor) { TextColor = InColor; }
	const FVector4& GetTextColor() const { return TextColor; }

	void SetAlwaysFaceCamera(bool bInAlwaysFaceCamera) { bAlwaysFaceCamera = bInAlwaysFaceCamera; }
	bool IsAlwaysFaceCamera() const { return bAlwaysFaceCamera; }

	void SetWorldSize(float InWorldSize) { WorldSize = InWorldSize; }
	float GetWorldSize() const { return WorldSize; }

	virtual FVector GetRenderWorldPosition() const { return GetWorldLocation(); }
	virtual FVector GetRenderWorldScale() const { return GetWorldTransform().GetScaleVector() * WorldSize; }

	virtual FRenderMesh* GetRenderMesh() const override;
	FDynamicMesh* GetTextMesh() const { return TextMesh.get(); }

	bool IsTextMeshDirty() const { return bTextMeshDirty; }
	void MarkTextMeshDirty() { bTextMeshDirty = true; if (TextMesh) TextMesh->bIsDirty = true; }
	void ClearTextMeshDirty() { bTextMeshDirty = false; }

	void Serialize(FArchive& Ar) override;
	virtual void DuplicateSubObjects() override;

protected:
	FString Text = "Text";
	FVector4 TextColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	float WorldSize = 1.0f;
	bool bAlwaysFaceCamera = false;

	std::shared_ptr<struct FDynamicMesh> TextMesh;

	bool bTextMeshDirty = true;
};
