#include "Texture.h"

#include "Core/Engine.h"
#include "Core/Paths.h"
#include "Object/Class.h"
#include "Object/ObjectFactory.h"
#include "Object/ObjectIterator.h"
#include "Renderer/Renderer.h"

#include <algorithm>
#include <array>
#include <cwctype>

IMPLEMENT_RTTI(UTexture, UObject)

void UTexture::SetAssetPathFileName(const FString& InAssetPathFileName)
{
	const FString NormalizedPath = NormalizeTextureAssetPath(InAssetPathFileName);
	if (AssetPathFileName == NormalizedPath)
	{
		return;
	}

	AssetPathFileName = NormalizedPath;
	TextureResource.reset();
}

bool UTexture::EnsureTextureResource()
{
	if (TextureResource && TextureResource->TextureSRV)
	{
		return true;
	}

	if (AssetPathFileName.empty())
	{
		return false;
	}

	FRenderer* Renderer = GEngine ? GEngine->GetRenderer() : nullptr;
	if (!Renderer)
	{
		return false;
	}

	const std::filesystem::path TexturePath = ResolveTexturePath(AssetPathFileName);
	if (TexturePath.empty() || !std::filesystem::exists(TexturePath))
	{
		return false;
	}

	ID3D11ShaderResourceView* TextureSRV = nullptr;
	if (!Renderer->CreateTextureFromSTB(Renderer->GetDevice(), TexturePath, &TextureSRV))
	{
		return false;
	}

	TextureResource = std::make_shared<FMaterialTexture>();
	TextureResource->TextureSRV = TextureSRV;
	return true;
}

UTexture* UTexture::FindByAssetPath(const FString& InAssetPathFileName)
{
	const FString NormalizedPath = NormalizeTextureAssetPath(InAssetPathFileName);
	if (NormalizedPath.empty())
	{
		return nullptr;
	}

	for (TObjectIterator<UTexture> It; It; ++It)
	{
		UTexture* Texture = It.Get();
		if (Texture && Texture->GetAssetPathFileName() == NormalizedPath)
		{
			return Texture;
		}
	}

	return nullptr;
}

UTexture* UTexture::FindOrLoad(const FString& InAssetPathFileName, UObject* InOuter)
{
	const FString NormalizedPath = NormalizeTextureAssetPath(InAssetPathFileName);
	if (NormalizedPath.empty())
	{
		return nullptr;
	}

	if (UTexture* ExistingTexture = FindByAssetPath(NormalizedPath))
	{
		return ExistingTexture;
	}

	const std::filesystem::path TexturePath = ResolveTexturePath(NormalizedPath);
	if (TexturePath.empty() || !std::filesystem::exists(TexturePath))
	{
		return nullptr;
	}

	const FString TextureName = FPaths::FromPath(TexturePath.filename());
	UTexture* NewTexture = FObjectFactory::ConstructObject<UTexture>(InOuter, TextureName);
	if (!NewTexture)
	{
		return nullptr;
	}

	NewTexture->SetAssetPathFileName(NormalizedPath);
	return NewTexture;
}

TArray<FString> UTexture::GetAvailableTextureAssetPaths()
{
	TArray<FString> AvailableTextureAssetPaths;
	/*GatherTextureAssetPathsFromDirectory(FPaths::TextureDir(), FPaths::AssetDir(), AvailableTextureAssetPaths);
	GatherTextureAssetPathsFromDirectory(FPaths::ContentDir() / "Textures", FPaths::ProjectRoot(), AvailableTextureAssetPaths);*/
	GatherTextureAssetPathsFromDirectory(FPaths::AssetDir() / "Editor", FPaths::ProjectRoot(), AvailableTextureAssetPaths);

	std::sort(AvailableTextureAssetPaths.begin(), AvailableTextureAssetPaths.end());
	AvailableTextureAssetPaths.erase(
		std::unique(AvailableTextureAssetPaths.begin(), AvailableTextureAssetPaths.end()),
		AvailableTextureAssetPaths.end()
	);

	return AvailableTextureAssetPaths;
}

FString UTexture::NormalizeTextureAssetPath(const FString& InAssetPathFileName)
{
	if (InAssetPathFileName.empty())
	{
		return "";
	}

	const std::filesystem::path InputPath = FPaths::ToPath(InAssetPathFileName).lexically_normal();
	if (!InputPath.is_absolute())
	{
		return FPaths::FromPath(InputPath);
	}

	const std::filesystem::path AssetRelative = InputPath.lexically_relative(FPaths::AssetDir());
	if (!AssetRelative.empty() && AssetRelative.native()[0] != L'.')
	{
		return FPaths::FromPath(AssetRelative);
	}

	const std::filesystem::path ProjectRelative = InputPath.lexically_relative(FPaths::ProjectRoot());
	if (!ProjectRelative.empty() && ProjectRelative.native()[0] != L'.')
	{
		return FPaths::FromPath(ProjectRelative);
	}

	return FPaths::FromPath(InputPath);
}

std::filesystem::path UTexture::ResolveTexturePath(const FString& InAssetPathFileName)
{
	if (InAssetPathFileName.empty())
	{
		return {};
	}

	const std::filesystem::path InPath = FPaths::ToPath(InAssetPathFileName).lexically_normal();
	if (InPath.is_absolute())
	{
		return InPath;
	}

	const std::filesystem::path AssetCandidate = FPaths::AssetDir() / InPath;
	if (std::filesystem::exists(AssetCandidate))
	{
		return AssetCandidate.lexically_normal();
	}

	const std::filesystem::path ContentCandidate = FPaths::ContentDir() / InPath;
	if (std::filesystem::exists(ContentCandidate))
	{
		return ContentCandidate.lexically_normal();
	}

	const std::filesystem::path ProjectCandidate = FPaths::ProjectRoot() / InPath;
	if (std::filesystem::exists(ProjectCandidate))
	{
		return ProjectCandidate.lexically_normal();
	}

	return {};
}

bool UTexture::IsSupportedTextureFile(const std::filesystem::path& InPath)
{
	static constexpr std::array<const wchar_t*, 6> SupportedExtensions =
	{
		L".png",
		L".jpg",
		L".jpeg",
		L".bmp",
		L".tga",
		L".dds"
	};

	std::wstring Extension = InPath.extension().wstring();
	std::transform(Extension.begin(), Extension.end(), Extension.begin(), towlower);

	for (const wchar_t* SupportedExtension : SupportedExtensions)
	{
		if (Extension == SupportedExtension)
		{
			return true;
		}
	}

	return false;
}

void UTexture::GatherTextureAssetPathsFromDirectory(
	const std::filesystem::path& InDirectory,
	const std::filesystem::path& InRelativeBase,
	TArray<FString>& OutAssetPaths)
{
	if (!std::filesystem::exists(InDirectory) || !std::filesystem::is_directory(InDirectory))
	{
		return;
	}

	for (const std::filesystem::directory_entry& Entry : std::filesystem::recursive_directory_iterator(InDirectory))
	{
		if (!Entry.is_regular_file() || !IsSupportedTextureFile(Entry.path()))
		{
			continue;
		}

		const std::filesystem::path RelativePath = Entry.path().lexically_relative(InRelativeBase);
		if (RelativePath.empty())
		{
			continue;
		}

		OutAssetPaths.push_back(FPaths::FromPath(RelativePath));
	}
}
