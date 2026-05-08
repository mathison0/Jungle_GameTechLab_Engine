#include "Component/SkinnedMeshComponent.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(USkinnedMeshComponent, UMeshComponent)

// 임시구현
void USkinnedMeshComponent::SetSkeletalMesh(USkeletalMesh* InMesh)
{
    SkeletalMesh = InMesh;
    OverrideMaterials.clear();
    MaterialSlots.clear();
    CacheLocalBounds();
    MarkRenderStateDirty();
    MarkWorldBoundsDirty();
}

void USkinnedMeshComponent::CacheLocalBounds()
{
    bHasValidBounds = false;
}
