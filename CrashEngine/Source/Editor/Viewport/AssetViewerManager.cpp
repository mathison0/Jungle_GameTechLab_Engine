#include "AssetViewerManager.h"
#include "Editor/Viewport/SkeletalMeshViewer.h"

void FAssetViewerManager::Initialize(UEditorEngine* InEditorEngine, ID3D11Device* InDevice)
{
    EditorEngine = InEditorEngine;
    Device = InDevice;
    NextViewerId = 1;
}

void FAssetViewerManager::Release()
{
    for (std::unique_ptr<FSkeletalMeshViewer>& Viewer : Viewers)
    {
        if (Viewer)
        {
            Viewer->Release();
        }
    }

    Viewers.clear();
    EditorEngine = nullptr;
    Device = nullptr;
}

void FAssetViewerManager::Tick(float DeltaTime)
{
    if (Viewers.empty()) return;
    for (std::unique_ptr<FSkeletalMeshViewer>& Viewer : Viewers)
    {
        if (Viewer && Viewer->IsOpen())
        {
            Viewer->Tick(DeltaTime);
        }
    }
}

void FAssetViewerManager::Render(float DeltaTime)
{
    for (auto It = Viewers.begin(); It != Viewers.end(); It++)
    {
        FSkeletalMeshViewer* Viewer = It->get();
        if (!Viewer)
        {
            It = Viewers.erase(It);
            continue;
        }

        if (!Viewer->IsOpen())
        {
            Viewer->Release();
            It = Viewers.erase(It);
            continue;
        }

        Viewer->Render(DeltaTime);
    }
}

FSkeletalMeshViewer* FAssetViewerManager::OpenSkeletalMeshEditor(USkeletalMesh* Mesh)
{
	//이미 존재하는지 확인
    if (FSkeletalMeshViewer* Existing = FindSkeletalMeshEditor(Mesh))
    {
        Existing->RequestFocus();
        return Existing;
    }

    auto Editor = std::make_unique<FSkeletalMeshViewer>();
    FSkeletalMeshViewer* RawEditor = Editor.get();

    RawEditor->Initialize(NextViewerId++, EditorEngine, Device, Mesh);
    Viewers.push_back(std::move(Editor));

    return RawEditor;
}

FSkeletalMeshViewer* FAssetViewerManager::FindSkeletalMeshEditor(USkeletalMesh* Mesh)
{
    if (Viewers.empty()) return nullptr;

    for (auto It = Viewers.begin(); It != Viewers.end(); It++)
    {
        FSkeletalMeshViewer* Viewer = It->get();
        return Viewer;
	}
	return nullptr;
}
