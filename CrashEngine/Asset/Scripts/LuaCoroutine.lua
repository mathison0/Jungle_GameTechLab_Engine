local Coroutine = {}

function Coroutine.Wait(Time)
    return coroutine.yield("wait_time", Time)
end

function Coroutine.WaitNextFrame()
    return coroutine.yield("wait_next_frame")
end

function Coroutine.WaitUntil(Predicate)
    return coroutine.yield("wait_until", Predicate)
end

return Coroutine