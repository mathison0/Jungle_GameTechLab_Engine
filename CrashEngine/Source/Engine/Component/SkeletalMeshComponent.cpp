#include "Component/SkeletalMeshComponent.h"
#include "Engine/Runtime/Engine.h"

IMPLEMENT_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

void USkeletalMeshComponent::SetSkeletalMesh(USkeletalMesh* InMesh)
{
    SkeletalMesh = InMesh;
    if (InMesh)
    {
        SkeletalMeshPath = InMesh->GetAssetPathFileName();
    }
    else
    {
        SkeletalMeshPath = "None";
    }
    // TODO: Init skeletal mesh resources, update bounds, etc.
    MarkRenderStateDirty();
    MarkWorldBoundsDirty();
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
            SkeletalMesh = nullptr;
        }
        else
        {
            USkeletalMesh* Loaded = FSkeletalMeshManager::Get().Load(SkeletalMeshPath);
            SetSkeletalMesh(Loaded);
        }
    }
}
