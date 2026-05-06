#pragma once
#include "ActorComponent.h"

class UPostProcessComponent : public UActorComponent
{
public:
    DECLARE_CLASS(UPostProcessComponent, UActorComponent)

    UPostProcessComponent() = default;
    ~UPostProcessComponent() override = default;

    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void Serialize(FArchive& Ar) override;
    void PostDuplicate(UObject* Original) override;

public:
    bool IsEnableVignette() const { return bVignette; }
    float GetVignetteIntensity() const { return VignetteIntensity; }
	float GetVignetteRadius() const { return VignetteRadius; }
	float GetVignetteSoftness() const { return VignetteSoftness; }

    bool IsEnableGammaCorrection() const { return bGammaCorrection; }
	float GetGamma() const { return Gamma; }

private:
    bool bVignette = false;
    float VignetteIntensity = 0.5f;
	float VignetteRadius = 0.75f;
	float VignetteSoftness = 0.25f;

	bool bGammaCorrection = false;
	float Gamma = 2.2f;
};
