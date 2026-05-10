#pragma once
#include "Viewport/ViewportClient.h"
#include "Core/CoreGlobals.h"
#include "Math/Vector.h"
#include <d3d11.h>

struct FPreviewInput
{
    bool bHovered = false;
    bool bActive = false;

    bool bLeftDown = false;
    bool bRightDown = false;
    bool bMiddleDown = false;

    bool bAltDown = false;
    bool bCtrlDown = false;
    bool bShiftDown = false;

    FVector2 MousePos;
    FVector2 MouseDelta;
    float MouseWheel = 0.0f;
};

struct FPreviewViewportOptions
{
    bool bShowMesh = true;
    bool bShowSkeleton = true;
    bool bShowBoneNames = false;
    int32 SelectedBoneIndex = 0;
};

class FViewport;
class FPreviewSceneContext;
class UCameraComponent;
class UEditorEngine;

class FPreviewViewportClient : public FViewportClient
{
public:
    void Initialize(UEditorEngine* InEditorEngine, ID3D11Device* InDevice, FPreviewSceneContext* InPreviewContext);
	void Release();
    void Update();
    void Tick(float DeltaTime);

    void ResetCamera();

	void SetInputState(const FPreviewInput& InInput) { Input = InInput; }
    void SetPreviewOptions(const FPreviewViewportOptions& InOptions) { Options = InOptions; }
	//void SetViewport(FViewport* InViewport) { Viewport = InViewport; }
	void SetViewportRect(float X, float Y, float Width, float Height);

    FViewport* GetViewport() const override { return Viewport; }
    UCameraComponent* GetCamera() const override;
	void RenderViewportImage();

    virtual void Draw(FViewport* Viewport, float DeltaTime) override;
private:
    void ApplyOrbitToCamera();

    UEditorEngine* EditorEngine = nullptr;
    FPreviewSceneContext* PreviewContext = nullptr;

    FViewport* Viewport = nullptr;
    float ViewportX = 0;
    float ViewportY = 0;
    float ViewportWidth = 256;
    float ViewportHeight = 256;

	FPreviewInput Input;
    FPreviewViewportOptions Options;

    FVector OrbitPivot = FVector(0.0f, 0.0f, 0.0f);
    float OrbitDistance = 8.0f;
    float OrbitYaw = -135.0f;
    float OrbitPitch = 23.0f;
};
