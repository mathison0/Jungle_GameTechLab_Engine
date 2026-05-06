#include "Editor/UI/EditorCameraShakeWidget.h"

#include "Editor/EditorEngine.h"
#include "Editor/Viewport/EditorViewportClient.h"
#include "Editor/Viewport/ViewportLayout.h"
#include "Engine/Camera/PlayerCameraManager.h"
#include "ImGui/imgui.h"
#include "Bezier.h"

void FEditorCameraShakeWidget::Initialize(UEditorEngine* InEditorEngine)
{
    FEditorWidget::Initialize(InEditorEngine);
}

void FEditorCameraShakeWidget::Render(float DeltaTime)
{
    (void)DeltaTime;

    ImGui::SetNextWindowSize(ImVec2(420.0f, 400.0f), ImGuiCond_Once);
    if (!ImGui::Begin("Camera Shake"))
    {
        ImGui::End();
        return;
    }

    // ── 파라미터 슬라이더 ──────────────────────────────────────
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);

    ImGui::SliderFloat("Amplitude",  &PreviewAmplitude, 0.0f,  2.0f,  "%.2f");
    ImGui::SliderFloat("Frequency",  &PreviewFrequency, 1.0f,  60.0f, "%.1f Hz");
    ImGui::SliderFloat("Duration",   &PreviewDuration,  0.1f,  5.0f,  "%.1f s");

    ImGui::PopItemWidth();

    // ── Bezier 감쇠 커브 ────────────────────────────────────────
    ImGui::Spacing();
    ImGui::Text("Decay Curve");
    ImGui::SameLine();
    ImGui::TextDisabled("(drag control points)");

    Bezier::Bezier("##shake_curve", BezierCP);

    // ── 현재 커브 미리보기 값 텍스트 ────────────────────────────
    ImGui::Spacing();
    ImGui::TextDisabled("CP1(%.2f, %.2f)  CP2(%.2f, %.2f)",
        BezierCP[0], BezierCP[1], BezierCP[2], BezierCP[3]);

    // ── 버튼 ────────────────────────────────────────────────────
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    APlayerCameraManager* Manager = GetCameraManager();
    const bool bIsShaking = Manager && Manager->IsCameraShaking();

    if (bIsShaking)
        ImGui::BeginDisabled();

    if (ImGui::Button("Preview Shake", ImVec2(130.0f, 0.0f)))
    {
        if (Manager)
        {
            Manager->StartCameraShake(PreviewAmplitude, PreviewFrequency, PreviewDuration, BezierCP); // 만들어진 에셋을 잠시 플레이
        }
    }

    if (bIsShaking)
        ImGui::EndDisabled();

    ImGui::SameLine();

    if (!bIsShaking)
        ImGui::BeginDisabled();

    if (ImGui::Button("Stop", ImVec2(70.0f, 0.0f)))
    {
        if (Manager)
            Manager->StopCameraShake(); // 만들어진 에셋 리스트에서 빼기
    }

    if (!bIsShaking)
        ImGui::EndDisabled();

    ImGui::SameLine();
    if (bIsShaking)
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "SHAKING");
    else
        ImGui::TextDisabled("idle");

    ImGui::End();
}

APlayerCameraManager* FEditorCameraShakeWidget::GetCameraManager() const
{
    if (!EditorEngine)
        return nullptr;

    const int32 FocusedIndex = EditorEngine->GetViewportLayout().GetLastFocusedViewportIndex();
    FEditorViewportClient* Client = EditorEngine->GetViewportLayout().GetViewportClient(FocusedIndex);
    return Client ? &Client->GetPlayerCameraManager() : nullptr;
}
