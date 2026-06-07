#include "Game/Musou/Score/MusouScoreboard.h"

#include "Core/Logging/Log.h"
#include "Platform/Paths.h"
#include "SimpleJSON/json.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <string>

namespace
{
	constexpr const char* ScoreboardFileName = "MusouScoreboard.json";

	namespace ScoreboardKeys
	{
		constexpr const char* Version = "Version";
		constexpr const char* Entries = "Entries";
		constexpr const char* PlayerName = "PlayerName";
		constexpr const char* Score = "Score";
		constexpr const char* KillCount = "KillCount";
		constexpr const char* MaxCombo = "MaxCombo";
		constexpr const char* MatchTime = "MatchTime";
		constexpr const char* PlayerHealthRatio = "PlayerHealthRatio";
		constexpr const char* SavedAt = "SavedAt";
		constexpr const char* SavedAtEpochMs = "SavedAtEpochMs";
	}

	std::filesystem::path GetScoreboardPath()
	{
		return std::filesystem::path(FPaths::SaveDir()) / FPaths::ToWide(ScoreboardFileName);
	}

	FString TrimPlayerName(const FString& Name)
	{
		size_t First = 0;
		while (First < Name.size() && std::isspace(static_cast<unsigned char>(Name[First])))
		{
			++First;
		}

		size_t Last = Name.size();
		while (Last > First && std::isspace(static_cast<unsigned char>(Name[Last - 1])))
		{
			--Last;
		}

		FString Trimmed = Name.substr(First, Last - First);
		if (Trimmed.empty())
		{
			return "Player";
		}

		return Trimmed;
	}

	int64 MakeSavedAtEpochMs()
	{
		using namespace std::chrono;
		return static_cast<int64>(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
	}

	FString MakeSavedAtText(int64 SavedAtEpochMs)
	{
		std::time_t Time = static_cast<std::time_t>(SavedAtEpochMs / 1000);
		std::tm LocalTime{};
		localtime_s(&LocalTime, &Time);

		char Buffer[24] = {};
		std::strftime(Buffer, sizeof(Buffer), "%Y-%m-%d %H:%M:%S", &LocalTime);
		return FString(Buffer);
	}

	int64 ParseSavedAtEpochMs(const FString& SavedAt)
	{
		if (SavedAt.size() < 19)
		{
			return 0;
		}

		std::tm LocalTime{};
		LocalTime.tm_isdst = -1;
		std::istringstream Stream(SavedAt.substr(0, 19));
		Stream >> std::get_time(&LocalTime, "%Y-%m-%d %H:%M:%S");
		if (Stream.fail())
		{
			return 0;
		}

		int64 Milliseconds = 0;
		if (SavedAt.size() > 20 && SavedAt[19] == '.')
		{
			try
			{
				Milliseconds = static_cast<int64>(std::stoll(SavedAt.substr(20, 3)));
			}
			catch (...)
			{
				Milliseconds = 0;
			}
		}

		return static_cast<int64>(std::mktime(&LocalTime)) * 1000 + Milliseconds;
	}

	int32 ReadInt32(json::JSON& Node, const char* Key, int32 DefaultValue = 0)
	{
		if (!Node.hasKey(Key))
		{
			return DefaultValue;
		}

		return static_cast<int32>(Node[Key].ToInt());
	}

	float ReadFloat(json::JSON& Node, const char* Key, float DefaultValue = 0.0f)
	{
		if (!Node.hasKey(Key))
		{
			return DefaultValue;
		}

		json::JSON& Value = Node[Key];
		if (Value.JSONType() == json::JSON::Class::Integral)
		{
			return static_cast<float>(Value.ToInt());
		}

		return static_cast<float>(Value.ToFloat());
	}

	int64 ReadScore(json::JSON& Node)
	{
		if (!Node.hasKey(ScoreboardKeys::Score))
		{
			return 0;
		}

		json::JSON& ScoreNode = Node[ScoreboardKeys::Score];
		if (ScoreNode.JSONType() == json::JSON::Class::String)
		{
			try
			{
				return static_cast<int64>(std::stoll(ScoreNode.ToString()));
			}
			catch (...)
			{
				return 0;
			}
		}

		return static_cast<int64>(ScoreNode.ToInt());
	}

	int64 ReadInt64(json::JSON& Node, const char* Key, int64 DefaultValue = 0)
	{
		if (!Node.hasKey(Key))
		{
			return DefaultValue;
		}

		json::JSON& Value = Node[Key];
		if (Value.JSONType() == json::JSON::Class::String)
		{
			try
			{
				return static_cast<int64>(std::stoll(Value.ToString()));
			}
			catch (...)
			{
				return DefaultValue;
			}
		}

		return static_cast<int64>(Value.ToInt());
	}

	FMusouScoreboardEntry ReadEntry(json::JSON& Node)
	{
		FMusouScoreboardEntry Entry;
		if (Node.hasKey(ScoreboardKeys::PlayerName))
		{
			Entry.PlayerName = Node[ScoreboardKeys::PlayerName].ToString();
		}
		if (Node.hasKey(ScoreboardKeys::SavedAt))
		{
			Entry.SavedAt = Node[ScoreboardKeys::SavedAt].ToString();
		}

		Entry.Score = ReadScore(Node);
		Entry.KillCount = ReadInt32(Node, ScoreboardKeys::KillCount);
		Entry.MaxCombo = ReadInt32(Node, ScoreboardKeys::MaxCombo);
		Entry.MatchTime = ReadFloat(Node, ScoreboardKeys::MatchTime);
		Entry.PlayerHealthRatio = std::clamp(ReadFloat(Node, ScoreboardKeys::PlayerHealthRatio, 1.0f), 0.0f, 1.0f);
		Entry.SavedAtEpochMs = ReadInt64(Node, ScoreboardKeys::SavedAtEpochMs, ParseSavedAtEpochMs(Entry.SavedAt));

		if (Entry.PlayerName.empty())
		{
			Entry.PlayerName = "Player";
		}

		return Entry;
	}

	json::JSON WriteEntry(const FMusouScoreboardEntry& Entry)
	{
		json::JSON Node = json::Object();
		Node[ScoreboardKeys::PlayerName] = Entry.PlayerName;

		// SimpleJSON의 Integral 저장소가 long 기반이라 Windows에서 64bit 점수가 줄어들 수 있다.
		// 파일에는 문자열로 저장하고 로드 시 int64로 복원한다.
		Node[ScoreboardKeys::Score] = std::to_string(static_cast<long long>(Entry.Score));
		Node[ScoreboardKeys::KillCount] = Entry.KillCount;
		Node[ScoreboardKeys::MaxCombo] = Entry.MaxCombo;
		Node[ScoreboardKeys::MatchTime] = static_cast<double>(Entry.MatchTime);
		Node[ScoreboardKeys::PlayerHealthRatio] = static_cast<double>(Entry.PlayerHealthRatio);
		Node[ScoreboardKeys::SavedAt] = Entry.SavedAt;
		Node[ScoreboardKeys::SavedAtEpochMs] = std::to_string(static_cast<long long>(Entry.SavedAtEpochMs));
		return Node;
	}

	void SortAndTrimEntries(TArray<FMusouScoreboardEntry>& Entries)
	{
		std::sort(Entries.begin(), Entries.end(), [](const FMusouScoreboardEntry& Left, const FMusouScoreboardEntry& Right)
		{
			if (Left.Score != Right.Score)
			{
				return Left.Score > Right.Score;
			}
			if (Left.SavedAtEpochMs != Right.SavedAtEpochMs)
			{
				return Left.SavedAtEpochMs > Right.SavedAtEpochMs;
			}
			if (Left.SavedAt != Right.SavedAt)
			{
				return Left.SavedAt > Right.SavedAt;
			}
			return Left.PlayerName < Right.PlayerName;
		});

		if (Entries.size() > static_cast<size_t>(FMusouScoreboard::MaxEntryCount))
		{
			Entries.resize(static_cast<size_t>(FMusouScoreboard::MaxEntryCount));
		}
	}

	bool SaveEntries(const TArray<FMusouScoreboardEntry>& Entries)
	{
		const std::filesystem::path FilePath = GetScoreboardPath();
		std::error_code ErrorCode;
		std::filesystem::create_directories(FilePath.parent_path(), ErrorCode);
		if (ErrorCode)
		{
			UE_LOG("[MusouScoreboard] Failed to create directory: %s", ErrorCode.message().c_str());
			return false;
		}

		json::JSON Root = json::Object();
		Root[ScoreboardKeys::Version] = 1;

		json::JSON EntryArray = json::Array();
		for (const FMusouScoreboardEntry& Entry : Entries)
		{
			EntryArray.append(WriteEntry(Entry));
		}
		Root[ScoreboardKeys::Entries] = EntryArray;

		std::ofstream File(FilePath, std::ios::trunc);
		if (!File.is_open())
		{
			UE_LOG("[MusouScoreboard] Failed to open save file: %s", FilePath.string().c_str());
			return false;
		}

		File << Root.dump();
		return File.good();
	}
}

TArray<FMusouScoreboardEntry> FMusouScoreboard::LoadEntries()
{
	TArray<FMusouScoreboardEntry> Entries;
	const std::filesystem::path FilePath = GetScoreboardPath();
	if (!std::filesystem::exists(FilePath))
	{
		return Entries;
	}

	std::ifstream File(FilePath);
	if (!File.is_open())
	{
		UE_LOG("[MusouScoreboard] Failed to open save file: %s", FilePath.string().c_str());
		return Entries;
	}

	FString FileContent((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
	if (FileContent.empty())
	{
		return Entries;
	}

	json::JSON Root = json::JSON::Load(FileContent);
	if (!Root.hasKey(ScoreboardKeys::Entries))
	{
		return Entries;
	}

	for (json::JSON& EntryNode : Root[ScoreboardKeys::Entries].ArrayRange())
	{
		Entries.push_back(ReadEntry(EntryNode));
	}

	SortAndTrimEntries(Entries);
	return Entries;
}

bool FMusouScoreboard::Submit(const FMusouMatchResult& Result, const FString& PlayerName, TArray<FMusouScoreboardEntry>* OutEntries)
{
	TArray<FMusouScoreboardEntry> Entries = LoadEntries();

	FMusouScoreboardEntry NewEntry;
	NewEntry.PlayerName = TrimPlayerName(PlayerName);
	NewEntry.Score = Result.Score;
	NewEntry.KillCount = Result.KillCount;
	NewEntry.MaxCombo = Result.MaxCombo;
	NewEntry.MatchTime = Result.MatchTime;
	NewEntry.PlayerHealthRatio = std::clamp(Result.PlayerHealthRatio, 0.0f, 1.0f);
	NewEntry.SavedAtEpochMs = MakeSavedAtEpochMs();
	NewEntry.SavedAt = MakeSavedAtText(NewEntry.SavedAtEpochMs);

	Entries.push_back(NewEntry);
	SortAndTrimEntries(Entries);

	if (!SaveEntries(Entries))
	{
		return false;
	}

	if (OutEntries)
	{
		*OutEntries = Entries;
	}

	return true;
}
