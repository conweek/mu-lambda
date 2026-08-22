ts fn count -> curr:
    if curr == 100:
        return 100
    end

    print (curr)
    return (count (curr + 1))
end

ep ts fn main -> :
    x = (count 1)
    return main 
end
