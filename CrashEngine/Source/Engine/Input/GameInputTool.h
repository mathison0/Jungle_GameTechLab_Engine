#pragma once

#include "Input/GameViewportTool.h"
#include "Input/GameInput.h"

class FGameInputTool : public FGameViewportTool
{
public:
    using FGameViewportTool::FGameViewportTool;

    bool InputKey(const FViewportKeyEvent& Event) override
    {
        bool bPressed = (Event.Type == EKeyEventType::Pressed);
        bool bReleased = (Event.Type == EKeyEventType::Released);
        bool bDown = (Event.Type == EKeyEventType::Pressed || Event.Type == EKeyEventType::Repeat);
        
        if (Event.Type == EKeyEventType::Released) bDown = false;

        GameInput::UpdateKeyState(Event.Key, bDown, bPressed, bReleased);
        return false; // Allow other tools to see it
    }

    void BeginInputFrame() override
    {
        GameInput::ResetFrameState();
    }
};
