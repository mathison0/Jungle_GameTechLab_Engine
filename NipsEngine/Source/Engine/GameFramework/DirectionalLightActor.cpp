#include "DirectionalLightActor.h"
#include "Component/BillboardComponent.h"

DEFINE_CLASS(ADirectionalLightActor, AActor)
REGISTER_FACTORY(ADirectionalLightActor)

void ADirectionalLightActor::InitDefaultComponents()
{
    USceneComponent* Root = AddComponent<USceneComponent>();
    SetRootComponent(Root);

    LightComponent = AddComponent<UDirectionalLightComponent>();
    LightComponent->AttachToComponent(Root);

	UBillboardComponent* LightBillboard = AddComponent<UBillboardComponent>();
    LightBillboard->AttachToComponent(Root);
    LightBillboard->SetTextureName("Asset\\Texture\\S_LightDirectional.png");
    LightBillboard->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
    LightBillboard->SetOutlineEnabled(false);
}