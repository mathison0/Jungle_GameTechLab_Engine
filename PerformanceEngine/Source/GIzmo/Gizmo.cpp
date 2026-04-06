#include "Gizmo/Gizmo.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

#include "Camera/Camera.h"
#include "Gizmo/GizmoMeshFactory.h"
#include "Graphics/D3D11/D3D11RHI.h"
#include "Graphics/D3D11/D3D11Utils.h"
#include "Picking/PickingSystem.h"
#include "Picking/PickingMath.h"
#include "Scene/Scene.h"

namespace
{
	constexpr float ScaleReferenceUnits = 20.0f;
	constexpr float UniformScalePixelsPerUnit = 120.0f;
	constexpr float MinScaleMagnitude = 0.01f;
	constexpr float MaxScaleMagnitude = 1000.0f;
	constexpr float MinGizmoScale = 0.05f;
	constexpr float MaxGizmoScale = 3.0f;
	constexpr float GizmoViewportHeightRatio = 0.30f;
	constexpr float TranslationAxisLengthUnits = 47.0f;
	constexpr float ScaleAxisLengthUnits = 25.0f;
	constexpr float RotationCacheQuantization = 100.0f;

	float ClampScaleComponent(float InValue)
	{
		float ClampedValue = std::clamp(InValue, -MaxScaleMagnitude, MaxScaleMagnitude);
		if (std::fabs(ClampedValue) < MinScaleMagnitude)
		{
			ClampedValue = (ClampedValue < 0.0f) ? -MinScaleMagnitude : MinScaleMagnitude;
		}

		return ClampedValue;
	}

	FVector ClampScaleVector(const FVector& InValue)
	{
		return FVector(
			ClampScaleComponent(InValue.X),
			ClampScaleComponent(InValue.Y),
			ClampScaleComponent(InValue.Z));
	}

	int32 QuantizeDirection(float InValue)
	{
		return static_cast<int32>(std::round(InValue * RotationCacheQuantization));
	}
}

bool FGizmo::Initialize(FD3D11RHI& InRHI)
{
	Device = InRHI.GetDevice();
	return Device != nullptr;
}

void FGizmo::Shutdown()
{
	TranslationHandles = FHandleSet();
	ScaleHandles = FHandleSet();
	RotationHandleCache.clear();
	MeshCache.clear();
	Device = nullptr;
	ClearHover();
	EndDrag();
}

void FGizmo::SetMode(EGizmoMode InMode)
{
	if (Mode == InMode)
	{
		return;
	}

	Mode = InMode;
	ClearHover();
	EndDrag();
}

void FGizmo::ToggleCoordinateSpace()
{
	SetCoordinateSpace(CoordinateSpace == EGizmoCoordinateSpace::World
		? EGizmoCoordinateSpace::Local
		: EGizmoCoordinateSpace::World);
}

void FGizmo::SetCoordinateSpace(EGizmoCoordinateSpace InSpace)
{
	if (CoordinateSpace == InSpace)
	{
		return;
	}

	CoordinateSpace = InSpace;
	ClearHover();
	EndDrag();
}

void FGizmo::ClearHover()
{
	if (!IsDragging())
	{
		HoveredAxis = EGizmoAxis::None;
	}
}

bool FGizmo::HasRenderableSelection(const FScene& InScene, const FPickState& InPickState) const
{
	return ResolveSelectedTransform(InScene, InPickState) != nullptr;
}

void FGizmo::UpdateHover(
	const FScene& InScene,
	const FPickState& InPickState,
	const FCamera& InCamera,
	POINT InMousePositionClient,
	int32 InViewportWidth,
	int32 InViewportHeight)
{
	if (IsDragging())
	{
		return;
	}

	const FPickResult PickResult = PickGizmo(InScene, InPickState, InCamera, InMousePositionClient, InViewportWidth, InViewportHeight);
	HoveredAxis = PickResult.bHit ? PickResult.Axis : EGizmoAxis::None;
}

bool FGizmo::BeginDrag(
	FScene& InScene,
	const FPickState& InPickState,
	const FCamera& InCamera,
	POINT InMousePositionClient,
	int32 InViewportWidth,
	int32 InViewportHeight)
{
	const FTransform* SelectedTransform = ResolveSelectedTransform(InScene, InPickState);
	if (SelectedTransform == nullptr)
	{
		return false;
	}

	const FPickResult PickResult = PickGizmo(InScene, InPickState, InCamera, InMousePositionClient, InViewportWidth, InViewportHeight);
	if (!PickResult.bHit || PickResult.Axis == EGizmoAxis::None)
	{
		return false;
	}

	const FRay Ray = PickingMath::BuildPickRay(InCamera, InMousePositionClient.x, InMousePositionClient.y, InViewportWidth, InViewportHeight);
	if (Mode == EGizmoMode::Translation)
	{
		return BeginTranslationDrag(*SelectedTransform, PickResult.Axis, Ray, InCamera);
	}

	if (Mode == EGizmoMode::Rotation)
	{
		return BeginRotationDrag(*SelectedTransform, PickResult.Axis, Ray, InCamera);
	}

	return BeginScaleDrag(*SelectedTransform, PickResult.Axis, Ray, InCamera, InMousePositionClient);
}

bool FGizmo::UpdateDrag(
	FScene& InScene,
	const FPickState& InPickState,
	const FCamera& InCamera,
	POINT InMousePositionClient,
	int32 InViewportWidth,
	int32 InViewportHeight)
{
	if (!IsDragging())
	{
		return false;
	}

	const FTransform* SelectedTransform = ResolveSelectedTransform(InScene, InPickState);
	if (SelectedTransform == nullptr)
	{
		return false;
	}

	const FRay Ray = PickingMath::BuildPickRay(InCamera, InMousePositionClient.x, InMousePositionClient.y, InViewportWidth, InViewportHeight);
	FVector Intersection = FVector::ZeroVector;
	if (!PickingMath::IntersectPlane(Ray, DragStartGizmoLocation, DragPlaneNormal, Intersection))
	{
		return false;
	}

	FTransform NewTransform = DragStartTransform;
	if (Mode == EGizmoMode::Translation)
	{
		FVector NewLocation = DragStartTransform.GetLocation();
		if (ActiveAxis >= EGizmoAxis::X && ActiveAxis <= EGizmoAxis::Z)
		{
			const FVector Axis = GetGizmoAxisVector(ActiveAxis, DragStartTransform);
			const float AxisDistance = FVector::DotProduct(Intersection - DragStartGizmoLocation, Axis);
			NewLocation = DragStartTransform.GetLocation() + Axis * (AxisDistance - DragStartAxisDistance);
		}
		else
		{
			NewLocation = DragStartTransform.GetLocation() + (Intersection - DragStartIntersection);
		}

		NewTransform.SetLocation(NewLocation);
		return InScene.SetPrimitiveTransformWorld(InPickState.SelectedPrimitiveIndex, NewTransform);
	}

	if (Mode == EGizmoMode::Rotation)
	{
		const FVector CurrentVector = (Intersection - DragStartGizmoLocation).GetSafeNormal();
		if (CurrentVector.IsNearlyZero(PickingMath::ParallelTolerance) || DragStartRotationVector.IsNearlyZero(PickingMath::ParallelTolerance))
		{
			return false;
		}

		const FVector Axis = DragPlaneNormal.GetSafeNormal();
		if (Axis.IsNearlyZero(PickingMath::ParallelTolerance))
		{
			return false;
		}

		const FVector Cross = FVector::CrossProduct(DragStartRotationVector, CurrentVector);
		const float SignedAngleRadians = std::atan2(
			FVector::DotProduct(Cross, Axis),
			FVector::DotProduct(DragStartRotationVector, CurrentVector));
		CurrentRotationDeltaDegrees = FMath::RadiansToDegrees(SignedAngleRadians);
		const FQuat DeltaRotation(Axis, SignedAngleRadians);
		NewTransform.SetRotation((DragStartTransform.GetRotation() * DeltaRotation).GetNormalized());
		return InScene.SetPrimitiveTransformWorld(InPickState.SelectedPrimitiveIndex, NewTransform);
	}

	FVector NewScale = DragStartTransform.GetScale3D();
	const float GizmoScale = GetRenderGizmoScale(ComputeGizmoScale(DragStartGizmoLocation, InCamera));
	const float ScaleDenominator = (ScaleReferenceUnits * GizmoScale > PickingMath::ParallelTolerance)
		? (ScaleReferenceUnits * GizmoScale)
		: ScaleReferenceUnits;
	const FQuat StartRotation = DragStartTransform.GetRotation();

	if (ActiveAxis >= EGizmoAxis::X && ActiveAxis <= EGizmoAxis::Z)
	{
		const FVector AxisWorld = GetGizmoAxisVector(ActiveAxis, DragStartTransform);
		const float CurrentAxisDistance = FVector::DotProduct(Intersection - DragStartGizmoLocation, AxisWorld);
		const float DeltaScale = (CurrentAxisDistance - DragStartAxisDistance) / ScaleDenominator;

		if (CoordinateSpace == EGizmoCoordinateSpace::Local)
		{
			const int32 AxisIndex = static_cast<int32>(ActiveAxis) - 1;
			NewScale[AxisIndex] = ClampScaleComponent(DragStartTransform.GetScale3D()[AxisIndex] + DeltaScale);
		}
		else
		{
			const FVector LocalDelta = StartRotation.Inverse().RotateVector(GetAxisVector(ActiveAxis) * DeltaScale);
			NewScale = ClampScaleVector(DragStartTransform.GetScale3D() + LocalDelta);
		}
	}
	else if (ActiveAxis >= EGizmoAxis::XY && ActiveAxis <= EGizmoAxis::YZ)
	{
		const FVector Offset = Intersection - DragStartIntersection;
		if (CoordinateSpace == EGizmoCoordinateSpace::Local)
		{
			switch (ActiveAxis)
			{
			case EGizmoAxis::XY:
			{
				const float DeltaX = FVector::DotProduct(Offset, GetGizmoAxisVector(EGizmoAxis::X, DragStartTransform)) / ScaleDenominator;
				const float DeltaY = FVector::DotProduct(Offset, GetGizmoAxisVector(EGizmoAxis::Y, DragStartTransform)) / ScaleDenominator;
				NewScale.X = ClampScaleComponent(DragStartTransform.GetScale3D().X + DeltaX);
				NewScale.Y = ClampScaleComponent(DragStartTransform.GetScale3D().Y + DeltaY);
				break;
			}
			case EGizmoAxis::XZ:
			{
				const float DeltaX = FVector::DotProduct(Offset, GetGizmoAxisVector(EGizmoAxis::X, DragStartTransform)) / ScaleDenominator;
				const float DeltaZ = FVector::DotProduct(Offset, GetGizmoAxisVector(EGizmoAxis::Z, DragStartTransform)) / ScaleDenominator;
				NewScale.X = ClampScaleComponent(DragStartTransform.GetScale3D().X + DeltaX);
				NewScale.Z = ClampScaleComponent(DragStartTransform.GetScale3D().Z + DeltaZ);
				break;
			}
			case EGizmoAxis::YZ:
			{
				const float DeltaY = FVector::DotProduct(Offset, GetGizmoAxisVector(EGizmoAxis::Y, DragStartTransform)) / ScaleDenominator;
				const float DeltaZ = FVector::DotProduct(Offset, GetGizmoAxisVector(EGizmoAxis::Z, DragStartTransform)) / ScaleDenominator;
				NewScale.Y = ClampScaleComponent(DragStartTransform.GetScale3D().Y + DeltaY);
				NewScale.Z = ClampScaleComponent(DragStartTransform.GetScale3D().Z + DeltaZ);
				break;
			}
			default:
				break;
			}
		}
		else
		{
			FVector WorldDelta = FVector::ZeroVector;
			switch (ActiveAxis)
			{
			case EGizmoAxis::XY:
				WorldDelta = GetAxisVector(EGizmoAxis::X) * (FVector::DotProduct(Offset, GetAxisVector(EGizmoAxis::X)) / ScaleDenominator)
					+ GetAxisVector(EGizmoAxis::Y) * (FVector::DotProduct(Offset, GetAxisVector(EGizmoAxis::Y)) / ScaleDenominator);
				break;
			case EGizmoAxis::XZ:
				WorldDelta = GetAxisVector(EGizmoAxis::X) * (FVector::DotProduct(Offset, GetAxisVector(EGizmoAxis::X)) / ScaleDenominator)
					+ GetAxisVector(EGizmoAxis::Z) * (FVector::DotProduct(Offset, GetAxisVector(EGizmoAxis::Z)) / ScaleDenominator);
				break;
			case EGizmoAxis::YZ:
				WorldDelta = GetAxisVector(EGizmoAxis::Y) * (FVector::DotProduct(Offset, GetAxisVector(EGizmoAxis::Y)) / ScaleDenominator)
					+ GetAxisVector(EGizmoAxis::Z) * (FVector::DotProduct(Offset, GetAxisVector(EGizmoAxis::Z)) / ScaleDenominator);
				break;
			default:
				break;
			}

			NewScale = ClampScaleVector(DragStartTransform.GetScale3D() + StartRotation.Inverse().RotateVector(WorldDelta));
		}
	}
	else if (ActiveAxis == EGizmoAxis::XYZ)
	{
		const float PixelDelta = static_cast<float>((InMousePositionClient.x - DragStartScreenX) - (InMousePositionClient.y - DragStartScreenY));
		const float UniformDelta = PixelDelta / UniformScalePixelsPerUnit;
		NewScale = ClampScaleVector(DragStartTransform.GetScale3D() + FVector(UniformDelta, UniformDelta, UniformDelta));
	}

	NewTransform.SetScale3D(NewScale);
	return InScene.SetPrimitiveTransformWorld(InPickState.SelectedPrimitiveIndex, NewTransform);
}

void FGizmo::EndDrag()
{
	ActiveAxis = EGizmoAxis::None;
	CurrentRotationDeltaDegrees = 0.0f;
	DragStartTransform = FTransform::Identity;
	DragStartGizmoLocation = FVector::ZeroVector;
	DragStartIntersection = FVector::ZeroVector;
	DragPlaneNormal = FVector::ZeroVector;
	DragStartRotationVector = FVector::ZeroVector;
	DragStartAxisDistance = 0.0f;
	DragStartScreenX = 0;
	DragStartScreenY = 0;
}

void FGizmo::BuildDrawCommands(
	const FScene& InScene,
	const FPickState& InPickState,
	const FCamera& InCamera,
	std::vector<FGizmoDrawCommand>& OutDrawCommands)
{
	const FTransform* SelectedTransform = ResolveSelectedTransform(InScene, InPickState);
	if (SelectedTransform == nullptr)
	{
		return;
	}

	const FVector WorldLocation = SelectedTransform->GetLocation();
	const float GizmoScale = GetRenderGizmoScale(ComputeGizmoScale(WorldLocation, InCamera));
	const FQuat GizmoRotation = GetGizmoRotation(*SelectedTransform);
	const FMatrix AxisWorld = FTransform(GizmoRotation, WorldLocation, FVector(GizmoScale, GizmoScale, GizmoScale)).ToMatrixWithScale();
	const FMatrix ScreenWorld = FTransform(FQuat::Identity, WorldLocation, FVector(GizmoScale, GizmoScale, GizmoScale)).ToMatrixWithScale();

	if (Mode == EGizmoMode::Translation)
	{
		if (!EnsureTranslationHandleSet())
		{
			return;
		}

		for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
		{
			AppendHandleDrawCommand(TranslationHandles.Axis[AxisIndex], AxisWorld, static_cast<EGizmoAxis>(AxisIndex + 1), OutDrawCommands);
		}
		for (int32 PlaneIndex = 0; PlaneIndex < 3; ++PlaneIndex)
		{
			AppendHandleDrawCommand(TranslationHandles.Plane[PlaneIndex], AxisWorld, static_cast<EGizmoAxis>(static_cast<int32>(EGizmoAxis::XY) + PlaneIndex), OutDrawCommands);
		}
		AppendHandleDrawCommand(TranslationHandles.Screen, AxisWorld, EGizmoAxis::Screen, OutDrawCommands);
		return;
	}

	if (Mode == EGizmoMode::Rotation)
	{
		if (!EnsureRotationHandleSet(InCamera))
		{
			return;
		}

		const auto HandleSetIt = RotationHandleCache.find(BuildRotationCacheKey(InCamera));
		if (HandleSetIt == RotationHandleCache.end())
		{
			return;
		}

		const FHandleSet& RotationHandles = HandleSetIt->second;
		for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
		{
			AppendHandleDrawCommand(RotationHandles.Axis[AxisIndex], AxisWorld, static_cast<EGizmoAxis>(AxisIndex + 1), OutDrawCommands);
		}
		AppendHandleDrawCommand(RotationHandles.Screen, ScreenWorld, EGizmoAxis::Screen, OutDrawCommands);
		return;
	}

	if (!EnsureScaleHandleSet())
	{
		return;
	}

	for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
	{
		AppendHandleDrawCommand(ScaleHandles.Axis[AxisIndex], AxisWorld, static_cast<EGizmoAxis>(AxisIndex + 1), OutDrawCommands);
	}
	for (int32 PlaneIndex = 0; PlaneIndex < 3; ++PlaneIndex)
	{
		AppendHandleDrawCommand(ScaleHandles.Plane[PlaneIndex], AxisWorld, static_cast<EGizmoAxis>(static_cast<int32>(EGizmoAxis::XY) + PlaneIndex), OutDrawCommands);
	}
	AppendHandleDrawCommand(ScaleHandles.Center, AxisWorld, EGizmoAxis::XYZ, OutDrawCommands);
}

bool FGizmo::EnsureTranslationHandleSet()
{
	if (TranslationHandles.Axis[0] && TranslationHandles.Axis[1] && TranslationHandles.Axis[2] && TranslationHandles.Screen)
	{
		return true;
	}

	const FTranslationGizmoMeshes Meshes = GenerateTranslationGizmoMeshes();
	TranslationHandles.Axis[0] = GetOrCreateCachedMesh("gizmo_translation_axis_x", Meshes.AxisX);
	TranslationHandles.Axis[1] = GetOrCreateCachedMesh("gizmo_translation_axis_y", Meshes.AxisY);
	TranslationHandles.Axis[2] = GetOrCreateCachedMesh("gizmo_translation_axis_z", Meshes.AxisZ);
	TranslationHandles.Plane[0] = GetOrCreateCachedMesh("gizmo_translation_plane_xy", Meshes.PlaneXY);
	TranslationHandles.Plane[1] = GetOrCreateCachedMesh("gizmo_translation_plane_xz", Meshes.PlaneXZ);
	TranslationHandles.Plane[2] = GetOrCreateCachedMesh("gizmo_translation_plane_yz", Meshes.PlaneYZ);
	TranslationHandles.Screen = GetOrCreateCachedMesh("gizmo_translation_screen", Meshes.ScreenSphere);

	return TranslationHandles.Axis[0] && TranslationHandles.Axis[1] && TranslationHandles.Axis[2]
		&& TranslationHandles.Plane[0] && TranslationHandles.Plane[1] && TranslationHandles.Plane[2]
		&& TranslationHandles.Screen;
}

bool FGizmo::EnsureScaleHandleSet()
{
	if (ScaleHandles.Axis[0] && ScaleHandles.Axis[1] && ScaleHandles.Axis[2] && ScaleHandles.Center)
	{
		return true;
	}

	const FScaleGizmoMeshes Meshes = GenerateScaleGizmoMeshes();
	ScaleHandles.Axis[0] = GetOrCreateCachedMesh("gizmo_scale_axis_x", Meshes.AxisX);
	ScaleHandles.Axis[1] = GetOrCreateCachedMesh("gizmo_scale_axis_y", Meshes.AxisY);
	ScaleHandles.Axis[2] = GetOrCreateCachedMesh("gizmo_scale_axis_z", Meshes.AxisZ);
	ScaleHandles.Plane[0] = GetOrCreateCachedMesh("gizmo_scale_plane_xy", Meshes.PlaneXY);
	ScaleHandles.Plane[1] = GetOrCreateCachedMesh("gizmo_scale_plane_xz", Meshes.PlaneXZ);
	ScaleHandles.Plane[2] = GetOrCreateCachedMesh("gizmo_scale_plane_yz", Meshes.PlaneYZ);
	ScaleHandles.Center = GetOrCreateCachedMesh("gizmo_scale_center", Meshes.CenterCube);

	return ScaleHandles.Axis[0] && ScaleHandles.Axis[1] && ScaleHandles.Axis[2]
		&& ScaleHandles.Plane[0] && ScaleHandles.Plane[1] && ScaleHandles.Plane[2]
		&& ScaleHandles.Center;
}

bool FGizmo::EnsureRotationHandleSet(const FCamera& InCamera)
{
	const std::string RotationKey = BuildRotationCacheKey(InCamera);
	if (RotationHandleCache.find(RotationKey) != RotationHandleCache.end())
	{
		return true;
	}

	FRotationGizmoDesc Desc;
	Desc.ViewRight = InCamera.GetRotation().GetRightVector().GetSafeNormal();
	Desc.ViewUp = InCamera.GetRotation().GetUpVector().GetSafeNormal();
	Desc.CameraDirection = InCamera.GetRotation().GetForwardVector().GetSafeNormal();
	Desc.bIncludeScreenRing = true;
	Desc.bDragging = IsDragging();
	switch (ActiveAxis)
	{
	case EGizmoAxis::X:
		Desc.ActiveAxis = EGizmoAxisId::X;
		break;
	case EGizmoAxis::Y:
		Desc.ActiveAxis = EGizmoAxisId::Y;
		break;
	case EGizmoAxis::Z:
		Desc.ActiveAxis = EGizmoAxisId::Z;
		break;
	case EGizmoAxis::Screen:
		Desc.ActiveAxis = EGizmoAxisId::Screen;
		break;
	default:
		Desc.ActiveAxis = EGizmoAxisId::None;
		break;
	}
	Desc.DeltaRotationDegrees = CurrentRotationDeltaDegrees;

	const FRotationGizmoMeshes Meshes = GenerateRotationGizmoMeshes(Desc);
	FHandleSet HandleSet;
	HandleSet.Axis[0] = GetOrCreateCachedMesh(RotationKey + "_axis_x", Meshes.RingX);
	HandleSet.Axis[1] = GetOrCreateCachedMesh(RotationKey + "_axis_y", Meshes.RingY);
	HandleSet.Axis[2] = GetOrCreateCachedMesh(RotationKey + "_axis_z", Meshes.RingZ);
	HandleSet.Screen = GetOrCreateCachedMesh(RotationKey + "_screen", Meshes.ScreenRing);

	if (!HandleSet.Axis[0] || !HandleSet.Axis[1] || !HandleSet.Axis[2] || !HandleSet.Screen)
	{
		return false;
	}

	if (RotationHandleCache.size() >= 16)
	{
		RotationHandleCache.erase(RotationHandleCache.begin());
	}

	RotationHandleCache.emplace(RotationKey, std::move(HandleSet));
	return true;
}

std::shared_ptr<FGizmo::FCachedMeshEntry> FGizmo::CreateCachedMeshEntry(const FGizmoMesh& InMesh) const
{
	if (Device == nullptr || !InMesh.IsValid())
	{
		return nullptr;
	}

	auto Entry = std::make_shared<FCachedMeshEntry>();
	if (!Entry)
	{
		return nullptr;
	}

	const UINT VertexBufferSize = static_cast<UINT>(sizeof(FGizmoVertex) * InMesh.Vertices.size());
	const UINT IndexBufferSize = static_cast<UINT>(sizeof(uint32) * InMesh.Indices.size());
	if (!D3D11Utils::CreateImmutableBuffer(Device, VertexBufferSize, D3D11_BIND_VERTEX_BUFFER, InMesh.Vertices.data(), Entry->VertexBuffer)
		|| !D3D11Utils::CreateImmutableBuffer(Device, IndexBufferSize, D3D11_BIND_INDEX_BUFFER, InMesh.Indices.data(), Entry->IndexBuffer))
	{
		return nullptr;
	}

	Entry->Mesh = InMesh;
	Entry->LocalBVH = BuildGizmoLocalBVH(InMesh);
	Entry->IndexCount = static_cast<uint32>(InMesh.Indices.size());
	return Entry;
}

std::shared_ptr<FGizmo::FCachedMeshEntry> FGizmo::GetOrCreateCachedMesh(const std::string& InKey, const FGizmoMesh& InMesh)
{
	const auto ExistingIt = MeshCache.find(InKey);
	if (ExistingIt != MeshCache.end())
	{
		return ExistingIt->second;
	}

	std::shared_ptr<FCachedMeshEntry> NewEntry = CreateCachedMeshEntry(InMesh);
	if (NewEntry)
	{
		MeshCache.emplace(InKey, NewEntry);
	}

	return NewEntry;
}

FGizmo::FPickResult FGizmo::PickGizmo(
	const FScene& InScene,
	const FPickState& InPickState,
	const FCamera& InCamera,
	POINT InMousePositionClient,
	int32 InViewportWidth,
	int32 InViewportHeight)
{
	FPickResult BestResult;
	BestResult.Distance = std::numeric_limits<float>::max();

	const FTransform* SelectedTransform = ResolveSelectedTransform(InScene, InPickState);
	if (SelectedTransform == nullptr
		|| InViewportWidth <= 0
		|| InViewportHeight <= 0
		|| InMousePositionClient.x < 0
		|| InMousePositionClient.y < 0
		|| InMousePositionClient.x >= InViewportWidth
		|| InMousePositionClient.y >= InViewportHeight)
	{
		return BestResult;
	}

	const FRay WorldRay = PickingMath::BuildPickRay(InCamera, InMousePositionClient.x, InMousePositionClient.y, InViewportWidth, InViewportHeight);
	const FVector WorldLocation = SelectedTransform->GetLocation();
	const float GizmoScale = GetRenderGizmoScale(ComputeGizmoScale(WorldLocation, InCamera));
	const FQuat GizmoRotation = GetGizmoRotation(*SelectedTransform);
	const FMatrix AxisWorld = FTransform(GizmoRotation, WorldLocation, FVector(GizmoScale, GizmoScale, GizmoScale)).ToMatrixWithScale();
	const FMatrix ScreenWorld = FTransform(FQuat::Identity, WorldLocation, FVector(GizmoScale, GizmoScale, GizmoScale)).ToMatrixWithScale();

	if (Mode == EGizmoMode::Translation)
	{
		if (!EnsureTranslationHandleSet())
		{
			return BestResult;
		}

		for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
		{
			TryHitHandle(TranslationHandles.Axis[AxisIndex], AxisWorld, static_cast<EGizmoAxis>(AxisIndex + 1), WorldRay, BestResult);
		}
		for (int32 PlaneIndex = 0; PlaneIndex < 3; ++PlaneIndex)
		{
			TryHitHandle(TranslationHandles.Plane[PlaneIndex], AxisWorld, static_cast<EGizmoAxis>(static_cast<int32>(EGizmoAxis::XY) + PlaneIndex), WorldRay, BestResult);
		}
		TryHitHandle(TranslationHandles.Screen, AxisWorld, EGizmoAxis::Screen, WorldRay, BestResult);
		return BestResult;
	}

	if (Mode == EGizmoMode::Rotation)
	{
		if (!EnsureRotationHandleSet(InCamera))
		{
			return BestResult;
		}

		const auto RotationIt = RotationHandleCache.find(BuildRotationCacheKey(InCamera));
		if (RotationIt == RotationHandleCache.end())
		{
			return BestResult;
		}

		const FHandleSet& RotationHandles = RotationIt->second;
		for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
		{
			TryHitHandle(RotationHandles.Axis[AxisIndex], AxisWorld, static_cast<EGizmoAxis>(AxisIndex + 1), WorldRay, BestResult);
		}
		TryHitHandle(RotationHandles.Screen, ScreenWorld, EGizmoAxis::Screen, WorldRay, BestResult);
		return BestResult;
	}

	if (!EnsureScaleHandleSet())
	{
		return BestResult;
	}

	for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
	{
		TryHitHandle(ScaleHandles.Axis[AxisIndex], AxisWorld, static_cast<EGizmoAxis>(AxisIndex + 1), WorldRay, BestResult);
	}
	for (int32 PlaneIndex = 0; PlaneIndex < 3; ++PlaneIndex)
	{
		TryHitHandle(ScaleHandles.Plane[PlaneIndex], AxisWorld, static_cast<EGizmoAxis>(static_cast<int32>(EGizmoAxis::XY) + PlaneIndex), WorldRay, BestResult);
	}
	TryHitHandle(ScaleHandles.Center, AxisWorld, EGizmoAxis::XYZ, WorldRay, BestResult);
	return BestResult;
}

bool FGizmo::BeginTranslationDrag(const FTransform& InSelectedTransform, EGizmoAxis InAxis, const FRay& InRay, const FCamera& InCamera)
{
	const FVector GizmoLocation = InSelectedTransform.GetLocation();
	const FVector Axis = GetGizmoAxisVector(InAxis, InSelectedTransform);
	const FVector ViewForward = InCamera.GetRotation().GetForwardVector().GetSafeNormal();
	const FVector ViewRight = InCamera.GetRotation().GetRightVector().GetSafeNormal();

	FVector PlaneNormal = FVector::ZeroVector;
	if (InAxis >= EGizmoAxis::X && InAxis <= EGizmoAxis::Z)
	{
		FVector PlaneTangent = FVector::CrossProduct(ViewForward, Axis);
		if (PlaneTangent.SizeSquared() <= PickingMath::ParallelTolerance)
		{
			PlaneTangent = FVector::CrossProduct(ViewRight, Axis);
		}
		if (PlaneTangent.SizeSquared() <= PickingMath::ParallelTolerance)
		{
			PlaneTangent = FVector::CrossProduct(FVector::UpVector, Axis);
		}

		PlaneNormal = FVector::CrossProduct(Axis, PlaneTangent).GetSafeNormal();
	}
	else if (InAxis == EGizmoAxis::Screen)
	{
		PlaneNormal = ViewForward;
	}
	else
	{
		PlaneNormal = GetGizmoPlaneNormal(InAxis, InSelectedTransform);
	}

	FVector Intersection = FVector::ZeroVector;
	if (PlaneNormal.IsNearlyZero(PickingMath::ParallelTolerance)
		|| !PickingMath::IntersectPlane(InRay, GizmoLocation, PlaneNormal, Intersection))
	{
		return false;
	}

	ActiveAxis = InAxis;
	DragStartTransform = InSelectedTransform;
	DragStartGizmoLocation = GizmoLocation;
	DragStartIntersection = Intersection;
	DragPlaneNormal = PlaneNormal;
	DragStartAxisDistance = (InAxis >= EGizmoAxis::X && InAxis <= EGizmoAxis::Z)
		? FVector::DotProduct(Intersection - GizmoLocation, Axis)
		: 0.0f;
	return true;
}

bool FGizmo::BeginRotationDrag(const FTransform& InSelectedTransform, EGizmoAxis InAxis, const FRay& InRay, const FCamera& InCamera)
{
	const FVector GizmoLocation = InSelectedTransform.GetLocation();
	const FVector Axis = (InAxis == EGizmoAxis::Screen)
		? InCamera.GetRotation().GetForwardVector().GetSafeNormal()
		: GetGizmoAxisVector(InAxis, InSelectedTransform);

	FVector Intersection = FVector::ZeroVector;
	if (Axis.IsNearlyZero(PickingMath::ParallelTolerance)
		|| !PickingMath::IntersectPlane(InRay, GizmoLocation, Axis, Intersection))
	{
		return false;
	}

	const FVector StartVector = (Intersection - GizmoLocation).GetSafeNormal();
	if (StartVector.IsNearlyZero(PickingMath::ParallelTolerance))
	{
		return false;
	}

	ActiveAxis = InAxis;
	DragStartTransform = InSelectedTransform;
	DragStartGizmoLocation = GizmoLocation;
	DragPlaneNormal = Axis;
	DragStartRotationVector = StartVector;
	CurrentRotationDeltaDegrees = 0.0f;
	return true;
}

bool FGizmo::BeginScaleDrag(const FTransform& InSelectedTransform, EGizmoAxis InAxis, const FRay& InRay, const FCamera& InCamera, POINT InMousePositionClient)
{
	const FVector GizmoLocation = InSelectedTransform.GetLocation();
	const FVector ViewForward = InCamera.GetRotation().GetForwardVector().GetSafeNormal();
	const FVector ViewRight = InCamera.GetRotation().GetRightVector().GetSafeNormal();

	FVector PlaneNormal = FVector::ZeroVector;
	if (InAxis >= EGizmoAxis::X && InAxis <= EGizmoAxis::Z)
	{
		const FVector Axis = GetGizmoAxisVector(InAxis, InSelectedTransform);
		FVector PlaneTangent = FVector::CrossProduct(ViewForward, Axis);
		if (PlaneTangent.SizeSquared() <= PickingMath::ParallelTolerance)
		{
			PlaneTangent = FVector::CrossProduct(ViewRight, Axis);
		}
		if (PlaneTangent.SizeSquared() <= PickingMath::ParallelTolerance)
		{
			PlaneTangent = FVector::CrossProduct(FVector::UpVector, Axis);
		}

		PlaneNormal = FVector::CrossProduct(Axis, PlaneTangent).GetSafeNormal();
	}
	else if (InAxis >= EGizmoAxis::XY && InAxis <= EGizmoAxis::YZ)
	{
		PlaneNormal = GetGizmoPlaneNormal(InAxis, InSelectedTransform);
	}
	else if (InAxis == EGizmoAxis::XYZ)
	{
		PlaneNormal = ViewForward;
	}

	FVector Intersection = FVector::ZeroVector;
	if (PlaneNormal.IsNearlyZero(PickingMath::ParallelTolerance)
		|| !PickingMath::IntersectPlane(InRay, GizmoLocation, PlaneNormal, Intersection))
	{
		return false;
	}

	ActiveAxis = InAxis;
	DragStartTransform = InSelectedTransform;
	DragStartGizmoLocation = GizmoLocation;
	DragStartIntersection = Intersection;
	DragPlaneNormal = PlaneNormal;
	DragStartAxisDistance = (InAxis >= EGizmoAxis::X && InAxis <= EGizmoAxis::Z)
		? FVector::DotProduct(Intersection - GizmoLocation, GetGizmoAxisVector(InAxis, InSelectedTransform))
		: 0.0f;
	DragStartScreenX = InMousePositionClient.x;
	DragStartScreenY = InMousePositionClient.y;
	return true;
}

bool FGizmo::TryHitHandle(
	const std::shared_ptr<FCachedMeshEntry>& InMeshEntry,
	const FMatrix& InWorldMatrix,
	EGizmoAxis InAxis,
	const FRay& InWorldRay,
	FPickResult& InOutBestResult) const
{
	if (!InMeshEntry || !InMeshEntry->Mesh.IsValid() || InMeshEntry->LocalBVH.SpatialData.Nodes.empty())
	{
		return false;
	}

	const FMatrix InverseWorld = InWorldMatrix.GetInverse();
	FRay LocalRay;
	LocalRay.Origin = InverseWorld.TransformPosition(InWorldRay.Origin);
	LocalRay.Direction = InverseWorld.TransformVector(InWorldRay.Direction).GetSafeNormal();
	if (LocalRay.Direction.IsNearlyZero(PickingMath::ParallelTolerance))
	{
		return false;
	}

	float LocalHitDistance = PickingMath::NoHitDistance;
	FVector LocalHitPosition = FVector::ZeroVector;
	if (!IntersectGizmoLocalBVH(LocalRay, InMeshEntry->Mesh, InMeshEntry->LocalBVH, LocalHitDistance, LocalHitPosition))
	{
		return false;
	}

	const FVector WorldHitPosition = InWorldMatrix.TransformPosition(LocalHitPosition);
	const float DistanceSq = FVector::DistSquared(InWorldRay.Origin, WorldHitPosition);
	if (DistanceSq >= InOutBestResult.Distance)
	{
		return false;
	}

	InOutBestResult.bHit = true;
	InOutBestResult.Axis = InAxis;
	InOutBestResult.Distance = DistanceSq;
	InOutBestResult.WorldPosition = WorldHitPosition;
	return true;
}

void FGizmo::AppendHandleDrawCommand(
	const std::shared_ptr<FCachedMeshEntry>& InMeshEntry,
	const FMatrix& InWorldMatrix,
	EGizmoAxis InAxis,
	std::vector<FGizmoDrawCommand>& OutDrawCommands) const
{
	if (!InMeshEntry || InMeshEntry->VertexBuffer == nullptr || InMeshEntry->IndexBuffer == nullptr || InMeshEntry->IndexCount == 0)
	{
		return;
	}

	FGizmoDrawCommand DrawCommand;
	DrawCommand.VertexBuffer = InMeshEntry->VertexBuffer.Get();
	DrawCommand.IndexBuffer = InMeshEntry->IndexBuffer.Get();
	DrawCommand.IndexCount = InMeshEntry->IndexCount;
	DrawCommand.WorldMatrix = InWorldMatrix;

	const EGizmoAxis DisplayAxis = (ActiveAxis != EGizmoAxis::None) ? ActiveAxis : HoveredAxis;
	if (InAxis == DisplayAxis)
	{
		DrawCommand.Tint = FVector4(1.0f, 1.0f, 0.0f, 1.0f);
		DrawCommand.ColorBlend = 1.0f;
	}

	OutDrawCommands.push_back(DrawCommand);
}

const FTransform* FGizmo::ResolveSelectedTransform(const FScene& InScene, const FPickState& InPickState) const
{
	if (InPickState.SelectedPrimitiveIndex < 0)
	{
		return nullptr;
	}

	const size_t PrimitiveIndex = static_cast<size_t>(InPickState.SelectedPrimitiveIndex);
	if (PrimitiveIndex >= InScene.GetRenderItems().size())
	{
		return nullptr;
	}

	return &InScene.GetRenderItems()[PrimitiveIndex].Transform;
}

FVector FGizmo::GetSelectedLocation(const FScene& InScene, const FPickState& InPickState) const
{
	const FTransform* SelectedTransform = ResolveSelectedTransform(InScene, InPickState);
	return SelectedTransform ? SelectedTransform->GetLocation() : FVector::ZeroVector;
}

FQuat FGizmo::GetGizmoRotation(const FTransform& InSelectedTransform) const
{
	return (CoordinateSpace == EGizmoCoordinateSpace::Local)
		? InSelectedTransform.GetRotation()
		: FQuat::Identity;
}

FVector FGizmo::GetGizmoAxisVector(EGizmoAxis InAxis, const FTransform& InSelectedTransform) const
{
	const FVector WorldAxis = GetAxisVector(InAxis);
	if (CoordinateSpace != EGizmoCoordinateSpace::Local || WorldAxis.IsNearlyZero(PickingMath::ParallelTolerance))
	{
		return WorldAxis;
	}

	return InSelectedTransform.GetRotation().RotateVector(WorldAxis).GetSafeNormal();
}

FVector FGizmo::GetGizmoPlaneNormal(EGizmoAxis InAxis, const FTransform& InSelectedTransform) const
{
	const FVector WorldNormal = GetPlaneNormal(InAxis);
	if (CoordinateSpace != EGizmoCoordinateSpace::Local || WorldNormal.IsNearlyZero(PickingMath::ParallelTolerance))
	{
		return WorldNormal;
	}

	return InSelectedTransform.GetRotation().RotateVector(WorldNormal).GetSafeNormal();
}

FVector FGizmo::GetAxisVector(EGizmoAxis InAxis) const
{
	switch (InAxis)
	{
	case EGizmoAxis::X:
		return FVector::ForwardVector;
	case EGizmoAxis::Y:
		return FVector::RightVector;
	case EGizmoAxis::Z:
		return FVector::UpVector;
	default:
		return FVector::ZeroVector;
	}
}

FVector FGizmo::GetPlaneNormal(EGizmoAxis InAxis) const
{
	switch (InAxis)
	{
	case EGizmoAxis::XY:
		return FVector::UpVector;
	case EGizmoAxis::XZ:
		return FVector::RightVector;
	case EGizmoAxis::YZ:
		return FVector::ForwardVector;
	default:
		return FVector::ZeroVector;
	}
}

float FGizmo::ComputeGizmoScale(const FVector& InWorldPosition, const FCamera& InCamera) const
{
	const float Distance = (InWorldPosition - InCamera.GetLocation()).Size();
	const float HalfFovRadians = FMath::DegreesToRadians(InCamera.GetFOV() * 0.5f);
	const float VisibleHeight = 2.0f * std::max(Distance, 1.0f) * std::tan(HalfFovRadians);
	const float DesiredAxisLength = VisibleHeight * GizmoViewportHeightRatio;
	const float ReferenceAxisLength = (Mode == EGizmoMode::Scale) ? ScaleAxisLengthUnits : TranslationAxisLengthUnits;
	return std::clamp(DesiredAxisLength / ReferenceAxisLength, MinGizmoScale, MaxGizmoScale);
}

float FGizmo::GetRenderGizmoScale(float InBaseScale) const
{
	return (Mode == EGizmoMode::Scale) ? (InBaseScale * 0.5f) : InBaseScale;
}

std::string FGizmo::BuildRotationCacheKey(const FCamera& InCamera) const
{
	const FVector Forward = InCamera.GetRotation().GetForwardVector().GetSafeNormal();
	const FVector Up = InCamera.GetRotation().GetUpVector().GetSafeNormal();
	const FVector Right = InCamera.GetRotation().GetRightVector().GetSafeNormal();

	std::ostringstream Stream;
	Stream
		<< "gizmo_rotation_"
		<< QuantizeDirection(Forward.X) << '_'
		<< QuantizeDirection(Forward.Y) << '_'
		<< QuantizeDirection(Forward.Z) << '_'
		<< QuantizeDirection(Up.X) << '_'
		<< QuantizeDirection(Up.Y) << '_'
		<< QuantizeDirection(Up.Z) << '_'
		<< QuantizeDirection(Right.X) << '_'
		<< QuantizeDirection(Right.Y) << '_'
		<< QuantizeDirection(Right.Z) << '_'
		<< static_cast<int32>(IsDragging())
		<< '_'
		<< static_cast<int32>(ActiveAxis);
	return Stream.str();
}

FVector4 FGizmo::GetHandleTint(EGizmoAxis InAxis) const
{
	const EGizmoAxis DisplayAxis = (ActiveAxis != EGizmoAxis::None) ? ActiveAxis : HoveredAxis;
	return (DisplayAxis == InAxis)
		? FVector4(1.0f, 1.0f, 0.0f, 1.0f)
		: FVector4(1.0f, 1.0f, 1.0f, 1.0f);
}
