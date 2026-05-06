#pragma once

#include "Core/CoreMinimal.h"
#include "Math/Matrix.h"
#include "Math/Quat.h"
#include "Render/Common/ViewTypes.h"
#include "Viewport/ViewportRect.h"

class FViewportCamera;
class UCameraComponent;
class UCameraModifier;
struct FSceneView;
enum class EViewMode : int32;

// 최종적인 카메라 데이터를 RenderBus에 전달하기 위한 구조체
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

struct FCameraTransitionState
{
	bool bActive = false;

	FCameraViewInfo FromView;
	FCameraViewInfo ToView;

	FVector ControlPointA = FVector::ZeroVector;
	FVector ControlPointB = FVector::ZeroVector;
	bool bUseBezierCurve = false;

	float Duration = 0.0f;
	float Elapsed = 0.0f;

	// Cubic Bezier: Ease In/Out: 출발할 때 부드럽게 가속/도착할 때 부드럽게 감속
	FVector2 EaseControlPointA = FVector2(0.25f, 0.0f);
	FVector2 EaseControlPointB = FVector2(0.75f, 1.0f);
};

struct FCameraFadeState
{
	bool bActive = false;
	bool bHoldWhenFinished = false;

	FVector Color = FVector(0.0f, 0.0f, 0.0f);
	float FromAlpha = 0.0f;
	float ToAlpha = 0.0f;
	float CurrentAlpha = 0.0f;
	float Duration = 0.0f;
	float Elapsed = 0.0f;
};

// 게임 내 카메라의 최종 결정자, 최종 위치/회전/FOV 값이 모두 APlayerCameraManager에서 결정
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

	const FCameraViewInfo& GetCameraView() const { return CachedCameraView; }
	const FPostProcessSettings& GetPostProcessSettings() const { return CachedPostProcessSettings; }
	const FCameraOverlaySettings& GetOverlaySettings() const { return CachedCameraOverlaySettings; }

	// Fade
	void StartCameraFade(const FVector& Color, float FromAlpha, float ToAlpha, float Duration, bool bHoldWhenFinished = false);
	void SetManualCameraFade(const FVector& Color, float Alpha);
	void StopCameraFade();
	bool IsCameraFading() const { return FadeState.bActive; }

	// Modifier
	void AddCameraModifier(UCameraModifier* Modifier);
	void RemoveCameraModifier(UCameraModifier* Modifier);
	void ClearCameraModifiers();

	// Transition
	void StartCameraTransition(const FCameraViewInfo& From, const FCameraViewInfo& To, float Duration);
	void StartCameraTransitionBezier(const FCameraViewInfo& From, const FCameraViewInfo& To, const FVector& ControlPointA, const FVector& ControlPointB, float Duration);
	void StopCameraTransition();
	void UpdateCameraTransition(float DeltaTime, FCameraViewInfo& InOutView);
	float EvaluateTransitionAlpha(float NormalizedTime) const;
	FCameraViewInfo BlendCameraView(float Alpha) const;
	FVector EvaluateBezierPosition(float Alpha) const;

private:
	bool BuildBaseCameraView(FCameraViewInfo& OutView) const;
	void UpdateCameraFade(float DeltaTime);

	void ApplyCameraModifiers(float DeltaTime, FCameraViewInfo& InOutView);
	void ApplyPostProcessModifiers(float DeltaTime, FPostProcessSettings& InOutSettings);
	void ApplyOverlayModifiers(float DeltaTime, FCameraOverlaySettings& InOutOverlay);
	void ApplyCameraFade(FCameraOverlaySettings& InOutOverlay) const;

	void FillSceneView(FSceneView& OutView, const FCameraViewInfo& CameraView, const FViewportRect& ViewRect, EViewMode ViewMode) const;

private:
	UCameraComponent* ViewTarget = nullptr;
	FViewportCamera* FallbackCamera = nullptr;
	TArray<UCameraModifier*> CameraModifiers;
	
	bool bHasCachedCameraView = false;
	FCameraViewInfo CachedCameraView;
	FPostProcessSettings CachedPostProcessSettings;
	FCameraOverlaySettings CachedCameraOverlaySettings;

	FCameraTransitionState Transition;
	FCameraFadeState FadeState;
};
