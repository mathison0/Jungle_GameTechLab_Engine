// 뷰포트 영역의 세부 동작을 구현합니다.
#include "Viewport/GameViewportClient.h"
#include "Input/GameViewportInputController.h"

DEFINE_CLASS(UGameViewportClient, UObject)

UGameViewportClient::UGameViewportClient()
{
    InputController = std::make_unique<FGameViewportInputController>(this);
}

UGameViewportClient::~UGameViewportClient() = default;
