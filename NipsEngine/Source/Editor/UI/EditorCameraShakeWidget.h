#pragma once
#include "Editor/UI/EditorWidget.h"

class APlayerCameraManager;
class ULuaCameraModifier;

class FEditorCameraShakeWidget : public FEditorWidget
{
public:
    void Initialize(UEditorEngine* InEditorEngine) override;
    void Render(float DeltaTime) override;

private:
    APlayerCameraManager* GetCameraManager() const;

    void SaveLuaScript();
    void LoadLuaScript();

    bool OpenSaveDialog(std::wstring& OutPath);
    bool OpenLoadDialog(std::wstring& OutPath);

    void GenerateLuaSource(std::string& OutSource) const;
    bool ParseLuaSource(const std::string& Source);

    float PreviewAmplitude = 0.3f;
    float PreviewFrequency = 15.0f;
    float PreviewDuration  = 0.5f;
    // [0]=CP1x [1]=CP1y [2]=CP2x [3]=CP2y [4]=P0y(시작) [5]=P3y(끝)
    float BezierCP[6] = { 0.25f, 0.1f, 0.75f, 0.9f, 1.0f, 0.0f };

    ULuaCameraModifier* LuaModifier = nullptr;
    std::string LoadedScriptPath;
    std::string LastError;
};
