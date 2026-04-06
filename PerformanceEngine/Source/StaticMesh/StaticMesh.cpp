#include "StaticMesh.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <WICTextureLoader.h>

#include "Graphics/D3D11/D3D11Utils.h"

namespace
{
	constexpr std::array<float, 3> LODTriangleRatios = { 1.0f, 0.8f, 0.6f };
	constexpr float QuadricSolveTolerance = 1.0e-6f;

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
		bool bBoundary = false;
		bool bUVSeam = false;
		bool bRemoved = false;
	};

	struct FSectionTriangle
	{
		uint32 Indices[3] = {};
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

	bool FinalizeLODLevel(ID3D11Device* InDevice, FStaticMesh::FLODLevel& InOutLODLevel)
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

	void RebuildVertexFlagsAndQuadrics(std::vector<FSectionVertex>& InOutVertices, const std::vector<FSectionTriangle>& InTriangles)
	{
		for (FSectionVertex& Vertex : InOutVertices)
		{
			Vertex.Quadric = {};
			Vertex.bBoundary = false;
			Vertex.bUVSeam = false;
		}

		std::unordered_map<FSectionEdge, uint32, FSectionEdgeHasher> EdgeUseCounts;
		EdgeUseCounts.reserve(InTriangles.size() * 3);

		for (const FSectionTriangle& Triangle : InTriangles)
		{
			if (Triangle.bRemoved)
			{
				continue;
			}

			for (int EdgeIndex = 0; EdgeIndex < 3; ++EdgeIndex)
			{
				uint32 A = Triangle.Indices[EdgeIndex];
				uint32 B = Triangle.Indices[(EdgeIndex + 1) % 3];
				if (A == B || InOutVertices[A].bRemoved || InOutVertices[B].bRemoved)
				{
					continue;
				}

				if (A > B)
				{
					std::swap(A, B);
				}

				++EdgeUseCounts[{ A, B }];
			}
		}

		for (const FSectionTriangle& Triangle : InTriangles)
		{
			if (Triangle.bRemoved)
			{
				continue;
			}

			FSectionVertex& Vertex0 = InOutVertices[Triangle.Indices[0]];
			FSectionVertex& Vertex1 = InOutVertices[Triangle.Indices[1]];
			FSectionVertex& Vertex2 = InOutVertices[Triangle.Indices[2]];
			if (Vertex0.bRemoved || Vertex1.bRemoved || Vertex2.bRemoved)
			{
				continue;
			}

			const FVector Edge01 = Vertex1.Position - Vertex0.Position;
			const FVector Edge02 = Vertex2.Position - Vertex0.Position;
			const FVector Normal = FVector::CrossProduct(Edge01, Edge02).GetSafeNormal();
			if (Normal.IsNearlyZero())
			{
				continue;
			}

			const float PlaneDistance = -FVector::DotProduct(Normal, Vertex0.Position);
			const FQuadric FaceQuadric = FQuadric::FromPlane(Normal.X, Normal.Y, Normal.Z, PlaneDistance);
			Vertex0.Quadric += FaceQuadric;
			Vertex1.Quadric += FaceQuadric;
			Vertex2.Quadric += FaceQuadric;

			for (int EdgeIndex = 0; EdgeIndex < 3; ++EdgeIndex)
			{
				uint32 A = Triangle.Indices[EdgeIndex];
				uint32 B = Triangle.Indices[(EdgeIndex + 1) % 3];
				if (A > B)
				{
					std::swap(A, B);
				}

				const auto EdgeUseIt = EdgeUseCounts.find({ A, B });
				if (EdgeUseIt == EdgeUseCounts.end() || EdgeUseIt->second != 1)
				{
					continue;
				}

				InOutVertices[A].bBoundary = true;
				InOutVertices[B].bBoundary = true;

				const FVector EdgeDirection = (InOutVertices[B].Position - InOutVertices[A].Position).GetSafeNormal();
				const FVector BoundaryNormal = FVector::CrossProduct(EdgeDirection, Normal).GetSafeNormal();
				if (BoundaryNormal.IsNearlyZero())
				{
					continue;
				}

				const float BoundaryPlaneDistance = -FVector::DotProduct(BoundaryNormal, InOutVertices[A].Position);
				FQuadric BoundaryQuadric = FQuadric::FromPlane(BoundaryNormal.X, BoundaryNormal.Y, BoundaryNormal.Z, BoundaryPlaneDistance);
				BoundaryQuadric.M00 *= 10.0;
				BoundaryQuadric.M01 *= 10.0;
				BoundaryQuadric.M02 *= 10.0;
				BoundaryQuadric.M03 *= 10.0;
				BoundaryQuadric.M11 *= 10.0;
				BoundaryQuadric.M12 *= 10.0;
				BoundaryQuadric.M13 *= 10.0;
				BoundaryQuadric.M22 *= 10.0;
				BoundaryQuadric.M23 *= 10.0;
				BoundaryQuadric.M33 *= 10.0;
				InOutVertices[A].Quadric += BoundaryQuadric;
				InOutVertices[B].Quadric += BoundaryQuadric;
			}
		}

		std::unordered_map<FPositionKey, std::vector<uint32>, FPositionKeyHasher> VerticesByPosition;
		VerticesByPosition.reserve(InOutVertices.size());
		for (uint32 VertexIndex = 0; VertexIndex < static_cast<uint32>(InOutVertices.size()); ++VertexIndex)
		{
			if (InOutVertices[VertexIndex].bRemoved)
			{
				continue;
			}

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

	bool IsCollapseValid(
		const FSectionCollapse& InCollapse,
		const std::vector<FSectionVertex>& InVertices,
		const std::vector<FSectionTriangle>& InTriangles)
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

		for (const FSectionTriangle& Triangle : InTriangles)
		{
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

		RebuildVertexFlagsAndQuadrics(Vertices, Triangles);

		size_t ActiveTriangleCount = CountActiveTriangles(Triangles);
		while (ActiveTriangleCount > TargetTriangleCount)
		{
			std::unordered_set<FSectionEdge, FSectionEdgeHasher> UniqueEdges;
			UniqueEdges.reserve(ActiveTriangleCount * 3);
			FSectionCollapse BestCollapse;

			for (const FSectionTriangle& Triangle : Triangles)
			{
				if (Triangle.bRemoved)
				{
					continue;
				}

				for (int EdgeIndex = 0; EdgeIndex < 3; ++EdgeIndex)
				{
					uint32 A = Triangle.Indices[EdgeIndex];
					uint32 B = Triangle.Indices[(EdgeIndex + 1) % 3];
					if (A == B || Vertices[A].bRemoved || Vertices[B].bRemoved)
					{
						continue;
					}

					if (A > B)
					{
						std::swap(A, B);
					}

					const FSectionEdge Edge{ A, B };
					if (!UniqueEdges.insert(Edge).second)
					{
						continue;
					}

					FSectionCollapse Collapse = BuildCollapse(A, B, Vertices);
					if (Collapse.bValid && (Vertices[A].bBoundary || Vertices[A].bUVSeam))
					{
						const std::array<FVector, 2> ProtectedCandidates = { Vertices[A].Position, Vertices[B].Position };
						Collapse.Cost = std::numeric_limits<double>::max();
						for (const FVector& Candidate : ProtectedCandidates)
						{
							const double CandidateCost = (Vertices[A].Quadric + Vertices[B].Quadric).Evaluate(Candidate);
							if (CandidateCost < Collapse.Cost)
							{
								Collapse.Cost = CandidateCost;
								Collapse.Position = Candidate;
							}
						}
					}

					if (!Collapse.bValid || !IsCollapseValid(Collapse, Vertices, Triangles))
					{
						continue;
					}

					if (Collapse.bValid && Collapse.Cost < BestCollapse.Cost)
					{
						BestCollapse = Collapse;
					}
				}
			}

			if (!BestCollapse.bValid)
			{
				break;
			}

			FSectionVertex& VertexA = Vertices[BestCollapse.A];
			FSectionVertex& VertexB = Vertices[BestCollapse.B];
			VertexA.Position = BestCollapse.Position;
			VertexA.TexCoord = BestCollapse.TexCoord;
			VertexA.Quadric += VertexB.Quadric;
			VertexB.bRemoved = true;

			for (FSectionTriangle& Triangle : Triangles)
			{
				if (Triangle.bRemoved)
				{
					continue;
				}

				for (uint32& TriangleIndex : Triangle.Indices)
				{
					if (TriangleIndex == BestCollapse.B)
					{
						TriangleIndex = BestCollapse.A;
					}
				}

				if (Triangle.Indices[0] == Triangle.Indices[1]
					|| Triangle.Indices[1] == Triangle.Indices[2]
					|| Triangle.Indices[2] == Triangle.Indices[0])
				{
					Triangle.bRemoved = true;
				}
			}

			RebuildVertexFlagsAndQuadrics(Vertices, Triangles);
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

	FStaticMesh::FLODLevel GenerateLODLevel(
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
			return GenerateLODLevel(InSourceData, 1.0f);
		}

		return BuildLODLevel(LODVertices, LODIndices, LODSections, InTriangleRatio);
	}
}

bool FStaticMesh::Initialize(ID3D11Device* InDevice, ID3D11DeviceContext* InDeviceContext, FStaticMeshSourceData InSourceData)
{
	Release();

	if (InDevice == nullptr || InDeviceContext == nullptr || !InSourceData.IsValid())
	{
		return false;
	}

	SourcePath = std::move(InSourceData.SourcePath);
	LODLevels.reserve(LODTriangleRatios.size());
	for (float TriangleRatio : LODTriangleRatios)
	{
		FLODLevel LODLevel = GenerateLODLevel(InSourceData, TriangleRatio);
		if (!FinalizeLODLevel(InDevice, LODLevel))
		{
			Release();
			return false;
		}

		LODLevels.push_back(std::move(LODLevel));
	}

	Materials.reserve(InSourceData.Materials.size());
	std::unordered_map<std::string, ID3D11ShaderResourceView*> TextureViewCache;
	for (const FStaticMeshSourceData::FMaterial& SourceMaterial : InSourceData.Materials)
	{
		FMaterial Material = {};
		Material.Name = SourceMaterial.Name;
		Material.DiffuseTexturePath = SourceMaterial.DiffuseTexturePath;

		if (!Material.DiffuseTexturePath.empty())
		{
			const std::string TextureKey = Material.DiffuseTexturePath.lexically_normal().generic_string();
			const auto ExistingTextureIt = TextureViewCache.find(TextureKey);
			if (ExistingTextureIt != TextureViewCache.end())
			{
				Material.DiffuseTextureView = ExistingTextureIt->second;
			}
			else
			{
				DirectX::CreateWICTextureFromFile(
					InDevice,
					InDeviceContext,
					Material.DiffuseTexturePath.c_str(),
					nullptr,
					Material.DiffuseTextureView.GetAddressOf());

				if (Material.DiffuseTextureView != nullptr)
				{
					TextureViewCache.emplace(TextureKey, Material.DiffuseTextureView.Get());
				}
			}
		}

		Materials.push_back(std::move(Material));
	}

	return IsValid();
}

void FStaticMesh::Release()
{
	SourcePath.clear();
	LODLevels.clear();
	Materials.clear();
	SpatialData.reset();
}

const FStaticMesh::FLODLevel* FStaticMesh::GetBaseLOD() const
{
	return GetLODLevel(0);
}

const FStaticMesh::FLODLevel* FStaticMesh::GetLODLevel(uint32 InLODIndex) const
{
	if (LODLevels.empty())
	{
		return nullptr;
	}

	const size_t LODIndex = std::min<size_t>(InLODIndex, LODLevels.size() - 1);
	return &LODLevels[LODIndex];
}

ID3D11Buffer* FStaticMesh::GetVertexBuffer() const
{
	return GetVertexBuffer(0);
}

ID3D11Buffer* FStaticMesh::GetIndexBuffer() const
{
	return GetIndexBuffer(0);
}

ID3D11Buffer* FStaticMesh::GetVertexBuffer(uint32 InLODIndex) const
{
	const FLODLevel* LODLevel = GetLODLevel(InLODIndex);
	return LODLevel ? LODLevel->VertexBuffer.Get() : nullptr;
}

ID3D11Buffer* FStaticMesh::GetIndexBuffer(uint32 InLODIndex) const
{
	const FLODLevel* LODLevel = GetLODLevel(InLODIndex);
	return LODLevel ? LODLevel->IndexBuffer.Get() : nullptr;
}

UINT FStaticMesh::GetVertexCount() const
{
	const FLODLevel* LODLevel = GetBaseLOD();
	return LODLevel ? static_cast<UINT>(LODLevel->Vertices.size()) : 0;
}

UINT FStaticMesh::GetIndexCount() const
{
	const FLODLevel* LODLevel = GetBaseLOD();
	return LODLevel ? static_cast<UINT>(LODLevel->Indices.size()) : 0;
}

bool FStaticMesh::IsValid() const
{
	const FLODLevel* LODLevel = GetBaseLOD();
	return LODLevel != nullptr && LODLevel->IsValid();
}

const TArray<FStaticMeshVertex>& FStaticMesh::GetVertices() const
{
	static const TArray<FStaticMeshVertex> EmptyVertices;
	const FLODLevel* LODLevel = GetBaseLOD();
	return LODLevel ? LODLevel->Vertices : EmptyVertices;
}

const TArray<uint32>& FStaticMesh::GetIndices() const
{
	static const TArray<uint32> EmptyIndices;
	const FLODLevel* LODLevel = GetBaseLOD();
	return LODLevel ? LODLevel->Indices : EmptyIndices;
}

const TArray<FStaticMesh::FSection>& FStaticMesh::GetSections() const
{
	return GetSections(0);
}

const TArray<FStaticMesh::FSection>& FStaticMesh::GetSections(uint32 InLODIndex) const
{
	static const TArray<FSection> EmptySections;
	const FLODLevel* LODLevel = GetLODLevel(InLODIndex);
	return LODLevel ? LODLevel->Sections : EmptySections;
}

const FVector& FStaticMesh::GetBoundsMin() const
{
	static const FVector EmptyBounds = FVector::ZeroVector;
	const FLODLevel* LODLevel = GetBaseLOD();
	return LODLevel ? LODLevel->BoundsMin : EmptyBounds;
}

const FVector& FStaticMesh::GetBoundsMax() const
{
	static const FVector EmptyBounds = FVector::ZeroVector;
	const FLODLevel* LODLevel = GetBaseLOD();
	return LODLevel ? LODLevel->BoundsMax : EmptyBounds;
}

const FBoundingSphere& FStaticMesh::GetBoundsSphere() const
{
	static const FBoundingSphere EmptySphere;
	const FLODLevel* LODLevel = GetBaseLOD();
	return LODLevel ? LODLevel->BoundsSphere : EmptySphere;
}

ID3D11ShaderResourceView* FStaticMesh::GetMaterialTexture(int32 InMaterialIndex) const
{
	if (InMaterialIndex < 0 || static_cast<size_t>(InMaterialIndex) >= Materials.size())
	{
		return nullptr;
	}

	return Materials[InMaterialIndex].DiffuseTextureView.Get();
}
