#pragma once

#include "GameFramework/AActor.h"
#include "Component/Light/DirectionalLightComponent.h"

class ADirectionalLightActor : public AActor
{
  public:
    DECLARE_CLASS(ADirectionalLightActor, AActor)

    ADirectionalLightActor() = default;
    ~ADirectionalLightActor() override = default;

    virtual void InitDefaultComponents() override;

    UDirectionalLightComponent* GetLightComponent() const { return LightComponent; }

  private:
    UDirectionalLightComponent* LightComponent = nullptr;
};