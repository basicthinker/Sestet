#!/usr/bin/python
import os
import sys
import string

if len(sys.argv) != 2:
  print "Usage: %s DataDir" % sys.argv[0]
  sys.exit(-1)

dir_path = sys.argv[1]

for file_name in os.listdir(dir_path):
  data_file=open(dir_path + '/' + file_name, 'r')
  nlines = 0;
  time = 0.0
  for line in data_file:
    segs = string.split(line, '\t')
    time += float(segs[len(segs) - 1])
    nlines += 1
  if nlines == 0:
    continue
  print "%s\t%d\t%f" % (file_name, nlines, time)

