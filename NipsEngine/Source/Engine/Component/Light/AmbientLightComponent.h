#pragma once

#include "LightComponent.h"

class UAmbientLightComponent : public ULightComponent
{
  public:
    DECLARE_CLASS(UAmbientLightComponent, ULightComponentBase)
    UAmbientLightComponent() = default;
    ~UAmbientLightComponent() = default;

	virtual UAmbientLightComponent* Duplicate() override;
    virtual UAmbientLightComponent* DuplicateSubObjects() override { return this; }

	ELightType GetLightType() const override;
};