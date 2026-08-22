fn clamp -> x lo hi:
    if x < lo:
        return lo
    else:
        if x > hi:
            return hi
        else:
            return x
        end
    end
end

ep fn main -> :
    a = clamp 50 0 10
    b = clamp (0 - 5) 0 10
    c = clamp 7 0 10
    return a + b + c
end
