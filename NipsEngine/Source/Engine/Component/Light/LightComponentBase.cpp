#include "LightComponentBase.h"

DEFINE_CLASS(ULightComponentBase, USceneComponent)

const FColor& ULightComponentBase::GetColor() const 
{ 
	return Color; 
}

void ULightComponentBase::SetColor(const FColor& NewColor) 
{ 
	Color = NewColor; 
}

// Intensity
float ULightComponentBase::GetIntensity() const 
{ 
	return Intensity; 
}

void ULightComponentBase::SetIntensity(float NewIntensity) 
{
	Intensity = std::max(0.0f, NewIntensity); 
}
