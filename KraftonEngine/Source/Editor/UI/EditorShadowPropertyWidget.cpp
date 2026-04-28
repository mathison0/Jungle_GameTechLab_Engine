#include "EditorShadowPropertyWidget.h"
#include "Editor/EditorEngine.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "Engine/Runtime/Engine.h"
#include "imgui.h"
#include "Engine/Component/Light/PointLightComponent.h"

void FEditorShadowPropertyWidget::ShowShadowProperty(ULightComponent* LightComponent)
{
	if (CurrentShowLightComponent != LightComponent)
	{
		CurrentShowLightComponent = LightComponent;
	}

	if (!ImGui::Begin("Where there is light, there is also shadow."))
	{
		ImGui::End();
		return;
	}

	ShowShadowMapPropertWindow();
	ImGui::End();
}

void FEditorShadowPropertyWidget::ShowShadowMapPropertWindow()
{
	if (!CurrentShowLightComponent)
	{
		ImGui::TextUnformatted("No light selected.");
		return;
	}

	if (UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
	{
		if (FLevelEditorViewportClient* ActiveViewport = EditorEngine->GetActiveViewport())
		{
			FViewportRenderOptions& RenderOptions = ActiveViewport->GetRenderOptions();
			bool bOverrideCamera = RenderOptions.bOverrideCameraWithSelectedLight;
			if (ImGui::Checkbox("Override camera with light's perspective", &bOverrideCamera))
			{
				RenderOptions.bOverrideCameraWithSelectedLight = bOverrideCamera;
			}

			if (CurrentShowLightComponent->GetClass() == UPointLightComponent::StaticClass())
			{
				static constexpr const char* FaceLabels[] = { "+X", "-X", "+Y", "-Y", "+Z", "-Z" };
				RenderOptions.PointLightPreviewFaceIndex = RenderOptions.PointLightPreviewFaceIndex < static_cast<uint32>(std::size(FaceLabels))
					? RenderOptions.PointLightPreviewFaceIndex
					: 0u;

				if (ImGui::BeginCombo("Point Light Preview Face", FaceLabels[RenderOptions.PointLightPreviewFaceIndex]))
				{
					for (uint32 FaceIndex = 0; FaceIndex < static_cast<uint32>(std::size(FaceLabels)); ++FaceIndex)
					{
						const bool bSelected = RenderOptions.PointLightPreviewFaceIndex == FaceIndex;
						if (ImGui::Selectable(FaceLabels[FaceIndex], bSelected))
						{
							RenderOptions.PointLightPreviewFaceIndex = FaceIndex;
						}
						if (bSelected)
						{
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
				}
			}

			ImGui::BeginDisabled(true);
			ImGui::Text("Shadow Filter: %s",
				RenderOptions.ShadowFilterMode == EShadowFilterMode::VSM ? "VSM" : "PCF");
			ImGui::EndDisabled();
		}
		else
		{
			ImGui::TextDisabled("Shadow viewport settings unavailable without an active viewport.");
		}
	}

	ImGui::Separator();

	if (ImGui::RadioButton("Selected Light ShadowMap", PreviewMode == EShadowPreviewMode::SelectedLight))
	{
		PreviewMode = EShadowPreviewMode::SelectedLight;
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("Atlas Texture Layer", PreviewMode == EShadowPreviewMode::AtlasLayer))
	{
		PreviewMode = EShadowPreviewMode::AtlasLayer;
	}

	FTextureAtlasPool& AtlasPool = FTextureAtlasPool::Get();
	const uint32 AllocatedLayerCount = AtlasPool.GetAllocatedLayerCount();
	const int32 MaxLayerIndex = AllocatedLayerCount > 0 ? static_cast<int32>(AllocatedLayerCount - 1) : 0;

	if (PreviewAtlasLayerIndex < 0)
	{
		PreviewAtlasLayerIndex = 0;
	}
	else if (PreviewAtlasLayerIndex > MaxLayerIndex)
	{
		PreviewAtlasLayerIndex = MaxLayerIndex;
	}

	if (PreviewMode == EShadowPreviewMode::AtlasLayer)
	{
		ImGui::SliderInt("Atlas Layer", &PreviewAtlasLayerIndex, 0, MaxLayerIndex);
	}

	ID3D11ShaderResourceView* PreviewSRV = nullptr;
	if (PreviewMode == EShadowPreviewMode::SelectedLight)
	{
		FShadowHandleSet* Handle = CurrentShowLightComponent->GetShadowHandleSet();
		PreviewSRV = Handle ? AtlasPool.GetDebugSRV(Handle) : nullptr;
		if (!PreviewSRV)
		{
			ImGui::TextUnformatted("No shadow map for selected light.");
			return;
		}
	}
	else
	{
		PreviewSRV = AtlasPool.GetDebugLayerSRV(static_cast<uint32>(PreviewAtlasLayerIndex));
		//PreviewSRV = AtlasPool.GetSliceSRV(PreviewAtlasLayerIndex);
		if (!PreviewSRV)
		{
			ImGui::TextUnformatted("Atlas layer preview unavailable.");
			return;
		}
	}

	ImGui::Image(PreviewSRV, ImVec2(500, 500));
}
