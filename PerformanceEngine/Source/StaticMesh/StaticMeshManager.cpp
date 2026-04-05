#include "StaticMesh/StaticMeshManager.h"

#include <algorithm>
#include <cctype>
#include <string>

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
	return Mesh;
}

TArray<FString> FStaticMeshManager::GetLoadedMeshAssetKeys() const
{
	TArray<FString> LoadedMeshAssetKeys;
	LoadedMeshAssetKeys.reserve(MeshCache.size());

	for (const auto& [AssetKey, Mesh] : MeshCache)
	{
		if (Mesh && Mesh->IsValid())
		{
			LoadedMeshAssetKeys.push_back(AssetKey);
		}
	}

	return LoadedMeshAssetKeys;
}

std::shared_ptr<FStaticMesh> FStaticMeshManager::FindLoadedStaticMesh(const FString& InAssetKey) const
{
	const auto MeshIt = MeshCache.find(InAssetKey);
	if (MeshIt == MeshCache.end())
	{
		return {};
	}

	return MeshIt->second;
}

void FStaticMeshManager::Release()
{
	MeshCache.clear();
}
