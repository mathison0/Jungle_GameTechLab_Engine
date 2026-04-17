#pragma once

#include "Component/SceneComponent.h"

class ULightComponentBase : public USceneComponent
{
  public:
    DECLARE_CLASS(ULightComponentBase, USceneComponent)

    ULightComponentBase() = default;
    ~ULightComponentBase() = default;

    const FColor& GetColor() const;
    void   SetColor(const FColor& NewColor);

	float GetIntensity() const;
    void  SetIntensity(float NewIntensity);

  private:
    FColor Color;
    float  Intensity = 1.0f;

};
