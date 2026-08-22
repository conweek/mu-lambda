// MMA8452 3-axis accelerometer (I2C addr 0x1D = 29)
// Reads X, Y, Z MSB registers in a loop
//
// NOTE: MMA8452 must be set to ACTIVE mode before reads
// return real data (write 0x01 to CTRL_REG1 at 0x2A).
// That requires a 2-byte I2C write [reg, val] in one
// transaction -- current i2cWrite only sends 1 byte, so
// activate the device externally or add a regWrite builtin.
//
// Register map:
//   0x01 = OUT_X_MSB
//   0x03 = OUT_Y_MSB
//   0x05 = OUT_Z_MSB

ep fn main -> :
    wreg = i2cWrite "i2c1" 29

    // set pointer to OUT_X_MSB then read
    wreg 1
    xval = i2cRead "i2c1" 29
    print "X:"
    print xval

    // set pointer to OUT_Y_MSB then read
    wreg 3
    yval = i2cRead "i2c1" 29
    print "Y:"
    print yval

    // set pointer to OUT_Z_MSB then read
    wreg 5
    zval = i2cRead "i2c1" 29
    print "Z:"
    print zval

    print "---"
    sleep 500
    return main
end
