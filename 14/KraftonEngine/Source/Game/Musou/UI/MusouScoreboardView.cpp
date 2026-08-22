#include "Game/Musou/UI/MusouScoreboardView.h"

#include <algorithm>
#include <cstdio>
#include <string>

namespace
{
	FString EscapeRmlText(const FString& Text)
	{
		FString Escaped;
		Escaped.reserve(Text.size());

		for (char Ch : Text)
		{
			switch (Ch)
			{
			case '&':
				Escaped += "&amp;";
				break;
			case '<':
				Escaped += "&lt;";
				break;
			case '>':
				Escaped += "&gt;";
				break;
			case '"':
				Escaped += "&quot;";
				break;
			default:
				Escaped += Ch;
				break;
			}
		}

		return Escaped;
	}

	FString MakeElementRml(const FString& ClassName, const FString& Body)
	{
		FString Result;
		Result.reserve(ClassName.size() + Body.size() + 24);
		Result += "<div class=\"";
		Result += ClassName;
		Result += "\">";
		Result += Body;
		Result += "</div>";
		return Result;
	}

	int32 GetScoreboardDisplayRank(int32 EntryIndex)
	{
		return EntryIndex + 1;
	}
}

int32 FMusouScoreboardView::GetPageCount(size_t EntryCount, int32 PageSize)
{
	const int32 SafePageSize = std::max(PageSize, 1);
	if (EntryCount == 0)
	{
		return 1;
	}

	return static_cast<int32>((EntryCount + static_cast<size_t>(SafePageSize) - 1) / static_cast<size_t>(SafePageSize));
}

int32 FMusouScoreboardView::ClampPageIndex(int32 PageIndex, size_t EntryCount, int32 PageSize)
{
	const int32 PageCount = GetPageCount(EntryCount, PageSize);
	return std::clamp(PageIndex, 0, PageCount - 1);
}

FString FMusouScoreboardView::MakeRowsRml(const TArray<FMusouScoreboardEntry>& Entries, int32 PageIndex, int32 PageSize, const FMusouScoreboardViewStyle& Style)
{
	const int32 SafePageSize = std::max(PageSize, 1);
	if (Entries.empty())
	{
		return MakeElementRml(Style.EmptyClass, "저장된 기록이 없습니다.");
	}

	const int32 FirstEntryIndex = ClampPageIndex(PageIndex, Entries.size(), SafePageSize) * SafePageSize;
	const int32 LastEntryIndex = std::min(FirstEntryIndex + SafePageSize, static_cast<int32>(Entries.size()));

	FString Rows;
	for (int32 EntryIndex = FirstEntryIndex; EntryIndex < LastEntryIndex; ++EntryIndex)
	{
		const FMusouScoreboardEntry& Entry = Entries[static_cast<size_t>(EntryIndex)];
		const int32 DisplayRank = GetScoreboardDisplayRank(EntryIndex);

		char DetailsBuffer[64] = {};
		std::snprintf(DetailsBuffer, sizeof(DetailsBuffer),
			"K.O. %d / Combo %d / %.1fs",
			Entry.KillCount,
			Entry.MaxCombo,
			Entry.MatchTime);

		Rows += "<div class=\"";
		Rows += Style.RowClass;
		Rows += "\">";
		Rows += MakeElementRml(Style.RankClass, std::to_string(DisplayRank));
		Rows += MakeElementRml(Style.NameClass, EscapeRmlText(Entry.PlayerName));
		Rows += MakeElementRml(Style.ScoreClass, std::to_string(static_cast<long long>(Entry.Score)));
		Rows += MakeElementRml(Style.DetailsClass, DetailsBuffer);
		Rows += "</div>";
	}

	return Rows;
}
