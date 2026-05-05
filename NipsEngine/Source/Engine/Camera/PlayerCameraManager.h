#pragma once

#include "Core/CoreMinimal.h"
#include "Math/Matrix.h"
#include "Math/Quat.h"
#include "Viewport/ViewportRect.h"

class FViewportCamera;
class UCameraComponent;
class UCameraModifier;
struct FSceneView;
enum class EViewMode : int32;

struct FCameraViewInfo
{
	FVector Location = FVector::ZeroVector;
	FQuat Rotation = FQuat::Identity;

	float FOV = 3.14159265358979f / 3.0f;
	float AspectRatio = 16.0f / 9.0f;
	float NearPlane = 0.1f;
	float FarPlane = 1000.0f;
	float OrthoWidth = 10.0f;
	float OrthoHeight = 10.0f;
	bool bOrthographic = false;

	FVector GetForwardVector() const { return Rotation.GetForwardVector(); }
	FVector GetRightVector() const { return Rotation.GetRightVector(); }
	FVector GetUpVector() const { return Rotation.GetUpVector(); }
};

class APlayerCameraManager
{
public:
	APlayerCameraManager() = default;

	void SetViewTarget(UCameraComponent* InCamera);
	void SetViewTargetWithBlend(UCameraComponent* InCamera, float BlendTime = 0.0f);
	UCameraComponent* GetViewTarget() const { return ViewTarget; }

	void SetFallbackCamera(FViewportCamera* InCamera);
	FViewportCamera* GetFallbackCamera() const { return FallbackCamera; }

	void UpdateCamera(float DeltaTime);
	void BuildSceneView(FSceneView& OutView, const FViewportRect& ViewRect, EViewMode ViewMode) const;

	void AddCameraModifier(UCameraModifier* Modifier);
	void RemoveCameraModifier(UCameraModifier* Modifier);
	void ClearCameraModifiers();

	void StartCameraTransition(const FCameraViewInfo& FromView, const FCameraViewInfo& ToView, float Duration);
	const FCameraViewInfo& GetCameraView() const { return CachedCameraView; }

private:
	bool BuildBaseCameraView(FCameraViewInfo& OutView) const;
	void ApplyCameraModifiers(float DeltaTime, FCameraViewInfo& InOutView);
	void FillSceneView(FSceneView& OutView, const FCameraViewInfo& CameraView, const FViewportRect& ViewRect, EViewMode ViewMode) const;

private:
	UCameraComponent* ViewTarget = nullptr;
	FViewportCamera* FallbackCamera = nullptr;
	TArray<UCameraModifier*> CameraModifiers;
	FCameraViewInfo CachedCameraView;
	bool bHasCachedCameraView = false;
};
