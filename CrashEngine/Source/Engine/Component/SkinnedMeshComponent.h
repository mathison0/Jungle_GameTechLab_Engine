#pragma once

#include "Component/MeshComponent.h"

class USkeletalMesh;
struct FBoneInfo;

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
    void RefreshBoneTransforms();
    void UpdateSkinningMatrices();
    void UpdateSkinnedVertices();

	void ResetToReferencePose();

	bool SetBoneLocalMatrix(int32 BoneIndex, const FMatrix& LocalMatrix);
	const FMatrix& GetBoneLocalMatrix(int32 BoneIndex) const;
	const FMatrix& GetBoneComponentMatrix(int32 BoneIndex) const;


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
    TArray<FMatrix> CurrentBoneLocalMatrices; // 현재 pose에서 각 본의 Local transform을 저장하는 배열
    TArray<FMatrix> CurrentBoneGlobalMatrices;

	TArray<FMatrix> SkinningMatrices;

    TArray<FVertexPNCT_T> SkinnedVertices;

    FString SkeletalMeshPath = "None";
};

/*
= SkeletalMesh 포인터
= bone transform 배열
= reference pose를 실제 본 행렬로 계산
= CPU skinning
= skinned vertex 결과
= bounds cache
*/

/* 아래와 같은 구조로 가정하고 작성해둠
struct FSkeletalVertex
{
    FVector Position;
    FVector Normal;
    FVector4 Color;
    FVector2 UV;
    FVector4 Tangent;
    uint16 BoneIndices[4];
    float BoneWeights[4];
};

struct FBoneInfo
{
    FName Name;
    int32 ParentIndex;
    FTransform ReferenceLocalTransform;
    FMatrix InverseBindPose;
};
*/
