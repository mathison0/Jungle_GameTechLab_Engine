// 에디터 하단 drawer strip과 drawer overlay 상태를 관리합니다.
#pragma once

#include "Core/CoreTypes.h"

enum class EEditorDrawer
{
    None,
    Content,
    OutputLog,
};

class FEditorBottomBar
{
public:
    void Render(float DeltaTime);

    bool BeginDrawerOverlay();
    void EndDrawerOverlay();

    EEditorDrawer GetVisibleDrawer() const
    {
        return VisibleDrawer;
    }

private:
    void UpdateDrawerAnimation(float DeltaTime);
    void RenderBar();
    void DrawDrawerButton(const char* Label, EEditorDrawer Drawer);

private:
    EEditorDrawer ActiveDrawer = EEditorDrawer::None;
    EEditorDrawer VisibleDrawer = EEditorDrawer::None;
    float DrawerHeight = 320.0f;
    float DrawerOpenRatio = 0.0f;

    bool bDrawerOverlayBegun = false;
    float LastDrawerX = 0.0f;
    float LastDrawerY = 0.0f;
    float LastDrawerWidth = 0.0f;
    float LastDrawerHeight = 0.0f;
};
