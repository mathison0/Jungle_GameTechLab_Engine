#include "Component/SkeletalMeshComponent.h"
#include "Engine/Runtime/Engine.h"
#include "Render/Scene/Proxies/Primitive/SkeletalMeshSceneProxy.h"

IMPLEMENT_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

FPrimitiveProxy* USkeletalMeshComponent::CreateSceneProxy()
{
    return new FSkeletalMeshSceneProxy(this);
}

void USkeletalMeshComponent::SetSkeletalMesh(USkeletalMesh* InMesh)
{
    USkinnedMeshComponent::SetSkeletalMesh(InMesh);
}

void USkeletalMeshComponent::Serialize(FArchive& Ar)
{
    USkinnedMeshComponent::Serialize(Ar);
    Ar << SkeletalMeshPath;
}

void USkeletalMeshComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    USkinnedMeshComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Skeletal Mesh", EPropertyType::SkeletalMeshRef, &SkeletalMeshPath });
}

void USkeletalMeshComponent::PostEditProperty(const char* PropertyName)
{
    USkinnedMeshComponent::PostEditProperty(PropertyName);

    if (strcmp(PropertyName, "Skeletal Mesh") == 0)
    {
        if (SkeletalMeshPath.empty() || SkeletalMeshPath == "None")
        {
            SetSkeletalMesh(nullptr);
        }
        else
        {
            USkeletalMesh* Loaded = FSkeletalMeshManager::Get().Load(SkeletalMeshPath);
            SetSkeletalMesh(Loaded);
        }
    }
}
