/* real-time phase vocoder with curses interface
 * Copyright (C) 2007 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: pv-complex-curses.c,v 1.1 2007/10/14 06:17:43 kichiki Exp $
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
#include <curses.h>

#include <ao/ao.h>
#include "ao-wrapper.h" // ao_init_16_stereo()
#include "pv-complex.h" // struct pv_complex_data
#include "pv-conventional.h" // get_scale_factor_for_window()


/* play 1 milisecond and return
 * INPUT
 *  pv        : struct pv_complex_data
 *  play_cur  : current frame
 *  pv_rate   : rate of playback speed
 *  pv_pitch  : pitch-shift
 *  frame0, frame1 : the loop range
 * OUTPUT
 */
int
play_1msec_curses (struct pv_complex_data *pv,
		   long *play_cur,
		   double pv_rate,
		   double pv_pitch,
		   long frame0, long frame1)
{
  long len_1msec;
  len_1msec = (long)(0.1 /* sec */ * pv->sfinfo->samplerate /* Hz */);

  pv->hop_res = (long)((double)pv->hop_syn * pow (2.0, - pv_pitch / 12.0));
  pv->hop_ana = (long)((double)pv->hop_res * pv_rate);


  if (*play_cur <  frame0) *play_cur = frame0;
  if (*play_cur >= frame1) *play_cur = frame1;


  long len_play;
  long l;
  for (l = 0; l < len_1msec; l += len_play)
    {
      len_play = pv_complex_play_step (pv, *play_cur);

      // increment play_cur
      *play_cur += pv->hop_ana;

      // check the boundary
      if (*play_cur < frame0 ||
	  *play_cur + pv->hop_ana >= frame1)
	{
	  // rewind
	  if (pv_rate >= 0.0) *play_cur = frame0;
	  else                *play_cur = frame1;
	}
    }
  return 1; // TRUE
}


#define Y_file    (1)
#define Y_frames  (2)
#define Y_loop    (3)

#define Y_rate    (5)
#define Y_pitch   (6)
#define Y_lock    (8)
#define Y_window  (9)

#define Y_status  (12)
#define Y_comment (14)

static void
curses_print_window (int flag_window)
{
  switch (flag_window)
    {
    case 0: // square (no window)
      mvprintw (Y_window, 1, "window     : square  ");
      break;
    case 1: // parzen window
      mvprintw (Y_window, 1, "window     : parzen  ");
      break;
    case 2: // welch window
      mvprintw (Y_window, 1, "window     : welch   ");
      break;
    case 3: // hanning window
      mvprintw (Y_window, 1, "window     : hanning ");
      break;
    case 4: // hamming window
      mvprintw (Y_window, 1, "window     : hamming ");
      break;
    case 5: // blackman window
      mvprintw (Y_window, 1, "window     : blackman");
      break;
    case 6: // steeper 30-dB/octave rolloff window
      mvprintw (Y_window, 1, "window     : steeper ");
      break;
    }
}

static void
curses_print_pv (const char *file,
		 struct pv_complex_data *pv,
		 int flag_play,
		 long frame0, long frame1,
		 double pv_pitch,
		 double pv_rate)
{
  mvprintw (0, 0, "========== pv ==========");
  mvprintw (Y_file,   1, "file       : %s", file);
  mvprintw (Y_frames, 1, "current    : %010ld / %010ld", 0, pv->sfinfo->frames);
  mvprintw (Y_loop,   1, "loop       : %010ld - %010ld", frame0, frame1);
  mvprintw (Y_pitch,  1, "pitch      : %-5.0f", pv_pitch);
  mvprintw (Y_rate,   1, "rate       : %-5.1f", pv_rate);

  if (flag_play == 0) mvprintw(Y_status, 1, "status     : stop");
  else                mvprintw(Y_status, 1, "status     : play");

  if (pv->flag_lock == 0) mvprintw(Y_lock, 1, "phase-lock : off");
  else                    mvprintw(Y_lock, 1, "phase-lock : on ");
  curses_print_window (pv->flag_window);

  // help message
  mvprintw (Y_loop,   41, "< > by cur, shrink { }, extend [ ]");
  mvprintw (Y_pitch,  41, "UP   / DOWN");
  mvprintw (Y_rate,   41, "LEFT / RIGHT");
  mvprintw (Y_status, 41, "SPACE");
  mvprintw (Y_lock,   41, "L");
  mvprintw (Y_window, 41, "W");

  mvprintw (Y_comment-1, 0, "----------------------------------------");
}

/* phase vocoder by complex arithmetics with fixed hops.
 */
void pv_complex_curses (const char *file,
			double pv_rate,
			double pv_pitch,
			long len, long hop_syn)
{
  // ncurses initializing
  initscr();             /* Start curses mode */
  raw();                 /* Line buffering disabled */
  keypad(stdscr, TRUE);  /* We get F1, F2 etc.. */
  noecho();              /* Don't echo() while we do getch */
  nodelay(stdscr, TRUE); /* Don't wait the key press */


  long hop_ana;
  long hop_res;
  hop_res = (long)((double)hop_syn * pow (2.0, - pv_pitch / 12.0));
  hop_ana = (long)((double)hop_res * pv_rate);

  struct pv_complex_data *pv = NULL;
  int flag_window = 3;
  pv = pv_complex_init (len, hop_syn, flag_window);
  pv->hop_res = hop_res;
  pv->hop_ana = hop_ana;

  // open input file
  SNDFILE *sf = NULL;
  SF_INFO sfinfo;
  memset (&sfinfo, 0, sizeof (sfinfo));
  sf = sf_open (file, SFM_READ, &sfinfo);
  if (sf == NULL)
    {
      fprintf (stderr, "fail to open %s", file);
      exit (1);
    }
  //sndfile_print_info (&sfinfo);

  pv_complex_set_input (pv, sf, &sfinfo);

  ao_device *ao = NULL;
  ao = ao_init_16_stereo (sfinfo.samplerate, 0);
  pv_complex_set_output_ao (pv, ao);


  long frame0 = 0;
  long frame1 = (long)pv->sfinfo->frames - 1;
  pv->flag_lock = 0; // no phase-lock
  int flag_play = 1;
  long play_cur = 0;

  long len_1msec;
  len_1msec = (long)(0.1 /* sec */ * pv->sfinfo->samplerate /* Hz */);

  mvprintw (Y_comment, 1, "Welcome WaoN-pv in curses mode.");
  curses_print_pv (file, pv, flag_play, frame0, frame1,
		   pv_pitch, pv_rate);

  long status = 1; // TRUE
  do
    {
      if (flag_play != 0)
	{
	  status = play_1msec_curses (pv, &play_cur,
				      pv_rate, pv_pitch,
				      frame0, frame1);
	}
      // scan keyboard
      int ch = getch();
      switch (ch)
	{
	case ERR: // no key event
	  break;

	case ' ': // SPACE
	  flag_play++;
	  flag_play = flag_play % 2;
	  if (flag_play == 0) mvprintw(Y_status, 1, "status     : stop");
	  else                mvprintw(Y_status, 1, "status     : play");
	  break;

	case '>':
	case '.':
	  frame1 = play_cur;
	  mvprintw (Y_loop,   1, "loop       : %010ld - %010ld",
		    frame0, frame1);
	  break;

	case ']':
	  frame1 += len_1msec;
	  if (frame1 >= pv->sfinfo->frames - 1) frame1 = pv->sfinfo->frames - 1;
	  mvprintw (Y_loop,   1, "loop       : %010ld - %010ld",
		    frame0, frame1);
	  break;

	case '}':
	  frame1 -= len_1msec;
	  if (frame0 > frame1) frame1 = frame0;
	  mvprintw (Y_loop,   1, "loop       : %010ld - %010ld",
		    frame0, frame1);
	  break;

	case '<':
	case ',':
	  frame0 = play_cur;
	  mvprintw (Y_loop,   1, "loop       : %010ld - %010ld",
		    frame0, frame1);
	  break;

	case '[':
	  frame0 -= len_1msec;
	  if (frame0 < 0) frame0 = 0;
	  mvprintw (Y_loop,   1, "loop       : %010ld - %010ld",
		    frame0, frame1);
	  break;

	case '{':
	  frame0 += len_1msec;
	  if (frame0 > frame1) frame0 = frame1;
	  mvprintw (Y_loop,   1, "loop       : %010ld - %010ld",
		    frame0, frame1);
	  break;

	case 'L':
	case 'l':
	  pv->flag_lock++;
	  pv->flag_lock = pv->flag_lock % 2;

	  if (pv->flag_lock == 0) mvprintw(Y_lock, 1, "phase-lock : off");
	  else                    mvprintw(Y_lock, 1, "phase-lock : on ");
	  break;

	case 'W':
	case 'w':
	  pv->flag_window++;
	  pv->flag_window = pv->flag_window % 7; // 0 to 6

	  // reset scale factor
	  pv->window_scale
	    = get_scale_factor_for_window (pv->len, pv->hop_syn,
					   pv->flag_window);
	  curses_print_window (pv->flag_window);
	  break;

	case KEY_UP:
	  pv_pitch += 1.0;
	  mvprintw (Y_pitch,  1, "pitch      : %-5.0f", pv_pitch);
	  break;

	case KEY_DOWN:
	  pv_pitch -= 1.0;
	  mvprintw (Y_pitch,  1, "pitch      : %-5.0f", pv_pitch);
	  break;

	case KEY_LEFT:
	  pv_rate -= 0.1;
	  mvprintw (Y_rate,   1, "rate       : %-5.1f", pv_rate);
	  break;

	case KEY_RIGHT:
	  pv_rate += 0.1;
	  mvprintw (Y_rate,   1, "rate       : %-5.1f", pv_rate);
	  break;

	case KEY_HOME:
	case 'R':
	case 'r':
	  frame0 = 0;
	  frame1 = (long)pv->sfinfo->frames - 1;
	  pv_rate = 1.0;
	  pv_pitch = 0.0;
	  curses_print_pv (file, pv, flag_play, frame0, frame1,
			   pv_pitch, pv_rate);
	  mvprintw(Y_comment, 1, "reset everything");
	  break;

	case 'Q':
	case 'q':
	  status = 0;
	  mvprintw(Y_comment, 1, "good-bye!");
	  break;

	  /*
	defaut :
	  break;
	  */
	}
      mvprintw (Y_frames, 1, "current    : %010ld", play_cur);
      refresh();
    }
  while (status == 1);

  pv_complex_free (pv);
  sf_close (sf) ;


  /* End ncurses mode */
  endwin();
}
