/* header file for fft.c --
 * FFT subroutine for WaoN with FFTW library
 * Copyright (C) 1998-2007 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: fft.h,v 1.5 2007/02/09 06:01:04 kichiki Exp $
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
#ifndef	_FFT_H_
#define	_FFT_H_


double parzen (int i, int nn);
double welch (int i, int nn);
double hanning (int i, int nn);
double hamming (int i, int nn);
double blackman (int i, int nn);
double steeper (int i, int nn);

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
	   double *out);
void
fprint_window_name (FILE *out, int flag_window);

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
init_den (int n, char flag_window);



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
#ifdef FFTW2
#include <rfftw.h>
void
power_spectrum_fftw (int n, double *x, double *y, double *p,
		     double den,
		     char flag_window,
		     rfftw_plan plan)
#else // FFTW3
void
power_spectrum_fftw (int n, double *x, double *y, double *p,
		     double den,
		     char flag_window,
		     fftw_plan plan);
#endif // !FFTW2


#endif /* !_FFT_H_ */
