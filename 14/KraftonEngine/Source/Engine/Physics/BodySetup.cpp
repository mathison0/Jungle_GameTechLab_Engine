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

	struct FBoundaryEdgeRecord
	{
		uint32 A = 0;
		uint32 B = 0;
		int32 Count = 0;
	};

	struct FBoundaryLoop
	{
		TArray<uint32> Vertices;
		float Area2D = 0.0f;
		float Length2D = 0.0f;
	};

	FString MakeBoundaryEdgeKey(uint32 A, uint32 B)
	{
		if (A > B)
		{
			std::swap(A, B);
		}

		return std::to_string(A) + "_" + std::to_string(B);
	}

	float ComputeLoopArea2D(const TArray<uint32>& LoopVertices, const TArray<FVector>& Positions)
	{
		if (LoopVertices.size() < 3)
		{
			return 0.0f;
		}

		float Area = 0.0f;

		for (size_t Index = 0; Index < LoopVertices.size(); ++Index)
		{
			const uint32 AIndex = LoopVertices[Index];
			const uint32 BIndex = LoopVertices[(Index + 1) % LoopVertices.size()];

			if (AIndex >= Positions.size() || BIndex >= Positions.size())
			{
				continue;
			}

			const FVector& A = Positions[AIndex];
			const FVector& B = Positions[BIndex];

			Area += A.X * B.Y - B.X * A.Y;
		}

		return Area * 0.5f;
	}

	float ComputeLoopLength2D(const TArray<uint32>& LoopVertices, const TArray<FVector>& Positions)
	{
		if (LoopVertices.size() < 2)
		{
			return 0.0f;
		}

		float Length = 0.0f;

		for (size_t Index = 0; Index < LoopVertices.size(); ++Index)
		{
			const uint32 AIndex = LoopVertices[Index];
			const uint32 BIndex = LoopVertices[(Index + 1) % LoopVertices.size()];

			if (AIndex >= Positions.size() || BIndex >= Positions.size())
			{
				continue;
			}

			FVector Delta = Positions[BIndex] - Positions[AIndex];
			Delta.Z = 0.0f;
			Length += Delta.Length();
		}

		return Length;
	}

	TArray<FBoundaryLoop> BuildBoundaryLoops(
		const TArray<FBoundaryEdgeRecord>& BoundaryEdges,
		const TArray<FVector>& Positions)
	{
		TMap<uint32, TArray<uint32>> Adjacency;

		for (const FBoundaryEdgeRecord& Edge : BoundaryEdges)
		{
			if (Edge.A >= Positions.size() || Edge.B >= Positions.size())
			{
				continue;
			}

			Adjacency[Edge.A].push_back(Edge.B);
			Adjacency[Edge.B].push_back(Edge.A);
		}

		TMap<FString, bool> VisitedEdges;
		TArray<FBoundaryLoop> Loops;

		for (const FBoundaryEdgeRecord& StartEdge : BoundaryEdges)
		{
			const FString StartKey = MakeBoundaryEdgeKey(StartEdge.A, StartEdge.B);
			auto VisitedIt = VisitedEdges.find(StartKey);
			if (VisitedIt != VisitedEdges.end() && VisitedIt->second)
			{
				continue;
			}

			TArray<uint32> LoopVertices;
			uint32 Prev = UINT32_MAX;
			uint32 Current = StartEdge.A;
			uint32 Next = StartEdge.B;

			LoopVertices.push_back(Current);

			while (true)
			{
				const FString EdgeKey = MakeBoundaryEdgeKey(Current, Next);
				if (VisitedEdges[EdgeKey])
				{
					break;
				}

				VisitedEdges[EdgeKey] = true;
				LoopVertices.push_back(Next);

				Prev = Current;
				Current = Next;

				auto AdjIt = Adjacency.find(Current);
				if (AdjIt == Adjacency.end())
				{
					break;
				}

				const TArray<uint32>& Neighbors = AdjIt->second;
				uint32 Candidate = UINT32_MAX;

				for (uint32 Neighbor : Neighbors)
				{
					if (Neighbor == Prev)
					{
						continue;
					}

					const FString CandidateKey = MakeBoundaryEdgeKey(Current, Neighbor);
					if (!VisitedEdges[CandidateKey])
					{
						Candidate = Neighbor;
						break;
					}
				}

				if (Candidate == UINT32_MAX)
				{
					break;
				}

				Next = Candidate;

				if (Next == LoopVertices[0])
				{
					const FString ClosingKey = MakeBoundaryEdgeKey(Current, Next);
					if (!VisitedEdges[ClosingKey])
					{
						VisitedEdges[ClosingKey] = true;
					}
					break;
				}
			}

			if (LoopVertices.size() >= 3)
			{
				FBoundaryLoop Loop;
				Loop.Vertices = std::move(LoopVertices);
				Loop.Area2D = ComputeLoopArea2D(Loop.Vertices, Positions);
				Loop.Length2D = ComputeLoopLength2D(Loop.Vertices, Positions);
				Loops.push_back(std::move(Loop));
			}
		}

		return Loops;
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

int32 UBodySetup::BuildBoundaryBlockersFromComplexCollision(const FStaticMeshBlockerBuildSettings& Settings)
{
	if (!HasComplexCollision()) return 0;

	if (Settings.bClearExistingBlockers)
	{
		ClearShapes();
	}

	TMap<FString, FBoundaryEdgeRecord> EdgeRecords;

	auto AddEdge = [&](uint32 A, uint32 B)
	{
		if (A >= ComplexCollisionVertices.size() || B >= ComplexCollisionVertices.size()) return;

		const FString Key = MakeBoundaryEdgeKey(A, B);
		FBoundaryEdgeRecord& Record = EdgeRecords[Key];

		if (Record.Count == 0)
		{
			Record.A = std::min(A, B);
			Record.B = std::max(A, B);
		}

		++Record.Count;
	};

	for (size_t Index = 0; Index + 2 < ComplexCollisionIndices.size(); Index += 3)
	{
		const uint32 I0 = ComplexCollisionIndices[Index + 0];
		const uint32 I1 = ComplexCollisionIndices[Index + 1];
		const uint32 I2 = ComplexCollisionIndices[Index + 2];

		AddEdge(I0, I1);
		AddEdge(I1, I2);
		AddEdge(I2, I0);
	}

	TArray<FBoundaryEdgeRecord> BoundaryEdges;

	for (const auto& Pair : EdgeRecords)
	{
		const FBoundaryEdgeRecord& Record = Pair.second;

		if (Record.Count == 1)
		{
			BoundaryEdges.push_back(Record);
		}
	}

	TArray<FBoundaryLoop> BoundaryLoops =
		BuildBoundaryLoops(BoundaryEdges, ComplexCollisionVertices);

	TArray<const FBoundaryLoop*> SelectedLoops;

	if (Settings.bOnlyLargestLoop)
	{
		const FBoundaryLoop* LargestLoop = nullptr;

		for (const FBoundaryLoop& Loop : BoundaryLoops)
		{
			const float AbsArea = std::abs(Loop.Area2D);
			if (AbsArea < Settings.MinLoopArea)
			{
				continue;
			}

			if (!LargestLoop || AbsArea > std::abs(LargestLoop->Area2D))
			{
				LargestLoop = &Loop;
			}
		}

		if (LargestLoop)
		{
			SelectedLoops.push_back(LargestLoop);
		}
	}
	else
	{
		for (const FBoundaryLoop& Loop : BoundaryLoops)
		{
			if (std::abs(Loop.Area2D) >= Settings.MinLoopArea)
			{
				SelectedLoops.push_back(&Loop);
			}
		}
	}

	int32 CreatedBoxCount = 0;

	for (const FBoundaryLoop* Loop : SelectedLoops)
	{
		if (!Loop || Loop->Vertices.size() < 2)
		{
			continue;
		}

		for (size_t Index = 0; Index < Loop->Vertices.size(); ++Index)
		{
			const uint32 AIndex = Loop->Vertices[Index];
			const uint32 BIndex = Loop->Vertices[(Index + 1) % Loop->Vertices.size()];

			if (AIndex >= ComplexCollisionVertices.size() || BIndex >= ComplexCollisionVertices.size())
			{
				continue;
			}

			const FVector P0 = ComplexCollisionVertices[AIndex];
			const FVector P1 = ComplexCollisionVertices[BIndex];

			FVector EdgeVec = P1 - P0;
			EdgeVec.Z = 0.0f;

			const float Length = EdgeVec.Length();
			if (Length < Settings.MinEdgeLength)
			{
				continue;
			}

			const FVector EdgeDir = EdgeVec / Length;

			const FVector Center =
				(P0 + P1) * 0.5f
				+ FVector(0.0f, 0.0f, Settings.Height * 0.5f);

			const FVector Extents(
				Length * 0.5f,
				Settings.Thickness * 0.5f,
				Settings.Height * 0.5f);

			const float Yaw = std::atan2(EdgeDir.Y, EdgeDir.X);
			const FQuat Rotation = FQuat::FromAxisAngle(FVector::ZAxisVector, Yaw);

			AddBox(Center, Rotation, Extents);
			++CreatedBoxCount;
		}
	}

	return CreatedBoxCount;
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
