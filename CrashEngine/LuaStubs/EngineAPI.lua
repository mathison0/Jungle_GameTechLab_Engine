---@meta CrashEngine

-- 이 파일은 Lua Language Server가 자동완성과 타입 힌트를 만들 때만 읽는 설명서입니다.
-- 게임 런타임에서는 로드하지 않습니다. 실제 구현은 C++의 ScriptSystem/ScriptComponent 바인딩에 있습니다.
-- Class Actor =========================================================
---@class Actor
local Actor = {}

---@return boolean
function Actor:IsValid() end

---@return string
function Actor:GetName() end

---@return string
function Actor:GetClassName() end

---@return Vec3
function Actor:GetLocation() end

---@param location Vec3
---@return boolean
function Actor:SetLocation(location) end

---@param delta Vec3
---@return boolean
function Actor:AddWorldOffset(delta) end

---@return Actor
function ScriptComponent:GetActor() end

---@return Vec3
function Actor:GetRotation() end

---@param rotation Vec3
---@return boolean
function Actor:SetRotation(rotation) end

---@return Vec3
function Actor:GetScale() end

---@param scale Vec3
---@return boolean
function Actor:SetScale(scale) end

---@return Vec3
function Actor:GetForward() end

---@return boolean
function Actor:IsVisible() end

---@param visible boolean
---@return boolean
function Actor:SetVisible(visible) end
-- ====================================================================

-- Class Component ====================================================
---@class Component
local Component = {}

---@return boolean
function Component:IsValid() end

---@return string
function Component:GetName() end

---@return string
function Component:GetClassName() end

---@param className string
---@return boolean
function Component:IsA(className) end

---@return Actor
function Component:GetOwner() end

---@return boolean
function Component:IsActive() end

---@param active boolean
---@return boolean
function Component:SetActive(active) end

---@param className string
---@param index? integer
---@return Component
function Actor:GetComponent(className, index) end

---@param className? string
---@return Component[]
function Actor:GetComponents(className) end

---@param className string
---@param index? integer
---@return Component
function ScriptComponent:GetComponent(className, index) end

---@param className? string
---@return Component[]
function ScriptComponent:GetComponents(className) end
-- ====================================================================

---@class Vec3
---@field x number
---@field y number
---@field z number
---@field X number
---@field Y number
---@field Z number
---@field [integer] number

---@class LuaScriptPropertyDescriptor
---@field type "int"|"float"|"bool"|"string"|"vec3"
---@field default? integer|number|boolean|string|Vec3
---@field min? number
---@field max? number
---@field speed? number

---@class ScriptComponent
---@field properties? table<string, LuaScriptPropertyDescriptor>
local ScriptComponent = {}

---@param message string
function Log(message) end

---@class Input
Input = {}

---@param key integer
---@return boolean
function Input.GetKey(key) end

---@param key integer
---@return boolean
function Input.GetKeyDown(key) end

---@param key integer
---@return boolean
function Input.GetKeyUp(key) end

---@param axisName string
---@return number
function Input.GetAxis(axisName) end

function ScriptComponent:BeginPlay() end

---@param deltaTime number
function ScriptComponent:Tick(deltaTime) end

function ScriptComponent:EndPlay() end

---@param func fun()
---@return integer
function ScriptComponent:start_coroutine(func) end

---@param id integer
---@return boolean
function ScriptComponent:stop_coroutine(id) end

---@return Vec3
function ScriptComponent:GetActorLocation() end

---@param location Vec3
---@return boolean
function ScriptComponent:SetActorLocation(location) end

---@param delta Vec3
---@return boolean
function ScriptComponent:AddActorWorldOffset(delta) end

---@return Vec3
function ScriptComponent:GetActorRotation() end

---@param rotation Vec3
---@return boolean
function ScriptComponent:SetActorRotation(rotation) end

---@return Vec3
function ScriptComponent:GetActorScale() end

---@param scale Vec3
---@return boolean
function ScriptComponent:SetActorScale(scale) end

---@return Vec3
function ScriptComponent:GetActorForward() end

---@return boolean
function ScriptComponent:IsActorVisible() end

---@param visible boolean
---@return boolean
function ScriptComponent:SetActorVisible(visible) end
