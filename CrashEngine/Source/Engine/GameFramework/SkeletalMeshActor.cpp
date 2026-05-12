#include "SkeletalMeshActor.h"
#include "Component/SkeletalMeshComponent.h"
#include "Mesh/SkeletalMesh.h"
#include "Object/ObjectFactory.h"
#include "Engine/Runtime/Engine.h"

IMPLEMENT_CLASS(ASkeletalMeshActor, AActor)

void ASkeletalMeshActor::InitDefaultComponents()
{
    SkeletalMeshComponent = AddComponent<USkeletalMeshComponent>();
    SetRootComponent(SkeletalMeshComponent);
}

void ASkeletalMeshActor::InitDefaultComponents(const FString& UStaticMeshFileName)
{
    SkeletalMeshComponent = AddComponent<USkeletalMeshComponent>();
    SetRootComponent(SkeletalMeshComponent);

    ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
    USkeletalMesh* Asset = FObjManager::Get().Load(UStaticMeshFileName);

    SkeletalMeshComponent->SetSkeletalMesh(Asset);
}