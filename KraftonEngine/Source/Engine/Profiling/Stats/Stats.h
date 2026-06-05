#pragma once

#include "Core/Types/CoreTypes.h"
#include "Core/Singleton.h"
#include "Render/Types/RenderTypes.h"

#define NOMINMAX
#include <Windows.h>
#include <cfloat>

// --- 빌드 설정 ---
#ifndef STATS
#if defined(_DEBUG) || defined(DEBUG)
#define STATS 1
#else
#define STATS 1
#endif
#endif

// 슬라이딩 윈도우 크기
static constexpr uint32 STAT_WINDOW_SIZE = 60;

// --- Stat Entry (Snapshot용 — 읽기 전용) ---
struct FStatEntry
{
	const char* Name = nullptr;
	const char* Category = "Default";
	uint32 CallCount = 0;		// 현재 프레임 호출 횟수
	double TotalTime = 0.0;	// 현재 프레임 합산 시간
	double AvgTime = 0.0;		// 최근 N프레임 평균
	double MaxTime = 0.0;		// 최근 N프레임 최대
	double MinTime = 0.0;		// 최근 N프레임 최소
	double LastTime = 0.0;		// 직전 프레임 시간
};

// --- 내부 누적용 Entry ---
struct FStatAccum
{
	const char* Name = nullptr;
	const char* Category = "Default";

	// 현재 프레임 누적
	uint32 FrameCallCount = 0;
	double FrameTotal = 0.0;

	// 슬라이딩 윈도우 (ring buffer)
	double Window[STAT_WINDOW_SIZE] = {};
	uint32 WindowHead = 0;		// 다음 쓸 위치
	uint32 WindowCount = 0;	// 채워진 수 (최대 STAT_WINDOW_SIZE)
};

// --- Stat Key (Name + Category 복합 키) ---
struct FStatKey
{
	const char* Name     = nullptr;
	const char* Category = nullptr;

	bool operator==(const FStatKey& Other) const { return Name == Other.Name && Category == Other.Category; }
};

struct FStatKeyHash
{
	size_t operator()(const FStatKey& Key) const
	{
		size_t H1 = std::hash<const void*>{}(Key.Name);
		size_t H2 = std::hash<const void*>{}(Key.Category);
		return H1 ^ (H2 * 2654435761u);
	}
};

// --- Stat Manager (싱글턴) ---
class FStatManager : public TSingleton<FStatManager>
{
	friend class TSingleton<FStatManager>;

public:
	void RecordTime(const char* Name, double ElapsedSeconds, const char* Category = "Default");
	void TakeSnapshot();
	const TArray<FStatEntry>& GetSnapshot() const { return Snapshot; }
	LARGE_INTEGER GetFrequency() const { return Frequency; }

private:
	FStatManager();
	~FStatManager() = default;

	std::unordered_map<FStatKey, FStatAccum, FStatKeyHash> Stats;
	TArray<FStatEntry> Snapshot;
	LARGE_INTEGER Frequency;
};

// --- Scoped Timer (RAII) ---
class FScopedTimer
{
public:
	FScopedTimer(const char* InName, const char* InCategory = "Default")
		: Name(InName), Category(InCategory)
	{
		QueryPerformanceCounter(&StartTime);
	}

	~FScopedTimer()
	{
		LARGE_INTEGER EndTime;
		QueryPerformanceCounter(&EndTime);
		double Elapsed = static_cast<double>(EndTime.QuadPart - StartTime.QuadPart)
			/ static_cast<double>(FStatManager::Get().GetFrequency().QuadPart);
		FStatManager::Get().RecordTime(Name, Elapsed, Category);
	}

private:
	const char* Name;
	const char* Category;
	LARGE_INTEGER StartTime;
};

// --- Draw Call Counter ---
struct FDrawCallStats
{
	static uint32 Count;
	static void Reset() { Count = 0; }
	static void Increment() { ++Count; }
	static uint32 Get() { return Count; }
};

struct FSkeletalRenderStats
{
	static uint32 SkeletalDrawCalls;
	static uint32 SkeletalGpuSkinDrawCalls;
	static uint32 SkeletalCpuSkinDrawCalls;

	static uint32 SkeletalInstancedDrawCalls;
	static uint32 SkeletalSubmittedInstances;

	static uint32 SkeletalGpuSkinCommands;
	static uint32 SkeletalBatchableCommands;
	static uint32 SkeletalBatchRejectedCommands;
	static uint32 SkeletalBatchUniqueKeys;
	static uint32 SkeletalEstimatedInstancedDrawCalls;

	static uint32 SkeletalInstanceCandidates;
	static uint32 SkeletalInstanceRejectedCommands;
	static uint32 SkeletalInstanceSingleCommands;
	static uint32 SkeletalInstanceMergedDrawCalls;
	static uint32 SkeletalInstanceMergedInstances;
	static uint32 SkeletalInstanceOutputCommands;

	static uint32 GlobalSkinMatrixCharacters;
	static uint32 GlobalSkinMatrixCount;
	static uint32 GlobalSkinMatrixUploadBytes;
	static uint32 GlobalSkinMatrixBuildFailures;
	static uint32 GlobalSkinMatrixCommandReuses;
	static uint32 GlobalSkinMatrixPoseCacheHits;

	static uint32 PassDrawCalls[(uint32)ERenderPass::MAX];

	static void Reset()
	{
		SkeletalDrawCalls = 0;
		SkeletalGpuSkinDrawCalls = 0;
		SkeletalCpuSkinDrawCalls = 0;
		SkeletalInstancedDrawCalls = 0;
		SkeletalSubmittedInstances = 0;

		SkeletalGpuSkinCommands = 0;
		SkeletalBatchableCommands = 0;
		SkeletalBatchRejectedCommands = 0;
		SkeletalBatchUniqueKeys = 0;
		SkeletalEstimatedInstancedDrawCalls = 0;

		SkeletalInstanceCandidates = 0;
		SkeletalInstanceRejectedCommands = 0;
		SkeletalInstanceSingleCommands = 0;
		SkeletalInstanceMergedDrawCalls = 0;
		SkeletalInstanceMergedInstances = 0;
		SkeletalInstanceOutputCommands = 0;

		GlobalSkinMatrixCharacters = 0;
		GlobalSkinMatrixCount = 0;
		GlobalSkinMatrixUploadBytes = 0;
		GlobalSkinMatrixBuildFailures = 0;
		GlobalSkinMatrixCommandReuses = 0;
		GlobalSkinMatrixPoseCacheHits = 0;

		for (uint32 Index = 0; Index < (uint32)ERenderPass::MAX; ++Index)
		{
			PassDrawCalls[Index] = 0;
		}
	}

	static void RecordDrawCall(ERenderPass Pass, bool bGpuSkinned, bool bInstanced, uint32 InstanceCount)
	{
		++SkeletalDrawCalls;

		if (bGpuSkinned)
		{
			++SkeletalGpuSkinDrawCalls;
		}
		else
		{
			++SkeletalCpuSkinDrawCalls;
		}

		const uint32 PassIndex = static_cast<uint32>(Pass);
		if (PassIndex < (uint32)ERenderPass::MAX)
		{
			++PassDrawCalls[PassIndex];
		}

		if (bInstanced)
		{
			++SkeletalInstancedDrawCalls;
			SkeletalSubmittedInstances += InstanceCount;
		}
		else
		{
			SkeletalSubmittedInstances += 1;
		}
	}

	static void RecordBatchAnalysis(
		uint32 InGpuSkinCommands,
		uint32 InBatchableCommands,
		uint32 InRejectedCommands,
		uint32 InUniqueBatchKeys)
	{
		SkeletalGpuSkinCommands = InGpuSkinCommands;
		SkeletalBatchableCommands = InBatchableCommands;
		SkeletalBatchRejectedCommands = InRejectedCommands;
		SkeletalBatchUniqueKeys = InUniqueBatchKeys;
		SkeletalEstimatedInstancedDrawCalls = InUniqueBatchKeys;
	}

	static void RecordInstanceBatching(
		uint32 InCandidates,
		uint32 InRejectedCommands,
		uint32 InSingleCommands,
		uint32 InMergedDrawCalls,
		uint32 InMergedInstances,
		uint32 InOutputCommands)
	{
		SkeletalInstanceCandidates = InCandidates;
		SkeletalInstanceRejectedCommands = InRejectedCommands;
		SkeletalInstanceSingleCommands = InSingleCommands;
		SkeletalInstanceMergedDrawCalls = InMergedDrawCalls;
		SkeletalInstanceMergedInstances = InMergedInstances;
		SkeletalInstanceOutputCommands = InOutputCommands;
	}

	static void RecordGlobalSkinMatrixStats(
		uint32 InCharacters,
		uint32 InMatrixCount,
		uint32 InUploadBytes,
		uint32 InBuildFailures,
		uint32 InCommandReuses,
		uint32 InPoseCacheHits)
	{
		GlobalSkinMatrixCharacters = InCharacters;
		GlobalSkinMatrixCount = InMatrixCount;
		GlobalSkinMatrixUploadBytes = InUploadBytes;
		GlobalSkinMatrixBuildFailures = InBuildFailures;
		GlobalSkinMatrixCommandReuses = InCommandReuses;
		GlobalSkinMatrixPoseCacheHits = InPoseCacheHits;
	}

	static uint32 GetPassDrawCalls(ERenderPass Pass)
	{
		const uint32 PassIndex = static_cast<uint32>(Pass);
		return PassIndex < (uint32)ERenderPass::MAX ? PassDrawCalls[PassIndex] : 0;
	}
};

// --- LOD Distribution Counter ---
#if STATS
struct FLODStats
{
	static uint32 LODCount[4];
	static void Reset() { LODCount[0] = LODCount[1] = LODCount[2] = LODCount[3] = 0; }
	static void Record(uint32 LOD) { if (LOD < 4) ++LODCount[LOD]; }
	static uint32 GetLOD0() { return LODCount[0]; }
	static uint32 GetLOD1() { return LODCount[1]; }
	static uint32 GetLOD2() { return LODCount[2]; }
	static uint32 GetLOD3() { return LODCount[3]; }
};
#define LOD_STATS_RESET()       FLODStats::Reset()
#define LOD_STATS_RECORD(LOD)   FLODStats::Record(LOD)
#else
#define LOD_STATS_RESET()       ((void)0)
#define LOD_STATS_RECORD(LOD)   ((void)0)
#endif

// --- SCOPE_STAT 매크로 ---
#if STATS
#define SCOPE_STAT_CONCAT2(a, b) a##b
#define SCOPE_STAT_CONCAT(a, b)  SCOPE_STAT_CONCAT2(a, b)
#define SCOPE_STAT(Name)           FScopedTimer SCOPE_STAT_CONCAT(_ScopedTimer_, __COUNTER__)(Name)
#define SCOPE_STAT_CAT(Name, Cat)  FScopedTimer SCOPE_STAT_CONCAT(_ScopedTimer_, __COUNTER__)(Name, Cat)
#else
#define SCOPE_STAT(Name)           ((void)0)
#define SCOPE_STAT_CAT(Name, Cat)  ((void)0)
#endif
