#! /bin/bash

if [ $# -ne 1 ]; then
	echo "Please enter target dir."
	exit -1
fi

lib_files=(ada_trace.h shashtable.h ada_log.c ada_log.h ada_file.c ada_fs.h ada_rlog.h ada_sys.h)

for ((i=0;i<${#lib_files[*]};i=i+1))
do
	file=${lib_files[$i]}
	ln -sf ../$file $1/$file
done

if [ ! -f $1/ada_policy.h ]; then
	cp ada_policy.h $1/ada_policy.h
fi
