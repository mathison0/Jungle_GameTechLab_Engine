#include "Editor/UI/EditorMainPanel.h"

#include "Editor/EditorEngine.h"
#include "Editor/Selection/SelectionManager.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/Undo/EditorUndoSystem.h"
#include "Editor/Viewport/EditorViewportClient.h"
#include "Core/ResourceManager.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Math/Utils.h"
#include "Particle/ParticleDynamicData.h"
#include "Particle/ParticleSystemComponent.h"
#include "Render/Renderer/Renderer.h"
#include "Render/Resource/MeshBufferManager.h"
#include "Render/Scene/PrimitiveDrawCommandBuilder.h"
#include "Render/Scene/RenderBus.h"

#include "ImGui/imgui.h"

#include <algorithm>
#include <cstdio>

namespace
{
constexpr float MaxDebugCameraSpeedMultiplier = 20.0f;

FString FormatHistoryBytes(size_t Bytes)
{
    char Buffer[64];
    const double Value = static_cast<double>(Bytes);
    if (Bytes >= 1024ull * 1024ull)
    {
        snprintf(Buffer, sizeof(Buffer), "%.2f MB", Value / (1024.0 * 1024.0));
    }
    else if (Bytes >= 1024ull)
    {
        snprintf(Buffer, sizeof(Buffer), "%.2f KB", Value / 1024.0);
    }
    else
    {
        snprintf(Buffer, sizeof(Buffer), "%zu B", Bytes);
    }
    return Buffer;
}

float GetDebugCameraBaseSpeed()
{
    return std::max(0.1f, FEditorSettings::Get().CameraSpeed);
}

float GetDebugCameraSpeedMultiplier(FEditorViewportClient* Client)
{
    if (!Client)
    {
        return 1.0f;
    }
    return MathUtil::Clamp(Client->GetMoveSpeed() / GetDebugCameraBaseSpeed(), 0.01f, MaxDebugCameraSpeedMultiplier);
}

void SetDebugCameraSpeedMultiplier(FEditorViewportClient* Client, float Multiplier)
{
    if (!Client)
    {
        return;
    }

    Client->SetMoveSpeed(MathUtil::Clamp(
        GetDebugCameraBaseSpeed() * Multiplier,
        0.1f,
        GetDebugCameraBaseSpeed() * MaxDebugCameraSpeedMultiplier));
}

UParticleSystemComponent* FindSelectedParticleSystemComponent(UEditorEngine* InEditorEngine)
{
    if (!InEditorEngine)
    {
        return nullptr;
    }

    FWorldContext* Context = InEditorEngine->GetFocusedWorldContext();
    if (!Context || !Context->SelectionManager)
    {
        return nullptr;
    }

    if (UParticleSystemComponent* SelectedComponent = Cast<UParticleSystemComponent>(
        Context->SelectionManager->GetSelectedComponent()))
    {
        return SelectedComponent;
    }

    AActor* SelectedActor = Context->SelectionManager->GetPrimarySelection();
    return SelectedActor ? SelectedActor->FindComponent<UParticleSystemComponent>() : nullptr;
}

UParticleSystemComponent* FindFirstParticleSystemComponentInWorld(UEditorEngine* InEditorEngine)
{
    UWorld* World = InEditorEngine ? InEditorEngine->GetFocusedWorld() : nullptr;
    if (!World)
    {
        return nullptr;
    }

    for (AActor* Actor : World->GetActors())
    {
        if (!Actor)
        {
            continue;
        }

        if (UParticleSystemComponent* ParticleComponent = Actor->FindComponent<UParticleSystemComponent>())
        {
            return ParticleComponent;
        }
    }

    return nullptr;
}

bool RunSelectedParticleRuntimeSmoke(UEditorEngine* InEditorEngine, FString& OutSummary)
{
    UParticleSystemComponent* ParticleComponent = FindSelectedParticleSystemComponent(InEditorEngine);
    bool bUsedSelection = true;
    if (!ParticleComponent)
    {
        ParticleComponent = FindFirstParticleSystemComponentInWorld(InEditorEngine);
        bUsedSelection = false;
    }

    if (!ParticleComponent)
    {
        OutSummary = "no particle system component in selection or focused world";
        return false;
    }

    if (!ParticleComponent->GetTemplate())
    {
        OutSummary = "selected particle component has no template";
        return false;
    }

    const int32 EmitterInstanceCount = ParticleComponent->GetEmitterInstanceCount();
    if (EmitterInstanceCount <= 0)
    {
        OutSummary = "template did not create emitter instances";
        return false;
    }

    constexpr int32 WarmupFrameCount = 10;
    constexpr float FixedDeltaTime = 1.0f / 60.0f;
    for (int32 FrameIndex = 0; FrameIndex < WarmupFrameCount; ++FrameIndex)
    {
        ParticleComponent->TickPreview(FixedDeltaTime, true);
    }

    const int32 ActiveParticleCount = ParticleComponent->GetTotalActiveParticleCount();
    if (ActiveParticleCount <= 0)
    {
        char Buffer[160];
        snprintf(
            Buffer,
            sizeof(Buffer),
            "no active particles after %d warmup frames, emitters=%d",
            WarmupFrameCount,
            EmitterInstanceCount);
        OutSummary = Buffer;
        return false;
    }

    // Cycle 15a Phase 5: BuildInstanceData() 삭제됨 — CreateDynamicData() 가 대체.
    // 디버그 통계 의도: sprite instance build 결과 검증. 새 path 는 emitter active count 로 대체.
    uint32 SpriteInstanceCount = 0;
    for (int32 EmitterIndex = 0; EmitterIndex < EmitterInstanceCount; ++EmitterIndex)
    {
        FParticleEmitterInstance* Instance = ParticleComponent->GetEmitterInstance(EmitterIndex);
        if (!Instance)
        {
            continue;
        }

        // Sprite emitter 한정 통계 — DynamicData 생성 후 SpriteInstanceDataBuffer.size 측정 (한 frame 내 use-after-free 회피 위해 즉시 delete).
        FDynamicEmitterDataBase* DynData = Instance->CreateDynamicData();
        if (DynData && DynData->GetVertexFactoryType() == EVertexFactoryType::SpriteParticle)
        {
            FDynamicSpriteEmitterData* SpriteDyn = static_cast<FDynamicSpriteEmitterData*>(DynData);
            SpriteInstanceCount += static_cast<uint32>(SpriteDyn->SpriteInstanceDataBuffer.size());
        }
        delete DynData;
    }

    if (SpriteInstanceCount == 0)
    {
        char Buffer[192];
        snprintf(
            Buffer,
            sizeof(Buffer),
            "active particles exist but no sprite instance data was built, active=%d, emitters=%d",
            ActiveParticleCount,
            EmitterInstanceCount);
        OutSummary = Buffer;
        return false;
    }

    char Buffer[192];
    snprintf(
        Buffer,
        sizeof(Buffer),
        "%s, emitters=%d, active=%d, spriteInstances=%u",
        bUsedSelection ? "selected" : "firstInWorld",
        EmitterInstanceCount,
        ActiveParticleCount,
        SpriteInstanceCount);
    OutSummary = Buffer;
    return true;
}

bool RunParticleRenderCommandSmoke(UEditorEngine* InEditorEngine, FString& OutSummary)
{
    UParticleSystemComponent* ParticleComponent = FindSelectedParticleSystemComponent(InEditorEngine);
    bool bUsedSelection = true;
    if (!ParticleComponent)
    {
        ParticleComponent = FindFirstParticleSystemComponentInWorld(InEditorEngine);
        bUsedSelection = false;
    }

    if (!ParticleComponent)
    {
        OutSummary = "no particle system component in selection or focused world";
        return false;
    }

    if (!ParticleComponent->GetTemplate())
    {
        OutSummary = "particle component has no template";
        return false;
    }

    constexpr int32 WarmupFrameCount = 10;
    constexpr float FixedDeltaTime = 1.0f / 60.0f;
    for (int32 FrameIndex = 0; FrameIndex < WarmupFrameCount; ++FrameIndex)
    {
        ParticleComponent->TickPreview(FixedDeltaTime, true);
    }

    if (ParticleComponent->GetTotalActiveParticleCount() <= 0)
    {
        OutSummary = "no active particles available for render command collection";
        return false;
    }

    ID3D11Device* Device = InEditorEngine
        ? InEditorEngine->GetRenderer().GetFD3DDevice().GetDevice()
        : nullptr;
    if (!Device)
    {
        OutSummary = "renderer D3D device is not available";
        return false;
    }

    FRenderBus RenderBus;
    FMeshBufferManager MeshBufferManager;
    MeshBufferManager.Create(Device);

    FShowFlags ShowFlags;
    ShowFlags.bPrimitives = true;

    FPrimitiveDrawCommandBuilder Builder;
    const bool bCollected = Builder.CollectPrimitive(
        ParticleComponent,
        ShowFlags,
        EViewMode::Lit_BlinnPhong,
        RenderBus,
        MeshBufferManager);

    const TArray<FRenderCommand>& ParticleCommands = RenderBus.GetCommands(ERenderPass::Particle);
    uint32 SpriteInstanceCount = 0;
    bool bFoundSpriteCommand = false;
    for (const FRenderCommand& Command : ParticleCommands)
    {
        if (Command.SourcePrimitive != ParticleComponent)
        {
            continue;
        }

        if (Command.VertexFactoryType != EVertexFactoryType::SpriteParticle)
        {
            continue;
        }

        // Cycle 15a (D4): Particle 데이터는 단일 DynamicData* 슬롯으로 통합됨.
        // Sprite path 는 FDynamicSpriteEmitterData. ActiveParticleCount 가 sprite instance count 와 동일.
        if (Command.DynamicData)
        {
            const int32 ActiveCount = Command.DynamicData->GetSource().ActiveParticleCount;
            if (ActiveCount > 0)
            {
                bFoundSpriteCommand = true;
                SpriteInstanceCount += static_cast<uint32>(ActiveCount);
            }
        }
    }

    const int32 ParticleCommandCount = static_cast<int32>(ParticleCommands.size());
    MeshBufferManager.Release();

    if (!bCollected)
    {
        OutSummary = "primitive draw command builder rejected the particle component";
        return false;
    }

    if (!bFoundSpriteCommand)
    {
        char Buffer[192];
        snprintf(
            Buffer,
            sizeof(Buffer),
            "no sprite particle render command, particleCommands=%d",
            ParticleCommandCount);
        OutSummary = Buffer;
        return false;
    }

    char Buffer[192];
    snprintf(
        Buffer,
        sizeof(Buffer),
        "%s, commands=%d, spriteInstances=%u",
        bUsedSelection ? "selected" : "firstInWorld",
        ParticleCommandCount,
        SpriteInstanceCount);
    OutSummary = Buffer;
    return true;
}

} // namespace

void FEditorMainPanel::RenderUndoHistoryPanel(float DeltaTime)
{
    (void)DeltaTime;
    if (!PanelVisibility.bShowUndoHistory || !EditorEngine)
    {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(360.0f, 420.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Undo History", &PanelVisibility.bShowUndoHistory))
    {
        ImGui::End();
        return;
    }

    const FEditorUndoSystem& UndoSystem = EditorEngine->GetUndoSystem();
    const TArray<FUndoSnapshotEntry>& UndoEntries = UndoSystem.GetUndoHistory();
    const TArray<FUndoSnapshotEntry>& RedoEntries = UndoSystem.GetRedoHistory();
    const FUndoHistoryStats HistoryStats = UndoSystem.GetStats();

    const bool bCanUndo = !UndoEntries.empty();
    const bool bCanRedo = !RedoEntries.empty();
    ImGui::BeginDisabled(!bCanUndo);
    if (ImGui::Button("Undo", ImVec2(86.0f, 0.0f)))
    {
        EditorEngine->GetCommandSystem().Execute(EEditorCommand::Undo);
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(!bCanRedo);
    if (ImGui::Button("Redo", ImVec2(86.0f, 0.0f)))
    {
        EditorEngine->GetCommandSystem().Execute(EEditorCommand::Redo);
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(!bCanUndo && !bCanRedo);
    if (ImGui::Button("Clear", ImVec2(86.0f, 0.0f)))
    {
        EditorEngine->GetCommandSystem().Execute(EEditorCommand::ClearUndoHistory);
    }
    ImGui::EndDisabled();

    ImGui::Separator();
    if (ImGui::CollapsingHeader("Stat History", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("Entries: %d / %d", HistoryStats.UndoCount + HistoryStats.RedoCount, HistoryStats.MaxEntries);
        ImGui::TextDisabled("Undo %d, Redo %d", HistoryStats.UndoCount, HistoryStats.RedoCount);
        ImGui::Text("Snapshot Data: %s", FormatHistoryBytes(HistoryStats.LogicalBytes).c_str());
        ImGui::Text("Reserved Memory: %s", FormatHistoryBytes(HistoryStats.ReservedBytes).c_str());
        ImGui::TextDisabled("Approx Total: %s", FormatHistoryBytes(HistoryStats.ApproxTotalBytes).c_str());
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Approx Total = string reserved capacity + entry storage. Scene restore also creates a temporary world only while undo/redo is executing.");
        }
    }
    ImGui::Separator();
    ImGui::TextDisabled("Undo");
    ImGui::BeginChild("##UndoHistoryList", ImVec2(0.0f, ImGui::GetContentRegionAvail().y * 0.62f), true);
    if (UndoEntries.empty())
    {
        ImGui::TextDisabled("No undo history.");
    }
    else
    {
        for (int32 Index = static_cast<int32>(UndoEntries.size()) - 1; Index >= 0; --Index)
        {
            ImGui::PushID(Index);
            const FString Label = UndoEntries[Index].Label.empty() ? FString("Scene Edit") : UndoEntries[Index].Label;
            if (ImGui::Selectable(Label.c_str()))
            {
                FEditorCommandArgs Args;
                Args.HistoryIndex = Index;
                EditorEngine->GetCommandSystem().Execute(EEditorCommand::RestoreUndoHistoryIndex, Args);
            }
            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    ImGui::TextDisabled("Redo");
    ImGui::BeginChild("##RedoHistoryList", ImVec2(0.0f, 0.0f), true);
    if (RedoEntries.empty())
    {
        ImGui::TextDisabled("No redo history.");
    }
    else
    {
        for (int32 Index = static_cast<int32>(RedoEntries.size()) - 1; Index >= 0; --Index)
        {
            const FString Label = RedoEntries[Index].Label.empty() ? FString("Scene Edit") : RedoEntries[Index].Label;
            ImGui::TextUnformatted(Label.c_str());
        }
    }
    ImGui::EndChild();
    ImGui::End();
}

void FEditorMainPanel::RenderEditorDebugPanel(float DeltaTime)
{
    (void)DeltaTime;
    if (!PanelVisibility.bShowEditorDebug || !EditorEngine)
    {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(500.0f, 430.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Editor Debug", &PanelVisibility.bShowEditorDebug))
    {
        ImGui::End();
        return;
    }

    FEditorSettings& Settings = FEditorSettings::Get();
    if (ImGui::CollapsingHeader("Viewport", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::DragFloat("Camera Base Speed", &Settings.CameraSpeed, 0.1f, 0.1f, 100.0f, "%.1f");
        ImGui::DragFloat("Camera Rotate Speed", &Settings.CameraRotationSpeed, 1.0f, 1.0f, 720.0f, "%.0f");
        ImGui::DragFloat("Camera Zoom Speed", &Settings.CameraZoomSpeed, 1.0f, 10.0f, 5000.0f, "%.0f");
        ImGui::DragFloat("Dolly Speed Scale", &Settings.CameraDollySpeedScale, 0.01f, 0.05f, 5.0f, "%.2fx");
        ImGui::DragFloat("Pan Speed Scale", &Settings.CameraPanSpeedScale, 0.05f, 0.05f, 10.0f, "%.2fx");
        const char* PickingModeItems[] = { "ID Buffer", "Ray-Triangle" };
        int32 PickingModeIndex = static_cast<int32>(Settings.PickingMode);
        if (ImGui::Combo("Picking Mode", &PickingModeIndex, PickingModeItems, IM_ARRAYSIZE(PickingModeItems)))
        {
            if (PickingModeIndex >= 0 && PickingModeIndex < static_cast<int32>(EEditorPickingMode::Count))
            {
                Settings.PickingMode = static_cast<EEditorPickingMode>(PickingModeIndex);
            }
        }
        ImGui::Checkbox("Camera Smoothing", &Settings.bEnableCameraSmoothing);
        ImGui::BeginDisabled(!Settings.bEnableCameraSmoothing);
        ImGui::DragFloat("Move Smooth Speed", &Settings.CameraMoveSmoothSpeed, 0.05f, 0.1f, 40.0f, "%.2f");
        ImGui::DragFloat("Rotate Smooth Speed", &Settings.CameraRotateSmoothSpeed, 0.05f, 0.1f, 40.0f, "%.2f");
        ImGui::EndDisabled();

        FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();
        if (FEditorViewportClient* FocusedClient = Layout.GetViewportClient(Layout.GetLastFocusedViewportIndex()))
        {
            float SpeedMultiplier = GetDebugCameraSpeedMultiplier(FocusedClient);
            if (ImGui::DragFloat(
                "Focused Speed Multiplier",
                &SpeedMultiplier,
                0.05f,
                0.01f,
                MaxDebugCameraSpeedMultiplier,
                "%.2fx"))
            {
                SetDebugCameraSpeedMultiplier(FocusedClient, SpeedMultiplier);
            }
        }
    }

    if (ImGui::CollapsingHeader("Show Flags", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Primitives", &Settings.ShowFlags.bPrimitives);
        ImGui::Checkbox("Skeletal Mesh", &Settings.ShowFlags.bSkeletalMesh);
        ImGui::Checkbox("BillboardText", &Settings.ShowFlags.bBillboardText);
        ImGui::Checkbox("Axis", &Settings.ShowFlags.bAxis);
        ImGui::Checkbox("Grid", &Settings.ShowFlags.bGrid);
        ImGui::Checkbox("Gizmo", &Settings.ShowFlags.bGizmo);
        ImGui::Checkbox("Bounding Volume", &Settings.ShowFlags.bBoundingVolume);
        if (Settings.ShowFlags.bBoundingVolume)
        {
            ImGui::Indent();
            ImGui::Checkbox("BVH Bounding Volume", &Settings.ShowFlags.bBVHBoundingVolume);
            ImGui::Unindent();
        }
        ImGui::Checkbox("Enable LOD", &Settings.ShowFlags.bEnableLOD);
        ImGui::Checkbox("Decals", &Settings.ShowFlags.bDecals);
        ImGui::Checkbox("Fog", &Settings.ShowFlags.bFog);
        ImGui::Checkbox("Shadow", &Settings.ShowFlags.bShadow);
        ImGui::Checkbox("Gamma Correction", &Settings.ShowFlags.bGammaCorrection);
        if (Settings.ShowFlags.bGammaCorrection)
        {
            ImGui::Indent();
            ImGui::SliderFloat("Gamma", &Settings.ShowFlags.GammaValue, 1.0f, 3.0f, "%.2f");
            ImGui::Unindent();
        }
        ImGui::Checkbox("FXAA", &Settings.bEnableFXAA);
    }

    if (ImGui::CollapsingHeader("Particle", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::TextDisabled("Uses selected particle component, or first particle component in the focused world.");
        if (ImGui::Button("Runtime Smoke"))
        {
            FString Summary;
            const bool bPassed = RunSelectedParticleRuntimeSmoke(EditorEngine, Summary);
            FEditorConsoleWidget::AddLog(
                "Selected particle runtime smoke test %s: %s\n",
                bPassed ? "passed" : "failed",
                Summary.c_str());

            if (EditorEngine)
            {
                if (bPassed)
                {
                    EditorEngine->GetNotificationService().Info("Selected particle runtime smoke test passed");
                }
                else
                {
                    EditorEngine->GetNotificationService().Warning("Selected particle runtime smoke test failed");
                }
            }
        }

        if (ImGui::Button("Render Command Smoke"))
        {
            FString Summary;
            const bool bPassed = RunParticleRenderCommandSmoke(EditorEngine, Summary);
            FEditorConsoleWidget::AddLog(
                "Particle render command smoke test %s: %s\n",
                bPassed ? "passed" : "failed",
                Summary.c_str());

            if (EditorEngine)
            {
                if (bPassed)
                {
                    EditorEngine->GetNotificationService().Info("Particle render command smoke test passed");
                }
                else
                {
                    EditorEngine->GetNotificationService().Warning("Particle render command smoke test failed");
                }
            }
        }

        if (ImGui::Button("Run Particle Serialization Smoke Test"))
        {
            const FString SmokeTestPath = "Asset/Particle/SmokeTest.uasset";
            const bool bPassed = FResourceManager::Get().RunParticleSystemSerializationSmokeTest(SmokeTestPath);
            FEditorConsoleWidget::AddLog(
                "Particle serialization smoke test %s: %s\n",
                bPassed ? "passed" : "failed",
                SmokeTestPath.c_str());
        }
    }

    if (ImGui::CollapsingHeader("Place Actors (Grid)", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const int32 PrimitiveCount = Widgets.ControlWidget.GetPrimitiveTypeCount();
        DebugGridState.PrimitiveType = MathUtil::Clamp(DebugGridState.PrimitiveType, 0, PrimitiveCount - 1);

        if (ImGui::BeginCombo("Actor Type", Widgets.ControlWidget.GetPrimitiveTypeLabel(DebugGridState.PrimitiveType)))
        {
            for (int32 i = 0; i < PrimitiveCount; ++i)
            {
                const bool bSelected = (DebugGridState.PrimitiveType == i);
                if (ImGui::Selectable(Widgets.ControlWidget.GetPrimitiveTypeLabel(i), bSelected))
                {
                    DebugGridState.PrimitiveType = i;
                }
                if (bSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::DragInt("Rows", &DebugGridState.Rows, 1.0f, 1, 128, "%d");
        ImGui::DragInt("Cols", &DebugGridState.Cols, 1.0f, 1, 128, "%d");
        ImGui::DragInt("Layers", &DebugGridState.Layers, 1.0f, 1, 32, "%d");
        ImGui::DragFloat("Grid Spacing", &DebugGridState.Spacing, 0.1f, 0.1f, 1000.0f, "%.2f");
        ImGui::Checkbox("Center Grid Around Origin", &DebugGridState.bCenter);
        ImGui::DragFloat3("Origin", &DebugGridState.Origin.X, 0.1f, -100000.0f, 100000.0f, "%.2f");

        DebugGridState.Rows = MathUtil::Clamp(DebugGridState.Rows, 1, 128);
        DebugGridState.Cols = MathUtil::Clamp(DebugGridState.Cols, 1, 128);
        DebugGridState.Layers = MathUtil::Clamp(DebugGridState.Layers, 1, 32);
        DebugGridState.Spacing = std::max(0.1f, DebugGridState.Spacing);

        const int32 TotalActors = DebugGridState.Rows * DebugGridState.Cols * DebugGridState.Layers;
        ImGui::Text("Total Actors: %d", TotalActors);
        if (TotalActors > 2048)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.2f, 1.0f), "Large grid; spawn is capped at 2048 per click.");
        }

        if (ImGui::Button("Spawn Grid Actors"))
        {
            const float RowOffset = DebugGridState.bCenter ? static_cast<float>(DebugGridState.Rows - 1) * 0.5f : 0.0f;
            const float ColOffset = DebugGridState.bCenter ? static_cast<float>(DebugGridState.Cols - 1) * 0.5f : 0.0f;
            const float LayerOffset = DebugGridState.bCenter ? static_cast<float>(DebugGridState.Layers - 1) * 0.5f : 0.0f;
            const int32 SpawnLimit = std::min(TotalActors, 2048);
            int32 SpawnedCount = 0;

            for (int32 Layer = 0; Layer < DebugGridState.Layers && SpawnedCount < SpawnLimit; ++Layer)
            {
                for (int32 Row = 0; Row < DebugGridState.Rows && SpawnedCount < SpawnLimit; ++Row)
                {
                    for (int32 Col = 0; Col < DebugGridState.Cols && SpawnedCount < SpawnLimit; ++Col)
                    {
                        const FVector Location(
                            DebugGridState.Origin.X + (static_cast<float>(Col) - ColOffset) * DebugGridState.Spacing,
                            DebugGridState.Origin.Y + (static_cast<float>(Row) - RowOffset) * DebugGridState.Spacing,
                            DebugGridState.Origin.Z + (static_cast<float>(Layer) - LayerOffset) * DebugGridState.Spacing);
                        if (Widgets.ControlWidget.SpawnPrimitive(DebugGridState.PrimitiveType, Location, 1))
                        {
                            ++SpawnedCount;
                        }
                    }
                }
            }

            if (UWorld* World = EditorEngine->GetFocusedWorld())
            {
                World->RebuildSpatialIndex();
            }
            FEditorConsoleWidget::AddLog(
                "Editor Debug grid spawned %d %s actors\n",
                SpawnedCount,
                Widgets.ControlWidget.GetPrimitiveTypeLabel(DebugGridState.PrimitiveType));
        }
    }

    ImGui::End();
}
