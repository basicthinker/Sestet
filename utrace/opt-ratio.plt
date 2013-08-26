set terminal postscript eps color
set output OUT_FILE

set ylabel 'Opt Ratio (%)'
set yrange [0:100]
set ytics nomirror
set y2range [-3:3]
set y2label 'Slope'
set y2tics nomirror
set xlabel 'Staleness (KB)'

set arrow from LOC_X,0 to LOC_X,100 nohead lc 2
plot IN_FILE using 2:5 title 'Opt. Ratio' axes x1y1 with linespoints pt 7 ps 0.5, \
    IN_FILE using 2:6 title 'Slope' axes x1y2 with lines lt 1 lc 3
