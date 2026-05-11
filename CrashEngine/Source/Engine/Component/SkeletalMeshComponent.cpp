#include "Component/SkeletalMeshComponent.h"
#include "Engine/Runtime/Engine.h"
#include "Materials/Material.h"
#include "Materials/MaterialManager.h"
#include "Render/Renderer.h"
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
    Ar << MaterialSlots;
}

void USkeletalMeshComponent::PostDuplicate()
{
    USkinnedMeshComponent::PostDuplicate();

    // Load the referenced mesh asset after duplication.
    if (!SkeletalMeshPath.empty() && SkeletalMeshPath != "None")
    {
        USkeletalMesh* Loaded = FSkeletalMeshManager::Get().Load(SkeletalMeshPath);
        if (Loaded)
        {
            // Preserve serialized material slot data across SetSkeletalMesh.
            TArray<FMaterialSlot> SavedSlots = MaterialSlots;
            SetSkeletalMesh(Loaded);

            // Reload override materials from serialized slot paths.
            for (int32 i = 0; i < (int32)MaterialSlots.size() && i < (int32)SavedSlots.size(); ++i)
            {
                MaterialSlots[i] = SavedSlots[i];
                const FString& MatPath = MaterialSlots[i].Path;
                if (MatPath.empty() || MatPath == "None")
                {
                    OverrideMaterials[i] = nullptr;
                }
                else
                {
                    UMaterial* LoadedMat = FMaterialManager::Get().GetOrCreateStaticMeshMaterial(MatPath);
                    OverrideMaterials[i] = LoadedMat;
                }
            }
        }
    }

    CacheLocalBounds();
    MarkRenderStateDirty();
    MarkWorldBoundsDirty();
}

void USkeletalMeshComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    USkinnedMeshComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Skeletal Mesh", EPropertyType::SkeletalMeshRef, &SkeletalMeshPath });

    for (int32 i = 0; i < (int32)MaterialSlots.size(); ++i)
    {
        FPropertyDescriptor Desc;
        Desc.Name = "Element " + std::to_string(i);
        Desc.Type = EPropertyType::MaterialSlot;
        Desc.ValuePtr = &MaterialSlots[i];
        OutProps.push_back(Desc);
    }
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

    if (strncmp(PropertyName, "Element ", 8) == 0)
    {
        // Parse the numeric suffix after the 'Element ' prefix.
        int32 Index = atoi(&PropertyName[8]);

        // Validate the material slot index before applying the edit.
        if (Index >= 0 && Index < (int32)MaterialSlots.size())
        {
            FString NewMatPath = MaterialSlots[Index].Path;

            if (NewMatPath == "None" || NewMatPath.empty())
            {
                SetMaterial(Index, nullptr);
            }
            else
            {
                UMaterial* LoadedMat = FMaterialManager::Get().GetOrCreateStaticMeshMaterial(NewMatPath);
                if (LoadedMat)
                {
                    SetMaterial(Index, LoadedMat);
                }
            }
        }
    }
}
