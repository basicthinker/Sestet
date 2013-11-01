set terminal postscript eps enhanced font 26
set size 1.4,0.8
set output "baseline.eps"
set style data histograms
set style fill pattern 3
set style histogram cluster gap 1
set ylabel 'Throughput (MB/s)'
set yrange [1:800]
set y2label 'Energy (mJ)'
set y2range [0:8000]
set ytics nomirror
set y2tics
set logscale y 2
plot 'baseline.data' using 2:xtic(1) axes x1y1 title "Throughput", \
    '' using 3:xtic(1) axes x1y2 title "Energy"
