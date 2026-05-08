#include "SkeletalMeshSceneProxy.h"
#include "Component/SkeletalMeshComponent.h"

FSkeletalMeshSceneProxy::FSkeletalMeshSceneProxy(USkeletalMeshComponent* InComponent) : FMeshSceneProxy(InComponent)
{
	UpdateShadow();
}

void FSkeletalMeshSceneProxy::UpdateShadow() 
{
    UMeshComponent* Mesh = GetMeshComponent();
    bCastShadow          = Mesh ? Mesh->ShouldCastShadow() : true;
}

UMeshComponent* FSkeletalMeshSceneProxy::GetMeshComponent() const
{
    return static_cast<USkeletalMeshComponent*>(Owner);
}