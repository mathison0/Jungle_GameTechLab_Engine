// 뷰포트 영역의 세부 동작을 구현합니다.
#include "Viewport/GameViewportClient.h"
#include "Input/GameViewportInputController.h"
#include "Component/CameraComponent.h"
#include "Editor/Subsystem/OverlayStatSystem.h"
#include "GameFramework/World.h"
#include "Render/Scene/Proxies/UI/UIProxy.h"
#include "Render/Scene/Scene.h"
#include "UI/UIComponent.h"
#include "Viewport/Viewport.h"

#include <algorithm>

DEFINE_CLASS(UGameViewportClient, UObject)

UGameViewportClient::UGameViewportClient()
{
    InputController = std::make_unique<FGameViewportInputController>(this);
}

UGameViewportClient::~UGameViewportClient() = default;

void UGameViewportClient::BeginInputFrame()
{
    if (InputController)
    {
        InputController->BeginInputFrame();
    }
}

void UGameViewportClient::Tick(float DeltaTime)
{
    if (InputController)
    {
        InputController->HandleInput(DeltaTime);
    }

    if (OverlayStatSystem)
    {
        bool bNoCamera = (UCameraComponent::Main == nullptr);
        OverlayStatSystem->ShowNoCameraWarning(bNoCamera);
        if (bNoCamera)
        {
             // UE_LOG(GameViewportClient, Warning, "No Camera::Main found during Tick!");
        }
    }
}

UCameraComponent* UGameViewportClient::GetCamera() const
{
    if (UCameraComponent::Main)
    {
        return UCameraComponent::Main;
    }

    return FallbackCamera;
}

bool UGameViewportClient::RouteUIPointerEvent(const FViewportPointerEvent& Event)
{
    UWorld* World = GEngine ? GEngine->GetWorld() : nullptr;
    if (!World)
    {
        HoveredUIComponent = nullptr;
        CapturedUIComponent = nullptr;
        return false;
    }

    FScene& Scene = World->GetScene();
    Scene.UpdateDirtyUIProxies();

    if (HoveredUIComponent && !IsUIComponentRegistered(HoveredUIComponent))
    {
        HoveredUIComponent = nullptr;
    }
    if (CapturedUIComponent && !IsUIComponentRegistered(CapturedUIComponent))
    {
        CapturedUIComponent = nullptr;
    }

    const FVector2 ScreenPoint(
        static_cast<float>(Event.LocalPos.x),
        static_cast<float>(Event.LocalPos.y));
    const float ViewportWidth = Viewport ? static_cast<float>(Viewport->GetWidth()) : 0.0f;
    const float ViewportHeight = Viewport ? static_cast<float>(Viewport->GetHeight()) : 0.0f;

    UUIComponent* HitComponent = FindTopmostUIComponentAt(ScreenPoint, ViewportWidth, ViewportHeight);
    UpdateUIHover(HitComponent, Event);

    UUIComponent* TargetComponent = CapturedUIComponent ? CapturedUIComponent : HitComponent;

    if (Event.Type == EPointerEventType::Pressed &&
        Event.Button != EPointerButton::None &&
        HitComponent)
    {
        CapturedUIComponent = HitComponent;
        TargetComponent = HitComponent;
    }

    bool bHandled = false;
    if (TargetComponent)
    {
        bHandled = TargetComponent->HandleUIPointerEvent(Event);
    }

    if (Event.Type == EPointerEventType::Released &&
        Event.Button != EPointerButton::None)
    {
        CapturedUIComponent = nullptr;
    }

    return bHandled || HitComponent != nullptr || TargetComponent != nullptr;
}

UUIComponent* UGameViewportClient::FindTopmostUIComponentAt(const FVector2& ScreenPoint, float ViewportWidth, float ViewportHeight) const
{
    UWorld* World = GEngine ? GEngine->GetWorld() : nullptr;
    if (!World || ViewportWidth <= 0.0f || ViewportHeight <= 0.0f)
    {
        return nullptr;
    }

    const TArray<FUIProxy*>& UIProxies = World->GetScene().GetUIProxies();
    const FUIProxy* BestProxy = nullptr;

    auto IsHigher = [](const FUIProxy* A, const FUIProxy* B)
    {
        if (!B)
        {
            return true;
        }
        if (A->Layer != B->Layer)
        {
            return A->Layer > B->Layer;
        }
        if (A->ZOrder != B->ZOrder)
        {
            return A->ZOrder > B->ZOrder;
        }
        return A->ProxyId > B->ProxyId;
    };

    for (const FUIProxy* Proxy : UIProxies)
    {
        if (!Proxy ||
            !Proxy->Owner ||
            !Proxy->bVisible ||
            !Proxy->bHitTestVisible ||
            Proxy->RenderSpace != EUIRenderSpace::ScreenSpace ||
            Proxy->GeometryType != EUIGeometryType::Quad)
        {
            continue;
        }

        if (Proxy->Owner->HitTestScreenPoint(ScreenPoint, ViewportWidth, ViewportHeight) && IsHigher(Proxy, BestProxy))
        {
            BestProxy = Proxy;
        }
    }

    return BestProxy ? BestProxy->Owner : nullptr;
}

bool UGameViewportClient::IsUIComponentRegistered(const UUIComponent* Component) const
{
    if (!Component)
    {
        return false;
    }

    UWorld* World = GEngine ? GEngine->GetWorld() : nullptr;
    if (!World)
    {
        return false;
    }

    for (const FUIProxy* Proxy : World->GetScene().GetUIProxies())
    {
        if (Proxy && Proxy->Owner == Component)
        {
            return true;
        }
    }

    return false;
}

void UGameViewportClient::UpdateUIHover(UUIComponent* NewHovered, const FViewportPointerEvent& Event)
{
    if (HoveredUIComponent == NewHovered)
    {
        return;
    }

    if (HoveredUIComponent)
    {
        HoveredUIComponent->OnUIPointerLeave(Event);
    }

    HoveredUIComponent = NewHovered;

    if (HoveredUIComponent)
    {
        HoveredUIComponent->OnUIPointerEnter(Event);
    }
}
