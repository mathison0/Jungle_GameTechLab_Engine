#include "Game/Crowd/CrowdSpatialPartition.h"

#include <algorithm>
#include <cmath>

namespace
{
	float DistanceSquaredXY(const FVector& A, const FVector& B)
	{
		const float DX = A.X - B.X;
		const float DY = A.Y - B.Y;
		return DX * DX + DY * DY;
	}
}

void FCrowdSpatialPartition::Clear()
{
	SpatialGrid.clear();
}

void FCrowdSpatialPartition::Rebuild(const TArray<FCrowdUnit>& Units, float InCellSize)
{
	CellSize = (std::max)(InCellSize, 0.5f);
	SpatialGrid.clear();
	SpatialGrid.reserve(Units.size());

	for (uint32 Index = 0; Index < static_cast<uint32>(Units.size()); ++Index)
	{
		const FCrowdUnit& Unit = Units[Index];
		if (!Unit.bAlive)
		{
			continue;
		}

		const int32 CellX = GetCellCoord(Unit.Position.X);
		const int32 CellY = GetCellCoord(Unit.Position.Y);
		SpatialGrid[MakeCellKey(CellX, CellY)].push_back(Index);
	}
}

void FCrowdSpatialPartition::QueryUnitsInRadius(
	const TArray<FCrowdUnit>& Units,
	const FVector& Center,
	float Radius,
	TArray<uint32>& OutIndices) const
{
	OutIndices.clear();

	if (Radius <= 0.0f)
	{
		return;
	}

	const int32 MinX = GetCellCoord(Center.X - Radius);
	const int32 MaxX = GetCellCoord(Center.X + Radius);
	const int32 MinY = GetCellCoord(Center.Y - Radius);
	const int32 MaxY = GetCellCoord(Center.Y + Radius);
	const float RadiusSq = Radius * Radius;

	for (int32 CellX = MinX; CellX <= MaxX; ++CellX)
	{
		for (int32 CellY = MinY; CellY <= MaxY; ++CellY)
		{
			auto It = SpatialGrid.find(MakeCellKey(CellX, CellY));
			if (It == SpatialGrid.end())
			{
				continue;
			}

			for (uint32 UnitIndex : It->second)
			{
				if (UnitIndex >= Units.size())
				{
					continue;
				}

				const FCrowdUnit& Unit = Units[UnitIndex];
				if (Unit.bAlive && DistanceSquaredXY(Unit.Position, Center) <= RadiusSq)
				{
					OutIndices.push_back(UnitIndex);
				}
			}
		}
	}
}

int32 FCrowdSpatialPartition::GetCellCoord(float Value) const
{
	return static_cast<int32>(std::floor(Value / CellSize));
}

int64 FCrowdSpatialPartition::MakeCellKey(int32 CellX, int32 CellY)
{
	return (static_cast<int64>(static_cast<uint32>(CellX)) << 32)
		| static_cast<uint32>(CellY);
}
