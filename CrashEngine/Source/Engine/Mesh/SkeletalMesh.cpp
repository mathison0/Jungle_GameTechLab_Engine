#include "Mesh/SkeletalMesh.h"

#include <memory>

#include "Animation/SkeletonManager.h"
#include "Engine/Profiling/MemoryStats.h"
#include "Platform/Paths.h"

IMPLEMENT_CLASS(USkeletalSubMesh, UObject)
IMPLEMENT_CLASS(USkeletalMesh, UObject)

void USkeletalMesh::Serialize(FArchive& Ar)
{
    UObject::Serialize(Ar);

    FString SkeletonPath;
    if (Ar.IsSaving() && Skeleton)
    {
        // Skeleton 자체의 캐시 경로 (예: Hero.fbx_SkeletonData_Humanoid)
        FString SourceFBX, Sub;
        FPaths::ParseSubResourcePath(PathFileName, SourceFBX, Sub);
        SkeletonPath = SourceFBX + "_SkeletonData_" + Skeleton->GetFName().ToString();
    }
    
    Ar << SkeletonPath;

    if (Ar.IsLoading() && !SkeletonPath.empty())
    {
        Skeleton = FSkeletonManager::Get().Load(SkeletonPath);
    }

    int32 SubMeshCount = (int32)SubMeshes.size();
    Ar << SubMeshCount;

    if (Ar.IsSaving())
    {
        for (auto* Sub : SubMeshes)
        {
            Sub->Serialize(Ar);
        }
    }
    else
    {
        SubMeshes.clear();
        for (int32 i = 0; i < SubMeshCount; ++i)
        {
            USkeletalSubMesh* Sub = UObjectManager::Get().CreateObject<USkeletalSubMesh>();
            Sub->Serialize(Ar);
            SubMeshes.push_back(Sub);
        }
    }
}

static const FString EmptyPath;

USkeletalSubMesh::USkeletalSubMesh()
{
}

USkeletalSubMesh::~USkeletalSubMesh()
{
    if (SkeletalSubMeshAsset)
    {
        const uint32 CPUSize =
            static_cast<uint32>(SkeletalSubMeshAsset->Vertices.size() * sizeof(FVertexSkinned)) +
            static_cast<uint32>(SkeletalSubMeshAsset->Indices.size() * sizeof(uint32));

        MemoryStats::SubStaticMeshCPUMemory(CPUSize);
    }
}

void USkeletalSubMesh::Serialize(FArchive& Ar)
{
    UObject::Serialize(Ar);

    if (Ar.IsLoading() && !SkeletalSubMeshAsset)
    {
        SkeletalSubMeshAsset = new FSkeletalSubMesh();
    }

    if (SkeletalSubMeshAsset)
    {
        SkeletalSubMeshAsset->Serialize(Ar);
    }

    // Skeleton Serialization
    FString SkeletonPath;
    if (Ar.IsSaving() && Skeleton)
    {
        if (SkeletalSubMeshAsset)
        {
            FString SourceFBX, Dummy;
            FPaths::ParseSubResourcePath(SkeletalSubMeshAsset->PathFileName, SourceFBX, Dummy);
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
    if (Ar.IsLoading() && SkeletalSubMeshAsset)
    {
        for (FSkeletalMeshSection& Section : SkeletalSubMeshAsset->Sections)
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

const FString& USkeletalSubMesh::GetAssetPathFileName() const
{
    if (SkeletalSubMeshAsset)
    {
        return SkeletalSubMeshAsset->PathFileName;
    }
    return EmptyPath;
}

void USkeletalSubMesh::SetSkeletalSubMeshAsset(FSkeletalSubMesh* InMesh)
{
    if (SkeletalSubMeshAsset && SkeletalSubMeshAsset != InMesh)
    {
        delete SkeletalSubMeshAsset;
    }
    SkeletalSubMeshAsset = InMesh;

    if (SkeletalSubMeshAsset)
    {
        for (FSkeletalMeshSection& Section : SkeletalSubMeshAsset->Sections)
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

void USkeletalSubMesh::InitResources(ID3D11Device* InDevice)
{
    if (!InDevice || !SkeletalSubMeshAsset)
        return;

    const uint32 CPUSize =
        static_cast<uint32>(SkeletalSubMeshAsset->Vertices.size() * sizeof(FVertexSkinned)) +
        static_cast<uint32>(SkeletalSubMeshAsset->Indices.size() * sizeof(uint32));
    MemoryStats::AddStaticMeshCPUMemory(CPUSize);

    TMeshData<FVertexSkinned> RenderMeshData;
    RenderMeshData.Vertices = SkeletalSubMeshAsset->Vertices;
    RenderMeshData.Indices = SkeletalSubMeshAsset->Indices;

    SkeletalSubMeshAsset->RenderBuffer = std::make_unique<FMeshBuffer>();
    SkeletalSubMeshAsset->RenderBuffer->Create(InDevice, RenderMeshData);
}
