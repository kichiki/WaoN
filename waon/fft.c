/* FFT subroutine for WaoN with FFTW library
 * Copyright (C) 1998 Kengo ICHIKI (ichiki@geocities.com)
 * $Id: fft.c,v 1.1 2006/09/20 21:25:04 kichiki Exp $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <math.h>
#include <stdlib.h> /* realloc()  */
#include <stdio.h> /* fprintf()  */
#include <rfftw.h> /* FFTW library  */


/* Reference: "Numerical Recipes in C" 2nd Ed.
 * by W.H.Press, S.A.Teukolsky, W.T.Vetterling, B.P.Flannery
 * (1992) Cambridge University Press.
 * ISBN 0-521-43108-5
 * Sec.13.4 - Data Windowing
 */
double
parzen (int i, int nn)
{
  return (1.0 - fabs (((double)i-0.5*(double)(nn-1))
		      /(0.5*(double)(nn+1))));
}

double
welch (int i, int nn)
{
  return (1.0-(((double)i-0.5*(double)(nn-1))
	       /(0.5*(double)(nn+1)))
	  *(((double)i-0.5*(double)(nn-1))
	    /(0.5*(double)(nn+1))));
}

double
hanning (int i, int nn)
{
  return ( 0.5 * (1.0 - cos (2.0*M_PI*(double)i/(double)(nn-1))) );
}

/* Reference: "Digital Filters and Signal Processing" 2nd Ed.
 * by L. B. Jackson. (1989) Kluwer Academic Publishers.
 * ISBN 0-89838-276-9
 * Sec.7.3 - Windows in Spectrum Analysis
 */
double
hamming (int i, int nn)
{
  return ( 0.54 - 0.46 * cos (2.0*M_PI*(double)i/(double)(nn-1)) );
}

double
blackman (int i, int nn)
{
  return ( 0.42 - 0.5 * cos (2.0*M_PI*(double)i/(double)(nn-1))
	  + 0.08 * cos (4.0*M_PI*(double)i/(double)(nn-1)) );
}

double
steeper (int i, int nn)
{
  return ( 0.375
	  - 0.5 * cos (2.0*M_PI*(double)i/(double)(nn-1))
	  + 0.125 * cos (4.0*M_PI*(double)i/(double)(nn-1)) );
}

/* calc power spectrum of real data x[n]
 * INPUT
 *  n : # of data in x
 *  x[] : data
 *  den : weight of window function; calculated by init_den().
 *  winflg : 0 no window
 *           1 parzen window
 *           2 welch window
 *           3 hanning window
 * OUTPUT
 *  p[] : stored only n/2 data
 */
void
power_spectrum_fftw (int n, double x[], double p[], double den,
		     char winflg, rfftw_plan plan)
{
  static double maxamp = 2147483647.0; /* 2^32-1  */
  static fftw_real *out = NULL;
  int i;

  /* allocate work area at the first time  */
  if (out == NULL)
    {
      out = (double *)realloc(out, sizeof(double)*n);
      if (out == NULL)
	{
	  fprintf(stderr, "cannot allocate memory for FFT (%d)\n", n);
	  exit (1);
	}
    }

  /* window */
  for (i=0; i<n; i++)
    {
      if (winflg == 0) /* square window  */
	x[i] = x[i] / maxamp;
      else if (winflg == 1) /* Parzen window  */
	x[i] = parzen (i, n) * x[i] / maxamp;
      else if (winflg == 2) /* Welch window  */
	x[i] = welch (i, n) * x[i] / maxamp;
      else if (winflg == 3) /* Hanning window  */
	x[i] = hanning (i, n) * x[i] / maxamp;
      else if (winflg == 4) /* Hamming window  */
	x[i] = hamming (i, n) * x[i] / maxamp;
      else if (winflg == 5) /* Blackman window  */
	x[i] = blackman (i, n) * x[i] / maxamp;
      else if (winflg == 6) /* steeper window  */
	x[i] = steeper (i, n) * x[i] / maxamp;
    }

  rfftw_one (plan, x, out);

  p[0] = out[0]*out[0]/den;  /* DC component */
  for (i=1; i < (n+1)/2; ++i)  /* (i < n/2 rounded up) */
    p[i] = (out[i]*out[i] + out[n-i]*out[n-i])/den;
  if (n % 2 == 0) /* n is even */
    p[n/2] = out[n/2]*out[n/2]/den;  /* Nyquist freq. */
}

/* prepare window for FFT
 * INPUT
 *  n : # of samples for FFT
 *  winflg : flag for window function (see above)
 * OUTPUT
 *  density factor as RETURN VALUE
 */
double
init_den (int n, char winflg)
{
  double den;
  int i;

  den = 0.0;
  for (i=0; i<n; i++)
    {
      if (winflg==0)
	den += 1.0;
      else if (winflg == 1)
	den += parzen (i, n) * parzen (i, n);
      else if (winflg == 2)
	den += welch (i, n) * welch (i, n);
      else if (winflg == 3)
	den += hanning (i, n) * hanning (i, n);
      else if (winflg == 4)
	den += hamming (i, n) * hamming (i, n);
      else if (winflg == 5)
	den += blackman (i, n) * blackman (i, n);
      else if (winflg == 6)
	den += steeper (i, n) * steeper (i, n);
    }
  den *= (double)n;

  return den;
}
