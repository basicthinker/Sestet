#include <asm-generic/errno-base.h>

#include <stdint.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <set>
#include <list>

#define PAGE_SIZE 4096

#define TE_TYPE_READ  'r'
#define TE_TYPE_WRITE 'w'
#define TE_TYPE_FSYNC 's'
#define TE_TYPE_EVICT 'e'

#define TE_HIT_YES     'y'
#define TE_HIT_NO      'n'
#define TE_HIT_UNKNOWN 'u'

using namespace std;

struct timeval32 {
  uint32_t tv_sec;
  uint32_t tv_usec;
};

struct te_page {
  char te_type;
  char te_hit;
  uint32_t te_ino;
  uint32_t te_pgi;
};

inline double tv_float(const struct timeval32 &tv) {
  return tv.tv_sec + (double)tv.tv_usec / 1000000;
}

typedef pair<unsigned long, unsigned long> ipage_t;
typedef pair<unsigned long, double> rpoint_t;

class RCurve {
public:
  RCurve() {
    num_reads_hit = 0;
    num_reads_miss = 0;
    overall = 0;
    stale = 0;
    merged = 0;
  }

  void input(char type, unsigned long ino, unsigned long pgi, char hit) {
    ipage_t pg(ino, pgi);
    double r = 0;
    switch (type) {
    case TE_TYPE_READ:
      if (hit == TE_HIT_YES) ++num_reads_hit;
      else if (hit == TE_HIT_NO) ++num_reads_miss;
      else fprintf(stderr, "Warning: invalid hit status for read: %c\n", hit);
      break;
    case TE_TYPE_WRITE:
      if (log.find(pg) != log.end()) {
        merged += PAGE_SIZE;
        if (hit == TE_HIT_NO) {
          /*fprintf(stderr, "Warning: mismatch of hit: ino=%lu, pgi=%lu, "
              "hit=%c\n", ino, pgi, hit);*/
        }
      } else {
        log.insert(pg);
        if (hit == TE_HIT_YES) {
          /*fprintf(stderr, "Warning: mismatch of hit: ino=%lu, pgi=%lu, "
              "hit=%c\n", ino, pgi, hit);*/
        }
      }
      stale += PAGE_SIZE;

      overall += PAGE_SIZE;
      r = (double)merged / stale;
      points.push_back(make_pair(overall, r));
      break;
    case TE_TYPE_FSYNC:
      if (pgi != 0) {
        fprintf(stderr, "Warning: integrity check fails: fsync entry has non-zero page index.\n");
        return;
      }
      fsyncs.push_back(overall);
      break;
    case TE_TYPE_EVICT:
      if (log.find(pg) != log.end()) {
        log.erase(pg);
        merged += PAGE_SIZE;
        points.back().second = (double)merged / stale;
      }
      break;
    defalt:
      fprintf(stderr, "Warning: invalid trace entry type: %c\n", type);
    }
  }

  void clearLog() {
    stale = 0;
    merged = 0;
    log.clear();
  }

  list<rpoint_t> &getPoints() {
    return points;
  }

  list<unsigned long> &getFsyncs() {
    return fsyncs;
  }

  unsigned int numReadsHit() {
    return num_reads_hit;
  }

  unsigned int numReadsMiss() {
    return num_reads_miss;
  }

  size_t numWrites() {
    return points.size();
  }

private:
  unsigned int num_reads_hit;
  unsigned int num_reads_miss;
  unsigned long overall;
  unsigned long stale;
  unsigned long merged;
  set<ipage_t> log;
  list<rpoint_t> points;
  list<unsigned long> fsyncs;
};

int main(int argc, const char *argv[]) {
  if (argc != 5) {
    fprintf(stderr, "Usage: %s TraceFile IntervalSeconds CurvesFile FsyncsFile\n", argv[0]);
    return -EINVAL;
  }

  const char *trace_fname = argv[1];
  const int int_sec = atoi(argv[2]);
  const char *curves_fname = argv[3];
  const char *fsyncs_fname = argv[4];

  FILE *curves_fp = fopen(curves_fname, "w+");
  FILE *fsyncs_fp = fopen(fsyncs_fname, "w+");
  if (!curves_fp || !fsyncs_fp) {
    fprintf(stderr, "Error: failed to open output files: curves_fp=%p, fsyncs_fp=%p\n", curves_fp, fsyncs_fp);
    return -EIO;
  }

  struct timeval32 tv;
  struct te_page page;
  ifstream trace_file(trace_fname, ios::in | ios::binary);
  if (!trace_file.read((char *)&tv, sizeof(tv)) ||
      !trace_file.read((char *)&page, sizeof(page))) {
    fprintf(stderr, "Error: failed to read in first trace entry.\n");
    trace_file.close();
    return -EIO;
  }
  const double begin_time = tv_float(tv);
  double tran_time = int_sec;

  RCurve rc_ext4;
  RCurve rc_adafs;

  rc_ext4.input(page.te_type, page.te_ino, page.te_pgi, TE_HIT_UNKNOWN);
  rc_adafs.input(page.te_type, page.te_ino, page.te_pgi, page.te_hit);

  while (true) {
    if (!trace_file.read((char *)&tv, sizeof(tv)) ||
        !trace_file.read((char *)&page, sizeof(page)))
      break;

    if (tv_float(tv) - begin_time > tran_time) {
      rc_ext4.clearLog();
      tran_time += int_sec;
    }
    rc_ext4.input(page.te_type, page.te_ino, page.te_pgi, TE_HIT_UNKNOWN);
    rc_adafs.input(page.te_type, page.te_ino, page.te_pgi, page.te_hit);

  }
  trace_file.close();

  if (rc_ext4.numWrites() != rc_adafs.numWrites()) {
    fprintf(stderr, "Error: ext4 point number = %lu, adafs point number = %lu\n",
      rc_ext4.numWrites(), rc_adafs.numWrites());
    return -EFAULT;
  }

  /* Output results */
  list<rpoint_t>::iterator ie = rc_ext4.getPoints().begin();
  list<rpoint_t>::iterator ia = rc_adafs.getPoints().begin();

  for (; ie != rc_ext4.getPoints().end() && ia != rc_adafs.getPoints().end();
      ++ie, ++ia) {
    if (ie->first != ia->first) {
      fprintf(stderr, "Error: ext4 and AdaFS meet different staleness: "
          "%lu, %lu\n", ie->first, ia->first);
      return -EFAULT;
    }
    fprintf(curves_fp, "%f\t%f\t%f\n", (double)ie->first / 1024,
      ie->second * 100, ia->second * 100);
  }

  fprintf(stderr, "Ext4: number of hit reads = %u, number of miss reads = %u\n",
    rc_ext4.numReadsHit(), rc_ext4.numReadsMiss());
  fprintf(stderr, "AdaFS: number of hit reads = %u, number of miss reads = %u\n",
    rc_adafs.numReadsHit(), rc_adafs.numReadsMiss());

  if (rc_ext4.getFsyncs().size() != rc_adafs.getFsyncs().size()) {
    fprintf(stderr, "Error: Ext4 and AdaFS extracts different numbers of fsyncs: %lu, %lu\n", rc_ext4.getFsyncs().size(), rc_adafs.getFsyncs().size());
  } else {
    for (list<unsigned long>::iterator it = rc_ext4.getFsyncs().begin();
        it != rc_ext4.getFsyncs().end(); ++it) {
      fprintf(fsyncs_fp, "%lu\n", *it);
    }
  }

  fclose(curves_fp);
  fclose(fsyncs_fp);
  return 0;
}

