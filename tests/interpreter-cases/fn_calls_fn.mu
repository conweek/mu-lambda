fn double -> x:
    return x + x
end

fn quadruple -> x:
    return double (double x)
end

ep fn main -> :
    return quadruple 10
end
