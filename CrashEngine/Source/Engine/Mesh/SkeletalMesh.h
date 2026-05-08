#pragma once

#include "Object/Object.h"
#include "Mesh/StaticMeshAsset.h"
#include "Render/RHI/D3D11/Buffers/VertexTypes.h"
#include "Animation/Skeleton.h"

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

struct FSkeletalMesh
{
    FString PathFileName;
    TArray<FVertexSkinned> Vertices;
    TArray<uint32> Indices;
    TArray<FSkeletalMeshSection> Sections;

    std::unique_ptr<FMeshBuffer> RenderBuffer;

    void Serialize(FArchive& Ar)
    {
        Ar << PathFileName;
        Ar << Vertices;
        Ar << Indices;
        Ar << Sections;
    }
};

class USkeletalMesh : public UObject
{
public:
    DECLARE_CLASS(USkeletalMesh, UObject)

    USkeletalMesh();
    ~USkeletalMesh() override;

    void Serialize(FArchive& Ar) override;

    const FString& GetAssetPathFileName() const;
    void SetSkeletalMeshAsset(FSkeletalMesh* InMesh);
    FSkeletalMesh* GetSkeletalMeshAsset() const { return SkeletalMeshAsset; }

    void SetSkeleton(USkeleton* InSkeleton) { Skeleton = InSkeleton; }
    USkeleton* GetSkeleton() const { return Skeleton; }

    void SetStaticMaterials(TArray<FStaticMaterial>&& InMaterials) { StaticMaterials = std::move(InMaterials); }
    const TArray<FStaticMaterial>& GetStaticMaterials() const { return StaticMaterials; }

    void InitResources(ID3D11Device* InDevice);

private:
    FSkeletalMesh* SkeletalMeshAsset = nullptr;
    USkeleton* Skeleton = nullptr;
    TArray<FStaticMaterial> StaticMaterials;
};
