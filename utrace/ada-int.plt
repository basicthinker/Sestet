set terminal postscript eps enhanced font 22
set size 1,0.6
set output "ada-int.eps"
set style data histograms
set style fill pattern 2
set style histogram cluster gap 1.5
set ylabel 'Time (s)'
set yrange [0:50]
set key left
plot 'ada-int.data' using 2:xtic(1) axes x1y1 title "Avg. Interval"
