fn apply -> f x:
    return f x
end

ep fn main -> :
    return apply (\x -> x * x) 7
end
