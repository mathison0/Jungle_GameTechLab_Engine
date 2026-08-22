#include "Engine/Input/InputSystem.h"
#include <cmath>
#include <utility>
#include <Xinput.h>
#pragma comment(lib, "Xinput.lib")

void InputSystem::Tick()
{
    // 윈도우 포커스가 없으면 모든 입력 상태 해제
    bWindowFocused = !OwnerHWnd || GetForegroundWindow() == OwnerHWnd;
    if (!bWindowFocused)
    {
        bGamepadConnected = false;
        GamepadLX = GamepadLY = GamepadRX = GamepadRY = 0.0f;
        ResetAllKeyStates();
        ResetTransientState();
        UpdateCurrentSnapshot();
        return;
    }

    for (int i = 0; i < 256; ++i)
    {
        PrevStates[i] = CurrentStates[i];
        CurrentStates[i] = (GetAsyncKeyState(i) & 0x8000) != 0;
    }

    // 게임패드 — 키보드 폴링 직후 버튼 상태를 CurrentStates 에 OR 주입 (PrevStates 는 위에서
    // 이미 보존됐으므로 GetKeyDown 엣지 감지가 게임패드에도 그대로 적용된다).
    PollGamepad();

    bLeftDragJustStarted = false;
    bRightDragJustStarted = false;
    bLeftDragJustEnded = false;
    bRightDragJustEnded = false;

    PrevScrollDelta = ScrollDelta;
    ScrollDelta = 0;

    PrevMousePos = MousePos;
    GetCursorPos(&MousePos);
    FrameMouseDeltaX = MousePos.x - PrevMousePos.x;
    FrameMouseDeltaY = MousePos.y - PrevMousePos.y;
    if (bUseRawMouse)
    {
        FrameMouseDeltaX = RawMouseDeltaAccumX;
        FrameMouseDeltaY = RawMouseDeltaAccumY;
    }
    RawMouseDeltaAccumX = 0;
    RawMouseDeltaAccumY = 0;

    if (GetKeyDown(VK_LBUTTON))
    {
        bLeftDragCandidate = true;
        LeftMouseDownPos = MousePos;
    }
    if (GetKeyDown(VK_RBUTTON))
    {
        bRightDragCandidate = true;
        RightMouseDownPos = MousePos;
    }

    // Left drag
    if (!bLeftDragging && IsDraggingLeft())
    {
        FilterDragThreshold(bLeftDragCandidate, bLeftDragging, bLeftDragJustStarted,
            LeftMouseDownPos, LeftDragStartPos);
    }
    else if (GetKeyUp(VK_LBUTTON))
    {
        if (bLeftDragging) bLeftDragJustEnded = true;
        bLeftDragging = false;
        bLeftDragCandidate = false;
    }

    // Right drag
    if (!bRightDragging && IsDraggingRight())
    {
        FilterDragThreshold(bRightDragCandidate, bRightDragging, bRightDragJustStarted,
            RightMouseDownPos, RightDragStartPos);
    }
    else if (GetKeyUp(VK_RBUTTON))
    {
        if (bRightDragging) bRightDragJustEnded = true;
        bRightDragging = false;
        bRightDragCandidate = false;
    }

    UpdateCurrentSnapshot();
}

void InputSystem::PollGamepad()
{
    XINPUT_STATE State;
    ZeroMemory(&State, sizeof(State));
    if (XInputGetState(0, &State) != ERROR_SUCCESS)
    {
        bGamepadConnected = false;
        GamepadLX = GamepadLY = GamepadRX = GamepadRY = 0.0f;
        return;
    }
    bGamepadConnected = true;
    const XINPUT_GAMEPAD& Pad = State.Gamepad;

    // 스틱 정규화 + 데드존 — 데드존 밖 구간을 0..1 로 재매핑.
    auto Normalize = [](SHORT V, SHORT Deadzone) -> float
    {
        const float F = static_cast<float>(V);
        const float D = static_cast<float>(Deadzone);
        if (F >  D) { return (F - D) / (32767.0f - D); }
        if (F < -D) { return (F + D) / (32767.0f - D); }
        return 0.0f;
    };
    GamepadLX = Normalize(Pad.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
    GamepadLY = Normalize(Pad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
    GamepadRX = Normalize(Pad.sThumbRX, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
    GamepadRY = Normalize(Pad.sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);

    const WORD B = Pad.wButtons;
    auto Inject = [this](int VK, bool bPressed) { if (bPressed) { CurrentStates[VK] = true; } };

    // 버튼 → 액션 VK 주입 (키보드/마우스와 OR — 둘 다 같은 동작).
    Inject(0x20, (B & XINPUT_GAMEPAD_A) != 0);              // A  → 점프(Space) / 메뉴 확인
    Inject(0x0D, (B & XINPUT_GAMEPAD_A) != 0);              // A  → Enter (메뉴 확인 보강)
    Inject(0x01, (B & XINPUT_GAMEPAD_X) != 0);              // X  → 약공격(LBUTTON)
    Inject(0x02, (B & XINPUT_GAMEPAD_Y) != 0);              // Y  → 강공격(RBUTTON)
    Inject(0x10, (B & XINPUT_GAMEPAD_B) != 0);              // B  → 구르기(Shift)
    Inject(0x52, (B & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0); // RB → 무쌍기(R)
    Inject(0x58, (B & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0);  // LB → 발도/납도(X키)
    Inject(0x1B, (B & XINPUT_GAMEPAD_START) != 0);          // Start → 일시정지(ESC)

    // D-pad → 메뉴 방향키 (인게임엔 미사용이라 무해).
    Inject(0x25, (B & XINPUT_GAMEPAD_DPAD_LEFT) != 0);      // ←
    Inject(0x27, (B & XINPUT_GAMEPAD_DPAD_RIGHT) != 0);     // →
    Inject(0x26, (B & XINPUT_GAMEPAD_DPAD_UP) != 0);        // ↑
    Inject(0x28, (B & XINPUT_GAMEPAD_DPAD_DOWN) != 0);      // ↓

    // 좌스틱 → WASD 이동 (디지털, 임계 0.5). 기존 이동/잠금/재조준 경로를 그대로 탄다.
    constexpr float MoveThreshold = 0.5f;
    Inject(0x57, GamepadLY >  MoveThreshold);   // W (전진)
    Inject(0x53, GamepadLY < -MoveThreshold);   // S (후진)
    Inject(0x44, GamepadLX >  MoveThreshold);   // D (우)
    Inject(0x41, GamepadLX < -MoveThreshold);   // A (좌)
}

FInputSystemSnapshot InputSystem::TickAndMakeSnapshot()
{
    Tick();
    return MakeSnapshot();
}

FInputSystemSnapshot InputSystem::MakeSnapshot() const
{
    return CurrentSnapshot;
}

void InputSystem::RefreshSnapshot()
{
    UpdateCurrentSnapshot();
}

void InputSystem::SetUseRawMouse(bool bEnable)
{
    if (bUseRawMouse == bEnable)
    {
        return;
    }

    bUseRawMouse = bEnable;
    ResetMouseDelta();
    UpdateCurrentSnapshot();
}

void InputSystem::AddRawMouseDelta(int DeltaX, int DeltaY)
{
    RawMouseDeltaAccumX += DeltaX;
    RawMouseDeltaAccumY += DeltaY;
}

void InputSystem::AddTextInputCharacter(char32_t Character)
{
    if (Character == U'\0')
    {
        return;
    }

    PendingTextInputCharacters.push_back(Character);
}

std::vector<char32_t> InputSystem::ConsumeTextInputCharacters()
{
    std::vector<char32_t> Result = std::move(PendingTextInputCharacters);
    PendingTextInputCharacters.clear();
    return Result;
}

void InputSystem::ResetTransientState()
{
    bLeftDragJustStarted = false;
    bRightDragJustStarted = false;
    bLeftDragJustEnded = false;
    bRightDragJustEnded = false;
    ResetDragState();
    ResetMouseDelta();
    ResetWheelDelta();
    PendingTextInputCharacters.clear();
    UpdateCurrentSnapshot();
}

void InputSystem::ResetAllKeyStates()
{
    for (int VK = 0; VK < 256; ++VK)
    {
        CurrentStates[VK] = false;
        PrevStates[VK] = false;
    }
    UpdateCurrentSnapshot();
}

void InputSystem::ResetMouseDelta()
{
    GetCursorPos(&MousePos);
    PrevMousePos = MousePos;
    FrameMouseDeltaX = 0;
    FrameMouseDeltaY = 0;
    RawMouseDeltaAccumX = 0;
    RawMouseDeltaAccumY = 0;
    UpdateCurrentSnapshot();
}

void InputSystem::ResetWheelDelta()
{
    ScrollDelta = 0;
    PrevScrollDelta = 0;
    UpdateCurrentSnapshot();
}

void InputSystem::ResetCaptureStateForPIEEnd()
{
    SetUseRawMouse(false);
    ResetAllKeyStates();
    ResetTransientState();
    GuiState.bUsingMouse = false;
    GuiState.bUsingKeyboard = false;
    GuiState.bUsingTextInput = false;
    PendingTextInputCharacters.clear();
    UpdateCurrentSnapshot();
}

void InputSystem::UpdateCurrentSnapshot()
{
    FInputSystemSnapshot Snapshot{};
    for (int VK = 0; VK < 256; ++VK)
    {
        Snapshot.KeyDown[VK] = CurrentStates[VK];
        Snapshot.KeyPressed[VK] = CurrentStates[VK] && !PrevStates[VK];
        Snapshot.KeyReleased[VK] = !CurrentStates[VK] && PrevStates[VK];
    }

    Snapshot.bLeftMouseDown = Snapshot.KeyDown[VK_LBUTTON];
    Snapshot.bLeftMousePressed = Snapshot.KeyPressed[VK_LBUTTON];
    Snapshot.bLeftMouseReleased = Snapshot.KeyReleased[VK_LBUTTON];
    Snapshot.bRightMouseDown = Snapshot.KeyDown[VK_RBUTTON];
    Snapshot.bRightMousePressed = Snapshot.KeyPressed[VK_RBUTTON];
    Snapshot.bRightMouseReleased = Snapshot.KeyReleased[VK_RBUTTON];
    Snapshot.bMiddleMouseDown = Snapshot.KeyDown[VK_MBUTTON];
    Snapshot.bMiddleMousePressed = Snapshot.KeyPressed[VK_MBUTTON];
    Snapshot.bMiddleMouseReleased = Snapshot.KeyReleased[VK_MBUTTON];
    Snapshot.bXButton1Down = Snapshot.KeyDown[VK_XBUTTON1];
    Snapshot.bXButton1Pressed = Snapshot.KeyPressed[VK_XBUTTON1];
    Snapshot.bXButton1Released = Snapshot.KeyReleased[VK_XBUTTON1];
    Snapshot.bXButton2Down = Snapshot.KeyDown[VK_XBUTTON2];
    Snapshot.bXButton2Pressed = Snapshot.KeyPressed[VK_XBUTTON2];
    Snapshot.bXButton2Released = Snapshot.KeyReleased[VK_XBUTTON2];

    Snapshot.MousePos = MousePos;
    Snapshot.MouseDeltaX = FrameMouseDeltaX;
    Snapshot.MouseDeltaY = FrameMouseDeltaY;
    Snapshot.ScrollDelta = PrevScrollDelta;

    Snapshot.bLeftDragStarted = bLeftDragJustStarted;
    Snapshot.bLeftDragging = bLeftDragging;
    Snapshot.bLeftDragEnded = bLeftDragJustEnded;
    Snapshot.LeftDragVector = GetLeftDragVector();

    Snapshot.bRightDragStarted = bRightDragJustStarted;
    Snapshot.bRightDragging = bRightDragging;
    Snapshot.bRightDragEnded = bRightDragJustEnded;
    Snapshot.RightDragVector = GetRightDragVector();

    Snapshot.bUsingRawMouse = bUseRawMouse;
    Snapshot.bGuiUsingMouse = GuiState.bUsingMouse;
    Snapshot.bGuiUsingKeyboard = GuiState.bUsingKeyboard;
    Snapshot.bGuiUsingTextInput = GuiState.bUsingTextInput;
    Snapshot.bWindowFocused = bWindowFocused;
    CurrentSnapshot = Snapshot;
}

void InputSystem::ResetDragState()
{
    bLeftDragCandidate = false;
    bRightDragCandidate = false;
    bLeftDragging = false;
    bRightDragging = false;
    bLeftDragJustStarted = false;
    bRightDragJustStarted = false;
    bLeftDragJustEnded = false;
    bRightDragJustEnded = false;
    LeftDragStartPos = MousePos;
    LeftMouseDownPos = MousePos;
    RightDragStartPos = MousePos;
    RightMouseDownPos = MousePos;
}

void InputSystem::FilterDragThreshold(
    bool& bCandidate, bool& bDragging, bool& bJustStarted,
    const POINT& MouseDownPos, POINT& DragStartPos)
{
    if (bCandidate && !bDragging)
    {
        int DX = MousePos.x - MouseDownPos.x;
        int DY = MousePos.y - MouseDownPos.y;
        int DistSq = DX * DX + DY * DY;

        if (DistSq >= DRAG_THRESHOLD * DRAG_THRESHOLD)
        {
            bJustStarted = true;
            bDragging = true;
            DragStartPos = MouseDownPos;
        }
    }
}

POINT InputSystem::GetLeftDragVector() const
{
    POINT V;
    V.x = MousePos.x - LeftDragStartPos.x;
    V.y = MousePos.y - LeftDragStartPos.y;
    return V;
}

POINT InputSystem::GetRightDragVector() const
{
    POINT V;
    V.x = MousePos.x - RightDragStartPos.x;
    V.y = MousePos.y - RightDragStartPos.y;
    return V;
}

float InputSystem::GetLeftDragDistance() const
{
    POINT V = GetLeftDragVector();
    return std::sqrt((float)(V.x * V.x + V.y * V.y));
}

float InputSystem::GetRightDragDistance() const
{
    POINT V = GetRightDragVector();
    return std::sqrt((float)(V.x * V.x + V.y * V.y));
}
