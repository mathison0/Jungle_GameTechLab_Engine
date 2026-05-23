#pragma once

#include "Editor/UI/EditorWidget.h"
#include "Editor/Viewport/FSceneViewport.h"
#include "Editor/Viewport/ParticleSystemViewportClient.h"
#include "ImGui/imgui.h"

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
	void DrawDetailsPanel(const ImVec2& Size);
	void DrawCurveEditorPanel(const ImVec2& Size);

	FSceneViewport PreviewViewport;
	FParticleSystemViewportClient PreviewClient;
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
	float TopAreaHeight = 0.0f;
	float TopLeftWidth = 0.0f;
	float BottomLeftWidth = 0.0f;
};
