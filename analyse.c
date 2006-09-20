/* routines to analyse power spectrum and output notes
 * Copyright (C) 1998 Kengo ICHIKI (ichiki@geocities.com)
 * $Id: analyse.c,v 1.1 2006/09/20 21:26:44 kichiki Exp $
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

#include <stdio.h> /* fprintf()  */
#include <stdlib.h> /* malloc()  */
#include <fcntl.h> /* open()  */
#include <unistd.h> /* lseek()  */
#include <sys/stat.h> /* S_IRUSR, S_IWUSR  */
#include <string.h> /* strncmp()  */
#include <math.h> /* log10()  */
#include <rfftw.h> /* FFTW library  */

#include "sox-wav.h" /* ft_t, wavstartread(), wavread()  */
#include "midi.h" /* get_note()  */
#include "fft.h" /* init_den(), power_spectrum_fftw() */
#include "analyse.h"

/* global variables  */
int abs_flg; /* flag for absolute/relative cutoff  */
int patch_flg; /* flag for using patch file  */
double *pat; /* work area for patch  */
int npat; /* # of data in pat[]  */
double p0; /* maximum power  */
double if0; /* freq point of maximum  */
int peak_threshold; /* to select peaks in a note  */


/* get intensity of notes from power spectrum
 * INPUT
 *  n : number of spectrum frequencies (N/2, N is # of samples in time  )
 *  p[] : power spectrum
 *  cut_ratio : log10 of cutoff ratio to scale velocity
 *  rel_cut_ratio : log10 of cutoff ratio relative to average
 *                  0 means cutoff is equal to average
 *  i0, i1 : considering frequency range
 *  pat[] : power spectrum of patch
 * OUTPUT
 *  intens[] : with 127 elements (# of notes)
 */
void
note_intensity (int n, double p[], double cut_ratio, double rel_cut_ratio,
		int i0, int i1,
		double t0, char intens[])
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

  for (i=0; i<128; i++)
    intens[i] = 0;

  /* calc average power  */
  if (abs_flg == 0)
    {
      av = 0.0;
      for (i=i0; i<i1; i++)
	av += p[i];
      av /= (double)(i1 - i0);
    }
  else
    av = 1.0;

  for (;;)
    {
      /* search max  */
      if (abs_flg == 0)
	max = av * pow (10.0, rel_cut_ratio);
      else
	max = pow (10.0, cut_ratio);
      imax = -1;
      for (i=i0; i<i1; i++)
	{
	  if (p[i] > max)
	    {
	      max = p[i];
	      imax = i;
	    }
	}
      if (imax == -1) /* no maximum  */
	break;

      /* get midi note # from imax (FFT freq index)  */
      freq = (double)imax/t0; /* freq of maximum  */
      in = get_note (freq); /* midi note #  */

      if (intens[in] == 0) /* if second time on same note, skip  */
	{
	  /* scale intensity (velocity) of the peak  */
	  /* power range from 10^cut_ratio to 10^0 is scaled  */
	  x = 127.0/(double)(-cut_ratio)
	    * (log10 (p[imax]) - (double) cut_ratio);
	  if (x >= 128.0)
	    intens[in] = 127;
	  else if (x > 0)
	    intens[in] = (int)x;
	}

      /* subtract peak upto minimum in both sides  */
      if (patch_flg == 0)
	{
	  p[imax] = 0.0;
	  /* right side  */
	  for (i=imax+1;
	      p[i] != 0.0 && i<(i1-1) && p[i]>=p[i+1];
	      i++)
	    p[i] = 0.0;
	  if (i == i1-1)
	    p[i] = 0.0;
	  /* left side  */
	  for (i=imax-1;
	      p[i] != 0.0 && i>i0 && p[i-1]<=p[i];
	      i--)
	    p[i] = 0.0;
	  if (i == i0)
	    p[i] = 0.0;
	}
      else
	{
	  for (i=i0; i<i1; i++)
	    {
	      f = (double)i/t0; /* freq of maximum  */
	      p[i] -= max * patch_power (f/freq);
	      if (p[i] <0)
		p[i] = 0;
	    }
	}
    }
}

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

/* MIDI output of data note_on_off[]
 * INPUT
 *  num : # of event (on/off)
 *  *note_on_off : struct of note signal
 *  div : divisioin
 *  filename : filename of output midi file
 */
void
output_midi (int nmidi, struct ia_note *notes, double div, char *filename)
{
  int fd; /* file descriptor of output midi file  */
  char stdout_flg;
  int i;
  int idt;
  int p_midi;
  int n_midi;
  int h_midi; /* pointer of track header  */
  int dh_midi; /* pointer of data head  */
  int num; /* index of ia_note within a segment  */

  struct ia_note *next;
  struct note_sig *cur_note;
  int last_step;

  /* file open */
  if (strncmp (filename, "-", strlen (filename)) == 0)
    {
      fd = fcntl(STDOUT_FILENO, F_DUPFD, 0);
      stdout_flg = 1;
    }
  else
    {
      fd = open (filename, O_RDWR| O_CREAT| O_TRUNC, S_IRUSR| S_IWUSR);
      stdout_flg = 0;
    }
  if (fd < 0)
    {
      fprintf (stderr, "cannot open %s\n", filename);
      exit (1);
    }
  /* MIDI header */
  p_midi = 0;
  n_midi = smf_header_fmt (fd, 0, 1, div);
  if (n_midi != 14)
    {
      fprintf (stderr, "Error duing writing mid! %d (header)\n", p_midi);
      return;
    }
  p_midi += n_midi;

  h_midi = p_midi; /* pointer of track-head  */
  n_midi = smf_track_head (fd, (7+4*nmidi));
  if (n_midi != 8)
    {
      fprintf (stderr, "Error duing writing mid! %d (track header)\n", p_midi);
      return;
    }
  p_midi += n_midi;

  dh_midi = p_midi; /* head of data  */
  /* ch.0 prog. 0  */
  n_midi = smf_prog_change (fd, 0, 0);
  if (n_midi != 3)
    {
      fprintf (stderr, "Error duing writing mid! %d (prog change)\n", p_midi);
      return;
    }
  p_midi += n_midi;

  last_step=0; /* delta time  */
  for (i=0, num=0; i<nmidi; i++, num++)
    {
      if (num >= BLOCK_SIZE)
	{
	  next = notes->next;
	  if (next == NULL)
	    {
	      fprintf (stderr, "i=%d / %d - %d\n", i, nmidi, num);
	      fprintf (stderr, "EOF\n");
	      goto midi_end;
	    }
	  notes = next;
	  num = 0;
	}
      cur_note = &(notes->note[num]);

      /* calc delta time  */
      if (i==0)
	idt = 0;
      else
	idt = cur_note->step - last_step;
      last_step = cur_note->step;

      if (cur_note->sig == 1) /* start note  */
	n_midi = smf_note_on (fd, idt,
			      cur_note->note,
			      cur_note->intensity,
			      0);
      else /* stop note */
	n_midi = smf_note_off (fd, idt,
			       cur_note->note,
			       64, /* default  */
			       0);
      if (n_midi != 4)
	{
	  fprintf (stderr, "Error duing writing mid! %d (note)\n",
		   p_midi);
	  /*return;*/
	}
      p_midi += n_midi;
    }

 midi_end:
  n_midi = smf_track_end (fd);
  if (n_midi != 4)
    {
      fprintf (stderr, "Error duing writing mid! %d (track end)\n",
	       p_midi);
      return;
    }
  p_midi += n_midi;

  if (stdout_flg == 0) /* random-accessible file  */
    {
      /* re-calculate # of data in track  */
      if (lseek (fd, h_midi, SEEK_SET) < 0)
	{
	  fprintf (stderr, "Error duing lseek %d (re-calc)\n", h_midi);
	  return;
	}
      n_midi = smf_track_head (fd, (p_midi - dh_midi));
      if (n_midi != 8)
	{
	  fprintf (stderr, "Error duing write %d (re-calc)\n", p_midi);
	  return;
	}
    }
  else /* stdout  */
    {
      if ((7 + 4 * nmidi) != (p_midi - dh_midi))
	fprintf(stderr, "WaoN warning : data size seems to be different.\n");
    }

  close (fd);
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
  
  rfftw_plan plan; /* for FFTW library  */
  long *ibuf;
  double *x; /* wave data for FFT  */
  double den;

  struct soundstream patformat;
  ft_t ft_pat;

  int i;

  ft_pat = &patformat;

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
      ibuf = (long *)malloc (sizeof (long) * plen);
      if (ibuf == NULL)
	{
	  fprintf(stderr, "cannot allocate ibuf[%d]\n", plen);
	  patch_flg = 0;
	  return;
	}
      x = (double *)malloc (sizeof (double) * plen);
      if (x == NULL)
	{
	  fprintf(stderr, "cannot allocate p[%d]\n", plen);
	  patch_flg = 0;
	  free (ibuf);
	  return;
	}
      /* open patch file  */
      ft_pat->fp = fopen (file_patch, "r");
      /* read patch wav  */
      wavstartread (ft_pat);
      if (wavread (ft_pat, ibuf, plen) == NULL)
	{
	  fprintf (stderr, "No Patch Data!\n");
	  patch_flg = 0;
	  free (ibuf);
	  free (x);
	  return;
	}
      for (i=0; i<plen; i++)
	x[i] = (double)ibuf[i];

      /* calc power of patch  */
      den = init_den (plen, nwin);
      plan = rfftw_create_plan (plen, FFTW_REAL_TO_COMPLEX, FFTW_ESTIMATE);
      power_spectrum_fftw (plen, x, pat, den, nwin, plan);
      rfftw_destroy_plan (plan);

      free (ibuf);
      free (x);
      fclose (ft_pat->fp);

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
  new = malloc (sizeof (struct ia_note));
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
      new = malloc (sizeof (struct ia_note));
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
