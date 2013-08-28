import sys
import string

if len(sys.argv) != 2:
  print "Usage: python %s EventTraceFile" % sys.argv[0]
  sys.exit(1)

ev_file = open(sys.argv[1], 'r')

pre_time = -1.0
for line in ev_file:
  segs = string.split(line, '\t')
  cur_time = float(segs[1])
  if pre_time > 0 and cur_time - pre_time > 0.1:
    print "%f" % (cur_time - pre_time)
  pre_time = cur_time

ev_file.close()

