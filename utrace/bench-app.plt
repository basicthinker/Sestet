set terminal postscript eps enhanced font 24
set size 1,1
set output "bench-app.eps"
set style data histograms
set style fill pattern 2
set style histogram cluster gap 1.2
set ylabel 'Time (s)'
set yrange [0:25]
set y2label 'Energy (mJ)'
set y2range [0:2000]
#set key below box
plot 'bench-app.data' using 2:xtic(1) axes x1y1 title "Ext4-Time", \
    '' using 4:xtic(1) axes x1y1 title "AdaFS-Time", \
    '' using 3:xtic(1) axes x1y2 title "Ext4-Energy", \
    '' using 5:xtic(1) axes x1y2 title "AdaFS-Energy"
