#pragma once

#include "Component/Light/LightComponent.h"

class USpotLightComponent : public ULightComponent
{
  public:
    DECLARE_CLASS(USpotLightComponent, ULightComponent)
    USpotLightComponent() = default;
    ~USpotLightComponent() = default;

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

	const FVector& GetDirection() const;
    void  SetDirection(FVector NewDirection);

	float GetInnerConeAngle() const;
    void  SetInnerConeAngle(float InAngle);

    float GetOuterConeAngle() const;
    void  SetOuterConeAngle(float InAngle);

    void SetConeAngles(float InInnerAngle, float InOuterAngle);


	//Point Light Function?
	float GetRadius() { return Radius; }
    float SetRadius(float NewRadius) { Radius = NewRadius; }


	ELightType GetLightType() const override { return ELightType::Spot; };

  private:
    FVector Direction = FVector::Zero();
    float InnerConeAngle = 0.0f;
    float OuterConeAngle = 45.0f;
    float MaxConeAngle = 80.0f;


	//Point Light Variable?
    float Radius;

};
