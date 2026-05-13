#pragma once
#include "Viewport/ViewportClient.h"
#include "Core/CoreGlobals.h"
#include "Math/Rotator.h"
#include "Math/Vector.h"
#include "UI/SWindow.h"
#include "Math/Quat.h"
#include <d3d11.h>
#include <memory>

struct FPreviewViewportOptions
{
    bool bShowMesh = true;
    bool bShowSkeleton = true;
    int32 SelectedBoneIndex = 0;
};

class FViewport;
class FPreviewSceneContext;
class FSkeletalMeshViewer;
class FSkelViewerViewportInputController;
class UCameraComponent;
class UEditorEngine;
class UGizmoComponent;
class UWorld;

class FPreviewViewportClient : public FViewportClient
{
public:
    FPreviewViewportClient();
    ~FPreviewViewportClient();

    void Initialize(UEditorEngine* InEditorEngine, 
					ID3D11Device* InDevice,
					FPreviewSceneContext* InPreviewContext, 
					FSkeletalMeshViewer* InOwner
					);
	void Release();
    void Tick(float DeltaTime);

    void ResetCamera();

    void SetPreviewOptions(const FPreviewViewportOptions& InOptions) { Options = InOptions; }
	//void SetViewport(FViewport* InViewport) { Viewport = InViewport; }
	void SetViewportRect(float X, float Y, float Width, float Height);
    bool GetViewportRect(FRect& OutRect) const;

    FViewport* GetViewport() const override { return Viewport; }
    UCameraComponent* GetCamera() const override;
    UGizmoComponent* GetGizmo();
    UWorld* GetWorld() const;

	void RenderViewportImage();

    virtual void Draw(FViewport* Viewport, float DeltaTime) override;
    virtual void BeginInputFrame() override;
    virtual bool InputKey(const FViewportKeyEvent& Event) override;
    virtual bool InputAxis(const FViewportAxisEvent& Event) override;
    virtual bool InputPointer(const FViewportPointerEvent& Event) override;
    virtual void ResetInputState() override;
    virtual void ResetKeyboardInputState() override;
private:
    void ApplyOrbitToCamera();
    void SyncBoneGizmoToSelection();
    void ApplyBoneGizmoToSelection();

    UEditorEngine* EditorEngine = nullptr;
    FPreviewSceneContext* PreviewContext = nullptr;
    FSkeletalMeshViewer* OwnerViewer = nullptr;

    FViewport* Viewport = nullptr;
    float ViewportX = 0;
    float ViewportY = 0;
    float ViewportWidth = 256;
    float ViewportHeight = 256;

    FPreviewViewportOptions Options;

    FVector OrbitPivot = FVector(0.0f, 0.0f, 0.0f);
    float OrbitDistance = 8.0f;
    float OrbitYaw = -135.0f;
    float OrbitPitch = 23.0f;

    FQuat LastBoneGizmoComponentRotation;
    bool bHasLastBoneGizmoComponentRotation = false;

    std::unique_ptr<FSkelViewerViewportInputController> InputController;
};
