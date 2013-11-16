#include <asm-generic/errno-base.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <set>
#include <list>

#define PAGE_SIZE 4096

#define TE_TYPE_READ  'r'
#define TE_TYPE_WRITE 'w'
#define TE_TYPE_EVICT 'e'

#define TE_HIT_YES     'y'
#define TE_HIT_NO      'n'
#define TE_HIT_UNKNOWN 'u'

using namespace std;

struct te_page {
  char te_type;
  char te_hit;
  unsigned long te_ino;
  unsigned long te_pgi;
};

inline double tv_float(const struct timeval &tv) {
  return tv.tv_sec + (double)tv.tv_usec / 1000000;
}

typedef pair<unsigned long, unsigned long> ipage_t;
typedef pair<unsigned long, double> rpoint_t;

class RCurve {
public:
  RCurve() {
    stale = 0;
    merged = 0;
  }

  void input(char type, unsigned long ino, unsigned long pgi) {
    ipage_t pg(ino, pgi);
    double r = 0;
    switch (type) {
    case TE_TYPE_WRITE:
      if (log.find(pg) != log.end()) {
        merged += PAGE_SIZE;
      } else {
        log.insert(pg);
      }
      stale += PAGE_SIZE;

      r = (double)merged / stale;
      points.push_back(make_pair(stale, r));
      break;
    case TE_TYPE_EVICT:
      if (log.find(pg) != log.end()) {
        log.erase(pg);
        merged += PAGE_SIZE;
        points.back().second = (double)merged / stale;
      }
      break;
    defalt:
      printf("Warning: invalid trace entry type: %c\n", type);
    }
  }

  void clearLog() {
    merged = 0;
    log.clear();
  }

  list<rpoint_t> &getPoints() {
    return points;
  }

  size_t size() {
    return points.size();
  }

private:
  unsigned long stale;
  unsigned long merged;
  set<ipage_t> log;
  list<rpoint_t> points;
};

int main(int argc, const char *argv[]) {
  if (argc != 3) {
    printf("Usage: %s TraceFile IntervalSeconds\n", argv[0]);
    return -EINVAL;
  }

  const char *file_name = argv[1];
  const int int_sec = atoi(argv[2]);

  struct timeval tv;
  struct te_page page;
  ifstream file(file_name, ios::in | ios::binary);
  if (!file.read((char *)&tv, sizeof(tv)) ||
      !file.read((char *)&page, sizeof(page))) {
    printf("Error: failed to read in first trace entry.\n");
    return -EIO;
  }
  const double begin_time = tv_float(tv);
  double tran_time = int_sec;

  RCurve rc_ext4;
  RCurve rc_adafs;

  rc_ext4.input(page.te_type, page.te_ino, page.te_pgi);
  rc_adafs.input(page.te_type, page.te_ino, page.te_pgi);

  while (true) {
    if (!file.read((char *)&tv, sizeof(tv)) ||
        !file.read((char *)&page, sizeof(page)))
      break;

    if (tv_float(tv) - begin_time > tran_time) {
      rc_ext4.clearLog();
      tran_time += int_sec;
    }
    rc_ext4.input(page.te_type, page.te_ino, page.te_pgi);
    rc_adafs.input(page.te_type, page.te_ino, page.te_pgi);

  }

  if (rc_ext4.size() != rc_adafs.size()) {
    printf("Error: ext4 point number = %lu, adafs point number = %lu\n",
      rc_ext4.size(), rc_adafs.size());
    return -EFAULT;
  }

  list<rpoint_t>::iterator ie = rc_ext4.getPoints().begin();
  list<rpoint_t>::iterator ia = rc_adafs.getPoints().begin();
  
  return 0;
}

