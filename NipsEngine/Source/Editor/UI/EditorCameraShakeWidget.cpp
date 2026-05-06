#include "Editor/UI/EditorCameraShakeWidget.h"

#include "Editor/EditorEngine.h"
#include "Editor/Viewport/EditorViewportClient.h"
#include "Editor/Viewport/ViewportLayout.h"
#include "Engine/Camera/PlayerCameraManager.h"
#include "Engine/Camera/Modifier/LuaCameraModifier.h"
#include "Engine/Core/Paths.h"
#include "ImGui/imgui.h"
#include "BezierUI.h"

#include <Windows.h>
#include <commdlg.h>
#include <filesystem>
#include <fstream>
#include <cstdio>

// ── 내부 헬퍼 ──────────────────────────────────────────────────────────────

namespace
{
std::wstring GetLuaShakeDir()
{
    std::filesystem::path Dir(FPaths::RootDir());
    Dir /= L"Asset";
    Dir /= L"CameraShake";
    Dir.make_preferred();
    std::error_code Ec;
    std::filesystem::create_directories(Dir, Ec);
    return Dir.wstring();
}
} // namespace

// ── FEditorCameraShakeWidget ────────────────────────────────────────────────

void FEditorCameraShakeWidget::Initialize(UEditorEngine* InEditorEngine)
{
    FEditorWidget::Initialize(InEditorEngine);
}

void FEditorCameraShakeWidget::Render(float DeltaTime)
{
    (void)DeltaTime;

    ImGui::SetNextWindowSize(ImVec2(420.0f, 520.0f), ImGuiCond_Once);
    if (!ImGui::Begin("Camera Shake"))
    {
        ImGui::End();
        return;
    }

    // ── 파라미터 슬라이더 ──────────────────────────────────────────────────
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
    ImGui::SliderFloat("Amplitude", &PreviewAmplitude, 0.0f,  2.0f,  "%.2f");
    ImGui::SliderFloat("Frequency", &PreviewFrequency, 1.0f,  60.0f, "%.1f Hz");
    ImGui::SliderFloat("Duration",  &PreviewDuration,  0.1f,  5.0f,  "%.1f s");
    ImGui::PopItemWidth();

    // ── Bezier 감쇠 커브 ───────────────────────────────────────────────────
    ImGui::Spacing();
    ImGui::Text("Decay Curve");
    ImGui::SameLine();
    ImGui::TextDisabled("(drag control points)");
    BezierUI::Bezier("##shake_curve", BezierCP);

    ImGui::Spacing();
    ImGui::TextDisabled("CP1(%.2f, %.2f)  CP2(%.2f, %.2f)",
        BezierCP[0], BezierCP[1], BezierCP[2], BezierCP[3]);

    // ── C++ 빌트인 미리보기 ────────────────────────────────────────────────
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    APlayerCameraManager* Manager = GetCameraManager();
    const bool bIsShaking = Manager && Manager->IsCameraShaking();

    if (bIsShaking) ImGui::BeginDisabled();
    if (ImGui::Button("Preview Shake", ImVec2(130.0f, 0.0f)))
    {
        if (Manager)
            Manager->StartCameraShake(PreviewAmplitude, PreviewFrequency, PreviewDuration, BezierCP);
    }
    if (bIsShaking) ImGui::EndDisabled();

    ImGui::SameLine();

    if (!bIsShaking) ImGui::BeginDisabled();
    if (ImGui::Button("Stop##builtin", ImVec2(60.0f, 0.0f)))
    {
        if (Manager)
            Manager->StopCameraShake();
    }
    if (!bIsShaking) ImGui::EndDisabled();

    ImGui::SameLine();
    if (bIsShaking)
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "SHAKING");
    else
        ImGui::TextDisabled("idle");

    // ── Lua 스크립트 섹션 ─────────────────────────────────────────────────
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Lua Script");
    ImGui::Spacing();

    // Save / Load 버튼
    if (ImGui::Button("Save Lua", ImVec2(90.0f, 0.0f)))
        SaveLuaScript();

    ImGui::SameLine();

    if (ImGui::Button("Load Lua", ImVec2(90.0f, 0.0f)))
        LoadLuaScript();

    // 로드된 파일 표시
    ImGui::Spacing();
    if (LoadedScriptPath.empty())
    {
        ImGui::TextDisabled("No script loaded");
    }
    else
    {
        const std::filesystem::path P(LoadedScriptPath);
        ImGui::TextDisabled("File: %s", P.filename().string().c_str());
    }

    // 에러 메시지
    if (!LastError.empty())
    {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Error: %s", LastError.c_str());
    }

    // Lua Preview / Stop
    ImGui::Spacing();
    const bool bScriptReady = !LoadedScriptPath.empty();
    const bool bLuaActive   = (LuaModifier != nullptr);

    if (!bScriptReady) ImGui::BeginDisabled();
    if (ImGui::Button("Preview Lua", ImVec2(100.0f, 0.0f)))
    {
        if (Manager)
        {
            // 이전 modifier 제거 후 새로 생성 (elapsed 초기화)
            if (LuaModifier)
            {
                Manager->RemoveCameraModifier(LuaModifier);
                LuaModifier = nullptr;
            }
            LuaModifier = Manager->AddLuaCameraModifier(LoadedScriptPath);
            LastError.clear();
            if (LuaModifier && !LuaModifier->IsScriptLoaded())
            {
                LastError = LuaModifier->GetLastScriptError();
                Manager->RemoveCameraModifier(LuaModifier);
                LuaModifier = nullptr;
            }
        }
    }
    if (!bScriptReady) ImGui::EndDisabled();

    ImGui::SameLine();

    if (!bLuaActive) ImGui::BeginDisabled();
    if (ImGui::Button("Stop##lua", ImVec2(60.0f, 0.0f)))
    {
        if (Manager && LuaModifier)
        {
            Manager->RemoveCameraModifier(LuaModifier);
            LuaModifier = nullptr;
        }
    }
    if (!bLuaActive) ImGui::EndDisabled();

    ImGui::SameLine();
    if (bLuaActive)
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "LUA ACTIVE");
    else
        ImGui::TextDisabled("idle");

    ImGui::End();
}

// ── Save ──────────────────────────────────────────────────────────────────

void FEditorCameraShakeWidget::SaveLuaScript()
{
    std::wstring OutPath;
    if (!OpenSaveDialog(OutPath))
        return;

    std::string Source;
    GenerateLuaSource(Source);

    std::ofstream File(OutPath, std::ios::binary);
    if (!File.is_open())
    {
        LastError = "Failed to open file for writing.";
        return;
    }

    File.write(Source.c_str(), static_cast<std::streamsize>(Source.size()));
    if (File.bad())
    {
        LastError = "Failed to write lua file.";
        return;
    }

    LastError.clear();
    LoadedScriptPath = FPaths::ToUtf8(OutPath);
}

// ── Load ──────────────────────────────────────────────────────────────────

void FEditorCameraShakeWidget::LoadLuaScript()
{
    std::wstring OutPath;
    if (!OpenLoadDialog(OutPath))
        return;

    std::ifstream File(OutPath, std::ios::binary);
    if (!File.is_open())
    {
        LastError = "Failed to open file for reading.";
        return;
    }

    std::string Source((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
    if (File.bad())
    {
        LastError = "Failed to read lua file.";
        return;
    }

    if (!ParseLuaSource(Source))
    {
        LastError = "Failed to parse shake parameters from lua file.";
        return;
    }

    LastError.clear();
    LoadedScriptPath = FPaths::ToUtf8(OutPath);

    // 기존 재생 중인 modifier 정리
    if (LuaModifier)
    {
        APlayerCameraManager* Manager = GetCameraManager();
        if (Manager)
            Manager->RemoveCameraModifier(LuaModifier);
        LuaModifier = nullptr;
    }
}

// ── 파일 다이얼로그 ───────────────────────────────────────────────────────

bool FEditorCameraShakeWidget::OpenSaveDialog(std::wstring& OutPath)
{
    const std::wstring InitialDir = GetLuaShakeDir();
    const std::filesystem::path PrevCwd = std::filesystem::current_path();
    std::error_code ChdirEc;
    std::filesystem::current_path(InitialDir, ChdirEc);

    WCHAR FileBuffer[MAX_PATH] = {};
    const std::wstring DefaultName = std::filesystem::path(InitialDir) / L"NewShake.lua";
    wcsncpy_s(FileBuffer, MAX_PATH, DefaultName.c_str(), _TRUNCATE);

    OPENFILENAMEW Dlg = {};
    Dlg.lStructSize   = sizeof(Dlg);
    Dlg.hwndOwner     = static_cast<HWND>(ImGui::GetMainViewport()->PlatformHandleRaw);
    Dlg.lpstrFilter   = L"Lua Files (*.lua)\0*.lua\0All Files (*.*)\0*.*\0";
    Dlg.lpstrFile     = FileBuffer;
    Dlg.nMaxFile      = MAX_PATH;
    Dlg.lpstrInitialDir = InitialDir.c_str();
    Dlg.lpstrDefExt   = L"lua";
    Dlg.Flags         = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

    const BOOL bPicked = GetSaveFileNameW(&Dlg);
    std::error_code RestoreEc;
    std::filesystem::current_path(PrevCwd, RestoreEc);

    if (!bPicked)
        return false;

    OutPath = FileBuffer;
    return true;
}

bool FEditorCameraShakeWidget::OpenLoadDialog(std::wstring& OutPath)
{
    const std::wstring InitialDir = GetLuaShakeDir();
    const std::filesystem::path PrevCwd = std::filesystem::current_path();
    std::error_code ChdirEc;
    std::filesystem::current_path(InitialDir, ChdirEc);

    WCHAR FileBuffer[MAX_PATH] = {};
    wcsncpy_s(FileBuffer, MAX_PATH, InitialDir.c_str(), _TRUNCATE);

    OPENFILENAMEW Dlg = {};
    Dlg.lStructSize   = sizeof(Dlg);
    Dlg.hwndOwner     = static_cast<HWND>(ImGui::GetMainViewport()->PlatformHandleRaw);
    Dlg.lpstrFilter   = L"Lua Files (*.lua)\0*.lua\0All Files (*.*)\0*.*\0";
    Dlg.lpstrFile     = FileBuffer;
    Dlg.nMaxFile      = MAX_PATH;
    Dlg.lpstrInitialDir = InitialDir.c_str();
    Dlg.lpstrDefExt   = L"lua";
    Dlg.Flags         = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    const BOOL bPicked = GetOpenFileNameW(&Dlg);
    std::error_code RestoreEc;
    std::filesystem::current_path(PrevCwd, RestoreEc);

    if (!bPicked)
        return false;

    OutPath = FileBuffer;
    return true;
}

// ── Lua 생성 / 파싱 ──────────────────────────────────────────────────────

void FEditorCameraShakeWidget::GenerateLuaSource(std::string& OutSource) const
{
    char Buf[2048];
    std::snprintf(Buf, sizeof(Buf),
        "-- Camera Shake Asset\n"
        "-- Generated by NipsEngine Camera Shake Editor\n"
        "\n"
        "local amplitude = %.4f\n"
        "local frequency = %.4f\n"
        "local duration = %.4f\n"
        "local cp1x = %.4f\n"
        "local cp1y = %.4f\n"
        "local cp2x = %.4f\n"
        "local cp2y = %.4f\n"
        "local cp0y = %.4f\n"
        "local cp3y = %.4f\n"
        "\n"
        "local elapsed = 0.0\n"
        "\n"
        "local function evaluateCubic(t, p0, p1, p2, p3)\n"
        "    local u = 1.0 - t\n"
        "    return u*u*u*p0 + 3*u*u*t*p1 + 3*u*t*t*p2 + t*t*t*p3\n"
        "end\n"
        "\n"
        "local function bezierDecay(t)\n"
        "    return evaluateCubic(t, cp0y, cp1y, cp2y, cp3y)\n"
        "end\n"
        "\n"
        "function ModifyCamera(view, deltaTime)\n"
        "    elapsed = elapsed + deltaTime\n"
        "    if elapsed >= duration then\n"
        "        elapsed = 0.0\n"
        "        return false\n"
        "    end\n"
        "\n"
        "    local t = elapsed / duration\n"
        "    local alpha = bezierDecay(t)\n"
        "\n"
        "    view.Rotation.X = view.Rotation.X + math.sin(elapsed * frequency) * amplitude * alpha\n"
        "    view.Rotation.Y = view.Rotation.Y + math.cos(elapsed * frequency * 0.8) * amplitude * alpha\n"
        "    view.Rotation.Z = view.Rotation.Z + math.sin(elapsed * frequency * 1.2) * amplitude * alpha\n"
        "    return true\n"
        "end\n",
        PreviewAmplitude, PreviewFrequency, PreviewDuration,
        BezierCP[0], BezierCP[1], BezierCP[2], BezierCP[3], BezierCP[4], BezierCP[5]
    );
    OutSource = Buf;
}

bool FEditorCameraShakeWidget::ParseLuaSource(const std::string& Source)
{
    auto ParseFloat = [&](const char* VarName, float& OutValue) -> bool
    {
        const std::string Pattern = std::string("local ") + VarName + " = ";
        const size_t Pos = Source.find(Pattern);
        if (Pos == std::string::npos)
            return false;
        const char* Start = Source.c_str() + Pos + Pattern.size();
        return std::sscanf(Start, "%f", &OutValue) == 1;
    };

    float A, F, D, Cx1, Cy1, Cx2, Cy2, C0y, C3y;
    if (!ParseFloat("amplitude", A))  return false;
    if (!ParseFloat("frequency", F))  return false;
    if (!ParseFloat("duration",  D))  return false;
    if (!ParseFloat("cp1x", Cx1))     return false;
    if (!ParseFloat("cp1y", Cy1))     return false;
    if (!ParseFloat("cp2x", Cx2))     return false;
    if (!ParseFloat("cp2y", Cy2))     return false;
    if (!ParseFloat("cp0y", C0y))     return false;
    if (!ParseFloat("cp3y", C3y))     return false;

    PreviewAmplitude = A;
    PreviewFrequency = F;
    PreviewDuration  = D;
    BezierCP[0] = Cx1;
    BezierCP[1] = Cy1;
    BezierCP[2] = Cx2;
    BezierCP[3] = Cy2;
    BezierCP[4] = C0y;
    BezierCP[5] = C3y;
    return true;
}

// ── 카메라 매니저 접근 ────────────────────────────────────────────────────

APlayerCameraManager* FEditorCameraShakeWidget::GetCameraManager() const
{
    if (!EditorEngine)
        return nullptr;

    const int32 FocusedIndex = EditorEngine->GetViewportLayout().GetLastFocusedViewportIndex();
    FEditorViewportClient* Client = EditorEngine->GetViewportLayout().GetViewportClient(FocusedIndex);
    return Client ? &Client->GetPlayerCameraManager() : nullptr;
}
