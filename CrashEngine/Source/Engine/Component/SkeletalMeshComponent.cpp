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
    if (TemporaryPreviewBuffer && TemporaryPreviewBuffer->IsValid())
    {
        return TemporaryPreviewBuffer.get();
    }

	// Un-comment once the SkeletalMesh class is ready

    //if (!SkeletalMesh)
    //    return nullptr;
    //FSkeletalMesh* Asset = SkeletalMesh->GetSkeletalMeshAsset();
    //if (!Asset || !Asset->RenderBuffer)
    //    return nullptr;
    //return Asset->RenderBuffer.get();
	return nullptr;	// Placeholder
}

FMeshDataView USkeletalMeshComponent::GetMeshDataView() const
{
    if (!TemporaryPreviewVertices.empty() && !TemporaryPreviewIndices.empty())
    {
        FMeshDataView View;
        View.VertexData = TemporaryPreviewVertices.data();
        View.VertexCount = static_cast<uint32>(TemporaryPreviewVertices.size());
        View.Stride = sizeof(FVertexPNCT_T);
        View.IndexData = TemporaryPreviewIndices.data();
        View.IndexCount = static_cast<uint32>(TemporaryPreviewIndices.size());
        return View;
    }

    return USkinnedMeshComponent::GetMeshDataView();
}

void USkeletalMeshComponent::CreateTemporaryPreviewMesh(ID3D11Device* Device)
{
    TemporaryPreviewVertices.clear();
    TemporaryPreviewIndices.clear();

    auto AddVertex = [this](const FVector& Position, const FVector& Normal, const FVector4& Color, const FVector2& UV)
    {
        FVertexPNCT_T Vertex = {};
        Vertex.Position = Position;
        Vertex.Normal = Normal;
        Vertex.Color = Color;
        Vertex.UV = UV;
        Vertex.Tangent = FVector4(1.0f, 0.0f, 0.0f, 1.0f);
        TemporaryPreviewVertices.push_back(Vertex);
    };

    auto AddTriangle = [this](uint32 A, uint32 B, uint32 C)
    {
        TemporaryPreviewIndices.push_back(A);
        TemporaryPreviewIndices.push_back(B);
        TemporaryPreviewIndices.push_back(C);
    };

    const FVector4 RootColor(0.85f, 0.85f, 0.95f, 1.0f);
    const FVector4 LimbColor(0.35f, 0.65f, 1.0f, 1.0f);

    AddVertex(FVector(-0.45f, -0.25f, 0.0f), FVector(0.0f, 0.0f, -1.0f), RootColor, FVector2(0.0f, 0.0f));
    AddVertex(FVector( 0.45f, -0.25f, 0.0f), FVector(0.0f, 0.0f, -1.0f), RootColor, FVector2(1.0f, 0.0f));
    AddVertex(FVector( 0.45f,  0.25f, 0.0f), FVector(0.0f, 0.0f, -1.0f), RootColor, FVector2(1.0f, 1.0f));
    AddVertex(FVector(-0.45f,  0.25f, 0.0f), FVector(0.0f, 0.0f, -1.0f), RootColor, FVector2(0.0f, 1.0f));
    AddVertex(FVector( 0.0f,   0.0f, 1.25f), FVector(0.0f, 0.0f,  1.0f), LimbColor, FVector2(0.5f, 0.5f));

    AddTriangle(0, 2, 1);
    AddTriangle(0, 3, 2);
    AddTriangle(0, 1, 4);
    AddTriangle(1, 2, 4);
    AddTriangle(2, 3, 4);
    AddTriangle(3, 0, 4);

    TMeshData<FVertexPNCT_T> MeshData;
    MeshData.Vertices = TemporaryPreviewVertices;
    MeshData.Indices = TemporaryPreviewIndices;

    TemporaryPreviewBuffer = std::make_unique<FSkeletalMeshBuffer>();
    TemporaryPreviewBuffer->Create(Device, MeshData);

    RefPoseBoneLocalMatrices.clear();
    RefPoseBoneGlobalMatrices.clear();
    CurrentBoneLocalMatrices.clear();
    CurrentBoneGlobalMatrices.clear();

    RefPoseBoneLocalMatrices.push_back(FMatrix::Identity);
    RefPoseBoneLocalMatrices.push_back(FMatrix::MakeTranslationMatrix(FVector(0.0f, 0.0f, 0.65f)));
    RefPoseBoneLocalMatrices.push_back(FMatrix::MakeTranslationMatrix(FVector(0.0f, 0.0f, 0.60f)));

    RefPoseBoneGlobalMatrices.push_back(RefPoseBoneLocalMatrices[0]);
    RefPoseBoneGlobalMatrices.push_back(RefPoseBoneLocalMatrices[1] * RefPoseBoneGlobalMatrices[0]);
    RefPoseBoneGlobalMatrices.push_back(RefPoseBoneLocalMatrices[2] * RefPoseBoneGlobalMatrices[1]);

    CurrentBoneLocalMatrices = RefPoseBoneLocalMatrices;
    CurrentBoneGlobalMatrices = RefPoseBoneGlobalMatrices;

    CachedLocalCenter = FVector(0.0f, 0.0f, 0.6f);
    CachedLocalExtent = FVector(0.5f, 0.35f, 0.7f);
    bHasValidBounds = true;

    MarkRenderStateDirty();
    MarkWorldBoundsDirty();
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
