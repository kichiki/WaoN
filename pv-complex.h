/* header file for pv-complex.c --
 * the core of phase vocoder with complex arithmetics
 * Copyright (C) 2007 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: pv-complex.h,v 1.8 2007/10/15 06:12:11 kichiki Exp $
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
#ifndef	_PV_COMPLEX_H_
#define	_PV_COMPLEX_H_


// FFTW library
#include <fftw3.h>

// libsndfile
#include <sndfile.h>
#include "snd.h"

// ao device
#include <ao/ao.h>


struct pv_complex_data {
  // input (just reference purpose only)
  SNDFILE *sf;
  SF_INFO *sfinfo;

  // output (just reference purpose only)
  int flag_out; // 0 = ao, 1 = sf

  ao_device *ao;

  SNDFILE *sfout;
  SF_INFO *sfout_info;

  long len; // FFT length

  long hop_ana;
  long hop_syn;
  long hop_res;

  int flag_window;
  double window_scale;

  double *time;
  double *freq;
  fftw_plan plan;

  double *t_out;
  double *f_out;
  fftw_plan plan_inv;

  int flag_left;  // whether l_f_old[] is ready (1) or not (0)
  int flag_right; // whether r_f_old[] is ready (1) or not (0)

  double *l_f_old;
  double *r_f_old;

  double *l_out;
  double *r_out;

  int flag_lock; // 0 = no phase lock, 1 = loose phase lock
};


/** utility routines for struct pv_omplex_data **/

struct pv_complex_data *
pv_complex_init (long len, long hop_out, int flag_window);

/* change rate and pitch (note that hop_syn is fixed)
 * INPUT
 *  pv : struct pv_complex_data
 *  rate  : rate of speed (1 == same speed, negative == backward)
 *  pitch : pitch-shift (0 == no-shift, +1(-1) = half-note up(down))
 * OUTPUT
 *  pv->hop_res : 
 *  pv->hop_ana : 
 */
void
pv_complex_change_rate_pitch (struct pv_complex_data *pv,
			      double rate,
			      double pitch);

void
pv_complex_set_input (struct pv_complex_data *pv,
		      SNDFILE *sf, SF_INFO *sfinfo);
void
pv_complex_set_output_sf (struct pv_complex_data *pv,
			  SNDFILE *sf, SF_INFO *sfinfo);
void
pv_complex_set_output_ao (struct pv_complex_data *pv,
			  ao_device *ao);

void
pv_complex_free (struct pv_complex_data *pv);


/* play the segment of pv->[lr]_out[] for pv->hop_out
 * pv->pitch_shift is taken into account
 */
int
pv_complex_play_resample (struct pv_complex_data *pv);


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
 *  pv->flag_lock : 0 == no phase lock
 *                  1 == loose phase lock
 * OUTPUT (returned value)
 *  status : output frame.
 */
long pv_complex_play_step (struct pv_complex_data *pv,
			   long cur);


/** some wrapper routines **/

/* phase vocoder by complex arithmetics with fixed hops.
 *   t_i - s_i = u_i - u_{i-1} = hop
 *   where s_i and t_i are the times for two analysis FFT
 *   and u_i is the time for the synthesis FFT at step i
 * Reference: M.Puckette (1995)
 * INPUT
 *  flag_lock : 0 == no phase lock is applied
 *              1 == loose phase lock is applied
 *  rate : time-streching rate
 *  pitch_shift : in the unit of half-note
 */
void pv_complex (const char *file, const char *outfile,
		 double rate, double pitch_shift,
		 long len, long hop_syn,
		 int flag_window,
		 int flag_lock);


#endif /* !_PV_COMPLEX_H_ */
