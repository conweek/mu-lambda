// Conway's Game of Life on a 6x6 torus, one bitmask per row.
// Built for the nucleo_f429zi, the B1 user button pauses and resumes.
// Needs a terminal that understands ANSI escapes.

fn wpr -> i:
if i == -1:
return 5
end
if i==6:
return 0
end
return i
end
fn wpc -> i:
if i == -1:
return 5
end
if i==6:
return 0
end
return i
end
fn gr -> i r0 r1 r2 r3 r4 r5:
if i==0:
return r0
end
if i==1:
return r1
end
if i==2:
return r2
end
if i==3:
return r3
end
if i==4:
return r4
end
return r5
end
fn bt -> row c:
return (row >> c) & 1
end
fn nb -> r c r0 r1 r2 r3 r4 r5:
a0 = (bt (gr (wpr (r - 1)) r0 r1 r2 r3 r4 r5) (wpc (c - 1)))
a1 = (bt (gr (wpr (r - 1)) r0 r1 r2 r3 r4 r5) c)
a2 = (bt (gr (wpr (r - 1)) r0 r1 r2 r3 r4 r5) (wpc (c + 1)))
a3 = (bt (gr r r0 r1 r2 r3 r4 r5) (wpc (c - 1)))
a4 = (bt (gr r r0 r1 r2 r3 r4 r5) (wpc (c + 1)))
a5 = (bt (gr (wpr (r + 1)) r0 r1 r2 r3 r4 r5) (wpc (c - 1)))
a6 = (bt (gr (wpr (r + 1)) r0 r1 r2 r3 r4 r5) c)
a7 = (bt (gr (wpr (r + 1)) r0 r1 r2 r3 r4 r5) (wpc (c + 1)))
return a0 + a1 + a2 + a3 + a4 + a5 + a6 + a7
end
fn nc -> cur n:
return ((cur==1) & ((n==2) | (n==3))) | ((cur==0) & (n==3))
end
fn cr -> ri r0 r1 r2 r3 r4 r5:
b0 = (nc (bt (gr ri r0 r1 r2 r3 r4 r5) 0) (nb ri 0 r0 r1 r2 r3 r4 r5)) << 0
b1 = (nc (bt (gr ri r0 r1 r2 r3 r4 r5) 1) (nb ri 1 r0 r1 r2 r3 r4 r5)) << 1
b2 = (nc (bt (gr ri r0 r1 r2 r3 r4 r5) 2) (nb ri 2 r0 r1 r2 r3 r4 r5)) << 2
b3 = (nc (bt (gr ri r0 r1 r2 r3 r4 r5) 3) (nb ri 3 r0 r1 r2 r3 r4 r5)) << 3
b4 = (nc (bt (gr ri r0 r1 r2 r3 r4 r5) 4) (nb ri 4 r0 r1 r2 r3 r4 r5)) << 4
b5 = (nc (bt (gr ri r0 r1 r2 r3 r4 r5) 5) (nb ri 5 r0 r1 r2 r3 r4 r5)) << 5
return b0 + b1 + b2 + b3 + b4 + b5
end
// A live cell is green, a dead one is dark grey
fn cell -> row c:
if bt row c:
write "\e[38;5;46m#"
else:
write "\e[38;5;238m."
end
end
fn rr -> row:
write "   "
cell row 0
cell row 1
cell row 2
cell row 3
cell row 4
cell row 5
print "\e[0m\e[K"
end

// B1 on PC13 reads high while held. Swap this for `return 0` to run
// the animation on a board with no button.
fn btn -> x:
return gpioRead "gpioc" 13
end

// One int carries both flags: paused in bit 1, last button level in bit 0.
// Only a fresh press, low then high, flips paused.
fn nextS -> s b:
p = (s >> 1) & 1
prev = s & 1
np = p ^ ((b == 1) & (prev == 0))
return (np << 1) | b
end

// Spend the frame in short naps so a quick press is never missed
ts fn poll -> n s:
if n == 0:
return s
end
sleep 20
return poll (n - 1) (nextS s (btn 0))
end

// While paused the board holds still and the counter stops
fn nx -> p cur nxt:
if p:
return cur
end
return nxt
end
fn inc -> p g:
if p:
return g
end
return g + 1
end
fn stat -> p:
if p:
write "   PAUSED"
end
end
ts fn st -> g s r0 r1 r2 r3 r4 r5:
write "\e[H"
print "\e[1;38;5;213m   Conway's Game of Life\e[0m\e[K"
print "\e[K"
rr r0
rr r1
rr r2
rr r3
rr r4
rr r5
print "\e[K"
p0 = (s >> 1) & 1
write "\e[38;5;220m   gen "
write g
stat p0
print "\e[0m\e[K"
ns = poll 20 s
p1 = (ns >> 1) & 1
n0 = cr 0 r0 r1 r2 r3 r4 r5
n1 = cr 1 r0 r1 r2 r3 r4 r5
n2 = cr 2 r0 r1 r2 r3 r4 r5
n3 = cr 3 r0 r1 r2 r3 r4 r5
n4 = cr 4 r0 r1 r2 r3 r4 r5
n5 = cr 5 r0 r1 r2 r3 r4 r5
q0 = nx p1 r0 n0
q1 = nx p1 r1 n1
q2 = nx p1 r2 n2
q3 = nx p1 r3 n3
q4 = nx p1 r4 n4
q5 = nx p1 r5 n5
return st (inc p1 g) ns q0 q1 q2 q3 q4 q5
end
ep fn main -> :
print "\e[2J\e[?25l"
return st 1 0 2 4 7 0 0 0
end
