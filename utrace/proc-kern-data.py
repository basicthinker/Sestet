import sys
import string
import re

if len(sys.argv) != 2:
  print "Usage: python %s KernelDataFile" % sys.argv[0]
  sys.exit(1)

log_file = open(sys.argv[1], 'r')

line = log_file.readline()
while line.find("[rffs] begin time") < 0:
  line = log_file.readline()
begin_time = float(line[line.index('[') + 1:line.index(']')])

for line in log_file:
  begin = line.find("staleness")
  if begin < 0:
    continue
  time=float(line[line.index('[') + 1:line.index(']')])
  segs = re.split("[,=\n]", line[begin:]);
  r = float(segs[3]) / float(segs[1])
  print "%.3f\t%.3f\t%.3f\t%s\t%.2f" % (time - begin_time, float(segs[1])/1024, float(segs[3])/1024, segs[5], r * 100)

log_file.close()

