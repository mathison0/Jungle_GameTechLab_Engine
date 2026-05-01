#include "SphereComponent.h"
#include "Object/ObjectFactory.h"
#include "Render/Scene/Debug/DebugRenderAPI.h"
#include "Serialization/Archive.h"
#include <algorithm>

IMPLEMENT_CLASS(USphereComponent, UShapeComponent)

void USphereComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UShapeComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "SphereRadius", EPropertyType::Float, &SphereRadius, 0.0f, 10000.0f, 1.0f });
}

void USphereComponent::PostEditProperty(const char* PropertyName)
{
    UShapeComponent::PostEditProperty(PropertyName);
}

void USphereComponent::Serialize(FArchive& Ar)
{
    UShapeComponent::Serialize(Ar);
    Ar << SphereRadius;
}

void USphereComponent::SetRadius(float NewRadius)
{
    SphereRadius = NewRadius;
}

float USphereComponent::GetScaledSphereRadius() const
{
    const FVector Scale = GetAbsWorldScale();
    const float MaxScale = std::max(Scale.X, std::max(Scale.Y, Scale.Z));

	return SphereRadius * MaxScale;
}

void USphereComponent::RenderDebugShape(FScene& Scene) const
{
    RenderDebugSphere(Scene, GetShapeWorldLocation(), GetScaledSphereRadius(), 24, GetDebugShapeColor(), 0.0f);
}
