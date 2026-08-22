ts fn sum -> n acc:
    if n == 0:
        return acc
    else:
        return sum (n - 1) (acc + n)
    end
end

ep fn main -> :
    return sum 100 0
end
