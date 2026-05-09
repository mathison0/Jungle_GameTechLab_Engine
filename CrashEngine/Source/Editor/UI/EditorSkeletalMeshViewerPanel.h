#pragma once

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
    void RenderBoneDebugLine(int32 index);

private:
    TArray<FTransform> CurrentLocalPose;
    TArray<FTransform> CurrentGlobalPose;

    FSkeletalMeshViewer* Owner = nullptr;
	//FSkeletalPreviewViewportClient PreviewClient;

    // 실제 SkeletalMesh 참조
    USkeletalMesh* SkeletalMesh = nullptr;
	//실제 SkeletalMesh가 아닌 복제
    USkeletalMeshComponent* PreviewMeshComponent = nullptr;
    int SelectedBoneIndex = INDEX_NONE;

    TArray<TArray<uint32>> BonesHierarchy;
    TArray<FViewerBoneInfo> BoneInfos;
};
