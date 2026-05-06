#include "ExplodeVfxActor.h"

#include "Component/SubUVComponent.h"

IMPLEMENT_CLASS(AExplodeVfxActor, AActor)

void AExplodeVfxActor::InitDefaultComponents()
{
    USubUVComponent* subUV = AddComponent<USubUVComponent>();
    SetRootComponent(subUV);
    subUV->SetParticle("Explode");
    subUV->SetFrameRate(240.0f);
    subUV->SetLoop(false);
}