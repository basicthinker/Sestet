set terminal postscript eps enhanced font 24
set size 1,1
set output "trace-data/int-pred-len.eps"
set style data histograms
set style fill pattern 1
set style histogram cluster gap 1
set ylabel 'Avg Len of Intervals (s)'
set key below box
plot 'trace-data/app-int-stat.data' using 3:xtic(1) axes x1y1 title "Last-Min", \
    '' using 5:xtic(1) axes x1y1 title "Last-Avg", \
    '' using 7:xtic(1) axes x1y1 title "State-machine"
