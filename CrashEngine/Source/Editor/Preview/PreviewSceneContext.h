#pragma once

#include "Component/CameraComponent.h"
#include "Component/SkeletalMeshComponent.h"
#include "GameFramework/World.h"

class UEditorEngine;
class UStaticMeshComponent;
class UGizmoComponent;

// preview 전용 world와 preview object들을 관리하는 wrapper
class FPreviewSceneContext
{
public:
    void Initialize(UEditorEngine* InEditorEngine);
    void Release();
    void Tick(float DeltaTime);

    UWorld* GetWorld() const { return PreviewWorld; }
    FScene* GetScene() const { return PreviewWorld ? &PreviewWorld->GetScene() : nullptr; }

	UCameraComponent* GetCamera() { return Camera; }
    USkeletalMeshComponent* GetPreviewMeshComponent() const { return PreviewMeshComponent; }
    AActor* GetBoneGizmoTargetActor() const { return BoneGizmoTargetActor; }
    UGizmoComponent* GetBoneGizmo() { return BoneGizmo; }

    void SetSkeletalMesh(USkeletalMesh* InMesh);

private:
    UEditorEngine* EditorEngine = nullptr;
    UCameraComponent* Camera = nullptr;

    UWorld* PreviewWorld = nullptr;
    AActor* PreviewActor = nullptr;
    USkeletalMeshComponent* PreviewMeshComponent = nullptr;

    AActor* BoneGizmoTargetActor = nullptr;
    UGizmoComponent* BoneGizmo = nullptr;
};
