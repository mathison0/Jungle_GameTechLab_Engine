#pragma once

#include "Input/InputTypes.h"
#include <string>

// Unity 스타일의 입력 관리자
// GameInputTool까지 Unreal 스타일로 입력을 처리하다가 여기서 Unity 스타일로 래핑
struct FGameInput {
    static bool GetKey(int vk);
    static bool GetKeyDown(int vk);
    static bool GetKeyUp(int vk);
    static float GetAxis(const std::string& axisName);

    // Internal use by FGameInputTool
    static void UpdateKeyState(int vk, bool bDown, bool bPressed, bool bReleased);
    static void ResetFrameState();

    struct FState {
        bool KeyDown[256] = {};
        bool KeyPressed[256] = {};
        bool KeyReleased[256] = {};
    };

    static FState CurrentState;
};
