/* the core of phase vocoder with complex arithmetics
 * Copyright (C) 2007 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: pv-complex.c,v 1.4 2007/02/23 02:10:25 kichiki Exp $
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


  pv->time = (double *)fftw_malloc (len * sizeof(double));
  pv->freq = (double *)fftw_malloc (len * sizeof(double));
  pv->plan = fftw_plan_r2r_1d (len, pv->time, pv->freq,
			       FFTW_R2HC, FFTW_ESTIMATE);

  pv->f_out = (double *)fftw_malloc (len * sizeof(double));
  pv->t_out = (double *)fftw_malloc (len * sizeof(double));
  pv->plan_inv = fftw_plan_r2r_1d (len, pv->f_out, pv->t_out,
				   FFTW_HC2R, FFTW_ESTIMATE);

  pv->l_f_out_old = (double *)malloc (len * sizeof(double));
  pv->r_f_out_old = (double *)malloc (len * sizeof(double));

  pv->l_out = (double *) malloc ((hop_out + len) * sizeof(double));
  pv->r_out = (double *) malloc ((hop_out + len) * sizeof(double));
  int i;
  for (i = 0; i < (hop_out + len); i ++)
    {
      pv->l_out [i] = 0.0;
      pv->r_out [i] = 0.0;
    }

  pv->flag_run = 0; // not running

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

  if (pv->l_f_out_old != NULL) free (pv->l_f_out_old);
  if (pv->r_f_out_old != NULL) free (pv->r_f_out_old);

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

/* the results are stored in pv->[lr]_out [i]
 * for i = hop_out to (hop_out + len)
 * INPUT
 *  scale : for safety (give 0.5, for example)
 */
static void
apply_invFFT_stereo (struct pv_complex_data *pv,
		     double *f_left, double *f_right,
		     double scale)
{
  int i;

  // scale
  for (i = 0; i < pv->len; i ++)
    {
      pv->f_out [i] = 0.5 * f_left [i];
    }
  fftw_execute (pv->plan_inv); // iFFT: f_out[] -> t_out[]
  // scale by len and windowing
  windowing (pv->len, pv->t_out, pv->flag_window, (double)pv->len,
	     pv->t_out);
  // superimpose
  for (i = 0; i < pv->len; i ++)
    {
      pv->l_out [pv->hop_out + i] += pv->t_out [i];
    }

  // scale
  for (i = 0; i < pv->len; i ++)
    {
      pv->f_out [i] = 0.5 * f_right [i];
    }
  fftw_execute (pv->plan_inv); // iFFT: f_out[] -> t_out[]
  // scale by len and windowing
  windowing (pv->len, pv->t_out, pv->flag_window, (double)pv->len,
	     pv->t_out);
  // superimpose
  for (i = 0; i < pv->len; i ++)
    {
      pv->r_out [pv->hop_out + i] += pv->t_out [i];
    }
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
 *  flag_lock : 0 == no phase lock
 *              1 == loose phase lock
 * OUTPUT (returned value)
 *  status : output frame.
 */
long pv_complex_play_step (struct pv_complex_data *pv,
			   long cur, double rate,
			   int flag_lock)
{
  int i;

  double *l_fs = NULL;
  double *r_fs = NULL;
  l_fs = (double *)malloc (pv->len * sizeof (double));
  r_fs = (double *)malloc (pv->len * sizeof (double));

  double *l_ft = NULL;
  double *r_ft = NULL;
  l_ft = (double *)malloc (pv->len * sizeof (double));
  r_ft = (double *)malloc (pv->len * sizeof (double));

  double *l_tmp = NULL;
  double *r_tmp = NULL;
  l_tmp = (double *)malloc (pv->len * sizeof (double));
  r_tmp = (double *)malloc (pv->len * sizeof (double));


  long status;
  // read the starting frame (cur)
  status = read_and_FFT_stereo (pv, cur, l_fs, r_fs);
  if (status != pv->len)
    return 0; // no output

  // read the terminal frame (cur + hop_out)
  status = read_and_FFT_stereo (pv, cur + pv->hop_out, l_ft, r_ft);
  if (status != pv->len)
    return 0; // no output


  if (pv->flag_run == 0)
    {
      // set [lr]_f_out_old at "u_{i-1}" (because there is no back up yet)
      if (flag_lock == 0) // no phase lock
	{
	  for (i = 0; i < pv->len; i ++)
	    {
	      pv->l_f_out_old [i] = l_fs [i];
	      pv->r_f_out_old [i] = r_fs [i];
	    }
	}
      else // loose phase lock
	{
	  // apply loose phase lock
	  HC_puckette_lock (pv->len, l_fs, pv->l_f_out_old);
	  HC_puckette_lock (pv->len, r_fs, pv->r_f_out_old);
	}

      // check
      fprintf (stderr, "flag_lock = %d\n", flag_lock);
      pv->flag_run = 1;
    }

  // generate the frame (out_0 + (n+1) * hop_out), that is, "u_i"
  if (flag_lock == 0) // no phase lock
    {
      // Y[u_i] = X[t_i] (Y[u_{i-1}]/X[s_i]) / |Y[u_{i-1}]/X[s_i]|
      HC_complex_phase_vocoder (pv->len, l_fs, l_ft, pv->l_f_out_old,
				pv->l_f_out_old);
      HC_complex_phase_vocoder (pv->len, r_fs, r_ft, pv->r_f_out_old,
				pv->r_f_out_old);
      // already backed up for the next step in [lr]_f_out_old[]

      apply_invFFT_stereo (pv, pv->l_f_out_old, pv->r_f_out_old, 0.5);
    }
  else // loose phase lock
    {
      // Y[u_i] = X[t_i] (Z[u_{i-1}]/X[s_i]) / |Z[u_{i-1}]/X[s_i]|
      HC_complex_phase_vocoder (pv->len, l_fs, l_ft, pv->l_f_out_old,
				l_tmp);
      HC_complex_phase_vocoder (pv->len, r_fs, r_ft, pv->r_f_out_old,
				r_tmp);
      // apply loose phase lock and store for the next step
      HC_puckette_lock (pv->len, l_tmp, pv->l_f_out_old);
      HC_puckette_lock (pv->len, r_tmp, pv->r_f_out_old);

      apply_invFFT_stereo (pv, l_tmp, r_tmp, 0.5);
    }


  // output
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

  /* shift acc_out by hop_out */
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

  free (l_fs);
  free (r_fs);
  free (l_ft);
  free (r_ft);
  free (l_tmp);
  free (r_tmp);

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
 */
void pv_complex (const char *file, const char *outfile,
		 double rate, long len, long hop_out,
		 int flag_window,
		 int flag_lock)
{
  long hop_in;
  hop_in = (long)((double)hop_out * rate);

  struct pv_complex_data *pv = NULL;
  pv = pv_complex_init (len, hop_out, flag_window);


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

  long cur;
  for (cur = 0; cur < (long)sfinfo.frames; cur += hop_in)
    {
      long status;
      status = pv_complex_play_step (pv, cur, rate, flag_lock);
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
