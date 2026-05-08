#include "Component/SkeletalMeshComponent.h"
#include "Render/Scene/Proxies/Primitive/SkeletalMeshSceneProxy.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

FPrimitiveProxy* USkeletalMeshComponent::CreateSceneProxy()
{
    return new FSkeletalMeshSceneProxy(this);
}

FSkeletalMeshBuffer* USkeletalMeshComponent::GetMeshBuffer() const 
{
	// Un-comment once the SkeletalMesh class is ready

    //if (!SkeletalMesh)
    //    return nullptr;
    //FSkeletalMesh* Asset = SkeletalMesh->GetSkeletalMeshAsset();
    //if (!Asset || !Asset->RenderBuffer)
    //    return nullptr;
    //return Asset->RenderBuffer.get();
	return nullptr;	// Placeholder
}
