/* header file for gwaon-pv.c --
 * gWaoN -- gtk+ Spectra Analyzer : pv
 * Copyright (C) 2007 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: gwaon-pv.h,v 1.1 2007/02/17 04:58:36 kichiki Exp $
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
#ifndef	_GWAON_PV_H_
#define	_GWAON_PV_H_

// FFTW library
#include <fftw3.h>


struct pv_complex_data {
  // input (just reference purpose only)
  SNDFILE *sf;
  SF_INFO *sfinfo;

  // output (just reference purpose only)
  int flag_out; // 0 = esd, 1 = sf
  int esd;
  SNDFILE *sfout;
  SF_INFO *sfout_info;

  long len; // FFT length
  long hop_out;

  int flag_window;

  double *time;
  double *freq;
  fftw_plan plan;

  double *t_out;
  double *f_out;
  fftw_plan plan_inv;

  int flag_run; // 0 = stop, 1 = running

  double *l_f_out_old;
  double *r_f_out_old;

  double *l_out;
  double *r_out;
};


struct pv_complex_data *
pv_complex_init (long len, long hop_out, int flag_window);

void
pv_complex_set_input (struct pv_complex_data *pv,
		      SNDFILE *sf, SF_INFO *sfinfo);
void
pv_complex_set_output_esd (struct pv_complex_data *pv,
			   int esd);
void
pv_complex_set_output_sf (struct pv_complex_data *pv,
			  SNDFILE *sf, SF_INFO *sfinfo);

void
pv_complex_free (struct pv_complex_data *pv);


/* play one hop_in by the phase vocoder:
 * phase vocoder by complex arithmetics with fixed hops.
 *   t_i - s_i = u_i - u_{i-1} = hop
 *   where s_i and t_i are the times for two analysis FFT
 *   and u_i is the time for the synthesis FFT at step i
 * Reference: M.Puckette (1995)
 * INPUT
 *  pv : struct pv_complex_data
 *  cur : current frame to play.
 *        you have to increment this by yourself.
 *  rate : time-stretch rate (larger is slower)
 * OUTPUT (returned value)
 *  status : output frame.
 */
long pv_complex_play_step (struct pv_complex_data *pv,
			   long cur, double rate);


#endif /* !_GWAON_PV_H_ */
