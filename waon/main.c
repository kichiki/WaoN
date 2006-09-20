/* WaoN - a Wave-to-Notes transcriber : main
 * Copyright (C) 1998 Kengo ICHIKI (ichiki@geocities.com)
 * $Id: main.c,v 1.1 2006/09/20 21:25:04 kichiki Exp $
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

#include <rfftw.h> /* FFTW library  */

#include "sox-wav.h" /* wavstartread(), wavread()  */
#include "midi.h" /* smf_...(), mid2freq[], get_note()  */
#include "fft.h" /* four1()  */
#include "analyse.h" /* note_intensity(), note_on_off(), output_midi()  */

#include "VERSION.h"

int main (int argc, char** argv);

int main (int argc, char** argv)
{
  extern int abs_flg; /* flag for absolute/relative cutoff  */
  extern int peak_threshold; /* to select peaks in a note  */
  extern double adj_pitch;
  extern double pitch_shift;
  extern int n_pitch;

  /* for wav file : soundstream & ft_t is defined in sox-wav.h  */
  struct soundstream informat;
  ft_t ft;

  int show_version;
  int show_help;

  char *file_midi;
  char *file_wav;
  char *file_patch;

  int i;
  int icnt; /* counter  */
  int nwin;
  int i0, i1, notetop, notelow;
  double cut_ratio; /* log10 of cutoff ratio for scale velocity  */
  double rel_cut_ratio; /* log10 of cutoff ratio relative to average  */
  double den; /* weight of window function for FFT  */
  double t0; /* time-period for FFT (inverse of smallest frequency)  */
  long *ibuf, *ibuf0;
  /* ibuf[] is used for interpolate into p[]  */
  /* ibuf0[] is used for read from wav and shift  */
  double *x; /* wave data for FFT  */
  double *p; /* power spectrum  */
  long sft, len, div;
  int num, nmidi;

  rfftw_plan plan; /* for FFTW library  */

  struct ia_note *note_top; /* top of infinite array of note_sig  */
  struct ia_note *notes; /* infinite array of note_sig  */
  int n_notes; /* index within one segment of ia_note  */
  /* intensity matrix  */
  /*char i_lsts[384];*/ /* intensity list for 3 steps  */
  char i_lsts[128]; /* intensity list  */
  char * on_lst[128]; /* on list point to intensity in ia_note array  */


  /* init pointer  */
  ft = &informat;

  /* malloc for note_on_off buffer  */
  note_top = init_ia_note ();
  if (note_top == NULL)
    {
      fprintf(stderr, "cannot note array\n");
      exit (1);
    }
  notes = note_top;
  n_notes = 0;

  /* default value */
  cut_ratio = -5.0;
  rel_cut_ratio = 1.0; /* this value is ignored when abs_flg == 1  */
  len = 2048;
  nwin = 0;
  /* for 76 keys piano  */
  notetop = 103; /* G8  */
  notelow = 28; /* E2  */

  abs_flg = 1;

  sft = 0;
  show_version = 0;
  show_help = 0;
  file_wav = NULL;
  file_midi = NULL;
  file_patch = NULL;
  adj_pitch = 0.0;
  peak_threshold = 128; /* this means no peak search  */

  for (i=1; i<argc; i++)
    {
      if ((strcmp (argv[i], "-input" ) == 0)
	 || (strcmp (argv[i], "-i" ) == 0))
	{
	  if ( i+1 < argc )
	    {
	      file_wav = argv[++i];
	    }
	}
      else if ((strcmp (argv[i], "-output" ) == 0)
	      || (strcmp (argv[i], "-o" ) == 0))
	{
	  if ( i+1 < argc )
	    {
	      file_midi = argv[++i];
	    }
	}
      else if ((strcmp (argv[i], "--cutoff") == 0)
	       || (strcmp (argv[i], "-c") == 0))
	{
	  if ( i+1 < argc )
	    {
	      cut_ratio = atof (argv[++i]);
	    }
	}
      else if ((strcmp (argv[i], "--top") == 0)
	       || (strcmp (argv[i], "-t") == 0))
	{
	  if ( i+1 < argc )
	    {
	      notetop = atoi( argv[++i] );
	    }
	}
      else if ((strcmp (argv[i], "--bottom") == 0)
	       || (strcmp (argv[i], "-b") == 0))
	{
	  if ( i+1 < argc )
	    {
	      notelow = atoi (argv[++i]);
	    }
	}
      else if ((strcmp (argv[i], "--window") == 0)
	       || (strcmp (argv[i], "-w") == 0))
	{
	  if ( i+1 < argc )
	    {
	      nwin = atoi (argv[++i]);
	    }
	}
      else if ( strcmp (argv[i], "-n") == 0)
	{
	  if ( i+1 < argc )
	    {
	      len = atoi (argv[++i]);
	    }
	}
      else if ((strcmp (argv[i], "--shift") == 0)
	       || (strcmp (argv[i], "-s") == 0))
	{
	  if ( i+1 < argc )
	    {
	      sft = atoi (argv[++i]);
	    }
	}
      else if ((strcmp (argv[i], "--patch") == 0)
	       || (strcmp (argv[i], "-p") == 0))
	{
	  if ( i+1 < argc )
	    {
	      file_patch = argv[++i];
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
	}
      else if ((strcmp (argv[i], "--peak") == 0)
	       || (strcmp (argv[i], "-k") == 0))
	{
	  if ( i+1 < argc )
	    {
	      peak_threshold = atoi (argv[++i]);
	    }
	}
      else if ((strcmp (argv[i], "--adjust") == 0)
	       || (strcmp (argv[i], "-a") == 0))
	{
	  if ( i+1 < argc )
	    {
	      adj_pitch = atof (argv[++i]);
	    }
	}
      else if ((strcmp (argv[i], "--help") == 0)
	       || (strcmp (argv[i], "-h") == 0))
	{
	  show_help = 1;
 	  /*argv[i] = NULL;*/
	}
      else if ((strcmp (argv[i], "--version") == 0)
	       || (strcmp (argv[i], "-v") == 0))
	{
	  show_version = 1;
 	  /*argv[i] = NULL;*/
	}
      else
	{
	  show_help = 1;
	}
    }

  if (show_version)
    {
      fprintf (stderr, "WaoN - a Wave-to-Notes transcriber, version %s\n",
	       WAON_VERSION);
      fprintf (stderr, "Copyright (C) 1998 Kengo ICHIKI "
	       "<ichiki@geocities.com>\n\n");
      exit( 1 );
    }

  if (show_help)
    {
      fprintf (stderr, "WaoN - a Wave-to-Notes transcriber, version %s\n",
	       WAON_VERSION);
      fprintf (stderr, "Copyright (C) 1998 Kengo ICHIKI "
	       "<ichiki@geocities.com>\n\n");
      fprintf (stderr, "Usage: %s [option ...]\n", argv[0]);
      fprintf (stderr, "  -h --help\tprint this help.\n");
      fprintf (stderr, "  -v --version\tprint version info.\n");
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
	       "(DEFAULT 103 = G8)\n");
      fprintf (stderr, "  -b --bottom\tbottom note [midi #] "
	       "(DEFAULT 28 = E2)\n");
      fprintf (stderr, "\tHere middle C (261 Hz) = C3 = midi 60. "
	       "Midi # ranges [0,127].\n");
      fprintf (stderr, "  -a --adjust\tadjust-pitch param, "
	       "which is suggested by WaoN after analysis.\n"
	       "\t\tunit is half-note, that is, +1 is half-note up,\n"
	       "\t\tand -0.5 is quater-note down. (DEFAULT 0)\n");
      fprintf (stderr, "READING WAV OPTIONS\n");
      fprintf (stderr, "  -n\t\tsampling number from WAV in 1 step "
	       "(DEFAULT 2048)\n");
      fprintf (stderr, "  -s --shift\tshift number from WAV in 1 step\n");
      fprintf (stderr, "\t\t(DEFAULT same in the value in -n option)\n");
      /*fprintf (stderr, "\t\tYou Should Set 2^n for -[nsp] options\n");*/
      fprintf (stderr, "FFT OPTIONS\n");
      fprintf (stderr, "  -w --window\t0 no window (DEFAULT)\n");
      fprintf (stderr, "\t\t1 parzen window\n");
      fprintf (stderr, "\t\t2 welch window\n");
      fprintf (stderr, "\t\t3 hanning window\n");
      fprintf (stderr, "\t\t4 hamming window\n");
      fprintf (stderr, "\t\t5 blackman window\n");
      fprintf (stderr, "\t\t6 steeper 30-dB/octave rolloff window\n");
      exit (1);
    }

  if (nwin <0 || nwin>6)
    nwin = 0;
  if (sft == 0) sft = len;

  /* clear intensity matrix  */
  for (i=0; i<128; i++)
    on_lst[i] = NULL;
  for (i=0; i<128; i++)
      i_lsts[i] = 0;

  /* initialization plan for FFTW  */
  plan = rfftw_create_plan (len, FFTW_REAL_TO_COMPLEX, FFTW_ESTIMATE);

  /* allocate buffers  */
  ibuf = (long *)malloc (sizeof (long) * len);
  if (ibuf == NULL)
    {
      fprintf(stderr, "cannot allocate ibuf[%ld]\n", len);
      exit(1);
    }
  ibuf0 = (long *)malloc (sizeof (long) * sft);
  if (ibuf0 == NULL)
    {
      fprintf(stderr, "cannot allocate ibuf0[%ld]\n", sft);
      exit(1);
    }
  x = (double *)malloc (sizeof (double) * len);
  if (x == NULL)
    {
      fprintf(stderr, "cannot allocate p[%ld]\n", len);
      exit(1);
    }
  p = (double *)malloc (sizeof (double) * (len / 2 + 1));
  if (p == NULL)
    {
      fprintf (stderr, "cannot allocate p[%ld]\n", (len/2+1));
      exit(1);
    }

  /* MIDI output  */
  if (file_midi == NULL)
    {
      file_midi = malloc ( 11 );
      strcpy (file_midi, "output.mid");
    }

  /* open input wav file */
  if (file_wav == NULL
      || strncmp (file_wav, "-", strlen (file_wav)) == 0)
    {
      ft->fp = stdin;
    }
  else
    {
      ft->fp = fopen (file_wav, "r");
    }
  if (ft->fp == NULL)
    {
      fprintf (stderr, "Can't open input file %s : %s\n",
	       file_wav, strerror (errno));
      exit( 1 );
    }
  /* read header of wav file */
  wavstartread (ft);

  /* time period of FFT  */
  t0 = (double)len/(ft->rate);
  /* window factor for FFT  */
  den = init_den (len, nwin);

  /* set range to analyse (search notes) */
  /* -- after 't0' is calculated  */
  i0 = (int)(mid2freq[notelow]*t0 - 0.5);
  i1 = (int)(mid2freq[notetop]*t0 - 0.5)+1;
  if (i0<0)
    i0 = 0;
  if (i1 >= len/2)
    i1 = len/2 - 1;

  /* init patch  */
  init_patch (file_patch, len, nwin);
  /*                      ^^^ len could be given by option separately  */

  /* for first step */
  if (sft != len)
    {
      if (wavread( ft, ibuf, len) == NULL)
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
      /* read from wav */
      if (wavread (ft, ibuf0, sft) == NULL)
	{
	  fprintf (stderr, "WaoN : end of file.\n");
	  break;
	}

      /* shift */
      for (i=0; i<len-sft; i++)
	{
	  ibuf[i] = ibuf[i+sft];
	}
      for (i=len-sft; i<len; i++)
	{
	  ibuf[i] = ibuf0[i-len+sft];
	}
      /* interpolate by spline  */
      for (i=0; i<len; i++)
	x[i] = (double)ibuf[i];

      /* calc power spectrum  */
      power_spectrum_fftw (len, x, p, den, nwin, plan);

      note_intensity (len/2, p, cut_ratio, rel_cut_ratio, i0, i1, t0, i_lsts);
      notes = chk_note_on_off (icnt, i_lsts, on_lst,
			       notes, &n_notes, &num);
      nmidi += num;
    }

  pitch_shift /= (double) n_pitch;
  fprintf (stderr, "WaoN : difference of pitch = %f ( + %f )\n",
	   -(pitch_shift - 0.5),
	   adj_pitch);

  /* 0.5 [sec] means 120 beat per minute */
  div = (long)(0.5 * (double) (ft->rate) / (double) sft);
  fprintf (stderr, "WaoN : # of notes = %d\n",nmidi);

  output_midi (nmidi, note_top, div, file_midi);

  rfftw_destroy_plan (plan);

  return 0;
}
