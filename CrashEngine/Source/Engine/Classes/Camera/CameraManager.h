#pragma once
#include "Core/CoreTypes.h"
#include "Object/ObjectFactory.h"
#include "GameFramework/AActor.h"
#include "Math/MathUtils.h"
#include "Math/Rotator.h"
#include "Math/Vector.h"
#include <memory>


class UCameraModifier;
class UClass;


enum EViewTargetBlendFunction
{
    Linear,
    Cubic,
    EaseIn,
    EaseOut,
    EaseInOut
};

struct FMinimalViewInfo
{
    FVector Location = FVector(0, 0, 0);
    FRotator Rotation = FRotator();
    float FOV = 90.0f;
    float AspectRatio = 16.0f / 9.0f;
    float NearZ = 0.1f;
    float FarZ = 1000.0f;

    bool bConstrainAspectRatio = false;
    bool bOrthographic = false;
    float OrthoWidth = 10.0f;

    void BlendViewInfo(const FMinimalViewInfo& OtherInfo, float OtherWeight)
    {
        Location = FMath::Lerp(Location, OtherInfo.Location, OtherWeight);

        const FRotator DeltaAng = (OtherInfo.Rotation - Rotation).GetNormalized();
        Rotation = Rotation + DeltaAng * OtherWeight;

        FOV = FMath::Lerp(FOV, OtherInfo.FOV, OtherWeight);

        AspectRatio = FMath::Lerp(AspectRatio, OtherInfo.AspectRatio, OtherWeight);
        bConstrainAspectRatio |= OtherInfo.bConstrainAspectRatio;
    }
};

struct FViewTargetTransitionParams
{
    float BlendTime = 0.0f;
    EViewTargetBlendFunction BlendFunction = EViewTargetBlendFunction::Cubic;
    float BlendExp = 2.0f;
    bool bLockOutgoing = false;

    float GetBlendAlpha(float TimePct) const;
};

struct FCameraCacheEntry
{
    float TimeStamp = 0.0f;
    FMinimalViewInfo POV;
};

struct FViewTarget
{
    AActor* Target = nullptr;
    FMinimalViewInfo POV;

    bool IsValid() const { return Target != nullptr; }

    void CheckViewTarget(AActor* Actor)
    {
        if (Target)
        {
            return;
        }

        Target = Actor ? Actor : nullptr;
    }

    void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
    {
        (void)DeltaTime;

        if (!Target)
        {
            return;
        }

        Target->CalcCamera(OutResult.Location, OutResult.Rotation);
    }
};

class APlayerCameraManager : public AActor
{
public:
    DECLARE_CLASS(APlayerCameraManager, AActor)

public:
    // void InitializeFor(APlayerController* InOwner);
    ~APlayerCameraManager() override;

    void InitializeFor(AActor* InOwner);


    void UpdateCamera(float DeltaTime);

    void SetViewTarget(AActor* NewTarget, const FViewTargetTransitionParams& Params = {});
    AActor* GetViewTarget() const;

    virtual const FMinimalViewInfo& GetCameraCacheView() const;
    virtual const FMinimalViewInfo& GetLastFrameCameraCacheView() const;

    virtual void GetCameraViewPoint(FVector& OutLocation, FRotator& OutRotation) const;
    virtual FVector GetCameraLocation() const;
    virtual FRotator GetCameraRotation() const;

    virtual void SetGameCameraCutThisFrame() { bGameCameraCutThisFrame = true; }

    UCameraModifier* AddCameraModifier(UClass* ModifierUClass);
    UCameraModifier* AddCameraModifier(const char* ModifierName);
    bool AddCameraModifierToList(UCameraModifier* NewModifier);
    bool RemoveCameraModifier(UCameraModifier* Modifier);

private:
    void DoUpdateCamera(float DeltaTime);
    // void UpdateCameraFade(float DeltaTime);
    void UpdateViewTarget(FViewTarget& OutVT, float DeltaTime);
    // FMinimalViewInfo BlendViewTargets(const FViewTarget& A, const FViewTarget& B, float Alpha);
    void ApplyCameraModifiers(float DeltaTime, FMinimalViewInfo& InOutPOV);
    void SetCameraCachePOV(const FMinimalViewInfo& InPOV);
    void SetLastFrameCameraCachePOV(const FMinimalViewInfo& InPOV);
    void FillCameraCache(const FMinimalViewInfo& NewInfo);
    void AssignViewTarget(AActor* NewTarget, FViewTarget& OutViewTarget, const FViewTargetTransitionParams& Params);

private:
    // APlayerController* PCOwner = nullptr;
    AActor* PCOwner = nullptr;

    FViewTarget ViewTarget;
    FViewTarget PendingViewTarget;

    float BlendTimeToGo = 0.0f;
    FViewTargetTransitionParams BlendParams;

    float DefaultAspectRatio = 16.0f / 9.0f;
    bool bDefaultConstrainAspectRatio = false;
    float DefaultFOV = 90.0f;

    bool bGameCameraCutThisFrame = false;

    FCameraCacheEntry CameraCachePrivate;
    FCameraCacheEntry LastFrameCameraCachePrivate;

    TArray<UCameraModifier*> Modifiers;
};
