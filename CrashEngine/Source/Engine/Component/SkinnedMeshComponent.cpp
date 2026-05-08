#include "Component/SkinnedMeshComponent.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(USkinnedMeshComponent, UMeshComponent)

// 임시구현 (SkeletalMeshAsset과 SkeletalMesh 작성된 후 최종작성)
void USkinnedMeshComponent::SetSkeletalMesh(USkeletalMesh* InMesh)
{
    SkeletalMesh = InMesh;
    OverrideMaterials.clear();
    MaterialSlots.clear();

	if (SkeletalMesh)
    {
        /*SkeletalMeshPath = SkeletalMesh->GetAssetPathFileName();
        const TArray<FDynamicMaterial>& DefaultMaterials = SkeletalMesh->GetMaterials();

        OverrideMaterials.resize(DefaultMaterials.size());
        MaterialSlots.resize(DefaultMaterials.size());

        for (int32 i = 0; i < (int32)DefaultMaterials.size(); ++i)
        {
            OverrideMaterials[i] = DefaultMaterials[i].MaterialInterface;

            if (OverrideMaterials[i])
                MaterialSlots[i].Path = OverrideMaterials[i]->GetAssetPathFileName();
            else
                MaterialSlots[i].Path = "None";
        }*/
    }
    else
    {
        SkeletalMeshPath = "None";
		OverrideMaterials.clear();
        MaterialSlots.clear();
		
	}

	RefreshReferencePose();
    RefreshBoneTransforms();
    UpdateSkinningMatrices();
    UpdateSkinnedVertices();
    
	CacheLocalBounds();
    MarkRenderStateDirty();
    MarkWorldBoundsDirty();
}

void USkinnedMeshComponent::CacheLocalBounds()
{
    bHasValidBounds = false;
	if (!SkeletalMesh)
        return;
 //   FSkeletalMesh* Asset = SkeletalMesh->GetSkeletalMeshAsset();
 //   if (!Asset || Asset->Vertices.empty())
 //       return;

	//// Reuse cached asset bounds when available.
 //   if (!Asset->bBoundsValid)
 //   {
 //       Asset->CacheBounds();
 //   }

 //   CachedLocalCenter = Asset->BoundsCenter;
 //   CachedLocalExtent = Asset->BoundsExtent;
 //   bHasValidBounds = Asset->bBoundsValid;
}

FMeshDataView USkinnedMeshComponent::GetMeshDataView() const
{
    /*if (!SkeletalMesh)
        return {};
    FSkeletalMesh* Asset = SkeletalMesh->GetSkeletalMeshAsset();
    if (!Asset || Asset->Vertices.empty())
        return {};

    FMeshDataView View;
    View.VertexData = Asset->SkinnedVertices.data();
    View.VertexCount = (uint32)Asset->Vertices.size();
    View.Stride = sizeof(FVertexPNCT_T);
    View.IndexData = Asset->Indices.data();
    View.IndexCount = (uint32)Asset->Indices.size();

    return View;*/
    return {};
}

void USkinnedMeshComponent::RefreshReferencePose()
{
    /*RefPoseBoneLocalMatrices.clear();
    RefPoseBoneGlobalMatrices.clear();

    if (!SkeletalMesh)
    {
        return;
    }

    const TArray<FBoneInfo>& Bones = SkeletalMesh->GetBones();
    const int32 BoneCount = static_cast<int32>(Bones.size());

    RefPoseBoneLocalMatrices.resize(BoneCount, FMatrix::Identity);
    RefPoseBoneGlobalMatrices.resize(BoneCount, FMatrix::Identity);

    for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
    {
        const FBoneInfo& Bone = Bones[BoneIndex];

        const FMatrix LocalMatrix = Bone.ReferenceLocalTransform.ToMatrix();
        RefPoseBoneLocalMatrices[BoneIndex] = LocalMatrix;

        if (Bone.ParentIndex >= 0 && Bone.ParentIndex < BoneIndex)
        {
            RefPoseBoneGlobalMatrices[BoneIndex] =
                LocalMatrix * RefPoseBoneGlobalMatrices[Bone.ParentIndex];
        }
        else
        {
            RefPoseBoneGlobalMatrices[BoneIndex] = LocalMatrix;
        }
    }*/
}

void USkinnedMeshComponent::RefreshBoneTransforms()
{
    CurrentBoneLocalMatrices.clear();
    CurrentBoneGlobalMatrices.clear();

    if (RefPoseBoneLocalMatrices.empty() || RefPoseBoneGlobalMatrices.empty())
    {
        return;
    }

    CurrentBoneLocalMatrices = RefPoseBoneLocalMatrices;
    CurrentBoneGlobalMatrices = RefPoseBoneGlobalMatrices;
}


//CPU skinning
void USkinnedMeshComponent::UpdateSkinningMatrices() {};
void USkinnedMeshComponent::UpdateSkinnedVertices() {};

const TArray<FVertexPNCT_T>& USkinnedMeshComponent::GetSkinnedVertices() const
{
    return SkinnedVertices;
}

const TArray<uint32>& USkinnedMeshComponent::GetIndices() const
{
    static const TArray<uint32> EmptyIndices;

    // TODO: return SkeletalMesh asset indices after USkeletalMesh API is merged.
    return EmptyIndices;
}

int32 USkinnedMeshComponent::GetNumBones() const
{
    return static_cast<int32>(CurrentBoneGlobalMatrices.size());
}

const FBoneInfo* USkinnedMeshComponent::GetBoneInfo(int32 BoneIndex) const
{
    // TODO: return SkeletalMesh->GetBoneInfo(BoneIndex) after FBoneInfo storage is merged.
    return nullptr;
}

const FMatrix& USkinnedMeshComponent::GetBoneWorldMatrix(int32 BoneIndex) const
{
    static const FMatrix Identity = FMatrix::Identity;

    if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(CurrentBoneGlobalMatrices.size()))
    {
        return Identity;
    }

    return CurrentBoneGlobalMatrices[BoneIndex];
}

int32 USkinnedMeshComponent::FindBoneIndex(const FName& BoneName) const
{
    // TODO: return SkeletalMesh->FindBoneIndex(BoneName) after skeleton lookup API is merged.
    return -1;
}
