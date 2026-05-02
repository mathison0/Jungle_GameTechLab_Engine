local Vec = {}

local function NewVector(X, Y, Z)
    return FVector.new(X or 0.0, Y or 0.0, Z or 0.0)
end

function Vec.New(X, Y, Z)
    return NewVector(X, Y, Z)
end

function Vec.Zero()
    return NewVector(0.0, 0.0, 0.0)
end

function Vec.LengthSquared(V)
    local X = V.X or V.x or V[1] or 0.0
    local Y = V.Y or V.y or V[2] or 0.0
    local Z = V.Z or V.z or V[3] or 0.0
    return X * X + Y * Y + Z * Z
end

function Vec.Length(V)
    return math.sqrt(Vec.LengthSquared(V))
end

function Vec.Normalized(V)
    local Length = Vec.Length(V)
    if Length <= 0.000001 then
        return Vec.Zero()
    end

    return V * (1.0 / Length)
end

function Vec.DirectionTo(From, To)
    return Vec.Normalized(To - From)
end

function Vec.DistanceSquared(A, B)
    return Vec.LengthSquared(B - A)
end

return Vec
