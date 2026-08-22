fn add -> a b:
    return a + b
end

fn mul -> a b:
    return a * b
end

ep fn main -> :
    step1 = add 5 3
    step2 = mul step1 2
    step3 = add step2 (0 - 6)
    return step3
end
