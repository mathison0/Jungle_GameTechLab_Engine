#pragma once

#include <filesystem>
#include <memory>

#include "Graphics/D3D11/D3D11Common.h"
#include "Math/Vector.h"
#include "Math/Vector2.h"
#include "Types/Array.h"
#include "BVH/BVHTypes.h"
#include "Types/PlatformTypes.h"
#include "Types/String.h"

struct FStaticMeshVertex
{
	FVector Position;
	FVector2 TexCoord;
};

struct FBoundingSphere
{
	FVector Center = FVector::ZeroVector;
	float Radius = 0.0f;
};

struct FStaticMeshSourceData
{
	struct FMaterial
	{
		FString Name;
		std::filesystem::path DiffuseTexturePath;
	};

	struct FSection
	{
		uint32 IndexStart = 0;
		uint32 IndexCount = 0;
		int32 MaterialIndex = -1;
	};

	std::filesystem::path SourcePath;
	TArray<FStaticMeshVertex> Vertices;
	TArray<uint32> Indices;
	TArray<FMaterial> Materials;
	TArray<FSection> Sections;
	FVector BoundsMin = FVector::ZeroVector;
	FVector BoundsMax = FVector::ZeroVector;
	FBoundingSphere BoundsSphere;

	bool IsValid() const { return !Vertices.empty() && !Indices.empty() && !Sections.empty(); }
};

class IStaticMeshSpatialData
{
public:
	virtual ~IStaticMeshSpatialData() = default;
};

class FStaticMesh
{
public:
	struct FMaterial
	{
		FString Name;
		std::filesystem::path DiffuseTexturePath;
		TComPtr<ID3D11ShaderResourceView> DiffuseTextureView;
	};

	struct FSection
	{
		uint32 IndexStart = 0;
		uint32 IndexCount = 0;
		int32 MaterialIndex = -1;
	};

	struct FLODLevel
	{
		TArray<FStaticMeshVertex> Vertices;
		TArray<uint32> Indices;
		TComPtr<ID3D11Buffer> VertexBuffer;
		TComPtr<ID3D11Buffer> IndexBuffer;
		TArray<FSection> Sections;
		FVector BoundsMin = FVector::ZeroVector;
		FVector BoundsMax = FVector::ZeroVector;
		FBoundingSphere BoundsSphere;
		float TriangleRatio = 1.0f;

		bool IsValid() const
		{
			return VertexBuffer != nullptr && IndexBuffer != nullptr && !Vertices.empty() && !Indices.empty() && !Sections.empty();
		}
	};

public:
	FStaticMesh() = default;
	~FStaticMesh() = default;

	bool Initialize(ID3D11Device* InDevice, ID3D11DeviceContext* InDeviceContext, FStaticMeshSourceData InSourceData);
	void Release();

	ID3D11Buffer* GetVertexBuffer() const;
	ID3D11Buffer* GetIndexBuffer() const;
	ID3D11Buffer* GetVertexBuffer(uint32 InLODIndex) const;
	ID3D11Buffer* GetIndexBuffer(uint32 InLODIndex) const;

	UINT GetVertexCount() const;
	UINT GetIndexCount() const;
	bool IsValid() const;

	const std::filesystem::path& GetSourcePath() const { return SourcePath; }
	const TArray<FStaticMeshVertex>& GetVertices() const;
	const TArray<uint32>& GetIndices() const;
	const TArray<FMaterial>& GetMaterials() const { return Materials; }
	const TArray<FSection>& GetSections() const;
	const TArray<FSection>& GetSections(uint32 InLODIndex) const;
	const FLODLevel* GetLODLevel(uint32 InLODIndex) const;
	uint32 GetLODCount() const { return static_cast<uint32>(LODLevels.size()); }

	const FVector& GetBoundsMin() const;
	const FVector& GetBoundsMax() const;
	const FBoundingSphere& GetBoundsSphere() const;

	const std::shared_ptr<FBVHSpatialData>& GetSpatialData() const { return SpatialData; }
	void SetSpatialData(std::shared_ptr<FBVHSpatialData> InSpatialData) { SpatialData = std::move(InSpatialData); }

	ID3D11ShaderResourceView* GetMaterialTexture(int32 InMaterialIndex) const;

private:
	const FLODLevel* GetBaseLOD() const;

	std::filesystem::path SourcePath;

	TComPtr<ID3D11ShaderResourceView> DiffuseTextureView;
	std::shared_ptr<FBVHSpatialData> SpatialData;
	TArray<FMaterial> Materials;
	TArray<FLODLevel> LODLevels;
};
