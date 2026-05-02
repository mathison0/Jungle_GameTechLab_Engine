---@class LuaCoroutine
local Coroutine = {}

---@param Time number
function Coroutine.Wait(Time)
    return coroutine.yield("wait_time", Time)
end

function Coroutine.WaitNextFrame()
    return coroutine.yield("wait_next_frame")
end

---@param Predicate fun(): boolean
function Coroutine.WaitUntil(Predicate)
    return coroutine.yield("wait_until", Predicate)
end

return Coroutine
