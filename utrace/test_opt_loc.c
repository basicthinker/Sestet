#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "policy_util.h"

#define likely(x)	__builtin_expect((x),1)

#define LEN_BITS 3

// Ported from GSL for verification
int
gsl_fit_linear (const double *x, const size_t xstride,
                const double *y, const size_t ystride,
                const size_t n,
                double *c0, double *c1);

int main(int argc, char *argv[]) {
  double min_stal, max_stal, thr_s;
  int thr_n, bits, len;
  double time, stal, merg, ratio;
  double loc, opt;
  struct flex_curve_history fh = FLEX_HISTORY_INIT(LEN_BITS,
      FLEX_POINT_ZERO, FLEX_LINE_STATE_ZERO);

  if (argc != 6) {
    fprintf(stderr, "Usage: %s LogFile MinStal MaxStal SkipPeaks SlopeThreshold\n",
        argv[0]);
    return -1;
  }

  freopen(argv[1], "r", stdin);

  min_stal = atof(argv[2]);
  max_stal = atof(argv[3]);
  thr_n = atoi(argv[4]);
  thr_s = atof(argv[5]);

  loc = 0.0;
  while(scanf("%lf\t%lf\t%lf\t%d\t%lf", 
      &time, &stal, &merg, &len, &ratio) == 5) {
    flex_point p = { stal, ratio };
    double c0, c1;

    fh_update_curve(&fh, &p);
    gsl_fit_linear((double *)fh.array, 2, (double *)fh.array + 1, 2,
        fh.mask + 1, &c0, &c1);
    if (fabs(c1 - fh_state(&fh).slope) > 0.0000001)
      fprintf(stderr, "Mismatch at %lu: GSL - %f \t FlexFS - %f\n",
          fh.seq, c1, fh_state(&fh).slope); 
    printf("%f\t%f\t%f\t%d\t%f\t%f\n", time, stal, merg, len, ratio,
        fh_state(&fh).slope);

    if (loc == 0.0) {
      if (stal < min_stal) continue;
      if (stal > max_stal) {
        loc = stal;
        opt = ratio;
        continue;
      }
      if (c1 > thr_s) continue;
      if (thr_n) --thr_n;
      else {
        loc = stal;
        opt = ratio;
      }
    }
  } // while

  if (loc == 0.0) {
    loc = stal;
    opt = ratio;
  }

  {
    char fn[127];
    sprintf(fn, "%s.loc", argv[1]);
    freopen(fn, "w", stdout);
    printf("%f\t%f\n", loc, opt);
  }
  return 0;
}

// Ported from GSL for verification
int
gsl_fit_linear (const double *x, const size_t xstride,
                const double *y, const size_t ystride,
                const size_t n,
                double *c0, double *c1)
{
  double m_x = 0, m_y = 0, m_dx2 = 0, m_dxdy = 0;

  size_t i;

  for (i = 0; i < n; i++)
    {
      m_x += (x[i * xstride] - m_x) / (i + 1.0);
      m_y += (y[i * ystride] - m_y) / (i + 1.0);
    }

  for (i = 0; i < n; i++)
    {
      const double dx = x[i * xstride] - m_x;
      const double dy = y[i * ystride] - m_y;

      m_dx2 += (dx * dx - m_dx2) / (i + 1.0);
      m_dxdy += (dx * dy - m_dxdy) / (i + 1.0);
    }

  /* In terms of y = a + b x */

  {
    double s2 = 0, d2 = 0;
    double b = m_dxdy / m_dx2;
    double a = m_y - m_x * b;

    *c0 = a;
    *c1 = b;

  }

  return 0;
}
