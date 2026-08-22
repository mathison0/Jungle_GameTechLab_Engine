#pragma once

#include "PrimitiveComponent.h"
#include "Renderer/MeshData.h"

#include <memory>

class ENGINE_API UArrowComponent : public UPrimitiveComponent
{
public:
	DECLARE_RTTI(UArrowComponent, UPrimitiveComponent)

	void PostConstruct() override;
	void OnRegister() override;
	void DuplicateSubObjects() override;
	void Serialize(FArchive& Ar) override;

	FRenderMesh* GetRenderMesh() const override;
	FBoxSphereBounds GetLocalBounds() const override;

	void SetArrowColor(const FVector4& InArrowColor);

private:
	void RebuildMesh();

	std::shared_ptr<FDynamicMesh> ArrowMesh;
	FVector4 ArrowColor = FVector4(0.45f, 1.0f, 0.35f, 1.0f);
	float ArrowLength = 1.0f;
	float ArrowHeadLength = 0.28f;
	float ArrowHeadSize = 0.14f;
};
