#include "Editor/UI/EditorParticleSystemWidget.h"

#include "ImGui/imgui.h"

void FEditorParticleSystemWidget::Initialize(UEditorEngine* InEditorEngine)
{
	FEditorWidget::Initialize(InEditorEngine);
}

void FEditorParticleSystemWidget::Render(float DeltaTime)
{
	RenderEmbedded(DeltaTime);
}

void FEditorParticleSystemWidget::RenderEmbedded(float DeltaTime)
{
	(void)DeltaTime;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 12.0f));
	ImGui::TextUnformatted("Particle Editor");
	ImGui::Separator();
	ImGui::Spacing();
	ImGui::TextDisabled("Layout shell is ready. Toolbar, viewport, emitters, details, and curve editor panels will be added in the next steps.");
	ImGui::Text("Document: %s", DocumentPath.empty() ? "Asset_Nmae" : DocumentPath.c_str());
	ImGui::PopStyleVar();
}

void FEditorParticleSystemWidget::OpenLayoutTest(const FString& InDocumentPath)
{
	DocumentPath = InDocumentPath.empty() ? "Asset_Nmae" : InDocumentPath;
	bDirty = true;
}
