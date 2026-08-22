#include "ArrowComponent.h"

#include "Object/Class.h"
#include "Serializer/Archive.h"

#include <algorithm>

namespace
{
	void AppendLine(FDynamicMesh& Mesh, const FVector& Start, const FVector& End, const FVector4& Color)
	{
		const uint32 BaseIndex = static_cast<uint32>(Mesh.Vertices.size());

		FVertex StartVertex;
		StartVertex.Position = Start;
		StartVertex.Color = Color;
		StartVertex.Normal = FVector::ZeroVector;

		FVertex EndVertex;
		EndVertex.Position = End;
		EndVertex.Color = Color;
		EndVertex.Normal = FVector::ZeroVector;

		Mesh.Vertices.push_back(StartVertex);
		Mesh.Vertices.push_back(EndVertex);
		Mesh.Indices.push_back(BaseIndex);
		Mesh.Indices.push_back(BaseIndex + 1);
	}
}

IMPLEMENT_RTTI(UArrowComponent, UPrimitiveComponent)

void UArrowComponent::PostConstruct()
{
	bDrawDebugBounds = false;

	ArrowMesh = std::make_shared<FDynamicMesh>();
	ArrowMesh->Topology = EMeshTopology::EMT_LineList;
	RebuildMesh();
}

void UArrowComponent::OnRegister()
{
	UPrimitiveComponent::OnRegister();
	UpdateBounds();
}

void UArrowComponent::DuplicateSubObjects()
{
	UPrimitiveComponent::DuplicateSubObjects();

	ArrowMesh = std::make_shared<FDynamicMesh>();
	ArrowMesh->Topology = EMeshTopology::EMT_LineList;
	RebuildMesh();
}

void UArrowComponent::Serialize(FArchive& Ar)
{
	UPrimitiveComponent::Serialize(Ar);

	Ar.Serialize("ArrowColor", ArrowColor);
	Ar.Serialize("ArrowLength", ArrowLength);
	Ar.Serialize("ArrowHeadLength", ArrowHeadLength);
	Ar.Serialize("ArrowHeadSize", ArrowHeadSize);

	if (Ar.IsLoading())
	{
		RebuildMesh();
		UpdateBounds();
	}
}

FRenderMesh* UArrowComponent::GetRenderMesh() const
{
	return ArrowMesh.get();
}

FBoxSphereBounds UArrowComponent::GetLocalBounds() const
{
	if (ArrowMesh)
	{
		return {
			ArrowMesh->GetCenterCoord(),
			ArrowMesh->GetLocalBoundRadius(),
			(ArrowMesh->GetMaxCoord() - ArrowMesh->GetMinCoord()) * 0.5f
		};
	}

	return { FVector::ZeroVector, 0.0f, FVector::ZeroVector };
}

void UArrowComponent::SetArrowColor(const FVector4& InArrowColor)
{
	ArrowColor = InArrowColor;
	RebuildMesh();
	UpdateBounds();
}

void UArrowComponent::RebuildMesh()
{
	if (!ArrowMesh)
	{
		return;
	}

	ArrowMesh->Vertices.clear();
	ArrowMesh->Indices.clear();
	ArrowMesh->Topology = EMeshTopology::EMT_LineList;

	const float ClampedLength = (std::max)(ArrowLength, 0.1f);
	const float ClampedHeadLength = (std::max)(ArrowHeadLength, 0.05f);
	const float ClampedHeadSize = (std::max)(ArrowHeadSize, 0.02f);
	const float ArrowTipX = ClampedLength;
	const float ArrowBaseX = ArrowTipX - ClampedHeadLength;

	AppendLine(*ArrowMesh, FVector(0.0f, 0.0f, 0.0f), FVector(ArrowTipX, 0.0f, 0.0f), ArrowColor);
	AppendLine(*ArrowMesh, FVector(ArrowTipX, 0.0f, 0.0f), FVector(ArrowBaseX, ClampedHeadSize, ClampedHeadSize), ArrowColor);
	AppendLine(*ArrowMesh, FVector(ArrowTipX, 0.0f, 0.0f), FVector(ArrowBaseX, -ClampedHeadSize, ClampedHeadSize), ArrowColor);
	AppendLine(*ArrowMesh, FVector(ArrowTipX, 0.0f, 0.0f), FVector(ArrowBaseX, ClampedHeadSize, -ClampedHeadSize), ArrowColor);
	AppendLine(*ArrowMesh, FVector(ArrowTipX, 0.0f, 0.0f), FVector(ArrowBaseX, -ClampedHeadSize, -ClampedHeadSize), ArrowColor);

	ArrowMesh->bIsDirty = true;
	ArrowMesh->UpdateLocalBound();
}
