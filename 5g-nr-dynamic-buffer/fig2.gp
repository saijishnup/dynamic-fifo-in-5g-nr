N = " pfifo codel pie fqcodel fqpie cobalt fifo20 fifo83 fifo200 dyn fsd fsdl "
C = "#bcbd22 #1f77b4 #17becf #9467bd #2ca02c #8c564b #b0b0b0 #707070 #303030 #ff7f0e #d62728 #e377c2"
qc(q) = word(C, words(substr(N, 1, strstrt(N, " ".q." "))) + 1) # fixed colour per config, same in every figure
if (!exists("tt")) tt = "AQM comparison (mean of 3 seeds)"
if (!exists("qs")) qs="pfifo codel pie fqcodel fqpie cobalt"
set terminal pngcairo size 1100,900 font ",10"
do for [q in qs] {
  d=sprintf("results/avg/%s/",q)
  set output sprintf("results/avg/fig-%s.png",q)
  set multiplot layout 3,1 title sprintf("%s (mean of 3 seeds, 10 s bins)",q)
  set xlabel "time (s)"; set key outside right
  set ylabel "goodput (Mbps)"
  plot for [i=2:8] d."goodput.dat" u 1:i w l lw 1.5 t word("UDPburst TCPbulk FTP HTTP VoIP Gaming Video",i-1)
  unset key
  set ylabel "queueing delay (ms)"
  plot d."qdelay.dat" u 1:2 w l lw 1.5 lc rgb "red"
  set ylabel "drops per s"
  plot d."drops.dat" u 1:2 w l lw 1.5 lc rgb "blue"
  unset multiplot
}
set terminal pngcairo size 1300,1000 font ",10"
set output "results/avg/".cmp
set multiplot layout 3,2 title tt
set xlabel "time (s)"; set key top right
set logscale y; set ylabel "queueing delay (ms)"
plot for [q in qs] "results/avg/".q."/qdelay.dat" u 1:2 w l lw 1.5 lc rgb qc(q) t q
unset logscale y; set ylabel "total goodput (Mbps)"
plot for [q in qs] "results/avg/".q."/goodput.dat" u 1:($2+$3+$4+$5+$6+$7+$8) w l lw 1.5 lc rgb qc(q) t q
unset key; set xlabel ""
set style data histograms; set style histogram errorbars gap 1 lw 1
set style fill solid 0.6; set boxwidth 0.8
set logscale y; set ylabel "mean delay (ms)"
plot "results/avg/stats.dat" u 2:3:xtic(1)
unset logscale y; set ylabel "goodput (Mbps)"
plot "results/avg/stats.dat" u 4:5:xtic(1)
set ylabel "Jain fairness"
plot "results/avg/stats.dat" u 6:7:xtic(1)
set ylabel "total drops"
plot "results/avg/stats.dat" u 8:9:xtic(1)
unset multiplot
