/* WaoN - a Wave-to-Notes transcriber : main
 * Copyright (C) 1998-2007 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: main.c,v 1.5 2007/02/11 23:49:46 kichiki Exp $
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

#include <math.h>
#include <stdio.h> /* printf(), fprintf(), strerror()  */
#include <sys/errno.h> /* errno  */
#include <stdlib.h> /* exit()  */
#include <string.h> /* strcat(), strcpy()  */

/* FFTW library  */
#ifdef FFTW2
#include <rfftw.h>
#else // FFTW3
#include <fftw3.h>
#endif // FFTW2

#include "fft.h" // FFT utility functions
#include "hc.h" // HC array manipulation routines

// libsndfile
#include <sndfile.h>
#include "snd.h"

#include "midi.h" /* smf_...(), mid2freq[], get_note()  */
#include "analyse.h" /* note_intensity(), note_on_off(), output_midi()  */

#include "VERSION.h"


void usage (char * argv0)
{
  fprintf (stderr, "WaoN - a Wave-to-Notes transcriber, version %s\n",
	   WAON_VERSION);
  fprintf (stderr, "Copyright (C) 1998-2007 Kengo Ichiki "
	   "<kichiki@users.sourceforge.net>\n\n");
  fprintf (stderr, "Usage: %s [option ...]\n", argv0);
  fprintf (stderr, "  -h --help\tprint this help.\n");
  fprintf (stderr, "OPTIONS FOR FILES\n");
  fprintf (stderr, "  -i --input\tinput wav file (DEFAULT stdin)\n");
  fprintf (stderr, "  -o --output\toutput mid file"
	   " (DEFAULT 'output.mid')\n");
  fprintf (stderr, "\toptions -i and -o have argument '-' "
	   "as stdin/stdout\n");
  fprintf (stderr, "  -p --patch\tpatch file (DEFAULT no patch)\n");
  fprintf (stderr, "NOTE SELECTION OPTIONS\n");
  fprintf (stderr, "  -c --cutoff\tlog10 of cut-off ratio "
	   "to scale velocity of note\n"
	   "\t\t(DEFAULT -5.0)\n");
  fprintf (stderr, "  -r --relative\tlog10 of cut-off ratio "
	   "relative to the average.\n"
	   "\t\t(DEFAULT no relative cutoff\n"
	   "\t\t= absolute cutoff with the value in -c option)\n");
  fprintf (stderr, "  -k --peak\tpeak threshold for note-on, "
	   "which ranges [0,127]\n"
	   "\t\t(DEFAULT 128 = no peak-search = "
	   "search only first on-event)\n");
  fprintf (stderr, "  -t --top\ttop note [midi #] "
	   "(DEFAULT 103 = G7)\n");
  fprintf (stderr, "  -b --bottom\tbottom note [midi #] "
	   "(DEFAULT 28 = E1)\n");
  fprintf (stderr, "\tHere middle C (261 Hz) = C4 = midi 60. "
	   "Midi # ranges [0,127].\n");
  fprintf (stderr, "  -a --adjust\tadjust-pitch param, "
	   "which is suggested by WaoN after analysis.\n"
	   "\t\tunit is half-note, that is, +1 is half-note up,\n"
	   "\t\tand -0.5 is quater-note down. (DEFAULT 0)\n");
  fprintf (stderr, "READING WAV OPTIONS\n");
  fprintf (stderr, "  -n\t\tsampling number from WAV in 1 step "
	   "(DEFAULT 2048)\n");
  fprintf (stderr, "  -s --shift\tshift number from WAV in 1 step\n");
  fprintf (stderr, "\t\t(DEFAULT (1/4 of the value in -n option)\n");
  /*fprintf (stderr, "\t\tYou Should Set 2^n for -[nsp] options\n");*/
  fprintf (stderr, "FFT OPTIONS\n");
  fprintf (stderr, "  -w --window\t0 no window (DEFAULT)\n");
  fprintf (stderr, "\t\t1 parzen window\n");
  fprintf (stderr, "\t\t2 welch window\n");
  fprintf (stderr, "\t\t3 hanning window\n");
  fprintf (stderr, "\t\t4 hamming window\n");
  fprintf (stderr, "\t\t5 blackman window\n");
  fprintf (stderr, "\t\t6 steeper 30-dB/octave rolloff window\n");
  fprintf (stderr, "  --phase\tuse phase diff to improve freq estimation."
	   " (DEFAULT: no)\n");
}


int main (int argc, char** argv)
{
  extern int abs_flg; /* flag for absolute/relative cutoff  */
  extern int peak_threshold; /* to select peaks in a note  */
  extern double adj_pitch;
  extern double pitch_shift;
  extern int n_pitch;

  int show_help;

  char *file_midi = NULL;
  char *file_wav = NULL;
  char *file_patch = NULL;

  int i;
  int icnt; /* counter  */
  int nwin;
  int i0, i1, notetop, notelow;
  double cut_ratio; /* log10 of cutoff ratio for scale velocity  */
  double rel_cut_ratio; /* log10 of cutoff ratio relative to average  */
  double den; /* weight of window function for FFT  */
  double t0; /* time-period for FFT (inverse of smallest frequency)  */

  long hop, len, div;
  int num, nmidi;

  int flag_phase;

  struct ia_note *note_top; /* top of infinite array of note_sig  */
  struct ia_note *notes; /* infinite array of note_sig  */
  int n_notes; /* index within one segment of ia_note  */
  /* intensity matrix  */
  /*char i_lsts[384];*/ /* intensity list for 3 steps  */
  char i_lsts[128]; /* intensity list  */
  char * on_lst[128]; /* on list point to intensity in ia_note array  */


  /* default value */
  cut_ratio = -5.0;
  rel_cut_ratio = 1.0; /* this value is ignored when abs_flg == 1  */
  len = 2048;
  nwin = 0;
  /* for 76 keys piano  */
  notetop = 103; /* G8  */
  notelow = 28; /* E2  */

  abs_flg = 1;

  hop = 0;
  show_help = 0;
  adj_pitch = 0.0;
  peak_threshold = 128; /* this means no peak search  */

  flag_phase = 0;
  for (i=1; i<argc; i++)
    {
      if ((strcmp (argv[i], "-input" ) == 0)
	 || (strcmp (argv[i], "-i" ) == 0))
	{
	  if ( i+1 < argc )
	    {
	      file_wav = (char *)malloc (sizeof (char)
					 * (strlen (argv[++i]) + 1));
	      strcpy (file_wav, argv[i]);
	    }
	  else
	    {
	      show_help = 1;
	      break;
	    }
	}
      else if ((strcmp (argv[i], "-output" ) == 0)
	      || (strcmp (argv[i], "-o" ) == 0))
	{
	  if ( i+1 < argc )
	    {
	      file_midi = (char *)malloc (sizeof (char)
					 * (strlen (argv[++i]) + 1));
	      strcpy (file_midi, argv[i]);
	    }
	  else
	    {
	      show_help = 1;
	      break;
	    }
	}
      else if ((strcmp (argv[i], "--cutoff") == 0)
	       || (strcmp (argv[i], "-c") == 0))
	{
	  if ( i+1 < argc )
	    {
	      cut_ratio = atof (argv[++i]);
	    }
	  else
	    {
	      show_help = 1;
	      break;
	    }
	}
      else if ((strcmp (argv[i], "--top") == 0)
	       || (strcmp (argv[i], "-t") == 0))
	{
	  if ( i+1 < argc )
	    {
	      notetop = atoi( argv[++i] );
	    }
	  else
	    {
	      show_help = 1;
	      break;
	    }
	}
      else if ((strcmp (argv[i], "--bottom") == 0)
	       || (strcmp (argv[i], "-b") == 0))
	{
	  if ( i+1 < argc )
	    {
	      notelow = atoi (argv[++i]);
	    }
	  else
	    {
	      show_help = 1;
	      break;
	    }
	}
      else if ((strcmp (argv[i], "--window") == 0)
	       || (strcmp (argv[i], "-w") == 0))
	{
	  if ( i+1 < argc )
	    {
	      nwin = atoi (argv[++i]);
	    }
	  else
	    {
	      show_help = 1;
	      break;
	    }
	}
      else if ( strcmp (argv[i], "-n") == 0)
	{
	  if ( i+1 < argc )
	    {
	      len = atoi (argv[++i]);
	    }
	  else
	    {
	      show_help = 1;
	      break;
	    }
	}
      else if ((strcmp (argv[i], "--shift") == 0)
	       || (strcmp (argv[i], "-s") == 0))
	{
	  if ( i+1 < argc )
	    {
	      hop = atoi (argv[++i]);
	    }
	  else
	    {
	      show_help = 1;
	      break;
	    }
	}
      else if ((strcmp (argv[i], "--patch") == 0)
	       || (strcmp (argv[i], "-p") == 0))
	{
	  if ( i+1 < argc )
	    {
	      file_patch = argv[++i];
	    }
	  else
	    {
	      show_help = 1;
	      break;
	    }
	}
      else if ((strcmp (argv[i], "--relative") == 0)
	       || (strcmp (argv[i], "-r") == 0))
	{
	  if ( i+1 < argc )
	    {
	      rel_cut_ratio = atof (argv[++i]);
	      abs_flg = 0;
	    }
	  else
	    {
	      show_help = 1;
	      break;
	    }
	}
      else if ((strcmp (argv[i], "--peak") == 0)
	       || (strcmp (argv[i], "-k") == 0))
	{
	  if ( i+1 < argc )
	    {
	      peak_threshold = atoi (argv[++i]);
	    }
	  else
	    {
	      show_help = 1;
	      break;
	    }
	}
      else if ((strcmp (argv[i], "--adjust") == 0)
	       || (strcmp (argv[i], "-a") == 0))
	{
	  if ( i+1 < argc )
	    {
	      adj_pitch = atof (argv[++i]);
	    }
	  else
	    {
	      show_help = 1;
	      break;
	    }
	}
      else if ((strcmp (argv[i], "--help") == 0)
	       || (strcmp (argv[i], "-h") == 0))
	{
	  show_help = 1;
	  break;
	}
      else if (strcmp (argv[i], "--phase") == 0)
	{
	  flag_phase = 1;
	}
      else
	{
	  show_help = 1;
	}
    }
  if (show_help)
    {
      usage (argv[0]);
      exit (1);
    }

  if (nwin < 0 || nwin > 6)
    {
      nwin = 0;
    }
  if (hop == 0)
    {
      hop = len / 4;
    }


  /* malloc for note_on_off buffer  */
  note_top = init_ia_note ();
  if (note_top == NULL)
    {
      fprintf(stderr, "cannot note array\n");
      exit (1);
    }
  notes = note_top;
  n_notes = 0;

  /* clear intensity matrix  */
  for (i = 0; i < 128; i++)
    {
      on_lst[i] = NULL;
      i_lsts[i] = 0;
    }

  // allocate buffers
  double *left = NULL;
  double *right = NULL;
  left  = (double *)malloc (sizeof (double) * len);
  right = (double *)malloc (sizeof (double) * len);
  if (left == NULL)
    {
      fprintf(stderr, "cannot allocate left[%ld]\n", len);
      exit (1);
    }
  if (right == NULL)
    {
      fprintf(stderr, "cannot allocate right[%ld]\n", len);
      exit (1);
    }

  double *x = NULL; /* wave data for FFT  */
  double *y = NULL; /* spectrum data for FFT */ 
#ifdef FFTW2
  x = (double *)malloc (sizeof (double) * len);
  y = (double *)malloc (sizeof (double) * len);
#else // FFTW3
  x = (double *)fftw_malloc (sizeof (double) * len);
  y = (double *)fftw_malloc (sizeof (double) * len);
#endif // FFTW2
  if (x == NULL)
    {
      fprintf(stderr, "cannot allocate x[%ld]\n", len);
      exit (1);
    }
  if (y == NULL)
    {
      fprintf(stderr, "cannot allocate y[%ld]\n", len);
      exit (1);
    }

  double *p = NULL; /* power spectrum  */
  p = (double *)malloc (sizeof (double) * (len / 2 + 1));
  if (p == NULL)
    {
      fprintf (stderr, "cannot allocate p[%ld]\n", (len/2+1));
      exit (1);
    }

  double *p0 = NULL;
  double *dphi = NULL;
  double *ph0 = NULL;
  double *ph1 = NULL;
  if (flag_phase != 0)
    {
      p0 = (double *)malloc (sizeof (double) * (len / 2 + 1));
      if (p0 == NULL)
	{
	  fprintf (stderr, "cannot allocate p0[%ld]\n", (len/2+1));
	  exit (1);
	}

      dphi = (double *)malloc (sizeof (double) * (len / 2 + 1));
      if (dphi == NULL)
	{
	  fprintf (stderr, "cannot allocate dphi[%ld]\n", (len/2+1));
	  exit (1);
	}

      ph0 = (double *)malloc (sizeof (double) * (len/2+1));
      ph1 = (double *)malloc (sizeof (double) * (len/2+1));
      if (ph0 == NULL)
	{
	  fprintf (stderr, "cannot allocate ph0[%ld]\n", (len/2+1));
	  exit (1);
	}
      if (ph1 == NULL)
	{
	  fprintf (stderr, "cannot allocate ph1[%ld]\n", (len/2+1));
	  exit (1);
	}
    }

  double *pmidi = NULL;
  pmidi = (double *)malloc (sizeof (double) * 128);
  if (pmidi == NULL)
    {
      fprintf (stderr, "cannot allocate pmidi[%d]\n", 128);
      exit (1);
    }


  // MIDI output
  if (file_midi == NULL)
    {
      file_midi = (char *)malloc (sizeof (char) * (strlen("output.mid") + 1));
      strcpy (file_midi, "output.mid");
    }

  // open input wav file
  SNDFILE *sf = NULL;
  SF_INFO sfinfo;
  if (file_wav == NULL)
    {
      file_wav = (char *) malloc (sizeof (char) * 2);
      file_wav [0] = '-';
    }
  sf = sf_open (file_wav, SFM_READ, &sfinfo);
  if (sf == NULL)
    {
      fprintf (stderr, "Can't open input file %s : %s\n",
	       file_wav, strerror (errno));
      exit (1);
    }
  sndfile_print_info (&sfinfo);


  // check stereo or mono
  if (sfinfo.channels != 2 && sfinfo.channels != 1)
    {
      fprintf (stderr, "only mono and stereo inputs are supported.\n");
      exit (1);
    }


  // time period of FFT
  t0 = (double)len/(double)sfinfo.samplerate;

  // window factor for FFT
  den = init_den (len, nwin);

  /* set range to analyse (search notes) */
  /* -- after 't0' is calculated  */
  i0 = (int)(mid2freq[notelow]*t0 - 0.5);
  i1 = (int)(mid2freq[notetop]*t0 - 0.5)+1;
  if (i0 <= 0)
    {
      i0 = 1; // i0=0 means DC component (frequency = 0)
    }
  if (i1 >= (len/2))
    {
      i1 = len/2 - 1;
    }

  // init patch
  init_patch (file_patch, len, nwin);
  /*                      ^^^ len could be given by option separately  */

  // initialization plan for FFTW
#ifdef FFTW2
  rfftw_plan plan;
  plan = rfftw_create_plan (len, FFTW_REAL_TO_COMPLEX, FFTW_ESTIMATE);
#else // FFTW3
  fftw_plan plan;
  plan = fftw_plan_r2r_1d (len, x, y, FFTW_R2HC, FFTW_ESTIMATE);
#endif

  // for first step
  if (hop != len)
    {
      if (sndfile_read (sf, sfinfo,
			left + hop,
			right + hop,
			(len - hop))
	  != (len - hop))
	{
	  fprintf (stderr, "No Wav Data!\n");
	  exit(0);
	}
    }

  /** main loop (icnt) **/
  nmidi = 0; /* number of midi data (note star or stop) */
  pitch_shift = 0.0;
  n_pitch = 0;
  for (icnt=0; ; icnt++)
    {
      // shift
      for (i = 0; i < len - hop; i ++)
	{
	  if (sfinfo.channels == 2) // stereo
	    {
	      left  [i] = left  [i + hop];
	      right [i] = right [i + hop];
	    }
	  else // mono
	    {
	      left  [i] = left  [i + len - hop];
	    }
	}
      // read from wav
      if (sndfile_read (sf, sfinfo,
			left  + (len - hop),
			right + (len - hop),
			hop)
	  != hop)
	{
	  fprintf (stderr, "WaoN : end of file.\n");
	  break;
	}

      // set double table x[] for FFT
      for (i = 0; i < len; i ++)
	{
	  if (sfinfo.channels == 2) // stereo
	    {
	      x [i] = 0.5 * (left [i] + right [i]);
	    }
	  else // mono
	    {
	      x [i] = left [i];
	    }
	}


      // stage 1: calc power spectrum
      windowing (len, x, nwin, 1.0, x);

      /* FFTW library  */
#ifdef FFTW2
      rfftw_one (plan, x, y);
#else // FFTW3
      fftw_execute (plan); // x[] -> y[]
#endif

      if (flag_phase == 0)
	{
	  HC_to_amp2 (len, y, den, p);
	}
      else
	{
	  HC_to_polar2 (len, y, 0, den, p, ph1);

	  if (icnt == 0) // first step, so no ph0[] yet
	    {
	      for (i = 0; i < (len/2+1); ++i) // full span
		{
		  // no correction
		  dphi[i] = 0.0;

		  // backup the phase for the next step
		  p0  [i] = p   [i];
		  ph0 [i] = ph1 [i];
		}	  
	    }
	  else // icnt > 0
	    {
	      // freq correction by phase difference
	      for (i = 0; i < (len/2+1); ++i) // full span
		{
		  double twopi = 2.0 * M_PI;
		  //double dphi;
		  dphi[i] = ph1[i] - ph0[i]
		    - twopi * (double)i / (double)len * (double)hop;
		  for (; dphi[i] >= M_PI; dphi[i] -= twopi);
		  for (; dphi[i] < -M_PI; dphi[i] += twopi);

		  // frequency correction
		  // NOTE: freq is (i / len + dphi) * samplerate [Hz]
		  dphi[i] = dphi[i] / twopi / (double)hop;

		  // backup the phase for the next step
		  p0  [i] = p   [i];
		  ph0 [i] = ph1 [i];

		  // then, average the power for the analysis
		  p[i] = 0.5 *(sqrt (p[i]) + sqrt (p0[i]));
		  p[i] = p[i] * p[i];
		}
	    }
	}

      // stage 2: pickup notes
      /* new code
      if (flag_phase == 0)
	{
	  average_FFT_into_midi (len, (double)sfinfo.samplerate,
				 p, NULL,
				 pmidi);
	}
      else
	{
	  average_FFT_into_midi (len, (double)sfinfo.samplerate,
				 p, dphi,
				 pmidi);
	}
      pickup_notes (pmidi,
		    cut_ratio, rel_cut_ratio,
		    notelow, notetop,
		    i_lsts);
      */

      /* old code */
      if (flag_phase == 0)
	{
	  note_intensity (p, NULL,
			  cut_ratio, rel_cut_ratio, i0, i1, t0, i_lsts);
	}
      else
	{
	  // make corrected frequency (i / len + dphi) * samplerate [Hz]
	  for (i = 0; i < (len/2+1); ++i) // full span
	    {
	      dphi[i] = ((double)i / (double)len + dphi[i])
		* (double)sfinfo.samplerate;
	    }
	  note_intensity (p, dphi,
			  cut_ratio, rel_cut_ratio, i0, i1, t0, i_lsts);
	}


      // stage 3: check previous time for note-on/off
      notes = chk_note_on_off (icnt, i_lsts, on_lst,
			       notes, &n_notes, &num);
      nmidi += num;
    }

  pitch_shift /= (double) n_pitch;
  fprintf (stderr, "WaoN : difference of pitch = %f ( + %f )\n",
	   -(pitch_shift - 0.5),
	   adj_pitch);

  /* div is the divisions for one beat (quater-note).
   * here we assume 120 BPM, that is, 1 beat is 0.5 sec.
   * note: (hop / ft->rate) = duration for 1 step (sec) */
  //div = (long)(0.5 * (double) (ft->rate) / (double) hop);
  div = (long)(0.5 * (double)sfinfo.samplerate / (double) hop);
  fprintf (stderr, "division = %ld\n", div);
  fprintf (stderr, "WaoN : # of notes = %d\n",nmidi);

  output_midi (nmidi, note_top, div, file_midi);

#ifdef FFTW2
  rfftw_destroy_plan (plan);
#else
  fftw_destroy_plan (plan);
#endif /* FFTW2 */


  free (left);
  free (right);
  free (x);
  free (y);
  free (p);
  if (p0 != NULL) free (p0);
  if (dphi != NULL) free (dphi);
  if (pmidi != NULL) free (pmidi);
  if (ph0 != NULL) free (ph0);
  if (ph1 != NULL) free (ph1);

  if (file_wav != NULL) free (file_wav);
  free (file_midi);

  sf_close (sf);

  return 0;
}
