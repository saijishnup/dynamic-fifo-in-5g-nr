#!/bin/bash
B=10
QS=${QS:-"pfifo codel pie fqcodel fqpie cobalt"}
: > results/avg-stats.tmp
for q in $QS; do
  o=results/avg/$q; mkdir -p $o
  for f in goodput qdelay drops; do
    cat results/$q/s*/$f.dat | awk -v B=$B '{b=int(($1-1)/B);n[b]++;for(i=2;i<=NF;i++)s[b,i]+=$i;nf=NF;if(b>m)m=b}
    END{for(b=0;b<=m;b++){printf "%d",b*B+B/2;for(i=2;i<=nf;i++)printf " %g",s[b,i]/n[b];print ""}}' > $o/$f.dat
  done
  awk -v q=$q '$1==q && $3==300{for(c=4;c<=7;c++){s[c]+=$c;t[c]+=$c*$c};n++}
  END{printf "%s",q;for(c=4;c<=7;c++){m=s[c]/n;v=t[c]/n-m*m;printf " %g %g",m,(v>0?sqrt(v):0)};print ""}' results/summary.tsv >> results/avg-stats.tmp
done
mv results/avg-stats.tmp results/avg/stats.dat
gnuplot -e "qs='$QS'; cmp='${CMP:-fig-compare.png}'; tt='${TT:-AQM comparison (mean of 3 seeds)}'" fig2.gp
