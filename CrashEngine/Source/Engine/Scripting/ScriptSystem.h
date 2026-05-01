#pragma once
#include <memory>
#include <Sol/forward.hpp>

#include "Core/CoreTypes.h"

class FScriptSystem
{
public:
	FScriptSystem();
	~FScriptSystem();

	bool Initialize();
	void Shutdown();

	bool ExecuteString(const FString& Code) const;
	bool ExecuteFile(const FString& Path);
	
	sol::state& GetLua();

private:
	void RegisterEngineAPI();
	
private:
	std::unique_ptr<sol::state> Lua;
};
