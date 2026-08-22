fn square -> x:
    return x * x
end

fn addOne -> x:
    return x + 1
end

ep fn main -> :
    return addOne (square 5)
end
