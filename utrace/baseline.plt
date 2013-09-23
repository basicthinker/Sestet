set terminal postscript eps enhanced font 24
set size 1.5,1
set output "baseline.eps"
set style data histograms
set style fill pattern 2
set style histogram cluster gap 1.2
set ylabel 'Time (s)'
set yrange [0:20]
set y2label 'Energy (mJ)'
set y2range [0:2000]
#set key below box
plot 'baseline.data' using 2:xtic(1) axes x1y1 title "Time (s)", \
    '' using 3:xtic(1) axes x1y2 title "Energy (mJ)"
