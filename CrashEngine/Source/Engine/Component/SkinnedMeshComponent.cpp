#include "Component/SkinnedMeshComponent.h"

#include "Mesh/SkeletalMesh.h"
#include "Mesh/Skeleton.h"
#include "Object/ObjectFactory.h"

#include <algorithm>

IMPLEMENT_CLASS(USkinnedMeshComponent, UMeshComponent)

void USkinnedMeshComponent::SetSkeletalMesh(USkeletalMesh* InMesh)
{
    SkeletalMesh = InMesh;
    OverrideMaterials.clear();
    MaterialSlots.clear();

    if (SkeletalMesh)
    {
        SkeletalMeshPath = SkeletalMesh->GetAssetPathFileName();

        for (USkeletalSubMesh* SubMesh : SkeletalMesh->GetSubMeshes())
        {
            if (!SubMesh)
            {
                continue;
            }

            const TArray<FStaticMaterial>& DefaultMaterials = SubMesh->GetStaticMaterials();
            for (const FStaticMaterial& DefaultMaterial : DefaultMaterials)
            {
                OverrideMaterials.push_back(DefaultMaterial.MaterialInterface);

                FMaterialSlot Slot;
                Slot.Path = DefaultMaterial.MaterialInterface
                    ? DefaultMaterial.MaterialInterface->GetAssetPathFileName()
                    : "None";
                MaterialSlots.push_back(Slot);
            }
        }
    }
    else
    {
        SkeletalMeshPath = "None";
    }

    RefreshReferencePose();
    ResetToReferencePose();

    // CPU skinning 구현이 붙으면 여기서 갱신한다.
    // UpdateSkinningMatrices();
    // UpdateSkinnedVertices();

    CacheLocalBounds();
    MarkRenderStateDirty();
    MarkWorldBoundsDirty();
}

void USkinnedMeshComponent::CacheLocalBounds()
{
    bHasValidBounds = false;
    if (!SkeletalMesh)
    {
        return;
    }

    bool bFoundVertex = false;
    FVector LocalMin;
    FVector LocalMax;

    for (USkeletalSubMesh* SubMesh : SkeletalMesh->GetSubMeshes())
    {
        if (!SubMesh || !SubMesh->GetSkeletalSubMeshAsset())
        {
            continue;
        }

        const TArray<FVertexSkinned>& Vertices = SubMesh->GetSkeletalSubMeshAsset()->Vertices;
        for (const FVertexSkinned& Vertex : Vertices)
        {
            if (!bFoundVertex)
            {
                LocalMin = Vertex.Position;
                LocalMax = Vertex.Position;
                bFoundVertex = true;
                continue;
            }

            LocalMin.X = (std::min)(LocalMin.X, Vertex.Position.X);
            LocalMin.Y = (std::min)(LocalMin.Y, Vertex.Position.Y);
            LocalMin.Z = (std::min)(LocalMin.Z, Vertex.Position.Z);
            LocalMax.X = (std::max)(LocalMax.X, Vertex.Position.X);
            LocalMax.Y = (std::max)(LocalMax.Y, Vertex.Position.Y);
            LocalMax.Z = (std::max)(LocalMax.Z, Vertex.Position.Z);
        }
    }

    if (!bFoundVertex)
    {
        return;
    }

    CachedLocalCenter = (LocalMin + LocalMax) * 0.5f;
    CachedLocalExtent = (LocalMax - LocalMin) * 0.5f;
    bHasValidBounds = true;
}

FMeshDataView USkinnedMeshComponent::GetMeshDataView() const
{
    if (!SkeletalMesh)
    {
        return {};
    }

    for (USkeletalSubMesh* SubMesh : SkeletalMesh->GetSubMeshes())
    {
        if (!SubMesh || !SubMesh->GetSkeletalSubMeshAsset())
        {
            continue;
        }

        const FSkeletalSubMesh* Asset = SubMesh->GetSkeletalSubMeshAsset();
        if (Asset->Vertices.empty() || Asset->Indices.empty())
        {
            continue;
        }

        FMeshDataView View;
        View.VertexData = Asset->Vertices.data();
        View.VertexCount = (uint32)Asset->Vertices.size();
        View.Stride = sizeof(FVertexSkinned);
        View.IndexData = Asset->Indices.data();
        View.IndexCount = (uint32)Asset->Indices.size();
        return View;
    }

    return {};
}

void USkinnedMeshComponent::RefreshReferencePose()
{
    RefPoseBoneLocalMatrices.clear();
    RefPoseBoneGlobalMatrices.clear();

    if (!SkeletalMesh || !SkeletalMesh->GetSkeleton())
    {
        return;
    }

    const TArray<FBoneInfo>& Bones = SkeletalMesh->GetSkeleton()->GetBones();
    const int32 BoneCount = static_cast<int32>(Bones.size());

    RefPoseBoneLocalMatrices.resize(BoneCount, FMatrix::Identity);
    RefPoseBoneGlobalMatrices.resize(BoneCount, FMatrix::Identity);

    for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
    {
        const FBoneInfo& Bone = Bones[BoneIndex];
        const FMatrix LocalMatrix = Bone.ReferenceTransform.ToMatrix();
        RefPoseBoneLocalMatrices[BoneIndex] = LocalMatrix;

        if (Bone.ParentIndex >= 0 && Bone.ParentIndex < BoneIndex)
        {
            RefPoseBoneGlobalMatrices[BoneIndex] = LocalMatrix * RefPoseBoneGlobalMatrices[Bone.ParentIndex];
        }
        else
        {
            RefPoseBoneGlobalMatrices[BoneIndex] = LocalMatrix;
        }
    }
}

void USkinnedMeshComponent::RefreshBoneTransforms()
{
    CurrentBoneGlobalMatrices.clear();

    const int32 BoneCount = static_cast<int32>(CurrentBoneLocalMatrices.size());
    if (BoneCount <= 0)
    {
        return;
    }

    CurrentBoneGlobalMatrices.resize(BoneCount, FMatrix::Identity);

    if (!SkeletalMesh || !SkeletalMesh->GetSkeleton())
    {
        CurrentBoneGlobalMatrices = CurrentBoneLocalMatrices;
        return;
    }

    const TArray<FBoneInfo>& Bones = SkeletalMesh->GetSkeleton()->GetBones();
    if (static_cast<int32>(Bones.size()) != BoneCount)
    {
        CurrentBoneGlobalMatrices = CurrentBoneLocalMatrices;
        return;
    }

    for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
    {
        const int32 ParentIndex = Bones[BoneIndex].ParentIndex;
        const FMatrix& LocalMatrix = CurrentBoneLocalMatrices[BoneIndex];

        if (ParentIndex >= 0 && ParentIndex < BoneIndex)
        {
            CurrentBoneGlobalMatrices[BoneIndex] = LocalMatrix * CurrentBoneGlobalMatrices[ParentIndex];
        }
        else
        {
            CurrentBoneGlobalMatrices[BoneIndex] = LocalMatrix;
        }
    }
}

void USkinnedMeshComponent::ResetToReferencePose()
{
    CurrentBoneLocalMatrices = RefPoseBoneLocalMatrices;

    if (!RefPoseBoneGlobalMatrices.empty() &&
        RefPoseBoneGlobalMatrices.size() == RefPoseBoneLocalMatrices.size())
    {
        CurrentBoneGlobalMatrices = RefPoseBoneGlobalMatrices;
    }
    else
    {
        RefreshBoneTransforms();
    }
}

bool USkinnedMeshComponent::SetBoneLocalMatrix(int32 BoneIndex, const FMatrix& LocalMatrix)
{
    if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(CurrentBoneLocalMatrices.size()))
    {
        return false;
    }

    CurrentBoneLocalMatrices[BoneIndex] = LocalMatrix;
    RefreshBoneTransforms();
    CacheLocalBounds();

    // CPU skinning 구현이 붙으면 여기서 갱신한다.
    // UpdateSkinningMatrices();
    // UpdateSkinnedVertices();

    MarkRenderStateDirty();
    MarkWorldBoundsDirty();
    return true;
}

void USkinnedMeshComponent::UpdateSkinningMatrices()
{
}

void USkinnedMeshComponent::UpdateSkinnedVertices()
{
}

const TArray<FVertexPNCT_T>& USkinnedMeshComponent::GetSkinnedVertices() const
{
    return SkinnedVertices;
}

const TArray<uint32>& USkinnedMeshComponent::GetIndices() const
{
    static const TArray<uint32> EmptyIndices;

    if (!SkeletalMesh)
    {
        return EmptyIndices;
    }

    for (USkeletalSubMesh* SubMesh : SkeletalMesh->GetSubMeshes())
    {
        if (!SubMesh || !SubMesh->GetSkeletalSubMeshAsset())
        {
            continue;
        }

        return SubMesh->GetSkeletalSubMeshAsset()->Indices;
    }

    return EmptyIndices;
}

int32 USkinnedMeshComponent::GetNumBones() const
{
    return static_cast<int32>(CurrentBoneGlobalMatrices.size());
}

const FBoneInfo* USkinnedMeshComponent::GetBoneInfo(int32 BoneIndex) const
{
    if (!SkeletalMesh || !SkeletalMesh->GetSkeleton())
    {
        return nullptr;
    }

    const TArray<FBoneInfo>& Bones = SkeletalMesh->GetSkeleton()->GetBones();
    if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(Bones.size()))
    {
        return nullptr;
    }

    return &Bones[BoneIndex];
}

const FMatrix& USkinnedMeshComponent::GetBoneLocalMatrix(int32 BoneIndex) const
{
    static const FMatrix Identity = FMatrix::Identity;

    if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(CurrentBoneLocalMatrices.size()))
    {
        return Identity;
    }

    return CurrentBoneLocalMatrices[BoneIndex];
}

const FMatrix& USkinnedMeshComponent::GetBoneComponentMatrix(int32 BoneIndex) const
{
    static const FMatrix Identity = FMatrix::Identity;

    if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(CurrentBoneGlobalMatrices.size()))
    {
        return Identity;
    }

    return CurrentBoneGlobalMatrices[BoneIndex];
}

FMatrix USkinnedMeshComponent::GetBoneWorldMatrix(int32 BoneIndex) const
{
	// Blender scaling convention
    return GetBoneComponentMatrix(BoneIndex) * FMatrix::MakeScaleMatrix(0.01f) * GetWorldMatrix();
}

int32 USkinnedMeshComponent::FindBoneIndex(const FName& BoneName) const
{
    if (!SkeletalMesh || !SkeletalMesh->GetSkeleton())
    {
        return -1;
    }

    return SkeletalMesh->GetSkeleton()->FindBoneIndex(BoneName.ToString());
}
