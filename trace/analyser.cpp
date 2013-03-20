#include <cstdio>
#include <set>
#include <map>
#include <vector>

//#define DEBUG_RAND
#define DEBUG_OVER 

using namespace std;

class entry {
  public:
    long unsigned ino;
    long long int begin;
    long long int end;
    bool operator <(const entry &other) const {
      return ino < other.ino || (ino == other.ino && begin < other.begin);
    }
};

int main(int argc, const char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "No file input specified.\n");
    return -1;
  }

  freopen(argv[1], "r", stdin);

  char line[1024];
  entry inst;
  vector<entry> trace;
  long long int space = 0;
  int len;
  while (fgets(line, sizeof(line), stdin) != NULL) {
    char *ptr = line;
    while (*ptr != '\t') ++ptr;
    sscanf(ptr, "\t%lu\t%lld\t%d", &inst.ino, &inst.begin, &len);
    inst.end = inst.begin + len;
    trace.push_back(inst);
    space += len;
  }

  const long unsigned cnt = trace.size();
  if (cnt < 2) {
    fprintf(stderr, "Too few entries.\n");
    return -2;
  }
  long unsigned rand_cnt = 0;
  for (int i = 1; i <= cnt; ++i) {
    const entry &cur = trace[i];
    const entry &pre = trace[i-1];
    if (cur.ino != pre.ino || 
        cur.begin > pre.end || cur.end < pre.begin) {
      ++rand_cnt;
#ifdef DEBUG_RAND
      fprintf(stderr, "pre -  %lu %lld %d\t\\\n", pre.ino, pre.begin, pre.end - pre.begin);
      fprintf(stderr, " - cur %lu %lld %d\n", cur.ino, cur.begin, cur.end - cur.begin);
#endif
    }
  }
  
  long unsigned merged_cnt = 0;
  long long int overlap = 0;
  map<long unsigned, set<entry> > merged;
  for (vector<entry>::iterator icur = trace.begin(); icur != trace.end(); ++icur) {
    map<long unsigned, set<entry> >::iterator is = merged.find(icur->ino);
    if (is == merged.end()) {
      set<entry> sorted;
      sorted.insert(*icur);
      merged[icur->ino] = sorted;
    } else {
      set<entry> &sorted = is->second;
      set<entry>::iterator inext = sorted.upper_bound(*icur);

      set<entry>::iterator iprev = inext;
      if (iprev != sorted.begin()) --iprev; // set ahead in case of deleting inext
      else iprev = sorted.end();

      while (inext != sorted.end()) {
        if (inext->end <= icur->end) {
          ++merged_cnt;
          overlap += inext->end - inext->begin;
#ifdef DEBUG_OVER
          fprintf(stderr, "cur -   %lu %lld %lld\n", icur->ino, icur->begin, icur->end);
          fprintf(stderr, " - full %lu %lld %lld\n", inext->ino, inext->begin, inext->end);
#endif
          sorted.erase(inext++);
        } else if (inext->begin < icur->end) {
          overlap += icur->end - inext->begin;
#ifdef DEBUG_OVER
          fprintf(stderr, "cur -   %lu %lld %lld\n", icur->ino, icur->begin, icur->end);
          fprintf(stderr, " - part %lu %lld %lld\n", inext->ino, inext->begin, inext->end);
#endif
          icur->end = inext->begin;
        } else break;
      }
      if (iprev != sorted.end()) {
        if (iprev->end > icur->begin) {
          if (iprev->begin == icur->begin) {
            ++merged_cnt;
            overlap += iprev->end - iprev->begin;
#ifdef DEBUG_OVER
            fprintf(stderr, "cur -   %lu %lld %lld\n", icur->ino, icur->begin, icur->end);
            fprintf(stderr, " - full %lu %lld %lld\n", iprev->ino, iprev->begin, iprev->end);
#endif
            sorted.erase(iprev);
          } else {
            overlap += iprev->end - icur->begin;
#ifdef DEBUG_OVER
            fprintf(stderr, "cur -   %lu %lld %lld\n", icur->ino, icur->begin, icur->end);
            fprintf(stderr, " - part %lu %lld %lld\n", iprev->ino, iprev->begin, iprev->end);
#endif
            icur->begin = iprev->end;
          }
        } // else no overlap
      }
      sorted.insert(*icur);
    }
  } // for trace iteration
  
  fprintf(stdout, "Total number & space: %lu\t%lld\n", cnt, space);
  fprintf(stdout, "Random writes: %lu\n", rand_cnt);
  fprintf(stdout, "Merged number & space: %lu\t%lld\n", merged_cnt, overlap);
 
  fclose(stdin);
  return 0;
}

