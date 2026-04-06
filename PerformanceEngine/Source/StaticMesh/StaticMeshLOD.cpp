#include "StaticMeshLOD.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <WICTextureLoader.h>

#include "Graphics/D3D11/D3D11Utils.h"

namespace
{
	constexpr std::array<float, 3> LODTriangleRatios = { 1.0f, 0.6f, 0.3f };
	constexpr float QuadricSolveTolerance = 1.0e-6f;
	constexpr size_t MinTrianglesForLOD1Simplification = 64;
	constexpr size_t MinTrianglesForLOD2Simplification = 128;

	struct FQuadric
	{
		double M00 = 0.0;
		double M01 = 0.0;
		double M02 = 0.0;
		double M03 = 0.0;
		double M11 = 0.0;
		double M12 = 0.0;
		double M13 = 0.0;
		double M22 = 0.0;
		double M23 = 0.0;
		double M33 = 0.0;

		FQuadric& operator+=(const FQuadric& InOther)
		{
			M00 += InOther.M00;
			M01 += InOther.M01;
			M02 += InOther.M02;
			M03 += InOther.M03;
			M11 += InOther.M11;
			M12 += InOther.M12;
			M13 += InOther.M13;
			M22 += InOther.M22;
			M23 += InOther.M23;
			M33 += InOther.M33;
			return *this;
		}

		FQuadric operator+(const FQuadric& InOther) const
		{
			FQuadric Result = *this;
			Result += InOther;
			return Result;
		}

		static FQuadric FromPlane(float A, float B, float C, float D)
		{
			FQuadric Result;
			Result.M00 = static_cast<double>(A) * A;
			Result.M01 = static_cast<double>(A) * B;
			Result.M02 = static_cast<double>(A) * C;
			Result.M03 = static_cast<double>(A) * D;
			Result.M11 = static_cast<double>(B) * B;
			Result.M12 = static_cast<double>(B) * C;
			Result.M13 = static_cast<double>(B) * D;
			Result.M22 = static_cast<double>(C) * C;
			Result.M23 = static_cast<double>(C) * D;
			Result.M33 = static_cast<double>(D) * D;
			return Result;
		}

		double Evaluate(const FVector& InPosition) const
		{
			const double X = InPosition.X;
			const double Y = InPosition.Y;
			const double Z = InPosition.Z;
			return
				M00 * X * X + 2.0 * M01 * X * Y + 2.0 * M02 * X * Z + 2.0 * M03 * X +
				M11 * Y * Y + 2.0 * M12 * Y * Z + 2.0 * M13 * Y +
				M22 * Z * Z + 2.0 * M23 * Z +
				M33;
		}
	};

	struct FSectionVertex
	{
		FVector Position = FVector::ZeroVector;
		FVector2 TexCoord;
		FQuadric Quadric;
		std::unordered_set<uint32> AdjacentTriangles;
		bool bBoundary = false;
		bool bUVSeam = false;
		bool bRemoved = false;
		uint32 Version = 0;
	};

	struct FSectionTriangle
	{
		uint32 Indices[3] = {};
		FQuadric Quadric;
		bool bRemoved = false;
	};

	struct FSectionEdge
	{
		uint32 A = 0;
		uint32 B = 0;

		bool operator==(const FSectionEdge& InOther) const
		{
			return A == InOther.A && B == InOther.B;
		}
	};

	struct FSectionEdgeHasher
	{
		size_t operator()(const FSectionEdge& InEdge) const
		{
			return (static_cast<size_t>(InEdge.A) << 32) ^ static_cast<size_t>(InEdge.B);
		}
	};

	struct FPositionKey
	{
		uint32 X = 0;
		uint32 Y = 0;
		uint32 Z = 0;

		bool operator==(const FPositionKey& InOther) const
		{
			return X == InOther.X && Y == InOther.Y && Z == InOther.Z;
		}
	};

	struct FPositionKeyHasher
	{
		size_t operator()(const FPositionKey& InKey) const
		{
			return (static_cast<size_t>(InKey.X) * 73856093ull)
				^ (static_cast<size_t>(InKey.Y) * 19349663ull)
				^ (static_cast<size_t>(InKey.Z) * 83492791ull);
		}
	};

	struct FSectionCollapse
	{
		uint32 A = 0;
		uint32 B = 0;
		FVector Position = FVector::ZeroVector;
		FVector2 TexCoord;
		double Cost = std::numeric_limits<double>::max();
		bool bValid = false;
	};

	struct FSectionSimplificationResult
	{
		TArray<FStaticMeshVertex> Vertices;
		TArray<uint32> Indices;
	};

	struct FQueuedCollapse
	{
		FSectionCollapse Collapse;
		uint32 VersionA = 0;
		uint32 VersionB = 0;

		bool operator<(const FQueuedCollapse& InOther) const
		{
			return Collapse.Cost > InOther.Collapse.Cost;
		}
	};

	FPositionKey MakePositionKey(const FVector& InPosition)
	{
		return
		{
			std::bit_cast<uint32>(InPosition.X),
			std::bit_cast<uint32>(InPosition.Y),
			std::bit_cast<uint32>(InPosition.Z)
		};
	}

	float TriangleAreaSquared(const FVector& InA, const FVector& InB, const FVector& InC)
	{
		return FVector::CrossProduct(InB - InA, InC - InA).SizeSquared();
	}

	FVector ComputeTriangleNormal(const FVector& InA, const FVector& InB, const FVector& InC)
	{
		return FVector::CrossProduct(InB - InA, InC - InA).GetSafeNormal();
	}

	bool CreateMeshBuffer(
		ID3D11Device* InDevice,
		UINT InByteWidth,
		D3D11_BIND_FLAG InBindFlags,
		const void* InInitialData,
		TComPtr<ID3D11Buffer>& OutBuffer)
	{
		return D3D11Utils::CreateImmutableBuffer(InDevice, InByteWidth, InBindFlags, InInitialData, OutBuffer);
	}

	void ComputeMeshBounds(
		const TArray<FStaticMeshVertex>& InVertices,
		FVector& OutBoundsMin,
		FVector& OutBoundsMax,
		FBoundingSphere& OutBoundsSphere)
	{
		if (InVertices.empty())
		{
			OutBoundsMin = FVector::ZeroVector;
			OutBoundsMax = FVector::ZeroVector;
			OutBoundsSphere = {};
			return;
		}

		OutBoundsMin = InVertices.front().Position;
		OutBoundsMax = InVertices.front().Position;
		for (const FStaticMeshVertex& Vertex : InVertices)
		{
			OutBoundsMin = FVector::Min(OutBoundsMin, Vertex.Position);
			OutBoundsMax = FVector::Max(OutBoundsMax, Vertex.Position);
		}

		OutBoundsSphere.Center = (OutBoundsMin + OutBoundsMax) * 0.5f;
		float MaxDistanceSquared = 0.0f;
		for (const FStaticMeshVertex& Vertex : InVertices)
		{
			MaxDistanceSquared = std::max(MaxDistanceSquared, FVector::DistSquared(Vertex.Position, OutBoundsSphere.Center));
		}

		OutBoundsSphere.Radius = std::sqrt(MaxDistanceSquared);
	}

	FStaticMesh::FLODLevel BuildLODLevel(
		const TArray<FStaticMeshVertex>& InVertices,
		const TArray<uint32>& InIndices,
		const TArray<FStaticMesh::FSection>& InSections,
		float InTriangleRatio)
	{
		FStaticMesh::FLODLevel LODLevel;
		LODLevel.Vertices = InVertices;
		LODLevel.Indices = InIndices;
		LODLevel.Sections = InSections;
		LODLevel.TriangleRatio = InTriangleRatio;
		ComputeMeshBounds(LODLevel.Vertices, LODLevel.BoundsMin, LODLevel.BoundsMax, LODLevel.BoundsSphere);
		return LODLevel;
	}

	bool FinalizeLODLevelInternal(ID3D11Device* InDevice, FStaticMesh::FLODLevel& InOutLODLevel)
	{
		if (InDevice == nullptr || InOutLODLevel.Vertices.empty() || InOutLODLevel.Indices.empty() || InOutLODLevel.Sections.empty())
		{
			return false;
		}

		return
			CreateMeshBuffer(
				InDevice,
				static_cast<UINT>(InOutLODLevel.Vertices.size() * sizeof(FStaticMeshVertex)),
				D3D11_BIND_VERTEX_BUFFER,
				InOutLODLevel.Vertices.data(),
				InOutLODLevel.VertexBuffer)
			&& CreateMeshBuffer(
				InDevice,
				static_cast<UINT>(InOutLODLevel.Indices.size() * sizeof(uint32)),
				D3D11_BIND_INDEX_BUFFER,
				InOutLODLevel.Indices.data(),
				InOutLODLevel.IndexBuffer);
	}

	bool SolveOptimalPosition(const FQuadric& InQuadric, FVector& OutPosition)
	{
		double Matrix[3][4] =
		{
			{ InQuadric.M00, InQuadric.M01, InQuadric.M02, -InQuadric.M03 },
			{ InQuadric.M01, InQuadric.M11, InQuadric.M12, -InQuadric.M13 },
			{ InQuadric.M02, InQuadric.M12, InQuadric.M22, -InQuadric.M23 }
		};

		for (int PivotIndex = 0; PivotIndex < 3; ++PivotIndex)
		{
			int BestRow = PivotIndex;
			double BestValue = std::abs(Matrix[PivotIndex][PivotIndex]);
			for (int Row = PivotIndex + 1; Row < 3; ++Row)
			{
				const double Value = std::abs(Matrix[Row][PivotIndex]);
				if (Value > BestValue)
				{
					BestValue = Value;
					BestRow = Row;
				}
			}

			if (BestValue <= QuadricSolveTolerance)
			{
				return false;
			}

			if (BestRow != PivotIndex)
			{
				for (int Column = PivotIndex; Column < 4; ++Column)
				{
					std::swap(Matrix[PivotIndex][Column], Matrix[BestRow][Column]);
				}
			}

			const double PivotValue = Matrix[PivotIndex][PivotIndex];
			for (int Column = PivotIndex; Column < 4; ++Column)
			{
				Matrix[PivotIndex][Column] /= PivotValue;
			}

			for (int Row = 0; Row < 3; ++Row)
			{
				if (Row == PivotIndex)
				{
					continue;
				}

				const double Scale = Matrix[Row][PivotIndex];
				for (int Column = PivotIndex; Column < 4; ++Column)
				{
					Matrix[Row][Column] -= Scale * Matrix[PivotIndex][Column];
				}
			}
		}

		OutPosition = FVector(
			static_cast<float>(Matrix[0][3]),
			static_cast<float>(Matrix[1][3]),
			static_cast<float>(Matrix[2][3]));
		return true;
	}

	bool ShouldSkipSectionSimplification(size_t InSourceTriangleCount, float InTriangleRatio)
	{
		if (InTriangleRatio >= 0.999f)
		{
			return true;
		}

		if (InTriangleRatio <= 0.35f)
		{
			return InSourceTriangleCount < MinTrianglesForLOD2Simplification;
		}

		return InSourceTriangleCount < MinTrianglesForLOD1Simplification;
	}

	FSectionEdge MakeEdge(uint32 InA, uint32 InB)
	{
		if (InA > InB)
		{
			std::swap(InA, InB);
		}

		return { InA, InB };
	}

	FQuadric ScaleQuadric(const FQuadric& InQuadric, double InScale)
	{
		FQuadric Result = InQuadric;
		Result.M00 *= InScale;
		Result.M01 *= InScale;
		Result.M02 *= InScale;
		Result.M03 *= InScale;
		Result.M11 *= InScale;
		Result.M12 *= InScale;
		Result.M13 *= InScale;
		Result.M22 *= InScale;
		Result.M23 *= InScale;
		Result.M33 *= InScale;
		return Result;
	}

	bool RecomputeTriangleQuadric(FSectionTriangle& InOutTriangle, const std::vector<FSectionVertex>& InVertices)
	{
		if (InOutTriangle.bRemoved)
		{
			InOutTriangle.Quadric = {};
			return false;
		}

		const FSectionVertex& Vertex0 = InVertices[InOutTriangle.Indices[0]];
		const FSectionVertex& Vertex1 = InVertices[InOutTriangle.Indices[1]];
		const FSectionVertex& Vertex2 = InVertices[InOutTriangle.Indices[2]];
		if (Vertex0.bRemoved || Vertex1.bRemoved || Vertex2.bRemoved)
		{
			InOutTriangle.Quadric = {};
			return false;
		}

		const FVector Edge01 = Vertex1.Position - Vertex0.Position;
		const FVector Edge02 = Vertex2.Position - Vertex0.Position;
		const FVector Normal = FVector::CrossProduct(Edge01, Edge02).GetSafeNormal();
		if (Normal.IsNearlyZero())
		{
			InOutTriangle.Quadric = {};
			return false;
		}

		const float PlaneDistance = -FVector::DotProduct(Normal, Vertex0.Position);
		InOutTriangle.Quadric = FQuadric::FromPlane(Normal.X, Normal.Y, Normal.Z, PlaneDistance);
		return true;
	}

	void AddTriangleAdjacency(
		std::vector<FSectionVertex>& InOutVertices,
		const FSectionTriangle& InTriangle,
		uint32 InTriangleIndex)
	{
		for (uint32 VertexIndex : InTriangle.Indices)
		{
			InOutVertices[VertexIndex].AdjacentTriangles.insert(InTriangleIndex);
		}
	}

	void RemoveTriangleAdjacency(
		std::vector<FSectionVertex>& InOutVertices,
		const FSectionTriangle& InTriangle,
		uint32 InTriangleIndex)
	{
		for (uint32 VertexIndex : InTriangle.Indices)
		{
			InOutVertices[VertexIndex].AdjacentTriangles.erase(InTriangleIndex);
		}
	}

	void AddTriangleEdges(
		const FSectionTriangle& InTriangle,
		std::unordered_map<FSectionEdge, uint32, FSectionEdgeHasher>& InOutEdgeUseCounts)
	{
		for (int EdgeIndex = 0; EdgeIndex < 3; ++EdgeIndex)
		{
			const uint32 A = InTriangle.Indices[EdgeIndex];
			const uint32 B = InTriangle.Indices[(EdgeIndex + 1) % 3];
			if (A != B)
			{
				++InOutEdgeUseCounts[MakeEdge(A, B)];
			}
		}
	}

	void RemoveTriangleEdges(
		const FSectionTriangle& InTriangle,
		std::unordered_map<FSectionEdge, uint32, FSectionEdgeHasher>& InOutEdgeUseCounts)
	{
		for (int EdgeIndex = 0; EdgeIndex < 3; ++EdgeIndex)
		{
			const uint32 A = InTriangle.Indices[EdgeIndex];
			const uint32 B = InTriangle.Indices[(EdgeIndex + 1) % 3];
			if (A == B)
			{
				continue;
			}

			const FSectionEdge Edge = MakeEdge(A, B);
			const auto EdgeIt = InOutEdgeUseCounts.find(Edge);
			if (EdgeIt == InOutEdgeUseCounts.end())
			{
				continue;
			}

			if (EdgeIt->second <= 1)
			{
				InOutEdgeUseCounts.erase(EdgeIt);
			}
			else
			{
				--EdgeIt->second;
			}
		}
	}

	void MarkUVSeams(std::vector<FSectionVertex>& InOutVertices)
	{
		std::unordered_map<FPositionKey, std::vector<uint32>, FPositionKeyHasher> VerticesByPosition;
		VerticesByPosition.reserve(InOutVertices.size());
		for (uint32 VertexIndex = 0; VertexIndex < static_cast<uint32>(InOutVertices.size()); ++VertexIndex)
		{
			if (InOutVertices[VertexIndex].bRemoved)
			{
				continue;
			}

			InOutVertices[VertexIndex].bUVSeam = false;
			VerticesByPosition[MakePositionKey(InOutVertices[VertexIndex].Position)].push_back(VertexIndex);
		}

		for (const auto& [PositionKey, VertexIndices] : VerticesByPosition)
		{
			(void)PositionKey;
			if (VertexIndices.size() < 2)
			{
				continue;
			}

			for (size_t LeftIndex = 0; LeftIndex < VertexIndices.size(); ++LeftIndex)
			{
				for (size_t RightIndex = LeftIndex + 1; RightIndex < VertexIndices.size(); ++RightIndex)
				{
					const FSectionVertex& LeftVertex = InOutVertices[VertexIndices[LeftIndex]];
					const FSectionVertex& RightVertex = InOutVertices[VertexIndices[RightIndex]];
					const bool bTexCoordMismatch =
						std::fabs(LeftVertex.TexCoord.X - RightVertex.TexCoord.X) > 1.0e-4f
						|| std::fabs(LeftVertex.TexCoord.Y - RightVertex.TexCoord.Y) > 1.0e-4f;
					if (!bTexCoordMismatch)
					{
						continue;
					}

					InOutVertices[VertexIndices[LeftIndex]].bUVSeam = true;
					InOutVertices[VertexIndices[RightIndex]].bUVSeam = true;
				}
			}
		}
	}

	void CollectCollapseTriangles(
		uint32 InA,
		uint32 InB,
		const std::vector<FSectionVertex>& InVertices,
		std::unordered_set<uint32>& OutTriangleIndices)
	{
		OutTriangleIndices.clear();
		OutTriangleIndices.insert(InVertices[InA].AdjacentTriangles.begin(), InVertices[InA].AdjacentTriangles.end());
		OutTriangleIndices.insert(InVertices[InB].AdjacentTriangles.begin(), InVertices[InB].AdjacentTriangles.end());
	}

	void RecomputeVertexQuadric(
		uint32 InVertexIndex,
		std::vector<FSectionVertex>& InOutVertices,
		const std::vector<FSectionTriangle>& InTriangles,
		const std::unordered_map<FSectionEdge, uint32, FSectionEdgeHasher>& InEdgeUseCounts)
	{
		FSectionVertex& Vertex = InOutVertices[InVertexIndex];
		if (Vertex.bRemoved)
		{
			Vertex.Quadric = {};
			Vertex.bBoundary = false;
			return;
		}

		Vertex.Quadric = {};
		Vertex.bBoundary = false;

		for (uint32 TriangleIndex : Vertex.AdjacentTriangles)
		{
			const FSectionTriangle& Triangle = InTriangles[TriangleIndex];
			if (Triangle.bRemoved)
			{
				continue;
			}

			Vertex.Quadric += Triangle.Quadric;
		}

		for (uint32 TriangleIndex : Vertex.AdjacentTriangles)
		{
			const FSectionTriangle& Triangle = InTriangles[TriangleIndex];
			if (Triangle.bRemoved)
			{
				continue;
			}

			for (int EdgeIndex = 0; EdgeIndex < 3; ++EdgeIndex)
			{
				const uint32 A = Triangle.Indices[EdgeIndex];
				const uint32 B = Triangle.Indices[(EdgeIndex + 1) % 3];
				if (A != InVertexIndex && B != InVertexIndex)
				{
					continue;
				}

				const auto EdgeUseIt = InEdgeUseCounts.find(MakeEdge(A, B));
				if (EdgeUseIt == InEdgeUseCounts.end() || EdgeUseIt->second != 1)
				{
					continue;
				}

				Vertex.bBoundary = true;
				const uint32 OtherIndex = (A == InVertexIndex) ? B : A;
				const FVector EdgeDirection = (InOutVertices[OtherIndex].Position - Vertex.Position).GetSafeNormal();
				const FVector Normal = ComputeTriangleNormal(
					InOutVertices[Triangle.Indices[0]].Position,
					InOutVertices[Triangle.Indices[1]].Position,
					InOutVertices[Triangle.Indices[2]].Position);
				const FVector BoundaryNormal = FVector::CrossProduct(EdgeDirection, Normal).GetSafeNormal();
				if (BoundaryNormal.IsNearlyZero())
				{
					continue;
				}

				const float BoundaryPlaneDistance = -FVector::DotProduct(BoundaryNormal, Vertex.Position);
				Vertex.Quadric += ScaleQuadric(
					FQuadric::FromPlane(BoundaryNormal.X, BoundaryNormal.Y, BoundaryNormal.Z, BoundaryPlaneDistance),
					10.0);
			}
		}
	}

	FSectionCollapse BuildCollapse(
		uint32 InA,
		uint32 InB,
		const std::vector<FSectionVertex>& InVertices)
	{
		FSectionCollapse Collapse;
		Collapse.A = InA;
		Collapse.B = InB;

		const FSectionVertex& VertexA = InVertices[InA];
		const FSectionVertex& VertexB = InVertices[InB];
		if (VertexA.bRemoved || VertexB.bRemoved)
		{
			return Collapse;
		}

		const FQuadric CombinedQuadric = VertexA.Quadric + VertexB.Quadric;
		const FVector MidPoint = (VertexA.Position + VertexB.Position) * 0.5f;
		FVector BestPosition = MidPoint;

		if (SolveOptimalPosition(CombinedQuadric, BestPosition))
		{
			Collapse.Position = BestPosition;
			Collapse.Cost = CombinedQuadric.Evaluate(BestPosition);
		}
		else
		{
			const std::array<FVector, 3> Candidates = { VertexA.Position, VertexB.Position, MidPoint };
			for (const FVector& Candidate : Candidates)
			{
				const double CandidateCost = CombinedQuadric.Evaluate(Candidate);
				if (CandidateCost < Collapse.Cost)
				{
					Collapse.Cost = CandidateCost;
					Collapse.Position = Candidate;
				}
			}
		}

		Collapse.TexCoord = FVector2(
			(VertexA.TexCoord.X + VertexB.TexCoord.X) * 0.5f,
			(VertexA.TexCoord.Y + VertexB.TexCoord.Y) * 0.5f);
		Collapse.bValid = true;
		return Collapse;
	}

	bool IsCollapseValid(
		const FSectionCollapse& InCollapse,
		const std::vector<FSectionVertex>& InVertices,
		const std::vector<FSectionTriangle>& InTriangles,
		const std::unordered_map<FSectionEdge, uint32, FSectionEdgeHasher>& InEdgeUseCounts)
	{
		const FSectionVertex& VertexA = InVertices[InCollapse.A];
		const FSectionVertex& VertexB = InVertices[InCollapse.B];
		if (VertexA.bRemoved || VertexB.bRemoved)
		{
			return false;
		}

		if (VertexA.bBoundary != VertexB.bBoundary)
		{
			return false;
		}

		if (VertexA.bUVSeam != VertexB.bUVSeam)
		{
			return false;
		}

		if (VertexA.bBoundary && VertexB.bBoundary)
		{
			const auto EdgeUseIt = InEdgeUseCounts.find(MakeEdge(InCollapse.A, InCollapse.B));
			if (EdgeUseIt == InEdgeUseCounts.end() || EdgeUseIt->second != 1)
			{
				return false;
			}
		}

		std::unordered_set<uint32> AffectedTriangles;
		CollectCollapseTriangles(InCollapse.A, InCollapse.B, InVertices, AffectedTriangles);
		for (uint32 TriangleIndex : AffectedTriangles)
		{
			const FSectionTriangle& Triangle = InTriangles[TriangleIndex];
			if (Triangle.bRemoved)
			{
				continue;
			}

			bool bUsesCollapseVertex = false;
			FVector OldPositions[3];
			FVector NewPositions[3];

			for (int CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
			{
				const uint32 VertexIndex = Triangle.Indices[CornerIndex];
				OldPositions[CornerIndex] = InVertices[VertexIndex].Position;
				NewPositions[CornerIndex] = OldPositions[CornerIndex];
				if (VertexIndex == InCollapse.A || VertexIndex == InCollapse.B)
				{
					NewPositions[CornerIndex] = InCollapse.Position;
					bUsesCollapseVertex = true;
				}
			}

			if (!bUsesCollapseVertex)
			{
				continue;
			}

			const bool bWouldDegenerate =
				(NewPositions[0] - NewPositions[1]).IsNearlyZero()
				|| (NewPositions[1] - NewPositions[2]).IsNearlyZero()
				|| (NewPositions[2] - NewPositions[0]).IsNearlyZero();
			if (bWouldDegenerate)
			{
				continue;
			}

			const float OldAreaSquared = TriangleAreaSquared(OldPositions[0], OldPositions[1], OldPositions[2]);
			const float NewAreaSquared = TriangleAreaSquared(NewPositions[0], NewPositions[1], NewPositions[2]);
			if (NewAreaSquared <= 1.0e-10f)
			{
				return false;
			}

			if (OldAreaSquared > 1.0e-8f && NewAreaSquared < OldAreaSquared * 0.05f)
			{
				return false;
			}

			const FVector OldNormal = ComputeTriangleNormal(OldPositions[0], OldPositions[1], OldPositions[2]);
			const FVector NewNormal = ComputeTriangleNormal(NewPositions[0], NewPositions[1], NewPositions[2]);
			if (!OldNormal.IsNearlyZero() && !NewNormal.IsNearlyZero())
			{
				const float NormalDot = FVector::DotProduct(OldNormal, NewNormal);
				if (NormalDot < 0.2f)
				{
					return false;
				}
			}
		}

		return true;
	}

	size_t CountActiveTriangles(const std::vector<FSectionTriangle>& InTriangles)
	{
		size_t Count = 0;
		for (const FSectionTriangle& Triangle : InTriangles)
		{
			if (!Triangle.bRemoved)
			{
				++Count;
			}
		}

		return Count;
	}

	FSectionSimplificationResult SimplifySectionQEM(
		const TArray<FStaticMeshVertex>& InVertices,
		const TArray<uint32>& InIndices,
		float InTriangleRatio)
	{
		FSectionSimplificationResult Result;
		if (InVertices.empty() || InIndices.size() < 3)
		{
			return Result;
		}

		const size_t SourceTriangleCount = InIndices.size() / 3;
		if (ShouldSkipSectionSimplification(SourceTriangleCount, InTriangleRatio))
		{
			Result.Vertices = InVertices;
			Result.Indices = InIndices;
			return Result;
		}

		const size_t TargetTriangleCount = std::max<size_t>(2, static_cast<size_t>(std::ceil(SourceTriangleCount * InTriangleRatio)));
		if (TargetTriangleCount >= SourceTriangleCount)
		{
			Result.Vertices = InVertices;
			Result.Indices = InIndices;
			return Result;
		}

		std::vector<FSectionVertex> Vertices;
		Vertices.reserve(InVertices.size());
		for (const FStaticMeshVertex& Vertex : InVertices)
		{
			FSectionVertex WorkingVertex;
			WorkingVertex.Position = Vertex.Position;
			WorkingVertex.TexCoord = Vertex.TexCoord;
			Vertices.push_back(WorkingVertex);
		}

		std::vector<FSectionTriangle> Triangles;
		Triangles.reserve(SourceTriangleCount);
		for (size_t Index = 0; Index + 2 < InIndices.size(); Index += 3)
		{
			FSectionTriangle Triangle;
			Triangle.Indices[0] = InIndices[Index];
			Triangle.Indices[1] = InIndices[Index + 1];
			Triangle.Indices[2] = InIndices[Index + 2];
			Triangles.push_back(Triangle);
		}

		std::unordered_map<FSectionEdge, uint32, FSectionEdgeHasher> EdgeUseCounts;
		EdgeUseCounts.reserve(Triangles.size() * 3);
		for (uint32 TriangleIndex = 0; TriangleIndex < static_cast<uint32>(Triangles.size()); ++TriangleIndex)
		{
			FSectionTriangle& Triangle = Triangles[TriangleIndex];
			RecomputeTriangleQuadric(Triangle, Vertices);
			AddTriangleAdjacency(Vertices, Triangle, TriangleIndex);
			AddTriangleEdges(Triangle, EdgeUseCounts);
		}

		MarkUVSeams(Vertices);
		for (uint32 VertexIndex = 0; VertexIndex < static_cast<uint32>(Vertices.size()); ++VertexIndex)
		{
			RecomputeVertexQuadric(VertexIndex, Vertices, Triangles, EdgeUseCounts);
		}

		std::priority_queue<FQueuedCollapse> CollapseQueue;
		auto QueueEdgeCollapse =
			[&Vertices, &Triangles, &EdgeUseCounts, &CollapseQueue](uint32 InA, uint32 InB)
			{
				if (InA == InB || Vertices[InA].bRemoved || Vertices[InB].bRemoved)
				{
					return;
				}

				if (InA > InB)
				{
					std::swap(InA, InB);
				}

				FSectionCollapse Collapse = BuildCollapse(InA, InB, Vertices);
				if (!Collapse.bValid)
				{
					return;
				}

				if (Vertices[InA].bBoundary || Vertices[InA].bUVSeam)
				{
					const FQuadric CombinedQuadric = Vertices[InA].Quadric + Vertices[InB].Quadric;
					const std::array<FVector, 2> ProtectedCandidates = { Vertices[InA].Position, Vertices[InB].Position };
					Collapse.Cost = std::numeric_limits<double>::max();
					for (const FVector& Candidate : ProtectedCandidates)
					{
						const double CandidateCost = CombinedQuadric.Evaluate(Candidate);
						if (CandidateCost < Collapse.Cost)
						{
							Collapse.Cost = CandidateCost;
							Collapse.Position = Candidate;
						}
					}
				}

				if (!IsCollapseValid(Collapse, Vertices, Triangles, EdgeUseCounts))
				{
					return;
				}

				CollapseQueue.push({ Collapse, Vertices[InA].Version, Vertices[InB].Version });
			};

		auto QueueNeighborhood =
			[&Vertices, &Triangles, &QueueEdgeCollapse](uint32 InVertexIndex)
			{
				if (Vertices[InVertexIndex].bRemoved)
				{
					return;
				}

				std::unordered_set<uint32> NeighborVertices;
				for (uint32 TriangleIndex : Vertices[InVertexIndex].AdjacentTriangles)
				{
					const FSectionTriangle& Triangle = Triangles[TriangleIndex];
					if (Triangle.bRemoved)
					{
						continue;
					}

					for (uint32 TriangleVertexIndex : Triangle.Indices)
					{
						if (TriangleVertexIndex != InVertexIndex && !Vertices[TriangleVertexIndex].bRemoved)
						{
							NeighborVertices.insert(TriangleVertexIndex);
						}
					}
				}

				for (uint32 NeighborVertexIndex : NeighborVertices)
				{
					QueueEdgeCollapse(InVertexIndex, NeighborVertexIndex);
				}
			};

		for (const FSectionTriangle& Triangle : Triangles)
		{
			if (Triangle.bRemoved)
			{
				continue;
			}

			QueueEdgeCollapse(Triangle.Indices[0], Triangle.Indices[1]);
			QueueEdgeCollapse(Triangle.Indices[1], Triangle.Indices[2]);
			QueueEdgeCollapse(Triangle.Indices[2], Triangle.Indices[0]);
		}

		size_t ActiveTriangleCount = CountActiveTriangles(Triangles);
		while (ActiveTriangleCount > TargetTriangleCount && !CollapseQueue.empty())
		{
			FQueuedCollapse QueuedCollapse = CollapseQueue.top();
			CollapseQueue.pop();

			const FSectionCollapse& BestCollapse = QueuedCollapse.Collapse;
			if (Vertices[BestCollapse.A].bRemoved
				|| Vertices[BestCollapse.B].bRemoved
				|| Vertices[BestCollapse.A].Version != QueuedCollapse.VersionA
				|| Vertices[BestCollapse.B].Version != QueuedCollapse.VersionB
				|| !IsCollapseValid(BestCollapse, Vertices, Triangles, EdgeUseCounts))
			{
				continue;
			}

			std::unordered_set<uint32> AffectedTriangles;
			CollectCollapseTriangles(BestCollapse.A, BestCollapse.B, Vertices, AffectedTriangles);
			std::unordered_set<uint32> AffectedVertices;

			for (uint32 TriangleIndex : AffectedTriangles)
			{
				const FSectionTriangle& Triangle = Triangles[TriangleIndex];
				if (Triangle.bRemoved)
				{
					continue;
				}

				for (uint32 TriangleVertexIndex : Triangle.Indices)
				{
					AffectedVertices.insert(TriangleVertexIndex);
				}
			}

			FSectionVertex& VertexA = Vertices[BestCollapse.A];
			FSectionVertex& VertexB = Vertices[BestCollapse.B];
			VertexA.Position = BestCollapse.Position;
			VertexA.TexCoord = BestCollapse.TexCoord;
			++VertexA.Version;
			VertexB.bRemoved = true;
			++VertexB.Version;

			for (uint32 TriangleIndex : AffectedTriangles)
			{
				FSectionTriangle& Triangle = Triangles[TriangleIndex];
				if (Triangle.bRemoved)
				{
					continue;
				}

				RemoveTriangleEdges(Triangle, EdgeUseCounts);
				RemoveTriangleAdjacency(Vertices, Triangle, TriangleIndex);

				for (uint32& TriangleVertexIndex : Triangle.Indices)
				{
					if (TriangleVertexIndex == BestCollapse.B)
					{
						TriangleVertexIndex = BestCollapse.A;
					}
				}

				if (Triangle.Indices[0] == Triangle.Indices[1]
					|| Triangle.Indices[1] == Triangle.Indices[2]
					|| Triangle.Indices[2] == Triangle.Indices[0])
				{
					Triangle.bRemoved = true;
					Triangle.Quadric = {};
					--ActiveTriangleCount;
					continue;
				}

				RecomputeTriangleQuadric(Triangle, Vertices);
				AddTriangleAdjacency(Vertices, Triangle, TriangleIndex);
				AddTriangleEdges(Triangle, EdgeUseCounts);

				for (uint32 TriangleVertexIndex : Triangle.Indices)
				{
					AffectedVertices.insert(TriangleVertexIndex);
				}
			}

			AffectedVertices.insert(BestCollapse.A);
			for (auto VertexIt = AffectedVertices.begin(); VertexIt != AffectedVertices.end(); )
			{
				if (*VertexIt == BestCollapse.B || Vertices[*VertexIt].bRemoved)
				{
					VertexIt = AffectedVertices.erase(VertexIt);
				}
				else
				{
					++VertexIt;
				}
			}

			for (uint32 VertexIndex : AffectedVertices)
			{
				RecomputeVertexQuadric(VertexIndex, Vertices, Triangles, EdgeUseCounts);
				++Vertices[VertexIndex].Version;
			}

			for (uint32 VertexIndex : AffectedVertices)
			{
				QueueNeighborhood(VertexIndex);
			}

			ActiveTriangleCount = CountActiveTriangles(Triangles);
		}

		std::vector<uint32> Remap(Vertices.size(), UINT32_MAX);
		for (const FSectionTriangle& Triangle : Triangles)
		{
			if (Triangle.bRemoved)
			{
				continue;
			}

			for (uint32 TriangleIndex : Triangle.Indices)
			{
				if (TriangleIndex < Remap.size() && Remap[TriangleIndex] == UINT32_MAX)
				{
					Remap[TriangleIndex] = static_cast<uint32>(Result.Vertices.size());
					Result.Vertices.push_back({
						Vertices[TriangleIndex].Position,
						Vertices[TriangleIndex].TexCoord
					});
				}

				Result.Indices.push_back(Remap[TriangleIndex]);
			}
		}

		if (Result.Indices.size() < 3 || Result.Vertices.empty())
		{
			Result.Vertices = InVertices;
			Result.Indices = InIndices;
		}

		return Result;
	}

	FStaticMesh::FLODLevel GenerateLODLevelInternal(
		const FStaticMeshSourceData& InSourceData,
		float InTriangleRatio)
	{
		if (InTriangleRatio >= 0.999f)
		{
			TArray<FStaticMesh::FSection> Sections;
			Sections.reserve(InSourceData.Sections.size());
			for (const FStaticMeshSourceData::FSection& SourceSection : InSourceData.Sections)
			{
				Sections.push_back({ SourceSection.IndexStart, SourceSection.IndexCount, SourceSection.MaterialIndex });
			}

			return BuildLODLevel(InSourceData.Vertices, InSourceData.Indices, Sections, 1.0f);
		}

		TArray<FStaticMeshVertex> LODVertices;
		TArray<uint32> LODIndices;
		TArray<FStaticMesh::FSection> LODSections;
		LODSections.reserve(InSourceData.Sections.size());

		for (const FStaticMeshSourceData::FSection& SourceSection : InSourceData.Sections)
		{
			if (SourceSection.IndexCount < 3)
			{
				continue;
			}

			std::unordered_map<uint32, uint32> SectionVertexMap;
			TArray<FStaticMeshVertex> SectionVertices;
			TArray<uint32> SectionIndices;
			SectionVertices.reserve(SourceSection.IndexCount);
			SectionIndices.reserve(SourceSection.IndexCount);

			for (uint32 IndexOffset = 0; IndexOffset < SourceSection.IndexCount; ++IndexOffset)
			{
				const uint32 SourceIndex = InSourceData.Indices[SourceSection.IndexStart + IndexOffset];
				const auto ExistingVertexIt = SectionVertexMap.find(SourceIndex);
				if (ExistingVertexIt != SectionVertexMap.end())
				{
					SectionIndices.push_back(ExistingVertexIt->second);
					continue;
				}

				const uint32 NewIndex = static_cast<uint32>(SectionVertices.size());
				SectionVertexMap.emplace(SourceIndex, NewIndex);
				SectionVertices.push_back(InSourceData.Vertices[SourceIndex]);
				SectionIndices.push_back(NewIndex);
			}

			const FSectionSimplificationResult SimplifiedSection = SimplifySectionQEM(SectionVertices, SectionIndices, InTriangleRatio);
			if (SimplifiedSection.Indices.size() < 3 || SimplifiedSection.Vertices.empty())
			{
				continue;
			}

			const uint32 SectionIndexStart = static_cast<uint32>(LODIndices.size());
			const uint32 VertexBase = static_cast<uint32>(LODVertices.size());
			LODVertices.insert(LODVertices.end(), SimplifiedSection.Vertices.begin(), SimplifiedSection.Vertices.end());
			for (uint32 SectionIndex : SimplifiedSection.Indices)
			{
				LODIndices.push_back(VertexBase + SectionIndex);
			}

			LODSections.push_back({
				SectionIndexStart,
				static_cast<uint32>(SimplifiedSection.Indices.size()),
				SourceSection.MaterialIndex
			});
		}

		if (LODVertices.empty() || LODIndices.empty() || LODSections.empty())
		{
			return GenerateLODLevelInternal(InSourceData, 1.0f);
		}

		return BuildLODLevel(LODVertices, LODIndices, LODSections, InTriangleRatio);
	}
}

namespace StaticMeshLOD
{
	const std::array<float, 3>& GetTriangleRatios()
	{
		return LODTriangleRatios;
	}

	bool FinalizeLODLevel(ID3D11Device* InDevice, FStaticMesh::FLODLevel& InOutLODLevel)
	{
		return FinalizeLODLevelInternal(InDevice, InOutLODLevel);
	}

	FStaticMesh::FLODLevel GenerateLODLevel(const FStaticMeshSourceData& InSourceData, float InTriangleRatio)
	{
		return GenerateLODLevelInternal(InSourceData, InTriangleRatio);
	}
}

