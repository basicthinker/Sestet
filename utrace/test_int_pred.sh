#!/bin/bash

LOG_POST=".log"

if [ $# -ne 3 ]; then
  echo "Usage: $0 TraceDataDirectory MultiThreshold IntervalThreshold"
  exit 1
fi

trace_dir=$1
thr_m=$2
thr_int=$3

num_conflicts=0
num_pred=0
total_len=0
for ev_file in `ls $trace_dir/*-ev-*.log`
do
  file_pre=${ev_file%$LOG_POST}

  python proc-ev-data.py $ev_file > $file_pre
  ./test_int_pred.out $file_pre $thr_m $thr_int > $file_pre.int 2>/dev/null

  tmp=(`cat $file_pre.int`)
  num_conf=`expr $num_conf + ${tmp[1]}`
  num_pred=`expr $num_pred + ${tmp[2]}`
  total_len=`echo $total_len + ${tmp[3]} | bc -l`
  rm $file_pre
done

avg_len=`echo $total_len / $num_pred | bc -l`
echo "No. Conflicts = $num_conf, Total Len. = $total_len, No. Pred. = $num_pred, Avg. Len. = $avg_len"

