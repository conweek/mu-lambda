// MMA8452 3-axis accelerometer (I2C addr 0x1D = 29)
// Reads X, Y, Z MSB registers in a loop
//
// Register map:
//   0x2A (42) = CTRL_REG1 (write 0x01 to activate)
//   0x01 = OUT_X_MSB
//   0x03 = OUT_Y_MSB
//   0x05 = OUT_Z_MSB

// activate sensor: CTRL_REG1 = 0x01
i2cRegWrite "i2c1" 29 42 1
sleep 20

// partially apply device and addr for reuse
rd = i2cRegRead "i2c1" 29

ep fn main -> :
    xval = rd 1
    print "X:"
    print xval

    yval = rd 3
    print "Y:"
    print yval

    zval = rd 5
    print "Z:"
    print zval

    print "---"
    sleep 500
    return main
end
