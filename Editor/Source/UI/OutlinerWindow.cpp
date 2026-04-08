#include "OutlinerWindow.h"

#include "imgui.h"
#include "EditorEngine.h"
#include "Core/ShowFlags.h"
#include "Core/ViewportClient.h"
#include "Scene/Scene.h"
#include "Actor/Actor.h"
#include "Component/ActorComponent.h"
#include "Component/BillboardComponent.h"
#include "Component/SubUVComponent.h"
#include "Component/TextRenderComponent.h"
#include "Component/UUIDTextRenderComponent.h"

void FOutlinerWindow::Render(FEditorEngine* Engine)
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
	const bool bOpen = ImGui::Begin("Outliner");
	ImGui::PopStyleVar();
	if (!bOpen)
	{
		ImGui::End();
		return;
	}
	if (!Engine || !Engine->GetScene())
	{
		ImGui::End();
		return;
	}


	AActor* SelectedActor = Engine->GetSelectedActor();
	UActorComponent* SelectedComponent = Engine->GetSelectedComponent();

	ImGui::SeparatorText("Actors");

	UScene* Scene = Engine->GetScene();
	const TArray<AActor*>& Actors = Scene->GetActors();
	

	for (AActor* Actor : Actors)
	{
;
		if (!Actor || Actor->IsPendingDestroy())
		{
			continue;
		}

		ImGui::PushID(Actor);
		bool bVisible = Actor->IsVisible();
		if (ImGui::Checkbox("##visible", &bVisible))
		{
			Actor->SetVisible(bVisible);
		}
		ImGui::SameLine();

		const bool bActorSelected = (Actor == SelectedActor) && (SelectedComponent == nullptr);
		const ImGuiTreeNodeFlags TreeFlags =
			ImGuiTreeNodeFlags_OpenOnArrow |
			ImGuiTreeNodeFlags_OpenOnDoubleClick |
			(Actor->GetComponents().empty() ? ImGuiTreeNodeFlags_Leaf : 0) |
			(bActorSelected ? ImGuiTreeNodeFlags_Selected : 0);

		const bool bOpenNode = ImGui::TreeNodeEx("##ActorNode", TreeFlags, "%s", Actor->GetName().c_str());
		if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
		{
			Engine->SetSelectedActor(Actor);
		}

		if (bOpenNode)
		{
			for (UActorComponent* Component : Actor->GetComponents())
			{
				if (!Component)
				{
					continue;
				}

				ImGui::PushID(Component);
				const bool bComponentSelected = (Component == SelectedComponent);
				if (ImGui::Selectable(Component->GetName().c_str(), bComponentSelected))
				{
					Engine->SetSelectedComponent(Component);
				}
				ImGui::PopID();
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	}

	ImGui::End();

}
