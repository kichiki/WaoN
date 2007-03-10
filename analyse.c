/* routines to analyse power spectrum and output notes
 * Copyright (C) 1998-2007 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: analyse.c,v 1.7 2007/03/10 20:52:34 kichiki Exp $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h> // fprintf()
#include <stdlib.h> // malloc()
#include <string.h> // memset()
#include <math.h> // log10()
#include <sys/errno.h> // errno

/* FFTW library  */
#ifdef FFTW2
#include <rfftw.h>
#else // FFTW3
#include <fftw3.h>
#endif // FFTW2

// libsndfile
#include <sndfile.h>
#include "snd.h"

#include "midi.h" /* get_note()  */
#include "fft.h" /* init_den(), power_spectrum_fftw() */
#include "analyse.h"

#include "memory-check.h" // CHECK_MALLOC() macro


/* global variables  */
int abs_flg; /* flag for absolute/relative cutoff  */
int patch_flg; /* flag for using patch file  */
double *pat; /* work area for patch  */
int npat; /* # of data in pat[]  */
double p0; /* maximum power  */
double if0; /* freq point of maximum  */
int peak_threshold; /* to select peaks in a note  */


/** for stage 2 : note selection process **/

/* get intensity of notes from power spectrum
 * INPUT
 *  p[]              : power spectrum
 *  fp[]             : frequencies for each bin
 *                     if NULL, we use the center frequencies
 *  cut_ratio        : log10 of cutoff ratio to scale velocity
 *  rel_cut_ratio    : log10 of cutoff ratio relative to average
 *                     0 means cutoff is equal to average
 *  (global)abs_flg  : 0 for relative, 1 for absolute
 *  i0, i1           : considering frequency range
 *  (global)patch_flg: whether patch is used or not.
 * OUTPUT
 *  intens[128]      : intensity [0,128) for each midi note
 */
void
note_intensity (double *p, double *fp,
		double cut_ratio, double rel_cut_ratio,
		int i0, int i1,
		double t0, char *intens)
{
  extern int patch_flg; /* flag for using patch file  */
  extern int abs_flg; /* flag for absolute/relative cutoff  */

  int i;
  int imax;
  double max;
  double x;
  double freq; /* freq of peak in power  */
  double f;
  int in;
  double av;

  // clear
  for (i = 0; i < 128; i++)
    {
      intens[i] = 0;
    }

  // calc average power
  if (abs_flg == 0)
    {
      av = 0.0;
      for (i = i0; i < i1; i++)
	{
	  av += p[i];
	}
      av /= (double)(i1 - i0);
    }
  else
    {
      av = 1.0;
    }

  for (;;)
    {
      // search peak
      // set the threshold to the average
      if (abs_flg == 0)
	{
	  max = av * pow (10.0, rel_cut_ratio);
	}
      else
	{
	  max = pow (10.0, cut_ratio);
	}

      imax = -1;
      for (i = i0; i < i1; i++)
	{
	  if (p[i] > max)
	    {
	      max = p[i];
	      imax = i;
	    }
	}

      if (imax == -1) // no peak found
	break;

      // get midi note # from imax (FFT freq index)
      if (fp == NULL)
	{
	  freq = (double)imax / t0;
	}
      else
	{
	  freq = fp [imax];
	  //fprintf (stderr, "freq = %f, %f\n", freq, (double)imax / t0);
	}
      in = get_note (freq); // midi note #
      // check  the range of the note
      if (in >= i0 && in <= i1)
	{
	  // if second time on same note, skip
	  if (intens[in] == 0)
	    {
	      /* scale intensity (velocity) of the peak
	       * power range from 10^cut_ratio to 10^0 is scaled  */
	      x = 127.0 / (double)(-cut_ratio)
		* (log10 (p[imax]) - (double) cut_ratio);
	      if (x >= 128.0)
		{
		  intens[in] = 127;
		}
	      else if (x > 0)
		{
		  intens[in] = (int)x;
		}
	    }
	}

      // subtract peak upto minimum in both sides
      if (patch_flg == 0)
	{
	  p[imax] = 0.0;
	  // right side
	  for (i = imax+1;
	      p[i] != 0.0 && i < (i1-1) && p[i] >= p[i+1];
	      i++)
	    p[i] = 0.0;
	  if (i == i1-1)
	    p[i] = 0.0;
	  // left side
	  for (i = imax-1;
	       p[i] != 0.0 && i > i0 && p[i-1] <= p[i];
	       i--)
	    p[i] = 0.0;
	  if (i == i0)
	    p[i] = 0.0;
	}
      else
	{
	  for (i = i0; i < i1; i++)
	    {
	      if (fp == NULL)
		{
		  f = (double)i / t0;
		}
	      else
		{
		  f = fp [i];
		}
	      p[i] -= max * patch_power (f/freq);
	      if (p[i] <0)
		{
		  p[i] = 0;
		}
	    }
	}
    }
}


/*
 * INPUT
 *  amp2 [(len/2)+1] : power spectrum (amp^2)
 *  dphi [(len/2)+1] : PV freq correction factor defined by
 *                     (1/2pi hop)principal(phi - phi0 - Omega),
 *                     therefore, the corrected freq is
 *                     (k/len + dphi[k])*samplerate [Hz].
 *                     give NULL for plain FFT power spectrum.
 * OUTPUT
 *  ave2 [128] : averaged amp2 for each midi note
 */
void
average_FFT_into_midi (int len, double samplerate,
		       const double *amp2, const double *dphi,
		       double *ave2)
{
  int k;
  int midi;
  double f;
  int *n = NULL;
  n = (int *) malloc (sizeof (int) * 128);
  CHECK_MALLOC (n, "average_FFT_into_midi");

  for (midi = 0; midi < 128; midi ++)
    {
      ave2 [midi] = 0.0;
      n [midi] = 0;
    }

  for (k = 1; k < (len+1)/2; k ++)
    {
      // corrected frequency
      if (dphi == NULL)
	{
	  f = (double)k / (double)len * samplerate;
	}
      else
	{
	  f = ((double)k / (double)len + dphi [k]) * samplerate;
	}

      midi = freq_to_midi (f);
      if (midi >= 0 && midi < 128)
	{
	  ave2 [midi] += sqrt (amp2 [k]);
	  n [midi] ++;
	}
    }

  // average and square
  for (midi = 0; midi < 128; midi ++)
    {
      if (n [midi] > 0)
	{
	  ave2 [midi] = ave2 [midi] / (double)n [midi]; // average
	  ave2 [midi] = ave2 [midi] * ave2 [midi]; // square
	}
    }

  free (n);
}

/* pickup notes and its power from a table of power for each midi note
 * INPUT
 *  amp2midi [128]  : amp^2 for each midi note
 *  cut_ratio       : log10 of cutoff ratio to scale velocity
 *  rel_cut_ratio   : log10 of cutoff ratio relative to average
 *                    0 means cutoff is equal to average
 *  i0, i1          : considering midi note range (NOT FREQUENCY INDEX!!)
 *  (global)abs_flg : 0 for relative, 1 for absolute
 * OUTPUT
 *  intens[]        : with 127 elements (# of notes)
 */
void
pickup_notes (double *amp2midi,
	      double cut_ratio, double rel_cut_ratio,
	      int i0, int i1,
	      char *intens)
{
  extern int abs_flg; /* flag for absolute/relative cutoff  */

  //double oct_fac = 0.5; // octave harmonics factor
  double oct_fac = 0.0;

  int i;
  int imax;
  double max;
  double x;
  int in;
  double av;

  // clear
  for (i = 0; i < 128; i++)
    {
      intens[i] = 0;
    }

  // calc average power
  if (abs_flg == 0)
    {
      av = 0.0;
      for (i = i0; i < i1; i++)
	{
	  av += amp2midi[i];
	}
      av /= (double)(i1 - i0);
    }
  else
    {
      av = 1.0;
    }

  for (;;)
    {
      // search peak
      // set the threshold to the average
      if (abs_flg == 0)
	{
	  max = av * pow (10.0, rel_cut_ratio);
	}
      else
	{
	  max = pow (10.0, cut_ratio);
	}

      imax = -1;
      for (i = i0; i < i1; i++)
	{
	  if (amp2midi[i] > max)
	    {
	      max = amp2midi[i];
	      imax = i;
	    }
	}

      if (imax == -1) // no peak found
	break;

      // so that imax is THE midi note
      in = imax;

      // if second time on same note, skip
      if (intens[in] == 0)
	{
	  /* scale intensity (velocity) of the peak  */
	  /* power range from 10^cut_ratio to 10^0 is scaled  */
	  x = 127.0 / (double)(-cut_ratio)
	    * (log10 (amp2midi[in]) - (double) cut_ratio);
	  if (x >= 128.0)
	    {
	      intens[in] = 127;
	    }
	  else if (x > 0)
	    {
	      intens[in] = (int)x;
	    }

	  // octave harmonics reduction
	  if (oct_fac > 0.0)
	    {
	      for (i = in + 12; in < 128; in += 12)
		{
		  amp2midi[i] = sqrt (amp2midi[i])
		    - oct_fac * sqrt (amp2midi[i-12]);
		  if (amp2midi[i] < 0.0) amp2midi[i] = 0.0;
		  else amp2midi[i] = amp2midi[i] * amp2midi[i];
		}
	    }
	}

      // subtract the peak bin
      amp2midi[imax] = 0.0;
    }
}


/** for stage 3 : time-difference check for note-on/off **/

/* check note on and off comparing intens[] now and last times
 * INPUT
 *  icnt : counter (present time)
 *  i_lsts[128] : intensity matrix
 *  on_lst[] : on-note list at last step
 *  *notes : ia_note pointer
 * OUTPUT
 *  nmidi : # of midi event
 *  num : current pointer of ia_note within a segment
 * RETURN VALUE
 *  current ia_note pointer
 */
struct ia_note *
chk_note_on_off (int icnt, char i_lsts[], char * on_lst[],
		 struct ia_note *notes, int *num, int *nmidi)
{
  const int on_threshold = 8;
  const int off_threshold = 0;
  extern int peak_threshold; /* to select peaks in a note  */
  int i;
  struct note_sig *cur_note;

  *nmidi = 0;
  for (i=0; i<128; i++) /* present step  */
    {
      if (on_lst[i] == NULL) /* off at last step  */
	{
	  /* search note on   */
	  if (i_lsts[i] > on_threshold)
	    {
	      /* on  */
	      cur_note = &(notes->note[*num]);
	      cur_note->step = icnt;
	      cur_note->sig = 1; /* on  */
	      cur_note->note = i;
	      cur_note->intensity = i_lsts[i];
	      on_lst[i] = &(cur_note->intensity);
	      (*nmidi) ++;
	      notes = incr_ia_note (notes, num);
	    }
	}
      else /* on at last step  */
	{
	  /* search note off -- off_threshold  */
	  if (i_lsts[i] <= off_threshold) /* off at last step  */
	    {
	      /* off  */
	      cur_note = &(notes->note[*num]);
	      cur_note->step = icnt;
	      cur_note->sig = 0; /* off  */
	      cur_note->note = i;
	      cur_note->intensity = 64;
	      on_lst[i] = NULL; /* off  */
	      (*nmidi) ++;
	      notes = incr_ia_note (notes, num);
	    }
	  else /* now note is over off_threshold at least  */
	    {
	      if (i_lsts[i] >= (*on_lst[i] + peak_threshold))
		/* first, search peak  */
		{
		  /* off  */
		  cur_note = &(notes->note[*num]);
		  cur_note->step = icnt;
		  cur_note->sig = 0; /* off  */
		  cur_note->note = i;
		  cur_note->intensity = 64;
		  (*nmidi) ++;
		  notes = incr_ia_note (notes, num);
		  /* on  */
		  cur_note = &(notes->note[*num]);
		  cur_note->step = icnt;
		  cur_note->sig = 1; /* on  */
		  cur_note->note = i;
		  cur_note->intensity = i_lsts[i];
		  on_lst[i] = &(cur_note->intensity);
		  (*nmidi) ++;
		  notes = incr_ia_note (notes, num);
		}
	      else if (i_lsts[i] > (*on_lst[i]))
		/* then, other larger note...  */
		{
		  /* overwrite velocity  */
		  *on_lst[i] = i_lsts[i];
		}
	    }
	}
    }
  return notes;
}


/* return power of patch relative to its maximum
 * at the freqency where the ratio to the maximum is 'freq_ratio'
 */
double
patch_power (double freq_ratio)
{
  extern double *pat;
  extern int npat; /* # of data in pat[]  */
  extern double p0; /* maximum power  */
  extern double if0; /* freq point of maximum  */
  
  int i0, i1;
  double dpdf;
  double f;
  double p;

  f = (double)if0 * freq_ratio;
  i0 = (int)f;
  i1 = i0 + 1;

  if (i0 < 1 || i1 > npat)
    return 0.0;
  dpdf = pat[i1] - pat[i0];
  p = pat[i0] + dpdf * (f - (double)i0);
  return (p/p0);
}

/* initialize patch
 * INPUT
 *   file_patch : filename
 *   plen : # of data in patch (wav)
 *   nwin : index of window
 * OUTPUT (extern values)
 *   pat[] : power of pat
 *   npat : # of data in pat[] ( = plen/2 +1 )
 *   p0 : maximun of power
 *   if0 : freq point of maximum
 */
void
init_patch (char *file_patch, int plen, int nwin)
{
  extern int patch_flg;
  extern double *pat;
  extern int npat; /* # of data in pat[]  */
  extern double p0; /* maximum power  */
  extern double if0; /* freq point of maximum  */
  
  int i;


  /* prepare patch  */
  if (file_patch == NULL)
    {
      patch_flg = 0;
      return;
    }
  else
    {
      /* allocate pat[]  */
      pat = (double *)malloc (sizeof (double) * (plen/2+1));
      if (pat == NULL)
	{
	  fprintf(stderr, "cannot allocate pat[%d]\n", (plen/2+1));
	  patch_flg = 0;
	  return;
	}

      double *x = NULL;
      double *xx = NULL;
      x  = (double *)malloc (sizeof (double) * plen);
      xx = (double *)malloc (sizeof (double) * plen);
      if (x == NULL || xx == NULL)
	{
	  fprintf(stderr, "cannot allocate x[%d]\n", plen);
	  patch_flg = 0;
	  return;
	}

      /* spectrum data for FFT */ 
      double *y = NULL;
      y = (double *)malloc (sizeof (double) * plen);
      if (y == NULL)
	{
	  fprintf(stderr, "cannot allocate y[%d]\n", plen);
	  patch_flg = 0;
	  free (x);
	  free (xx);
	  return;
	}

      /* open patch file  */
      SNDFILE *sf = NULL;
      SF_INFO sfinfo;
      sf = sf_open (file_patch, SFM_READ, &sfinfo);
      if (sf == NULL)
	{
	  fprintf (stderr, "Can't open patch file %s : %s\n",
		   file_patch, strerror (errno));
	  exit (1);
	}

      /* read patch wav  */
      if (sndfile_read (sf, sfinfo, x, xx, plen) != plen)
	{
	  fprintf (stderr, "No Patch Data!\n");
	  patch_flg = 0;
	  free (x);
	  free (xx);
	  free (y);
	  return;
	}
      if (sfinfo.channels == 2)
	{
	  for (i = 0; i < plen; i ++)
	    {
	      x[i] = 0.5 * (x[i] + xx[i]);
	    }
	}

      /* calc power of patch  */
      double den;
      den = init_den (plen, nwin);

#ifdef FFTW2
      rfftw_plan plan;
      plan = rfftw_create_plan (plen, FFTW_REAL_TO_COMPLEX, FFTW_ESTIMATE);
#else
      fftw_plan plan;
      plan = fftw_plan_r2r_1d (plen, x, y, FFTW_R2HC, FFTW_ESTIMATE);
#endif /* FFTW2 */

      power_spectrum_fftw (plen, x, y, pat, den, nwin, plan);
      fftw_destroy_plan (plan);

      free (x);
      free (xx);
      free (y);
      sf_close (sf);

      /* search maximum  */
      p0 = 0.0;
      if0 = -1;
      for (i=0; i<plen/2; i++)
	{
	  if (pat[i] > p0)
	    {
	      p0 = pat[i];
	      if0 = i;
	    }
	}
      if (if0 == -1)
	patch_flg = 0;

      npat = plen/2;
      patch_flg = 1;
    }
}

/* initialize infinite array and return its pointer
 *   concept of 'infinite array' is described on "Practical C Programming"
 *   3rd Ed. (1997) by Steve Oualline in Chp. 18. as an example.
 */
struct ia_note *
init_ia_note (void)
{
  struct ia_note *new;
  new = (struct ia_note *)malloc (sizeof (struct ia_note));
  if (new == NULL)
    {
      fprintf (stderr, "cannot allocate ia_note\n");
    }
  else
    {
      memset (new, '\0', sizeof (struct ia_note));
      new->next = NULL;
    }
  return new;
}

/* append infinite array 'last' and return its pointer
 * INPUT & OUTPUT
 *   last : new pointer if succeeded, NULL if failed
 */
struct ia_note *
append_ia_note (struct ia_note *last)
{
  struct ia_note *new;

  if (last->next == NULL)
    {
      new = (struct ia_note *)malloc (sizeof (struct ia_note));
      last->next = new;
      if (new == NULL)
	{
	  fprintf (stderr, "cannot allocate ia_note\n");
	}
      else
	{
	  memset (new, '\0', sizeof (struct ia_note));
	  new->next = NULL;
	}
    }
  else
    new = last->next;

  return new;
}

/* increment infinite array
 * INPUT
 *   last : current pointer of ia_note
 *   num : current pointer within a segment
 * OUTPUT
 *   RETURN VALUE : next pointer of ia_note
 *   num : next pointer within a segment
 */
struct ia_note *
incr_ia_note (struct ia_note *last, int *num)
{
  struct ia_note * new;
  (*num)++;
  if ((*num) >= BLOCK_SIZE)
    {
      new = append_ia_note (last);
      if (new != NULL)
	(*num) = 0;
      return new;
    }
  return last;
}
