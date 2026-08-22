fn makeAdder -> n:
    return (\x -> x + n)
end

ep fn main -> :
    add10 = makeAdder 10
    return add10 32
end
