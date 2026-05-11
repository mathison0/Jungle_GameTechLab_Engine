#pragma once

#include "Component/MeshComponent.h"

class USkeletalMesh;
struct FBoneInfo;

enum class ESkeletalDebugPoseMode
{
    SkinBindPose,
    FbxLocalPose
};

class USkinnedMeshComponent : public UMeshComponent
{
public:
    DECLARE_CLASS(USkinnedMeshComponent, UMeshComponent)
    USkinnedMeshComponent() = default;
    ~USkinnedMeshComponent() override = default;

    FMeshDataView GetMeshDataView() const override;
    void SetSkeletalMesh(USkeletalMesh* InMesh);
    USkeletalMesh* GetSkeletalMesh() const { return SkeletalMesh; }

    void RefreshReferencePose();
    void RefreshDisplayPose();
    void RefreshEditedDisplayPose();
    void RefreshBoneTransforms();
    void UpdateSkinningMatrices();
    void UpdateSkinnedVertices();

	void ResetToReferencePose();

	bool SetBoneLocalMatrix(int32 BoneIndex, const FMatrix& LocalMatrix);
	const FMatrix& GetBoneLocalMatrix(int32 BoneIndex) const;
	const FMatrix& GetBoneComponentMatrix(int32 BoneIndex) const;
    FMatrix GetBoneDebugWorldMatrix(int32 BoneIndex) const;
    void SetSkeletalDebugPoseMode(ESkeletalDebugPoseMode InMode);
    ESkeletalDebugPoseMode GetSkeletalDebugPoseMode() const { return DebugPoseMode; }


	const TArray<FVertexPNCT_T>& GetSkinnedVertices() const;
    const TArray<uint32>& GetIndices() const;

    int32 GetNumBones() const;
    const FBoneInfo* GetBoneInfo(int32 BoneIndex) const;
    FMatrix GetBoneWorldMatrix(int32 BoneIndex) const;
    int32 FindBoneIndex(const FName& BoneName) const;

protected:
    void CacheLocalBounds() override;

    USkeletalMesh* SkeletalMesh = nullptr;

	TArray<FMatrix> RefPoseBoneLocalMatrices; // reference pose에서 각 본의 Local transform을 저장하는 배열
    TArray<FMatrix> RefPoseBoneGlobalMatrices;
    TArray<FMatrix> DisplayPoseBoneLocalMatrices;
    TArray<FMatrix> DisplayPoseBoneGlobalMatrices;
    TArray<FMatrix> EditedDisplayPoseBoneGlobalMatrices;
    TArray<FMatrix> CurrentBoneLocalMatrices; // 현재 pose에서 각 본의 Local transform을 저장하는 배열
    TArray<FMatrix> CurrentBoneGlobalMatrices;

	TArray<FMatrix> SkinningMatrices;

    TArray<FVertexPNCT_T> SkinnedVertices;
    TArray<uint32> SkinnedIndices;

    FString SkeletalMeshPath = "None";
    ESkeletalDebugPoseMode DebugPoseMode = ESkeletalDebugPoseMode::SkinBindPose;
};
