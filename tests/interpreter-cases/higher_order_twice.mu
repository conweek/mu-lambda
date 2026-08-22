fn twice -> f x:
    return f (f x)
end

fn triple -> x:
    return x * 3
end

ep fn main -> :
    return twice triple 2
end
