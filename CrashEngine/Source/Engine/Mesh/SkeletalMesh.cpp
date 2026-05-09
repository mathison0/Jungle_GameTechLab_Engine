#include "Mesh/SkeletalMesh.h"

#include <memory>

#include "Animation/SkeletonManager.h"
#include "Engine/Profiling/MemoryStats.h"
#include "Platform/Paths.h"

IMPLEMENT_CLASS(USkeletalMesh, UObject)

static const FString EmptyPath;

USkeletalMesh::USkeletalMesh()
{
}

USkeletalMesh::~USkeletalMesh()
{
    if (SkeletalMeshAsset)
    {
        const uint32 CPUSize =
            static_cast<uint32>(SkeletalMeshAsset->Vertices.size() * sizeof(FVertexSkinned)) +
            static_cast<uint32>(SkeletalMeshAsset->Indices.size() * sizeof(uint32));

        MemoryStats::SubStaticMeshCPUMemory(CPUSize); // Using static mesh counter for now or could add skeletal mesh counter
    }
}

void USkeletalMesh::Serialize(FArchive& Ar)
{
    UObject::Serialize(Ar);

    if (Ar.IsLoading() && !SkeletalMeshAsset)
    {
        SkeletalMeshAsset = new FSkeletalMesh();
    }

    if (SkeletalMeshAsset)
    {
        SkeletalMeshAsset->Serialize(Ar);
    }

    // Skeleton Serialization
    FString SkeletonPath;
    if (Ar.IsSaving() && Skeleton)
    {
        // Skeleton의 경로는 보통 "원본.fbx_Skel_SkeletonName" 형식입니다.
        if (SkeletalMeshAsset)
        {
            FString SourceFBX, Dummy;
            FPaths::ParseSubResourcePath(SkeletalMeshAsset->PathFileName, SourceFBX, Dummy);
            SkeletonPath = SourceFBX + "_Skel_" + Skeleton->GetFName().ToString();
        }
    }
    
    Ar << SkeletonPath;
    
    if (Ar.IsLoading() && !SkeletonPath.empty())
    {
        Skeleton = FSkeletonManager::Get().Load(SkeletonPath);
    }

    Ar << StaticMaterials;

    // Cache MaterialIndex
    if (Ar.IsLoading() && SkeletalMeshAsset)
    {
        for (FSkeletalMeshSection& Section : SkeletalMeshAsset->Sections)
        {
            Section.MaterialIndex = -1;
            for (int32 i = 0; i < (int32)StaticMaterials.size(); ++i)
            {
                if (StaticMaterials[i].MaterialSlotName == Section.MaterialSlotName)
                {
                    Section.MaterialIndex = i;
                    break;
                }
            }
        }
    }
}

const FString& USkeletalMesh::GetAssetPathFileName() const
{
    if (SkeletalMeshAsset)
    {
        return SkeletalMeshAsset->PathFileName;
    }
    return EmptyPath;
}

void USkeletalMesh::SetSkeletalMeshAsset(FSkeletalMesh* InMesh)
{
    if (SkeletalMeshAsset && SkeletalMeshAsset != InMesh)
    {
        delete SkeletalMeshAsset;
    }
    SkeletalMeshAsset = InMesh;

    if (SkeletalMeshAsset)
    {
        for (FSkeletalMeshSection& Section : SkeletalMeshAsset->Sections)
        {
            Section.MaterialIndex = -1;
            for (int32 i = 0; i < (int32)StaticMaterials.size(); ++i)
            {
                if (StaticMaterials[i].MaterialSlotName == Section.MaterialSlotName)
                {
                    Section.MaterialIndex = i;
                    break;
                }
            }
        }
    }
}

void USkeletalMesh::InitResources(ID3D11Device* InDevice)
{
    if (!InDevice || !SkeletalMeshAsset)
        return;

    const uint32 CPUSize =
        static_cast<uint32>(SkeletalMeshAsset->Vertices.size() * sizeof(FVertexSkinned)) +
        static_cast<uint32>(SkeletalMeshAsset->Indices.size() * sizeof(uint32));
    MemoryStats::AddStaticMeshCPUMemory(CPUSize);

    TMeshData<FVertexSkinned> RenderMeshData;
    RenderMeshData.Vertices = SkeletalMeshAsset->Vertices;
    RenderMeshData.Indices = SkeletalMeshAsset->Indices;

    SkeletalMeshAsset->RenderBuffer = std::make_unique<FMeshBuffer>();
    SkeletalMeshAsset->RenderBuffer->Create(InDevice, RenderMeshData);
}
