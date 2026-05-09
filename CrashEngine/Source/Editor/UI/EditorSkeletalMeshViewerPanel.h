#pragma once

#include "Core/EngineTypes.h"
#include "Editor/UI/EditorPanel.h"
#include "Object/FName.h"
#include "Math/Matrix.h"
#include "Math/Transform.h"

inline constexpr int32 INDEX_NONE = -1;
inline constexpr int32 RootBoneIndex = 0;

class FSkeletalMeshViewer;
class USkeletalMesh;
class USkeletalMeshComponent;

struct FViewerBoneInfo
{
    FName BoneName;
    int32 ParentIndex = INDEX_NONE;
    FTransform ReferenceLocalTransform;
    FMatrix InverseBindPose = FMatrix::Identity;
};

class FEditorSkeletalMeshViewerPanel : public FEditorPanel
{
public:
    void Initialize(UEditorEngine* InEditorEngine, FSkeletalMeshViewer* InOwner)
    {
        FEditorPanel::Initialize(InEditorEngine);
        Owner = InOwner;
    }

    // FEditorPanel을(를) 통해 상속됨
    void Render(float DeltaTime);

    void Release();
    void Update();

	void RenderToolbar();
    void RenderPreviewViewport(float DeltaTime);
	void RenderBoneHierarchyTree();
    void RenderBoneNode(uint32 RootBone);
	void RenderSelectedBoneTransformInspector();
	void SetSkeletalMesh(USkeletalMesh* InSkeletalMesh);
	void BuildBoneHierarchy();
    void RenderBoneDebugLine(int32 index, bool bInSelectedSubtree);

private:
    void RecalculatePoseFromBone(int32 BoneIndex);
    void RecalculatePoseRecursive(int32 BoneIndex);

    TArray<FTransform> CurrentLocalPose;
    TArray<FTransform> CurrentGlobalPose;

    FSkeletalMeshViewer* Owner = nullptr;
	//FSkeletalPreviewViewportClient PreviewClient;

    // 실제 SkeletalMesh 참조
    USkeletalMesh* SkeletalMesh = nullptr;
	//실제 SkeletalMesh가 아닌 복제
    USkeletalMeshComponent* PreviewMeshComponent = nullptr;

    TArray<TArray<uint32>> BonesHierarchy;
    TArray<FViewerBoneInfo> BoneInfos;

	FColor SelectedColor = FColor(124, 252, 0);
	FColor BoneColor = FColor(0, 0, 55);
    FColor SelectedChildColor = FColor::White();
    FColor SelectedParentColor = FColor(255,127,80);
};
