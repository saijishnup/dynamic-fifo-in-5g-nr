# fig-tradeoff.png: mean delay vs throughput (3-seed means); fig-limit.png: Dyn / Dyn+FSD limit vs time (seed 1)
set terminal pngcairo size 900,600 font "Sans,11"
f = "results/avg/summary-avg.dat"
g(r) = "< grep -E '^(".r.") ' ".f
lab(s) = s eq "dyn" ? "Dyn" : s eq "fsd" ? "Dyn+FSD" : s eq "fsdl" ? "FSD-longest" : s
set output "results/avg/fig-tradeoff.png"
set logscale x
set xlabel "Mean queueing delay (ms, log scale)"
set ylabel "Total throughput (Mbps)"
set grid
set xrange [5:1500]
set offsets 0,0,0.15,0
set key bottom right
plot g("fifo20|fifo83|fifo200|pfifo")." | sort -k2 -n" u 2:3 w lp pt 7 lc rgb "#888888" t "Static FIFO (20, 83, 200 pkt, 1 GB)", \
     g("codel|pie|cobalt|fqcodel|fqpie") u 2:3 w p pt 5 ps 1.3 lc rgb "#4477aa" t "AQMs", \
     g("dyn|fsd") u 2:3 w p pt 9 ps 2 lc rgb "#cc3311" t "Proposed (Dyn, Dyn+FSD)", \
     g("fifo20|fifo83|fifo200|pie|fqcodel|fqpie") u 2:3:1 w labels offset 0,0.8 font ",9" notitle, \
     g("cobalt") u 2:3:1 w labels offset -2,0.8 font ",9" notitle, \
     g("codel") u 2:3:1 w labels offset 2,0.8 font ",9" notitle, \
     g("pfifo") u 2:3:("pfifo (1 GB)") w labels offset -2,0.8 font ",9" notitle, \
     g("dyn|fsd") u 2:3:(lab(strcol(1))) w labels offset 0,-1 font ",9" tc rgb "#cc3311" notitle
unset logscale x
set xrange [0:300]
set offsets 0,0,0,0
set output "results/avg/fig-limit.png"
set xlabel "Time (s)"
set ylabel "FIFO limit (packets)"
set yrange [0:220]
set key top right
do for [w in "50 150 250"] { set obj rect from w+0,graph 0 to w+50,graph 1 fc rgb "#eeeeee" fs solid noborder behind }
plot "results/dyn/s1/limit.dat" w steps lw 1.5 lc rgb "#cc3311" t "Dyn", \
     "results/fsd/s1/limit.dat" w steps lw 1.5 dt 2 lc rgb "#222222" t "Dyn+FSD (grey = UDP burst ON)"
