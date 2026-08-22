#pragma once

#include "Core/Types/CoreTypes.h"

struct FDrawCommand;

class FSkeletalBatchAnalyzer
{
public:
	static void Analyze(const TArray<FDrawCommand>& Commands);
};
