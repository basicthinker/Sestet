set terminal postscript eps enhanced font 24
set size 1,1
set output "trace-data/int-pred-confl.eps"
set style data histograms
set style fill pattern 1
set style histogram cluster gap 1
set ylabel 'Number of Conflicts'
set yrange [0:160]
set key below box
plot 'trace-data/app-int-stat.data' using 2:xtic(1) axes x1y1 title "Last-Min", \
    '' using 4:xtic(1) axes x1y1 title "Last-Avg", \
    '' using 6:xtic(1) axes x1y1 title "State-machine"
