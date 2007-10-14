/* PV - phase vocoder : pv-freq.c
 * Copyright (C) 2007 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: pv-freq.c,v 1.8 2007/10/14 06:26:09 kichiki Exp $
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

#include "memory-check.h" // CHECK_MALLOC() macro


/* phase vocoder by frequency domain
 * only integer rate is working
 */
void pv_freq (const char *file, const char *outfile,
	      double rate, long len, long hop_syn,
	      int flag_window)
{
  //double amp_lim = 5.0;
  double amp_lim = 0.0;

  int i;

  long len_out;
  int iscale;

  // open the input file
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
  CHECK_MALLOC (left,  "pv_freq");
  CHECK_MALLOC (right, "pv_freq");

  // now only integer rate is working
  iscale = (int)rate;
  len_out = len * (long)iscale;


  // prepare the output
  int status;
  ao_device *ao = NULL;
  SNDFILE *sfout = NULL;
  SF_INFO sfout_info;
  if (outfile == NULL)
    {
      ao = ao_init_16_stereo (sfinfo.samplerate, 1 /* verbose */);
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


  /* initialization plan for FFTW  */
  double *time = NULL;
  double *freq = NULL;
  time = (double *)fftw_malloc (len * sizeof(double));
  freq = (double *)fftw_malloc (len * sizeof(double));
  CHECK_MALLOC (time, "pv_freq");
  CHECK_MALLOC (freq, "pv_freq");
  fftw_plan plan;
  plan = fftw_plan_r2r_1d (len, time, freq, FFTW_R2HC, FFTW_ESTIMATE);

  double *t_out = NULL;
  double *f_out = NULL;
  f_out = (double *)fftw_malloc (len_out * sizeof(double));
  t_out = (double *)fftw_malloc (len_out * sizeof(double));
  CHECK_MALLOC (f_out, "pv_freq");
  CHECK_MALLOC (t_out, "pv_freq");
  fftw_plan plan_inv;
  plan_inv = fftw_plan_r2r_1d (len_out, f_out, t_out,
			       FFTW_HC2R, FFTW_ESTIMATE);


  double *l_out = NULL;
  double *r_out = NULL;
  l_out = (double *)malloc (len_out * sizeof(double));
  r_out = (double *)malloc (len_out * sizeof(double));
  CHECK_MALLOC (l_out, "pv_freq");
  CHECK_MALLOC (r_out, "pv_freq");

  double *phs = NULL;
  double *amp = NULL;
  phs = (double *)malloc (((len/2)+1) * sizeof(double));
  amp = (double *)malloc (((len/2)+1) * sizeof(double));
  CHECK_MALLOC (phs, "pv_freq");
  CHECK_MALLOC (amp, "pv_freq");

  double *p_out = NULL;
  double *a_out = NULL;
  p_out = (double *)malloc (((len/2)+1) * sizeof(double));
  a_out = (double *)malloc (((len/2)+1) * sizeof(double));
  CHECK_MALLOC (p_out, "pv_freq");
  CHECK_MALLOC (a_out, "pv_freq");


  int step = 0;
  while (sndfile_read (sf, sfinfo, left, right, len) != 0)
    {
      // left channel
      apply_FFT (len, left, flag_window, plan, time, freq, 2.0, amp, phs);

      // cut signal of amp < amp_lim
      for (i = 0; i < (len/2)+1; i ++)
	{
	  if (amp [i] < amp_lim) amp [i] = 0.0;
	}

      // prepare f_out[]; now iscale = 2
      polar_to_HC_scale (len, amp, phs, 0, iscale, f_out);
      fftw_execute (plan_inv); // f_out[] -> t_out[]
      for (i = 0; i < len_out; i ++)
	{
	  l_out [i] = t_out [i] / (double)len;
	}


      // right channel
      apply_FFT (len, right, flag_window, plan, time, freq, 2.0, amp, phs);

      // cut signal of amp < amp_lim
      for (i = 0; i < (len/2)+1; i ++)
	{
	  if (amp [i] < amp_lim) amp [i] = 0.0;
	}

      // prepare f_out[]; now iscale = 2
      polar_to_HC_scale (len, amp, phs, 0, iscale, f_out);
      fftw_execute (plan_inv); // f_out[] -> t_out[]
      for (i = 0; i < len_out; i ++)
	{
	  r_out [i] = t_out [i] / (double)len;
	}


      // output
      if (outfile == NULL)
	{
	  status = ao_write (ao, l_out, r_out, hop_syn);
	  status /= 4; // 2 bytes for 2 channels
	}
      else
	{
	  status = sndfile_write (sfout, sfout_info, l_out, r_out, len_out);
	}

      step ++;
    }


  sf_close (sf);
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

  free (l_out);
  free (r_out);

  free (phs);
  free (amp);

  free (p_out);
  free (a_out);
}
