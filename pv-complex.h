/* header file for pv-complex.c --
 * the core of phase vocoder with complex arithmetics
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
#ifndef	_PV_COMPLEX_H_
#define	_PV_COMPLEX_H_


// FFTW library
#include <fftw3.h>

// libsndfile
#include <sndfile.h>
#include "snd.h"

// ao device
#include <ao/ao.h>


struct pv_complex {
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

struct pv_complex *
pv_complex_init (long len, long hop_syn, int flag_window);

/* change rate and pitch (note that hop_syn is fixed)
 * INPUT
 *  pv : struct pv_complex
 *  rate  : rate of speed (1 == same speed, negative == backward)
 *  pitch : pitch-shift (0 == no-shift, +1(-1) = half-note up(down))
 * OUTPUT
 *  pv->hop_res : hop size in 1 step for the output
 *                == hop_syn with no pitch-shifting
 *  pv->hop_ana : hop size in 1 step for the input
 *                == hop_syn with no time-stretching (rate = 1).
 */
void
pv_complex_change_rate_pitch (struct pv_complex *pv,
			      double rate,
			      double pitch);

void
pv_complex_set_input (struct pv_complex *pv,
		      SNDFILE *sf, SF_INFO *sfinfo);
void
pv_complex_set_output_sf (struct pv_complex *pv,
			  SNDFILE *sf, SF_INFO *sfinfo);
void
pv_complex_set_output_ao (struct pv_complex *pv,
			  ao_device *ao);

void
pv_complex_free (struct pv_complex *pv);


long
read_and_FFT_stereo (struct pv_complex *pv,
		     long frame,
		     double *f_left, double *f_right);
/* the results are stored in out [i] for i = hop_syn to (hop_syn + len)
 * INPUT
 *  scale : for safety (give 0.5, for example)
 */
void
apply_invFFT_mono (struct pv_complex *pv,
		   const double *f, double scale,
		   double *out);
/* resample pv->[rl]_out[i] for i = 0 to pv->hop_syn
 *       to [left,right][i] for i = 0 to pv->hop_res
 * INPUT
 * OUTPUT
 */
void
pv_complex_resample (struct pv_complex *pv,
		     double *left, double *right);

/* play the segment of pv->[lr]_out[] for pv->hop_syn
 * pv->pitch_shift is taken into account, so that 
 * the output frames are pv->hop_res.
 * OUTPUT
 *  returned value : output frames (should be hop_res)
 */
int
pv_complex_play_resample (struct pv_complex *pv);


/* play one hop_syn by the phase vocoder:
 * phase vocoder by complex arithmetics with fixed hops.
 *   t_i - s_i = u_i - u_{i-1} = hop
 *   where s_i and t_i are the times for two analysis FFT
 *   and u_i is the time for the synthesis FFT at step i
 * Reference: M.Puckette (1995)
 * INPUT
 *  pv : struct pv_complex
 *  cur : current frame to play.
 *        you have to increment this by yourself.
 *  pv->flag_lock : 0 == no phase lock
 *                  1 == loose phase lock
 * OUTPUT (returned value)
 *  status : output frames (should be hop_res)
 */
long
pv_complex_play_step (struct pv_complex *pv,
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
