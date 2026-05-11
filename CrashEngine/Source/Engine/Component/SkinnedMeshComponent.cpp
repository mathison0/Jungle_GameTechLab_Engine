#include "Component/SkinnedMeshComponent.h"

#include "Engine/Runtime/Engine.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/Skeleton.h"
#include "Object/ObjectFactory.h"
#include "Render/Scene/Proxies/Primitive/SkeletalMeshSceneProxy.h"
#include "Collision/RayUtils.h"

#include <algorithm>

IMPLEMENT_CLASS(USkinnedMeshComponent, UMeshComponent)

namespace
{
    FMatrix GetMeshBindGlobalInverse(const FSkeletalSubMesh* Asset)
    {
        if (!Asset)
        {
            return FMatrix::Identity;
        }

        return Asset->MeshBindGlobalInverse;
    }
}

bool USkinnedMeshComponent::LineTraceComponent(const FRay& Ray, FHitResult& OutHit)
{
    if (!SkeletalMesh)
        return false;
    float TMin, TMax;
    if (!FRayUtils::IntersectRayAABB(Ray, GetWorldBoundingBox().Min, GetWorldBoundingBox().Max, TMin, TMax))
        return false;

    const FMatrix& World = GetWorldMatrix();
    const FMatrix& WorldInv = GetWorldInverseMatrix();

    bool bAny = false;
    FHitResult Best{};
    Best.Distance = FLT_MAX;

    const auto& SubMeshes = SkeletalMesh->GetSubMeshes();
    FHitResult Hit{};
    if (FRayUtils::RaycastTriangles(Ray, World, WorldInv,
                                    SkinnedVertices.data(), sizeof(FVertexPNCT_T),
                                    SkinnedIndices.data(), (uint32)SkinnedIndices.size(), Hit) &&
        Hit.Distance < Best.Distance)
    {
        Best = Hit;
        bAny = true;
    }
    
    if (bAny)
    {
        Best.HitComponent = this;
        OutHit = Best;
        return true;
    }

    return false;
}

void USkinnedMeshComponent::SetSkeletalMesh(USkeletalMesh* InMesh)
{
    SkeletalMesh = InMesh;
    OverrideMaterials.clear();
    MaterialSlots.clear();
    SkinnedRenderBuffers.clear();

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
    RefreshDisplayPose();
    if (DebugPoseMode == ESkeletalDebugPoseMode::FbxLocalPose &&
        DisplayPoseBoneLocalMatrices.size() == RefPoseBoneLocalMatrices.size())
    {
        CurrentBoneLocalMatrices = DisplayPoseBoneLocalMatrices;
        RefreshBoneTransforms();
    }
    else
    {
        ResetToReferencePose();
    }

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
    {
        return;
    }

    bool bFoundVertex = false;
    FVector LocalMin;
    FVector LocalMax;

    if (!SkinnedVertices.empty())
    {
        for (const FVertexPNCT_T& Vertex : SkinnedVertices)
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
    else
    {
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
    if (SkinnedVertices.empty() || SkinnedIndices.empty())
    {
        return {};
    }

    FMeshDataView View;
    View.VertexData = SkinnedVertices.data();
    View.VertexCount = (uint32)SkinnedVertices.size();
    View.Stride = sizeof(FVertexPNCT_T);
    View.IndexData = SkinnedIndices.data();
    View.IndexCount = (uint32)SkinnedIndices.size();
    return View;
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
    const TArray<FMatrix>& ReferenceLocalMatrices = SkeletalMesh->GetSkeleton()->GetReferenceLocalMatrices();
    const int32 BoneCount = static_cast<int32>(Bones.size());
    const bool bHasReferenceLocalMatrices = ReferenceLocalMatrices.size() == Bones.size();

    RefPoseBoneLocalMatrices.resize(BoneCount, FMatrix::Identity);
    RefPoseBoneGlobalMatrices.resize(BoneCount, FMatrix::Identity);

    for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
    {
        const FBoneInfo& Bone = Bones[BoneIndex];
        const FMatrix LocalMatrix = bHasReferenceLocalMatrices
            ? ReferenceLocalMatrices[BoneIndex]
            : Bone.ReferenceTransform.ToMatrix();
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

void USkinnedMeshComponent::RefreshDisplayPose()
{
    DisplayPoseBoneLocalMatrices.clear();
    DisplayPoseBoneGlobalMatrices.clear();
    EditedDisplayPoseBoneGlobalMatrices.clear();

    if (!SkeletalMesh || !SkeletalMesh->GetSkeleton())
    {
        return;
    }

    const TArray<FBoneInfo>& Bones = SkeletalMesh->GetSkeleton()->GetBones();
    const TArray<FTransform>& DisplayTransforms = SkeletalMesh->GetSkeleton()->GetDisplayTransforms();
    const TArray<FMatrix>& DisplayLocalMatrices = SkeletalMesh->GetSkeleton()->GetDisplayLocalMatrices();
    const int32 BoneCount = static_cast<int32>(Bones.size());
    const bool bHasDisplayLocalMatrices = DisplayLocalMatrices.size() == Bones.size();

    DisplayPoseBoneLocalMatrices.resize(BoneCount, FMatrix::Identity);
    DisplayPoseBoneGlobalMatrices.resize(BoneCount, FMatrix::Identity);

    for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
    {
        const FBoneInfo& Bone = Bones[BoneIndex];
        FMatrix LocalMatrix = FMatrix::Identity;
        if (bHasDisplayLocalMatrices)
        {
            LocalMatrix = DisplayLocalMatrices[BoneIndex];
        }
        else
        {
            const FTransform& DisplayTransform =
                BoneIndex < static_cast<int32>(DisplayTransforms.size())
                    ? DisplayTransforms[BoneIndex]
                    : Bone.ReferenceTransform;
            LocalMatrix = DisplayTransform.ToMatrix();
        }
        DisplayPoseBoneLocalMatrices[BoneIndex] = LocalMatrix;

        if (Bone.ParentIndex >= 0 && Bone.ParentIndex < BoneIndex)
        {
            DisplayPoseBoneGlobalMatrices[BoneIndex] = LocalMatrix * DisplayPoseBoneGlobalMatrices[Bone.ParentIndex];
        }
        else
        {
            DisplayPoseBoneGlobalMatrices[BoneIndex] = LocalMatrix;
        }
    }

    RefreshEditedDisplayPose();
}

void USkinnedMeshComponent::RefreshEditedDisplayPose()
{
    EditedDisplayPoseBoneGlobalMatrices.clear();

    const int32 BoneCount = static_cast<int32>(DisplayPoseBoneLocalMatrices.size());
    if (BoneCount <= 0)
    {
        return;
    }

    EditedDisplayPoseBoneGlobalMatrices.resize(BoneCount, FMatrix::Identity);

    const bool bCanApplyCurrentEdits =
        CurrentBoneLocalMatrices.size() == DisplayPoseBoneLocalMatrices.size() &&
        RefPoseBoneLocalMatrices.size() == DisplayPoseBoneLocalMatrices.size();

    for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
    {
        FMatrix LocalMatrix = DisplayPoseBoneLocalMatrices[BoneIndex];
        if (bCanApplyCurrentEdits)
        {
            const FMatrix EditDelta = RefPoseBoneLocalMatrices[BoneIndex].GetInverse() * CurrentBoneLocalMatrices[BoneIndex];
            LocalMatrix = EditDelta * DisplayPoseBoneLocalMatrices[BoneIndex];
        }

        int32 ParentIndex = -1;
        if (SkeletalMesh && SkeletalMesh->GetSkeleton())
        {
            const TArray<FBoneInfo>& Bones = SkeletalMesh->GetSkeleton()->GetBones();
            if (BoneIndex < static_cast<int32>(Bones.size()))
            {
                ParentIndex = Bones[BoneIndex].ParentIndex;
            }
        }

        if (ParentIndex >= 0 && ParentIndex < BoneIndex)
        {
            EditedDisplayPoseBoneGlobalMatrices[BoneIndex] = LocalMatrix * EditedDisplayPoseBoneGlobalMatrices[ParentIndex];
        }
        else
        {
            EditedDisplayPoseBoneGlobalMatrices[BoneIndex] = LocalMatrix;
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

    RefreshEditedDisplayPose();
}

bool USkinnedMeshComponent::SetBoneLocalMatrix(int32 BoneIndex, const FMatrix& LocalMatrix)
{
    if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(CurrentBoneLocalMatrices.size()))
    {
        return false;
    }

    CurrentBoneLocalMatrices[BoneIndex] = LocalMatrix;
    RefreshBoneTransforms();
    RefreshEditedDisplayPose();
    UpdateSkinningMatrices();
    UpdateSkinnedVertices();
    CacheLocalBounds();

    MarkRenderStateDirty();
    MarkWorldBoundsDirty();
    return true;
}

void USkinnedMeshComponent::UpdateSkinningMatrices()
{
    SkinningMatrices.clear();

    const int32 BoneCount = static_cast<int32>(CurrentBoneGlobalMatrices.size());
    if (BoneCount <= 0 || RefPoseBoneGlobalMatrices.size() != CurrentBoneGlobalMatrices.size())
    {
        return;
    }

    SkinningMatrices.resize(BoneCount, FMatrix::Identity);
    for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
    {
        SkinningMatrices[BoneIndex] =
            RefPoseBoneGlobalMatrices[BoneIndex].GetInverse() * CurrentBoneGlobalMatrices[BoneIndex];
    }
}

void USkinnedMeshComponent::UpdateSkinnedVertices()
{
    SkinnedVertices.clear();
    SkinnedIndices.clear();

    if (!SkeletalMesh)
    {
        return;
    }

    uint32 VertexBase = 0;
    ID3D11Device* Device = GEngine ? GEngine->GetRenderer().GetFD3DDevice().GetDevice() : nullptr;
    ID3D11DeviceContext* Context = GEngine ? GEngine->GetRenderer().GetFD3DDevice().GetDeviceContext() : nullptr;
    const TArray<USkeletalSubMesh*>& SubMeshes = SkeletalMesh->GetSubMeshes();
    if (SkinnedRenderBuffers.size() != SubMeshes.size())
    {
        SkinnedRenderBuffers.clear();
        SkinnedRenderBuffers.resize(SubMeshes.size());
    }

    for (uint32 i = 0; i < SubMeshes.size(); i++)
    {
		auto* SubMesh = SubMeshes[i];

        if (!SubMesh || !SubMesh->GetSkeletalSubMeshAsset())
        {
            continue;
        }

        FSkeletalSubMesh* Asset = SubMesh->GetSkeletalSubMeshAsset();
        SkinnedVertices.reserve(SkinnedVertices.size() + Asset->Vertices.size());
        SkinnedIndices.reserve(SkinnedIndices.size() + Asset->Indices.size());

        TArray<FVertexSkinned> SkinnedRenderVertices;
        SkinnedRenderVertices.reserve(Asset->Vertices.size());
        const bool bHasFbxSkinningBindData =
            Asset->InverseBindPoseMatrices.size() == CurrentBoneGlobalMatrices.size() &&
            Asset->BoneBindGlobalMatrices.size() == CurrentBoneGlobalMatrices.size() &&
            RefPoseBoneGlobalMatrices.size() == CurrentBoneGlobalMatrices.size() &&
            !Asset->InverseBindPoseMatrices.empty();
        const FMatrix MeshBindGlobalInverse = GetMeshBindGlobalInverse(Asset);

        for (const FVertexSkinned& SourceVertex : Asset->Vertices)
        {
            FVertexPNCT_T SkinnedVertex;
            SkinnedVertex.Position = SourceVertex.Position;
            SkinnedVertex.Normal = SourceVertex.Normal;
            SkinnedVertex.Color = SourceVertex.Color;
            SkinnedVertex.UV = SourceVertex.UV;
            SkinnedVertex.Tangent = SourceVertex.Tangent;

            FVector SkinnedPosition(0.0f, 0.0f, 0.0f);
            FVector SkinnedNormal(0.0f, 0.0f, 0.0f);
            FVector SkinnedTangent(0.0f, 0.0f, 0.0f);
            float TotalWeight = 0.0f;

            for (int32 InfluenceIndex = 0; InfluenceIndex < 8; ++InfluenceIndex)
            {
                const float Weight = SourceVertex.BoneWeights[InfluenceIndex];
                const int32 BoneIndex = static_cast<int32>(SourceVertex.BoneIndices[InfluenceIndex]);
                const int32 MatrixCount = bHasFbxSkinningBindData
                    ? static_cast<int32>(CurrentBoneGlobalMatrices.size())
                    : static_cast<int32>(SkinningMatrices.size());
                if (Weight <= 0.0f || BoneIndex < 0 || BoneIndex >= MatrixCount)
                {
                    continue;
                }

                FMatrix SkinMatrix;
                if (bHasFbxSkinningBindData)
                {
                    const FMatrix BoneCurrentForSkin =
                        Asset->BoneBindGlobalMatrices[BoneIndex] *
                        RefPoseBoneGlobalMatrices[BoneIndex].GetInverse() *
                        CurrentBoneGlobalMatrices[BoneIndex];

                    SkinMatrix =
                        Asset->InverseBindPoseMatrices[BoneIndex] *
                        BoneCurrentForSkin *
                        MeshBindGlobalInverse;
                }
                else
                {
                    SkinMatrix = SkinningMatrices[BoneIndex];
                }

                SkinnedPosition += SkinMatrix.TransformPositionWithW(SourceVertex.Position) * Weight;
                SkinnedNormal += SkinMatrix.TransformVector(SourceVertex.Normal) * Weight;
                SkinnedTangent += SkinMatrix.TransformVector(FVector(SourceVertex.Tangent.X, SourceVertex.Tangent.Y, SourceVertex.Tangent.Z)) * Weight;
                TotalWeight += Weight;
            }

            if (TotalWeight > 0.0f)
            {
                SkinnedVertex.Position = SkinnedPosition / TotalWeight;
                SkinnedVertex.Normal = SkinnedNormal / TotalWeight;
                SkinnedVertex.Normal.Normalize();

                SkinnedTangent /= TotalWeight;
                SkinnedTangent.Normalize();
                SkinnedVertex.Tangent = FVector4(SkinnedTangent, SourceVertex.Tangent.W);
            }

            FVertexSkinned SkinnedRenderVertex = SourceVertex;
            SkinnedRenderVertex.Position = SkinnedVertex.Position;
            SkinnedRenderVertex.Normal = SkinnedVertex.Normal;
            SkinnedRenderVertex.Tangent = SkinnedVertex.Tangent;

            SkinnedVertices.push_back(SkinnedVertex);
            SkinnedRenderVertices.push_back(SkinnedRenderVertex);
        }

        for (uint32 Index : Asset->Indices)
        {
            SkinnedIndices.push_back(VertexBase + Index);
        }

        if (Device && Context && !SkinnedRenderVertices.empty())
        {
            std::unique_ptr<FSkeletalMeshBuffer>& OwnedBuffer = SkinnedRenderBuffers[i];
            FSkeletalMeshBuffer* RenderBuffer = OwnedBuffer.get();
            const uint32 SubMeshVertexCount = static_cast<uint32>(Asset->Vertices.size());
            const bool bNeedsCreate =
                !RenderBuffer ||
                !RenderBuffer->IsValid() ||
                RenderBuffer->GetVertexStride() != sizeof(FVertexSkinned) ||
                RenderBuffer->GetVertexCount() != SubMeshVertexCount;

            if (bNeedsCreate)
            {
                TMeshData<FVertexSkinned> RenderMeshData;
                RenderMeshData.Vertices = SkinnedRenderVertices;
                RenderMeshData.Indices = Asset->Indices;

                OwnedBuffer = std::make_unique<FSkeletalMeshBuffer>();
                RenderBuffer = OwnedBuffer.get();
                RenderBuffer->Create(Device, RenderMeshData);
            }
            else
            {
                RenderBuffer->UpdateVertex(Context, SkinnedRenderVertices.data(), SubMeshVertexCount);
            }
        }

        VertexBase += static_cast<uint32>(Asset->Vertices.size());
    }
}

const TArray<FVertexPNCT_T>& USkinnedMeshComponent::GetSkinnedVertices() const
{
    return SkinnedVertices;
}

const TArray<uint32>& USkinnedMeshComponent::GetIndices() const
{
    return SkinnedIndices;
}

FSkeletalMeshBuffer* USkinnedMeshComponent::GetSkinnedRenderBuffer(int32 SubMeshIndex) const
{
    if (SubMeshIndex < 0 || SubMeshIndex >= static_cast<int32>(SkinnedRenderBuffers.size()))
    {
        return nullptr;
    }

    return SkinnedRenderBuffers[SubMeshIndex].get();
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
    const FSkeletalSubMesh* Asset = nullptr;
    if (SkeletalMesh)
    {
        for (USkeletalSubMesh* SubMesh : SkeletalMesh->GetSubMeshes())
        {
            if (SubMesh && SubMesh->GetSkeletalSubMeshAsset())
            {
                Asset = SubMesh->GetSkeletalSubMeshAsset();
                break;
            }
        }
    }

    const bool bHasFbxSkinningBindData =
        Asset &&
        BoneIndex >= 0 &&
        BoneIndex < static_cast<int32>(Asset->BoneBindGlobalMatrices.size()) &&
        Asset->BoneBindGlobalMatrices.size() == RefPoseBoneGlobalMatrices.size() &&
        Asset->BoneBindGlobalMatrices.size() == CurrentBoneGlobalMatrices.size();

    if (bHasFbxSkinningBindData)
    {
        const FMatrix BoneInMeshLocal =
            Asset->BoneBindGlobalMatrices[BoneIndex] *
            RefPoseBoneGlobalMatrices[BoneIndex].GetInverse() *
            CurrentBoneGlobalMatrices[BoneIndex] *
            Asset->MeshBindGlobalInverse;
        return BoneInMeshLocal * GetWorldMatrix();
    }

    return GetBoneComponentMatrix(BoneIndex) * GetWorldMatrix();
}

FMatrix USkinnedMeshComponent::GetBoneDebugWorldMatrix(int32 BoneIndex) const
{
    return GetBoneWorldMatrix(BoneIndex);
}

void USkinnedMeshComponent::SetSkeletalDebugPoseMode(ESkeletalDebugPoseMode InMode)
{
    if (DebugPoseMode == InMode)
    {
        return;
    }

    DebugPoseMode = InMode;

    if (DebugPoseMode == ESkeletalDebugPoseMode::FbxLocalPose &&
        DisplayPoseBoneLocalMatrices.size() == RefPoseBoneLocalMatrices.size())
    {
        CurrentBoneLocalMatrices = DisplayPoseBoneLocalMatrices;
        RefreshBoneTransforms();
    }
    else
    {
        ResetToReferencePose();
    }

    UpdateSkinningMatrices();
    UpdateSkinnedVertices();
    CacheLocalBounds();

    MarkRenderStateDirty();
    MarkWorldBoundsDirty();
}

int32 USkinnedMeshComponent::FindBoneIndex(const FName& BoneName) const
{
    if (!SkeletalMesh || !SkeletalMesh->GetSkeleton())
    {
        return -1;
    }

    return SkeletalMesh->GetSkeleton()->FindBoneIndex(BoneName.ToString());
}
