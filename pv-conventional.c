/* PV - phase vocoder : pv-conventional.c
 * Copyright (C) 2007 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: pv-conventional.c,v 1.5 2007/02/25 03:36:46 kichiki Exp $
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


/** general utility routines for pv **/

/*
 * INPUT
 *  hop_in :
 *  hop_out :
 *  l_out [hop_out] :
 *  r_out [hop_out] :
 *  ao, sfout, sfout_info : properties for output
 *  flag_pitch :
 */
int
pv_play_resample (long hop_in, long hop_out,
		  double *l_out, double *r_out,
		  ao_device *ao, SNDFILE *sfout, SF_INFO *sfout_info,
		  int flag_pitch)
{
  int status = 0;

  int i;


  // samplerate conversion
  float *fl_in  = NULL;
  float *fl_out = NULL;
  double *l_out_src = NULL;
  double *r_out_src = NULL;
  SRC_DATA srdata;

  if (flag_pitch != 0)
    {
      fl_in  = (float *)malloc (sizeof (float) * 2 * hop_out);
      fl_out = (float *)malloc (sizeof (float) * 2 * hop_in);

      srdata.input_frames  = hop_out;
      srdata.output_frames = hop_in;
      srdata.src_ratio = (double)hop_in / (double)hop_out;
      srdata.data_in  = fl_in;
      srdata.data_out = fl_out;

      l_out_src = (double *)malloc (sizeof (double) * hop_in);
      r_out_src = (double *)malloc (sizeof (double) * hop_in);


      // samplerate conversion (time fixed)
      for (i = 0; i < hop_out; i ++)
	{
	  fl_in [i*2 + 0] = (float)l_out [i];
	  fl_in [i*2 + 1] = (float)r_out [i];
	}
      status = src_simple (&srdata, SRC_SINC_BEST_QUALITY, 2);
      if (status != 0)
	{
	  fprintf (stderr, "fail to samplerate conversion\n");
	  exit (1);
	}
      for (i = 0; i < hop_in; i ++)
	{
	  l_out_src [i] = (double)fl_out [i*2 + 0];
	  r_out_src [i] = (double)fl_out [i*2 + 1];
	}

      // output
      if (sfout == NULL)
	{
	  status = ao_write (ao, l_out_src, r_out_src, hop_in);
	  status /= 4; // 2 bytes for 2 channels
	}
      else
	{
	  status = sndfile_write (sfout, *sfout_info,
				  l_out_src, r_out_src, hop_in);
	}

      free (fl_in);
      free (fl_out);

      free (l_out_src);
      free (r_out_src);
    }
  else
    {
      // no samplerate conversion (pitch fixed)
      // output
      if (sfout == NULL)
	{
	  status = ao_write (ao, l_out, r_out, hop_out);
	  status /= 4; // 2 bytes for 2 channels
	}
      else
	{
	  status = sndfile_write (sfout, *sfout_info,
				  l_out, r_out, hop_out);
	}
    }

  return (status);
}


/* standard phase vocoder
 * Ref: J.Laroche and M.Dolson (1999)
 */
void pv (const char *file, const char *outfile,
	 double rate, long len, long hop_out,
	 int flag_window,
	 int flag_pitch)
{
  long hop_in;
  hop_in = (long)((double)hop_out * rate);


  double twopi = 2.0 * M_PI;

  int i;
  int k;

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

  double *amp = NULL;
  double *ph_in = NULL;
  amp = (double *)malloc (((len/2)+1) * sizeof(double));
  ph_in     = (double *)malloc (((len/2)+1) * sizeof(double));

  double *l_ph_out = NULL;
  double *r_ph_out = NULL;
  l_ph_out    = (double *)malloc (((len/2)+1) * sizeof(double));
  r_ph_out    = (double *)malloc (((len/2)+1) * sizeof(double));

  double *l_ph_in_old = NULL;
  double *r_ph_in_old = NULL;
  l_ph_in_old = (double *)malloc (((len/2)+1) * sizeof(double));
  r_ph_in_old = (double *)malloc (((len/2)+1) * sizeof(double));
  for (i = 0; i < (len/2)+1; i ++)
    {
      ph_in [i]  = 0.0;
      l_ph_out [i] = 0.0;
      r_ph_out [i] = 0.0;
      l_ph_in_old [i] = 0.0;
      r_ph_in_old [i] = 0.0;
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
  double * omega = NULL;
  omega = (double *) malloc (((len/2)+1) * sizeof(double));
  for (k = 0; k < (len/2)+1; k ++)
    {
      omega [k] = twopi * (double)k / (double)len;
    }


  // read the first frame
  read_status = sndfile_read (sf, sfinfo, left, right, len);
  if (read_status != len)
    {
      exit (1);
    }

  int flag_ph = 0;
  for (;;)
    {
      // left channel
      apply_FFT (len, left, flag_window, plan, time, freq, 0.5, amp, ph_in);
      if (flag_ph == 0)
	{
	  // initialize phase
	  for (k = 0; k < (len/2)+1; k ++)
	    {
	      l_ph_out [k] = ph_in [k] * (double)hop_out / (double)hop_in;
	      //l_ph_out [k] = ph_in [k];

	      // backup for the next step
	      l_ph_in_old [k] = ph_in [k];
	    }
	  //flag_ph = 1; // right channel is in the following!
	}
      else
	{
	  // only for imag components who have phase
	  for (k = 1; k < ((len+1)/2); k ++)
	    {
	      double dphi;
	      dphi = ph_in [k] - l_ph_in_old [k]
		- omega [k] * (double)hop_in;
	      for (; dphi >= M_PI; dphi -= twopi);
	      for (; dphi < -M_PI; dphi += twopi);

	      l_ph_out [k] += dphi * (double)hop_out / (double)hop_in
		+ omega [k] * (double)hop_out;

	      l_ph_in_old [k] = ph_in [k];
	    }
	}
      polar_to_HC (len, amp, l_ph_out, 0, f_out);
      fftw_execute (plan_inv);
      // scale by len and windowing
      windowing (len, t_out, flag_window, (double)len, t_out);
      // superimpose
      for (i = 0; i < len; i ++)
	{
	  l_out [hop_out + i] += t_out [i];
	}

      // right channel
      apply_FFT (len, right, flag_window, plan, time, freq, 0.5, amp, ph_in);
      if (flag_ph == 0)
	{
	  // initialize phase
	  for (k = 0; k < (len/2)+1; k ++)
	    {
	      r_ph_out [k] = ph_in [k] * (double)hop_out / (double)hop_in;
	      //r_ph_out [k] = ph_in [k];

	      // backup for the next step
	      r_ph_in_old [k] = ph_in [k];
	    }
	  flag_ph = 1;
	}
      else
	{
	  // only for imag components who have phase
	  for (k = 1; k < ((len+1)/2); k ++)
	    {
	      double dphi;
	      dphi = ph_in [k] - r_ph_in_old [k]
		- omega [k] * (double)hop_in;
	      for (; dphi >= M_PI; dphi -= twopi);
	      for (; dphi < -M_PI; dphi += twopi);

	      r_ph_out [k] += dphi * (double)hop_out / (double)hop_in
		+ omega [k] * (double)hop_out;

	      r_ph_in_old [k] = ph_in [k];
	    }
	}
      polar_to_HC (len, amp, r_ph_out, 0, f_out);
      fftw_execute (plan_inv);
      // scale by len and windowing
      windowing (len, t_out, flag_window, (double)len, t_out);
      // superimpose
      for (i = 0; i < len; i ++)
	{
	  r_out [hop_out + i] += t_out [i];
	}


      // output
      status = pv_play_resample (hop_in, hop_out, l_out, r_out,
				 ao, sfout, &sfout_info,
				 flag_pitch);


      /* shift acc_out by hop_out */
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


      /* for the next step */
      for (i = 0; i < (len - hop_in); i ++)
	{
	  left  [i]  = left  [i + hop_in];
	  right  [i] = right  [i + hop_in];
	}

      /* read next segment */
      read_status = sndfile_read (sf, sfinfo,
				  left  + len - hop_in,
				  right + len - hop_in,
				  hop_in);
      if (read_status != hop_in)
	{
	  // most likely, it is EOF.
	  break;
	}
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

  free (amp);
  free (ph_in);

  free (l_ph_out);
  free (r_ph_out);
  free (l_ph_in_old);
  free (r_ph_in_old);

  free (l_out);
  free (r_out);

  free (omega);
}
