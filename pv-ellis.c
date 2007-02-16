/* PV - phase vocoder : pv-ellis.c
 * Copyright (C) 2007 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: pv-ellis.c,v 1.2 2007/02/16 06:25:50 kichiki Exp $
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

// esd sound device
#include <esd.h>
#include "esd-wrapper.h"


/* this routine is translated from Matlab script written by D.P.W.Ellis:
 *   http://www.ee.columbia.edu/~dpwe/resources/matlab/pvoc/
 */
void pv_ellis (const char *file, const char *outfile,
	       double rate, long len, long hop,
	       int flag_window)
{
  double twopi = 2.0 * M_PI;

  int i;
  int k;

  long nf;
  long nf0;

  long hop_in;
  long hop_out;

  hop_in  = hop;
  hop_out = hop;


  // open wav file
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

  /* open sound device */
  int status;
  int esd = 0; // for compiler warning...
  SNDFILE *sfout = NULL;
  SF_INFO sfout_info;
  if (outfile == NULL)
    {
      esd = esd_init_16_stereo_strem_play (sfinfo.samplerate);
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
  long out_frames = 0;


  /* initialization plan for FFTW  */
  double *time = NULL;
  double *freq = NULL;
  time = (double *)fftw_malloc (len * sizeof(double));
  freq = (double *)fftw_malloc (len * sizeof(double));
  fftw_plan plan;
  plan = fftw_plan_r2r_1d (len, time, freq, FFTW_R2HC, FFTW_ESTIMATE);

  double *t_out = NULL;
  double *f_out = NULL;
  f_out = (double *)fftw_malloc (len * sizeof(double));
  t_out = (double *)fftw_malloc (len * sizeof(double));
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

  double *l_am0 = NULL;
  double *l_ph0 = NULL;
  double *r_am0 = NULL;
  double *r_ph0 = NULL;
  l_am0 = (double *)malloc (((len/2)+1) * sizeof(double));
  l_ph0 = (double *)malloc (((len/2)+1) * sizeof(double));
  r_am0 = (double *)malloc (((len/2)+1) * sizeof(double));
  r_ph0 = (double *)malloc (((len/2)+1) * sizeof(double));

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
  l_out = (double *) malloc ((hop_out + len) * sizeof(double));
  r_out = (double *) malloc ((hop_out + len) * sizeof(double));
  for (i = 0; i < (hop_out + len); i ++)
    {
      l_out [i] = 0.0;
      r_out [i] = 0.0;
    }

  // expected frequency
  double *omega = NULL;
  omega = (double *) malloc (sizeof (double) * ((len/2)+1));
  for (k = 0; k < (len/2)+1; k ++)
    {
      omega [k] = twopi * (double)k / (double)len;
    }

  // averaged magnitude (amplitude) to re-synthesize
  double *l_mag = NULL;
  double *r_mag = NULL;
  l_mag = (double *) malloc (sizeof (double) * ((len/2)+1));
  r_mag = (double *) malloc (sizeof (double) * ((len/2)+1));

  // Phase accumulator to re-synthesize
  double *l_ph = NULL;
  double *r_ph = NULL;
  l_ph = (double *) malloc (sizeof (double) * ((len/2)+1));
  r_ph = (double *) malloc (sizeof (double) * ((len/2)+1));


  // read the first frame
  read_status = sndfile_read (sf, sfinfo, left, right, len);
  if (read_status != len)
    {
      exit (1);
    }

  // FFT
  apply_FFT (len, left, flag_window, plan, time, freq, 0.5, l_amp, l_phs);
  apply_FFT (len, right, flag_window, plan, time, freq, 0.5, r_amp, r_phs);
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
  // shift
  for (i = 0; i < (len - hop_in); i ++)
    {
      left  [i]  = left  [i + hop_in];
      right  [i] = right  [i + hop_in];
    }
  // read hop_in frames
  read_status = sndfile_read (sf, sfinfo,
			      left + len - hop_in,
			      right + len - hop_in,
			      hop_in);
  if (read_status != hop_in)
    {
      exit (1);
    }

  // FFT
  apply_FFT (len, left, flag_window, plan, time, freq, 0.5, l_amp, l_phs);
  apply_FFT (len, right, flag_window, plan, time, freq, 0.5, r_amp, r_phs);
  nf = 1; // 1*hop_in frame

  double tt;
  int step;
  for (tt = 0.0, step = 0; ; tt += 1.0 / rate, step++)
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

	      // read t1 * hop_in frame, the next one!
	      // shift
	      for (i = 0; i < (len - hop_in); i ++)
		{
		  left  [i]  = left  [i + hop_in];
		  right  [i] = right  [i + hop_in];
		}
	      read_status = sndfile_read (sf, sfinfo,
					  left + len - hop_in,
					  right + len - hop_in,
					  hop_in);
	      if (read_status != hop_in)
		{
		  // most likely, it is EOF.
		  break;
		}
	      // FFT
	      apply_FFT (len, left, flag_window, plan, time, freq, 0.5,
			 l_amp, l_phs);
	      apply_FFT (len, right, flag_window, plan, time, freq, 0.5,
			 r_amp, r_phs);
	      nf = t1;
	    }
	  else // we have to read the last data for t0 (and for t1, too)
	    {
	      // read t0 * hop_in frame
	      read_status = sndfile_read_at (sf, sfinfo,
					     (long)t0 * hop_in,
					     left, right, len);
	      if (read_status != len)
		{
		  // most likely, it is EOF.
		  break;
		}
	      // FFT
	      apply_FFT (len, left, flag_window, plan, time, freq, 0.5,
			 l_am0, l_ph0);
	      apply_FFT (len, right, flag_window, plan, time, freq, 0.5,
			 r_am0, r_ph0);
	      nf0 = t0;

	      // read t1 * hop_in frame, the next one!
	      // shift
	      for (i = 0; i < (len - hop_in); i ++)
		{
		  left  [i] = left  [i + hop_in];
		  right [i] = right [i + hop_in];
		}
	      read_status = sndfile_read (sf, sfinfo,
					  left + len - hop_in,
					  right + len - hop_in,
					  hop_in);
	      if (read_status != hop_in)
		{
		  // most likely, it is EOF.
		  break;
		}
	      // FFT
	      apply_FFT (len, left, flag_window, plan, time, freq, 0.5,
			 l_amp, l_phs);
	      apply_FFT (len, right, flag_window, plan, time, freq, 0.5,
			 r_amp, r_phs);
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
      windowing (len, t_out, flag_window, (double)len, t_out);
      // superimpose
      for (i = 0; i < len; i ++)
	{
	  l_out [hop_out + i] += t_out [i];
	}

      polar_to_HC (len, r_mag, r_ph, 0, f_out); // (bmag, ph) -> f_out[]
      fftw_execute (plan_inv); // inv-FFT: f_out[] -> t_out[]
      // scale by len and windowing
      windowing (len, t_out, flag_window, (double)len, t_out);
      // superimpose
      for (i = 0; i < len; i ++)
	{
	  r_out [hop_out + i] += t_out [i];
	}


      // Cumulate phase, ready for next frame
      // calculate phase advance
      double dp;
      for (k = 0; k < (len/2)+1; k ++)
	{
	  // NOTE: I don't understand why the following is working...
	  // the phase for the next output is hop_out/rate frames ahead,
	  // while the phase increment is for hop_out frames.

	  dp = l_phs [k] - l_ph0 [k] - omega [k] * (double)hop_in;
	  for (; dp >= M_PI; dp -= twopi);
	  for (; dp < -M_PI; dp += twopi);
	  l_ph [k] += (omega [k] + dp / (double)hop_in) * (double) hop_out;
	  l_ph [k] -= twopi * (double)((int)(l_ph [k] / twopi));

	  dp = r_phs [k] - r_ph0 [k] - omega [k] * (double)hop_in;
	  for (; dp >= M_PI; dp -= twopi);
	  for (; dp < -M_PI; dp += twopi);
	  r_ph [k] += (omega [k] + dp / (double)hop_in) * (double) hop_out;
	  r_ph [k] -= twopi * (double)((int)(r_ph [k] / twopi));
	}

      // output
      if (outfile == NULL)
	{
	  status = esd_write (esd, l_out, r_out, hop_out);
	}
      else
	{
	  status = sndfile_write (sfout, sfout_info, l_out, r_out, hop_out);
	  out_frames += status;
	}


      // shift acc_out by hop_out
      for (i = 0; i < len; i ++)
	{
	  l_out [i] = l_out [i + hop_out];
	  r_out [i] = r_out [i + hop_out];
	}
      for (i = len; i < len + hop_out; i ++)
	{
	  l_out [i] = 0.0;
	  r_out [i] = 0.0;
	}
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

  sf_close (sf) ;
  if (outfile == NULL) esd_close (esd);
  else
    {
      // set frames
      sfout_info.frames = out_frames;
      sf_write_sync (sfout);
      sf_close (sfout);
    }
}
