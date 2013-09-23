set terminal postscript eps enhanced font 24
set size 1.2,1
set output "bench-item.eps"
set style data histograms
set style fill pattern 1
set style histogram cluster gap 1
set ylabel 'Throughput (MB/s)'
set yrange [0:7]
set y2label 'Trans. per sec.'
set y2rang [1:1200]
set logscale y2
plot 'bench-item.data' using 2:xtic(1) axes x1y1 title "Seq. W", \
    '' using 3:xtic(1) axes x1y1 title "Seq. R", \
    '' using 4:xtic(1) axes x1y2 title "Rand. W (TPS)", \
    '' using 5:xtic(1) axes x1y2 title "Rand. R (TPS)", \
    '' using 6:xtic(1) axes x1y2 title "Insert (TPS)", \
    '' using 7:xtic(1) axes x1y2 title "Update (TPS)", \
    '' using 8:xtic(1) axes x1y2 title "Delete (TPS)"
