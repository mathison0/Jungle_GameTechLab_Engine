#include "FCoroutineSceduler.h"

void FWaitCommandRealTime::Run(sol::coroutine& InCoroutine, FCouroutineContext& Context)
{
    CurrentTime += Context.DeltaTime;

    if (CurrentTime >= TargetTime)
        Result = InCoroutine();
}

void FWaitNextFrame::Run(sol::coroutine& InCoroutine, FCouroutineContext& Context)
{
    Result = InCoroutine(Context.DeltaTime);
}

void CoroutineExecutor::SetCommand(FExecuteCommand* NewCommand)
{
    if (Command)
        delete Command;

    Command = NewCommand;
}

void CoroutineExecutor::Tick(FCouroutineContext& Context)
{
    if (!Command)                         // Command 설정이 안되면
        SetCommand(new FExecuteCommand()); // Default로 해주세요

    // Command를 돌려봅니다. Lua가 돌 수도 있고, c++에서 처리할 수도 있습니다.
    // 예를들어 Wait의 경우 시간 만큼 기다리는건 c++이지만 목표시간 경과시 결과를 받아오는 부분은 LUA입니다.
    Command->Run(Task.Coroutine, Context);

    if (!Command->IsEnd()) // 안끝났으면
        return;            // 더 이상 볼것도 없네요. 별다른 설정 없이 다음 Tick에도 불러와지게 합니다.

    auto Result = Command->GetResult(); // 끝났으면 결과를 봐요
    if (!Result.valid())                // 유효하지 않으면
        return;                         // 더 이상 볼것도 없습니다

    if (Result.status() == sol::call_status::yielded) // 코루틴 결과를 봅니다. 혹시 yield 인가요
    {
        std::string Command = Result.get<std::string>(0); // 그렇다면 Command를 봅니다
        // Command는 LUA의 Coroutine.Yield의 첫번째 파라메터에 1:1 로 대응되야 합니다.
        if (Command == "wait_time") // 기다리라네요
        {
            float TargetTime = Result.get<float>(1);         // 몇 초동안이요
            SetCommand(new FWaitCommandRealTime(TargetTime)); // 대기 명령을 시간을 할당해서 설정해줍니다
        }
    }

    // 마지막으로 새로 설정된 커맨드가 없다면 그 커맨드가 IsValid검사에 들어갑니다.
}

bool CoroutineExecutor::IsValid()
{
    if (!Command->IsEnd()) // Command가 안끝났으면 아직 유효합니다.
        return true;

    auto Result = Command->GetResult();
    if (!Result.valid()) // 무슨 일이 생겼다면 유효하지 않아요
        return false;

    if (Result.status() == sol::call_status::yielded) // Yield 상태면 아직 진행 중이니 유효합니다
        return true;

    return false; // Command도 끝나고 Yield상태도 아니면 더 이상 이 객체는 필요가 없습니다.
}

void CoroutineExecutorSet::Start(const std::string& FunctionName)
{
    sol::function LuaFunc = Lua[FunctionName];

    if (!LuaFunc.valid())
    {
        // UE_Log("Lua function not found: %s\n", FunctionName.c_str());
        return;
    }

    FLuaCoroutineTask Task;

    // Lua VM 안에 코루틴용 thread 생성
    Task.Thread = sol::thread::create(Lua);

    // thread 내부 state에 접근하기 위한 view
    sol::state_view ThreadState = Task.Thread.state();

    // 메인 Lua state에 있던 함수를 thread 쪽에 꽂음
    ThreadState[FunctionName] = LuaFunc;

    // thread 안의 함수를 coroutine으로 감쌈
    Task.Coroutine = sol::coroutine(ThreadState[FunctionName]);


    Executors[FunctionName] = new CoroutineExecutor(std::move(Task));
}

void CoroutineExecutorSet::Stop(const FString& FunctionName)
{
    for (auto It = Executors.begin(); It != Executors.end();)
    {
        if (It->first == FunctionName)
        {
            delete It->second;
            It = Executors.erase(It);
            break;
        }
        else
        {
            ++It;
        }
    }
}

void CoroutineExecutorSet::Tick(FCouroutineContext& Context)
{
    // 일단 한바퀴 돌립니다.
    for (auto& pair : Executors)
    {
        pair.second->Tick(Context);
    }

    // 끝나거나 쓸모 없는 놈들을 삭제합니다.
    for (auto It = Executors.begin(); It != Executors.end();)
    {
        CoroutineExecutor* Executor = It->second;

        if (Executor == nullptr || !Executor->IsValid())
        {
            delete Executor;
            It = Executors.erase(It);
        }
        else
        {
            ++It;
        }
    }
}

