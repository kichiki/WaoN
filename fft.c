/* FFT subroutine for WaoN with FFTW library
 * Copyright (C) 1998-2007 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: fft.c,v 1.7 2007/02/28 08:33:56 kichiki Exp $
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

/* FFTW library  */
#ifdef FFTW2
#include <rfftw.h>
#else // FFTW3
#include <fftw3.h>
#endif // FFTW2

#include "hc.h" // HC_to_amp2()


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

/* apply window function to data[]
 * INPUT
 *  flag_window : 0 : no-window (default -- that is, other than 1 ~ 6)
 *                1 : parzen window
 *                2 : welch window
 *                3 : hanning window
 *                4 : hamming window
 *                5 : blackman window
 *                6 : steeper 30-dB/octave rolloff window
 */
void
windowing (int n, const double *data, int flag_window, double scale,
	   double *out)
{
  int i;
  for (i = 0; i < n; i ++)
    {
      switch (flag_window)
	{
	case 1: // parzen window
	  out [i] = data [i] * parzen (i, n) / scale;
	  break;

	case 2: // welch window
	  out [i] = data [i] * welch (i, n) / scale;
	  break;

	case 3: // hanning window
	  out [i] = data [i] * hanning (i, n) / scale;
	  break;

	case 4: // hamming window
	  out [i] = data [i] * hamming (i, n) / scale;
	  break;

	case 5: // blackman window
	  out [i] = data [i] * blackman (i, n) / scale;
	  break;

	case 6: // steeper 30-dB/octave rolloff window
	  out [i] = data [i] * steeper (i, n) / scale;
	  break;

	default:
	  fprintf (stderr, "invalid flag_window\n");
	case 0: // square (no window)
	  out [i] = data [i] / scale;
	  break;
	}
    }
}

void
fprint_window_name (FILE *out, int flag_window)
{
  switch (flag_window)
    {
    case 0: // square (no window)
      fprintf (stdout, "no window\n");
      break;

    case 1: // parzen window
      fprintf (stdout, "parzen window\n");
      break;

    case 2: // welch window
      fprintf (stdout, "welch window\n");
      break;

    case 3: // hanning window
      fprintf (stdout, "hanning window\n");
      break;

    case 4: // hamming window
      fprintf (stdout, "hamming window\n");
      break;

    case 5: // blackman window
      fprintf (stdout, "blackman window\n");
      break;

    case 6: // steeper 30-dB/octave rolloff window
      fprintf (stdout, "steeper 30-dB/octave rolloff window\n");
      break;

    default:
      fprintf (stdout, "invalid window\n");
      break;
    }
}

/* apply FFT with the window and return amplitude and phase
 * this is a wrapper mainly for phase vocoder process
 * INPUT
 *  len : FFT length
 *  data[len] : data to analyze
 *  flag_window : window type
 *  plan, in[len], out[len] : for FFTW3
 *  scale : amplitude scale factor
 * OUTPUT
 *  amp[len/2+1] : amplitude multiplied by the factor "scale" above
 *  phs[len/2+1] : phase
 */
void
apply_FFT (int len, const double *data, int flag_window,
	   fftw_plan plan, double *in, double *out,
	   double scale,
	   double *amp, double *phs)
{
  int i;

  windowing (len, data, flag_window, 1.0, in);
  fftw_execute (plan); // FFT: in[] -> out[]
  HC_to_polar (len, out, 0, amp, phs); // freq[] -> (amp, phs)

  // some scaling
  for (i = 0; i < (len/2)+1; i ++)
    {
      amp [i] /= scale;
    }
}


/* prepare window for FFT
 * INPUT
 *  n : # of samples for FFT
 *  flag_window : 0 : no-window (default -- that is, other than 1 ~ 6)
 *                1 : parzen window
 *                2 : welch window
 *                3 : hanning window
 *                4 : hamming window
 *                5 : blackman window
 *                6 : steeper 30-dB/octave rolloff window
 * OUTPUT
 *  density factor as RETURN VALUE
 */
double
init_den (int n, char flag_window)
{
  double den;
  int i;

  den = 0.0;
  for (i = 0; i < n; i ++)
    {
      switch (flag_window)
	{
	case 1: // parzen window
	  den += parzen (i, n) * parzen (i, n);
	  break;

	case 2: // welch window
	  den += welch (i, n) * welch (i, n);
	  break;

	case 3: // hanning window
	  den += hanning (i, n) * hanning (i, n);
	  break;

	case 4: // hamming window
	  den += hamming (i, n) * hamming (i, n);
	  break;

	case 5: // blackman window
	  den += blackman (i, n) * blackman (i, n);
	  break;

	case 6: // steeper 30-dB/octave rolloff window
	  den += steeper (i, n) * steeper (i, n);
	  break;

	default:
	  fprintf (stderr, "invalid flag_window\n");
	case 0: // square (no window)
	  den += 1.0;
	  break;
	}
    }

  den *= (double)n;

  return den;
}


/* calc power spectrum of real data x[n]
 * INPUT
 *  n : # of data in x
 *  x[] : data
 *  y[] : for output (you have to allocate before calling)
 *  den : weight of window function; calculated by init_den().
 *  flag_window : 0 : no-window (default -- that is, other than 1 ~ 6)
 *                1 : parzen window
 *                2 : welch window
 *                3 : hanning window
 *                4 : hamming window
 *                5 : blackman window
 *                6 : steeper 30-dB/octave rolloff window
 * OUTPUT
 *  y[] : fourier transform of x[]
 *  p[(n+1)/2] : stored only n/2 data
 */
void
power_spectrum_fftw (int n, double *x, double *y, double *p,
		     double den,
		     char flag_window,
#ifdef FFTW2
		     rfftw_plan plan)
#else // FFTW3
		     fftw_plan plan)
#endif // FFTW2
{
  static double maxamp = 2147483647.0; /* 2^32-1  */

  /* window */
  windowing (n, x, flag_window, maxamp, x);

/* FFTW library  */
#ifdef FFTW2
  rfftw_one (plan, x, y);
#else // FFTW3
  fftw_execute (plan); // x[] -> y[]
#endif

  HC_to_amp2 (n, y, den, p);
}


/* subtract average from the power spectrum
 * -- intend to remove non-tonal signal (such as drums, percussions)
 * INPUT
 *  n : FFT size
 *  p[(n+1)/2] : power spectrum
 *  m : number of bins to average out
 *  factor : factor * average is subtracted from the power
 *           (factor = 0.0) means no subtraction
 *           (factor = 1.0) means full subtraction of the average
 *           (factor = 2.0) means over subtraction
 * OUTPUT
 *  p[(n+1)/2] : subtracted power spectrum
 */
void
power_subtract_ave (int n, double *p, int m, double factor)
{
  int nlen = n/2+1;
  int i;
  int k;
  int nave;

  double *ave = NULL;
  ave = (double *) malloc (sizeof (double) * nlen);

  for (i = 0; i < nlen; i ++) // full span
    {
      ave [i] = 0.0;
      nave = 0;
      for (k = -m; k <= m; k ++)
	{
	  if ((i + k) < 0 || (i + k) >= nlen) continue;

	  ave [i] += p [i+k];
	  nave ++;
	}
      if (nave > 0) ave [i] /= (double)nave;
    }

  for (i = 0; i < nlen; i ++) // full span
    {
      p [i] = sqrt (p[i]) - factor * sqrt (ave [i]);
      if (p [i] < 0.0) p [i] = 0.0;
      else             p [i] = p [i] * p [i];
    }

  free (ave);
}

/* octave remover
 * INPUT
 *  n : FFT size
 *  p[(n+1)/2] : power spectrum
 *  factor : factor * average is subtracted from the power
 *           (factor = 0.0) means no subtraction
 *           (factor = 1.0) means full subtraction of the average
 *           (factor = 2.0) means over subtraction
 * OUTPUT
 *  p[(n+1)/2] : subtracted power spectrum
 */
void
power_subtract_octave (int n, double *p, double factor)
{
  int nlen = (n+1)/2;
  int i;
  int i2;

  double *oct = NULL;
  oct = (double *)calloc (n/2+1, sizeof (double));

  oct [0] = p [0];
  for (i = 1; i < nlen/2+1; i ++)
    {
      i2 = i * 2;
      if (i2 >= n/2+1) break;

      oct [i2]   = factor * p[i];
      if (i2-1 > 0)    oct [i2-1] = 0.5 * factor * p[i];
      if (i2+1 < nlen) oct [i2+1] = 0.5 * factor * p[i];
    }

  for (i = 0; i < nlen; i ++) // full span
    {
      p [i] = sqrt (p[i]) - factor * sqrt (oct [i]);
      if (p [i] < 0.0) p [i] = 0.0;
      else             p [i] = p [i] * p [i];
    }

  free (oct);
}
