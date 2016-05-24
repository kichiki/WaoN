/* header file for hc.c --
 * half-complex format routines
 * Copyright (C) 2007-2013 Kengo Ichiki <kengoichiki@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#ifndef	_HC_H_
#define	_HC_H_


/* return angle (arg) of the complex number (freq(k),freq(len-k));
 * where (real,imag) = (cos(angle), sin(angle)).
 * INPUT
 *  len        :
 *  freq [len] :
 *  conj       : set 0 for normal case.
 *               set 1 for conjugate of the complex (freq(k),freq(len-k))
 *               that is, for (freq(k),-freq(len-k)).
 * OUTPUT
 *  amp [len/2+1] :
 *  phs [len/2+1] :
 */
void HC_to_polar (long len, const double * freq,
		  int conj,
		  double * amp, double * phs);

/* return angle (arg) of the complex number (freq(k),freq(len-k));
 * where (real,imag) = (cos(angle), sin(angle)).
 * INPUT
 *  len        :
 *  freq [len] :
 *  conj       : set 0 for normal case.
 *               set 1 for conjugate of the complex (freq(k),freq(len-k))
 *               that is, for (freq(k),-freq(len-k)).
 *  scale      : scale factor for amp2[]
 * OUTPUT
 *  amp2 [len/2+1] := (real^2 + imag^2) / scale
 *  phs  [len/2+1] := atan2 (+imag / real) for conj==0
 *                  = atan2 (-imag / real) for conj==1
 */
void HC_to_polar2 (long len, const double * freq,
		   int conj, double scale,
		   double * amp2, double * phs);

/* return power (amp2) of the complex number (freq(k),freq(len-k));
 * where (real,imag) = (cos(angle), sin(angle)).
 * INPUT
 *  len        :
 *  freq [len] :
 *  scale      : scale factor for amp2[]
 * OUTPUT
 *  amp2 [len/2+1] := (real^2 + imag^2) / scale
 */
void HC_to_amp2 (long len, const double * freq, double scale,
		 double * amp2);

/* 
 * INPUT
 *  len           : N
 *  amp [len/2+1] :
 *  phs [len/2+1] :
 * OUTPUT
 *  freq [len] :
 * NOTE
 *  if len == even,
 *  (freq(0)     ... freq(N/2)) = (r(0)          ...      r(N/2)) and
 *  (freq(N/2+1) ... freq(N)  ) = (     i(N/2-1) ... i(1)       )
 *     where Fourier coefficient Y(k) = r(k) + i i(k).
 *     note that Y(k) = Y*(N-k) and Y(0) and Y(N/2) are real.
 *  if len == odd,
 *  (freq(0)     ... freq(N/2)) = (r(0)     ... r(N/2)) and
 *  (freq(N/2+1) ... freq(N)  ) = (i(N/2-1) ... i(1)  )
 *     note in this case that Y(0) is real but Y(N/2) is not.
 *  in either case, number of elements for the coefficients are N/2+1.
 */
void polar_to_HC (long len, const double * amp, const double * phs,
		  int conj,
		  double * freq);

/* convert polar to HC with the scaling in freq domain
 * INPUT
 *  len           : N
 *  amp [len/2+1] :
 *  phs [len/2+1] :
 *  scale         : integer scale factor
 *                  bin k in the input is placed in scale*k in the output
 *                  except for k=0 and k=len/2 for even len.
 * OUTPUT
 *  freq [len*2] :
 */
void polar_to_HC_scale (long len, const double * amp, const double * phs,
			int conj, int scale,
			double * freq);

/* Z = X * Y, that is,
 * (rz + i iz) = (rx + i ix) * (ry + i iy)
 *             = (rx * ry - ix * iy) + i (rx * iy + ix * ry)
 */
void HC_mul (long len, const double *x, const double *y,
	     double *z);

/* Z = X / Y, that is,
 * (rz + i iz) = (rx + i ix) / (ry + i iy)
 *             = (rx + i ix) * (ry - i iy) / (ry*ry + iy*iy)
 *             = (rx*ry + ix*iy + i (ix*ry - rx*iy)) / (ry*ry + iy*iy)
 */
void HC_div (long len, const double *x, const double *y,
	     double *z);

void HC_abs (long len, const double *x,
	     double *z);

/* NOTE: y cannot be z!
 */
void HC_puckette_lock (long len, const double *y,
		       double *z);

/* Y[u_i] = X[t_i] (Y[u_{i-1}]/X[s_i]) / |Y[u_{i-1}]/X[s_i]|
 * Reference: M.Puckette (1995)
 * INPUT
 *  f_out_old[] : Y[u_{i-1}], synthesis-FFT at (i-1) step
 *  fs[]        : X[s_i], analysis-FFT at starting time of i step
 *  ft[]        : X[t_i], analysis-FFT at terminal time of i step
 *                Note: t_i - s_i = u_i - u_{i-1} = hop_out
 * OUTPUT
 *  f_out[]     : Y[u_i], synthesis-FFT at i step
 *                you can use the same point f_out_old[] for this.
 */
void
HC_complex_phase_vocoder (int len, const double *fs, const double *ft,
			  const double *f_out_old, 
			  double *f_out);


#endif /* !_HC_H_ */
