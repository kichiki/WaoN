/*
 * Copyright (C) 1998 Kengo ICHIKI (ichiki@geocities.com)
 * $Id: fft.h,v 1.1 2006/09/20 21:26:44 kichiki Exp $
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

double parzen (int i, int nn);
double welch (int i, int nn);
double hanning (int i, int nn);
double hamming (int i, int nn);
double blackman (int i, int nn);
double steeper (int i, int nn);

void power_spectrum_fftw (int n, double x[], double p[], double den,
			  char winflg, rfftw_plan plan);
double init_den (int n, char winflg);
