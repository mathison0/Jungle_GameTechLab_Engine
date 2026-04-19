#pragma once

#include "Component/Light/LightComponent.h"

class UPointLightComponent : public ULightComponent
{
  public:
    DECLARE_CLASS(UPointLightComponent, ULightComponent)
    UPointLightComponent() = default;
    ~UPointLightComponent() = default;

    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

    // Point Light Function?
    float GetRadius() { return Radius; }
    float SetRadius(float NewRadius) { Radius = NewRadius; }

    ELightType GetLightType() const override { return ELightType::Point; };

  private:
    float Radius = 10.f;
};
