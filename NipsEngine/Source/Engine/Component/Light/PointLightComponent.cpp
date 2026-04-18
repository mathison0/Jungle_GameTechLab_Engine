#include "PointLightComponent.h"

DEFINE_CLASS(UPointLightComponent, ULightComponent)

void UPointLightComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    ULightComponent::GetEditableProperties(OutProps);
    OutProps.push_back({"Radius", EPropertyType::Float, &Radius});
}
