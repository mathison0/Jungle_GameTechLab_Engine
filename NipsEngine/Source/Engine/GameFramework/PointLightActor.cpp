#include "PointLightActor.h"
#include "Component/BillboardComponent.h"

DEFINE_CLASS(APointLightActor, AActor)
REGISTER_FACTORY(APointLightActor)

void APointLightActor::InitDefaultComponents()
{
    USceneComponent* Root = AddComponent<USceneComponent>();
    SetRootComponent(Root);

    LightComponent = AddComponent<UPointLightComponent>();
    LightComponent->AttachToComponent(Root);

    UBillboardComponent* LightBillboard = AddComponent<UBillboardComponent>();
    LightBillboard->AttachToComponent(Root);
    LightBillboard->SetTextureName("Asset\\Texture\\S_LightPoint.png");
    LightBillboard->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
    LightBillboard->SetOutlineEnabled(false);
}
