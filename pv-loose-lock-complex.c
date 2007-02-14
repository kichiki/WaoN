/* PV - phase vocoder : pv-loose-lock-complex.c
 * Copyright (C) 2007 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: pv-loose-lock-complex.c,v 1.1 2007/02/14 03:46:11 kichiki Exp $
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
#include "fft.h" // windowing(), HC_complex_phase_vocoder()

// libsndfile
#include <sndfile.h>
#include "snd.h"

// esd sound device
#include <esd.h>
#include "esd-wrapper.h"


/* puckette's loose phase lock scheme by complex arithmetics with fixed hops.
 *   t_i - s_i = u_i - u_{i-1} = hop
 *   where s_i and t_i are the times for two analysis FFT
 *   and u_i is the time for the synthesis FFT at step i
 * Reference: M.Puckette (1995)
 */
void pv_loose_lock_complex (const char *file, const char *outfile,
			    double rate, long len, long hop_in,
			    int flag_window)
{
  long hop_out;
  hop_out = rate * hop_in;


  int i;

  // open wav file
  int samplerate;
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
  samplerate = sfinfo.samplerate;


  /* allocate buffers  */
  double * left = NULL;
  double * right = NULL;
  left  = (double *) malloc (sizeof (double) * len);
  right = (double *) malloc (sizeof (double) * len);


  // esd sound device
  int status;
  int out_bits     = ESD_BITS16;
  int out_channels = ESD_STEREO;
  int out_mode     = ESD_STREAM;
  int out_func     = ESD_PLAY;
  int out_format;
  out_format = out_bits | out_channels | out_mode | out_func;

  int esd = 0; // for compiler warning...
  SNDFILE *sfout = NULL;
  SF_INFO sfout_info;
  if (outfile == NULL)
    {
      esd = esd_play_stream_fallback (out_format,
				      samplerate, // 44100
				      NULL, // host
				      "fs-test" //name
				      );
    }
  else
    {
      sfout = sndfile_open_for_write (&sfout_info,
				      outfile,
				      samplerate,
				      sfinfo.channels);
      if (sfout == NULL)
	{
	  fprintf (stderr, "fail to open file %s\n", outfile);
	  exit (1);
	}
    }
  long out_frames = 0;


  /* initialization plan for FFTW  */
  /* for FFTW library */
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

  double *l_f_out_old = NULL;
  double *r_f_out_old = NULL;
  l_f_out_old = (double *)malloc (len * sizeof(double));
  r_f_out_old = (double *)malloc (len * sizeof(double));

  double *l_fs = NULL;
  double *r_fs = NULL;
  double *l_ft = NULL;
  double *r_ft = NULL;
  l_fs = (double *)malloc (len * sizeof (double));
  r_fs = (double *)malloc (len * sizeof (double));
  l_ft = (double *)malloc (len * sizeof (double));
  r_ft = (double *)malloc (len * sizeof (double));


  double *l_out = NULL;
  double *r_out = NULL;
  l_out = (double *) malloc ((hop_out + len) * sizeof(double));
  r_out = (double *) malloc ((hop_out + len) * sizeof(double));
  for (i = 0; i < (hop_out + len); i ++)
    {
      l_out [i] = 0.0;
      r_out [i] = 0.0;
    }


  long in_0;
  long out_0;
  int n;
  for (n = 0;; n++)
    {
      // set initial frames for in and out
      if (hop_in < hop_out) // expanding
	{
	  out_0 = 0;
	  in_0 = hop_out - hop_in;
	}
      else // shrinking
	{
	  out_0 = hop_in - hop_out;
	  in_0 = 0;
	}

      // read the starting frame (in_0 + n * hop_in)
      read_status = sndfile_read_at (sf, sfinfo,
				     in_0 + n * hop_in,
				     left, right, len);
      if (read_status != len)
	{
	  // most likely, it is EOF.
	  break;
	}

      // FFT for starting time
      windowing (len, left, flag_window, 1.0, time);
      fftw_execute (plan); // FFT: time[] -> freq[]
      for (i = 0; i < len; i ++)
	{
	  l_fs [i] = freq [i];
	}
      windowing (len, right, flag_window, 1.0, time);
      fftw_execute (plan); // FFT: time[] -> freq[]
      for (i = 0; i < len; i ++)
	{
	  r_fs [i] = freq [i];
	}


      // read the terminal frame (in_0 + n * hop_in + hop_out)
      read_status = sndfile_read_at (sf, sfinfo,
				     in_0 + n * hop_in + hop_out,
				     left, right, len);
      if (read_status != len)
	{
	  // most likely, it is EOF.
	  break;
	}

      // FFT for terminal time
      windowing (len, left, flag_window, 1.0, time);
      fftw_execute (plan); // FFT: time[] -> freq[]
      for (i = 0; i < len; i ++)
	{
	  l_ft [i] = freq [i];
	}
      windowing (len, right, flag_window, 1.0, time);
      fftw_execute (plan); // FFT: time[] -> freq[]
      for (i = 0; i < len; i ++)
	{
	  r_ft [i] = freq [i];
	}

      if (n == 0)
	{
	  if (hop_in == hop_out)
	    {
	      // then, f_out_old = fs
	      for (i = 0; i < len; i ++)
		{
		  l_f_out_old [i] = l_fs [i];
		  r_f_out_old [i] = r_fs [i];
		}
	    }
	  else
	    {
	      // make f_out_old[] at the frame (out_0)
	      read_status = sndfile_read_at (sf, sfinfo,
					     out_0,
					     left, right, len);
	      if (read_status != len)
		{
		  exit (1);
		}

	      // left channel
	      windowing (len, left, flag_window, 1.0, time);
	      fftw_execute (plan); // FFT: time[] -> freq[]
	      // loose phase lock scheme
	      HC_puckette_lock (len, freq, l_f_out_old);

	      // right channel
	      windowing (len, right, flag_window, 1.0, time);
	      fftw_execute (plan); // FFT: time[] -> freq[]
	      // loose phase lock scheme
	      HC_puckette_lock (len, freq, r_f_out_old);
	    }
	}

      // left channel
      // f_out = X[t] (Y[u(i-1)]/X[x(i)]) / |Y[u(i-1)]/X[x(i)]|
      HC_complex_phase_vocoder (len, l_fs, l_ft, l_f_out_old, f_out);

      // locked coefficients for the next step
      HC_puckette_lock (len, f_out, l_f_out_old);

      // scale -- for safety
      for (i = 0; i < len; i ++)
	{
	  f_out [i] *= 0.5;
	}
      fftw_execute (plan_inv); // iFFT: f_out[] -> t_out[]
      // scale by len and windowing
      windowing (len, t_out, flag_window, (double)len, t_out);
      // superimpose
      for (i = 0; i < len; i ++)
	{
	  l_out [hop_out + i] += t_out [i];
	}

      // right channel
      // f_out = X[t] (Y[u(i-1)]/X[x(i)]) / |Y[u(i-1)]/X[x(i)]|
      HC_complex_phase_vocoder (len, r_fs, r_ft, r_f_out_old, f_out);

      // locked coefficients for the next step
      HC_puckette_lock (len, f_out, r_f_out_old);

      // scale -- for safety
      for (i = 0; i < len; i ++)
	{
	  f_out [i] *= 0.5;
	}
      fftw_execute (plan_inv); // iFFT: f_out[] -> t_out[]
      // scale by len and windowing
      windowing (len, t_out, flag_window, (double)len, t_out);
      // superimpose
      for (i = 0; i < len; i ++)
	{
	  r_out [hop_out + i] += t_out [i];
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

  free (l_f_out_old);
  free (r_f_out_old);

  free (l_fs);
  free (r_fs);
  free (l_ft);
  free (r_ft);

  free (l_out);
  free (r_out);

  sf_close (sf) ;
  if (outfile == NULL) esd_close (esd);
  else
    {
      // frames left in l_out[] and r_out[]
      status = sndfile_write (sfout, sfout_info, l_out, r_out, len);
      out_frames += status;

      // set frames
      sfout_info.frames = out_frames;
      sf_write_sync (sfout);
      sf_close (sfout);
    }
}
