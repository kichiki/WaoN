/* the core of phase vocoder without FFT, for fun (this is not phase vocoder)
 * Copyright (C) 2007 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: pv-nofft.c,v 1.1 2007/10/29 01:17:11 kichiki Exp $
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
#include <stdlib.h> // malloc()
#include <string.h> // memset()
#include <math.h>   // pow()
#include "memory-check.h" // CHECK_MALLOC() macro

#include "pv-complex.h" // struct pv_complex, pv_complex_play_resample()
#include "fft.h"        // windowing()

// libsndfile
#include <sndfile.h>
#include "snd.h" // sndfile_read_at()

// ao device
#include <ao/ao.h>
#include "ao-wrapper.h"


#include "pv-nofft.h"


/* play one hop_syn by the phase vocoder without FFT
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
pv_nofft_play_step (struct pv_complex *pv,
		    long cur)
{
  int n = 0;
  static double *left  = NULL;
  static double *right = NULL;
  if (left == NULL)
    {
      left  = (double *)malloc (sizeof (double) * pv->len);
      right = (double *)malloc (sizeof (double) * pv->len);
      CHECK_MALLOC (left,  "read_and_FFT_stereo");
      CHECK_MALLOC (right, "read_and_FFT_stereo");
      n = pv->len;
    }
  else if (n < pv->len)
    {
      left  = (double *)realloc (left,  sizeof (double) * pv->len);
      right = (double *)realloc (right, sizeof (double) * pv->len);
      CHECK_MALLOC (left,  "read_and_FFT_stereo");
      CHECK_MALLOC (right, "read_and_FFT_stereo");
      n = pv->len;
    }

  // read [cur, cur+len] => left, right [len]
  long status
    = sndfile_read_at (pv->sf, *(pv->sfinfo), cur,
		       left, right, pv->len);
  if (status != pv->len)
    {
      return 0; // no output
    }

  windowing (pv->len, left,  pv->flag_window, pv->window_scale, left);
  windowing (pv->len, right, pv->flag_window, pv->window_scale, right);
  // left, right [len] ==> superimposing out[hop_syn, hop_syn + len]
  int i;
  for (i = 0; i < pv->len; i ++)
    {
      pv->l_out[pv->hop_syn + i] += left[i];
      pv->r_out[pv->hop_syn + i] += right[i];
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


/* phase vocoder by no-FFT -- wrapper for pv_nofft_play_step()
 * INPUT
 *  flag_lock : 0 == no phase lock is applied
 *              1 == loose phase lock is applied
 *  rate : time-streching rate
 *  pitch_shift : in the unit of half-note
 */
void pv_nofft (const char *file, const char *outfile,
	       double rate, double pitch_shift,
	       long len, long hop_syn,
	       int flag_window)
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

  long cur;
  for (cur = 0; cur < (long)sfinfo.frames; cur += pv->hop_ana)
    {
      long len_play = pv_nofft_play_step (pv, cur);
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
