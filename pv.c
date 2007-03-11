/* PV - phase vocoder : main
 * Copyright (C) 2007 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: pv.c,v 1.10 2007/03/11 01:13:22 kichiki Exp $
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>


#include "pv-complex.h"
#include "pv-conventional.h"
#include "pv-ellis.h"
#include "pv-freq.h"
#include "pv-loose-lock.h"

#include "memory-check.h" // CHECK_MALLOC() macro

#include "VERSION.h"


void usage (char * argv0)
{
  fprintf (stderr, "WaoN-pv : phase vocoder, Version %s\n",
	   WAON_VERSION);
  fprintf (stderr, "Copyright (C) 2007 Kengo Ichiki "
	   "<kichiki@users.sourceforge.net>\n");
  fprintf (stderr, "Web: http://waon.sourceforge.net/\n\n");
  fprintf (stderr, "Usage: %s [option ...]\n", argv0);
  fprintf (stderr, "  -h, --help\tprint this help.\n");
  fprintf (stderr, "OPTIONS FOR FILES\n");
  fprintf (stderr, "  -i, --input\tinput file (default: stdin)\n");
  fprintf (stderr, "  -o, --output\toutput file in flac"
	   " (default: play audio by ao)\n");
  fprintf (stderr, "  -n         \tFFT data number (default: 2048)\n");
  fprintf (stderr, "  -hop       \thop number (default: 256)\n");
  fprintf (stderr, "  -rate      \tsynthesize rate; larger is faster"
	   " (default: 1.0)\n");
  fprintf (stderr, "  -pitch\tpitch shift. +1/-1 is half-note up/down"
	   " (default: 0)\n");
  fprintf (stderr, "  -scheme    \tgive the number for PV scheme\n");
  fprintf (stderr, "\t\t0 : conventional PV (default)\n");
  fprintf (stderr, "\t\t1 : PV by complex arithmetics with fixed hops\n");
  fprintf (stderr, "\t\t2 : Puckette's loose-locking PV\n");
  fprintf (stderr, "\t\t3 : Puckette's loose-locking PV by complex"
	   " with fixed hops\n");
  fprintf (stderr, "\t\t4 : PV with fixed hops by Ellis\n");
  fprintf (stderr, "\t\t5 : PV in freq. domain\n");
  fprintf (stderr, "  -w --window\t0 no window\n");
  fprintf (stderr, "\t\t1 parzen window\n");
  fprintf (stderr, "\t\t2 welch window\n");
  fprintf (stderr, "\t\t3 hanning window (default)\n");
  fprintf (stderr, "\t\t4 hamming window\n");
  fprintf (stderr, "\t\t5 blackman window\n");
  fprintf (stderr, "\t\t6 steeper 30-dB/octave rolloff window\n");
}



int main (int argc, char** argv)
{
  int i;

  char * file_in = NULL;
  char * file_out = NULL;

  /* option analysis */
  long len = 2048;
  long hop = 256;
  double rate = 1.0;
  double pitch_shift = 0.0;
  int scheme = 0;
  int flag_window = 3; // hanning window
  for (i=1; i<argc; i++)
    {
      if ((strcmp (argv[i], "--input" ) == 0)
	 || (strcmp (argv[i], "-i" ) == 0))
	{
	  if (i+1 < argc)
	    {
	      file_in = (char *) malloc (sizeof (char)
					 * (strlen (argv [++i]) + 1));
	      CHECK_MALLOC (file_in, "main");
	      strcpy (file_in, argv[i]);
	    }
	}
      else if ((strcmp (argv[i], "--output" ) == 0)
	 || (strcmp (argv[i], "-o" ) == 0))
	{
	  if (i+1 < argc)
	    {
	      file_out = (char *) malloc (sizeof (char)
					 * (strlen (argv [++i]) + 1));
	      CHECK_MALLOC (file_out, "main");
	      strcpy (file_out, argv[i]);
	    }
	}
      else if (strcmp (argv[i], "-n" ) == 0)
	{
	  if (i+1 < argc)
	    {
	      len = (long)atoi (argv [++i]);
	    }
	}
      else if (strcmp (argv[i], "-hop" ) == 0)
	{
	  if (i+1 < argc)
	    {
	      hop = (long)atoi (argv [++i]);
	    }
	}
      else if (strcmp (argv[i], "-rate" ) == 0)
	{
	  if (i+1 < argc)
	    {
	      rate = atof (argv [++i]);
	    }
	}
      else if (strcmp (argv[i], "-pitch" ) == 0)
	{
	  if (i+1 < argc)
	    {
	      pitch_shift = atof (argv [++i]);
	    }
	}
      else if (strcmp (argv[i], "-scheme" ) == 0)
	{
	  if (i+1 < argc)
	    {
	      scheme = atoi (argv [++i]);
	    }
	}
      else if ((strcmp (argv[i], "--window") == 0)
	       || (strcmp (argv[i], "-w") == 0))
	{
	  if ( i+1 < argc )
	    {
	      flag_window = atoi (argv[++i]);
	    }
	}
      else
	{
	  usage (argv [0]);
	  exit (1);
	}
    }

  if (file_in == NULL)
    {
      usage (argv [0]);
      exit (1);
    }


  switch (scheme)
    {
    case 0:
      pv_conventional (file_in, file_out, rate, pitch_shift,
		       len, hop, flag_window);
      break;

    case 1:
      pv_complex (file_in, file_out, rate, pitch_shift,
		  len, hop, flag_window,
		  0 /* no phase lock */
		  );
      break;

    case 2:
      pv_loose_lock (file_in, file_out, rate, pitch_shift,
		     len, hop, flag_window);
      break;

    case 3:
      pv_complex (file_in, file_out, rate, pitch_shift,
		  len, hop, flag_window,
		  1 /* loose phase lock */
		  );
      break;

    case 4:
      pv_ellis (file_in, file_out, rate, pitch_shift,
		len, hop, flag_window);
      break;

    case 5:
      if (pitch_shift != 0.0)
	{
	  fprintf (stderr, "pitch-shifting is not implemented yet\n");
	  break;
	}
      pv_freq (file_in, file_out, rate, len, hop, flag_window);
      break;

    default:
      fprintf (stderr, "invalid scheme number\n");
      break;
    }

  free (file_in);

  return 0;
}
