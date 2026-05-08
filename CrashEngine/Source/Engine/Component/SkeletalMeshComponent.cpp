#include "Component/SkeletalMeshComponent.h"
#include "Render/Scene/Proxies/Primitive/SkeletalMeshSceneProxy.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

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

void USkeletalMeshComponent::Serialize(FArchive& Ar) 
{
	UMeshComponent::Serialize(Ar);
	Ar << SkeletalMeshPath;
	//Ar << MaterialSlots;
};
void USkeletalMeshComponent::PostDuplicate() 
{
 //   UMeshComponent::PostDuplicate();
	//if (!SkeletalMeshPath.empty() && SkeletalMeshPath != "None")
	//{
	//	ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
 //       USkeletalMesh* Loaded; // = FObjManager::LoadObjSkeletalMesh(SkeletalMeshPath, Device);
	//	if (Loaded)
	//	{
	//		SetSkeletalMesh(Loaded);
 //       }
	//}
};

void USkeletalMeshComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) 
{
    /*UMeshComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Skeletal Mesh", EPropertyType::SkeletalMeshRef, &SkeletalMeshPath });

	*/
};
void USkeletalMeshComponent::PostEditProperty(const char* PropertyName) 
{
    //UMeshComponent::PostEditProperty(PropertyName);

    //if (strcmp(PropertyName, "Skeletal Mesh") == 0)
    //{
    //    if (SkeletalMeshPath.empty() || SkeletalMeshPath == "None")
    //    {
    //        SkeletalMesh = nullptr;
    //    }
    //    else
    //    {
    //        ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
    //        USkeletalMesh* Loaded;// = FObjManager::LoadObjStaticMesh(SkeletalMeshPath, Device);
    //        SetSkeletalMesh(Loaded);
    //    }
    //    CacheLocalBounds();
    //    MarkWorldBoundsDirty();
    //}

    //if (strncmp(PropertyName, "Element ", 8) == 0)
    //{
    //    // Parse the numeric suffix after the 'Element ' prefix.
    //    int32 Index = atoi(&PropertyName[8]);

    //    // Validate the material slot index before applying the edit.
    //    if (Index >= 0 && Index < (int32)MaterialSlots.size())
    //    {
    //        FString NewMatPath = MaterialSlots[Index].Path;

    //        if (NewMatPath == "None" || NewMatPath.empty())
    //        {
    //            SetMaterial(Index, nullptr);
    //        }
    //        else
    //        {
    //            UMaterial* LoadedMat = FMaterialManager::Get().GetOrCreateStaticMeshMaterial(NewMatPath);
    //            if (LoadedMat)
    //            {
    //                SetMaterial(Index, LoadedMat);
    //            }
    //        }
    //    }
    //}
};