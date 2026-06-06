#pragma once

#include "Component/Primitive/WeaponTrailComponent.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Render/Geometry/SpriteParticleGeometry.h"

class FWeaponTrailSceneProxy : public FPrimitiveSceneProxy
{
public:
	explicit FWeaponTrailSceneProxy(UWeaponTrailComponent* InComponent);
	~FWeaponTrailSceneProxy() override;

public:
	void UpdateMesh() override;

	bool PrepareDrawBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context, FDrawCommandBuffer& OutBuffer) const override;

private:
	TArray<FWeaponTrailSample> Samples;
	float TrailLifetime;
	UMaterialInterface* TrailMaterial = nullptr;
	FVector4 HeadColor;
	FVector4 TailColor;
	float AlphaMultiplier = 1.0f;
	float AlphaPower = 2.0f;
	float HeadWidthScale = 1.0f;
	float TailWidthScale = 0.65f;
	float WidthFadePower = 0.5f;
	float DistanceUVScale = 0.01f;

	mutable FSpriteParticleGeometry Geometry;
	mutable bool bGeometryCreated = false;
};
