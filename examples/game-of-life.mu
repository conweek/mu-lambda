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
fn rr -> v:
if v==0:
print "......"
end
if v==1:
print "#....."
end
if v==2:
print ".#...."
end
if v==3:
print "##...."
end
if v==4:
print "..#..."
end
if v==5:
print "#.#..."
end
if v==6:
print ".##..."
end
if v==7:
print "###..."
end
if v==8:
print "...#.."
end
if v==9:
print "#..#.."
end
if v==10:
print ".#.#.."
end
if v==11:
print "##.#.."
end
if v==12:
print "..##.."
end
if v==13:
print "#.##.."
end
if v==14:
print ".###.."
end
if v==15:
print "####.."
end
if v==16:
print "....#."
end
if v==17:
print "#...#."
end
if v==18:
print ".#..#."
end
if v==19:
print "##..#."
end
if v==20:
print "..#.#."
end
if v==21:
print "#.#.#."
end
if v==22:
print ".##.#."
end
if v==23:
print "###.#."
end
if v==24:
print "...##."
end
if v==25:
print "#..##."
end
if v==26:
print ".#.##."
end
if v==27:
print "##.##."
end
if v==28:
print "..###."
end
if v==29:
print "#.###."
end
if v==30:
print ".####."
end
if v==31:
print "#####."
end
if v==32:
print ".....#"
end
if v==33:
print "#....#"
end
if v==34:
print ".#...#"
end
if v==35:
print "##...#"
end
if v==36:
print "..#..#"
end
if v==37:
print "#.#..#"
end
if v==38:
print ".##..#"
end
if v==39:
print "###..#"
end
if v==40:
print "...#.#"
end
if v==41:
print "#..#.#"
end
if v==42:
print ".#.#.#"
end
if v==43:
print "##.#.#"
end
if v==44:
print "..##.#"
end
if v==45:
print "#.##.#"
end
if v==46:
print ".###.#"
end
if v==47:
print "####.#"
end
if v==48:
print "....##"
end
if v==49:
print "#...##"
end
if v==50:
print ".#..##"
end
if v==51:
print "##..##"
end
if v==52:
print "..#.##"
end
if v==53:
print "#.#.##"
end
if v==54:
print ".##.##"
end
if v==55:
print "###.##"
end
if v==56:
print "...###"
end
if v==57:
print "#..###"
end
if v==58:
print ".#.###"
end
if v==59:
print "##.###"
end
if v==60:
print "..####"
end
if v==61:
print "#.####"
end
if v==62:
print ".#####"
end
if v==63:
print "######"
end
end
ts fn st -> r0 r1 r2 r3 r4 r5:
rr r0
rr r1
rr r2
rr r3
rr r4
rr r5
print "------"
sleep 400
n0 = cr 0 r0 r1 r2 r3 r4 r5
n1 = cr 1 r0 r1 r2 r3 r4 r5
n2 = cr 2 r0 r1 r2 r3 r4 r5
n3 = cr 3 r0 r1 r2 r3 r4 r5
n4 = cr 4 r0 r1 r2 r3 r4 r5
n5 = cr 5 r0 r1 r2 r3 r4 r5
return st n0 n1 n2 n3 n4 n5
end
ep fn main -> :
return st 2 4 7 0 0 0
end
