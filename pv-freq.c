/* PV - phase vocoder : pv-freq.c
 * Copyright (C) 2007 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: pv-freq.c,v 1.2 2007/02/16 06:25:09 kichiki Exp $
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

// esd sound device
#include <esd.h>
#include "esd-wrapper.h"


/* phase vocoder by frequency domain
 * only integer rate is working
 */
void pv_freq (const char *file, const char *outfile,
	      double rate, long len, long hop_in,
	      int flag_window)
{
  //double amp_lim = 5.0;
  double amp_lim = 0.0;

  int i;

  long len_out;
  int iscale;

  // open wav file
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

  // now only integer rate is working
  iscale = (int)rate;
  len_out = len * (long)iscale;


  /* esd sound device */
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
  fftw_plan plan;

  time = (double *)fftw_malloc (len * sizeof(double));
  freq = (double *)fftw_malloc (len * sizeof(double));
  plan = fftw_plan_r2r_1d (len, time, freq, FFTW_R2HC, FFTW_ESTIMATE);

  double *t_out = NULL;
  double *f_out = NULL;
  fftw_plan plan_inv;

  f_out = (double *)fftw_malloc (len_out * sizeof(double));
  t_out = (double *)fftw_malloc (len_out * sizeof(double));
  plan_inv = fftw_plan_r2r_1d (len_out, f_out, t_out,
			       FFTW_HC2R, FFTW_ESTIMATE);


  double *l_out = NULL;
  double *r_out = NULL;
  l_out = (double *)malloc (len_out * sizeof(double));
  r_out = (double *)malloc (len_out * sizeof(double));

  double *phs = NULL;
  double *amp = NULL;
  phs = (double *)malloc (((len/2)+1) * sizeof(double));
  amp = (double *)malloc (((len/2)+1) * sizeof(double));

  double *p_out = NULL;
  double *a_out = NULL;
  p_out = (double *)malloc (((len/2)+1) * sizeof(double));
  a_out = (double *)malloc (((len/2)+1) * sizeof(double));


  int step = 0;
  while (sndfile_read (sf, sfinfo, left, right, len) != 0)
    {
      // left channel
      apply_FFT (len, left, flag_window, plan, time, freq, 0.5, amp, phs);

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
      apply_FFT (len, right, flag_window, plan, time, freq, 0.5, amp, phs);

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
	  status = esd_write (esd, l_out, r_out, len_out);
	}
      else
	{
	  status = sndfile_write (sfout, sfout_info, l_out, r_out, len_out);
	  out_frames += status;
	}

      step ++;
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
