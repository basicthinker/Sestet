set terminal postscript eps enhanced font 24
set size 1.2,1
set output "bench-unif.eps"
set style data histograms
set style fill pattern 2
set style histogram cluster gap 1
set ylabel 'Score'
set yrange [0:2]
plot 'bench-unif.data' using 2:xtic(1) axes x1y1 title "Ext4 Perform.", \
    '' using 3:xtic(1) axes x1y1 title "AdaFS Perform.", \
    '' using 4:xtic(1) axes x1y1 title "Ext4 Energy", \
    '' using 5:xtic(1) axes x1y1 title "AdaFS Energy"
