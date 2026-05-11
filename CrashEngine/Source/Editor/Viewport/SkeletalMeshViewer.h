#pragma once
#include "Editor/UI/EditorSkeletalMeshViewerPanel.h"
#include "Viewport/PreviewViewportClient.h"
#include "Preview/PreviewSceneContext.h"
#include <d3d11.h>
#include <Math/Transform.h>

enum ESkeletalMeshPreviewPoseMode
{
    TPose,
	APose,
	Count
};

struct FSkeletalMeshViewerState
{
    USkeletalMesh* ActiveMesh = nullptr;
	// 뷰어에서 보이고 변경되는 건 복사본
    //USkeletalMesh ActiveMeshCopy = nullptr;

    int32 SelectedBoneIndex = -1;

    bool bShowMesh = true;
    bool bShowSkeleton = true;
    bool bShowBoneNames = false;
    bool bUseFbxLocalSkeleton = false;

    //ESkeletalMeshPreviewPoseMode PoseMode = ESkeletalMeshPreviewPoseMode::BindPose;

	void reset() {
        ActiveMesh			= nullptr;
		SelectedBoneIndex	= -1;

        bShowMesh		= true;
        bShowSkeleton	= true;
        bShowBoneNames	= false;
        bUseFbxLocalSkeleton = false;

        //PoseMode = ESkeletalMeshPreviewPoseMode::TPose;
	}
};

class FPreviewViewportClient;

class FSkeletalMeshViewer 
{
public:
    void Initialize(
        uint32 InEditorId,
        UEditorEngine* InEditorEngine,
        ID3D11Device* InDevice,
        USkeletalMesh* InSkeletalMesh);

    void Release();
    void Tick(float DeltaTime);
    void Render(float DeltaTime);
	//outline처럼 이미 존재한다는 것을 보여줌
    void RequestFocus();

    uint32 GetEditorId() const;
    bool IsOpen() const;
    void SetOpen(bool bInOpen);

	void SetBoneLocalTransform(int32 BoneIndex, const FTransform& Transform);
    void ApplyPreviewFlags();
    FSkeletalMeshViewerState& GetState();
    FPreviewSceneContext& GetPreviewScene() {return ViewerScene;};
    FPreviewViewportClient& GetViewportClient() { return ViewportClient;};

private:
	//외부 Manager에서 확인하기위한 index
    uint32 ViewerID = 0;
    bool bOpen = false;
	bool bFlag = false;

    FSkeletalMeshViewerState ViewerState;
    FEditorSkeletalMeshViewerPanel ViewerPanel;

	FPreviewViewportClient ViewportClient;
    FPreviewSceneContext ViewerScene;
};
