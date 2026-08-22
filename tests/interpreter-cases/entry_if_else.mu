fn abs -> x:
    if x < 0:
        return 0 - x
    else:
        return x
    end
end

ep fn main -> :
    return abs (0 - 42)
end
