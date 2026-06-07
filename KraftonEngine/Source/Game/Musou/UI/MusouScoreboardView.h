#pragma once

#include "Core/Types/CoreTypes.h"
#include "Game/Musou/Score/MusouScoreboard.h"

#include <cstddef>

// 스코어보드 저장 데이터는 FMusouScoreboard가 담당하고,
// 이 파일은 화면별 CSS class를 받아 공통 RML 행/페이지 정보를 만드는 데만 집중한다.
struct FMusouScoreboardViewStyle
{
	FString RowClass;
	FString RankClass;
	FString NameClass;
	FString ScoreClass;
	FString DetailsClass;
	FString EmptyClass;
};

class FMusouScoreboardView
{
public:
	static int32 GetPageCount(size_t EntryCount, int32 PageSize);
	static int32 ClampPageIndex(int32 PageIndex, size_t EntryCount, int32 PageSize);
	static FString MakeRowsRml(const TArray<FMusouScoreboardEntry>& Entries, int32 PageIndex, int32 PageSize, const FMusouScoreboardViewStyle& Style);
};
