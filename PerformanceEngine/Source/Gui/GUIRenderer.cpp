#include "Gui/GUIRenderer.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "Camera/Camera.h"
#include "Math/Quat.h"
#include "Graphics/D3D11/D3D11RHI.h"
#include "Math/Rotator.h"
#include "Picking/PickingSystem.h"
#include "Scene/Scene.h"
#include "Scene/SceneTypes.h"
#include "StaticMesh/StaticMesh.h"
#include "Types/PathUtils.h"
#include "ThirdParty/ImGui/imgui.h"
#include "ThirdParty/ImGui/imgui_impl_dx11.h"
#include "ThirdParty/ImGui/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace
{
	constexpr float MinSpawnDistance = 10.0f;
	constexpr float MaxSpawnDistance = 150.0f;
	constexpr float DefaultSpawnDistance = 10.0f;

	bool IsMouseMessage(UINT InMessage)
	{
		switch (InMessage)
		{
		case WM_MOUSEMOVE:
		case WM_MOUSEWHEEL:
		case WM_MOUSEHWHEEL:
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_LBUTTONDBLCLK:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_RBUTTONDBLCLK:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_MBUTTONDBLCLK:
		case WM_XBUTTONDOWN:
		case WM_XBUTTONUP:
		case WM_XBUTTONDBLCLK:
			return true;
		default:
			return false;
		}
	}

	bool IsKeyboardMessage(UINT InMessage)
	{
		switch (InMessage)
		{
		case WM_KEYDOWN:
		case WM_KEYUP:
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_CHAR:
		case WM_SYSCHAR:
			return true;
		default:
			return false;
		}
	}

	void DrawVector3(const char* InLabel, const FVector& InValue)
	{
		ImGui::Text("%s: %.3f, %.3f, %.3f", InLabel, InValue.X, InValue.Y, InValue.Z);
	}

	bool BeginPropertyTable(const char* InTableId)
	{
		return ImGui::BeginTable(
			InTableId,
			2,
			ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_PadOuterX);
	}

	void SetupPropertyTable()
	{
		ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 130.0f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
	}

	void DrawPropertyLabel(const char* InLabel)
	{
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(InLabel);
		ImGui::TableSetColumnIndex(1);
	}

	void DrawTextProperty(const char* InLabel, const char* InValue)
	{
		DrawPropertyLabel(InLabel);
		ImGui::TextWrapped("%s", InValue);
	}

	void DrawIntProperty(const char* InLabel, int32 InValue)
	{
		DrawPropertyLabel(InLabel);
		ImGui::Text("%d", InValue);
	}

	void DrawUIntProperty(const char* InLabel, uint32 InValue)
	{
		DrawPropertyLabel(InLabel);
		ImGui::Text("%u", InValue);
	}

	void DrawBoolProperty(const char* InLabel, bool bInValue)
	{
		DrawPropertyLabel(InLabel);
		ImGui::TextUnformatted(bInValue ? "True" : "False");
	}

	void DrawFloatProperty(const char* InLabel, float InValue, const char* InFormat = "%.3f")
	{
		DrawPropertyLabel(InLabel);
		ImGui::Text(InFormat, InValue);
	}

	void DrawVector3Property(const char* InLabel, const FVector& InValue)
	{
		DrawPropertyLabel(InLabel);
		ImGui::Text("%.3f, %.3f, %.3f", InValue.X, InValue.Y, InValue.Z);
	}

	bool DrawFloat3EditorProperty(const char* InLabel, const char* InWidgetId, float InOutValues[3], float InSpeed, const char* InFormat = "%.3f")
	{
		DrawPropertyLabel(InLabel);
		ImGui::SetNextItemWidth(-FLT_MIN);
		return ImGui::DragFloat3(InWidgetId, InOutValues, InSpeed, 0.0f, 0.0f, InFormat);
	}

	float SanitizeScaleComponent(float InValue)
	{
		if (std::fabs(InValue) >= 1.e-3f)
		{
			return InValue;
		}

		return (InValue < 0.0f) ? -1.e-3f : 1.e-3f;
	}

	FVector SanitizeScale(const FVector& InScale)
	{
		return FVector(
			SanitizeScaleComponent(InScale.X),
			SanitizeScaleComponent(InScale.Y),
			SanitizeScaleComponent(InScale.Z));
	}
}

FGUIRenderer::FGUIRenderer() = default;

FGUIRenderer::~FGUIRenderer()
{
	Shutdown();
}

bool FGUIRenderer::Initialize(FD3D11RHI& InRHI, HWND InWindowHandle)
{
	if (bInitialized)
	{
		return true;
	}

	ID3D11Device* Device = InRHI.GetDevice();
	ID3D11DeviceContext* DeviceContext = InRHI.GetDeviceContext();
	if (Device == nullptr || DeviceContext == nullptr || InWindowHandle == nullptr)
	{
		return false;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGui::GetIO();

	ImGui::StyleColorsDark();

	if (!ImGui_ImplWin32_Init(InWindowHandle))
	{
		ImGui::DestroyContext();
		return false;
	}

	if (!ImGui_ImplDX11_Init(Device, DeviceContext))
	{
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		return false;
	}

	bInitialized = true;
	return true;
}

void FGUIRenderer::Shutdown()
{
	if (!bInitialized)
	{
		return;
	}

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	bInitialized = false;
	bWantsMouseCapture = false;
	bWantsKeyboardCapture = false;
}

bool FGUIRenderer::HandleMessage(HWND InWindowHandle, UINT InMessage, WPARAM InWParam, LPARAM InLParam)
{
	if (!bInitialized || ImGui::GetCurrentContext() == nullptr)
	{
		return false;
	}

	const bool bBackendHandled = ImGui_ImplWin32_WndProcHandler(InWindowHandle, InMessage, InWParam, InLParam) != 0;
	const ImGuiIO& IO = ImGui::GetIO();

	if (bBackendHandled)
	{
		return true;
	}

	if (IsMouseMessage(InMessage) && IO.WantCaptureMouse)
	{
		return true;
	}

	if (IsKeyboardMessage(InMessage) && IO.WantCaptureKeyboard)
	{
		return true;
	}

	return false;
}

bool FGUIRenderer::Render(const FD3D11RHI& InRHI, FScene& InScene, const FCamera& InCamera, FPickState& InOutPickState)
{
	if (!bInitialized)
	{
		return false;
	}

	ID3D11DeviceContext* DeviceContext = InRHI.GetDeviceContext();
	if (DeviceContext == nullptr)
	{
		return false;
	}

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	const bool bSceneChanged = DrawSceneWindow(InScene, InCamera, InOutPickState) | DrawSelectedObjectWindow(InScene, InOutPickState);

	ImGui::Render();

	ID3D11RenderTargetView* RenderTargets[] = { InRHI.GetBackBufferRTV() };
	DeviceContext->OMSetRenderTargets(1, RenderTargets, InRHI.GetDepthStencilView());

	const D3D11_VIEWPORT Viewport = InRHI.GetViewport();
	DeviceContext->RSSetViewports(1, &Viewport);

	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	const ImGuiIO& IO = ImGui::GetIO();
	bWantsMouseCapture = IO.WantCaptureMouse;
	bWantsKeyboardCapture = IO.WantCaptureKeyboard;

	return bSceneChanged;
}

bool FGUIRenderer::WantsMouseCapture() const
{
	return bWantsMouseCapture;
}

bool FGUIRenderer::WantsKeyboardCapture() const
{
	return bWantsKeyboardCapture;
}

bool FGUIRenderer::DrawSceneWindow(FScene& InScene, const FCamera& InCamera, FPickState& InOutPickState)
{
	ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(420.0f, 128.0f), ImGuiCond_FirstUseEver);

	bool bSceneChanged = false;
	if (!ImGui::Begin("Scene"))
	{
		ImGui::End();
		return false;
	}

	TArray<FString> LoadedMeshAssetKeys = InScene.GetLoadedMeshAssetKeys();
	std::sort(LoadedMeshAssetKeys.begin(), LoadedMeshAssetKeys.end());

	ImGui::Text("Loaded StaticMeshes: %d", static_cast<int32>(LoadedMeshAssetKeys.size()));

	if (!LoadedMeshAssetKeys.empty())
	{
		const auto SelectedMeshIt = std::find(LoadedMeshAssetKeys.begin(), LoadedMeshAssetKeys.end(), SceneWindowSelectedMeshAssetKey);
		if (SelectedMeshIt == LoadedMeshAssetKeys.end())
		{
			SceneWindowSelectedMeshAssetKey = LoadedMeshAssetKeys.front();
			SceneWindowSelectedMeshIndex = 0;
		}
		else
		{
			SceneWindowSelectedMeshIndex = static_cast<int32>(std::distance(LoadedMeshAssetKeys.begin(), SelectedMeshIt));
		}

		if (ImGui::BeginCombo("StaticMesh", SceneWindowSelectedMeshAssetKey.c_str()))
		{
			for (int32 MeshIndex = 0; MeshIndex < static_cast<int32>(LoadedMeshAssetKeys.size()); ++MeshIndex)
			{
				const bool bSelected = (MeshIndex == SceneWindowSelectedMeshIndex);
				if (ImGui::Selectable(LoadedMeshAssetKeys[MeshIndex].c_str(), bSelected))
				{
					SceneWindowSelectedMeshIndex = MeshIndex;
					SceneWindowSelectedMeshAssetKey = LoadedMeshAssetKeys[MeshIndex];
				}

				if (bSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}

			ImGui::EndCombo();
		}

		if (ImGui::Button("Add"))
		{
			float SpawnDistance = DefaultSpawnDistance;
			if (const std::shared_ptr<FStaticMesh> StaticMesh = InScene.FindLoadedStaticMesh(SceneWindowSelectedMeshAssetKey))
			{
				const float MeshBoundsDiagonal = (StaticMesh->GetBoundsMax() - StaticMesh->GetBoundsMin()).Size();
				SpawnDistance = std::clamp(MeshBoundsDiagonal * 0.5f, MinSpawnDistance, MaxSpawnDistance);
			}

			const FVector CameraForward = InCamera.GetRotation().GetForwardVector().GetSafeNormal();
			const FVector SpawnLocation = InCamera.GetLocation() + CameraForward * SpawnDistance;
			const FTransform SpawnTransform(FQuat::Identity, SpawnLocation, FVector::OneVector);

			int32 NewPrimitiveIndex = -1;
			if (InScene.AddLoadedStaticMeshInstance(SceneWindowSelectedMeshAssetKey, SpawnTransform, NewPrimitiveIndex))
			{
				InOutPickState.bHit = false;
				InOutPickState.SelectedPrimitiveIndex = NewPrimitiveIndex;
				InOutPickState.SelectedPrimitiveId =
					(static_cast<size_t>(NewPrimitiveIndex) < InScene.GetPrimitiveRuntimeData().size())
					? InScene.GetPrimitiveRuntimeData()[NewPrimitiveIndex].PrimitiveId
					: -1;
				InOutPickState.HitWorldPosition = SpawnLocation;
				InOutPickState.TraversedWorldBVHNodes.clear();
				bSceneChanged = true;
			}
		}
	}
	else
	{
		ImGui::TextDisabled("No loaded meshes are available.");
	}

	const bool bHasSelection =
		InOutPickState.SelectedPrimitiveIndex >= 0
		&& static_cast<size_t>(InOutPickState.SelectedPrimitiveIndex) < InScene.GetRenderItems().size();

	if (!bHasSelection)
	{
		ImGui::BeginDisabled();
	}

	if (ImGui::Button("Delete Selected") && bHasSelection)
	{
		if (InScene.RemovePrimitiveFromScene(InOutPickState.SelectedPrimitiveIndex))
		{
			InOutPickState.bHit = false;
			InOutPickState.SelectedPrimitiveId = -1;
			InOutPickState.SelectedPrimitiveIndex = -1;
			InOutPickState.HitWorldPosition = FVector::ZeroVector;
			InOutPickState.TraversedWorldBVHNodes.clear();
			bSceneChanged = true;
		}
	}

	if (!bHasSelection)
	{
		ImGui::EndDisabled();
	}

	ImGui::End();
	return bSceneChanged;
}

bool FGUIRenderer::DrawSelectedObjectWindow(FScene& InScene, const FPickState& InPickState)
{
	ImGui::SetNextWindowPos(ImVec2(16.0f, 160.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(380.0f, 360.0f), ImGuiCond_FirstUseEver);

	bool bSceneChanged = false;

	if (!ImGui::Begin("Details"))
	{
		ImGui::End();
		return false;
	}

	if (InPickState.SelectedPrimitiveIndex < 0
		|| static_cast<size_t>(InPickState.SelectedPrimitiveIndex) >= InScene.GetRenderItems().size()
		|| static_cast<size_t>(InPickState.SelectedPrimitiveIndex) >= InScene.GetPrimitiveRuntimeData().size())
	{
		ImGui::TextUnformatted("No object selected");
		ImGui::Spacing();
		ImGui::TextUnformatted("Select an object in the viewport to inspect its properties.");
		ImGui::End();
		return false;
	}

	const int32 PrimitiveIndex = InPickState.SelectedPrimitiveIndex;
	const FRenderItem& RenderItem = InScene.GetRenderItems()[PrimitiveIndex];
	const FScenePrimitiveRuntimeData& RuntimeData = InScene.GetPrimitiveRuntimeData()[PrimitiveIndex];
	const FTransform& Transform = RenderItem.Transform;
	const FRotator Rotation = Transform.Rotator().GetNormalized();
	const std::shared_ptr<FStaticMesh>& StaticMesh = RenderItem.StaticMesh;

	ImGui::Text("Primitive %d", RuntimeData.PrimitiveId);
	ImGui::TextDisabled("Index %d", PrimitiveIndex);
	ImGui::Spacing();

	if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
	{
		FTransform EditedTransform = Transform;

		float LocationValues[3] =
		{
			Transform.GetLocation().X,
			Transform.GetLocation().Y,
			Transform.GetLocation().Z,
		};

		float RotationValues[3] =
		{
			Rotation.Roll,
			Rotation.Pitch,
			Rotation.Yaw,
		};

		float ScaleValues[3] =
		{
			Transform.GetScale3D().X,
			Transform.GetScale3D().Y,
			Transform.GetScale3D().Z,
		};

		bool bTransformEdited = false;

		if (BeginPropertyTable("TransformProperties"))
		{
			SetupPropertyTable();

			if (DrawFloat3EditorProperty("Location", "##Location", LocationValues, 0.1f))
			{
				EditedTransform.SetLocation(FVector(LocationValues[0], LocationValues[1], LocationValues[2]));
				bTransformEdited = true;
			}

			if (DrawFloat3EditorProperty("Rotation", "##Rotation", RotationValues, 0.25f))
			{
				EditedTransform.SetRotation(FQuat::MakeFromEuler(FVector(RotationValues[0], RotationValues[1], RotationValues[2])));
				bTransformEdited = true;
			}

			if (DrawFloat3EditorProperty("Scale", "##Scale", ScaleValues, 0.01f))
			{
				EditedTransform.SetScale3D(SanitizeScale(FVector(ScaleValues[0], ScaleValues[1], ScaleValues[2])));
				bTransformEdited = true;
			}

			ImGui::EndTable();
		}

		if (bTransformEdited)
		{
			bSceneChanged |= InScene.SetPrimitiveTransformWorld(PrimitiveIndex, EditedTransform);
		}
	}

	if (ImGui::CollapsingHeader("Picking", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (BeginPropertyTable("PickingProperties"))
		{
			SetupPropertyTable();
			DrawBoolProperty("Hit", InPickState.bHit);
			DrawFloatProperty("Last Pick (ms)", static_cast<float>(InPickState.LastPickTimeMsWorldBVH), "%.4f");
			DrawVector3Property("Hit Position", InPickState.HitWorldPosition);
			DrawIntProperty("Primitive ID", RuntimeData.PrimitiveId);
			DrawIntProperty("Primitive Index", PrimitiveIndex);
			ImGui::EndTable();
		}
	}

	if (ImGui::CollapsingHeader("Bounds", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (BeginPropertyTable("BoundsProperties"))
		{
			SetupPropertyTable();
			DrawVector3Property("World Min", RuntimeData.WorldBoundsMin);
			DrawVector3Property("World Max", RuntimeData.WorldBoundsMax);
			ImGui::EndTable();
		}
	}

	if (ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (BeginPropertyTable("MeshProperties"))
		{
			SetupPropertyTable();
			DrawTextProperty("Asset", RenderItem.MeshAssetPath.c_str());

			if (StaticMesh)
			{
				DrawUIntProperty("Vertices", StaticMesh->GetVertexCount());
				DrawUIntProperty("Indices", StaticMesh->GetIndexCount());
				DrawIntProperty("Sections", static_cast<int32>(StaticMesh->GetSections().size()));
				DrawIntProperty("Materials", static_cast<int32>(StaticMesh->GetMaterials().size()));

				const std::string SourcePath = PathUtils::PathToUtf8(StaticMesh->GetSourcePath());
				DrawTextProperty("Source", SourcePath.empty() ? "-" : SourcePath.c_str());
			}
			else
			{
				DrawTextProperty("Resource", "Unavailable");
			}

			ImGui::EndTable();
		}
	}

	ImGui::End();
	return bSceneChanged;
}
