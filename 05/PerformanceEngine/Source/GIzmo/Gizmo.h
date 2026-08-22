#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Graphics/D3D11/D3D11Common.h"
#include "Gizmo/GizmoLocalBVH.h"
#include "Math/Transform.h"
#include "Math/Vector4.h"
#include "Scene/SceneTypes.h"
#include "Types/PlatformTypes.h"

class FCamera;
class FD3D11RHI;
class FInput;
class FScene;
struct FPickState;

enum class EGizmoMode : uint8
{
	Translation,
	Rotation,
	Scale
};

enum class EGizmoCoordinateSpace : uint8
{
	World,
	Local
};

enum class EGizmoAxis : uint8
{
	None = 0,
	X,
	Y,
	Z,
	XY,
	XZ,
	YZ,
	XYZ,
	Screen
};

struct FGizmoDrawCommand
{
	ID3D11Buffer* VertexBuffer = nullptr;
	ID3D11Buffer* IndexBuffer = nullptr;
	uint32 IndexCount = 0;
	FMatrix WorldMatrix = FMatrix::Identity;
	FVector4 Tint = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	float ColorBlend = 0.0f;
};

class FGizmo
{
public:
	FGizmo() = default;
	~FGizmo() = default;

	bool Initialize(FD3D11RHI& InRHI);
	void Shutdown();

	void SetMode(EGizmoMode InMode);
	EGizmoMode GetMode() const { return Mode; }
	void ToggleCoordinateSpace();
	void SetCoordinateSpace(EGizmoCoordinateSpace InSpace);
	EGizmoCoordinateSpace GetCoordinateSpace() const { return CoordinateSpace; }

	void ClearHover();
	bool IsDragging() const { return ActiveAxis != EGizmoAxis::None; }
	bool HasRenderableSelection(const FScene& InScene, const FPickState& InPickState) const;

	void UpdateHover(
		const FScene& InScene,
		const FPickState& InPickState,
		const FCamera& InCamera,
		POINT InMousePositionClient,
		int32 InViewportWidth,
		int32 InViewportHeight);

	bool BeginDrag(
		FScene& InScene,
		const FPickState& InPickState,
		const FCamera& InCamera,
		POINT InMousePositionClient,
		int32 InViewportWidth,
		int32 InViewportHeight);

	bool UpdateDrag(
		FScene& InScene,
		const FPickState& InPickState,
		const FCamera& InCamera,
		POINT InMousePositionClient,
		int32 InViewportWidth,
		int32 InViewportHeight);

	void EndDrag();

	void BuildDrawCommands(
		const FScene& InScene,
		const FPickState& InPickState,
		const FCamera& InCamera,
		std::vector<FGizmoDrawCommand>& OutDrawCommands);

private:
	struct FCachedMeshEntry
	{
		FGizmoMesh Mesh;
		FGizmoLocalBVH LocalBVH;
		TComPtr<ID3D11Buffer> VertexBuffer;
		TComPtr<ID3D11Buffer> IndexBuffer;
		uint32 IndexCount = 0;
	};

	struct FHandleSet
	{
		std::shared_ptr<FCachedMeshEntry> Axis[3];
		std::shared_ptr<FCachedMeshEntry> Plane[3];
		std::shared_ptr<FCachedMeshEntry> Screen;
		std::shared_ptr<FCachedMeshEntry> Center;
	};

	struct FPickResult
	{
		bool bHit = false;
		EGizmoAxis Axis = EGizmoAxis::None;
		float Distance = 0.0f;
		FVector WorldPosition = FVector::ZeroVector;
	};

	bool EnsureTranslationHandleSet();
	bool EnsureScaleHandleSet();
	bool EnsureRotationHandleSet(const FCamera& InCamera);

	std::shared_ptr<FCachedMeshEntry> CreateCachedMeshEntry(const FGizmoMesh& InMesh) const;
	std::shared_ptr<FCachedMeshEntry> GetOrCreateCachedMesh(const std::string& InKey, const FGizmoMesh& InMesh);

	FPickResult PickGizmo(
		const FScene& InScene,
		const FPickState& InPickState,
		const FCamera& InCamera,
		POINT InMousePositionClient,
		int32 InViewportWidth,
		int32 InViewportHeight);

	bool BeginTranslationDrag(const FTransform& InSelectedTransform, EGizmoAxis InAxis, const FRay& InRay, const FCamera& InCamera);
	bool BeginRotationDrag(const FTransform& InSelectedTransform, EGizmoAxis InAxis, const FRay& InRay, const FCamera& InCamera);
	bool BeginScaleDrag(const FTransform& InSelectedTransform, EGizmoAxis InAxis, const FRay& InRay, const FCamera& InCamera, POINT InMousePositionClient);

	bool TryHitHandle(
		const std::shared_ptr<FCachedMeshEntry>& InMeshEntry,
		const FMatrix& InWorldMatrix,
		EGizmoAxis InAxis,
		const FRay& InWorldRay,
		FPickResult& InOutBestResult) const;

	void AppendHandleDrawCommand(
		const std::shared_ptr<FCachedMeshEntry>& InMeshEntry,
		const FMatrix& InWorldMatrix,
		EGizmoAxis InAxis,
		std::vector<FGizmoDrawCommand>& OutDrawCommands) const;

	const FTransform* ResolveSelectedTransform(const FScene& InScene, const FPickState& InPickState) const;
	FVector GetSelectedLocation(const FScene& InScene, const FPickState& InPickState) const;
	FQuat GetGizmoRotation(const FTransform& InSelectedTransform) const;
	FVector GetGizmoAxisVector(EGizmoAxis InAxis, const FTransform& InSelectedTransform) const;
	FVector GetGizmoPlaneNormal(EGizmoAxis InAxis, const FTransform& InSelectedTransform) const;
	FVector GetAxisVector(EGizmoAxis InAxis) const;
	FVector GetPlaneNormal(EGizmoAxis InAxis) const;
	float ComputeGizmoScale(const FVector& InWorldPosition, const FCamera& InCamera) const;
	float GetRenderGizmoScale(float InBaseScale) const;
	std::string BuildRotationCacheKey(const FCamera& InCamera) const;
	FVector4 GetHandleTint(EGizmoAxis InAxis) const;

private:
	ID3D11Device* Device = nullptr;

	EGizmoMode Mode = EGizmoMode::Translation;
	EGizmoCoordinateSpace CoordinateSpace = EGizmoCoordinateSpace::World;
	EGizmoAxis HoveredAxis = EGizmoAxis::None;
	EGizmoAxis ActiveAxis = EGizmoAxis::None;

	FHandleSet TranslationHandles;
	FHandleSet ScaleHandles;
	std::map<std::string, FHandleSet> RotationHandleCache;
	mutable std::map<std::string, std::shared_ptr<FCachedMeshEntry>> MeshCache;

	FTransform DragStartTransform = FTransform::Identity;
	FVector DragStartGizmoLocation = FVector::ZeroVector;
	FVector DragStartIntersection = FVector::ZeroVector;
	FVector DragPlaneNormal = FVector::ZeroVector;
	FVector DragStartRotationVector = FVector::ZeroVector;
	float DragStartAxisDistance = 0.0f;
	float CurrentRotationDeltaDegrees = 0.0f;
	int32 DragStartScreenX = 0;
	int32 DragStartScreenY = 0;
};
