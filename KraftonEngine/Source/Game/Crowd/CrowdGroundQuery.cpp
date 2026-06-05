#include "Game/Crowd/CrowdGroundQuery.h"

#include "Collision/Ray/RayUtils.h"
#include "Component/Primitive/StaticMeshComponent.h"
#include "Core/Logging/Log.h"
#include "Core/Types/CollisionTypes.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Mesh/Static/StaticMesh.h"
#include "Physics/BodySetup.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace
{
	constexpr float DegToRad = 0.01745329251994329577f;

	bool IsValidTriangleIndex(const TArray<FVector>& Vertices, uint32 Index)
	{
		return Index < static_cast<uint32>(Vertices.size());
	}

	bool IsFallbackGroundCandidate(UStaticMeshComponent* Component)
	{
		return Component
			&& Component->IsQueryCollisionEnabled()
			&& Component->GetCollisionObjectType() == ECollisionChannel::WorldStatic;
	}
}

void FCrowdGroundQuery::Clear()
{
	Triangles.clear();
	Grid.clear();
	bUsedFallbackWithoutTag = false;
	CandidateComponentCount = 0;
}

void FCrowdGroundQuery::Rebuild(UWorld* World, const FCrowdGroundBuildParams& Params)
{
	Clear();

	if (!World)
	{
		return;
	}

	CellSize = std::max(Params.CellSize, 0.5f);
	const float ClampedSlope = std::clamp(Params.WalkableSlopeAngle, 0.0f, 89.0f);
	const float MinNormalZ = std::cos(ClampedSlope * DegToRad);

	int32 TaggedActorCount = 0;
	for (AActor* Actor : World->GetActors())
	{
		if (!Actor || !Actor->HasTag(Params.GroundActorTag))
		{
			continue;
		}

		++TaggedActorCount;
		for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
		{
			AddStaticMeshComponent(Cast<UStaticMeshComponent>(Primitive), MinNormalZ);
		}
	}

	if (TaggedActorCount == 0 && Params.bAllowFallbackWithoutTag)
	{
		bUsedFallbackWithoutTag = true;
		const FString TagString = Params.GroundActorTag.ToString();
		UE_LOG("[CrowdGroundQuery] no actor with tag '%s'; falling back to WorldStatic query static meshes", TagString.c_str());

		for (AActor* Actor : World->GetActors())
		{
			if (!Actor)
			{
				continue;
			}

			for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
			{
				UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Primitive);
				if (IsFallbackGroundCandidate(StaticMeshComponent))
				{
					AddStaticMeshComponent(StaticMeshComponent, MinNormalZ);
				}
			}
		}
	}

	if (Triangles.empty())
	{
		const FString TagString = Params.GroundActorTag.ToString();
		UE_LOG("[CrowdGroundQuery] built no walkable triangles (tag='%s', taggedActors=%d, components=%d)",
			TagString.c_str(), TaggedActorCount, CandidateComponentCount);
	}
	else
	{
		UE_LOG("[CrowdGroundQuery] built %d walkable triangles from %d components%s",
			static_cast<int32>(Triangles.size()),
			CandidateComponentCount,
			bUsedFallbackWithoutTag ? " using fallback" : "");
	}
}

void FCrowdGroundQuery::AddStaticMeshComponent(UStaticMeshComponent* Component, float MinNormalZ)
{
	if (!Component)
	{
		return;
	}

	UStaticMesh* StaticMesh = Component->GetStaticMesh();
	if (!StaticMesh)
	{
		return;
	}

	StaticMesh->BuildDefaultBodySetupIfNeeded();

	UBodySetup* BodySetup = StaticMesh->GetBodySetup();
	if (!BodySetup || !BodySetup->HasComplexCollision())
	{
		return;
	}

	++CandidateComponentCount;

	const TArray<FVector>& Vertices = BodySetup->GetComplexCollisionVertices();
	const TArray<uint32>& Indices = BodySetup->GetComplexCollisionIndices();
	const FMatrix& WorldMatrix = Component->GetWorldMatrix();

	for (size_t Index = 0; Index + 2 < Indices.size(); Index += 3)
	{
		const uint32 I0 = Indices[Index + 0];
		const uint32 I1 = Indices[Index + 1];
		const uint32 I2 = Indices[Index + 2];
		if (!IsValidTriangleIndex(Vertices, I0)
			|| !IsValidTriangleIndex(Vertices, I1)
			|| !IsValidTriangleIndex(Vertices, I2))
		{
			continue;
		}

		const FVector A = WorldMatrix.TransformPositionWithW(Vertices[I0]);
		const FVector B = WorldMatrix.TransformPositionWithW(Vertices[I1]);
		const FVector C = WorldMatrix.TransformPositionWithW(Vertices[I2]);

		FVector Normal = (B - A).Cross(C - A);
		const float NormalLength = Normal.Length();
		if (NormalLength <= 1.e-6f)
		{
			continue;
		}

		Normal = Normal / NormalLength;
		if (Normal.Z < 0.0f)
		{
			Normal = Normal * -1.0f;
		}

		if (Normal.Z < MinNormalZ)
		{
			continue;
		}

		const uint32 TriangleIndex = static_cast<uint32>(Triangles.size());
		Triangles.push_back({ A, B, C, Normal, Component });
		RegisterTriangle(TriangleIndex);
	}
}

bool FCrowdGroundQuery::SampleGround(const FVector& Position, const FCrowdGroundSampleParams& Params, FCrowdGroundHit& OutHit) const
{
	OutHit = FCrowdGroundHit();

	if (Triangles.empty())
	{
		return false;
	}

	const float TraceUp = std::max(Params.TraceUp, 0.0f);
	const float TraceDown = std::max(Params.TraceDown, 0.0f);
	const float MaxDistance = TraceUp + TraceDown;
	if (MaxDistance <= 0.0f)
	{
		return false;
	}

	const int32 CellX = GetCellCoord(Position.X);
	const int32 CellY = GetCellCoord(Position.Y);
	auto GridIt = Grid.find(MakeCellKey(CellX, CellY));
	if (GridIt == Grid.end())
	{
		return false;
	}

	const FVector RayOrigin(Position.X, Position.Y, Position.Z + TraceUp);
	const FVector RayDirection = FVector::DownVector;

	float ClosestT = FLT_MAX;
	const FGroundTriangle* BestTriangle = nullptr;

	for (uint32 TriangleIndex : GridIt->second)
	{
		if (TriangleIndex >= Triangles.size())
		{
			continue;
		}

		const FGroundTriangle& Triangle = Triangles[TriangleIndex];
		float T = 0.0f;
		if (!FRayUtils::IntersectTriangle(RayOrigin, RayDirection, Triangle.A, Triangle.B, Triangle.C, T))
		{
			continue;
		}

		if (T <= MaxDistance && T < ClosestT)
		{
			ClosestT = T;
			BestTriangle = &Triangle;
		}
	}

	if (!BestTriangle)
	{
		return false;
	}

	OutHit.Location = RayOrigin + RayDirection * ClosestT;
	OutHit.Location.Z += Params.HeightOffset;
	OutHit.Normal = BestTriangle->Normal;
	OutHit.Component = BestTriangle->Component;
	return true;
}

void FCrowdGroundQuery::RegisterTriangle(uint32 TriangleIndex)
{
	if (TriangleIndex >= Triangles.size())
	{
		return;
	}

	const FGroundTriangle& Triangle = Triangles[TriangleIndex];
	const float MinX = std::min({ Triangle.A.X, Triangle.B.X, Triangle.C.X });
	const float MaxX = std::max({ Triangle.A.X, Triangle.B.X, Triangle.C.X });
	const float MinY = std::min({ Triangle.A.Y, Triangle.B.Y, Triangle.C.Y });
	const float MaxY = std::max({ Triangle.A.Y, Triangle.B.Y, Triangle.C.Y });

	const int32 MinCellX = GetCellCoord(MinX);
	const int32 MaxCellX = GetCellCoord(MaxX);
	const int32 MinCellY = GetCellCoord(MinY);
	const int32 MaxCellY = GetCellCoord(MaxY);

	for (int32 CellX = MinCellX; CellX <= MaxCellX; ++CellX)
	{
		for (int32 CellY = MinCellY; CellY <= MaxCellY; ++CellY)
		{
			Grid[MakeCellKey(CellX, CellY)].push_back(TriangleIndex);
		}
	}
}

int32 FCrowdGroundQuery::GetCellCoord(float Value) const
{
	return static_cast<int32>(std::floor(Value / CellSize));
}

int64 FCrowdGroundQuery::MakeCellKey(int32 CellX, int32 CellY)
{
	return (static_cast<int64>(static_cast<uint32>(CellX)) << 32)
		| static_cast<uint32>(CellY);
}
