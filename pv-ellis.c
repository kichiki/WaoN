/* PV - phase vocoder : pv-ellis.c
 * Copyright (C) 2007 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: pv-ellis.c,v 1.7 2007/03/11 01:04:21 kichiki Exp $
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
#include "fft.h" // windowing(), apply_FFT()
// libsndfile
#include <sndfile.h>
#include "snd.h"

// ao device
#include <ao/ao.h>
#include "ao-wrapper.h"

#include "memory-check.h" // CHECK_MALLOC() macro
#include "pv-conventional.h" // pv_play_resample()


static long
read_and_FFT_stereo (SNDFILE *sf, SF_INFO *sfinfo,
		     long frame,
		     int len,
		     int flag_window,
		     fftw_plan plan, double *time, double *freq,
		     double *l_amp, double *l_phs,
		     double *r_amp, double *r_phs)
{
  double * left = NULL;
  double * right = NULL;
  left  = (double *) malloc (sizeof (double) * len);
  right = (double *) malloc (sizeof (double) * len);
  CHECK_MALLOC (left,  "read_and_FFT_stereo");
  CHECK_MALLOC (right, "read_and_FFT_stereo");

  long status;
  status = sndfile_read_at (sf, *sfinfo, frame,
			    left, right, len);
  if (status != len)
    {
      free (left);
      free (right);

      return (status);
    }

  // FFT
  apply_FFT (len, left, flag_window, plan, time, freq,
	     1.0,
	     l_amp, l_phs);
  apply_FFT (len, right, flag_window, plan, time, freq,
	     1.0,
	     r_amp, r_phs);

  free (left);
  free (right);

  return (status);
}

/* this routine is translated from Matlab script written by D.P.W.Ellis:
 *   http://www.ee.columbia.edu/~dpwe/resources/matlab/pvoc/
 */
void pv_ellis (const char *file, const char *outfile,
	       double rate, double pitch_shift,
	       long len, long hop,
	       int flag_window)
{
  double twopi = 2.0 * M_PI;

  int i;
  int k;

  long nf;
  long nf0;

  long hop_syn;
  hop_syn = hop;
  long hop_res;
  hop_res = (long)((double)hop_syn * pow (2.0, - pitch_shift / 12.0));

  double corr_rate;
  corr_rate = (double)hop_res * rate / (double)hop_syn;


  // open the input file
  long read_status;
  // libsndfile version
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


  /* allocate buffers  */
  double * left = NULL;
  double * right = NULL;
  left  = (double *) malloc (sizeof (double) * len);
  right = (double *) malloc (sizeof (double) * len);
  CHECK_MALLOC (left,  "pv_ellis");
  CHECK_MALLOC (right, "pv_ellis");

  // prepare the output
  int status;
  ao_device *ao = NULL;
  SNDFILE *sfout = NULL;
  SF_INFO sfout_info;
  if (outfile == NULL)
    {
      ao = ao_init_16_stereo (sfinfo.samplerate);
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
    }


  double window_scale;
  window_scale = get_scale_factor_for_window (len, hop_syn, flag_window);


  /* initialization plan for FFTW  */
  double *time = NULL;
  double *freq = NULL;
  time = (double *)fftw_malloc (len * sizeof(double));
  freq = (double *)fftw_malloc (len * sizeof(double));
  CHECK_MALLOC (time, "pv_ellis");
  CHECK_MALLOC (freq, "pv_ellis");
  fftw_plan plan;
  plan = fftw_plan_r2r_1d (len, time, freq, FFTW_R2HC, FFTW_ESTIMATE);

  double *t_out = NULL;
  double *f_out = NULL;
  f_out = (double *)fftw_malloc (len * sizeof(double));
  t_out = (double *)fftw_malloc (len * sizeof(double));
  CHECK_MALLOC (f_out, "pv_ellis");
  CHECK_MALLOC (t_out, "pv_ellis");
  fftw_plan plan_inv;
  plan_inv = fftw_plan_r2r_1d (len, f_out, t_out,
			       FFTW_HC2R, FFTW_ESTIMATE);

  double *l_amp = NULL;
  double *l_phs = NULL;
  double *r_amp = NULL;
  double *r_phs = NULL;
  l_amp = (double *)malloc (((len/2)+1) * sizeof(double));
  l_phs = (double *)malloc (((len/2)+1) * sizeof(double));
  r_amp = (double *)malloc (((len/2)+1) * sizeof(double));
  r_phs = (double *)malloc (((len/2)+1) * sizeof(double));
  CHECK_MALLOC (l_amp, "pv_ellis");
  CHECK_MALLOC (l_phs, "pv_ellis");
  CHECK_MALLOC (r_amp, "pv_ellis");
  CHECK_MALLOC (r_phs, "pv_ellis");

  double *l_am0 = NULL;
  double *l_ph0 = NULL;
  double *r_am0 = NULL;
  double *r_ph0 = NULL;
  l_am0 = (double *)malloc (((len/2)+1) * sizeof(double));
  l_ph0 = (double *)malloc (((len/2)+1) * sizeof(double));
  r_am0 = (double *)malloc (((len/2)+1) * sizeof(double));
  r_ph0 = (double *)malloc (((len/2)+1) * sizeof(double));
  CHECK_MALLOC (l_am0, "pv_ellis");
  CHECK_MALLOC (l_ph0, "pv_ellis");
  CHECK_MALLOC (r_am0, "pv_ellis");
  CHECK_MALLOC (r_ph0, "pv_ellis");

  for (i = 0; i < (len/2)+1; i ++)
    {
      l_amp [i] = 0.0;
      l_phs [i] = 0.0;
      r_amp [i] = 0.0;
      r_phs [i] = 0.0;

      l_am0 [i] = 0.0;
      l_ph0 [i] = 0.0;
      r_am0 [i] = 0.0;
      r_ph0 [i] = 0.0;
    }

  double *l_out = NULL;
  double *r_out = NULL;
  l_out = (double *) malloc ((hop_syn + len) * sizeof(double));
  r_out = (double *) malloc ((hop_syn + len) * sizeof(double));
  CHECK_MALLOC (l_out, "pv_ellis");
  CHECK_MALLOC (r_out, "pv_ellis");
  for (i = 0; i < (hop_syn + len); i ++)
    {
      l_out [i] = 0.0;
      r_out [i] = 0.0;
    }

  // expected frequency
  double *omega = NULL;
  omega = (double *) malloc (sizeof (double) * ((len/2)+1));
  CHECK_MALLOC (omega, "pv_ellis");
  for (k = 0; k < (len/2)+1; k ++)
    {
      omega [k] = twopi * (double)k / (double)len;
    }

  // averaged magnitude (amplitude) to re-synthesize
  double *l_mag = NULL;
  double *r_mag = NULL;
  l_mag = (double *) malloc (sizeof (double) * ((len/2)+1));
  r_mag = (double *) malloc (sizeof (double) * ((len/2)+1));
  CHECK_MALLOC (l_mag, "pv_ellis");
  CHECK_MALLOC (r_mag, "pv_ellis");

  // Phase accumulator to re-synthesize
  double *l_ph = NULL;
  double *r_ph = NULL;
  l_ph = (double *) malloc (sizeof (double) * ((len/2)+1));
  r_ph = (double *) malloc (sizeof (double) * ((len/2)+1));
  CHECK_MALLOC (l_ph, "pv_ellis");
  CHECK_MALLOC (r_ph, "pv_ellis");


  // read the first frame
  read_status = read_and_FFT_stereo (sf, &sfinfo, 0,
				     len, flag_window,
				     plan, time, freq,
				     l_amp, l_phs,
				     r_amp, r_phs);
  if (read_status != len)
    {
      exit (1);
    }
  nf = 0; // 0 frame

  // Preset to phase of first frame for perfect reconstruction
  // in case of 1:1 time scaling
  for (k = 0; k < (len/2) + 1; k ++)
    {
      l_ph [k] = l_phs [k];
      r_ph [k] = r_phs [k];
    }

  // read next frame
  // backup
  nf0 = nf;
  for (k = 0; k < (len/2) + 1; k ++)
    {
      l_am0 [k] = l_amp [k];
      l_ph0 [k] = l_phs [k];

      r_am0 [k] = r_amp [k];
      r_ph0 [k] = r_phs [k];
    }
  read_status = read_and_FFT_stereo (sf, &sfinfo, hop_syn,
				     len, flag_window,
				     plan, time, freq,
				     l_amp, l_phs,
				     r_amp, r_phs);
  if (read_status != len)
    {
      exit (1);
    }
  nf = 1; // 1*hop_syn frame

  double tt;
  int step;
  //for (tt = 0.0, step = 0; ; tt += rate, step++)
  for (tt = 0.0, step = 0; ; tt += corr_rate, step++)
    {
      int t0, t1;
      t0 = (int)tt;
      t1 = t0 + 1;
      if (t0 != nf0)
	{
	  if (t0 == nf) // then, we can use the last data for t0
	    {
	      for (k = 0; k < (len/2) + 1; k ++)
		{
		  l_am0 [k] = l_amp [k];
		  l_ph0 [k] = l_phs [k];
		  r_am0 [k] = r_amp [k];
		  r_ph0 [k] = r_phs [k];
		}
	      nf0 = t0;

	      // read t1 * hop_syn frame, the next one!
	      read_status = read_and_FFT_stereo (sf, &sfinfo,
						 (long)t1 * hop_syn,
						 len, flag_window,
						 plan, time, freq,
						 l_amp, l_phs,
						 r_amp, r_phs);
	      if (read_status != len)
		{
		  // most likely, it is EOF.
		  break;
		}
	      nf = t1;
	    }
	  else // we have to read the last data for t0 (and for t1, too)
	    {
	      // read t0 * hop_syn frame
	      read_status = read_and_FFT_stereo (sf, &sfinfo,
						 (long)t0 * hop_syn,
						 len, flag_window,
						 plan, time, freq,
						 l_am0, l_ph0,
						 r_am0, r_ph0);
	      if (read_status != len)
		{
		  // most likely, it is EOF.
		  break;
		}
	      nf0 = t0;

	      // read t1 * hop_syn frame, the next one!
	      read_status = read_and_FFT_stereo (sf, &sfinfo,
						 (long)t1 * hop_syn,
						 len, flag_window,
						 plan, time, freq,
						 l_amp, l_phs,
						 r_amp, r_phs);
	      if (read_status != len)
		{
		  // most likely, it is EOF.
		  break;
		}
	      nf = t1;
	    }
	}
      /* otherwise, t0 == nf0 (and therefore t1 == nf),
       * so that we do not need to read the frames */

      double tf;
      tf = tt - (double)((int) tt);
      for (k = 0; k < (len/2)+1; k ++)
	{
	  l_mag [k] = (1.0 - tf) * l_am0 [k] + tf * l_amp [k];
	  r_mag [k] = (1.0 - tf) * r_am0 [k] + tf * r_amp [k];
	}

      // synthesize
      polar_to_HC (len, l_mag, l_ph, 0, f_out); // (bmag, ph) -> f_out[]
      fftw_execute (plan_inv); // inv-FFT: f_out[] -> t_out[]
      // scale by len and windowing
      windowing (len, t_out, flag_window, (double)len * window_scale, t_out);
      // superimpose
      for (i = 0; i < len; i ++)
	{
	  l_out [hop_syn + i] += t_out [i];
	}

      polar_to_HC (len, r_mag, r_ph, 0, f_out); // (bmag, ph) -> f_out[]
      fftw_execute (plan_inv); // inv-FFT: f_out[] -> t_out[]
      // scale by len and windowing
      windowing (len, t_out, flag_window, (double)len * window_scale, t_out);
      // superimpose
      for (i = 0; i < len; i ++)
	{
	  r_out [hop_syn + i] += t_out [i];
	}


      // Cumulate phase, ready for next frame
      // calculate phase advance
      double dp;
      for (k = 0; k < (len/2)+1; k ++)
	{
	  // NOTE: I don't understand why the following is working...
	  // the phase for the next output is hop_syn/rate frames ahead,
	  // while the phase increment is for hop_syn frames.

	  dp = l_phs [k] - l_ph0 [k] - omega [k] * (double)hop_syn;
	  for (; dp >= M_PI; dp -= twopi);
	  for (; dp < -M_PI; dp += twopi);
	  l_ph [k] += (omega [k] + dp / (double)hop_syn) * (double) hop_syn;
	  l_ph [k] -= twopi * (double)((int)(l_ph [k] / twopi));

	  dp = r_phs [k] - r_ph0 [k] - omega [k] * (double)hop_syn;
	  for (; dp >= M_PI; dp -= twopi);
	  for (; dp < -M_PI; dp += twopi);
	  r_ph [k] += (omega [k] + dp / (double)hop_syn) * (double) hop_syn;
	  r_ph [k] -= twopi * (double)((int)(r_ph [k] / twopi));
	}

      // output
      /*
      status = pv_play_resample ((long)((double)hop_syn * rate),
				 hop_syn, l_out, r_out,
				 ao, sfout, &sfout_info,
				 flag_pitch);
      */
      status = pv_play_resample (hop_res, hop_syn, l_out, r_out,
				 ao, sfout, &sfout_info);
      // note here hop_syn is set equal to hop_syn, so give the correct one


      // shift acc_out by hop_syn
      for (i = 0; i < len; i ++)
	{
	  l_out [i] = l_out [i + hop_syn];
	  r_out [i] = r_out [i + hop_syn];
	}
      for (i = len; i < len + hop_syn; i ++)
	{
	  l_out [i] = 0.0;
	  r_out [i] = 0.0;
	}
    }


  sf_close (sf) ;
  if (outfile == NULL)
    {
      ao_close (ao);
    }
  else
    {
      // frames left in l_out[] and r_out[]
      status = sndfile_write (sfout, sfout_info, l_out, r_out, len);
      sf_write_sync (sfout);
      sf_close (sfout);
    }

  free (left);
  free (right);

  free (time);
  free (freq);
  fftw_destroy_plan (plan);

  free (t_out);
  free (f_out);
  fftw_destroy_plan (plan_inv);

  free (l_amp);
  free (l_phs);
  free (r_amp);
  free (r_phs);
  free (l_am0);
  free (l_ph0);
  free (r_am0);
  free (r_ph0);

  free (l_out);
  free (r_out);

  free (omega);
  free (l_mag);
  free (r_mag);

  free (l_ph);
  free (r_ph);
}
