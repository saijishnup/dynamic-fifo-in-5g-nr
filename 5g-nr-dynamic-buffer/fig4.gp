# fig4.gp: paper-style figures (layout of Naik et al. Figs 3, 5-9) from existing data, no new runs.
# One run (seed sd, default 1) at 1 s bins, no averaging. Panels stacked: (a) goodput,
# (b) queueing delay, (c) drops (points), captions below each panel as in the paper.
# Usage: gnuplot fig4.gp      or      gnuplot -e "qs='codel fsd'; sd='2'" fig4.gp
if (!exists("qs")) qs = "pfifo codel pie cobalt fqcodel fqpie fifo20 fifo83 fifo200 dyn fsd"
if (!exists("sd")) sd = "1"
nm(q) = q eq "pfifo" ? "PFIFO Fast" : q eq "codel" ? "CoDel" : q eq "pie" ? "PIE" : \
        q eq "cobalt" ? "COBALT" : q eq "fqcodel" ? "FQ-CoDel" : q eq "fqpie" ? "FQ-PIE" : \
        q eq "dyn" ? "Dyn" : q eq "fsd" ? "Dyn+FSD" : q eq "fifo20" ? "FIFO-20" : \
        q eq "fifo83" ? "FIFO-83" : "FIFO-200"
fl(c) = c == 3 ? "TCP Bulk" : c == 4 ? "FTP" : c == 5 ? "HTTP" : c == 6 ? "VoIP" : \
        c == 7 ? "Gaming" : "Video"   # goodput.dat: col 2 UDP, cols 3..8 TCP Bulk..Video
if (!exists("rd")) rd = "results"
system("mkdir -p ".rd."/paper")
set terminal pngcairo noenhanced size 900,1000 font "Sans,10"
set xrange [0:300]
set xtics 50
set grid
set key top right font ",8"
set ylabel offset 1,0
do for [q in qs] {
    d = rd."/".q."/s".sd."/"
    set output rd."/paper/fig-".q."-paper.png"
    set multiplot
    # (a) goodput: paper legend order and colours (UDP last, black; no DASH)
    set origin 0,0.5
    set size 1,0.5
    set xlabel "Time (Seconds)\n\n(a) ".nm(q).": Goodput vs Time"
    set ylabel "Goodput (Mbps)"
    set yrange [0:14]
    plot for [c=3:8] d."goodput.dat" u 1:c w l lw 1.2 lt (c-2) t fl(c), \
         d."goodput.dat" u 1:2 w l lw 1.2 lc rgb "black" t "UDP"
    # (b) queueing delay
    set origin 0,0.25
    set size 1,0.25
    set xlabel "Time (Seconds)\n\n(b) ".nm(q).": Queuing Delay vs Time"
    set ylabel "Queueing Delay (ms)"
    set yrange [0:*]
    plot d."qdelay.dat" u 1:2 w l lw 1.2 lt 1 t nm(q)
    # (c) drops per second, drawn as points like the paper
    set origin 0,0
    set xlabel "Time\n\n(c) ".nm(q).": PacketDrop Count vs Time"
    set ylabel "Number of Packet Drops"
    plot d."drops.dat" u 1:2 w p pt 1 ps 0.8 lt 1 t "Packet Drops"
    unset multiplot
}
