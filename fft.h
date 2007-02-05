/* header file for fft.c --
 * FFT subroutine for WaoN with FFTW library
 * Copyright (C) 1998-2007 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: fft.h,v 1.4 2007/02/05 05:39:31 kichiki Exp $
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

#ifdef FFTW2
#include <rfftw.h>
void
power_spectrum_fftw (int n, double *x, double *y, double *p,
		     double den,
		     char winflg,
		     rfftw_plan plan)
#else // FFTW3
void
power_spectrum_fftw (int n, double *x, double *y, double *p,
		     double den,
		     char winflg,
		     fftw_plan plan);
#endif // FFTW2

double init_den (int n, char winflg);

#endif /* !_FFT_H_ */
