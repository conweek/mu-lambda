// MMA8452 sanity check, i2c1 at 0x1D (29).
// WHO_AM_I (0x0D) must read 42. Anything else means wrong address,
// wrong device, or a bus that is not talking.
// The registers come back unsigned but the sensor means them as signed
// bytes, so a small negative tilt reads 255, not -1, until converted.
// Flat on a desk expect X and Y near 0 and Z near 64, since 1g is about
// 64 counts at the default +-2g range.
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
