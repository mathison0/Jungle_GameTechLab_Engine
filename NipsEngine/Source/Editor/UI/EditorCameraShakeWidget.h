#pragma once
#include "Editor/UI/EditorWidget.h"

class UCameraShakeModifier;
class APlayerCameraManager;

class FEditorCameraShakeWidget : public FEditorWidget
{
public:
    void Initialize(UEditorEngine* InEditorEngine) override;
    void Render(float DeltaTime) override;

private:
    APlayerCameraManager* GetCameraManager() const;

    float PreviewAmplitude = 0.3f;
    float PreviewFrequency = 15.0f;
    float PreviewDuration  = 0.5f;
    float BezierCP[4] = { 0.25f, 0.1f, 0.75f, 0.9f };

    UCameraShakeModifier* ShakeModifier = nullptr;
};
