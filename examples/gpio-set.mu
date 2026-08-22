// This file is built for an ESP32S3
ts fn set -> val:

    print val
    sleep 250

    gset = (\x -> gpioSet "gpio0" 1 x)

    if val == 1:
        x = gset 1
        return (set 0)
    else:
        x = gset 0
        return (set 1)
    end
end

ep ts fn main -> :
    x = set 1
    return main
end
