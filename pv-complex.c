/* the core of phase vocoder with complex arithmetics
 * Copyright (C) 2007 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: pv-complex.c,v 1.8 2007/02/25 06:38:58 kichiki Exp $
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

#include "pv-conventional.h" // pv_play_resample() for check
#include "pv-complex.h"


/** utility routines for struct pv_omplex_data **/

struct pv_complex_data *
pv_complex_init (long len, long hop_out, int flag_window)
{
  struct pv_complex_data *pv = NULL;

  pv = (struct pv_complex_data *) malloc (sizeof (struct pv_complex_data));
  if (pv == NULL)
    {
      fprintf (stderr, "allocation error in pv_complex_init()\n");
      exit (1);
    }

  pv->len = len;
  pv->hop_out = hop_out;

  pv->flag_window = flag_window;

  pv->window_scale = get_scale_factor_for_window (len, hop_out, flag_window);

  pv->time = (double *)fftw_malloc (len * sizeof(double));
  pv->freq = (double *)fftw_malloc (len * sizeof(double));
  pv->plan = fftw_plan_r2r_1d (len, pv->time, pv->freq,
			       FFTW_R2HC, FFTW_ESTIMATE);

  pv->f_out = (double *)fftw_malloc (len * sizeof(double));
  pv->t_out = (double *)fftw_malloc (len * sizeof(double));
  pv->plan_inv = fftw_plan_r2r_1d (len, pv->f_out, pv->t_out,
				   FFTW_HC2R, FFTW_ESTIMATE);

  pv->l_f_old = (double *)malloc (len * sizeof(double));
  pv->r_f_old = (double *)malloc (len * sizeof(double));

  pv->l_out = (double *) malloc ((hop_out + len) * sizeof(double));
  pv->r_out = (double *) malloc ((hop_out + len) * sizeof(double));
  int i;
  for (i = 0; i < (hop_out + len); i ++)
    {
      pv->l_out [i] = 0.0;
      pv->r_out [i] = 0.0;
    }

  pv->flag_left  = 0; // l_f_old[] is not initialized yet
  pv->flag_right = 0; // r_f_old[] is not initialized yet

  pv->flag_lock = 0; // no phase lock (for default)

  pv->pitch_shift = 0.0; // no pitch-shift

  return (pv);
}

void
pv_complex_set_input (struct pv_complex_data *pv,
		      SNDFILE *sf, SF_INFO *sfinfo)
{
  pv->sf = sf;
  pv->sfinfo = sfinfo;
}

void
pv_complex_set_output_ao (struct pv_complex_data *pv,
			  ao_device *ao)
{
  pv->flag_out = 0;
  pv->ao = ao;
}

void
pv_complex_set_output_sf (struct pv_complex_data *pv,
			  SNDFILE *sf, SF_INFO *sfinfo)
{
  pv->flag_out = 1;
  pv->sfout = sf;
  pv->sfout_info = sfinfo;
}

void
pv_complex_free (struct pv_complex_data *pv)
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


static long
read_and_FFT_stereo (struct pv_complex_data *pv,
		     long frame,
		     double *f_left, double *f_right)
{
  int i;

  double * left = NULL;
  double * right = NULL;
  left  = (double *) malloc (sizeof (double) * pv->len);
  right = (double *) malloc (sizeof (double) * pv->len);

  long status;
  status = sndfile_read_at (pv->sf, *(pv->sfinfo), frame,
			    left, right, pv->len);
  if (status != pv->len)
    {
      free (left);
      free (right);

      return (status);
    }

  // FFT for left channel
  windowing (pv->len, left, pv->flag_window, 1.0, pv->time);
  fftw_execute (pv->plan); // FFT: time[] -> freq[]
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

  free (left);
  free (right);

  return (status);
}

/* the results are stored in out [i] for i = hop_out to (hop_out + len)
 * INPUT
 *  scale : for safety (give 0.5, for example)
 */
static void
apply_invFFT_mono (struct pv_complex_data *pv,
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
      out [pv->hop_out + i] += pv->t_out [i];
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


/* play the segment of pv->[lr]_out[] for pv->hop_out
 * pv->pitch_shift is taken into account
 */
int
pv_complex_play_resample (struct pv_complex_data *pv)
{
  int status = 0;
  int i;

  // samplerate conversion
  float *fl_in  = NULL;
  float *fl_out = NULL;
  double *l_out_src = NULL;
  double *r_out_src = NULL;
  SRC_DATA srdata;

  if (pv->pitch_shift != 0.0)
    {
      double rate_by_pitch_shift;
      rate_by_pitch_shift = pow (2.0, - pv->pitch_shift / 12.0);
      long hop_in = (long)((double)pv->hop_out * rate_by_pitch_shift);

      fl_in  = (float *)malloc (sizeof (float) * 2 * pv->hop_out);
      fl_out = (float *)malloc (sizeof (float) * 2 * hop_in);

      srdata.input_frames  = pv->hop_out;
      srdata.output_frames = hop_in;
      //srdata.src_ratio = (double)hop_in / (double)(pv->hop_out);
      srdata.src_ratio = rate_by_pitch_shift;
      srdata.data_in  = fl_in;
      srdata.data_out = fl_out;

      // samplerate conversion (time fixed)
      for (i = 0; i < pv->hop_out; i ++)
	{
	  fl_in [i*2 + 0] = (float)(pv->l_out [i]);
	  fl_in [i*2 + 1] = (float)(pv->r_out [i]);
	}
      //status = src_simple (&srdata, SRC_SINC_BEST_QUALITY, 2);
      status = src_simple (&srdata, SRC_SINC_FASTEST, 2);
      // this works better
      if (status != 0)
	{
	  fprintf (stderr, "fail to samplerate conversion\n");
	  exit (1);
	}

      l_out_src = (double *)malloc (sizeof (double) * hop_in);
      r_out_src = (double *)malloc (sizeof (double) * hop_in);

      for (i = 0; i < hop_in; i ++)
	{
	  l_out_src [i] = (double)(fl_out [i*2 + 0]);
	  r_out_src [i] = (double)(fl_out [i*2 + 1]);
	}
      free (fl_in);
      free (fl_out);

      // output
      if (pv->flag_out == 0)
	{
	  status = ao_write (pv->ao, l_out_src, r_out_src, hop_in);
	  status /= 4; // 2 bytes for 2 channels
	}
      else if (pv->flag_out == 1)
	{
	  status = sndfile_write (pv->sfout, *(pv->sfout_info),
				  l_out_src, r_out_src, hop_in);
	}
      else
	{
	  fprintf (stderr, "invalid output device\n");
	}
      free (l_out_src);
      free (r_out_src);

      // modify status
      if (status == hop_in) status = pv->hop_out;
      else status = (int)((double)status / rate_by_pitch_shift);
    }
  else
    {
      if (pv->flag_out == 0)
	{
	  status = ao_write (pv->ao, pv->l_out, pv->r_out, pv->hop_out);
	  status /= 4; // 2 bytes for 2 channels
	}
      else if (pv->flag_out == 1)
	{
	  status = sndfile_write (pv->sfout, *(pv->sfout_info),
				  pv->l_out, pv->r_out, pv->hop_out);
	}
      else
	{
	  fprintf (stderr, "invalid output device\n");
	}
    }

  return (status);
}

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
 *  pv->flag_lock : 0 == no phase lock
 *                  1 == loose phase lock
 * OUTPUT (returned value)
 *  status : output frame.
 */
long pv_complex_play_step (struct pv_complex_data *pv,
			   long cur, double rate)
{
  int i;

  double *l_fs = NULL;
  double *r_fs = NULL;
  l_fs = (double *)malloc (pv->len * sizeof (double));
  r_fs = (double *)malloc (pv->len * sizeof (double));

  long status;
  // read the starting frame (cur)
  status = read_and_FFT_stereo (pv, cur, l_fs, r_fs);
  if (status != pv->len)
    {
      free (l_fs);
      free (r_fs);

      return 0; // no output
    }

  double *l_ft = NULL;
  double *r_ft = NULL;
  l_ft = (double *)malloc (pv->len * sizeof (double));
  r_ft = (double *)malloc (pv->len * sizeof (double));

  // read the terminal frame (cur + hop_out)
  status = read_and_FFT_stereo (pv, cur + pv->hop_out, l_ft, r_ft);
  if (status != pv->len)
    {
      free (l_fs);
      free (r_fs);
      free (l_ft);
      free (r_ft);

      return 0; // no output
    }

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


  double *l_tmp = NULL;
  double *r_tmp = NULL;
  l_tmp = (double *)malloc (pv->len * sizeof (double));
  r_tmp = (double *)malloc (pv->len * sizeof (double));


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

      // generate the frame (out_0 + (n+1) * hop_out), that is, "u_i"
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
  free (l_fs);
  free (l_ft);
  free (l_tmp);


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

      // generate the frame (out_0 + (n+1) * hop_out), that is, "u_i"
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
  free (r_fs);
  free (r_ft);
  free (r_tmp);


  // output
  status = pv_complex_play_resample (pv);


  /* shift [lr]_out by hop_out */
  for (i = 0; i < pv->len; i ++)
    {
      pv->l_out [i] = pv->l_out [i + pv->hop_out];
      pv->r_out [i] = pv->r_out [i + pv->hop_out];
    }
  for (i = pv->len; i < pv->len + pv->hop_out; i ++)
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
		 double rate, long len, long hop_out,
		 int flag_window,
		 int flag_lock,
		 double pitch_shift)
{
  long hop_in;
  hop_in = (long)((double)hop_out * rate);

  struct pv_complex_data *pv = NULL;
  pv = pv_complex_init (len, hop_out, flag_window);
  pv->pitch_shift = pitch_shift;

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
      ao = ao_init_16_stereo (sfinfo.samplerate);
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
  for (cur = 0; cur < (long)sfinfo.frames; cur += hop_in)
    {
      long status;
      status = pv_complex_play_step (pv, cur, rate);
      if (status < pv->hop_out)
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
