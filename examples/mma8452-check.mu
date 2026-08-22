// MMA8452 sanity check, i2c1 at 0x1D (29). WHO_AM_I must read 42.
// Registers come back unsigned but mean signed, so a small negative tilt
// reads 255 until converted. Flat on a desk: X,Y near 0 and Z near 64.
fn sgn -> v:
if v > 127:
return v - 256
end
return v
end
who = i2cRegRead "i2c1" 29 13
print "WHO_AM_I, expect 42:"
print who
// CTRL_REG1 = 1, go active
i2cRegWrite "i2c1" 29 42 1
sleep 20
rd = i2cRegRead "i2c1" 29
ep fn main -> :
print "raw X Y Z:"
print (rd 1)
print (rd 3)
print (rd 5)
print "signed X Y Z:"
print (sgn (rd 1))
print (sgn (rd 3))
print (sgn (rd 5))
print "---"
sleep 500
return main
end
