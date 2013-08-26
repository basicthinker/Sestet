import sys
import string

if len(sys.argv) != 3:
  print "Usage: python %s KernTraceFile EventTraceFile" % sys.argv[0]
  sys.exit(1)

kern_file = open(sys.argv[1], 'r')
ev_file = open(sys.argv[2], 'r')

line = kern_file.readline()
while line.find("begin time") < 0:
  line = kern_file.readline()
begin_time = float(line[line.index('[') + 1:line.index(']')])

pre_time = -1.0
for line in ev_file:
  segs = string.split(line, '\t')
  cur_time = float(segs[1]) - begin_time
  if pre_time > 0 and cur_time - pre_time > 0.1:
    print "%f" % pre_time
  pre_time = cur_time
print "%f" % pre_time
kern_file.close()
ev_file.close()
