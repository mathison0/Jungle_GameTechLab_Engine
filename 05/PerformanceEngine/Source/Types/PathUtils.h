#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include <Windows.h>

#include "Types/String.h"

namespace PathUtils
{
	namespace Detail
	{
		inline bool DecodeCodePageToWide(UINT InCodePage, DWORD InFlags, std::string_view InBytes, std::wstring& OutWideText)
		{
			if (InBytes.empty())
			{
				OutWideText.clear();
				return true;
			}

			if (InBytes.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
			{
				return false;
			}

			const int RequiredCharacterCount = MultiByteToWideChar(
				InCodePage,
				InFlags,
				InBytes.data(),
				static_cast<int>(InBytes.size()),
				nullptr,
				0);
			if (RequiredCharacterCount <= 0)
			{
				return false;
			}

			OutWideText.resize(static_cast<size_t>(RequiredCharacterCount));
			return MultiByteToWideChar(
				InCodePage,
				InFlags,
				InBytes.data(),
				static_cast<int>(InBytes.size()),
				OutWideText.data(),
				RequiredCharacterCount) == RequiredCharacterCount;
		}

		inline bool EncodeWideToUtf8(std::wstring_view InWideText, std::string& OutUtf8Text)
		{
			if (InWideText.empty())
			{
				OutUtf8Text.clear();
				return true;
			}

			if (InWideText.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
			{
				return false;
			}

			const int RequiredByteCount = WideCharToMultiByte(
				CP_UTF8,
				0,
				InWideText.data(),
				static_cast<int>(InWideText.size()),
				nullptr,
				0,
				nullptr,
				nullptr);
			if (RequiredByteCount <= 0)
			{
				return false;
			}

			OutUtf8Text.resize(static_cast<size_t>(RequiredByteCount));
			return WideCharToMultiByte(
				CP_UTF8,
				0,
				InWideText.data(),
				static_cast<int>(InWideText.size()),
				OutUtf8Text.data(),
				RequiredByteCount,
				nullptr,
				nullptr) == RequiredByteCount;
		}

		inline bool ReadBinaryFile(const std::filesystem::path& InFilePath, std::vector<char>& OutBytes)
		{
			std::ifstream File(InFilePath, std::ios::binary | std::ios::ate);
			if (!File.is_open())
			{
				return false;
			}

			const std::streamoff FileSize = File.tellg();
			if (FileSize < 0)
			{
				return false;
			}

			OutBytes.resize(static_cast<size_t>(FileSize));
			File.seekg(0, std::ios::beg);
			if (!OutBytes.empty())
			{
				File.read(OutBytes.data(), static_cast<std::streamsize>(OutBytes.size()));
			}

			return File.good() || File.eof();
		}

		inline bool ConvertUtf16BytesToUtf8(const char* InBytes, size_t InByteCount, bool bBigEndian, std::string& OutUtf8Text)
		{
			if ((InByteCount % 2u) != 0u)
			{
				return false;
			}

			const size_t CodeUnitCount = InByteCount / 2u;
			std::wstring WideText;
			WideText.resize(CodeUnitCount);
			for (size_t Index = 0; Index < CodeUnitCount; ++Index)
			{
				const uint8_t Byte0 = static_cast<uint8_t>(InBytes[Index * 2u]);
				const uint8_t Byte1 = static_cast<uint8_t>(InBytes[Index * 2u + 1u]);
				const uint16_t CodeUnit = bBigEndian
					? (static_cast<uint16_t>(Byte0) << 8u) | static_cast<uint16_t>(Byte1)
					: (static_cast<uint16_t>(Byte1) << 8u) | static_cast<uint16_t>(Byte0);
				WideText[Index] = static_cast<wchar_t>(CodeUnit);
			}

			return EncodeWideToUtf8(WideText, OutUtf8Text);
		}
	}

	inline bool ReadTextFileUtf8(const std::filesystem::path& InFilePath, std::string& OutUtf8Text)
	{
		std::vector<char> FileBytes;
		if (!Detail::ReadBinaryFile(InFilePath, FileBytes))
		{
			return false;
		}

		if (FileBytes.size() >= 3u
			&& static_cast<uint8_t>(FileBytes[0]) == 0xEFu
			&& static_cast<uint8_t>(FileBytes[1]) == 0xBBu
			&& static_cast<uint8_t>(FileBytes[2]) == 0xBFu)
		{
			OutUtf8Text.assign(FileBytes.begin() + 3, FileBytes.end());
			return true;
		}

		if (FileBytes.size() >= 2u)
		{
			const uint8_t Byte0 = static_cast<uint8_t>(FileBytes[0]);
			const uint8_t Byte1 = static_cast<uint8_t>(FileBytes[1]);
			if (Byte0 == 0xFFu && Byte1 == 0xFEu)
			{
				return Detail::ConvertUtf16BytesToUtf8(FileBytes.data() + 2, FileBytes.size() - 2u, false, OutUtf8Text);
			}

			if (Byte0 == 0xFEu && Byte1 == 0xFFu)
			{
				return Detail::ConvertUtf16BytesToUtf8(FileBytes.data() + 2, FileBytes.size() - 2u, true, OutUtf8Text);
			}
		}

		const std::string_view ByteView(FileBytes.data(), FileBytes.size());
		std::wstring WideText;
		if (Detail::DecodeCodePageToWide(CP_UTF8, MB_ERR_INVALID_CHARS, ByteView, WideText))
		{
			OutUtf8Text.assign(ByteView.begin(), ByteView.end());
			return true;
		}

		if (!Detail::DecodeCodePageToWide(CP_ACP, 0, ByteView, WideText))
		{
			return false;
		}

		return Detail::EncodeWideToUtf8(WideText, OutUtf8Text);
	}

	inline std::filesystem::path PathFromUtf8(std::string_view InUtf8Text)
	{
		if (InUtf8Text.empty())
		{
			return {};
		}

		std::wstring WidePath;
		if (Detail::DecodeCodePageToWide(CP_UTF8, MB_ERR_INVALID_CHARS, InUtf8Text, WidePath)
			|| Detail::DecodeCodePageToWide(CP_ACP, 0, InUtf8Text, WidePath))
		{
			return std::filesystem::path(WidePath);
		}

		return std::filesystem::path(std::string(InUtf8Text));
	}

	inline std::string Utf8FromWide(std::wstring_view InWideText)
	{
		std::string Utf8Text;
		Detail::EncodeWideToUtf8(InWideText, Utf8Text);
		return Utf8Text;
	}

	inline std::string PathToUtf8(const std::filesystem::path& InPath)
	{
		if (InPath.empty())
		{
			return {};
		}

#ifdef _WIN32
		return Utf8FromWide(InPath.native());
#else
		return InPath.string();
#endif
	}

	inline FString NormalizeAbsolutePathUtf8(const std::filesystem::path& InPath)
	{
		if (InPath.empty())
		{
			return {};
		}

		return PathToUtf8(std::filesystem::absolute(InPath).lexically_normal());
	}

	inline std::filesystem::path AppendUtf8Suffix(const std::filesystem::path& InPath, std::string_view InUtf8Suffix)
	{
		if (InPath.empty())
		{
			return {};
		}

#ifdef _WIN32
		std::filesystem::path Result = InPath.parent_path();
		Result /= std::filesystem::path(InPath.filename().native() + PathFromUtf8(InUtf8Suffix).native());
		return Result;
#else
		std::filesystem::path Result = InPath.parent_path();
		Result /= std::filesystem::path(InPath.filename().string() + std::string(InUtf8Suffix));
		return Result;
#endif
	}
}
