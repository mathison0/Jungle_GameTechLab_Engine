#pragma once

#include "Component/CameraComponent.h"
#include "Component/SkeletalMeshComponent.h"
#include "GameFramework/World.h"

class UEditorEngine;
class UStaticMeshComponent;

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

    void SetSkeletalMesh(USkeletalMesh* InMesh);
    void ResetPose();

private:
    UEditorEngine* EditorEngine = nullptr;

    UWorld* PreviewWorld = nullptr;
    AActor* PreviewActor = nullptr;
    USkeletalMeshComponent* PreviewMeshComponent = nullptr;
    UCameraComponent* Camera = nullptr;
};
