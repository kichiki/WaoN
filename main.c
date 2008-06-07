/* WaoN - a Wave-to-Notes transcriber : main
 * Copyright (C) 1998-2008 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: main.c,v 1.11 2008/06/07 04:53:16 kichiki Exp $
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
#include "memory-check.h" // CHECK_MALLOC() macro

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
#include "notes.h" // struct WAON_notes

#include "VERSION.h"


void print_version (void)
{
  fprintf (stdout, "WaoN - a Wave-to-Notes transcriber, Version %s\n\n",
	   WAON_VERSION);
  fprintf (stdout, "Copyright (C) 1998-2007 Kengo Ichiki "
	   "<kichiki@users.sourceforge.net>\n");
  fprintf (stdout, "Web: http://waon.sourceforge.net/\n\n");
}

void print_usage (char * argv0)
{
  print_version ();
  fprintf (stdout, "WaoN is a Wave-to-Notes transcriber,\n"
	   "that is, a converter from sound file to midi file.\n\n");
  fprintf (stdout, "Usage: %s [option ...]\n\n", argv0);
  fprintf (stdout, "Options:\n");
  fprintf (stdout, "  -h --help\tprint this help.\n");
  fprintf (stdout, "  -v, --version\tprint version information.\n");
  fprintf (stdout, "OPTIONS FOR FILES\n");
  fprintf (stdout, "  -i --input\tinput wav file (default: stdin)\n");
  fprintf (stdout, "  -o --output\toutput mid file"
	   " (default: 'output.mid')\n");
  fprintf (stdout, "\toptions -i and -o have argument '-' "
	   "as stdin/stdout\n");
  fprintf (stdout, "  -p --patch\tpatch file (default: no patch)\n");
  fprintf (stdout, "FFT OPTIONS\n");
  fprintf (stdout, "  -n\t\tsampling number from WAV in 1 step "
	   "(default: 2048)\n");
  fprintf (stdout, "  -w --window\t0 no window\n");
  fprintf (stdout, "\t\t1 parzen window\n");
  fprintf (stdout, "\t\t2 welch window\n");
  fprintf (stdout, "\t\t3 hanning window (default)\n");
  fprintf (stdout, "\t\t4 hamming window\n");
  fprintf (stdout, "\t\t5 blackman window\n");
  fprintf (stdout, "\t\t6 steeper 30-dB/octave rolloff window\n");
  fprintf (stdout, "READING WAV OPTIONS\n");
  fprintf (stdout, "  -s --shift\tshift number from WAV in 1 step\n");
  fprintf (stdout, "\t\t(default: 1/4 of the value in -n option)\n");
  fprintf (stdout, "PHASE-VOCODER OPTIONS\n");
  fprintf (stdout, "  -nophase\tdon't use phase diff to improve freq estimation.\n"
	   "\t\t(default: use the correction)\n");
  fprintf (stdout, "NOTE SELECTION OPTIONS\n");
  fprintf (stdout, "  -c --cutoff\tlog10 of cut-off ratio "
	   "to scale velocity of note\n"
	   "\t\t(default: -5.0)\n");
  fprintf (stdout, "  -r --relative\tlog10 of cut-off ratio "
	   "relative to the average.\n"
	   "\t\t(default: no relative cutoff\n"
	   "\t\t= absolute cutoff with the value in -c option)\n");
  fprintf (stdout, "  -k --peak\tpeak threshold for note-on, "
	   "which ranges [0,127]\n"
	   "\t\t(default: 128 = no peak-search = "
	   "search only first on-event)\n");
  fprintf (stdout, "  -t --top\ttop note [midi #] "
	   "(default: 103 = G7)\n");
  fprintf (stdout, "  -b --bottom\tbottom note [midi #] "
	   "(default: 28 = E1)\n");
  fprintf (stdout, "\tHere middle C (261 Hz) = C4 = midi 60. "
	   "Midi # ranges [0,127].\n");
  fprintf (stdout, "  -a --adjust\tadjust-pitch param, "
	   "which is suggested by WaoN after analysis.\n"
	   "\t\tunit is half-note, that is, +1 is half-note up,\n"
	   "\t\tand -0.5 is quater-note down. (default: 0)\n");
  fprintf (stdout, "DRUM-REMOVAL OPTIONS\n");
  fprintf (stdout, "  -psub-n\tnumber of averaging bins in one side.\n"
	   "\t\tthat is, for n, (i-n,...,i,...,i+n) are averaged\n"
	   "\t\t(default: 0)\n");
  fprintf (stdout, "  -psub-f\tfactor to the average,"
	   " where the power is modified as\n"
	   "\t\tp[i] = (sqrt(p[i]) - f * sqrt(ave[i]))^2\n"
	   "\t\t(default: 0.0)\n");
  fprintf (stdout, "OCTAVE-REMOVAL OPTIONS\n");
  fprintf (stdout, "  -oct\tfactor to the octave removal,"
	   " where the power is modified as\n"
	   "\t\tp[i] = (sqrt(p[i]) - f * sqrt(oct[i]))^2\n"
	   "\t\t(default: 0.0)\n");
}


int main (int argc, char** argv)
{
  extern int abs_flg; /* flag for absolute/relative cutoff  */
  extern double adj_pitch;
  extern double pitch_shift;
  extern int n_pitch;

  char *file_midi = NULL;
  char *file_wav = NULL;
  char *file_patch = NULL;

  int i;

  // default value
  double cut_ratio; // log10 of cutoff ratio for scale velocity
  cut_ratio = -5.0;
  double rel_cut_ratio; // log10 of cutoff ratio relative to average
  rel_cut_ratio = 1.0; // this value is ignored when abs_flg == 1
  long len = 2048;
  int flag_window = 3; // hanning window
  /* for 76 keys piano  */
  int notetop = 103; /* G8  */
  int notelow = 28; /* E2  */

  abs_flg = 1;

  long hop = 0;
  int show_help = 0;
  int show_version = 0;
  adj_pitch = 0.0;
  /* to select peaks in a note  */
  int peak_threshold = 128; /* this means no peak search  */

  int flag_phase = 1; // use the phase correction
  int psub_n = 0;
  double psub_f = 0.0;
  double oct_f = 0.0;
  for (i = 1; i < argc; i++)
    {
      if ((strcmp (argv[i], "-input" ) == 0)
	 || (strcmp (argv[i], "-i" ) == 0))
	{
	  if ( i+1 < argc )
	    {
	      file_wav = (char *)malloc (sizeof (char)
					 * (strlen (argv[++i]) + 1));
	      CHECK_MALLOC (file_wav, "main");
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
	      CHECK_MALLOC (file_midi, "main");
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
	      flag_window = atoi (argv[++i]);
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
      else if (strcmp (argv[i], "-nophase") == 0)
	{
	  flag_phase = 0;
	}
      else if (strcmp (argv[i], "-psub-n") == 0)
	{
	  if ( i+1 < argc )
	    {
	      psub_n = atoi (argv[++i]);
	    }
	  else
	    {
	      show_help = 1;
	      break;
	    }
	}
      else if (strcmp (argv[i], "-psub-f") == 0)
	{
	  if ( i+1 < argc )
	    {
	      psub_f = atof (argv[++i]);
	    }
	  else
	    {
	      show_help = 1;
	      break;
	    }
	}
      else if (strcmp (argv[i], "-oct") == 0)
	{
	  if ( i+1 < argc )
	    {
	      oct_f = atof (argv[++i]);
	    }
	  else
	    {
	      show_help = 1;
	      break;
	    }
	}
      else if (strcmp (argv[i], "-v") == 0 ||
	       strcmp (argv[i], "--version") == 0)
	{
	  show_version = 1;
	}
      else
	{
	  show_help = 1;
	}
    }
  if (show_help == 1)
    {
      print_usage (argv[0]);
      exit (1);
    }
  else if (show_version == 1)
    {
      print_version ();
      exit (1);
    }

  if (flag_window < 0 || flag_window > 6)
    {
      flag_window = 0;
    }
  if (hop == 0)
    {
      hop = len / 4;
    }
  if (psub_n == 0) psub_f = 0.0;
  if (psub_f == 0.0) psub_n = 0;


  struct WAON_notes *notes = WAON_notes_init();
  CHECK_MALLOC (notes, "main");
  char vel[128];     // velocity at the current step
  int on_event[128]; // event index of struct WAON_notes.
  for (i = 0; i < 128; i ++)
    {
      vel[i]      = 0;
      on_event[i] = -1;
    }

  // allocate buffers
  double *left  = (double *)malloc (sizeof (double) * len);
  double *right = (double *)malloc (sizeof (double) * len);
  CHECK_MALLOC (left,  "main");
  CHECK_MALLOC (right, "main");

  double *x = NULL; /* wave data for FFT  */
  double *y = NULL; /* spectrum data for FFT */ 
#ifdef FFTW2
  x = (double *)malloc (sizeof (double) * len);
  y = (double *)malloc (sizeof (double) * len);
#else // FFTW3
  x = (double *)fftw_malloc (sizeof (double) * len);
  y = (double *)fftw_malloc (sizeof (double) * len);
#endif // FFTW2
  CHECK_MALLOC (x, "main");
  CHECK_MALLOC (y, "main");

  /* power spectrum  */
  double *p = (double *)malloc (sizeof (double) * (len / 2 + 1));
  CHECK_MALLOC (p, "main");

  double *p0 = NULL;
  double *dphi = NULL;
  double *ph0 = NULL;
  double *ph1 = NULL;
  if (flag_phase != 0)
    {
      p0 = (double *)malloc (sizeof (double) * (len / 2 + 1));
      CHECK_MALLOC (p0, "main");

      dphi = (double *)malloc (sizeof (double) * (len / 2 + 1));
      CHECK_MALLOC (dphi, "main");

      ph0 = (double *)malloc (sizeof (double) * (len/2+1));
      ph1 = (double *)malloc (sizeof (double) * (len/2+1));
      CHECK_MALLOC (ph0, "main");
      CHECK_MALLOC (ph1, "main");
    }

  double *pmidi = (double *)malloc (sizeof (double) * 128);
  CHECK_MALLOC (pmidi, "main");


  // MIDI output
  if (file_midi == NULL)
    {
      file_midi = (char *)malloc (sizeof (char) * (strlen("output.mid") + 1));
      CHECK_MALLOC (file_midi, "main");
      strcpy (file_midi, "output.mid");
    }

  // open input wav file
  if (file_wav == NULL)
    {
      file_wav = (char *) malloc (sizeof (char) * 2);
      CHECK_MALLOC (file_wav, "main");
      file_wav [0] = '-';
    }
  SF_INFO sfinfo;
  SNDFILE *sf = sf_open (file_wav, SFM_READ, &sfinfo);
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


  // time-period for FFT (inverse of smallest frequency)
  double t0 = (double)len/(double)sfinfo.samplerate;

  // weight of window function for FFT
  double den = init_den (len, flag_window);

  /* set range to analyse (search notes) */
  /* -- after 't0' is calculated  */
  int i0 = (int)(mid2freq[notelow]*t0 - 0.5);
  int i1 = (int)(mid2freq[notetop]*t0 - 0.5)+1;
  if (i0 <= 0)
    {
      i0 = 1; // i0=0 means DC component (frequency = 0)
    }
  if (i1 >= (len/2))
    {
      i1 = len/2 - 1;
    }

  // init patch
  init_patch (file_patch, len, flag_window);
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
  pitch_shift = 0.0;
  n_pitch = 0;
  int icnt; /* counter  */
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
	      left  [i] = left  [i + hop];
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

      /**
       * stage 1: calc power spectrum
       */
      windowing (len, x, flag_window, 1.0, x);

      /* FFTW library  */
#ifdef FFTW2
      rfftw_one (plan, x, y);
#else // FFTW3
      fftw_execute (plan); // x[] -> y[]
#endif

      if (flag_phase == 0)
	{
	  // no phase-vocoder correction
	  HC_to_amp2 (len, y, den, p);
	}
      else
	{
	  // with phase-vocoder correction
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

      // drum-removal process
      if (psub_n != 0)
	{
	  power_subtract_ave (len, p, psub_n, psub_f);
	}

      // octave-removal process
      if (oct_f != 0.0)
	{
	  power_subtract_octave (len, p, oct_f);
	}

      /**
       * stage 2: pickup notes
       */
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
		    vel);
      */

      /* old code */
      if (flag_phase == 0)
	{
	  // no phase-vocoder correction
	  note_intensity (p, NULL,
			  cut_ratio, rel_cut_ratio, i0, i1, t0, vel);
	}
      else
	{
	  // with phase-vocoder correction
	  // make corrected frequency (i / len + dphi) * samplerate [Hz]
	  for (i = 0; i < (len/2+1); ++i) // full span
	    {
	      dphi[i] = ((double)i / (double)len + dphi[i])
		* (double)sfinfo.samplerate;
	    }
	  note_intensity (p, dphi,
			  cut_ratio, rel_cut_ratio, i0, i1, t0, vel);
	}

      /**
       * stage 3: check previous time for note-on/off
       */
      WAON_notes_check (notes, icnt, vel, on_event,
			8, 0, peak_threshold);
    }

  pitch_shift /= (double) n_pitch;
  fprintf (stderr, "WaoN : difference of pitch = %f ( + %f )\n",
	   -(pitch_shift - 0.5),
	   adj_pitch);

  /* div is the divisions for one beat (quater-note).
   * here we assume 120 BPM, that is, 1 beat is 0.5 sec.
   * note: (hop / ft->rate) = duration for 1 step (sec) */
  long div = (long)(0.5 * (double)sfinfo.samplerate / (double) hop);
  fprintf (stderr, "division = %ld\n", div);
  fprintf (stderr, "WaoN : # of events = %d\n", notes->n);

  WAON_notes_output_midi (notes, div, file_midi);


#ifdef FFTW2
  rfftw_destroy_plan (plan);
#else
  fftw_destroy_plan (plan);
#endif /* FFTW2 */


  WAON_notes_free (notes);
  free (left);
  free (right);
  free (x);
  free (y);
  free (p);
  if (p0 != NULL) free (p0);
  if (dphi != NULL) free (dphi);
  if (ph0 != NULL) free (ph0);
  if (ph1 != NULL) free (ph1);

  if (pmidi != NULL) free (pmidi);

  if (file_wav  != NULL) free (file_wav);
  if (file_midi != NULL) free (file_midi);

  sf_close (sf);

  return 0;
}
