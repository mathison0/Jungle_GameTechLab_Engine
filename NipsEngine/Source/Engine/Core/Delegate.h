#pragma once

#include <functional>
#include <algorithm>
#include <vector>

#define DECLARE_DELEGATE(Name, ...) using Name = TDelegate<__VA_ARGS__>

template<typename... Args>
class TDelegate
{
public:
	using HandlerType = std::function<void(Args...)>;

	void Add(const HandlerType& handler)
	{
        Handlers.push_back({ nullptr, handler });
	}

	template <typename T> 
	void AddDynamic(T* Instance, void (T::* Func)(Args...))
	{
        Handlers.push_back({ Instance, [Instance, Func](Args... args)
        { 
			(Instance->*Func)(args...);
	    } });
	}

	template <typename T>
	void RemoveDynamic(T* Instance)
	{
		Handlers.erase(
			std::remove_if(
				Handlers.begin(),
				Handlers.end(),
				[Instance](const FHandlerEntry& Entry)
				{
					return Entry.Owner == Instance;
				}),
			Handlers.end());
	}

	void Broadcast(Args... args)
	{
		auto HandlersCopy = Handlers;
		for (auto& Entry : HandlersCopy)
		{
			if (Entry.Handler)
			{
                Entry.Handler(args...);
			}
		}
	}

	void Clear()
    {
        Handlers.clear();
    }

	private:
		struct FHandlerEntry
		{
			void* Owner = nullptr;
			HandlerType Handler;
		};

		std::vector<FHandlerEntry> Handlers;
};
