#pragma once

#include "Editor/UI/EditorPanel.h"

class FSkeletalMeshViewer;
class USkeletalMesh;
class USkeletalMeshComponent;

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
private:
    FSkeletalMeshViewer* Owner = nullptr;
	//FSkeletalPreviewViewportClient PreviewClient;

    // 실제 SkeletalMesh 참조
    USkeletalMesh* SkeletalMesh = nullptr;
	//실제 SkeletalMesh가 아닌 복제
    USkeletalMeshComponent* PreviewMeshComponent = nullptr;
    int SelectedBoneIndex = 0;

	uint32 RootBoneIndex = 1;
    TArray<TArray<uint32>> BonesHierarchy;
    //TArray<FBoneInfo> BoneInfos;
};
