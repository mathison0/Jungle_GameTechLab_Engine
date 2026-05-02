// 뷰포트 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Core/Logging/LogMacros.h"
#include "Engine/Runtime/Engine.h"
#include "Object/Object.h"
#include "Viewport/ViewportClient.h"
#include "Input/GameViewportInputController.h"

class FViewport;
class FOverlayStatSystem;

// UGameViewportClient는 게임 월드와 실제 뷰포트 출력을 연결합니다.
class UGameViewportClient : public UObject, public FViewportClient
{
public:
    DECLARE_CLASS(UGameViewportClient, UObject)

    UGameViewportClient();
    ~UGameViewportClient() override;

    // FViewportClient 인터페이스입니다.
    void Draw(FViewport* Viewport, float DeltaTime) override {}
    void BeginInputFrame() override;
    void Tick(float DeltaTime) override;

    class UCameraComponent* GetCamera() const override;

    bool InputKey(const FViewportKeyEvent& Event) override
    {
        return InputController && InputController->InputKey(Event);
    }

    bool InputAxis(const FViewportAxisEvent& Event) override
    {
        return InputController && InputController->InputAxis(Event);
    }

    bool InputPointer(const FViewportPointerEvent& Event) override
    {
        return InputController && InputController->InputPointer(Event);
    }

    void ResetInputState() override
    {
        if (InputController)
        {
            InputController->BeginInputFrame();
        }
    }

    // 렌더링 대상 뷰포트를 설정합니다.
    void SetViewport(FViewport* InViewport) { Viewport = InViewport; }
    FViewport* GetViewport() const override { return Viewport; }

    void SetFallbackCamera(class UCameraComponent* InCamera) { FallbackCamera = InCamera; }
    void SetOverlayStatSystem(FOverlayStatSystem* InOverlayStatSystem) { OverlayStatSystem = InOverlayStatSystem; }

private:
    FViewport* Viewport = nullptr;
    std::unique_ptr<FGameViewportInputController> InputController;

    class UCameraComponent* FallbackCamera = nullptr;
    FOverlayStatSystem* OverlayStatSystem = nullptr;
};
