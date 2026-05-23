#pragma once

#include "Editor/UI/EditorWidget.h"
#include "Editor/Viewport/FSceneViewport.h"
#include "Editor/Viewport/ParticleSystemViewportClient.h"
#include "ImGui/imgui.h"

class UParticleEmitter;
class UParticleModule;
class UParticleSystem;

class FEditorParticleSystemWidget : public FEditorWidget
{
public:
	~FEditorParticleSystemWidget() override;

	void Initialize(UEditorEngine* InEditorEngine) override;
	void Render(float DeltaTime) override;
	void RenderEmbedded(float DeltaTime);
	void RenderDetachedDocumentChrome(bool& bCloseRequested);
	void RenderDocumentToolbarControls();
	void Shutdown();

	void OpenLayoutTest(const FString& InDocumentPath = "");
	const FString& GetDocumentPath() const { return DocumentPath; }
	bool IsDirty() const { return bDirty; }
	bool IsPreviewViewportVisible() const { return bPreviewViewportVisible; }
	bool HasValidPreviewViewportRect() const { return bPreviewViewportRectValid; }
	FSceneViewport* GetPreviewViewport() { return bPreviewViewportInitialized ? &PreviewViewport : nullptr; }
	const FSceneViewport* GetPreviewViewport() const { return bPreviewViewportInitialized ? &PreviewViewport : nullptr; }
	FParticleSystemViewportClient* GetPreviewClient() { return bPreviewViewportInitialized ? &PreviewClient : nullptr; }
	const FParticleSystemViewportClient* GetPreviewClient() const { return bPreviewViewportInitialized ? &PreviewClient : nullptr; }

private:
	void EnsurePreviewViewport();
	void ShutdownPreviewViewport();
	void DrawMainLayout();
	void DrawViewportPanel(const ImVec2& Size);
	void DrawViewportMenuBar(const ImVec2& CanvasMin);
	void DrawEmittersPanel(const ImVec2& Size);
	void DrawEmitterContextMenu();
	void AddDefaultEmitter();
	void DeleteSelectedEmitter();
	void DeleteEmitter(int32 EmitterIndex);
	void ApplyPendingReorders();
	void ReorderEmitter(int32 SourceIndex, int32 InsertIndex);
	void ReorderModule(int32 EmitterIndex, int32 SourceModuleIndex, int32 InsertIndex);
	void DrawEmitterColumn(UParticleEmitter* Emitter, int32 EmitterIndex, float ColumnHeight);
	void DrawEmitterModuleRow(UParticleModule* Module, int32 EmitterIndex, int32 ModuleIndex, bool bRequired, float RowHeight);
	void DrawDetailsPanel(const ImVec2& Size);
	void DrawCurveEditorPanel(const ImVec2& Size);

	FSceneViewport PreviewViewport;
	FParticleSystemViewportClient PreviewClient;
	UParticleSystem* ParticleSystemAsset = nullptr;
	FName PreviewWorldHandle = FName::None;
	FString DocumentPath;
	bool bDirty = true;
	bool bShowThumbnail = false;
	bool bShowBounds = true;
	bool bShowOriginAxis = true;
	bool bPreviewViewportInitialized = false;
	bool bPreviewViewportVisible = false;
	bool bPreviewViewportRectValid = false;
	int32 CurrentLOD = 0;
	int32 SelectedEmitterIndex = 0;
	int32 SelectedModuleIndex = -1;
	int32 ContextEmitterIndex = -1;
	int32 PendingEmitterMoveSource = -1;
	int32 PendingEmitterMoveInsertIndex = -1;
	int32 PendingModuleMoveEmitterIndex = -1;
	int32 PendingModuleMoveSource = -1;
	int32 PendingModuleMoveInsertIndex = -1;
	float TopAreaHeight = 0.0f;
	float TopLeftWidth = 0.0f;
	float BottomLeftWidth = 0.0f;
};
