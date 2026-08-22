// Snake on a 6x6 wrapping grid. Tilt an MMA8452 on i2c1 to steer.
// No lists in the language, so each cell holds a countdown: the head is
// stamped with the length and every cell ticks down one per frame, so the
// tail clears itself. Six 4 bit cells per row int, max length 15.
fn wp -> i:
if i == -1:
return 5
end
if i == 6:
return 0
end
return i
end
fn gr -> i g0 g1 g2 g3 g4 g5:
if i==0:
return g0
end
if i==1:
return g1
end
if i==2:
return g2
end
if i==3:
return g3
end
if i==4:
return g4
end
return g5
end
fn pr -> i y new old:
if i == y:
return new
end
return old
end
fn gc -> row x:
return (row >> (x * 4)) & 15
end
fn sc -> row x v:
return (row & (~(15 << (x * 4)))) | (v << (x * 4))
end
fn age -> x y g0 g1 g2 g3 g4 g5:
return gc (gr y g0 g1 g2 g3 g4 g5) x
end
fn dec1 -> v:
if v > 0:
return v - 1
end
return 0
end
fn decRow -> row:
c0 = dec1 (gc row 0)
c1 = dec1 (gc row 1)
c2 = dec1 (gc row 2)
c3 = dec1 (gc row 3)
c4 = dec1 (gc row 4)
c5 = dec1 (gc row 5)
return c0 | (c1 << 4) | (c2 << 8) | (c3 << 12) | (c4 << 16) | (c5 << 20)
end
fn cellc -> row x ax ay y:
if (x == ax) & (y == ay):
write "\e[38;5;196m@"
else:
if gc row x:
write "\e[38;5;46m#"
else:
write "\e[38;5;238m."
end
end
end
fn rrow -> row y ax ay:
write "   "
cellc row 0 ax ay y
cellc row 1 ax ay y
cellc row 2 ax ay y
cellc row 3 ax ay y
cellc row 4 ax ay y
cellc row 5 ax ay y
print "\e[0m\e[K"
end
// 16 bit LCG, the multiply stays under 2^31
fn lcg -> s:
return ((s * 25173) + 13849) & 65535
end
// packed as seed<<6 | y<<3 | x
ts fn pick -> n sd g0 g1 g2 g3 g4 g5:
s1 = lcg sd
x = s1 & 7
y = (s1 >> 3) & 7
if n == 0:
return (s1 << 6) | (y << 3) | x
end
if (x > 5) | (y > 5):
return pick (n - 1) s1 g0 g1 g2 g3 g4 g5
end
if gc (gr y g0 g1 g2 g3 g4 g5) x:
return pick (n - 1) s1 g0 g1 g2 g3 g4 g5
end
return (s1 << 6) | (y << 3) | x
end
// registers read back unsigned, the sensor means them as signed bytes
fn sgn -> v:
if v > 127:
return v - 256
end
return v
end
// 0x1D, OUT_X_MSB=1 OUT_Y_MSB=3. Swap both for `return 0` if no sensor.
fn rx -> u:
return sgn (i2cRegRead "i2c1" 29 1)
end
fn ry -> u:
return sgn (i2cRegRead "i2c1" 29 3)
end
// 0 right, 1 down, 2 left, 3 up. Turning back on yourself is ignored.
fn ndir -> d:
x = rx 0
y = ry 0
if (x > 20) & (d != 2):
return 0
end
if (x < -20) & (d != 0):
return 2
end
if (y > 20) & (d != 3):
return 1
end
if (y < -20) & (d != 1):
return 3
end
return d
end
ts fn poll -> n d:
if n == 0:
return d
end
sleep 20
return poll (n - 1) (ndir d)
end
fn dx -> d:
if d == 0:
return 1
end
if d == 2:
return -1
end
return 0
end
fn dy -> d:
if d == 1:
return 1
end
if d == 3:
return -1
end
return 0
end
fn grow -> ate ln:
if ate & (ln < 15):
return ln + 1
end
return ln
end
fn newAp -> ate sd ap g0 g1 g2 g3 g4 g5:
if ate:
return pick 40 sd g0 g1 g2 g3 g4 g5
end
return (sd << 6) | ap
end
// halt unwinds like ctrl c, nothing is left behind
fn over -> ln:
print "\e[K"
write "\e[1;38;5;196m   GAME OVER   length "
write ln
print "\e[0m\e[K"
halt 0
end
ts fn st -> sd ap hx hy dr ln g0 g1 g2 g3 g4 g5:
ax = ap & 7
ay = (ap >> 3) & 7
write "\e[H"
print "\e[1;38;5;51m   Snake\e[0m\e[K"
print "\e[K"
rrow g0 0 ax ay
rrow g1 1 ax ay
rrow g2 2 ax ay
rrow g3 3 ax ay
rrow g4 4 ax ay
rrow g5 5 ax ay
print "\e[K"
write "\e[38;5;220m   length "
write ln
print "\e[0m\e[K"
nd = poll 10 dr
nhx = wp (hx + (dx nd))
nhy = wp (hy + (dy nd))
hit = age nhx nhy g0 g1 g2 g3 g4 g5
// count 1 is the tail tip, it clears this frame
if hit > 1:
over ln
end
ate = (nhx == ax) & (nhy == ay)
nln = grow ate ln
d0 = decRow g0
d1 = decRow g1
d2 = decRow g2
d3 = decRow g3
d4 = decRow g4
d5 = decRow g5
nr = sc (gr nhy d0 d1 d2 d3 d4 d5) nhx nln
e0 = pr 0 nhy nr d0
e1 = pr 1 nhy nr d1
e2 = pr 2 nhy nr d2
e3 = pr 3 nhy nr d3
e4 = pr 4 nhy nr d4
e5 = pr 5 nhy nr d5
sd1 = lcg sd
r = newAp ate sd1 ap e0 e1 e2 e3 e4 e5
return st (r >> 6) (r & 63) nhx nhy nd nln e0 e1 e2 e3 e4 e5
end
ep fn main -> :
print "\e[2J\e[?25l"
s0 = ((rx 0) + ((ry 0) * 31) + 7) & 65535
return st s0 36 2 2 0 3 0 0 801 0 0 0
end
