#include "Engine/Camera/PlayerCameraManager.h"

#include "Engine/Component/CameraComponent.h"
#include "Engine/Component/PostProcessComponent.h"
#include "Engine/GameFramework/AActor.h"
#include "Engine/Camera/Modifier/LetterBoxCameraModifier.h"
#include "Engine/Camera/Modifier/CameraShakeModifier.h"
#include "Engine/Camera/Modifier/LuaCameraModifier.h"
#include "Engine/Runtime/SceneView.h"
#include "Engine/Viewport/ViewportCamera.h"
#include "Engine/Math/Bezier.h"
#include "Engine/Math/Utils.h"

#include <algorithm>
#include <cmath>

namespace
{
FQuat MakeCameraRotation(const FVector& Forward, const FVector& Right, const FVector& Up)
{
	FMatrix RotationMatrix = FMatrix::Identity;
	RotationMatrix.SetAxes(Forward.GetSafeNormal(), Right.GetSafeNormal(), Up.GetSafeNormal());

	FQuat Rotation(RotationMatrix);
	Rotation.Normalize();
	return Rotation;
}

bool BuildCameraComponentView(UCameraComponent* Camera, FCameraViewInfo& OutView)
{
	if (Camera == nullptr)
	{
		return false;
	}

	const float Height = Camera->GetHeight();

	OutView.Location = Camera->GetWorldLocation();
	OutView.Rotation = MakeCameraRotation(Camera->GetForwardVector(), Camera->GetRightVector(), Camera->GetUpVector());
	OutView.FOV = Camera->GetFOV();
	OutView.AspectRatio = Height > 0.0f ? Camera->GetWidth() / Height : 16.0f / 9.0f;
	OutView.NearPlane = Camera->GetNearPlane();
	OutView.FarPlane = Camera->GetFarPlane();
	OutView.OrthoWidth = Camera->GetOrthoWidth();
	OutView.OrthoHeight = OutView.AspectRatio > 0.0f ? OutView.OrthoWidth / OutView.AspectRatio : OutView.OrthoWidth;
	OutView.bOrthographic = Camera->IsOrthogonal();
	return true;
}
}

void APlayerCameraManager::SetViewTarget(UCameraComponent* InCamera)
{
	ViewTarget = InCamera;
	StopCameraTransition();
}

void APlayerCameraManager::SetViewTargetWithBlend(UCameraComponent* InCamera, float BlendTime)
{
	if (InCamera == nullptr || BlendTime <= 0.0f)
	{
		SetViewTarget(InCamera); // give-up blending transition
		return;
	}

	FCameraViewInfo ToView;
	if (!BuildCameraComponentView(InCamera, ToView))
	{
		SetViewTarget(InCamera);
		return;
	}

	FCameraViewInfo FromView;
	if (bHasCachedCameraView)
	{
		FromView = CachedCameraView;
	}
	else if (!BuildBaseCameraView(FromView))
	{
		FromView = ToView;
	}

	ViewTarget = InCamera;
	StartCameraTransition(FromView, ToView, BlendTime);
}

void APlayerCameraManager::SetFallbackCamera(FViewportCamera* InCamera)
{
	FallbackCamera = InCamera;
}

void APlayerCameraManager::UpdateCamera(float DeltaTime)
{
	FCameraViewInfo NewView;
	FPostProcessSettings NewPostProcess;
	FCameraOverlaySettings NewOverlay;

	if (!BuildBaseCameraView(NewView))
	{
		bHasCachedCameraView = false;
		return;
	}

	if (Transition.bActive)
	{
		UpdateCameraTransition(DeltaTime, NewView);
	}
	
	UpdateCameraFade(DeltaTime);

	ApplyCameraModifiers(DeltaTime, NewView);
	ApplyPostProcessComponent(NewPostProcess);
	ApplyPostProcessModifiers(DeltaTime, NewPostProcess);
	ApplyOverlayModifiers(DeltaTime, NewOverlay);
	ApplyCameraFade(NewOverlay);

	CachedCameraView = NewView;
	CachedPostProcessSettings = NewPostProcess;
	CachedCameraOverlaySettings = NewOverlay;

	bHasCachedCameraView = true;
}

void APlayerCameraManager::BuildSceneView(FSceneView& OutView, const FViewportRect& ViewRect, EViewMode ViewMode) const
{
	FCameraViewInfo ViewInfo = CachedCameraView;
	if (!bHasCachedCameraView && !BuildBaseCameraView(ViewInfo))
	{
		OutView.ViewRect = ViewRect;
		OutView.ViewMode = ViewMode;
		return;
	}

	FillSceneView(OutView, ViewInfo, ViewRect, ViewMode);
}

void APlayerCameraManager::InitializeDefaultModifiers()
{
	if (LetterBoxCameraModifier == nullptr)
	{
		LetterBoxCameraModifier = AddNewCameraModifier<ULetterBoxCameraModifier>();
	}
}

void APlayerCameraManager::Shutdown()
{
	ClearModifierList();

	for (UCameraModifier* Modifier : OwnedModifierList)
	{
		UObjectManager::Get().DestroyObject(Modifier);
	}
	OwnedModifierList.clear();

	LetterBoxCameraModifier = nullptr;
	CameraShakeModifier = nullptr;
	ViewTarget = nullptr;
	FallbackCamera = nullptr;
	bHasCachedCameraView = false;
	CachedCameraView = FCameraViewInfo();
	CachedPostProcessSettings = FPostProcessSettings();
	CachedCameraOverlaySettings = FCameraOverlaySettings();
	Transition = FCameraTransitionState();
	FadeState = FCameraFadeState();
}

ULetterBoxCameraModifier* APlayerCameraManager::GetLetterBoxCameraModifier()
{
	InitializeDefaultModifiers();
	return LetterBoxCameraModifier;
}

ULuaCameraModifier* APlayerCameraManager::AddLuaCameraModifier(const FString& ScriptPath)
{
	ULuaCameraModifier* Modifier = UObjectManager::Get().CreateObject<ULuaCameraModifier>();
	Modifier->SetScriptPath(ScriptPath);
	OwnedModifierList.push_back(Modifier);
	Modifier->AddedToCamera(this);
	AddCameraModifierToList(Modifier);
	return Modifier;
}

bool APlayerCameraManager::AddCameraModifierToList(UCameraModifier* NewModifier)
{
	if (NewModifier == nullptr)
	{
		return false;
	}

	if (std::find(ModifierList.begin(), ModifierList.end(), NewModifier) != ModifierList.end())
	{
		return false;
	}

	ModifierList.push_back(NewModifier);
	std::sort(ModifierList.begin(), ModifierList.end(), [](const UCameraModifier* A, const UCameraModifier* B)
			  {
		const int32 APriority = A ? A->GetPriority() : 0;
		const int32 BPriority = B ? B->GetPriority() : 0;
		return APriority < BPriority; });

	return true;
}

void APlayerCameraManager::RemoveCameraModifier(UCameraModifier* Modifier)
{
	if (Modifier == nullptr)
	{
		return;
	}

	const auto OldSize = ModifierList.size();
	ModifierList.erase(std::remove(ModifierList.begin(), ModifierList.end(), Modifier), ModifierList.end());
	if (ModifierList.size() != OldSize)
	{
		Modifier->RemovedFromCamera(this);
	}
}

void APlayerCameraManager::ClearModifierList()
{
	for (UCameraModifier* Modifier : ModifierList)
	{
		if (Modifier)
		{
			Modifier->RemovedFromCamera(this);
		}
	}
	ModifierList.clear();
}

void APlayerCameraManager::StartCameraFade(const FVector& Color, float FromAlpha, float ToAlpha, float Duration, bool bHoldWhenFinished)
{
	FadeState.bActive = true;
	FadeState.bHoldWhenFinished = bHoldWhenFinished;
	FadeState.Color = Color;
	FadeState.FromAlpha = MathUtil::Clamp(FromAlpha, 0.0f, 1.0f);
	FadeState.ToAlpha = MathUtil::Clamp(ToAlpha, 0.0f, 1.0f);
	FadeState.CurrentAlpha = FadeState.FromAlpha;
	FadeState.Duration = std::max(Duration, 0.0f);
	FadeState.Elapsed = 0.0f;

	if (FadeState.Duration <= 0.0f)
	{
		FadeState.CurrentAlpha = FadeState.ToAlpha;
		FadeState.bActive = false;

		if (!FadeState.bHoldWhenFinished)
		{
			FadeState.CurrentAlpha = 0.0f;
		}
	}
}

void APlayerCameraManager::SetManualCameraFade(const FVector& Color, float Alpha)
{
	FadeState.bActive = false;
	FadeState.bHoldWhenFinished = true;
	FadeState.Color = Color;
	FadeState.FromAlpha = MathUtil::Clamp(Alpha, 0.0f, 1.0f);
	FadeState.ToAlpha = FadeState.FromAlpha;
	FadeState.CurrentAlpha = FadeState.FromAlpha;
	FadeState.Duration = 0.0f;
	FadeState.Elapsed = 0.0f;
}

void APlayerCameraManager::StopCameraFade()
{
	FadeState = FCameraFadeState();
}

void APlayerCameraManager::StartLetterBox(float TargetRatio, float Duration)
{
	if (ULetterBoxCameraModifier* Modifier = GetLetterBoxCameraModifier())
	{
		Modifier->StartLetterBox(TargetRatio, Duration);
	}
}

void APlayerCameraManager::SetLetterBox(float Ratio)
{
	if (ULetterBoxCameraModifier* Modifier = GetLetterBoxCameraModifier())
	{
		Modifier->SetLetterBox(Ratio);
	}
}

void APlayerCameraManager::ClearLetterBox()
{
	if (ULetterBoxCameraModifier* Modifier = GetLetterBoxCameraModifier())
	{
		Modifier->ClearLetterBox();
	}
}

UCameraShakeModifier* APlayerCameraManager::GetCameraShakeModifier()
{
	if (CameraShakeModifier == nullptr)
	{
		CameraShakeModifier = AddNewCameraModifier<UCameraShakeModifier>();
	}

	return CameraShakeModifier;
}
void APlayerCameraManager::StartCameraShake(const FCameraShakeParams& Params)
{
	UCameraShakeModifier* Modifier = GetCameraShakeModifier();
	if (Modifier == nullptr)
		return;

	Modifier->StartShake(Params);
}

void APlayerCameraManager::StopCameraShake()
{
	if (CameraShakeModifier)
	{
		CameraShakeModifier->StopShake();
	}
}

bool APlayerCameraManager::IsCameraShaking() const
{
	return CameraShakeModifier && CameraShakeModifier->GetIsShaking();
}

// 카메라 Linear 보간 이동
void APlayerCameraManager::StartCameraTransition(const FCameraViewInfo& From, const FCameraViewInfo& To, float Duration)
{
	Transition.FromView = From;
	Transition.ToView = To;

	Transition.Duration = std::max(Duration, 0.001f);
	Transition.Elapsed = 0.0f;

	Transition.bUseBezierCurve = false;
	Transition.bActive = true;
}

// 카메라 Bezier Curve 보간 이동
void APlayerCameraManager::StartCameraTransitionBezier(const FCameraViewInfo& From, const FCameraViewInfo& To, const FVector& ControlPointA, const FVector& ControlPointB, float Duration)
{
	Transition.FromView = From;
	Transition.ToView = To;

	Transition.ControlPointA = ControlPointA;
	Transition.ControlPointB = ControlPointB;

	Transition.Duration = std::max(Duration, 0.001f);
	Transition.Elapsed = 0.0f;

	Transition.bUseBezierCurve = true;
	Transition.bActive = true;
}

void APlayerCameraManager::StopCameraTransition()
{
	Transition.bActive = false;
}

void APlayerCameraManager::UpdateCameraTransition(float DeltaTime, FCameraViewInfo& InOutView)
{
	if (!Transition.bActive)
	{
		return;
	}

	Transition.ToView = InOutView;
	Transition.Elapsed += DeltaTime;
	float NormalizedTime = MathUtil::Clamp(Transition.Elapsed / Transition.Duration, 0.0f, 1.0f);
	float Alpha = EvaluateTransitionAlpha(NormalizedTime);
	InOutView = BlendCameraView(Alpha);

	if (NormalizedTime >= 1.0f)
	{
		StopCameraTransition();
	}
}
	
// 정규화된 시간 값을 cubic-bezier easing curve의 x, y 값으로 변환합니다.
float APlayerCameraManager::EvaluateTransitionAlpha(float NormalizedTime) const
{
	const float ClampedTime = MathUtil::Clamp(NormalizedTime, 0.0f, 1.0f);
	const float ControlPoints[4] =
	{
		Transition.EaseControlPointA.X,
		Transition.EaseControlPointA.Y,
		Transition.EaseControlPointB.X,
		Transition.EaseControlPointB.Y
	};
	const float Alpha = Bezier::EvaluateCubicEasing(ClampedTime, ControlPoints);

	return MathUtil::Clamp(Alpha, 0.0f, 1.0f);
}

// 계산된 Transition Alpha 값을 바탕으로 From, To 카메라에 대한 위치, 회전, FOV 값 보간
FCameraViewInfo APlayerCameraManager::BlendCameraView(float Alpha) const
{
	FCameraViewInfo BlendedView;

	if (Transition.bUseBezierCurve)
	{
		BlendedView.Location = EvaluateBezierPosition(Alpha);
	}
	else
	{
		BlendedView.Location = FVector::Lerp(Transition.FromView.Location, Transition.ToView.Location, Alpha);
	}

	const FCameraViewInfo& From = Transition.FromView;
	const FCameraViewInfo& To = Transition.ToView;

	BlendedView.Rotation = FQuat::Slerp(From.Rotation, To.Rotation, Alpha);
	BlendedView.FOV = MathUtil::Lerp(From.FOV, To.FOV, Alpha);
	BlendedView.AspectRatio = MathUtil::Lerp(From.AspectRatio, To.AspectRatio, Alpha);
	BlendedView.NearPlane = MathUtil::Lerp(From.NearPlane, To.NearPlane, Alpha);
	BlendedView.FarPlane = MathUtil::Lerp(From.FarPlane, To.FarPlane, Alpha);
	BlendedView.OrthoWidth = MathUtil::Lerp(From.OrthoWidth, To.OrthoWidth, Alpha);
	BlendedView.OrthoHeight = MathUtil::Lerp(From.OrthoHeight, To.OrthoHeight, Alpha);
	BlendedView.bOrthographic = Alpha < 0.5f ? From.bOrthographic : To.bOrthographic;

	return BlendedView;
}

FVector APlayerCameraManager::EvaluateBezierPosition(float Alpha) const
{
	float T = Alpha;
	float U = 1.0f - Alpha;

	const FVector& P0 = Transition.FromView.Location;
	const FVector& P1 = Transition.ControlPointA;
	const FVector& P2 = Transition.ControlPointB;
	const FVector& P3 = Transition.ToView.Location;

	FVector Position = (U * U * U * P0) + (3.0f * U * U * T * P1) + (3.0f * U * T * T * P2) + (T * T * T * P3);

	return Position;
}

// ViewTarget이 유효하다면 ViewTarget을 기준으로 Base Camera View 생성
bool APlayerCameraManager::BuildBaseCameraView(FCameraViewInfo& OutView) const
{
	if (BuildCameraComponentView(ViewTarget, OutView))
	{
		return true;
	}

	if (FallbackCamera)
	{
		OutView.Location = FallbackCamera->GetLocation();
		OutView.Rotation = FallbackCamera->GetRotation();
		OutView.FOV = FallbackCamera->GetFOV();
		OutView.AspectRatio = FallbackCamera->GetAspectRatio();
		OutView.NearPlane = FallbackCamera->GetNearPlane();
		OutView.FarPlane = FallbackCamera->GetFarPlane();
		OutView.OrthoWidth = FallbackCamera->GetOrthoHeight() * FallbackCamera->GetAspectRatio();
		OutView.OrthoHeight = FallbackCamera->GetOrthoHeight();
		OutView.bOrthographic = FallbackCamera->IsOrthographic();
		return true;
	}

	return false;
}

void APlayerCameraManager::UpdateCameraFade(float DeltaTime)
{
	if (!FadeState.bActive)
	{
		return;
	}

	FadeState.Elapsed += DeltaTime;
	const float Alpha = FadeState.Duration > 0.0f ? MathUtil::Clamp(FadeState.Elapsed / FadeState.Duration, 0.0f, 1.0f) : 1.0f;
	FadeState.CurrentAlpha = MathUtil::Lerp(FadeState.FromAlpha, FadeState.ToAlpha, Alpha);

	if (Alpha >= 1.0f)
	{
		FadeState.CurrentAlpha = FadeState.ToAlpha;
		FadeState.bActive = false;

		if (!FadeState.bHoldWhenFinished)
		{
			FadeState.CurrentAlpha = 0.0f;
		}
	}
}

void APlayerCameraManager::ApplyCameraModifiers(float DeltaTime, FCameraViewInfo& InOutView)
{
	for (UCameraModifier* Modifier : ModifierList)
	{
		if (Modifier == nullptr || !Modifier->IsEnabled())
		{
			continue;
		}

		Modifier->ModifyCamera(DeltaTime, InOutView);
	}
}

void APlayerCameraManager::ApplyPostProcessModifiers(float DeltaTime, FPostProcessSettings& InOutSettings)
{
	for (UCameraModifier* Modifier : ModifierList)
	{
		if (Modifier == nullptr || !Modifier->IsEnabled())
		{
			continue;
		}

		Modifier->ModifyPostProcess(DeltaTime, InOutSettings);
	}
}

void APlayerCameraManager::ApplyOverlayModifiers(float DeltaTime, FCameraOverlaySettings& InOutOverlay)
{
	for (UCameraModifier* Modifier : ModifierList)
	{
		if (Modifier == nullptr || !Modifier->IsEnabled()) continue;
		Modifier->ModifyOverlay(DeltaTime, InOutOverlay);
	}
}

void APlayerCameraManager::ApplyCameraFade(FCameraOverlaySettings& InOutOverlay) const
{
	InOutOverlay.FadeColor = FVector4(FadeState.Color, MathUtil::Clamp(FadeState.CurrentAlpha, 0.0f, 1.0f));
}

// UCameraComponent의 Owner Actor를 찾은 뒤 Owner에게 PostProcessComponent가 있다면 적용합니다.
// 추후 Volume이 있는 UPostProcessComponent를 추가한다면 변경합니다.
void APlayerCameraManager::ApplyPostProcessComponent(FPostProcessSettings& InOutSettings)
{
	if (ViewTarget == nullptr)
	{
		return;
	}

	AActor* OwnerActor = ViewTarget->GetOwner();
	if (OwnerActor == nullptr)
	{
		return;
	}

	for (UActorComponent* Component : OwnerActor->GetComponents())
	{
		UPostProcessComponent* PostProcess = Cast<UPostProcessComponent>(Component);
		if (PostProcess == nullptr || !PostProcess->IsActive())
		{
			continue;
		}

		if (PostProcess->IsEnableVignette())
		{
			InOutSettings.VignetteIntensity = PostProcess->GetVignetteIntensity();
			InOutSettings.VignetteRadius = PostProcess->GetVignetteRadius();
			InOutSettings.VignetteSoftness = PostProcess->GetVignetteSoftness();
		}

		if (PostProcess->IsEnableGammaCorrection())
		{
			InOutSettings.Gamma = PostProcess->GetGamma();
		}
		
		break;
	}
}

void APlayerCameraManager::FillSceneView(FSceneView& OutView, const FCameraViewInfo& CameraView, const FViewportRect& ViewRect, EViewMode ViewMode) const
{
	const FVector Forward = CameraView.GetForwardVector().GetSafeNormal();
	const FVector Right = CameraView.GetRightVector().GetSafeNormal();
	const FVector Up = CameraView.GetUpVector().GetSafeNormal();

	OutView.View = FMatrix::MakeViewLookAtLH(CameraView.Location, CameraView.Location + Forward, Up);
	if (CameraView.bOrthographic)
	{
		OutView.Proj = FMatrix::MakeOrthographicLH(
			CameraView.OrthoWidth,
			CameraView.OrthoHeight,
			CameraView.NearPlane,
			CameraView.FarPlane);
	}
	else
	{
		OutView.Proj = FMatrix::MakePerspectiveFovLH(
			CameraView.FOV,
			CameraView.AspectRatio,
			CameraView.NearPlane,
			CameraView.FarPlane);
	}

	OutView.CameraPosition = CameraView.Location;
	OutView.CameraForward = Forward;
	OutView.CameraRight = Right;
	OutView.CameraUp = Up;
	OutView.NearPlane = CameraView.NearPlane;
	OutView.FarPlane = CameraView.FarPlane;
	OutView.bOrthographic = CameraView.bOrthographic;
	OutView.CameraOrthoHeight = CameraView.OrthoHeight;
	OutView.CameraFrustum.UpdateFromCamera(OutView.View, OutView.Proj);
	OutView.ViewRect = ViewRect;
	OutView.ViewMode = ViewMode;
	OutView.PostProcessSettings = CachedPostProcessSettings;
	OutView.CameraOverlaySettings = CachedCameraOverlaySettings;
}
