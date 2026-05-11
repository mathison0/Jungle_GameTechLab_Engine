#include "PreviewSceneContext.h"
#include "Component/GizmoComponent.h"
#include "Component/SceneComponent.h"
#include "Component/SkeletalMeshComponent.h"
#include "Component/StaticMeshComponent.h"
#include "EditorEngine.h"

void FPreviewSceneContext::Initialize(UEditorEngine* InEditorEngine)
{
	EditorEngine = InEditorEngine;
    auto Context = EditorEngine->CreateWorldContext(EWorldType::Preview, FName("Preview"));
	PreviewWorld = Context.World;
    PreviewWorld->InitWorld();

    PreviewActor = PreviewWorld->SpawnActor<AActor>();
    if (PreviewActor)
    {
        PreviewMeshComponent = PreviewActor->AddComponent<USkeletalMeshComponent>();
        PreviewActor->SetRootComponent(PreviewMeshComponent);
	}

    BoneGizmoTargetActor = PreviewWorld->SpawnActor<AActor>();
    if (BoneGizmoTargetActor)
    {
        USceneComponent* Root = BoneGizmoTargetActor->AddComponent<USceneComponent>();
        BoneGizmoTargetActor->SetRootComponent(Root);
        BoneGizmo = BoneGizmoTargetActor->AddComponent<UGizmoComponent>();
        if (BoneGizmo)
        {
            BoneGizmo->SetWorldSpace(true);
            BoneGizmo->SetVisibility(false);
        }
    }

    Camera = UObjectManager::Get().CreateObject<UCameraComponent>(PreviewWorld);
    if (Camera)
    {
        Camera->OnResize(256, 256);
        PreviewWorld->SetActiveCamera(Camera);
    }
}

void FPreviewSceneContext::Release()
{
    if (Camera)
    {
        UObjectManager::Get().DestroyObject(Camera);
    }

    if (PreviewWorld)
    {
        EditorEngine->DestroyWorldContext(FName("Preview"));
    }

    PreviewWorld = nullptr;
    PreviewActor = nullptr;
    PreviewMeshComponent = nullptr;
    BoneGizmoTargetActor = nullptr;
    BoneGizmo = nullptr;
    Camera = nullptr;
    EditorEngine = nullptr;
}

void FPreviewSceneContext::Tick(float DeltaTime)
{
    if (!PreviewActor) return;

    PreviewActor->Tick(DeltaTime);
}

void FPreviewSceneContext::SetSkeletalMesh(USkeletalMesh* SkeletalMesh) {
    if (!PreviewMeshComponent)
    {
        return;
    }

    PreviewMeshComponent->SetSkeletalMesh(SkeletalMesh);
}
