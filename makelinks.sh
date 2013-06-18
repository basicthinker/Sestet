#! /bin/bash

if [ $# -ne 1 ]; then
	echo "Please enter target dir."
	exit -1
fi

lib_files=(hashtable.h list.h log.c log.h rffs_file.c rffs.h rlog.h sys.h)

for ((i=0;i<${#lib_files[*]};i=i+1))
do
	file=${lib_files[$i]}
	ln -sf ../$file $1/$file
done

cp policy.h $1/policy.h
