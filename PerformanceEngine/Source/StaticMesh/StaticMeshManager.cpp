#include "StaticMesh/StaticMeshManager.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>

#include <Windows.h>

#include "StaticMesh/ObjParser.h"
#include "StaticMesh/StaticMesh.h"

namespace
{
	std::string ToLowerCopy(std::string InValue)
	{
		std::ranges::transform(InValue
		                       ,
		                       InValue.begin(),
		                       [](unsigned char Ch) { return static_cast<char>(std::tolower(Ch)); });
		return InValue;
	}

	void LogMeshLODStats(const FStaticMesh& InMesh, bool bFromCache)
	{
		for (uint32 LODIndex = 0; LODIndex < InMesh.GetLODCount(); ++LODIndex)
		{
			const FStaticMesh::FLODLevel* LODLevel = InMesh.GetLODLevel(LODIndex);
			if (LODLevel == nullptr)
			{
				continue;
			}

			std::ostringstream LogStream;
			LogStream
				<< "[StaticMesh][LOD] "
				<< (bFromCache ? "Cache Hit" : "Imported")
				<< " asset=" << InMesh.GetSourcePath().generic_string()
				<< " lod=" << LODIndex
				<< " vertices=" << LODLevel->Vertices.size()
				<< " triangles=" << (LODLevel->Indices.size() / 3)
				<< " ratio=" << LODLevel->TriangleRatio
				<< '\n';
			OutputDebugStringA(LogStream.str().c_str());
		}
	}
}

FString FStaticMeshManager::BuildAssetKey(const std::filesystem::path& InAssetPath)
{
	if (InAssetPath.empty())
	{
		return {};
	}

	return std::filesystem::absolute(InAssetPath).lexically_normal().generic_string();
}

std::shared_ptr<FStaticMesh> FStaticMeshManager::LoadStaticMesh(
	ID3D11Device* InDevice,
	ID3D11DeviceContext* InDeviceContext,
	const std::filesystem::path& InAssetPath)
{
	if (InDevice == nullptr || InDeviceContext == nullptr || InAssetPath.empty())
	{
		return {};
	}

	const std::filesystem::path NormalizedPath = std::filesystem::absolute(InAssetPath).lexically_normal();
	const FString MeshCacheKey = BuildAssetKey(NormalizedPath);

	const auto ExistingMeshIt = MeshCache.find(MeshCacheKey);
	if (ExistingMeshIt != MeshCache.end())
	{
		if (ExistingMeshIt->second)
		{
			LogMeshLODStats(*ExistingMeshIt->second, true);
		}
		return ExistingMeshIt->second;
	}

	const std::string Extension = ToLowerCopy(NormalizedPath.extension().string());

	FStaticMeshSourceData SourceData;
	if (Extension == ".obj")
	{
		if (!FObjParser::Parse(NormalizedPath, SourceData))
		{
			return {};
		}
	}
	else
	{
		return {};
	}

	std::shared_ptr<FStaticMesh> Mesh = std::make_shared<FStaticMesh>();
	if (!Mesh || !Mesh->Initialize(InDevice, InDeviceContext, std::move(SourceData)))
	{
		return {};
	}

	MeshCache.emplace(MeshCacheKey, Mesh);
	LogMeshLODStats(*Mesh, false);
	return Mesh;
}

void FStaticMeshManager::Release()
{
	MeshCache.clear();
}
