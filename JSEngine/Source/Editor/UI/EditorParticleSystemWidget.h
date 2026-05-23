#pragma once

#include "Editor/UI/EditorWidget.h"
#include "ImGui/imgui.h"

class FEditorParticleSystemWidget : public FEditorWidget
{
public:
	void Initialize(UEditorEngine* InEditorEngine) override;
	void Render(float DeltaTime) override;
	void RenderEmbedded(float DeltaTime);
	void RenderDocumentToolbarControls();

	void OpenLayoutTest(const FString& InDocumentPath = "");
	const FString& GetDocumentPath() const { return DocumentPath; }
	bool IsDirty() const { return bDirty; }

private:
	void DrawMainLayout();
	void DrawViewportPanel(const ImVec2& Size);
	void DrawEmittersPanel(const ImVec2& Size);
	void DrawDetailsPanel(const ImVec2& Size);
	void DrawCurveEditorPanel(const ImVec2& Size);

	FString DocumentPath;
	bool bDirty = true;
	bool bShowThumbnail = false;
	bool bShowBounds = true;
	bool bShowOriginAxis = true;
	int32 CurrentLOD = 0;
	float TopAreaHeight = 0.0f;
	float TopLeftWidth = 0.0f;
	float BottomLeftWidth = 0.0f;
};
