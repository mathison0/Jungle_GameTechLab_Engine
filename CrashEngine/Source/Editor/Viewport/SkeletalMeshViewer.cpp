#include "SkeletalMeshViewer.h"

void FSkeletalMeshViewer::Initialize(uint32 InEditorId, UEditorEngine* InEditorEngine, ID3D11Device* InDevice, USkeletalMesh* InSkeletalMesh)
{
	ViewerID = InEditorId;
    ViewerState.reset();
    ViewerState.ActiveMesh = InSkeletalMesh;

    ViewerPanel.Initialize(InEditorEngine, this);

    //ViewerScene.Initialize(EditorEngine);
    //ViewerScene.SetSkeletalMesh(InSkeletalMesh);
    //ViewportClient.Initialize(EditorEngine, &PreviewScene, Device);
}

void FSkeletalMeshViewer::Release() 
{
    ViewerPanel.Release();
	//ViewportClient.Release();
	//ViewerScene.Release();
}
void FSkeletalMeshViewer::Tick(float DeltaTime) 
{
	//client
}

void FSkeletalMeshViewer::Render(float DeltaTime)
{
    ViewerPanel.Render(DeltaTime);
	//scene
	//client?

	if (bFlag)
    {
        //client.Render(DeltaTime);
	}
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

void FSkeletalMeshViewer::ApplyPreviewFlags()
{
	bFlag = true;
}

FSkeletalMeshViewerState& FSkeletalMeshViewer::GetState()
{
	return ViewerState;
}
