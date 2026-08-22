// This file is built for an ESP32S3
ep ts fn main -> :

    val = gpioRead "gpio0" 0
    
    if val == 0:
        gpioSet "gpio0" 1 1
    else:
        gpioSet "gpio0" 1 0
    end

    return main
end
