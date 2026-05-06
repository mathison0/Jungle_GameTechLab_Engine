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

// ── UI 헬퍼 ────────────────────────────────────────────────────────────────

// 같은 줄에 Amplitude/Frequency를 vec3(3열)로 그린다.
// label: 행 레이블,  v[3]: 값 배열,  axisLabels: {"Pitch","Yaw","Roll"} 등
static void Vec3Row(const char* rowId, const char* label,
                    float* v, float vMin, float vMax, float speed, const char* fmt)
{
    ImGui::PushID(rowId);
    const float labelW = 90.0f;
    const float fieldW = (ImGui::GetContentRegionAvail().x - labelW - 8.0f) / 3.0f;

    ImGui::Text("%s", label);
    for (int i = 0; i < 3; ++i)
    {
        ImGui::SameLine(labelW + i * (fieldW + 4.0f));
        ImGui::PushID(i);
        ImGui::SetNextItemWidth(fieldW);
        ImGui::DragFloat("##v", &v[i], speed, vMin, vMax, fmt);
        ImGui::PopID();
    }
    ImGui::PopID();
}

// 축 헤더 라인 (Pitch | Yaw | Roll)
static void Vec3Header(const char* l0, const char* l1, const char* l2)
{
    const float labelW = 90.0f;
    const float fieldW = (ImGui::GetContentRegionAvail().x - labelW - 8.0f) / 3.0f;
    ImGui::Dummy(ImVec2(labelW, 0)); ImGui::SameLine();
    ImGui::SetNextItemWidth(fieldW);
    ImGui::TextDisabled("%*s%s", (int)(fieldW * 0.3f), "", l0); ImGui::SameLine(0, 0);
    ImGui::Dummy(ImVec2(4, 0)); ImGui::SameLine();
    ImGui::TextDisabled("%*s%s", (int)(fieldW * 0.3f), "", l1); ImGui::SameLine(0, 0);
    ImGui::Dummy(ImVec2(4, 0)); ImGui::SameLine();
    ImGui::TextDisabled("%*s%s", (int)(fieldW * 0.3f), "", l2);
}

// ── FEditorCameraShakeWidget ────────────────────────────────────────────────

void FEditorCameraShakeWidget::Initialize(UEditorEngine* InEditorEngine)
{
    FEditorWidget::Initialize(InEditorEngine);
}

void FEditorCameraShakeWidget::Render(float DeltaTime)
{
    (void)DeltaTime;

    ImGui::SetNextWindowSize(ImVec2(440.0f, 900.0f), ImGuiCond_Once);
    if (!ImGui::Begin("Camera Shake"))
    {
        ImGui::End();
        return;
    }

    ImGui::SliderFloat("Duration", &PreviewParams.Duration, 0.1f, 5.0f, "%.1f s");
    ImGui::Spacing();

    // ── Rotation ──────────────────────────────────────────────────────────
    ImGui::SeparatorText("Rotation");
    Vec3Header("Pitch", "Yaw", "Roll");
    Vec3Row("rot_amp",  "Amplitude", PreviewParams.RotAmplitude, 0.0f, 2.0f,  0.005f, "%.3f");
    Vec3Row("rot_freq", "Frequency", PreviewParams.RotFrequency, 0.1f, 60.0f, 0.2f,   "%.1f");
    ImGui::Spacing();
    ImGui::TextDisabled("Decay Curve"); ImGui::SameLine(); ImGui::TextDisabled("(drag)");
    BezierUI::Bezier("##bez_rot", PreviewParams.RotBezierCP);

    ImGui::Spacing();

    // ── Location ──────────────────────────────────────────────────────────
    ImGui::SeparatorText("Location");
    Vec3Header("X", "Y", "Z");
    Vec3Row("loc_amp",  "Amplitude", PreviewParams.LocAmplitude, 0.0f, 20.0f, 0.05f, "%.2f");
    Vec3Row("loc_freq", "Frequency", PreviewParams.LocFrequency, 0.1f, 60.0f, 0.2f,  "%.1f");
    ImGui::Spacing();
    ImGui::TextDisabled("Decay Curve"); ImGui::SameLine(); ImGui::TextDisabled("(drag)");
    BezierUI::Bezier("##bez_loc", PreviewParams.LocBezierCP);

    ImGui::Spacing();

    // ── FOV ───────────────────────────────────────────────────────────────
    ImGui::SeparatorText("FOV");
    {
        const float w = (ImGui::GetContentRegionAvail().x - 90.0f - 4.0f) * 0.5f;
        ImGui::Text("Amplitude"); ImGui::SameLine(90.0f);
        ImGui::SetNextItemWidth(w); ImGui::DragFloat("##fov_amp",  &PreviewParams.FOVAmplitude, 0.001f, 0.0f, 0.2f, "%.4f");
        ImGui::SameLine(0, 4);
        ImGui::Text("Frequency"); ImGui::SameLine();
        ImGui::SetNextItemWidth(w); ImGui::DragFloat("##fov_freq", &PreviewParams.FOVFrequency, 0.2f,   0.1f, 60.0f, "%.1f");
    }
    ImGui::Spacing();
    ImGui::TextDisabled("Decay Curve"); ImGui::SameLine(); ImGui::TextDisabled("(drag)");
    BezierUI::Bezier("##bez_fov", PreviewParams.FOVBezierCP);

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
            Manager->StartCameraShake(PreviewParams);
    }
    if (bIsShaking) ImGui::EndDisabled();

    ImGui::SameLine();

    if (!bIsShaking) ImGui::BeginDisabled();
    if (ImGui::Button("Stop##builtin", ImVec2(60.0f, 0.0f)))
        if (Manager) Manager->StopCameraShake();
    if (!bIsShaking) ImGui::EndDisabled();

    ImGui::SameLine();
    if (bIsShaking)
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "SHAKING");
    else
        ImGui::TextDisabled("idle");

    // ── Lua ───────────────────────────────────────────────────────────────
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Lua Script");
    ImGui::Spacing();

    if (ImGui::Button("Save Lua", ImVec2(90.0f, 0.0f))) SaveLuaScript();
    ImGui::SameLine();
    if (ImGui::Button("Load Lua", ImVec2(90.0f, 0.0f))) LoadLuaScript();

    ImGui::Spacing();
    if (LoadedScriptPath.empty())
        ImGui::TextDisabled("No script loaded");
    else
    {
        const std::filesystem::path P(LoadedScriptPath);
        ImGui::TextDisabled("File: %s", P.filename().string().c_str());
    }

    if (!LastError.empty())
    {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Error: %s", LastError.c_str());
    }

    ImGui::Spacing();
    const bool bScriptReady = !LoadedScriptPath.empty();
    const bool bLuaActive   = (LuaModifier != nullptr);

    if (!bScriptReady) ImGui::BeginDisabled();
    if (ImGui::Button("Preview Lua", ImVec2(100.0f, 0.0f)))
    {
        if (Manager)
        {
            if (LuaModifier) { Manager->RemoveCameraModifier(LuaModifier); LuaModifier = nullptr; }
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
        if (Manager && LuaModifier) { Manager->RemoveCameraModifier(LuaModifier); LuaModifier = nullptr; }
    }
    if (!bLuaActive) ImGui::EndDisabled();

    ImGui::SameLine();
    if (bLuaActive)
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "LUA ACTIVE");
    else
        ImGui::TextDisabled("idle");

    ImGui::End();
}

// ── Save / Load ───────────────────────────────────────────────────────────

void FEditorCameraShakeWidget::SaveLuaScript()
{
    std::wstring OutPath;
    if (!OpenSaveDialog(OutPath)) return;

    std::string Source;
    GenerateLuaSource(Source);

    std::ofstream File(OutPath, std::ios::binary);
    if (!File.is_open()) { LastError = "Failed to open file for writing."; return; }
    File.write(Source.c_str(), static_cast<std::streamsize>(Source.size()));
    if (File.bad())      { LastError = "Failed to write lua file."; return; }

    LastError.clear();
    LoadedScriptPath = FPaths::ToUtf8(OutPath);
}

void FEditorCameraShakeWidget::LoadLuaScript()
{
    std::wstring OutPath;
    if (!OpenLoadDialog(OutPath)) return;

    std::ifstream File(OutPath, std::ios::binary);
    if (!File.is_open()) { LastError = "Failed to open file for reading."; return; }

    std::string Source((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
    if (File.bad())      { LastError = "Failed to read lua file."; return; }

    if (!ParseLuaSource(Source)) { LastError = "Failed to parse shake parameters from lua file."; return; }

    LastError.clear();
    LoadedScriptPath = FPaths::ToUtf8(OutPath);

    if (LuaModifier)
    {
        APlayerCameraManager* Manager = GetCameraManager();
        if (Manager) Manager->RemoveCameraModifier(LuaModifier);
        LuaModifier = nullptr;
    }
}

// ── 파일 다이얼로그 ───────────────────────────────────────────────────────

bool FEditorCameraShakeWidget::OpenSaveDialog(std::wstring& OutPath)
{
    const std::wstring InitialDir = GetLuaShakeDir();
    const std::filesystem::path PrevCwd = std::filesystem::current_path();
    std::error_code Ec;
    std::filesystem::current_path(InitialDir, Ec);

    WCHAR FileBuffer[MAX_PATH] = {};
    const std::wstring DefaultName = std::filesystem::path(InitialDir) / L"NewShake.lua";
    wcsncpy_s(FileBuffer, MAX_PATH, DefaultName.c_str(), _TRUNCATE);

    OPENFILENAMEW Dlg = {};
    Dlg.lStructSize     = sizeof(Dlg);
    Dlg.hwndOwner       = static_cast<HWND>(ImGui::GetMainViewport()->PlatformHandleRaw);
    Dlg.lpstrFilter     = L"Lua Files (*.lua)\0*.lua\0All Files (*.*)\0*.*\0";
    Dlg.lpstrFile       = FileBuffer;
    Dlg.nMaxFile        = MAX_PATH;
    Dlg.lpstrInitialDir = InitialDir.c_str();
    Dlg.lpstrDefExt     = L"lua";
    Dlg.Flags           = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

    const BOOL bPicked = GetSaveFileNameW(&Dlg);
    std::filesystem::current_path(PrevCwd, Ec);
    if (!bPicked) return false;
    OutPath = FileBuffer;
    return true;
}

bool FEditorCameraShakeWidget::OpenLoadDialog(std::wstring& OutPath)
{
    const std::wstring InitialDir = GetLuaShakeDir();
    const std::filesystem::path PrevCwd = std::filesystem::current_path();
    std::error_code Ec;
    std::filesystem::current_path(InitialDir, Ec);

    WCHAR FileBuffer[MAX_PATH] = {};
    wcsncpy_s(FileBuffer, MAX_PATH, InitialDir.c_str(), _TRUNCATE);

    OPENFILENAMEW Dlg = {};
    Dlg.lStructSize     = sizeof(Dlg);
    Dlg.hwndOwner       = static_cast<HWND>(ImGui::GetMainViewport()->PlatformHandleRaw);
    Dlg.lpstrFilter     = L"Lua Files (*.lua)\0*.lua\0All Files (*.*)\0*.*\0";
    Dlg.lpstrFile       = FileBuffer;
    Dlg.nMaxFile        = MAX_PATH;
    Dlg.lpstrInitialDir = InitialDir.c_str();
    Dlg.lpstrDefExt     = L"lua";
    Dlg.Flags           = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    const BOOL bPicked = GetOpenFileNameW(&Dlg);
    std::filesystem::current_path(PrevCwd, Ec);
    if (!bPicked) return false;
    OutPath = FileBuffer;
    return true;
}

// ── Lua 생성 ─────────────────────────────────────────────────────────────

void FEditorCameraShakeWidget::GenerateLuaSource(std::string& OutSource) const
{
    const FCameraShakeParams& P = PreviewParams;
    char Buf[8192];
    std::snprintf(Buf, sizeof(Buf),
        "-- Camera Shake Asset\n"
        "-- Generated by NipsEngine Camera Shake Editor\n"
        "\n"
        "local duration      = %.4f\n"
        "\n"
        "-- Rotation (Pitch/Yaw/Roll)\n"
        "local rot_amp_pitch = %.4f\n"
        "local rot_amp_yaw   = %.4f\n"
        "local rot_amp_roll  = %.4f\n"
        "local rot_freq_pitch= %.4f\n"
        "local rot_freq_yaw  = %.4f\n"
        "local rot_freq_roll = %.4f\n"
        "local rot_cp1x=%.4f local rot_cp1y=%.4f local rot_cp2x=%.4f local rot_cp2y=%.4f local rot_cp0y=%.4f local rot_cp3y=%.4f\n"
        "\n"
        "-- Location (X/Y/Z)\n"
        "local loc_amp_x     = %.4f\n"
        "local loc_amp_y     = %.4f\n"
        "local loc_amp_z     = %.4f\n"
        "local loc_freq_x    = %.4f\n"
        "local loc_freq_y    = %.4f\n"
        "local loc_freq_z    = %.4f\n"
        "local loc_cp1x=%.4f local loc_cp1y=%.4f local loc_cp2x=%.4f local loc_cp2y=%.4f local loc_cp0y=%.4f local loc_cp3y=%.4f\n"
        "\n"
        "-- FOV\n"
        "local fov_amplitude = %.4f\n"
        "local fov_frequency = %.4f\n"
        "local fov_cp1x=%.4f local fov_cp1y=%.4f local fov_cp2x=%.4f local fov_cp2y=%.4f local fov_cp0y=%.4f local fov_cp3y=%.4f\n"
        "\n"
        "local elapsed = 0.0\n"
        "\n"
        "local function cubic(t, p0, p1, p2, p3)\n"
        "    local u = 1.0 - t\n"
        "    return u*u*u*p0 + 3*u*u*t*p1 + 3*u*t*t*p2 + t*t*t*p3\n"
        "end\n"
        "\n"
        "function ModifyCamera(view, dt)\n"
        "    elapsed = elapsed + dt\n"
        "    if elapsed >= duration then elapsed = 0.0; return false end\n"
        "\n"
        "    local t = elapsed / duration\n"
        "\n"
        "    local ra = cubic(t, rot_cp0y, rot_cp1y, rot_cp2y, rot_cp3y)\n"
        "    view.Rotation.X = view.Rotation.X + math.sin(elapsed * rot_freq_pitch) * rot_amp_pitch * ra\n"
        "    view.Rotation.Y = view.Rotation.Y + math.cos(elapsed * rot_freq_yaw)   * rot_amp_yaw   * ra\n"
        "    view.Rotation.Z = view.Rotation.Z + math.sin(elapsed * rot_freq_roll)  * rot_amp_roll  * ra\n"
        "\n"
        "    if loc_amp_x ~= 0.0 or loc_amp_y ~= 0.0 or loc_amp_z ~= 0.0 then\n"
        "        local la = cubic(t, loc_cp0y, loc_cp1y, loc_cp2y, loc_cp3y)\n"
        "        view.Location.X = view.Location.X + math.sin(elapsed * loc_freq_x) * loc_amp_x * la\n"
        "        view.Location.Y = view.Location.Y + math.cos(elapsed * loc_freq_y) * loc_amp_y * la\n"
        "        view.Location.Z = view.Location.Z + math.sin(elapsed * loc_freq_z) * loc_amp_z * la\n"
        "    end\n"
        "\n"
        "    if fov_amplitude ~= 0.0 then\n"
        "        local fa = cubic(t, fov_cp0y, fov_cp1y, fov_cp2y, fov_cp3y)\n"
        "        view.FOV = view.FOV + math.sin(elapsed * fov_frequency) * fov_amplitude * fa\n"
        "    end\n"
        "\n"
        "    return true\n"
        "end\n",
        P.Duration,
        P.RotAmplitude[0], P.RotAmplitude[1], P.RotAmplitude[2],
        P.RotFrequency[0], P.RotFrequency[1], P.RotFrequency[2],
        P.RotBezierCP[0], P.RotBezierCP[1], P.RotBezierCP[2], P.RotBezierCP[3], P.RotBezierCP[4], P.RotBezierCP[5],
        P.LocAmplitude[0], P.LocAmplitude[1], P.LocAmplitude[2],
        P.LocFrequency[0], P.LocFrequency[1], P.LocFrequency[2],
        P.LocBezierCP[0], P.LocBezierCP[1], P.LocBezierCP[2], P.LocBezierCP[3], P.LocBezierCP[4], P.LocBezierCP[5],
        P.FOVAmplitude, P.FOVFrequency,
        P.FOVBezierCP[0], P.FOVBezierCP[1], P.FOVBezierCP[2], P.FOVBezierCP[3], P.FOVBezierCP[4], P.FOVBezierCP[5]
    );
    OutSource = Buf;
}

// ── Lua 파싱 ─────────────────────────────────────────────────────────────

bool FEditorCameraShakeWidget::ParseLuaSource(const std::string& Source)
{
    auto ParseFloat = [&](const char* Name, float& Out, float Default = 0.0f) -> bool
    {
        const std::string Pattern = std::string("local ") + Name;
        const size_t Pos = Source.find(Pattern);
        if (Pos == std::string::npos) { Out = Default; return false; }
        const size_t Eq = Source.find('=', Pos);
        if (Eq == std::string::npos) { Out = Default; return false; }
        return std::sscanf(Source.c_str() + Eq + 1, " %f", &Out) == 1;
    };

    auto ParseBezier = [&](const char* prefix, float cp[6])
    {
        char name[64];
        float def[6] = { 0.25f, 0.1f, 0.75f, 0.9f, 1.0f, 0.0f };
        const char* keys[] = { "cp1x", "cp1y", "cp2x", "cp2y", "cp0y", "cp3y" };
        for (int i = 0; i < 6; ++i)
        {
            std::snprintf(name, sizeof(name), "%s_%s", prefix, keys[i]);
            ParseFloat(name, cp[i], def[i]);
        }
    };

    FCameraShakeParams& P = PreviewParams;

    if (!ParseFloat("duration", P.Duration, 0.5f)) return false;

    ParseFloat("rot_amp_pitch",  P.RotAmplitude[0], 0.3f);
    ParseFloat("rot_amp_yaw",    P.RotAmplitude[1], 0.0f);
    ParseFloat("rot_amp_roll",   P.RotAmplitude[2], 0.0f);
    ParseFloat("rot_freq_pitch", P.RotFrequency[0], 15.0f);
    ParseFloat("rot_freq_yaw",   P.RotFrequency[1], 15.0f);
    ParseFloat("rot_freq_roll",  P.RotFrequency[2], 15.0f);
    ParseBezier("rot", P.RotBezierCP);

    ParseFloat("loc_amp_x",  P.LocAmplitude[0], 0.0f);
    ParseFloat("loc_amp_y",  P.LocAmplitude[1], 0.0f);
    ParseFloat("loc_amp_z",  P.LocAmplitude[2], 0.0f);
    ParseFloat("loc_freq_x", P.LocFrequency[0], 15.0f);
    ParseFloat("loc_freq_y", P.LocFrequency[1], 15.0f);
    ParseFloat("loc_freq_z", P.LocFrequency[2], 15.0f);
    ParseBezier("loc", P.LocBezierCP);

    ParseFloat("fov_amplitude", P.FOVAmplitude, 0.0f);
    ParseFloat("fov_frequency", P.FOVFrequency, 15.0f);
    ParseBezier("fov", P.FOVBezierCP);

    return true;
}

// ── 카메라 매니저 접근 ────────────────────────────────────────────────────

APlayerCameraManager* FEditorCameraShakeWidget::GetCameraManager() const
{
    if (!EditorEngine) return nullptr;
    const int32 FocusedIndex = EditorEngine->GetViewportLayout().GetLastFocusedViewportIndex();
    FEditorViewportClient* Client = EditorEngine->GetViewportLayout().GetViewportClient(FocusedIndex);
    return Client ? &Client->GetPlayerCameraManager() : nullptr;
}
