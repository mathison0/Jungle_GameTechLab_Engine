#include "Input/GameInput.h"

GameInput::FState GameInput::CurrentState = {};

bool GameInput::GetKey(int vk)
{
    if (vk < 0 || vk >= 256) return false;
    return CurrentState.KeyDown[vk];
}

bool GameInput::GetKeyDown(int vk)
{
    if (vk < 0 || vk >= 256) return false;
    return CurrentState.KeyPressed[vk];
}

bool GameInput::GetKeyUp(int vk)
{
    if (vk < 0 || vk >= 256) return false;
    return CurrentState.KeyReleased[vk];
}

float GameInput::GetAxis(const std::string& axisName)
{
    if (axisName == "Horizontal")
    {
        float val = 0.0f;
        if (GetKey('D') || GetKey(VK_RIGHT)) val += 1.0f;
        if (GetKey('A') || GetKey(VK_LEFT)) val -= 1.0f;
        return val;
    }
    else if (axisName == "Vertical")
    {
        float val = 0.0f;
        if (GetKey('W') || GetKey(VK_UP)) val += 1.0f;
        if (GetKey('S') || GetKey(VK_DOWN)) val -= 1.0f;
        return val;
    }
    return 0.0f;
}

void GameInput::UpdateKeyState(int vk, bool bDown, bool bPressed, bool bReleased)
{
    if (vk < 0 || vk >= 256) return;
    CurrentState.KeyDown[vk] = bDown;
    if (bPressed) CurrentState.KeyPressed[vk] = true;
    if (bReleased) CurrentState.KeyReleased[vk] = true;
}

void GameInput::ResetFrameState()
{
    for (int i = 0; i < 256; ++i)
    {
        CurrentState.KeyPressed[i] = false;
        CurrentState.KeyReleased[i] = false;
    }
}
