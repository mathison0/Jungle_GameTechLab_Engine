#pragma once

#include "CoreMinimal.h"
#include "Object/Object.h"
#include "Renderer/Material.h"

#include <filesystem>
#include <memory>

class ENGINE_API UTexture : public UObject
{
public:
	DECLARE_RTTI(UTexture, UObject)

	void SetAssetPathFileName(const FString& InAssetPathFileName);
	const FString& GetAssetPathFileName() const { return AssetPathFileName; }

	std::shared_ptr<FMaterialTexture> GetTextureResource() const { return TextureResource; }
	bool EnsureTextureResource();

	static UTexture* FindByAssetPath(const FString& InAssetPathFileName);
	static UTexture* FindOrLoad(const FString& InAssetPathFileName, UObject* InOuter = nullptr);
	static TArray<FString> GetAvailableTextureAssetPaths();

private:
	static FString NormalizeTextureAssetPath(const FString& InAssetPathFileName);
	static std::filesystem::path ResolveTexturePath(const FString& InAssetPathFileName);
	static bool IsSupportedTextureFile(const std::filesystem::path& InPath);
	static void GatherTextureAssetPathsFromDirectory(
		const std::filesystem::path& InDirectory,
		const std::filesystem::path& InRelativeBase,
		TArray<FString>& OutAssetPaths
	);

private:
	FString AssetPathFileName;
	std::shared_ptr<FMaterialTexture> TextureResource;
};
