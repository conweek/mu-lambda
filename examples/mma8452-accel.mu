// MMA8452 3-axis accelerometer (I2C addr 0x1D = 29)
// Reads X, Y, Z MSB registers in a loop
//
// Register map:
//   0x2A = CTRL_REG1 (write 0x01 to activate)
//   0x01 = OUT_X_MSB
//   0x03 = OUT_Y_MSB
//   0x05 = OUT_Z_MSB

fn activate -> :
    // CTRL_REG1 (42) = 0x01 to enter ACTIVE mode
    i2cRegWrite "i2c1" 29 42 1
end

ep fn main -> :
    activate

    // set pointer to OUT_X_MSB then read
    i2cWrite "i2c1" 29 1
    xval = i2cRead "i2c1" 29
    print "X:"
    print xval

    // set pointer to OUT_Y_MSB then read
    i2cWrite "i2c1" 29 3
    yval = i2cRead "i2c1" 29
    print "Y:"
    print yval

    // set pointer to OUT_Z_MSB then read
    i2cWrite "i2c1" 29 5
    zval = i2cRead "i2c1" 29
    print "Z:"
    print zval

    print "---"
    sleep 500
    return main
end
