#include "Physics/BodySetup.h"

#include "Mesh/Static/StaticMeshAsset.h"
#include "Mesh/MeshSimplifier.h"
#include "Physics/PhysXInclude.h"
#include "Serialization/Archive.h"

namespace
{
	struct FCollisionBuildMesh
	{
		TArray<FNormalVertex> Vertices;
		TArray<uint32> Indices;
	};

	FCollisionBuildMesh BuildCandidateWalkableMesh(
		const FStaticMesh& Mesh,
		float MaxSlopeDegrees,
		float MinTriangleArea)
	{
		FCollisionBuildMesh Result;

		const float MinNormalZ = std::cos(MaxSlopeDegrees * FMath::DegToRad);

		TMap<uint32, uint32> Remap;

		auto AddVertex = [&](uint32 OldIndex) -> uint32
		{
			auto It = Remap.find(OldIndex);
			if (It != Remap.end())
			{
				return It->second;
			}

			const uint32 NewIndex = static_cast<uint32>(Result.Vertices.size());
			Remap[OldIndex] = NewIndex;
			Result.Vertices.push_back(Mesh.Vertices[OldIndex]);
			return NewIndex;
		};

		for (size_t i = 0; i + 2 < Mesh.Indices.size(); i += 3)
		{
			const uint32 I0 = Mesh.Indices[i + 0];
			const uint32 I1 = Mesh.Indices[i + 1];
			const uint32 I2 = Mesh.Indices[i + 2];

			if (I0 >= Mesh.Vertices.size() || I1 >= Mesh.Vertices.size() || I2 >= Mesh.Vertices.size())
			{
				continue;
			}

			const FVector& P0 = Mesh.Vertices[I0].pos;
			const FVector& P1 = Mesh.Vertices[I1].pos;
			const FVector& P2 = Mesh.Vertices[I2].pos;

			FVector N = (P1 - P0).Cross(P2 - P0);
			const float Area2 = N.Length();
			if (Area2 < MinTriangleArea * 2.0f)
			{
				continue;
			}

			N = N / Area2;

			// winding 뒤집힘 대응
			if (std::abs(N.Z) < MinNormalZ)
			{
				continue;
			}

			Result.Indices.push_back(AddVertex(I0));
			Result.Indices.push_back(AddVertex(I1));
			Result.Indices.push_back(AddVertex(I2));
		}

		return Result;
	}

	struct FQuantizedPos
	{
		int32 X;
		int32 Y;
		int32 Z;

		bool operator==(const FQuantizedPos& Other) const
		{
			return X == Other.X && Y == Other.Y && Z == Other.Z;
		}
	};

	FCollisionBuildMesh WeldCollisionMesh(const FCollisionBuildMesh& InMesh, float WeldEpsilon)
	{
		FCollisionBuildMesh OutMesh;
		if (WeldEpsilon <= 0.0f)
		{
			return InMesh;
		}

		TMap<FString, uint32> CellToVertex;
		TArray<uint32> Remap;
		Remap.resize(InMesh.Vertices.size(), UINT32_MAX);

		auto MakeKey = [&](const FVector& P)
		{
			const int32 X = static_cast<int32>(std::floor(P.X / WeldEpsilon));
			const int32 Y = static_cast<int32>(std::floor(P.Y / WeldEpsilon));
			const int32 Z = static_cast<int32>(std::floor(P.Z / WeldEpsilon));

			return std::to_string(X) + "_" + std::to_string(Y) + "_" + std::to_string(Z);
		};

		for (uint32 i = 0; i < static_cast<uint32>(InMesh.Vertices.size()); ++i)
		{
			const FString Key = MakeKey(InMesh.Vertices[i].pos);

			auto It = CellToVertex.find(Key);
			if (It != CellToVertex.end())
			{
				Remap[i] = It->second;
				continue;
			}

			const uint32 NewIndex = static_cast<uint32>(OutMesh.Vertices.size());
			CellToVertex[Key] = NewIndex;
			Remap[i] = NewIndex;
			OutMesh.Vertices.push_back(InMesh.Vertices[i]);
		}

		for (size_t i = 0; i + 2 < InMesh.Indices.size(); i += 3)
		{
			const uint32 A = Remap[InMesh.Indices[i + 0]];
			const uint32 B = Remap[InMesh.Indices[i + 1]];
			const uint32 C = Remap[InMesh.Indices[i + 2]];

			if (A == B || B == C || A == C)
			{
				continue;
			}

			OutMesh.Indices.push_back(A);
			OutMesh.Indices.push_back(B);
			OutMesh.Indices.push_back(C);
		}

		return OutMesh;
	}

	FCollisionBuildMesh SimplifyCollisionMesh(
		const FCollisionBuildMesh& InMesh,
		float TargetRatio)
	{
		FCollisionBuildMesh OutMesh;

		TArray<FStaticMeshSection> Sections;
		FStaticMeshSection Section;
		Section.FirstIndex = 0;
		Section.NumTriangles = static_cast<uint32>(InMesh.Indices.size() / 3);
		Section.MaterialIndex = 0;
		Section.MaterialSlotName = "Collision";
		Sections.push_back(Section);

		FSimplifiedMesh Simplified = FMeshSimplifier::Simplify(
			InMesh.Vertices,
			InMesh.Indices,
			Sections,
			TargetRatio);

		OutMesh.Vertices = std::move(Simplified.Vertices);
		OutMesh.Indices = std::move(Simplified.Indices);
		return OutMesh;
	}

	FCollisionBuildMesh RemoveSmallTriangleIslands(
		const FCollisionBuildMesh& InMesh,
		int32 MinIslandTriangleCount)
	{
		FCollisionBuildMesh OutMesh;

		const uint32 TriangleCount = static_cast<uint32>(InMesh.Indices.size() / 3);
		if (TriangleCount == 0)
		{
			return OutMesh;
		}

		if (MinIslandTriangleCount <= 1)
		{
			return InMesh;
		}

		// vertex -> triangles adjacency
		TArray<TArray<uint32>> VertexToTriangles;
		VertexToTriangles.resize(InMesh.Vertices.size());

		for (uint32 TriIndex = 0; TriIndex < TriangleCount; ++TriIndex)
		{
			const uint32 A = InMesh.Indices[TriIndex * 3 + 0];
			const uint32 B = InMesh.Indices[TriIndex * 3 + 1];
			const uint32 C = InMesh.Indices[TriIndex * 3 + 2];

			if (A < VertexToTriangles.size()) VertexToTriangles[A].push_back(TriIndex);
			if (B < VertexToTriangles.size()) VertexToTriangles[B].push_back(TriIndex);
			if (C < VertexToTriangles.size()) VertexToTriangles[C].push_back(TriIndex);
		}

		TArray<int32> IslandIds;
		IslandIds.resize(TriangleCount, -1);

		TArray<uint32> IslandTriangleCounts;
		TArray<uint32> Stack;

		int32 IslandId = 0;

		for (uint32 StartTri = 0; StartTri < TriangleCount; ++StartTri)
		{
			if (IslandIds[StartTri] != -1)
			{
				continue;
			}

			uint32 Count = 0;
			Stack.clear();
			Stack.push_back(StartTri);
			IslandIds[StartTri] = IslandId;

			while (!Stack.empty())
			{
				const uint32 Tri = Stack.back();
				Stack.pop_back();
				++Count;

				const uint32 V[3] =
				{
					InMesh.Indices[Tri * 3 + 0],
					InMesh.Indices[Tri * 3 + 1],
					InMesh.Indices[Tri * 3 + 2]
				};

				for (uint32 K = 0; K < 3; ++K)
				{
					const uint32 VertexIndex = V[K];
					if (VertexIndex >= VertexToTriangles.size())
					{
						continue;
					}

					for (uint32 NeighborTri : VertexToTriangles[VertexIndex])
					{
						if (IslandIds[NeighborTri] != -1)
						{
							continue;
						}

						IslandIds[NeighborTri] = IslandId;
						Stack.push_back(NeighborTri);
					}
				}
			}

			IslandTriangleCounts.push_back(Count);
			++IslandId;
		}

		// 살아남을 triangle만 vertex remap해서 복사
		TArray<uint32> VertexRemap;
		VertexRemap.resize(InMesh.Vertices.size(), UINT32_MAX);

		auto AddVertex = [&](uint32 OldVertexIndex) -> uint32
		{
			uint32& NewIndex = VertexRemap[OldVertexIndex];
			if (NewIndex != UINT32_MAX)
			{
				return NewIndex;
			}

			NewIndex = static_cast<uint32>(OutMesh.Vertices.size());
			OutMesh.Vertices.push_back(InMesh.Vertices[OldVertexIndex]);
			return NewIndex;
		};

		for (uint32 TriIndex = 0; TriIndex < TriangleCount; ++TriIndex)
		{
			const int32 Id = IslandIds[TriIndex];
			if (Id < 0)
			{
				continue;
			}

			if (IslandTriangleCounts[Id] < static_cast<uint32>(MinIslandTriangleCount))
			{
				continue;
			}

			const uint32 A = InMesh.Indices[TriIndex * 3 + 0];
			const uint32 B = InMesh.Indices[TriIndex * 3 + 1];
			const uint32 C = InMesh.Indices[TriIndex * 3 + 2];

			const uint32 NewA = AddVertex(A);
			const uint32 NewB = AddVertex(B);
			const uint32 NewC = AddVertex(C);

			if (NewA == NewB || NewB == NewC || NewA == NewC)
			{
				continue;
			}

			OutMesh.Indices.push_back(NewA);
			OutMesh.Indices.push_back(NewB);
			OutMesh.Indices.push_back(NewC);
		}

		return OutMesh;
	}
}

UBodySetup::~UBodySetup()
{
	if (CookedTriangleMesh)
	{
		CookedTriangleMesh->release();
		CookedTriangleMesh = nullptr;
	}
}

void UBodySetup::Serialize(FArchive& Ar)
{
	Ar << BoneName;
	Ar << AggGeom;
}

void UBodySetup::SerializeComplexCollision(FArchive& Ar)
{
	Ar << ComplexCollisionVertices;
	Ar << ComplexCollisionIndices;

	if (Ar.IsLoading() && CookedTriangleMesh)
	{
		CookedTriangleMesh->release();
		CookedTriangleMesh = nullptr;
	}
}

void UBodySetup::CreateDefaultBox(const FVector& Center, const FVector& Extents)
{
	AggGeom.BoxElems.clear();

	FKBoxElem Box;
	Box.Center = Center;
	Box.Rotation = FQuat::Identity;
	Box.Extents = Extents;

	AggGeom.BoxElems.push_back(Box);
}

void UBodySetup::AddSphere(const FVector& Center, float Radius)
{
	FKSphereElem Sphere;
	Sphere.Center = Center;
	Sphere.Radius = Radius;

	AggGeom.SphereElems.push_back(Sphere);
}

void UBodySetup::AddBox(const FVector& Center, const FQuat& Rotation, const FVector& Extents)
{
	FKBoxElem Box;
	Box.Center = Center;
	Box.Rotation = Rotation;
	Box.Extents = Extents;

	AggGeom.BoxElems.push_back(Box);
}

void UBodySetup::AddSphyl(const FVector& Center, const FQuat& Rotation, float Radius, float Length)
{
	FKSphylElem Sphyl;
	Sphyl.Center = Center;
	Sphyl.Rotation = Rotation;
	Sphyl.Radius = Radius;
	Sphyl.Length = Length;
	
	AggGeom.SphylElems.push_back(Sphyl);
}

int32 UBodySetup::GetShapeCount(EPhysicsAssetShapeType ShapeType) const
{
	switch (ShapeType)
	{
	case EPhysicsAssetShapeType::Sphere:
		return static_cast<int32>(AggGeom.SphereElems.size());
	case EPhysicsAssetShapeType::Box:
		return static_cast<int32>(AggGeom.BoxElems.size());
	case EPhysicsAssetShapeType::Sphyl:
		return static_cast<int32>(AggGeom.SphylElems.size());
	case EPhysicsAssetShapeType::Convex:
		return static_cast<int32>(AggGeom.ConvexElems.size());
	default:
		return 0;
	}
}

bool UBodySetup::RemoveShape(EPhysicsAssetShapeType ShapeType, int32 ShapeIndex)
{
	if (ShapeIndex < 0)
	{
		return false;
	}

	switch (ShapeType)
	{
	case EPhysicsAssetShapeType::Sphere:
		if (ShapeIndex >= static_cast<int32>(AggGeom.SphereElems.size())) return false;
		AggGeom.SphereElems.erase(AggGeom.SphereElems.begin() + ShapeIndex);
		return true;
	case EPhysicsAssetShapeType::Box:
		if (ShapeIndex >= static_cast<int32>(AggGeom.BoxElems.size())) return false;
		AggGeom.BoxElems.erase(AggGeom.BoxElems.begin() + ShapeIndex);
		return true;
	case EPhysicsAssetShapeType::Sphyl:
		if (ShapeIndex >= static_cast<int32>(AggGeom.SphylElems.size())) return false;
		AggGeom.SphylElems.erase(AggGeom.SphylElems.begin() + ShapeIndex);
		return true;
	case EPhysicsAssetShapeType::Convex:
		if (ShapeIndex >= static_cast<int32>(AggGeom.ConvexElems.size())) return false;
		AggGeom.ConvexElems.erase(AggGeom.ConvexElems.begin() + ShapeIndex);
		return true;
	default:
		return false;
	}
}

void UBodySetup::ClearShapes()
{
	AggGeom.SphereElems.clear();
	AggGeom.BoxElems.clear();
	AggGeom.SphylElems.clear();
	AggGeom.ConvexElems.clear();
}

void UBodySetup::ClearComplexCollision()
{
	if (CookedTriangleMesh)
	{
		CookedTriangleMesh->release();
		CookedTriangleMesh = nullptr;
	}

	ComplexCollisionVertices.clear();
	ComplexCollisionIndices.clear();
}

void UBodySetup::BuildComplexCollisionFromStaticMesh(const FStaticMesh& Mesh)
{
	ClearComplexCollision();

	if (Mesh.Vertices.empty() || Mesh.Indices.size() < 3) return;

	ComplexCollisionVertices.reserve(Mesh.Vertices.size());
	for (const FNormalVertex& Vertex : Mesh.Vertices)
	{
		ComplexCollisionVertices.push_back(Vertex.pos);
	}

	ComplexCollisionIndices.reserve(Mesh.Indices.size());
	for (uint32 Index : Mesh.Indices)
	{
		if (Index >= static_cast<uint32>(ComplexCollisionVertices.size()))
		{
			ComplexCollisionVertices.clear();
			ComplexCollisionIndices.clear();
			return;
		}

		ComplexCollisionIndices.push_back(Index);
	}
}

void UBodySetup::BuildWalkableCollisionFromStaticMesh(const FStaticMesh& Mesh, float MaxSlopeDegrees, float MinTriangleArea, int32 MinIslandTriangleCount)
{
	FStaticMeshCollisionBuildSettings Settings;
	Settings.Mode = EStaticMeshComplexCollisionMode::WalkableOnly;
	Settings.MaxSlopeDegrees = MaxSlopeDegrees;
	Settings.MinTriangleArea = MinTriangleArea;
	Settings.MinIslandTriangleCount = MinIslandTriangleCount;

	BuildWalkableCollisionFromStaticMesh(Mesh, Settings);
}

void UBodySetup::BuildWalkableCollisionFromStaticMesh(const FStaticMesh& Mesh, const FStaticMeshCollisionBuildSettings& Settings)
{
	ClearComplexCollision();

	FCollisionBuildMesh CollisionMesh =
		BuildCandidateWalkableMesh(Mesh, Settings.MaxSlopeDegrees, Settings.MinTriangleArea);

	CollisionMesh = WeldCollisionMesh(CollisionMesh, Settings.WeldEpsilon);

	CollisionMesh = RemoveSmallTriangleIslands(
		CollisionMesh,
		Settings.MinIslandTriangleCount);

	if (Settings.bSimplify
		&& Settings.SimplifyTargetRatio < 1.0f
		&& CollisionMesh.Indices.size() / 3 > static_cast<size_t>(Settings.SimplifyAboveTriangleCount))
	{
		CollisionMesh = SimplifyCollisionMesh(CollisionMesh, Settings.SimplifyTargetRatio);
	}

	ComplexCollisionVertices.reserve(CollisionMesh.Vertices.size());
	for (const FNormalVertex& V : CollisionMesh.Vertices)
	{
		ComplexCollisionVertices.push_back(V.pos);
	}

	ComplexCollisionIndices = std::move(CollisionMesh.Indices);
}

void UBodySetup::RebuildComplexCollisionFromStaticMesh(const FStaticMesh& Mesh, const FStaticMeshCollisionBuildSettings& Settings)
{
	switch (Settings.Mode)
	{
	case EStaticMeshComplexCollisionMode::WalkableOnly:
		BuildWalkableCollisionFromStaticMesh(Mesh, Settings);
		break;
	case EStaticMeshComplexCollisionMode::FullMesh:
	default:
		BuildComplexCollisionFromStaticMesh(Mesh);
		break;
	}
}

void UBodySetup::SetCookedTriangleMesh(physx::PxTriangleMesh* InMesh) const
{
	if (CookedTriangleMesh == InMesh) return;

	if (CookedTriangleMesh)
	{
		CookedTriangleMesh->release();
	}

	CookedTriangleMesh = InMesh;
}
