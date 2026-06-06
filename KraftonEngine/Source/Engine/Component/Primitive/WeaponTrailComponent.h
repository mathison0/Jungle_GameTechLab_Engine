#pragma once

#include "Component/PrimitiveComponent.h"
#include "Object/Ptr/SoftObjectPtr.h"
#include "Math/Vector.h"

#include "Source/Engine/Component/Primitive/WeaponTrailComponent.generated.h"

class UMaterialInterface;

struct FWeaponTrailSample
{
	FVector Start;
	FVector End;
	float Age = 0.0f;
	float Distance = 0.0f;
};

UCLASS()
class UWeaponTrailComponent : public UPrimitiveComponent
{
public:
	GENERATED_BODY()

	void SetTrailEnabled(bool bEnabled);
	void ClearTrail();

	void SetTrailMaterial(UMaterialInterface* InMaterial);
	void LoadTrailMaterialFromPath();

	void ContributeSelectedVisuals(FScene& Scene) const override;

	void PostLoad() override;
	void PostDuplicate() override;
	void PostEditProperty(const char* PropertyName) override;

	const TArray<FWeaponTrailSample>& GetTrailSamples() const { return Samples; }
	float GetTrailLifetime() const { return TrailLifetime; }
	UMaterialInterface* GetTrailMaterial() const { return TrailMaterial; }
	const FVector4& GetHeadColor() const { return HeadColor; }
	const FVector4& GetTailColor() const { return TailColor; }
	float GetAlphaMultiplier() const { return AlphaMultiplier; }
	float GetAlphaPower() const { return AlphaPower; }
	float GetHeadWidthScale() const { return HeadWidthScale; }
	float GetTailWidthScale() const { return TailWidthScale; }
	float GetWidthFadePower() const { return WidthFadePower; }
	float GetDistanceUVScale() const { return DistanceUVScale; }

protected:
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;
	FPrimitiveSceneProxy* CreateSceneProxy() override;

private:
	void AddSampleIfNeeded(float DeltaTime);
	void UpdateSamples(float DeltaTime);
	bool GetTrailPoints(FVector& OutStart, FVector& OutEnd) const;

private:
	TArray<FWeaponTrailSample> Samples;

	UPROPERTY(Edit, Save, Category = "WeaponTrail")
	bool bTrailEnabled = false;

	UPROPERTY(Edit, Save, Category = "WeaponTrail")
	float TrailLifetime = 0.18f;

	UPROPERTY(Edit, Save, Category = "WeaponTrail")
	float MinSampleDistance = 3.0f;

	UPROPERTY(Edit, Save, Category = "WeaponTrail")
	int32 MaxSamples = 48;

	UPROPERTY(Edit, Save, Category = "WeaponTrail")
	FVector StartLocalOffset = FVector(0.0f, -20.0f, 0.0f);

	UPROPERTY(Edit, Save, Category = "WeaponTrail")
	FVector EndLocalOffset = FVector(0.0f, 80.0f, 0.0f);

	UPROPERTY(Edit, Save, Category = "WeaponTrail", DisplayName = "Material", AssetType = "Material")
	FSoftObjectPtr TrailMaterialPath = "None";

	UMaterialInterface* TrailMaterial = nullptr;

	UPROPERTY(Edit, Save, Category = "WeaponTrail|Render", DisplayName = "Head Color")
	FVector4 HeadColor = FVector4(0.75f, 1.0f, 1.0f, 1.0f);

	UPROPERTY(Edit, Save, Category = "WeaponTrail|Render", DisplayName = "Tail Color")
	FVector4 TailColor = FVector4(0.12f, 0.35f, 1.0f, 0.35f);

	UPROPERTY(Edit, Save, Category = "WeaponTrail|Render", DisplayName = "Alpha Multiplier")
	float AlphaMultiplier = 1.0f;

	UPROPERTY(Edit, Save, Category = "WeaponTrail|Render", DisplayName = "Alpha Power")
	float AlphaPower = 2.0f;

	UPROPERTY(Edit, Save, Category = "WeaponTrail|Render", DisplayName = "Head Width Scale")
	float HeadWidthScale = 1.0f;

	UPROPERTY(Edit, Save, Category = "WeaponTrail|Render", DisplayName = "Tail Width Scale")
	float TailWidthScale = 0.65f;

	UPROPERTY(Edit, Save, Category = "WeaponTrail|Render", DisplayName = "Width Fade Power")
	float WidthFadePower = 0.5f;

	UPROPERTY(Edit, Save, Category = "WeaponTrail|Render", DisplayName = "Distance UV Scale")
	float DistanceUVScale = 0.01f;

	UPROPERTY(Edit, Save, Category = "WeaponTrail|Debug", DisplayName = "Draw Debug Points")
	bool bDrawDebugPoints = true;

	UPROPERTY(Edit, Save, Category = "WeaponTrail|Debug", DisplayName = "Debug Sphere Radius")
	float DebugSphereRadius = 2.0f;
};
