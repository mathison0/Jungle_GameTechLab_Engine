#include "CameraManager.h"
#include "Object/Object.h"
#include "GameFramework/World.h"
#include "CameraModifier.h"
#include "GameFramework/AActor.h"

#include <algorithm>
#include <cmath>

IMPLEMENT_CLASS(APlayerCameraManager, AActor)

float FViewTargetTransitionParams::GetBlendAlpha(float TimePct) const
{
    const float ClampedPct = FMath::Clamp(TimePct, 0.0f, 1.0f);
    const float SafeBlendExp = std::max(BlendExp, 0.01f);

    switch (BlendFunction)
    {
    case Linear:
        return ClampedPct;
    case Cubic:
        return (3.0f * ClampedPct * ClampedPct) - (2.0f * ClampedPct * ClampedPct * ClampedPct);
    case EaseIn:
        return std::pow(ClampedPct, SafeBlendExp);
    case EaseOut:
        return 1.0f - std::pow(1.0f - ClampedPct, SafeBlendExp);
    case EaseInOut:
        if (ClampedPct < 0.5f)
        {
            return 0.5f * std::pow(2.0f * ClampedPct, SafeBlendExp);
        }
        return 1.0f - (0.5f * std::pow(2.0f * (1.0f - ClampedPct), SafeBlendExp));
    default:
        return ClampedPct;
    }
}

APlayerCameraManager::~APlayerCameraManager()
{
    for (UCameraModifier*& Modifier : Modifiers)
    {
        if (Modifier)
        {
            Modifier->AddedToCamera(nullptr);
            UObjectManager::Get().DestroyObject(Modifier);
            Modifier = nullptr;
        }
    }

    Modifiers.clear();
    ModifierHandleMap.clear();
}

// void APlayerCameraManager::InitializeFor(APlayerController* PC)
void APlayerCameraManager::InitializeFor(AActor* PC)
{
    FMinimalViewInfo DefaultFOVCache = GetCameraCacheView();
    DefaultFOVCache.FOV = DefaultFOV;
    SetCameraCachePOV(DefaultFOVCache);

    PCOwner = PC;

    SetViewTarget(PC);

    // set the level default scale
    // SetDesiredColorScale(GetWorldSettings()->DefaultColorScale, 5.f);

    // Force camera update so it doesn't sit at (0,0,0) for a full tick.
    // This can have side effects with streaming.
    UpdateCamera(0.f);
}

void APlayerCameraManager::UpdateCamera(float DeltaTime)
{
    check(PCOwner != nullptr);

    DoUpdateCamera(DeltaTime);
}

void APlayerCameraManager::AssignViewTarget(AActor* NewTarget, FViewTarget& OutViewTarget, const FViewTargetTransitionParams& Params)
{
    if (!NewTarget)
    {
        return;
    }

    // Skip assigning to the same target unless we have a pending view target that's bLockOutgoing
    if (NewTarget == OutViewTarget.Target && !(PendingViewTarget.Target && BlendParams.bLockOutgoing))
    {
        return;
    }

    OutViewTarget.Target = NewTarget;

    // Use default FOV and aspect ratio.
    OutViewTarget.POV.AspectRatio = DefaultAspectRatio;
    OutViewTarget.POV.bConstrainAspectRatio = bDefaultConstrainAspectRatio;
    OutViewTarget.POV.FOV = DefaultFOV;
}

void APlayerCameraManager::SetViewTarget(AActor* NewTarget, const FViewTargetTransitionParams& Params)
{
    // 1. Target이 없으면 기본 타겟으로 보정
    if (NewTarget == nullptr)
    {
        NewTarget = PCOwner;
    }

    if (NewTarget == nullptr)
    {
        return;
    }

    // 최초 ViewTarget 지정
    if (ViewTarget.Target == nullptr)
    {
        AssignViewTarget(NewTarget, ViewTarget, Params);
        ViewTarget.CheckViewTarget(PCOwner);

        PendingViewTarget = {};
        BlendTimeToGo = 0.0f;
        BlendParams = Params;
        return;
    }

    // 2. 현재 ViewTarget 유효성 보정
    ViewTarget.CheckViewTarget(PCOwner);

    if (PendingViewTarget.Target != nullptr)
    {
        PendingViewTarget.CheckViewTarget(PCOwner);
    }

    // 3. 이미 같은 대상으로 블렌딩 중이면 무시
    if (PendingViewTarget.Target != nullptr &&
        NewTarget == PendingViewTarget.Target)
    {
        return;
    }

    // 4. 새 타겟으로 바꿔야 하는 경우
    const bool bDifferentTarget = (NewTarget != ViewTarget.Target);
    const bool bForceRestartBlend =
        (PendingViewTarget.Target != nullptr && BlendParams.bLockOutgoing);

    if (bDifferentTarget || bForceRestartBlend)
    {
        if (Params.BlendTime > 0.0f)
        {
            // 기존 Pending이 없으면 현재 타겟을 pending의 이전 기준으로 보존
            if (PendingViewTarget.Target == nullptr)
            {
                PendingViewTarget.Target = ViewTarget.Target;
                PendingViewTarget.POV = ViewTarget.POV;
            }

            // outgoing view를 지난 프레임 POV로 고정해서 blend 기준으로 사용
            ViewTarget.POV = GetLastFrameCameraCacheView();

            BlendTimeToGo = Params.BlendTime;

            AssignViewTarget(NewTarget, PendingViewTarget, Params);
            PendingViewTarget.CheckViewTarget(PCOwner);
        }
        else
        {
            // 즉시 전환
            AssignViewTarget(NewTarget, ViewTarget, Params);
            ViewTarget.CheckViewTarget(PCOwner);

            // 남아있는 블렌딩 취소
            PendingViewTarget = {};
            BlendTimeToGo = 0.0f;
        }
    }
    else
    {
        // 현재 타겟으로 다시 SetViewTarget한 경우:
        // 진행 중이던 Pending 전환만 취소
        PendingViewTarget = {};
        BlendTimeToGo = 0.0f;
    }

    // 5. 마지막에 갱신
    BlendParams = Params;
}

AActor* APlayerCameraManager::GetViewTarget() const
{
    // to handle verification/caching behavior while preserving constness upstream
    APlayerCameraManager* const NonConstThis = const_cast<APlayerCameraManager*>(this);

    // if blending to another view target, return this one first
    if (PendingViewTarget.Target)
    {
        NonConstThis->PendingViewTarget.CheckViewTarget(NonConstThis->PCOwner);
        if (PendingViewTarget.Target)
        {
            return PendingViewTarget.Target;
        }
    }

    NonConstThis->ViewTarget.CheckViewTarget(NonConstThis->PCOwner);
    return ViewTarget.Target;
}

const FMinimalViewInfo& APlayerCameraManager::GetCameraCacheView() const
{
    return CameraCachePrivate.POV;
}

const FMinimalViewInfo& APlayerCameraManager::GetLastFrameCameraCacheView() const
{
    return LastFrameCameraCachePrivate.POV;
}

void APlayerCameraManager::GetCameraViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
    const FMinimalViewInfo& CurrentPOV = GetCameraCacheView();
    OutLocation = CurrentPOV.Location;
    OutRotation = CurrentPOV.Rotation;
}

FRotator APlayerCameraManager::GetCameraRotation() const
{
    return GetCameraCacheView().Rotation;
}

FVector APlayerCameraManager::GetCameraLocation() const
{
    return GetCameraCacheView().Location;
}

uint32 APlayerCameraManager::AddCameraModifier(UClass* ModifierUClass)
{
    if (ModifierUClass == nullptr)
    {
        return 0;
    }

    uint32 Result = AddCameraModifier(ModifierUClass->GetName());
    return Result;
}

uint32 APlayerCameraManager::AddCameraModifier(const char* ModifierName)
{
    UObject* NewObject = FObjectFactory::Get().Create(ModifierName, this);
    if (NewObject == nullptr)
    {
        return 0;
    }

    UCameraModifier* NewModifier = Cast<UCameraModifier>(NewObject);
    if (NewModifier == nullptr)
    {
        UObjectManager::Get().DestroyObject(NewObject);
        return 0;
    }

    UCameraModifier* Result = NewModifier;

    // 필요하면 여기서 초기화
    // Result->Initialize(this);

    uint32 Handle = AddCameraModifierToList(Result);
    if (Handle == 0)
    {
        UObjectManager::Get().DestroyObject(NewModifier);
        return 0;
    }

    return Handle;
}

uint32 APlayerCameraManager::AddCameraModifierToList(UCameraModifier* NewModifier)
{
    uint32 Handle = 0;
    if (NewModifier)
    {
        // Look through current modifier list and find slot for this priority
        int32 BestIdx = static_cast<int32>(Modifiers.size());
        for (int32 ModifierIdx = 0; ModifierIdx < Modifiers.size(); ModifierIdx++)
        {
            UCameraModifier* const M = Modifiers[ModifierIdx];
            if (M)
            {
                if (M == NewModifier)
                {
                    // already in list, just bail
                    return Handle;
                }

                // If priority of current index has passed or equaled ours - we have the insert location
                if (NewModifier->Priority <= M->Priority)
                {
                    // Update best index
                    BestIdx = ModifierIdx;

                    break;
                }
            }
        }
        Modifiers.insert(Modifiers.begin() + BestIdx, NewModifier);
        Handle = NextModifierHandle++;
        ModifierHandleMap[Handle] = NewModifier;
        // Save camera
        NewModifier->AddedToCamera(this);
        return Handle;
    }

    return Handle;
}

uint32 APlayerCameraManager::GetModifierHandle(const char* ModifierUclassName) const
{
    for (const auto& Pair : ModifierHandleMap)
    {
        if (Pair.second->GetClass()->GetName() == ModifierUclassName)
        {
            return Pair.first;
        }
    }
    return 0;
}

bool APlayerCameraManager::DisableCameraModifier(uint32 ModifierHandle, bool bImmediate)
{
    auto It = ModifierHandleMap.find(ModifierHandle);
    if (It == ModifierHandleMap.end() || It->second == nullptr)
    {
        return false;
    }

    It->second->DisableModifier(bImmediate);
    return true;
}

bool APlayerCameraManager::RemoveCameraModifier(uint32 ModifierHandle)
{
    if (ModifierHandle == 0)
    {
        return false;
    }

    auto MapIt = ModifierHandleMap.find(ModifierHandle);
    if (MapIt == ModifierHandleMap.end() || MapIt->second == nullptr)
    {
        return false;
    }

    UCameraModifier* Modifier = MapIt->second;

    auto It = std::find(
        Modifiers.begin(),
        Modifiers.end(),
        Modifier);

    if (It == Modifiers.end())
    {
        ModifierHandleMap.erase(MapIt);
        return false;
    }

    (*It)->AddedToCamera(nullptr);
    UObjectManager::Get().DestroyObject(*It);

    Modifiers.erase(It);
    ModifierHandleMap.erase(MapIt);

    return true;
}

void APlayerCameraManager::Tick(float DeltaTime)
{
    AActor::Tick(DeltaTime);
    UpdateCamera(DeltaTime);
}

void APlayerCameraManager::DoUpdateCamera(float DeltaTime)
{
    // 1. 현재 ViewTarget 갱신
    // bLockOutgoing이면 블렌딩 시작 시점의 outgoing POV를 고정해야 하므로 갱신하지 않는다.
    if (PendingViewTarget.Target == nullptr || !BlendParams.bLockOutgoing)
    {
        ViewTarget.CheckViewTarget(PCOwner);
        UpdateViewTarget(ViewTarget, DeltaTime);
    }

    FMinimalViewInfo NewPOV = ViewTarget.POV;

    // 2. PendingViewTarget이 있으면 블렌딩 처리
    if (PendingViewTarget.Target != nullptr)
    {
        BlendTimeToGo -= DeltaTime;

        PendingViewTarget.CheckViewTarget(PCOwner);
        UpdateViewTarget(PendingViewTarget, DeltaTime);

        if (BlendTimeToGo > 0.0f)
        {
            const float DurationPct =
                (BlendParams.BlendTime - BlendTimeToGo) / BlendParams.BlendTime;

            const float BlendPct = BlendParams.GetBlendAlpha(DurationPct);

            NewPOV = ViewTarget.POV;
            NewPOV.BlendViewInfo(PendingViewTarget.POV, BlendPct);
        }
        else
        {
            // 3. 블렌딩 완료
            ViewTarget = PendingViewTarget;
            PendingViewTarget = {};

            BlendTimeToGo = 0.0f;
            NewPOV = ViewTarget.POV;

            // OnBlendComplete();
        }
    }

    // 4. 카메라 모디파이어 적용
    ApplyCameraModifiers(DeltaTime, NewPOV);

    // 5. 페이드 갱신
    // UpdateCameraFade(DeltaTime);

    // 6. 최종 카메라 캐시
    FillCameraCache(NewPOV);
}

void APlayerCameraManager::UpdateViewTarget(FViewTarget& OutVT, float DeltaTime)
{
    if (OutVT.Target)
    {
        OutVT.CalcCamera(DeltaTime, OutVT.POV);
    }
}

void APlayerCameraManager::ApplyCameraModifiers(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
    for (int i = 0; i < Modifiers.size(); ++i)
    {
        UCameraModifier* CameraModifier = Modifiers[i];
        if (CameraModifier != nullptr && !CameraModifier->IsDisabled())
        {
            bool bStop = CameraModifier->ModifyCamera(DeltaTime, InOutPOV);
            if (bStop)
            {
                break;
            }
        }
    }
}

void APlayerCameraManager::SetCameraCachePOV(const FMinimalViewInfo& InPOV)
{
    CameraCachePrivate.POV = InPOV;
}

void APlayerCameraManager::SetLastFrameCameraCachePOV(const FMinimalViewInfo& InPOV)
{
    LastFrameCameraCachePrivate.POV = InPOV;
}

void APlayerCameraManager::FillCameraCache(const FMinimalViewInfo& NewInfo)
{
    // const float CurrentCacheTime = GetCameraCacheTime();
    // const float CurrentGameTime = GetWorld()->TimeSeconds;
    // if (CurrentCacheTime != CurrentGameTime)
    //{
    //     SetLastFrameCameraCachePOV(GetCameraCacheView());
    //     SetLastFrameCameraCacheTime(CurrentCacheTime);
    // }

    // SetCameraCachePOV(NewInfo);
    // SetCameraCacheTime(CurrentGameTime);

    SetLastFrameCameraCachePOV(GetCameraCacheView());
    SetCameraCachePOV(NewInfo);
}

// void APlayerCameraManager::UpdateCameraFade(float DeltaTime)
//{
//     if (!bEnableFading)
//     {
//         return;
//     }
//
//     if (!bAutoAnimateFade)
//     {
//         return;
//     }
//
//     FadeTimeRemaining = FMath::Max(FadeTimeRemaining - DeltaTime, 0.0f);
//
//     if (FadeTime > 0.0f)
//     {
//         const float Alpha = 1.0f - FadeTimeRemaining / FadeTime;
//         FadeAmount = FMath::Lerp(FadeAlpha.X, FadeAlpha.Y, Alpha);
//     }
//     else
//     {
//         FadeAmount = FadeAlpha.Y;
//     }
//
//     if (!bHoldFadeWhenFinished && FadeTimeRemaining <= 0.0f)
//     {
//         StopCameraFade();
//     }
// }
