fn compose -> f g x:
    return f (g x)
end

ep fn main -> :
    return compose (\x -> x + 1) (\x -> x * 10) 4
end
