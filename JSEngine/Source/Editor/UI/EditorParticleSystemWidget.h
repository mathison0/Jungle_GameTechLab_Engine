#pragma once

#include "Editor/UI/EditorWidget.h"

class FEditorParticleSystemWidget : public FEditorWidget
{
public:
	void Initialize(UEditorEngine* InEditorEngine) override;
	void Render(float DeltaTime) override;
	void RenderEmbedded(float DeltaTime);

	void OpenLayoutTest(const FString& InDocumentPath = "");
	const FString& GetDocumentPath() const { return DocumentPath; }
	bool IsDirty() const { return bDirty; }

private:
	FString DocumentPath;
	bool bDirty = true;
};
