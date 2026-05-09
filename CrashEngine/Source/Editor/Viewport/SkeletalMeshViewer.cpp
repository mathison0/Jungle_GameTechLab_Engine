#include "SkeletalMeshViewer.h"

void FSkeletalMeshViewer::Initialize(uint32 InEditorId, UEditorEngine* InEditorEngine, ID3D11Device* InDevice, USkeletalMesh* InSkeletalMesh)
{
    bOpen = true;
	ViewerID = InEditorId;
    ViewerState.reset();
    ViewerState.ActiveMesh = InSkeletalMesh;

    ViewerPanel.Initialize(InEditorEngine, this);
    ViewerPanel.SetSkeletalMesh(InSkeletalMesh);

    ViewerScene.Initialize(InEditorEngine);
    ViewerScene.SetSkeletalMesh(InSkeletalMesh);
	
    ViewportClient.Initialize(InEditorEngine, InDevice, &ViewerScene);
}

void FSkeletalMeshViewer::Release() 
{
    bOpen = false;
    ViewerPanel.Release();
	ViewportClient.Release();
	ViewerScene.Release();
}
void FSkeletalMeshViewer::Tick(float DeltaTime) 
{
    ViewerScene.Tick(DeltaTime);
	ViewportClient.Tick(DeltaTime);
}

void FSkeletalMeshViewer::Render(float DeltaTime)
{
    ViewerPanel.Render(DeltaTime);
}

void FSkeletalMeshViewer::RequestFocus()
{
}

uint32 FSkeletalMeshViewer::GetEditorId() const
{
    return ViewerID;
}

bool FSkeletalMeshViewer::IsOpen() const
{
    return bOpen;
}

void FSkeletalMeshViewer::SetOpen(bool bInOpen)
{
    bOpen = bInOpen;
}

void FSkeletalMeshViewer::SetBoneLocalTransform(int32 BoneIndex, const FTransform& Transform)
{
    if (USkeletalMeshComponent* Comp = ViewerScene.GetPreviewMeshComponent())
    {
		//여기서 SkeletalMesh LocalMatrix변경 요청함
        //Comp->SetBoneLocalTransform(BoneIndex, Transform);

    }
}

void FSkeletalMeshViewer::ApplyPreviewFlags()
{
	bFlag = true;
}

FSkeletalMeshViewerState& FSkeletalMeshViewer::GetState()
{
	return ViewerState;
}
