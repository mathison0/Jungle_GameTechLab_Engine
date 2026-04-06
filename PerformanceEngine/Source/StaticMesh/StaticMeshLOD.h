#pragma once

#include <array>

#include "StaticMesh.h"

namespace StaticMeshLOD
{
	const std::array<float, 3>& GetTriangleRatios();
	FStaticMesh::FLODLevel GenerateLODLevel(const FStaticMeshSourceData& InSourceData, float InTriangleRatio);
	bool FinalizeLODLevel(ID3D11Device* InDevice, FStaticMesh::FLODLevel& InOutLODLevel);
}
