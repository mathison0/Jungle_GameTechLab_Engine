// 에디터 영역의 세부 동작을 구현합니다.
#include "Editor/UI/EditorControlPanel.h"
#include "Editor/EditorEngine.h"
#include "Editor/Settings/EditorSettings.h"
#include "ImGui/imgui.h"
#include "Component/CameraComponent.h"
#include "Math/MathUtils.h"

void FEditorControlPanel::Initialize(UEditorEngine* InEditorEngine)
{
    FEditorPanel::Initialize(InEditorEngine);
}

void FEditorControlPanel::Render(float DeltaTime)
{
    (void)DeltaTime;
    if (!EditorEngine)
        return;

    UCameraComponent* Camera = EditorEngine->GetCamera();
    if (!Camera)
    {
        return;
    }

    ImGui::SetNextWindowCollapsed(false, ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(420.0f, 280.0f), ImGuiCond_Once);
    ImGui::Begin("Control Camera");

    float FOV_Deg = Camera->GetFOV() * RAD_TO_DEG;
    if (ImGui::DragFloat("FOV", &FOV_Deg, 0.5f, 1.0f, 90.0f))
        Camera->SetFOV(FOV_Deg * DEG_TO_RAD);

    float Speed = FEditorSettings::Get().CameraSpeed;
    if (ImGui::DragFloat("Speed", &Speed, 0.1f, 0.1f, 200.0f, "%.2f"))
        FEditorSettings::Get().CameraSpeed = Clamp(Speed, 0.1f, 200.0f);

    float OrthoWidth = Camera->GetOrthoWidth();
    if (ImGui::DragFloat("Ortho Width", &OrthoWidth, 0.1f, 0.1f, 1000.0f))
        Camera->SetOrthoWidth(Clamp(OrthoWidth, 0.1f, 1000.0f));

    FVector CamPos = Camera->GetWorldLocation();
    float Location[3] = { CamPos.X, CamPos.Y, CamPos.Z };
    if (ImGui::DragFloat3("Location", Location, 0.1f))
        Camera->SetWorldLocation(FVector(Location[0], Location[1], Location[2]));

    FRotator CamRot = Camera->GetRelativeRotation();
    float Rotation[3] = { CamRot.Roll, CamRot.Pitch, CamRot.Yaw };
    if (ImGui::DragFloat3("Rotation", Rotation, 0.1f))
        Camera->SetRelativeRotation(FRotator(Rotation[1], Rotation[2], CamRot.Roll));

    ImGui::End();
}
