#include "DirectionalLightActor.h"

DEFINE_CLASS(ADirectionalLightActor, AActor)
REGISTER_FACTORY(ADirectionalLightActor)

void ADirectionalLightActor::InitDefaultComponents()
{
    USceneComponent* Root = AddComponent<USceneComponent>();
    SetRootComponent(Root);

    LightComponent = AddComponent<UDirectionalLightComponent>();
    LightComponent->AttachToComponent(Root);
}