local Co = {}

function Co.Wait(Time)
    return coroutine.yield("wait_time", Time)
end

function Co.WaitNextFrame()
    return coroutine.yield("wait_next_frame")
end

function Co.WaitUntil(Predicate)
    return coroutine.yield("wait_until", Predicate)
end

return Co
