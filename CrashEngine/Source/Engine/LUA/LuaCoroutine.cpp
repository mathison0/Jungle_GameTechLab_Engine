#include "LuaCoroutine.h"
#include "Core/Logging/LogMacros.h"
#include "Engine/Runtime/Engine.h"

void FExecuteCommand::ParseResult(sol::protected_function_result InResult)
{
    bIsValid = InResult.valid();
    Status = InResult.status();

    for (int i = 0; i < InResult.return_count(); ++i)
    {
        Args.push_back(InResult.get<sol::object>(i));
    }
}

void FWaitCommandRealTime::Run(sol::coroutine& InCoroutine, const FCouroutineContext& Context)
{
    CurrentTime += Context.DeltaTime;

    if (CurrentTime >= TargetTime)
        ParseResult(InCoroutine());
}

void FWaitNextFrame::Run(sol::coroutine& InCoroutine, const FCouroutineContext& Context)
{
    ParseResult(InCoroutine(Context.DeltaTime));
}

void CoroutineExecutor::SetCommand(FExecuteCommand* NewCommand)
{
    if (Command)
        delete Command;

    Command = NewCommand;
}

void CoroutineExecutor::Tick(const FCouroutineContext& Context)
{
    if (!Command)                          // Command 설정이 안되면
        SetCommand(new FExecuteCommand()); // Default로 해주세요

    // Command를 돌려봅니다. Lua가 돌 수도 있고, c++에서 처리할 수도 있습니다.
    // 예를들어 Wait의 경우 시간 만큼 기다리는건 c++이지만 목표시간 경과시 결과를 받아오는 부분은 LUA입니다.
    Command->Run(Task.Coroutine, Context);

    if (!Command->IsEnd()) // 안끝났으면
        return;            // 더 이상 볼것도 없네요. 별다른 설정 없이 다음 Tick에도 불러와지게 합니다.

    if (!Command->IsValid()) // 유효하지 않으면
        return;              // 더 이상 볼것도 없습니다

    if (Command->GetStatus() == sol::call_status::yielded) // 코루틴 결과를 봅니다. 혹시 yield 인가요
    {
        FString NextCommand = Command->GetResultParam<FString>(0); // 그렇다면 Command를 봅니다
        UE_LOG([Coroutine], Info, "NextCommand %s", NextCommand);
        // Command는 LUA의 Coroutine.Yield의 첫번째 파라메터에 1:1 로 대응되야 합니다.
        if (NextCommand == "wait_time") // 기다리라네요
        {
            float TargetTime = Command->GetResultParam<float>(1); // 몇 초동안이요
            SetCommand(new FWaitCommandRealTime(TargetTime));     // 대기 명령을 시간을 할당해서 설정해줍니다
        }
        else if (NextCommand == "wait_until") // 무언가를 기다리라네요
        {
            auto Predicate = Command->GetResultParam<sol::function>(1); // 뭘 기다릴까요
            SetCommand(new FWaitUntilPredicate(Predicate));             // 대기 명령 렛츠고
        }
    }
    UE_LOG([Coroutine], Info, "Coroutine End");

    // 마지막으로 새로 설정된 커맨드가 없다면 그 커맨드가 IsValid검사에 들어갑니다.
}

bool CoroutineExecutor::IsValid()
{
    if (!Command->IsEnd()) // Command가 안끝났으면 아직 유효합니다.
        return true;

    if (!Command->IsValid()) // 무슨 일이 생겼다면 유효하지 않아요
        return false;

    if (Command->GetStatus() == sol::call_status::yielded) // Yield 상태면 아직 진행 중이니 유효합니다
        return true;

    return false; // Command도 끝나고 Yield상태도 아니면 더 이상 이 객체는 필요가 없습니다.
}

bool FCoroutineExecutorSet::Start(const sol::function& LuaFunc)
{
    if (!LuaFunc.valid())
    {
        return false;
    }

    if (bIsTicking)
    {
        return false;
    }

    const void* FunctionKey = LuaFunc.pointer();
    if (!FunctionKey)
    {
        return false;
    }

    auto Existing = Executors.find(FunctionKey);
    if (Existing != Executors.end())
    {
        CoroutineExecutor* ExistingExecutor = Existing->second;
        if (ExistingExecutor && ExistingExecutor->IsValid() && !PendingStopKeys.contains(FunctionKey))
        {
            return false;
        }

        delete ExistingExecutor;
        Executors.erase(Existing);
    }

    const uint32 ExecutorId = ++NextExecutorId;
    const FString ThreadFunctionName = "__coroutine_entry_" + std::to_string(ExecutorId);

    FLuaCoroutineTask Task;

    // Lua VM 안에 코루틴용 thread 생성
    sol::state* Lua = &GEngine->GetScriptSystem().GetLua();
	Task.Thread = sol::thread::create(*Lua);

    // thread 내부 state에 접근하기 위한 view
    sol::state_view ThreadState = Task.Thread.state();

    // Lua에서 직접 넘겨받은 함수를 thread 쪽에 꽂음
    ThreadState[ThreadFunctionName] = LuaFunc;

    // thread 안의 함수를 coroutine으로 감쌈
    Task.Coroutine = sol::coroutine(ThreadState[ThreadFunctionName]);


    Executors[FunctionKey] = new CoroutineExecutor(std::move(Task));
    return true;
}

bool FCoroutineExecutorSet::Stop(const sol::function& LuaFunc)
{
    if (!LuaFunc.valid())
    {
        UE_LOG([Coroutine], Warning, "Lua function is invalid for stop.");
        return false;
    }

    const void* FunctionKey = LuaFunc.pointer();
    auto It = Executors.find(FunctionKey);
    if (It == Executors.end())
    {
        return false;
    }

    if (bIsTicking)
    {
        PendingStopKeys.insert(FunctionKey);
        return true;
    }

    delete It->second;
    Executors.erase(It);
    return true;
}

void FCoroutineExecutorSet::Tick(const FCouroutineContext& Context)
{
    // 일단 한바퀴 돌립니다.
    bIsTicking = true;
    for (auto& pair : Executors)
    {
        if (!PendingStopKeys.contains(pair.first))
        {
            pair.second->Tick(Context);
        }
    }
    bIsTicking = false;

    // 끝나거나 쓸모 없는 놈들을 삭제합니다.
    for (auto It = Executors.begin(); It != Executors.end();)
    {
        CoroutineExecutor* Executor = It->second;

        if (PendingStopKeys.contains(It->first) || Executor == nullptr || !Executor->IsValid())
        {
            delete Executor;
            It = Executors.erase(It);
        }
        else
        {
            ++It;
        }
    }
    PendingStopKeys.clear();
}
