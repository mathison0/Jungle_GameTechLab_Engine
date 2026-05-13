#pragma once
#include "Core/RayTypes.h"
#include "Preview/PreviewSceneContext.h"
#include "UI/EditorSkeletalMeshViewerPanel.h"
#include "Math/Transform.h"
#include "Viewport/PreviewViewportClient.h"
#include "Render/Execute/Context/Scene/ViewTypes.h"
#include <d3d11.h>

struct FSkeletalMeshViewerState
{
    USkeletalMesh* ActiveMesh = nullptr;
    int32 SelectedBoneIndex = -1;
    TArray<TArray<uint32>> BoneHierarchy;
    EViewMode ViewMode;

    bool bShowMesh = true;
    bool bShowSkeleton = true;
    bool bUseFbxLocalSkeleton = false;

	void reset() {
        ActiveMesh			= nullptr;
		SelectedBoneIndex	= -1;
        BoneHierarchy.clear();

        bShowMesh		= true;
        bShowSkeleton	= true;
        bUseFbxLocalSkeleton = false;
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
    void RenderBoneDebugLines();

	USkeletalMesh* GetSkeletalMesh();
    void SetSkeletalMesh(USkeletalMesh* InSkeletalMesh);
    void BuildBoneHierarchy();
    void ClearBoneSelection();
    void SelectBone(int32 BoneIndex);

    // Input Picking함수
    bool RaycastBonePicking(const FRay& Ray, int32& BestBoneIndex);
	//Gizmo Wrapper함수들
    bool GetCachedBoneLocalTransform(int32 BoneIndex, FTransform& OutTransform);
    bool SetCachedBoneLocalTransform(int32 BoneIndex, const FTransform& NewTransform, bool bApplyToComponent = true);
    FQuat GetCachedBoneComponentRotation(int32 BoneIndex);
    FVector GetCachedBoneComponentScale(int32 BoneIndex);

    FSkeletalMeshViewerState& GetState() { return ViewerState; }
    FPreviewSceneContext& GetPreviewScene() { return ViewerScene; };
    FPreviewViewportClient& GetViewportClient() { return ViewportClient; };

    uint32 GetEditorId() const { return ViewerID; }
    bool IsOpen() const { return bOpen; }
    void SetOpen(bool bInOpen) { bOpen = bInOpen; }

private:
	//외부 Manager에서 확인하기위한 index
    uint32 ViewerID = 0;
    bool bOpen = false;
    FSkeletalMeshViewerState ViewerState;
    FEditorSkeletalMeshViewerPanel ViewerPanel;

	FPreviewViewportClient ViewportClient;
    FPreviewSceneContext ViewerScene;
};
