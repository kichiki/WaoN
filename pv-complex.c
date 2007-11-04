/* the core of phase vocoder with complex arithmetics
 * Copyright (C) 2007 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: pv-complex.c,v 1.15 2007/11/04 23:57:06 kichiki Exp $
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
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "memory-check.h" // CHECK_MALLOC() macro

// FFTW library
#include <fftw3.h>
// half-complex format handling routines
#include "hc.h"
#include "fft.h" // windowing()

// libsndfile
#include <sndfile.h>
#include "snd.h"

// ao device
#include <ao/ao.h>
#include "ao-wrapper.h"

// samplerate
#include <samplerate.h>


#include "pv-conventional.h" // get_scale_factor_for_window()
#include "pv-complex.h"


/** utility routines for struct pv_omplex_data **/

struct pv_complex *
pv_complex_init (long len, long hop_syn, int flag_window)
{
  struct pv_complex *pv
    = (struct pv_complex *) malloc (sizeof (struct pv_complex));
  CHECK_MALLOC (pv, "pv_complex_init");

  pv->len = len;
  pv->hop_syn = hop_syn;

  pv->flag_window = flag_window;

  pv->window_scale = get_scale_factor_for_window (len, hop_syn, flag_window);

  pv->time = (double *)fftw_malloc (len * sizeof(double));
  pv->freq = (double *)fftw_malloc (len * sizeof(double));
  CHECK_MALLOC (pv->time, "pv_complex_init");
  CHECK_MALLOC (pv->freq, "pv_complex_init");
  pv->plan = fftw_plan_r2r_1d (len, pv->time, pv->freq,
			       FFTW_R2HC, FFTW_ESTIMATE);

  pv->f_out = (double *)fftw_malloc (len * sizeof(double));
  pv->t_out = (double *)fftw_malloc (len * sizeof(double));
  CHECK_MALLOC (pv->f_out, "pv_complex_init");
  CHECK_MALLOC (pv->t_out, "pv_complex_init");
  pv->plan_inv = fftw_plan_r2r_1d (len, pv->f_out, pv->t_out,
				   FFTW_HC2R, FFTW_ESTIMATE);

  pv->l_f_old = (double *)malloc (len * sizeof(double));
  pv->r_f_old = (double *)malloc (len * sizeof(double));
  CHECK_MALLOC (pv->l_f_old, "pv_complex_init");
  CHECK_MALLOC (pv->r_f_old, "pv_complex_init");

  pv->l_out = (double *) malloc ((hop_syn + len) * sizeof(double));
  pv->r_out = (double *) malloc ((hop_syn + len) * sizeof(double));
  CHECK_MALLOC (pv->l_out, "pv_complex_init");
  CHECK_MALLOC (pv->r_out, "pv_complex_init");
  int i;
  for (i = 0; i < (hop_syn + len); i ++)
    {
      pv->l_out [i] = 0.0;
      pv->r_out [i] = 0.0;
    }

  pv->flag_left  = 0; // l_f_old[] is not initialized yet
  pv->flag_right = 0; // r_f_old[] is not initialized yet

  pv->flag_lock = 0; // no phase lock (for default)

  //pv->pitch_shift = 0.0; // no pitch-shift

  return (pv);
}

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
			      double pitch)
{
  pv->hop_res = (long)((double)pv->hop_syn * pow (2.0, - pitch / 12.0));
  pv->hop_ana = (long)((double)pv->hop_res * rate);
}


void
pv_complex_set_input (struct pv_complex *pv,
		      SNDFILE *sf, SF_INFO *sfinfo)
{
  pv->sf = sf;
  pv->sfinfo = sfinfo;
}

void
pv_complex_set_output_ao (struct pv_complex *pv,
			  ao_device *ao)
{
  pv->flag_out = 0;
  pv->ao = ao;
}

void
pv_complex_set_output_sf (struct pv_complex *pv,
			  SNDFILE *sf, SF_INFO *sfinfo)
{
  pv->flag_out = 1;
  pv->sfout = sf;
  pv->sfout_info = sfinfo;
}

void
pv_complex_free (struct pv_complex *pv)
{
  if (pv == NULL) return;

  if (pv->time != NULL) free (pv->time);
  if (pv->freq != NULL) free (pv->freq);
  if (pv->plan != NULL) fftw_destroy_plan (pv->plan);

  if (pv->t_out != NULL) free (pv->t_out);
  if (pv->f_out != NULL) free (pv->f_out);
  if (pv->plan_inv != NULL) fftw_destroy_plan (pv->plan_inv);

  if (pv->l_f_old != NULL) free (pv->l_f_old);
  if (pv->r_f_old != NULL) free (pv->r_f_old);

  if (pv->l_out != NULL) free (pv->l_out);
  if (pv->r_out != NULL) free (pv->r_out);

  free (pv);
}


long
read_and_FFT_stereo (struct pv_complex *pv,
		     long frame,
		     double *f_left, double *f_right)
{
  static double *left  = NULL;
  static double *right = NULL;
  if (left == NULL)
    {
      left  = (double *)malloc (sizeof (double) * pv->len);
      right = (double *)malloc (sizeof (double) * pv->len);
      CHECK_MALLOC (left,  "read_and_FFT_stereo");
      CHECK_MALLOC (right, "read_and_FFT_stereo");
    }

  long status;
  status = sndfile_read_at (pv->sf, *(pv->sfinfo), frame,
			    left, right, pv->len);
  if (status != pv->len)
    {
      return (status);
    }

  // FFT for left channel
  windowing (pv->len, left, pv->flag_window, 1.0, pv->time);
  fftw_execute (pv->plan); // FFT: time[] -> freq[]
  int i;
  for (i = 0; i < pv->len; i ++)
    {
      f_left [i] = pv->freq [i];
    }

  // FFT for right channel
  windowing (pv->len, right, pv->flag_window, 1.0, pv->time);
  fftw_execute (pv->plan); // FFT: time[] -> freq[]
  for (i = 0; i < pv->len; i ++)
    {
      f_right [i] = pv->freq [i];
    }

  return (status);
}

/* the results are stored in out [i] for i = hop_syn to (hop_syn + len)
 * INPUT
 *  scale : for safety (give 0.5, for example)
 */
void
apply_invFFT_mono (struct pv_complex *pv,
		   const double *f, double scale,
		   double *out)
{
  int i;

  // scale
  for (i = 0; i < pv->len; i ++)
    {
      pv->f_out [i] = f [i];
    }
  fftw_execute (pv->plan_inv); // iFFT: f_out[] -> t_out[]
  // scale by len and windowing
  windowing (pv->len, pv->t_out, pv->flag_window, (double)pv->len * scale,
	     pv->t_out);
  // superimpose
  for (i = 0; i < pv->len; i ++)
    {
      out [pv->hop_syn + i] += pv->t_out [i];
    }
}

/*
 * OUTPUT
 *  returned value : 1 if x[i] = 0 for i = 0 to n-1
 *                   0 otherwise
 */
static int
check_zero (int n, const double *x)
{
  int i;

  for (i = 0; i < n; i ++)
    {
      if (x[i] != 0.0) return 1;
    }
  return 0;
}


/* resample pv->[rl]_out[i] for i = 0 to pv->hop_syn
 *       to [left,right][i] for i = 0 to pv->hop_res
 * INPUT
 * OUTPUT
 */
void
pv_complex_resample (struct pv_complex *pv,
		     double *left, double *right)
{
  // samplerate conversion
  SRC_DATA srdata;
  static float  *fl_in  = NULL;
  static float  *fl_out = NULL;
  static int hop_syn0 = 0;
  static int hop_res0 = 0;
  if (fl_in == NULL)
    {
      fl_in  = (float *)malloc (sizeof (float) * 2 * pv->hop_syn);
      fl_out = (float *)malloc (sizeof (float) * 2 * pv->hop_res);
      CHECK_MALLOC (fl_in,  "pv_complex_resample");
      CHECK_MALLOC (fl_out, "pv_complex_resample");
      hop_syn0 = pv->hop_syn;
      hop_res0 = pv->hop_res;
    }
  else
    {
      if (hop_syn0 < pv->hop_syn)
	{
	  fl_in = (float *)realloc (fl_in, sizeof (float) * 2 * pv->hop_syn);
	  CHECK_MALLOC (fl_in,  "pv_complex_resample");
	  hop_syn0 = pv->hop_syn;
	}
      if (hop_res0 < pv->hop_res)
	{
	  fl_out =
	    (float *)realloc (fl_out, sizeof (float) * 2 * pv->hop_res);
	  CHECK_MALLOC (fl_out, "pv_complex_resample");
	  hop_res0 = pv->hop_res;
	}
    }

  srdata.input_frames  = pv->hop_syn;
  srdata.output_frames = pv->hop_res;
  srdata.src_ratio = (double)(pv->hop_res) / (double)(pv->hop_syn);
  srdata.data_in  = fl_in;
  srdata.data_out = fl_out;

  // samplerate conversion (time fixed)
  int i;
  for (i = 0; i < pv->hop_syn; i ++)
    {
      fl_in [i*2 + 0] = (float)(pv->l_out [i]);
      fl_in [i*2 + 1] = (float)(pv->r_out [i]);
    }
  int status;
  //status = src_simple (&srdata, SRC_SINC_BEST_QUALITY, 2);
  status = src_simple (&srdata, SRC_SINC_FASTEST, 2);
  // this works better
  if (status != 0)
    {
      fprintf (stderr, "fail to samplerate conversion\n");
      exit (1);
    }

  for (i = 0; i < pv->hop_res; i ++)
    {
      left [i]  = (double)(fl_out [i*2 + 0]);
      right [i] = (double)(fl_out [i*2 + 1]);
    }
}

/* play l[n] and r[n] into ao or snd devices
 * INPUT
 * OUTPUT
 *  returned value : number of frames (should be equal to n)
 */
int
pv_complex_play (struct pv_complex *pv,
		 int n, double *l, double *r)
{
  int status = 0;
  if (pv->flag_out == 0)
    {
      status = ao_write (pv->ao, l, r, n);
      status /= 4; // 2 bytes for 2 channels
    }
  else if (pv->flag_out == 1)
    {
      status = sndfile_write (pv->sfout, *(pv->sfout_info),
			      l, r, n);
    }
  else
    {
      fprintf (stderr, "invalid output device\n");
    }

  return (status);
}

/* play the segment of pv->[lr]_out[] for pv->hop_syn
 * pv->pitch_shift is taken into account, so that 
 * the output frames are pv->hop_res.
 * OUTPUT
 *  returned value : output frames (should be hop_res)
 */
int
pv_complex_play_resample (struct pv_complex *pv)
{
  int status;

  if (pv->hop_syn != pv->hop_res)
    {
      // samplerate conversion
      static double *l_resamp = NULL;
      static double *r_resamp = NULL;
      static int hop_res0 = 0;
      if (l_resamp == NULL)
	{
	  l_resamp = (double *)malloc (sizeof (double) * pv->hop_res);
	  r_resamp = (double *)malloc (sizeof (double) * pv->hop_res);
	  CHECK_MALLOC (l_resamp, "pv_complex_play_resample");
	  CHECK_MALLOC (r_resamp, "pv_complex_play_resample");
	  hop_res0 = pv->hop_res;
	}
      else if (hop_res0 < pv->hop_res)
	{
	  l_resamp =
	    (double *)realloc (l_resamp, sizeof (double) * pv->hop_res);
	  r_resamp =
	    (double *)realloc (r_resamp, sizeof (double) * pv->hop_res);
	  CHECK_MALLOC (l_resamp, "pv_complex_play_resample");
	  CHECK_MALLOC (r_resamp, "pv_complex_play_resample");
	  hop_res0 = pv->hop_res;
	}

      pv_complex_resample (pv, l_resamp, r_resamp);
      status = pv_complex_play (pv, pv->hop_res, l_resamp, r_resamp);
    }
  else
    {
      status = pv_complex_play (pv, pv->hop_syn, pv->l_out, pv->r_out);
    }

  return (status);
}


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
		      long cur)
{
  static int len = 0;
  static double *l_fs  = NULL;
  static double *r_fs  = NULL;
  static double *l_ft  = NULL;
  static double *r_ft  = NULL;
  static double *l_tmp = NULL;
  static double *r_tmp = NULL;

  if (l_fs == NULL)
    {
      l_fs = (double *)malloc (pv->len * sizeof (double));
      r_fs = (double *)malloc (pv->len * sizeof (double));
      CHECK_MALLOC (l_fs, "pv_complex_play_step");
      CHECK_MALLOC (r_fs, "pv_complex_play_step");

      l_ft = (double *)malloc (pv->len * sizeof (double));
      r_ft = (double *)malloc (pv->len * sizeof (double));
      CHECK_MALLOC (l_ft, "pv_complex_play_step");
      CHECK_MALLOC (r_ft, "pv_complex_play_step");

      l_tmp = (double *)malloc (pv->len * sizeof (double));
      r_tmp = (double *)malloc (pv->len * sizeof (double));
      CHECK_MALLOC (l_tmp, "pv_complex_play_step");
      CHECK_MALLOC (r_tmp, "pv_complex_play_step");

      len = pv->len;
    }
  else if (len < pv->len)
    {
      l_fs = (double *)realloc (l_fs, pv->len * sizeof (double));
      r_fs = (double *)realloc (r_fs, pv->len * sizeof (double));
      CHECK_MALLOC (l_fs, "pv_complex_play_step");
      CHECK_MALLOC (r_fs, "pv_complex_play_step");

      l_ft = (double *)realloc (l_ft, pv->len * sizeof (double));
      r_ft = (double *)realloc (r_ft, pv->len * sizeof (double));
      CHECK_MALLOC (l_ft, "pv_complex_play_step");
      CHECK_MALLOC (r_ft, "pv_complex_play_step");

      l_tmp = (double *)realloc (l_tmp, pv->len * sizeof (double));
      r_tmp = (double *)realloc (r_tmp, pv->len * sizeof (double));
      CHECK_MALLOC (l_tmp, "pv_complex_play_step");
      CHECK_MALLOC (r_tmp, "pv_complex_play_step");

      len = pv->len;
    }

  long status;
  /* read starting data [cur, cur + len]
   * ==> FFT ==> fs[len] 
   */
  status = read_and_FFT_stereo (pv, cur, l_fs, r_fs);
  if (status != pv->len)
    {
      return 0; // no output
    }

  /* read terminal data [cur + hop_syn, cur + hop_syn + len]
   * ==> FFT ==> ft[len]
   */
  status = read_and_FFT_stereo (pv, cur + pv->hop_syn, l_ft, r_ft);
  if (status != pv->len)
    {
      return 0; // no output
    }

  // check zero
  int flag_left_cur;
  int flag_right_cur;
  if (check_zero (pv->len, l_fs) == 0 ||
      check_zero (pv->len, l_ft) == 0)
    {
      flag_left_cur = 0; // inactive
    }
  else
    {
      flag_left_cur = 1; // active
    }
  if (check_zero (pv->len, r_fs) == 0 ||
      check_zero (pv->len, r_ft) == 0)
    {
      flag_right_cur = 0; // inactive
    }
  else
    {
      flag_right_cur = 1; // active
    }


  /* phase vocoder process
   * fs[len] and ft[len] ==> superimposing out[hop_syn, hop_syn + len]
   */
  int i;
  // left channel
  if (flag_left_cur == 1)
    {
      // check l_f_old[]
      if (pv->flag_left == 0)
	{
	  if (pv->flag_lock == 0) // no phase lock
	    {
	      for (i = 0; i < pv->len; i ++)
		{
		  pv->l_f_old [i] = l_fs [i];
		}
	    }
	  else // loose phase lock
	    {
	      // apply loose phase lock
	      HC_puckette_lock (pv->len, l_fs, pv->l_f_old);
	    }

	  pv->flag_left = 1;
	}

      // generate the frame (out_0 + (n+1) * hop_syn), that is, "u_i"
      if (pv->flag_lock == 0) // no phase lock
	{
	  // Y[u_i] = X[t_i] (Y[u_{i-1}]/X[s_i]) / |Y[u_{i-1}]/X[s_i]|
	  HC_complex_phase_vocoder (pv->len, l_fs, l_ft, pv->l_f_old,
				    pv->l_f_old);
	  // already backed up for the next step in [lr]_f_old[]
	  apply_invFFT_mono (pv, pv->l_f_old, pv->window_scale, pv->l_out);
	}
      else // loose phase lock
	{
	  // Y[u_i] = X[t_i] (Z[u_{i-1}]/X[s_i]) / |Z[u_{i-1}]/X[s_i]|
	  HC_complex_phase_vocoder (pv->len, l_fs, l_ft, pv->l_f_old,
				    l_tmp);
	  // apply loose phase lock and store for the next step
	  HC_puckette_lock (pv->len, l_tmp, pv->l_f_old);

	  apply_invFFT_mono (pv, l_tmp, pv->window_scale, pv->l_out);
	}
    }


  // right channel
  if (flag_right_cur == 1)
    {
      // check l_f_old[]
      if (pv->flag_right == 0)
	{
	  if (pv->flag_lock == 0) // no phase lock
	    {
	      for (i = 0; i < pv->len; i ++)
		{
		  pv->r_f_old [i] = r_fs [i];
		}
	    }
	  else // loose phase lock
	    {
	      // apply loose phase lock
	      HC_puckette_lock (pv->len, r_fs, pv->r_f_old);
	    }
	  pv->flag_right = 1;
	}

      // generate the frame (out_0 + (n+1) * hop_syn), that is, "u_i"
      if (pv->flag_lock == 0) // no phase lock
	{
	  // Y[u_i] = X[t_i] (Y[u_{i-1}]/X[s_i]) / |Y[u_{i-1}]/X[s_i]|
	  HC_complex_phase_vocoder (pv->len, r_fs, r_ft, pv->r_f_old,
				    pv->r_f_old);
	  // already backed up for the next step in [lr]_f_old[]
	  apply_invFFT_mono (pv, pv->r_f_old, pv->window_scale, pv->r_out);
	}
      else // loose phase lock
	{
	  // Y[u_i] = X[t_i] (Z[u_{i-1}]/X[s_i]) / |Z[u_{i-1}]/X[s_i]|
	  HC_complex_phase_vocoder (pv->len, r_fs, r_ft, pv->r_f_old,
				    r_tmp);
	  // apply loose phase lock and store for the next step
	  HC_puckette_lock (pv->len, r_tmp, pv->r_f_old);

	  apply_invFFT_mono (pv, r_tmp, pv->window_scale, pv->r_out);
	}
    }


  /* output
   * out[0, hop_syn] ==> resample into hop_res ==> ao derive or snd file
   */
  status = pv_complex_play_resample (pv);


  /* shift
   * out[hop_syn, hop_syn + len] ==> out[0, len]
   */
  for (i = 0; i < pv->len; i ++)
    {
      pv->l_out [i] = pv->l_out [i + pv->hop_syn];
      pv->r_out [i] = pv->r_out [i + pv->hop_syn];
    }
  for (i = pv->len; i < pv->len + pv->hop_syn; i ++)
    {
      pv->l_out [i] = 0.0;
      pv->r_out [i] = 0.0;
    }

  return (status);
}



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
		  int flag_lock)
{
  long hop_res = (long)((double)hop_syn * pow (2.0, - pitch_shift / 12.0));
  long hop_ana = (long)((double)hop_res * rate);

  struct pv_complex *pv = pv_complex_init (len, hop_syn, flag_window);
  pv->hop_res = hop_res;
  pv->hop_ana = hop_ana;
  //pv->pitch_shift = pitch_shift;

  // open file
  SNDFILE *sf = NULL;
  SF_INFO sfinfo;
  memset (&sfinfo, 0, sizeof (sfinfo));
  sf = sf_open (file, SFM_READ, &sfinfo);
  if (sf == NULL)
    {
      fprintf (stderr, "fail to open %s\n", file);
      exit (1);
    }
  sndfile_print_info (&sfinfo);

  pv_complex_set_input (pv, sf, &sfinfo);


  ao_device *ao = NULL;
  SNDFILE *sfout = NULL;
  SF_INFO sfout_info;
  if (outfile == NULL)
    {
      ao = ao_init_16_stereo (sfinfo.samplerate, 1 /* verbose */);
      pv_complex_set_output_ao (pv, ao);
    }
  else
    {
      sfout = sndfile_open_for_write (&sfout_info,
				      outfile,
				      sfinfo.samplerate,
				      sfinfo.channels);
      if (sfout == NULL)
	{
	  fprintf (stderr, "fail to open file %s\n", outfile);
	  exit (1);
	}
      pv_complex_set_output_sf (pv, sfout, &sfout_info);
    }
  pv->flag_lock = flag_lock;

  long cur;
  for (cur = 0; cur < (long)sfinfo.frames; cur += pv->hop_ana)
    {
      long len_play = pv_complex_play_step (pv, cur);
      if (len_play < pv->hop_res)
	{
	  break;
	}
    }

  if (outfile == NULL)
    {
      ao_close (ao);
    }
  else
    {
      // frames left in l_out[] and r_out[]
      sndfile_write (sfout, sfout_info,
		     pv->l_out, pv->r_out, len);

      sf_write_sync (sfout);
      sf_close (sfout);
    }

  pv_complex_free (pv);
  sf_close (sf) ;
}
