#include "Engine/Camera/PlayerCameraManager.h"

#include "Component/CameraComponent.h"
#include "Engine/Camera/CameraModifier.h"
#include "Engine/Runtime/SceneView.h"
#include "Engine/Viewport/ViewportCamera.h"

#include <algorithm>

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
} // namespace

void APlayerCameraManager::SetViewTarget(UCameraComponent* InCamera)
{
    ViewTarget = InCamera;
}

void APlayerCameraManager::SetViewTargetWithBlend(UCameraComponent* InCamera, float BlendTime)
{
    (void)BlendTime;
    SetViewTarget(InCamera);
}

void APlayerCameraManager::SetFallbackCamera(FViewportCamera* InCamera)
{
    FallbackCamera = InCamera;
}

void APlayerCameraManager::UpdateCamera(float DeltaTime)
{
    FCameraViewInfo NewView;
    if (!BuildBaseCameraView(NewView))
    {
        bHasCachedCameraView = false;
        return;
    }

    ApplyCameraModifiers(DeltaTime, NewView);
    CachedCameraView = NewView;
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

void APlayerCameraManager::AddCameraModifier(UCameraModifier* Modifier)
{
    if (Modifier == nullptr)
    {
        return;
    }

    if (std::find(CameraModifiers.begin(), CameraModifiers.end(), Modifier) != CameraModifiers.end())
    {
        return;
    }

    CameraModifiers.push_back(Modifier);
    std::sort(CameraModifiers.begin(), CameraModifiers.end(), [](const UCameraModifier* A, const UCameraModifier* B)
              {
		const int32 APriority = A ? A->GetPriority() : 0;
		const int32 BPriority = B ? B->GetPriority() : 0;
		return APriority < BPriority; });
}

void APlayerCameraManager::RemoveCameraModifier(UCameraModifier* Modifier)
{
    CameraModifiers.erase(std::remove(CameraModifiers.begin(), CameraModifiers.end(), Modifier), CameraModifiers.end());
}

void APlayerCameraManager::ClearCameraModifiers()
{
    CameraModifiers.clear();
}

void APlayerCameraManager::StartCameraTransition(const FCameraViewInfo& FromView, const FCameraViewInfo& ToView, float Duration)
{
    (void)FromView;
    (void)Duration;
    CachedCameraView = ToView;
    bHasCachedCameraView = true;
}

bool APlayerCameraManager::BuildBaseCameraView(FCameraViewInfo& OutView) const
{
    if (ViewTarget)
    {
        const float Height = ViewTarget->GetHeight();

        OutView.Location = ViewTarget->GetWorldLocation(); 
        OutView.Rotation = MakeCameraRotation(
            ViewTarget->GetForwardVector(),
            ViewTarget->GetRightVector(),
            ViewTarget->GetUpVector());  
        OutView.FOV = ViewTarget->GetFOV();
        OutView.AspectRatio = Height > 0.0f ? ViewTarget->GetWidth() / Height : 16.0f / 9.0f;
        OutView.NearPlane = ViewTarget->GetNearPlane();
        OutView.FarPlane = ViewTarget->GetFarPlane();
        OutView.OrthoWidth = ViewTarget->GetOrthoWidth();
        OutView.OrthoHeight = OutView.AspectRatio > 0.0f ? OutView.OrthoWidth / OutView.AspectRatio : OutView.OrthoWidth;
        OutView.bOrthographic = ViewTarget->IsOrthogonal();
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

void APlayerCameraManager::ApplyCameraModifiers(float DeltaTime, FCameraViewInfo& InOutView)
{
    for (UCameraModifier* Modifier : CameraModifiers)
    {
        if (Modifier == nullptr || !Modifier->IsEnabled())
        {
            continue;
        }

        Modifier->ModifyCamera(DeltaTime, InOutView);
    }
}

void APlayerCameraManager::FillSceneView(FSceneView& OutView, const FCameraViewInfo& CameraView, const FViewportRect& ViewRect, EViewMode ViewMode) const
{
    const FVector Forward = CameraView.GetForwardVector().GetSafeNormal();
    const FVector Right = CameraView.GetRightVector().GetSafeNormal();
    const FVector Up = CameraView.GetUpVector().GetSafeNormal();

    OutView.ViewMatrix = FMatrix::MakeViewLookAtLH(CameraView.Location, CameraView.Location + Forward, Up);
    if (CameraView.bOrthographic)
    {
        OutView.ProjectionMatrix = FMatrix::MakeOrthographicLH(
            CameraView.OrthoWidth,
            CameraView.OrthoHeight,
            CameraView.NearPlane,
            CameraView.FarPlane);
    }
    else
    {
        OutView.ProjectionMatrix = FMatrix::MakePerspectiveFovLH(
            CameraView.FOV,
            CameraView.AspectRatio,
            CameraView.NearPlane,
            CameraView.FarPlane);
    }

    OutView.ViewProjectionMatrix = OutView.ViewMatrix * OutView.ProjectionMatrix;
    OutView.CameraPosition = CameraView.Location;
    OutView.CameraForward = Forward;
    OutView.CameraRight = Right;
    OutView.CameraUp = Up;
    OutView.NearPlane = CameraView.NearPlane;
    OutView.FarPlane = CameraView.FarPlane;
    OutView.bOrthographic = CameraView.bOrthographic;
    OutView.CameraOrthoHeight = CameraView.OrthoHeight;
    OutView.CameraFrustum.UpdateFromCamera(OutView.ViewProjectionMatrix);
    OutView.ViewRect = ViewRect;
    OutView.ViewMode = ViewMode;
}
