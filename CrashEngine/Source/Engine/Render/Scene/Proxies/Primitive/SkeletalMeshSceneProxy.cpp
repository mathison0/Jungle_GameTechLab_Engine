#include "SkeletalMeshSceneProxy.h"

void FSkeletalMeshSceneProxy::UpdateShadow() 
{
    // TODO: Fix this after the actual class type rolls in
    // UMeshComponent* Mesh = GetMeshComponent();
    // bCastShadow          = Mesh ? Mesh->ShouldCastShadow() : true;
}

UMeshComponent* FSkeletalMeshSceneProxy::GetMeshComponent() const
{
	// TODO: Fix this after the actual class type rolls in
    // return static_cast<USkeletalMeshComponent*>(Owner);
	return nullptr;
}