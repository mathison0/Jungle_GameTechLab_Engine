#pragma once

#include "Game/Crowd/CrowdUnitTypes.h"

class FCrowdSpatialPartition
{
public:
	void Clear();
	void Rebuild(const TArray<FCrowdUnit>& Units, float CellSize);
	void QueryUnitsInRadius(
		const TArray<FCrowdUnit>& Units,
		const FVector& Center,
		float Radius,
		TArray<uint32>& OutIndices) const;

	bool IsEmpty() const { return SpatialGrid.empty(); }

private:
	int32 GetCellCoord(float Value) const;
	static int64 MakeCellKey(int32 CellX, int32 CellY);

private:
	float CellSize = 4.0f;
	TMap<int64, TArray<uint32>> SpatialGrid;
};
