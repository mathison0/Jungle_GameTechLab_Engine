#pragma once

#include <functional>
#include <vector>

#include "CoreTypes.h"


template<typename... Args>
class TDelegate
{
    using HandlerType = std::function<void(Args...)>;
    struct FHandlerElement
    {
        void* Target;
        HandlerType Func;
    };

public:
    // 런타임 객체에 대한 지정 없이 핸들러 등록
    void Add(const HandlerType& handler)
    {
        AddInternal({nullptr, handler});
    }

    // Instance 명의로 핸들러를 등록
    template<typename T>
    void AddDynamic(T* Instance, void (T::*Func)(Args...))
    {
        AddInternal({
            Instance, 
            [Instance, Func](Args... args) { (Instance->*Func)(args...); }
        });
    }
    
    // Instance 명의로 등록된 핸들러를 모두 제거
    template<typename T>
    void RemoveDynamic(T* Instance)
    {
		if(Instance == nullptr) 
			return;

        Handlers.erase(
            std::remove_if(
                Handlers.begin(), Handlers.end(),
                [Instance](const FHandlerElement& Node) {
                    return Node.TargetObject == Instance;
                }
            ), 
            Handlers.end()
        );
    }
    
    void Clear()
    {
        Handlers.clear();
    }
    
    void Broadcast(Args... args)
    {
        for (FHandlerElement& Node : Handlers)
        {
            Node.Func(args...);
        }
    }

private:
    TArray<FHandlerElement> Handlers;
    void AddInternal(FHandlerElement handler)
    {
        Handlers.push_back(handler);
    }
};

#define DECLARE_DELEGATE(Name, ...) TDelegate<__VA_ARGS__> Name