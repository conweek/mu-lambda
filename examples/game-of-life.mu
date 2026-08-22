// Conway's Game of Life on a 6x6 torus, one bitmask per row.
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
// A live cell is green, a dead one is dim grey
fn cell -> row c:
if bt row c:
write "\e[92m#"
else:
write "\e[90m."
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
ts fn st -> g r0 r1 r2 r3 r4 r5:
write "\e[H"
print "\e[1;96m   Conway's Game of Life\e[0m\e[K"
print "\e[K"
rr r0
rr r1
rr r2
rr r3
rr r4
rr r5
print "\e[K"
write "\e[93m   gen \e[0m"
write g
print "\e[K"
sleep 400
n0 = cr 0 r0 r1 r2 r3 r4 r5
n1 = cr 1 r0 r1 r2 r3 r4 r5
n2 = cr 2 r0 r1 r2 r3 r4 r5
n3 = cr 3 r0 r1 r2 r3 r4 r5
n4 = cr 4 r0 r1 r2 r3 r4 r5
n5 = cr 5 r0 r1 r2 r3 r4 r5
return st (g + 1) n0 n1 n2 n3 n4 n5
end
ep fn main -> :
print "\e[2J"
return st 1 2 4 7 0 0 0
end
