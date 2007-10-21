/* real-time phase vocoder with curses interface
 * Copyright (C) 2007 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: pv-complex-curses.c,v 1.4 2007/10/21 04:04:24 kichiki Exp $
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
#include "pv-complex.h" // struct pv_complex
#include "pv-conventional.h" // get_scale_factor_for_window()

#include "memory-check.h" // CHECK_MALLOC


/* play 100 milisecond and return
 * INPUT
 *  pv        : struct pv_complex
 *  play_cur  : current frame
 *  frame0, frame1 : the loop range
 * OUTPUT
 */
int
play_100msec_curses (struct pv_complex *pv,
		     long *play_cur,
		     long frame0, long frame1)
{
  long len_100msec = (long)(0.1 /* sec */ * pv->sfinfo->samplerate /* Hz */);

  if (*play_cur <  frame0) *play_cur = frame0;
  if (*play_cur >= frame1) *play_cur = frame1;


  long len_play;
  long l;
  for (l = 0; l < len_100msec; l += len_play)
    {
      len_play = pv_complex_play_step (pv, *play_cur);

      // increment play_cur
      *play_cur += pv->hop_ana;

      // check the boundary
      if (*play_cur < frame0 ||
	  *play_cur + pv->hop_ana >= frame1)
	{
	  // rewind
	  if (pv->hop_ana >= 0.0) *play_cur = frame0;
	  else                    *play_cur = frame1;
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
#define Y_window  (10)
#define Y_len     (11)
#define Y_hop_syn (12)
#define Y_hop_ana (13)
#define Y_hop_res (14)

#define Y_status  (16)
#define Y_comment (18)

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
curses_print_pitch (int pv_pitch)
{
  int oct;
  int key;
  if (pv_pitch < 0)
    {
      oct = (-pv_pitch-1) / 12; // -12 == -2 octaves
      oct = -oct-1;
      key = (-pv_pitch-1) % 12;
      key = 11-key;
    }
  else
    {
      oct = pv_pitch / 12;
      key = pv_pitch % 12;
    }

  switch (key)
    {
    case 0: // 1 degree
      mvprintw (Y_pitch,  1, "pitch      : C  %+2d (%+3d)", oct, pv_pitch);
      break;
    case 1: // 1.5
      mvprintw (Y_pitch,  1, "pitch      : C# %+2d (%+3d)", oct, pv_pitch);
      break;
    case 2: // +2nd
      mvprintw (Y_pitch,  1, "pitch      : D  %+2d (%+3d)", oct, pv_pitch);
      break;
    case 3: // 2.5
      mvprintw (Y_pitch,  1, "pitch      : Eb %+2d (%+3d)", oct, pv_pitch);
      break;
    case 4: // +3
      mvprintw (Y_pitch,  1, "pitch      : E  %+2d (%+3d)", oct, pv_pitch);
      break;
    case 5: // +4th
      mvprintw (Y_pitch,  1, "pitch      : F  %+2d (%+3d)", oct, pv_pitch);
      break;
    case 6: // 4.5
      mvprintw (Y_pitch,  1, "pitch      : F# %+2d (%+3d)", oct, pv_pitch);
      break;
    case 7: // +5th
      mvprintw (Y_pitch,  1, "pitch      : G  %+2d (%+3d)", oct, pv_pitch);
      break;
    case 8: // 5.5
      mvprintw (Y_pitch,  1, "pitch      : G# %+2d (%+3d)", oct, pv_pitch);
      break;
    case 9: // +6th
      mvprintw (Y_pitch,  1, "pitch      : A  %+2d (%+3d)", oct, pv_pitch);
      break;
    case 10: // m7th
      mvprintw (Y_pitch,  1, "pitch      : Bb %+2d (%+3d)", oct, pv_pitch);
      break;
    case 11: // M7
      mvprintw (Y_pitch,  1, "pitch      : B  %+2d (%+3d)", oct, pv_pitch);
      break;
    default:
      break;
    }
}

static void
curses_print_pv (const char *file,
		 struct pv_complex *pv,
		 int flag_play,
		 long frame0, long frame1,
		 double pv_pitch,
		 double pv_rate)
{
  mvprintw (0, 0, "========== pv ==========");
  mvprintw (Y_file,   1, "file       : %s", file);
  mvprintw (Y_frames, 1, "current    : %010ld / %010ld", 0, pv->sfinfo->frames);
  mvprintw (Y_loop,   1, "loop       : %010ld - %010ld", frame0, frame1);
  //mvprintw (Y_pitch,  1, "pitch      : %-5.0f", pv_pitch);
  curses_print_pitch (pv_pitch);
  mvprintw (Y_rate,   1, "rate       : %-5.1f", pv_rate);
  mvprintw (Y_len,    1, "fft-len    : %06ld", pv->len);
  mvprintw (Y_hop_syn,1, "hop(syn)   : %06ld", pv->hop_syn);
  mvprintw (Y_hop_ana,1, "hop(ana)   : %06ld", pv->hop_ana);
  mvprintw (Y_hop_res,1, "hop(res)   : %06ld", pv->hop_res);

  if (flag_play == 0) mvprintw(Y_status, 1, "status     : stop");
  else                mvprintw(Y_status, 1, "status     : play");

  if (pv->flag_lock == 0) mvprintw(Y_lock, 1, "phase-lock : off");
  else                    mvprintw(Y_lock, 1, "phase-lock : on ");
  curses_print_window (pv->flag_window);

  // help message
  mvprintw (Y_loop,    41, "< > by cur, [ { expand } ]");
  mvprintw (Y_pitch,   41, "UP   / DOWN");
  mvprintw (Y_rate,    41, "LEFT / RIGHT");
  mvprintw (Y_hop_syn, 41, "H / h");
  mvprintw (Y_status,  41, "SPACE");
  mvprintw (Y_lock,    41, "L");
  mvprintw (Y_window,  41, "W");

  mvprintw (Y_comment-1, 0, "----------------------------------------");
}

/* phase vocoder by complex arithmetics with fixed hops.
 */
void pv_complex_curses (const char *file,
			long len, long hop_syn)
{
  // ncurses initializing
  initscr();             /* Start curses mode */
  raw();                 /* Line buffering disabled */
  keypad(stdscr, TRUE);  /* We get F1, F2 etc.. */
  noecho();              /* Don't echo() while we do getch */
  nodelay(stdscr, TRUE); /* Don't wait the key press */


  int flag_window = 3;
  struct pv_complex *pv
    = pv_complex_init (len, hop_syn, flag_window);
  CHECK_MALLOC (pv, "pv_complex_curses");

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


  // initial values
  double pv_rate  = 1.0;
  double pv_pitch = 0.0;
  pv_complex_change_rate_pitch (pv, pv_rate, pv_pitch);

  long frame0 = 0;
  long frame1 = (long)pv->sfinfo->frames - 1;
  pv->flag_lock = 0; // no phase-lock
  int flag_play = 1;
  long play_cur = 0;

  long len_1sec  = (long)(pv->sfinfo->samplerate /* Hz */);
  long len_10sec = (long)(10 * pv->sfinfo->samplerate /* Hz */);

  mvprintw (Y_comment, 1, "Welcome WaoN-pv in curses mode.");
  curses_print_pv (file, pv, flag_play, frame0, frame1,
		   pv_pitch, pv_rate);

  // main loop
  long status = 1; // TRUE
  do
    {
      if (flag_play != 0)
	{
	  status = play_100msec_curses (pv, &play_cur,
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

	case '<':
	case ',':
	  frame0 = play_cur;
	  mvprintw (Y_loop,   1, "loop       : %010ld - %010ld",
		    frame0, frame1);
	  break;

	case ']':
	  frame1 += len_10sec;
	  if (frame1 >= pv->sfinfo->frames - 1) frame1 = pv->sfinfo->frames - 1;
	  mvprintw (Y_loop,   1, "loop       : %010ld - %010ld",
		    frame0, frame1);
	  break;

	case '}':
	  frame1 += len_1sec;
	  if (frame1 >= pv->sfinfo->frames - 1) frame1 = pv->sfinfo->frames - 1;
	  mvprintw (Y_loop,   1, "loop       : %010ld - %010ld",
		    frame0, frame1);
	  break;

	case '[':
	  frame0 -= len_10sec;
	  if (frame0 < 0) frame0 = 0;
	  mvprintw (Y_loop,   1, "loop       : %010ld - %010ld",
		    frame0, frame1);
	  break;

	case '{':
	  frame0 -= len_1sec;
	  if (frame0 < 0) frame0 = 0;
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

	case 'H':
	  pv->hop_syn *= 2;
	  if (pv->hop_syn > len) pv->hop_syn = len;
	  // hop_res, hop_ana depend on hop_syn
	  pv_complex_change_rate_pitch (pv, pv_rate, pv_pitch);
	  mvprintw (Y_hop_syn,1, "hop(syn)   : %06ld", pv->hop_syn);
	  mvprintw (Y_hop_ana,1, "hop(ana)   : %06ld", pv->hop_ana);
	  mvprintw (Y_hop_res,1, "hop(res)   : %06ld", pv->hop_res);
	  break;

	case 'h':
	  pv->hop_syn /= 2;
	  if (pv->hop_syn < 1) pv->hop_syn = 1;
	  // hop_res, hop_ana depend on hop_syn
	  pv_complex_change_rate_pitch (pv, pv_rate, pv_pitch);
	  mvprintw (Y_hop_syn,1, "hop(syn)   : %06ld", pv->hop_syn);
	  mvprintw (Y_hop_ana,1, "hop(ana)   : %06ld", pv->hop_ana);
	  mvprintw (Y_hop_res,1, "hop(res)   : %06ld", pv->hop_res);
	  break;

	case KEY_UP:
	  pv_pitch += 1.0;
	  pv_complex_change_rate_pitch (pv, pv_rate, pv_pitch);
	  curses_print_pitch (pv_pitch);
	  mvprintw (Y_hop_syn,1, "hop(syn)   : %06ld", pv->hop_syn);
	  mvprintw (Y_hop_ana,1, "hop(ana)   : %06ld", pv->hop_ana);
	  mvprintw (Y_hop_res,1, "hop(res)   : %06ld", pv->hop_res);
	  break;

	case KEY_DOWN:
	  pv_pitch -= 1.0;
	  pv_complex_change_rate_pitch (pv, pv_rate, pv_pitch);
	  curses_print_pitch (pv_pitch);
	  mvprintw (Y_hop_syn,1, "hop(syn)   : %06ld", pv->hop_syn);
	  mvprintw (Y_hop_ana,1, "hop(ana)   : %06ld", pv->hop_ana);
	  mvprintw (Y_hop_res,1, "hop(res)   : %06ld", pv->hop_res);
	  break;

	case KEY_LEFT:
	  pv_rate -= 0.1;
	  pv_complex_change_rate_pitch (pv, pv_rate, pv_pitch);
	  mvprintw (Y_rate,   1, "rate       : %-5.1f", pv_rate);
	  mvprintw (Y_hop_syn,1, "hop(syn)   : %06ld", pv->hop_syn);
	  mvprintw (Y_hop_ana,1, "hop(ana)   : %06ld", pv->hop_ana);
	  mvprintw (Y_hop_res,1, "hop(res)   : %06ld", pv->hop_res);
	  break;

	case KEY_RIGHT:
	  pv_rate += 0.1;
	  pv_complex_change_rate_pitch (pv, pv_rate, pv_pitch);
	  mvprintw (Y_rate,   1, "rate       : %-5.1f", pv_rate);
	  mvprintw (Y_hop_syn,1, "hop(syn)   : %06ld", pv->hop_syn);
	  mvprintw (Y_hop_ana,1, "hop(ana)   : %06ld", pv->hop_ana);
	  mvprintw (Y_hop_res,1, "hop(res)   : %06ld", pv->hop_res);
	  break;

	case KEY_HOME:
	case 'R':
	case 'r':
	  frame0 = 0;
	  frame1 = (long)pv->sfinfo->frames - 1;
	  pv_rate = 1.0;
	  pv_pitch = 0.0;
	  pv->hop_syn = hop_syn; // value in the argument
	  pv_complex_change_rate_pitch (pv, pv_rate, pv_pitch);
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
