#include "StaticMesh.h"

#include <algorithm>
#include <array>
#include <string>
#include <unordered_map>

#include <WICTextureLoader.h>

#include "StaticMeshLOD.h"
#include "Types/PathUtils.h"

bool FStaticMesh::Initialize(ID3D11Device* InDevice, ID3D11DeviceContext* InDeviceContext, FStaticMeshSourceData InSourceData)
{
	Release();

	if (InDevice == nullptr || InDeviceContext == nullptr || !InSourceData.IsValid())
	{
		return false;
	}

	SourcePath = std::move(InSourceData.SourcePath);
	const std::array<float, 3>& TriangleRatios = StaticMeshLOD::GetTriangleRatios();
	LODLevels.reserve(TriangleRatios.size());
	for (float TriangleRatio : TriangleRatios)
	{
		FLODLevel LODLevel = StaticMeshLOD::GenerateLODLevel(InSourceData, TriangleRatio);
		if (!StaticMeshLOD::FinalizeLODLevel(InDevice, LODLevel))
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
			const std::string TextureKey = PathUtils::NormalizeAbsolutePathUtf8(Material.DiffuseTexturePath);
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
