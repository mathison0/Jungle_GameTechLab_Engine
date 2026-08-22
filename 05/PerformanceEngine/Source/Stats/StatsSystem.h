#pragma once

#include <array>
#include <vector>

#include "Types/PlatformTypes.h"
#include "Types/String.h"

struct FPickState;

struct FBenchmarkRunMetadata
{
	FString AdapterName;
	uint64 DedicatedVideoMemoryMB = 0;
	int32 ViewportWidth = 0;
	int32 ViewportHeight = 0;
};

class FStatsSystem
{
public:
	struct FFrameBenchmarkSample
	{
		uint64 Frame = 0;
		double FrameTimeMs = 0.0;
		double FramesPerSecond = 0.0;
	};

	struct FPickBenchmarkSample
	{
		uint64 Frame = 0;
		double PickTimeMs = 0.0;
		bool bHit = false;
		int32 SelectedPrimitiveId = -1;
	};

	void Reset();
	void BeginFrame();
	void EndFrame();
	void ApplyPickState(const FPickState& InPickState);
	void RecordPickEvent(const FPickState& InPickState);
	void WriteBenchmarkLogs(const FBenchmarkRunMetadata& InMetadata) const;

	double GetFrameTimeMs() const { return FrameTimeMs; }
	double GetFramesPerSecond() const { return FramesPerSecond; }
	double GetAverageFrameTimeMs() const { return AverageFrameTimeMs; }
	double GetAverageFramesPerSecond() const { return AverageFramesPerSecond; }
	double GetLastPickTimeMs() const { return LastPickTimeMs; }
	double GetTotalPickTimeMs() const { return TotalPickTimeMs; }
	uint64 GetTotalPickCount() const { return TotalPickCount; }
	uint64 GetFrameNumber() const { return FrameNumber; }
	uint64 GetTotalAABBCheckCount() const { return TotalAABBCheckCount; }
	double GetPickWorldBVHTimeMs() const { return LastPickTimeMsWorldBVH; }

private:
	uint64 LastFrameCounter = 0;
	double FrameTimeMs = 0.0;
	double FramesPerSecond = 0.0;
	double AverageFrameTimeMs = 0.0;
	double AverageFramesPerSecond = 0.0;
	double LastPickTimeMs = 0.0;
	double TotalPickTimeMs = 0.0;
	uint64 TotalPickCount = 0;
	uint64 FrameNumber = 0;
	uint64 TotalAABBCheckCount = 0;
	double LastPickTimeMsWorldBVH = 0.0;
	std::array<double, 60> FrameTimeHistoryMs = {};
	size_t FrameTimeHistoryIndex = 0;
	size_t FrameTimeHistoryCount = 0;
	double FrameTimeHistorySumMs = 0.0;

#if defined(BENCHMARK)
	std::vector<FFrameBenchmarkSample> FrameBenchmarkSamples;
	std::vector<FPickBenchmarkSample> PickBenchmarkSamples;
#endif
};
