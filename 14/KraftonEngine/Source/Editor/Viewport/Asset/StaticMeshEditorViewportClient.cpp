#include "StaticMeshEditorViewportClient.h"

#include "Component/Primitive/StaticMeshComponent.h"
#include "GameFramework/World.h"
#include "Mesh/Static/StaticMesh.h"
#include "Physics/BodySetup.h"
#include "Render/Scene/FScene.h"
#include "Input/InputSystem.h"
#include "Math/MathUtils.h"
#include "Physics/PhysicsGeometry.h"
#include "Render/Types/MinimalViewInfo.h"
#include "Settings/EditorSettings.h"
#include "Slate/SlateApplication.h"
#include "Viewport/Viewport.h"

#include <cmath>
#include <imgui.h>

namespace
{
	void DrawDebugBoxElem(FScene& Scene, const FMatrix& LocalToWorld, const FKBoxElem& Box, const FColor& Color)
	{
		const FVector Signs[8] = {
			FVector(-1.0f, -1.0f, -1.0f), FVector( 1.0f, -1.0f, -1.0f),
			FVector( 1.0f,  1.0f, -1.0f), FVector(-1.0f,  1.0f, -1.0f),
			FVector(-1.0f, -1.0f,  1.0f), FVector( 1.0f, -1.0f,  1.0f),
			FVector( 1.0f,  1.0f,  1.0f), FVector(-1.0f,  1.0f,  1.0f)
		};

		FVector Corners[8];
		for (int32 Index = 0; Index < 8; ++Index)
		{
			const FVector LocalCorner(
				Signs[Index].X * Box.Extents.X,
				Signs[Index].Y * Box.Extents.Y,
				Signs[Index].Z * Box.Extents.Z);
			Corners[Index] = LocalToWorld.TransformPositionWithW(Box.Center + Box.Rotation.RotateVector(LocalCorner));
		}

		const int32 Edges[12][2] = {
			{0, 1}, {1, 2}, {2, 3}, {3, 0},
			{4, 5}, {5, 6}, {6, 7}, {7, 4},
			{0, 4}, {1, 5}, {2, 6}, {3, 7}
		};

		for (const int32(&Edge)[2] : Edges)
		{
			Scene.AddDebugLine(Corners[Edge[0]], Corners[Edge[1]], Color);
		}
	}
}

void FStaticMeshEditorViewportClient::Initialize(ID3D11Device* Device, uint32 Width, uint32 Height)
{
	Viewport = new FViewport();
	Viewport->Initialize(Device, Width, Height);
	Viewport->SetClient(this);

	bIsRenderable = true;
}

void FStaticMeshEditorViewportClient::Release()
{
	if (Viewport)
	{
		Viewport->Release();
		delete Viewport;
		Viewport = nullptr;
	}

	PreviewWorld = nullptr;
	PreviewActor = nullptr;
	PreviewMeshComponent = nullptr;
	bIsRenderable = false;
}

void FStaticMeshEditorViewportClient::ResetCameraToPreviewBounds()
{
	FBoundingBox Bounds = PreviewMeshComponent
		? PreviewMeshComponent->GetWorldBoundingBox()
		: FBoundingBox(FVector(-0.5f, -0.5f, -0.5f), FVector(0.5f, 0.5f, 0.5f));

	FVector Center = Bounds.GetCenter();
	float Radius = Bounds.GetExtent().Length();
	if (Radius < 0.1f)
	{
		Radius = 1.0f;
	}

	const float FovRadians = ViewTransform.FOV;
	const float Distance = Radius / std::tan(FovRadians * 0.5f) * 1.25f;
	const FVector ViewDir = FVector(-1.0f, -1.0f, -0.6f).Normalized();

	ViewTransform.ViewLocation = Center - ViewDir * Distance;
	ViewTransform.LookAt(Center);

	TargetLocation = ViewTransform.ViewLocation;
	LastAppliedCameraLocation = ViewTransform.ViewLocation;
	bTargetLocationInitialized = true;
	bLastAppliedCameraLocationInitialized = true;
}

bool FStaticMeshEditorViewportClient::IsMouseOverViewport() const
{
	if (!bIsRenderable || ViewportScreenRect.Width <= 0.0f || ViewportScreenRect.Height <= 0.0f) return false;

	ImVec2 MousePos = ImGui::GetMousePos();
	return MousePos.x >= ViewportScreenRect.X && MousePos.x <= (ViewportScreenRect.X + ViewportScreenRect.Width) &&
		MousePos.y >= ViewportScreenRect.Y && MousePos.y <= (ViewportScreenRect.Y + ViewportScreenRect.Height);
}

void FStaticMeshEditorViewportClient::NotifyViewportResized(int32 NewWidth, int32 NewHeight)
{
	if (Viewport && NewHeight > 0)
	{
		ViewTransform.AspectRatio = static_cast<float>(NewWidth) / static_cast<float>(NewHeight);
	}
}

bool FStaticMeshEditorViewportClient::GetCameraView(FMinimalViewInfo& OutPOV) const
{
	OutPOV.Location = ViewTransform.ViewLocation;
	OutPOV.Rotation = ViewTransform.ViewRotation;
	OutPOV.FOV = ViewTransform.FOV;
	OutPOV.AspectRatio = ViewTransform.AspectRatio;
	return true;
}

void FStaticMeshEditorViewportClient::SubmitFrameDebugDraw()
{
	DrawPreviewCollision();
}

void FStaticMeshEditorViewportClient::Tick(float DeltaTime)
{
	SyncCameraSmoothingTarget();
	ApplySmoothedCameraLocation(DeltaTime);
	TickShortcuts();
	TickInput(DeltaTime);
}

void FStaticMeshEditorViewportClient::TickShortcuts()
{
	if (!FSlateApplication::Get().DoesClientOwnKeyboardInput(this)) return;

	if (InputSystem::Get().GetKeyDown('F'))
	{
		ResetCameraToPreviewBounds();
	}
}

void FStaticMeshEditorViewportClient::TickInput(float DeltaTime)
{
	if (!FSlateApplication::Get().DoesClientOwnMouseInput(this)) return;
	// 텍스트 입력 중에는 카메라 키/마우스 조작을 가로채지 않는다.
	if (ImGui::GetIO().WantTextInput) return;

	FViewportCameraControlSettings& ControlSettings = FEditorSettings::Get().MeshEditorViewportSettings.CameraControls;
	InputSystem& Input = InputSystem::Get();

	FVector LocalMove = FVector::ZeroVector;
	float WorldVerticalMove = 0.0f;
	const float CameraSpeed = ControlSettings.MoveSpeed;

	if (Input.GetKey('W')) LocalMove.X += CameraSpeed;
	if (Input.GetKey('S')) LocalMove.X -= CameraSpeed;
	if (Input.GetKey('D')) LocalMove.Y += CameraSpeed;
	if (Input.GetKey('A')) LocalMove.Y -= CameraSpeed;
	if (Input.GetKey('Q')) WorldVerticalMove -= CameraSpeed;
	if (Input.GetKey('E')) WorldVerticalMove += CameraSpeed;

	const FVector Forward = ViewTransform.ViewRotation.GetForwardVector();
	const FVector Right = ViewTransform.ViewRotation.GetRightVector();

	FVector DeltaMove = (Forward * LocalMove.X + Right * LocalMove.Y) * DeltaTime;
	DeltaMove.Z += WorldVerticalMove * DeltaTime;
	TargetLocation += DeltaMove;

	if (Input.GetKey(VK_RBUTTON))
	{
		const float MouseRotationSpeed = 0.15f * ControlSettings.RotationSpeed;
		const float DeltaYaw = static_cast<float>(Input.MouseDeltaX()) * MouseRotationSpeed;
		const float DeltaPitch = static_cast<float>(Input.MouseDeltaY()) * MouseRotationSpeed;
		ViewTransform.Rotate(DeltaYaw, DeltaPitch);
	}

	const float ScrollNotches = InputSystem::Get().GetScrollNotches();
	if (ScrollNotches != 0.0f)
	{
		if (InputSystem::Get().GetKey(VK_RBUTTON))
		{
			float& MoveSpeed = FEditorSettings::Get().MeshEditorViewportSettings.CameraControls.MoveSpeed;
			MoveSpeed = ScrollNotches < 0.0f ? MoveSpeed * 0.9f : MoveSpeed * 1.1f;
			MoveSpeed = Clamp(MoveSpeed, 0.001f, 1000.0f);
		}
		else if (ViewTransform.bIsOrtho)
		{
			const float NewWidth = ViewTransform.OrthoZoom - ScrollNotches * ControlSettings.ZoomSpeed * DeltaTime;
			ViewTransform.OrthoZoom = Clamp(NewWidth, 0.1f, 1000.0f);
		}
		else
		{
			TargetLocation += ViewTransform.ViewRotation.GetForwardVector() * (ScrollNotches * ControlSettings.ZoomSpeed * 0.015f);
		}
	}
}

void FStaticMeshEditorViewportClient::SyncCameraSmoothingTarget()
{
	const FVector CurrentLocation = ViewTransform.ViewLocation;
	const bool bCameraMovedExternally = bLastAppliedCameraLocationInitialized
		&& FVector::DistSquared(CurrentLocation, LastAppliedCameraLocation) > 0.0001f;

	if (!bTargetLocationInitialized || bCameraMovedExternally)
	{
		TargetLocation = CurrentLocation;
		bTargetLocationInitialized = true;
	}

	LastAppliedCameraLocation = CurrentLocation;
	bLastAppliedCameraLocationInitialized = true;
}

void FStaticMeshEditorViewportClient::ApplySmoothedCameraLocation(float DeltaTime)
{
	const FVector CurrentLocation = ViewTransform.ViewLocation;
	const float LerpAlpha = Clamp(DeltaTime * SmoothLocationSpeed, 0.0f, 1.0f);
	const FVector NewLocation = CurrentLocation + (TargetLocation - CurrentLocation) * LerpAlpha;
	ViewTransform.ViewLocation = NewLocation;

	LastAppliedCameraLocation = NewLocation;
	bLastAppliedCameraLocationInitialized = true;
}

void FStaticMeshEditorViewportClient::DrawPreviewCollision()
{
	if (!RenderOptions.ShowFlags.bShowCollisionShape || !PreviewWorld || !PreviewMeshComponent)
	{
		return;
	}

	const UStaticMesh* StaticMesh = PreviewMeshComponent->GetStaticMesh();
	const UBodySetup* BodySetup = StaticMesh ? StaticMesh->GetBodySetup() : nullptr;
	if (!BodySetup)
	{
		return;
	}

	const TArray<FVector>& Vertices = BodySetup->GetComplexCollisionVertices();
	const TArray<uint32>& Indices = BodySetup->GetComplexCollisionIndices();
	if (Vertices.empty() || Indices.size() < 3)
	{
		return;
	}

	FScene& Scene = PreviewWorld->GetScene();
	const FMatrix& LocalToWorld = PreviewMeshComponent->GetWorldMatrix();
	const FColor CollisionColor(0, 255, 255);
	const FColor BlockerColor(255, 220, 0);

	const size_t TriangleCount = Indices.size() / 3;
	for (size_t TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex)
	{
		const uint32 I0 = Indices[TriangleIndex * 3 + 0];
		const uint32 I1 = Indices[TriangleIndex * 3 + 1];
		const uint32 I2 = Indices[TriangleIndex * 3 + 2];
		if (I0 >= Vertices.size() || I1 >= Vertices.size() || I2 >= Vertices.size())
		{
			continue;
		}

		const FVector V0 = LocalToWorld.TransformPositionWithW(Vertices[I0]);
		const FVector V1 = LocalToWorld.TransformPositionWithW(Vertices[I1]);
		const FVector V2 = LocalToWorld.TransformPositionWithW(Vertices[I2]);

		Scene.AddDebugLine(V0, V1, CollisionColor);
		Scene.AddDebugLine(V1, V2, CollisionColor);
		Scene.AddDebugLine(V2, V0, CollisionColor);
	}

	const FKAggregateGeom& AggGeom = BodySetup->GetAggGeom();
	for (const FKBoxElem& Box : AggGeom.BoxElems)
	{
		DrawDebugBoxElem(Scene, LocalToWorld, Box, BlockerColor);
	}
}
