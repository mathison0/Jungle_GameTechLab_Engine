// Content Drawer 내부 콘텐츠를 렌더링합니다.
#pragma once

#include "Editor/UI/EditorPanel.h"

class FEditorContentDrawerPanel : public FEditorPanel
{
public:
    virtual void Render(float DeltaTime) override;
};
