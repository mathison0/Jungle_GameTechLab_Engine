#include "Mesh/SkeletalMesh.h"

#include <memory>

#include "Mesh/SkeletonManager.h"
#include "Engine/Profiling/MemoryStats.h"
#include "Platform/Paths.h"

IMPLEMENT_CLASS(USkeletalSubMesh, UObject)
IMPLEMENT_CLASS(USkeletalMesh, UObject)

namespace
{
    void RebuildSectionMaterialIndices(FSkeletalSubMesh* SkeletalSubMeshAsset, const TArray<FStaticMaterial>& StaticMaterials)
    {
        if (!SkeletalSubMeshAsset)
        {
            return;
        }

        for (FSkeletalMeshSection& Section : SkeletalSubMeshAsset->Sections)
        {
            Section.MaterialIndex = -1;
            for (int32 i = 0; i < static_cast<int32>(StaticMaterials.size()); ++i)
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

void USkeletalMesh::Serialize(FArchive& Ar)
{
    UObject::Serialize(Ar);

    // 1. 스켈레톤 참조 정보 직렬화
    FString SkeletonPath;
    if (Ar.IsSaving() && Skeleton)
    {
        FString SourceFBX, Sub;
        FPaths::ParseSubResourcePath(PathFileName, SourceFBX, Sub);
        SkeletonPath = SourceFBX + "_" + SkeletalMeshPrefix::Skeleton + Skeleton->GetFName().ToString();
    }
    
    Ar << SkeletonPath;

    if (Ar.IsLoading() && !SkeletonPath.empty())
    {
        Skeleton = FSkeletonManager::Get().Load(SkeletonPath);
    }

    // 2. 하위 메시(SubMesh) 목록 직렬화
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
            
            // 로드 시점에 부모의 스켈레톤 참조를 자식(SubMesh)에게 주입
            Sub->SetSkeleton(Skeleton);

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

    Ar << StaticMaterials;

    if (Ar.IsLoading() && SkeletalSubMeshAsset)
    {
        RebuildSectionMaterialIndices(SkeletalSubMeshAsset, StaticMaterials);
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
        RebuildSectionMaterialIndices(SkeletalSubMeshAsset, StaticMaterials);
    }
}

void USkeletalSubMesh::SetStaticMaterials(TArray<FStaticMaterial>&& InMaterials)
{
    StaticMaterials = std::move(InMaterials);
    RebuildSectionMaterialIndices(SkeletalSubMeshAsset, StaticMaterials);
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

    // TODO: Buffer 생성하는 코드 여기에 작성
     SkeletalSubMeshAsset->RenderBuffer = std::make_unique<FSkeletalMeshBuffer>();
     SkeletalSubMeshAsset->RenderBuffer->Create(InDevice, RenderMeshData);
     //NOTE: AI가 작성한 초안이 있었지만 FMeshBuffer 리팩토링과 관련한 Merge Conflict 때문에 
     //      주석 처리했습니다. 재구현 필요
}
