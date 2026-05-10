#pragma once

#include "Object/Object.h"
#include "Mesh/StaticMeshAsset.h"
#include "Render/RHI/D3D11/Buffers/VertexTypes.h"
#include "Mesh/Skeleton.h"

namespace SkeletalMeshPrefix
{
    inline const FString Mesh = "Mesh_";
    inline const FString Skeleton = "Skeleton_";
}

struct FSkeletalMeshSection
{
    int32 MaterialIndex = -1;
    FString MaterialSlotName;
    uint32 FirstIndex;
    uint32 NumTriangles;

    friend FArchive& operator<<(FArchive& Ar, FSkeletalMeshSection& Section)
    {
        Ar << Section.MaterialSlotName << Section.FirstIndex << Section.NumTriangles;
        return Ar;
    }
};

struct FSkeletalSubMesh
{
    FString PathFileName;
    TArray<FVertexSkinned> Vertices;
    TArray<uint32> Indices;
    TArray<FSkeletalMeshSection> Sections;

    std::unique_ptr<FSkeletalMeshBuffer> RenderBuffer;

    void Serialize(FArchive& Ar)
    {
        Ar << PathFileName;
        Ar << Vertices;
        Ar << Indices;
        Ar << Sections;
    }
};

class USkeletalSubMesh : public UObject
{
public:
    DECLARE_CLASS(USkeletalSubMesh, UObject)

    USkeletalSubMesh();
    ~USkeletalSubMesh() override;

    void Serialize(FArchive& Ar) override;

    const FString& GetAssetPathFileName() const;
    void SetSkeletalSubMeshAsset(FSkeletalSubMesh* InMesh);
    FSkeletalSubMesh* GetSkeletalSubMeshAsset() const { return SkeletalSubMeshAsset; }

    void SetSkeleton(USkeleton* InSkeleton) { Skeleton = InSkeleton; }
    USkeleton* GetSkeleton() const { return Skeleton; }

    void SetStaticMaterials(TArray<FStaticMaterial>&& InMaterials) { StaticMaterials = std::move(InMaterials); }
    const TArray<FStaticMaterial>& GetStaticMaterials() const { return StaticMaterials; }

    void InitResources(ID3D11Device* InDevice);

private:
    FSkeletalSubMesh* SkeletalSubMeshAsset = nullptr;
    
    // 메시에 연결된 스켈레톤 포인터.
    // 직렬화되지 않으며, 로드 시에 부모(USkeletalMesh)에게 주입받는 런타임 전용 필드.
    USkeleton* Skeleton = nullptr;
    
    TArray<FStaticMaterial> StaticMaterials;
};

// 새로운 컨테이너 클래스
class USkeletalMesh : public UObject
{
public:
    DECLARE_CLASS(USkeletalMesh, UObject)

    USkeletalMesh() = default;
    ~USkeletalMesh() override = default;

    void SetSkeleton(USkeleton* InSkeleton) { Skeleton = InSkeleton; }
    USkeleton* GetSkeleton() const { return Skeleton; }

    void AddSubMesh(USkeletalSubMesh* InSubMesh) { SubMeshes.push_back(InSubMesh); }
    const TArray<USkeletalSubMesh*>& GetSubMeshes() const { return SubMeshes; }

    void SetAssetPathFileName(const FString& InPath) { PathFileName = InPath; }
    const FString& GetAssetPathFileName() const { return PathFileName; }

    void Serialize(FArchive& Ar) override;

private:
    USkeleton* Skeleton = nullptr;
    TArray<USkeletalSubMesh*> SubMeshes;
    FString PathFileName;
};
