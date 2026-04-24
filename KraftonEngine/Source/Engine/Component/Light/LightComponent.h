#pragma once
#include "Component/Light/LightComponentBase.h"

class ULightComponent : public ULightComponentBase
{
public:
	DECLARE_CLASS(ULightComponent, ULightComponentBase)

	float GetShadowResolutionScale() const { return ShadowResolutionScale; }
	float GetShadowBias() const { return ShadowBias; }
	float GetShadowSlopeBias() const { return ShadowSlopeBias; }
	float GetShadowSharpen() const { return ShadowSharpen; }

	virtual void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	virtual void Serialize(FArchive& Ar) override;

protected:
	float ShadowResolutionScale = 1.0f;
	float ShadowBias = 0.01f;
	float ShadowSlopeBias = 0.01f;
	float ShadowSharpen = 0.0f;
};