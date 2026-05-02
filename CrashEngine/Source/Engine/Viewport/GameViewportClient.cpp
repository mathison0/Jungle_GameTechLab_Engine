// 뷰포트 영역의 세부 동작을 구현합니다.
#include "Viewport/GameViewportClient.h"
#include "Input/GameViewportInputController.h"
#include "Component/CameraComponent.h"
#include "Editor/Subsystem/OverlayStatSystem.h"

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
