#pragma once
#include "Editor/UI/EditorSkeletalMeshViewerPanel.h"
#include <d3d11.h>

enum ESkeletalMeshPreviewPoseMode
{
    TPose,
	APose,
	Count
};

struct FSkeletalMeshViewerState
{
    USkeletalMesh* ActiveMesh = nullptr;

    int32 SelectedBoneIndex = -1;

    bool bShowMesh = true;
    bool bShowSkeleton = true;
    bool bShowBoneNames = false;

    //ESkeletalMeshPreviewPoseMode PoseMode = ESkeletalMeshPreviewPoseMode::BindPose;

	void reset() {
        ActiveMesh			= nullptr;
		SelectedBoneIndex	= -1;

        bShowMesh		= true;
        bShowSkeleton	= true;
        bShowBoneNames	= false;

        //PoseMode = ESkeletalMeshPreviewPoseMode::TPose;
	}
};

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

    void ApplyPreviewFlags();
    FSkeletalMeshViewerState& GetState();
    //FSkeletalMeshPreviewScene& GetPreviewScene();
    //FSkeletalMeshEditorViewportClient& GetViewportClient();

private:
	//외부 Manager에서 확인하기위한 index
    uint32 ViewerID = 0;
    bool bOpen = true;
	bool bFlag = false;

    FSkeletalMeshViewerState ViewerState;
    FEditorSkeletalMeshViewerPanel ViewerPanel;
	//FSkeletalMeshEditorViewportClient ViewportClient;
	//FSkeletalMeshPreviewScene ViewerScene;
};
