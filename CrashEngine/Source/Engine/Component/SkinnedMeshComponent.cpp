#include "Component/SkinnedMeshComponent.h"

#include "Engine/Runtime/Engine.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/Skeleton.h"
#include "Object/ObjectFactory.h"
#include "Render/Scene/Proxies/Primitive/SkeletalMeshSceneProxy.h"
#include "Collision/RayUtils.h"

#include <algorithm>
#include <Profiling/Stats.h>

IMPLEMENT_CLASS(USkinnedMeshComponent, UMeshComponent)

namespace
{
    FMatrix GetMeshBindInverseScaleMatrix(const FSkeletalSubMesh* Asset)
    {
        if (!Asset)
        {
            return FMatrix::Identity;
        }

        return FMatrix::MakeScaleMatrix(Asset->MeshBindInverseScale);
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

void USkinnedMeshComponent::RefreshBoneTransformsFrom(int32 BoneIndex)
{
    SCOPE_STAT_CAT("RefreshBoneTransformsFrom", "USkinnedMeshComponent");
    const int32 BoneCount = static_cast<int32>(CurrentBoneLocalMatrices.size());
    if (BoneCount <= 0 || !SkeletalMesh || !SkeletalMesh->GetSkeleton())
        return;

    const TArray<FBoneInfo>& Bones = SkeletalMesh->GetSkeleton()->GetBones();
    if (static_cast<int32>(Bones.size()) != BoneCount)
        return;

    // StartBoneIndex부터만 순회 (그 이전 부모들은 이미 유효)
    for (int32 i = BoneIndex; i < BoneCount; ++i)
    {
        const int32 ParentIndex = Bones[i].ParentIndex;
        const FMatrix& Local = CurrentBoneLocalMatrices[i];

        if (ParentIndex >= 0 && ParentIndex < i)
        {
            CurrentBoneGlobalMatrices[i] = Local * CurrentBoneGlobalMatrices[ParentIndex];
        }
        else
        {
            CurrentBoneGlobalMatrices[i] = Local;
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
    RefreshBoneTransformsFrom(BoneIndex);
    RefreshEditedDisplayPoseFrom(BoneIndex);
    UpdateSkinningMatricesFrom(BoneIndex);

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
    SCOPE_STAT_CAT("UpdateSkinnedVertices", "USkinnedMeshComponent");

    SkinnedVertices.clear();
    SkinnedIndices.clear();

    if (!SkeletalMesh)
        return;

    ID3D11Device* Device = GEngine ? GEngine->GetRenderer().GetFD3DDevice().GetDevice() : nullptr;
    ID3D11DeviceContext* Context = GEngine ? GEngine->GetRenderer().GetFD3DDevice().GetDeviceContext() : nullptr;
    const TArray<USkeletalSubMesh*>& SubMeshes = SkeletalMesh->GetSubMeshes();

    if (SkinnedRenderBuffers.size() != SubMeshes.size())
    {
        SkinnedRenderBuffers.clear();
        SkinnedRenderBuffers.resize(SubMeshes.size());
    }

    uint32 VertexBase = 0;

    for (uint32 i = 0; i < SubMeshes.size(); ++i)
    {
        USkeletalSubMesh* SubMesh = SubMeshes[i];
        if (!SubMesh || !SubMesh->GetSkeletalSubMeshAsset())
            continue;

        FSkeletalSubMesh* Asset = SubMesh->GetSkeletalSubMeshAsset();
        const uint32 VertexCount = static_cast<uint32>(Asset->Vertices.size());

        const bool bHasFbxSkinningBindData =
            Asset->InverseBindPoseMatrices.size() == CurrentBoneGlobalMatrices.size() &&
            Asset->BoneBindGlobalMatrices.size() == CurrentBoneGlobalMatrices.size() &&
            RefPoseBoneGlobalMatrices.size() == CurrentBoneGlobalMatrices.size() &&
            !Asset->InverseBindPoseMatrices.empty();

        const FMatrix MeshBindInverseScale = GetMeshBindInverseScaleMatrix(Asset);

        const int32 BoneCount = static_cast<int32>(CurrentBoneGlobalMatrices.size());
        TArray<FMatrix> PerBoneSkinMatrices;
        PerBoneSkinMatrices.resize(BoneCount);

        if (bHasFbxSkinningBindData)
        {
            for (int32 BoneIdx = 0; BoneIdx < BoneCount; ++BoneIdx)
            {
                const FMatrix BoneCurrentForSkin =
                    Asset->BoneBindGlobalMatrices[BoneIdx] *
                    RefPoseBoneGlobalMatrices[BoneIdx].GetInverse() *
                    CurrentBoneGlobalMatrices[BoneIdx];

                PerBoneSkinMatrices[BoneIdx] =
                    Asset->InverseBindPoseMatrices[BoneIdx] *
                    BoneCurrentForSkin *
                    MeshBindInverseScale;
            }
        }
        else
        {
            PerBoneSkinMatrices = SkinningMatrices; // 이미 계산된 것 그대로 사용
        }

        SkinnedVertices.reserve(SkinnedVertices.size() + VertexCount);
        SkinnedIndices.reserve(SkinnedIndices.size() + Asset->Indices.size());

        TArray<FVertexSkinned> SkinnedRenderVertices;
        SkinnedRenderVertices.resize(VertexCount); // push_back 대신 인덱스 직접 접근

        for (uint32 VertIdx = 0; VertIdx < VertexCount; ++VertIdx)
        {
            const FVertexSkinned& Src = Asset->Vertices[VertIdx];

            FVector SkinnedPosition(0, 0, 0);
            FVector SkinnedNormal(0, 0, 0);
            FVector SkinnedTangent(0, 0, 0);
            float TotalWeight = 0.0f;

            for (int32 Inf = 0; Inf < 8; ++Inf)
            {
                const float Weight = Src.BoneWeights[Inf];
                if (Weight <= 0.0f)
                    continue;

                const int32 BoneIdx = static_cast<int32>(Src.BoneIndices[Inf]);
                if (BoneIdx < 0 || BoneIdx >= BoneCount)
                    continue;

                const FMatrix& SkinMatrix = PerBoneSkinMatrices[BoneIdx];

                SkinnedPosition += SkinMatrix.TransformPositionWithW(Src.Position) * Weight;
                SkinnedNormal += SkinMatrix.TransformVector(Src.Normal) * Weight;
                SkinnedTangent += SkinMatrix.TransformVector(
                                      FVector(Src.Tangent.X, Src.Tangent.Y, Src.Tangent.Z)) *
                                  Weight;
                TotalWeight += Weight;
            }

            FVertexPNCT_T Out;
            Out.Color = Src.Color;
            Out.UV = Src.UV;

            if (TotalWeight > 0.0f)
            {
                const float InvWeight = 1.0f / TotalWeight;
                Out.Position = SkinnedPosition * InvWeight;
                Out.Normal = (SkinnedNormal * InvWeight).Normalized();

                SkinnedTangent = (SkinnedTangent * InvWeight).Normalized();
                Out.Tangent = FVector4(SkinnedTangent, Src.Tangent.W);
            }
            else
            {
                Out.Position = Src.Position;
                Out.Normal = Src.Normal;
                Out.Tangent = Src.Tangent;
            }

            SkinnedVertices.push_back(Out);

            FVertexSkinned& RenderVert = SkinnedRenderVertices[VertIdx];
            RenderVert = Src;
            RenderVert.Position = Out.Position;
            RenderVert.Normal = Out.Normal;
            RenderVert.Tangent = Out.Tangent;
        }

        for (uint32 Index : Asset->Indices)
        {
            SkinnedIndices.push_back(VertexBase + Index);
        }

        if (Device && Context && !SkinnedRenderVertices.empty())
        {
            std::unique_ptr<FSkeletalMeshBuffer>& OwnedBuffer = SkinnedRenderBuffers[i];
            FSkeletalMeshBuffer* RenderBuffer = OwnedBuffer.get();
            const bool bNeedsCreate =
                !RenderBuffer || !RenderBuffer->IsValid() ||
                RenderBuffer->GetVertexStride() != sizeof(FVertexSkinned) ||
                RenderBuffer->GetVertexCount() != VertexCount;

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
                RenderBuffer->UpdateVertex(Context, SkinnedRenderVertices.data(), VertexCount);
            }
        }

        VertexBase += VertexCount;
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
    FMatrix DebugScaleMatrix = FMatrix::MakeScaleMatrix(0.01f);
    if (SkeletalMesh)
    {
        for (USkeletalSubMesh* SubMesh : SkeletalMesh->GetSubMeshes())
        {
            if (SubMesh && SubMesh->GetSkeletalSubMeshAsset())
            {
                DebugScaleMatrix = GetMeshBindInverseScaleMatrix(SubMesh->GetSkeletalSubMeshAsset());
                break;
            }
        }
    }

    return GetBoneComponentMatrix(BoneIndex) * DebugScaleMatrix * GetWorldMatrix();
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

void USkinnedMeshComponent::RefreshEditedDisplayPoseFrom(int32 StartBoneIndex)
{
    SCOPE_STAT_CAT("RefreshEditedDisplayPoseFrom", "USkinnedMeshComponent");
    const int32 BoneCount = static_cast<int32>(DisplayPoseBoneLocalMatrices.size());
    if (BoneCount <= 0 || !SkeletalMesh || !SkeletalMesh->GetSkeleton())
        return;

    const TArray<FBoneInfo>& Bones = SkeletalMesh->GetSkeleton()->GetBones();
    const bool bCanApply =
        CurrentBoneLocalMatrices.size() == DisplayPoseBoneLocalMatrices.size() &&
        RefPoseBoneLocalMatrices.size() == DisplayPoseBoneLocalMatrices.size();

    for (int32 i = StartBoneIndex; i < BoneCount; ++i)
    {
        FMatrix Local = DisplayPoseBoneLocalMatrices[i];
        if (bCanApply)
        {
            const FMatrix EditDelta = RefPoseBoneLocalMatrices[i].GetInverse() * CurrentBoneLocalMatrices[i];
            Local = EditDelta * DisplayPoseBoneLocalMatrices[i];
        }

        const int32 ParentIndex = Bones[i].ParentIndex;
        if (ParentIndex >= 0 && ParentIndex < i)
        {
            EditedDisplayPoseBoneGlobalMatrices[i] = Local * EditedDisplayPoseBoneGlobalMatrices[ParentIndex];
        }
        else
        {
            EditedDisplayPoseBoneGlobalMatrices[i] = Local;
        }
    }
}

void USkinnedMeshComponent::UpdateSkinningMatricesFrom(int32 StartBoneIndex)
{
    SCOPE_STAT_CAT("UpdateSkinningMatricesFrom", "USkinnedMeshComponent");
    const int32 BoneCount = static_cast<int32>(CurrentBoneGlobalMatrices.size());
    if (RefPoseBoneGlobalMatrices.size() != CurrentBoneGlobalMatrices.size())
        return;

    for (int32 i = StartBoneIndex; i < BoneCount; ++i)
    {
        SkinningMatrices[i] =
            RefPoseBoneGlobalMatrices[i].GetInverse() * CurrentBoneGlobalMatrices[i];
    }
}