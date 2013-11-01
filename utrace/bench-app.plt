set terminal postscript eps enhanced font 24
set size 1,0.7
set output "bench-app.eps"
set style data histograms
set style fill pattern 2
set style histogram cluster gap 2
set ylabel 'Time (s)'
set yrange [0:40]
set ytics nomirror
set y2label 'Energy (J)'
set y2range [0:27]
set y2tics
#set key below box
plot 'bench-app.data' using 2:xtic(1) axes x1y1 title "Browser-Time", \
    '' using 3:xtic(1) axes x1y2 title "Browser-Energy", \
    '' using 4:xtic(1) axes x1y1 title "Renren-Time", \
    '' using 5:xtic(1) axes x1y2 title "Renren-Energy"
