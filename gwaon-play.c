/* gWaoN -- gtk+ Spectra Analyzer : playback
 * Copyright (C) 2007 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: gwaon-play.c,v 1.2 2007/02/23 07:00:36 kichiki Exp $
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
#include <gtk/gtk.h>
#include <stdlib.h>


// libsndfile
#include <sndfile.h>
#include "snd.h" // sndfile_read_at()

// ao sound device
#include <ao/ao.h>

#include "pv-complex.h" // struct pv_complex_data, pv_complex_play_step()
#include "gwaon-wav.h" // draw_play_indicator()


// global variables
ao_device *ao;

long play_cur; // current frame to play
int flag_play; // status: 0 = not playing, 1 = playing
gint tag_play; // for timeout callback

double pv_rate;  // time-scaling rate (0 = stop, 1 = normal, -1 = backward)

struct pv_complex_data *pv = NULL; // initialized in create_wav()


/* play 1 milisecond and return
 */
gint
play_1msec (gpointer data)
{
  extern struct pv_complex_data *pv;
  extern long play_cur;
  extern double pv_rate;

  long len_1msec;
  len_1msec = (long)(0.1 /* sec */ * pv->sfinfo->samplerate /* Hz */);

  long hop_in;
  hop_in = (long)((double)pv->hop_out * pv_rate);


  // draw indicator
  draw_play_indicator ((GtkWidget *)data);

  extern int WIN_wav_cur;
  extern int WIN_wav_scale;
  extern int WIN_wav_width;
  long frame0;
  long frame1;
  frame0 = (long) WIN_wav_cur;
  frame1 = frame0 + (long)(WIN_wav_scale * WIN_wav_width) - 1;
  if (frame1 >= pv->sfinfo->frames) frame1 = (long)pv->sfinfo->frames - 1;
  if (play_cur < frame0) play_cur = frame0;
  if (play_cur >= frame1) play_cur = frame1;


  long len_play;
  long l;
  for (l = 0; l < len_1msec; l += len_play)
    {
      len_play = pv_complex_play_step (pv, play_cur, pv_rate,
				       0 /* no phase lock */);
      if (len_play < pv->hop_out)
	{
	  flag_play = 0; // stop playing
	  // rewind
	  if (pv_rate >= 0.0) play_cur = frame0;
	  else                play_cur = frame1;

	  // if FAIL is returned, this timeout is removed
	  return FALSE;
	}

      // increment play_cur
      play_cur += hop_in;

      // check the boundary
      if (play_cur < frame0 ||
	  play_cur + hop_in >= frame1)
	{
	  // rewind
	  if (pv_rate >= 0.0) play_cur = frame0;
	  else                play_cur = frame1;
	}
    }
  return TRUE;
}




