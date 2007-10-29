/* real-time phase vocoder with curses interface and JACK
 * Copyright (C) 2007 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: jack-pv.c,v 1.1 2007/10/29 02:48:41 kichiki Exp $
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
#include <unistd.h> // sleep()
#include <stdlib.h>
#include <string.h>
#include <math.h> // pow()

#include <ao/ao.h>
#include "ao-wrapper.h"
#include <sndfile.h>
#include "pv-conventional.h" // get_scale_factor_for_window ()
#include <curses.h>

#include <jack/jack.h>
#include <jack/transport.h>

#include "pv-complex.h" // struct pv_complex
#include "hc.h" // HC_complex_phase_vocoder()
#include "memory-check.h" // CHECK_MALLOC

#include "jack-pv.h"

// check
FILE *err_log = NULL;

/*
 * OUTPUT
 *  returned value : 1 if x[i] = 0 for i = 0 to n-1
 *                   0 otherwise
 */
static int
check_zero (int n, const double *x)
{
  int i;

  for (i = 0; i < n; i ++)
    {
      if (x[i] != 0.0) return 1;
    }
  return 0;
}

/* play one hop_in by the phase vocoder:
 * phase vocoder by complex arithmetics with fixed hops.
 *   t_i - s_i = u_i - u_{i-1} = hop
 *   where s_i and t_i are the times for two analysis FFT
 *   and u_i is the time for the synthesis FFT at step i
 * Reference: M.Puckette (1995)
 * INPUT
 *  pv : struct pv_complex
 *  cur : current frame to play.
 *        you have to increment this by yourself.
 *  pv->flag_lock : 0 == no phase lock
 *                  1 == loose phase lock
 * OUTPUT
 *  left[pv->hop_res], right[pv->hop_res] : 
 *  returned value : hop_res (not hop_syn).
 */
int
jack_pv_complex_play_step (struct pv_complex *pv,
			   long cur,
			   double *left, double *right)
{
  static double *l_fs  = NULL;
  static double *r_fs  = NULL;
  static double *l_ft  = NULL;
  static double *r_ft  = NULL;
  static double *l_tmp = NULL;
  static double *r_tmp = NULL;

  if (l_fs == NULL)
    {
      l_fs = (double *)malloc (pv->len * sizeof (double));
      r_fs = (double *)malloc (pv->len * sizeof (double));
      CHECK_MALLOC (l_fs, "pv_complex_play_step");
      CHECK_MALLOC (r_fs, "pv_complex_play_step");

      l_ft = (double *)malloc (pv->len * sizeof (double));
      r_ft = (double *)malloc (pv->len * sizeof (double));
      CHECK_MALLOC (l_ft, "pv_complex_play_step");
      CHECK_MALLOC (r_ft, "pv_complex_play_step");

      l_tmp = (double *)malloc (pv->len * sizeof (double));
      r_tmp = (double *)malloc (pv->len * sizeof (double));
      CHECK_MALLOC (l_tmp, "pv_complex_play_step");
      CHECK_MALLOC (r_tmp, "pv_complex_play_step");
    }

  long status;
  // read the starting frame (cur)
  status = read_and_FFT_stereo (pv, cur, l_fs, r_fs);
  if (status != pv->len)
    {
      return 0; // no output
    }

  // read the terminal frame (cur + hop_syn)
  status = read_and_FFT_stereo (pv, cur + pv->hop_syn, l_ft, r_ft);
  if (status != pv->len)
    {
      return 0; // no output
    }

  int flag_left_cur;
  int flag_right_cur;
  if (check_zero (pv->len, l_fs) == 0 ||
      check_zero (pv->len, l_ft) == 0)
    {
      flag_left_cur = 0; // inactive
    }
  else
    {
      flag_left_cur = 1; // active
    }

  if (check_zero (pv->len, r_fs) == 0 ||
      check_zero (pv->len, r_ft) == 0)
    {
      flag_right_cur = 0; // inactive
    }
  else
    {
      flag_right_cur = 1; // active
    }


  int i;

  // left channel
  if (flag_left_cur == 1)
    {
      // check l_f_old[]
      if (pv->flag_left == 0)
	{
	  if (pv->flag_lock == 0) // no phase lock
	    {
	      for (i = 0; i < pv->len; i ++)
		{
		  pv->l_f_old [i] = l_fs [i];
		}
	    }
	  else // loose phase lock
	    {
	      // apply loose phase lock
	      HC_puckette_lock (pv->len, l_fs, pv->l_f_old);
	    }

	  pv->flag_left = 1;
	}

      // generate the frame (out_0 + (n+1) * hop_syn), that is, "u_i"
      if (pv->flag_lock == 0) // no phase lock
	{
	  // Y[u_i] = X[t_i] (Y[u_{i-1}]/X[s_i]) / |Y[u_{i-1}]/X[s_i]|
	  HC_complex_phase_vocoder (pv->len, l_fs, l_ft, pv->l_f_old,
				    pv->l_f_old);
	  // already backed up for the next step in [lr]_f_old[]
	  apply_invFFT_mono (pv, pv->l_f_old, pv->window_scale, pv->l_out);
	}
      else // loose phase lock
	{
	  // Y[u_i] = X[t_i] (Z[u_{i-1}]/X[s_i]) / |Z[u_{i-1}]/X[s_i]|
	  HC_complex_phase_vocoder (pv->len, l_fs, l_ft, pv->l_f_old,
				    l_tmp);
	  // apply loose phase lock and store for the next step
	  HC_puckette_lock (pv->len, l_tmp, pv->l_f_old);

	  apply_invFFT_mono (pv, l_tmp, pv->window_scale, pv->l_out);
	}
    }


  // right channel
  if (flag_right_cur == 1)
    {
      // check l_f_old[]
      if (pv->flag_right == 0)
	{
	  if (pv->flag_lock == 0) // no phase lock
	    {
	      for (i = 0; i < pv->len; i ++)
		{
		  pv->r_f_old [i] = r_fs [i];
		}
	    }
	  else // loose phase lock
	    {
	      // apply loose phase lock
	      HC_puckette_lock (pv->len, r_fs, pv->r_f_old);
	    }
	  pv->flag_right = 1;
	}

      // generate the frame (out_0 + (n+1) * hop_syn), that is, "u_i"
      if (pv->flag_lock == 0) // no phase lock
	{
	  // Y[u_i] = X[t_i] (Y[u_{i-1}]/X[s_i]) / |Y[u_{i-1}]/X[s_i]|
	  HC_complex_phase_vocoder (pv->len, r_fs, r_ft, pv->r_f_old,
				    pv->r_f_old);
	  // already backed up for the next step in [lr]_f_old[]
	  apply_invFFT_mono (pv, pv->r_f_old, pv->window_scale, pv->r_out);
	}
      else // loose phase lock
	{
	  // Y[u_i] = X[t_i] (Z[u_{i-1}]/X[s_i]) / |Z[u_{i-1}]/X[s_i]|
	  HC_complex_phase_vocoder (pv->len, r_fs, r_ft, pv->r_f_old,
				    r_tmp);
	  // apply loose phase lock and store for the next step
	  HC_puckette_lock (pv->len, r_tmp, pv->r_f_old);

	  apply_invFFT_mono (pv, r_tmp, pv->window_scale, pv->r_out);
	}
    }


  // output
  //status = pv_complex_play_resample (pv);
  if (pv->hop_syn != pv->hop_res)
    {
      pv_complex_resample (pv, left, right);
    }
  else
    {
      for (i = 0; i < pv->hop_res; i ++)
	{
	  left[i]  = pv->l_out[i];
	  right[i] = pv->r_out[i];
	}
    }


  /* shift [lr]_out by hop_syn */
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

  return (pv->hop_res);
}

/**
 * The process callback for this JACK application is called in a
 * special realtime thread once for each audio cycle.
 *
 * This client follows a simple rule: when the JACK transport is
 * running, copy the input port to the output.  When it stops, exit.
 */
int
my_jack_process (jack_nframes_t nframes, void *arg)
{
  static long play_cur = 0;

  struct pv_jack *pv_jack = (struct pv_jack *)arg;

  static jack_default_audio_sample_t *in = NULL;
  static jack_nframes_t in_len = 0;
  if (in == NULL)
    {
      in = (jack_default_audio_sample_t *)malloc
	(sizeof (jack_default_audio_sample_t) * nframes);
      in_len = nframes;
    }
  else if (in_len < nframes)
    {
      in = (jack_default_audio_sample_t *)realloc
	(in,
	 sizeof (jack_default_audio_sample_t) * nframes);
      in_len = nframes;
    }

  static double *left  = NULL;
  static double *right = NULL;
  static int hop_res0 = 0;
  static int cur = 0;
  if (left == NULL)
    {
      left  = (double *)malloc (sizeof (double) * pv_jack->pv->hop_res);
      right = (double *)malloc (sizeof (double) * pv_jack->pv->hop_res);
      CHECK_MALLOC (left,  "my_jack_process");
      CHECK_MALLOC (right, "my_jack_process");
      hop_res0 = pv_jack->pv->hop_res;
      cur = hop_res0; // no data left
    }
  else if (hop_res0 < pv_jack->pv->hop_res)
    {
      left  = (double *)realloc (left,  sizeof (double) * pv_jack->pv->hop_res);
      right = (double *)realloc (right, sizeof (double) * pv_jack->pv->hop_res);
      CHECK_MALLOC (left,  "my_jack_process");
      CHECK_MALLOC (right, "my_jack_process");
      hop_res0 = pv_jack->pv->hop_res;
      cur = hop_res0; // no data left
    }

  // check
  extern FILE *err_log;
  fprintf (err_log, "cur = %d\n", cur);

  jack_transport_state_t ts = jack_transport_query (pv_jack->client, NULL);
  if (ts == JackTransportRolling)
    {
      if (pv_jack->state == Init)
	{
	  pv_jack->state = Run;
	}

      jack_default_audio_sample_t *out
	= (jack_default_audio_sample_t *)
	jack_port_get_buffer (pv_jack->out, nframes);

      // the process for nframes
      int i, j;
      // paste the data left in left[] and right[] from cur to hop_res0
      for (i = 0, j = cur;
	   (j < hop_res0) && (i < nframes);
	   i ++, j ++)
	{
	  in[i] = (jack_default_audio_sample_t)(0.5*(left[j] + right[j]));
	}
      cur = j;
      cur = cur % hop_res0;

      // if the data is not enough, 
      while (i < nframes)
	{
	  // process further data (next hop_res frames)
	  if (cur != 0)
	    {
	      fprintf (stderr, "cur update failed...\n");
	      fprintf (err_log, "cur update failed...\n");
	      fprintf (err_log, "cur = %d\n", cur);
	      fclose (err_log);
	      exit (1);
	    }
	  jack_pv_complex_play_step (pv_jack->pv, play_cur, left, right);
	  play_cur += pv_jack->pv->hop_ana;

	  // then, paste the data in left[] and right[] from 0 to hop_res0
	  int j;
	  for (j = 0;
	       (j < hop_res0) && (i < nframes);
	       j ++, i ++)
	    {
	      in[i] = (jack_default_audio_sample_t)(0.5*(left[j] + right[j]));
	    }
	  cur = j;
	  cur = cur % hop_res0;
	}

      // now, the data in in[] is enough to pass jack server
      memcpy (out, in,
	      sizeof (jack_default_audio_sample_t) * nframes);
    }
  else if (ts == JackTransportStopped)
    {
      if (pv_jack->state == Run)
	{
	  pv_jack->state = Exit;
	}
    }

  return 0;      
}

/**
 * JACK calls this shutdown_callback if the server ever shuts down or
 * decides to disconnect the client.
 */
void
jack_shutdown (void *arg)
{
  exit (1);
}

/* start jack process for output (playback)
 * INPUT
 * OUTPUT
 *  returned value : struct pv_jack *pv_jack.
 */
struct pv_jack *
pv_jack_init (struct pv_complex *pv)
{
  struct pv_jack *pv_jack
    = (struct pv_jack *)malloc (sizeof (struct pv_jack));
  CHECK_MALLOC (pv_jack, "main");

  pv_jack->pv = pv;
  pv_jack->state = Init;


  char *client_name = (char *)malloc (sizeof (char) * 8);
  strcpy (client_name, "jack-pv");

  const char *server_name = NULL;

  /* open a client connection to the JACK server */
  jack_options_t options = JackNullOption;
  jack_status_t jack_status;
  pv_jack->client
    = jack_client_open (client_name, options, &jack_status, server_name);
  if (pv_jack->client == NULL)
    {
      fprintf (stderr, "jack_client_open() failed, "
	       "status = 0x%2.0x\n", jack_status);
      if (jack_status & JackServerFailed)
	{
	  fprintf (stderr, "Unable to connect to JACK server\n");
	}
      exit (1);
    }
  if (jack_status & JackServerStarted)
    {
      fprintf (stderr, "JACK server started\n");
    }
  if (jack_status & JackNameNotUnique)
    {
      free (client_name);
      client_name = jack_get_client_name(pv_jack->client);
      fprintf (stderr, "unique name `%s' assigned\n", client_name);
    }

  /* tell the JACK server to call `process()' whenever
     there is work to be done.
  */
  jack_set_process_callback (pv_jack->client, my_jack_process, pv_jack);

  /* tell the JACK server to call `jack_shutdown()' if
     it ever shuts down, either entirely, or if it
     just decides to stop calling us.
  */
  jack_on_shutdown (pv_jack->client, jack_shutdown, pv_jack);


  /* display the current sample rate. 
   */
  printf ("engine sample rate: %" PRIu32 "\n",
	  jack_get_sample_rate (pv_jack->client));

  /* create the output ports */
  pv_jack->out = jack_port_register (pv_jack->client, "output",
				     JACK_DEFAULT_AUDIO_TYPE,
				     JackPortIsOutput, 0);

  if (pv_jack->out == NULL)
    {
      fprintf(stderr, "no more JACK ports available\n");
      exit (1);
    }

  /* Tell the JACK server that we are ready to roll.  Our
   * process() callback will start running now. */
  if (jack_activate (pv_jack->client))
    {
      fprintf (stderr, "cannot activate client");
      exit (1);
    }

  /* Connect the ports.  You can't do this before the client is
   * activated, because we can't make connections to clients
   * that aren't running.  Note the confusing (but necessary)
   * orientation of the driver backend ports: playback ports are
   * "input" to the backend, and capture ports are "output" from
   * it.
   */
  const char **ports;
  ports = jack_get_ports (pv_jack->client, NULL, NULL,
			  JackPortIsPhysical|JackPortIsInput);
  if (ports == NULL)
    {
      fprintf(stderr, "no physical playback ports\n");
      exit (1);
    }
  if (jack_connect (pv_jack->client, jack_port_name (pv_jack->out), ports[0]))
    {
      fprintf (stderr, "cannot connect output ports\n");
    }
  free (ports);


  return (pv_jack);
}

void
pv_jack_free (struct pv_jack *pv_jack)
{
  if (pv_jack != NULL) free (pv_jack);
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

/* change rate and pitch (note that hop_syn is fixed)
 * INPUT
 *  pv : struct pv_complex
 *  rate  : rate of speed (1 == same speed, negative == backward)
 *  pitch : pitch-shift (0 == no-shift, +1(-1) = half-note up(down))
 * OUTPUT
 *  pv->hop_res : 
 *  pv->hop_ana : 
 */
void
pv_complex_change_rate_pitch_ (struct pv_complex *pv,
			       int sr_in, int sr_out,
			       double rate,
			       double pitch)
{
  double rate0 = (double)sr_out / (double)sr_in;
  pv->hop_res = (long)(rate0 * (double)pv->hop_syn * pow (2.0, - pitch / 12.0));
  pv->hop_ana = (long)((double)pv->hop_res * rate);
}

/* phase vocoder by complex arithmetics with fixed hops.
 */
void pv_complex_curses_jack (const char *file,
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
      fprintf (stderr, "fail to open %s\n", file);
      exit (1);
    }
  //sndfile_print_info (&sfinfo);

  pv_complex_set_input (pv, sf, &sfinfo);

  /*
  ao_device *ao = NULL;
  ao = ao_init_16_stereo (sfinfo.samplerate, 0);
  pv_complex_set_output_ao (pv, ao);
  */

  // jack initialization
  struct pv_jack *pv_jack = pv_jack_init (pv);
  int jack_sr = (int)jack_get_sample_rate (pv_jack->client);
  extern FILE *err_log;
  err_log = fopen ("jack-pv.log", "w");


  // initial values
  double pv_rate  = 1.0;
  double pv_pitch = 0.0;
  pv_complex_change_rate_pitch_ (pv, sfinfo.samplerate, jack_sr,
				 pv_rate, pv_pitch);
  fprintf (stderr, "# samplerates: %d %d\n", sfinfo.samplerate, jack_sr);

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
  //long status = 1; // TRUE
  do
    {
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
	  pv_complex_change_rate_pitch_ (pv, sfinfo.samplerate, jack_sr,
					 pv_rate, pv_pitch);
	  mvprintw (Y_hop_syn,1, "hop(syn)   : %06ld", pv->hop_syn);
	  mvprintw (Y_hop_ana,1, "hop(ana)   : %06ld", pv->hop_ana);
	  mvprintw (Y_hop_res,1, "hop(res)   : %06ld", pv->hop_res);
	  break;

	case 'h':
	  pv->hop_syn /= 2;
	  if (pv->hop_syn < 1) pv->hop_syn = 1;
	  // hop_res, hop_ana depend on hop_syn
	  pv_complex_change_rate_pitch_ (pv, sfinfo.samplerate, jack_sr,
					 pv_rate, pv_pitch);
	  mvprintw (Y_hop_syn,1, "hop(syn)   : %06ld", pv->hop_syn);
	  mvprintw (Y_hop_ana,1, "hop(ana)   : %06ld", pv->hop_ana);
	  mvprintw (Y_hop_res,1, "hop(res)   : %06ld", pv->hop_res);
	  break;

	case KEY_UP:
	  pv_pitch += 1.0;
	  pv_complex_change_rate_pitch_ (pv, sfinfo.samplerate, jack_sr,
					 pv_rate, pv_pitch);
	  curses_print_pitch (pv_pitch);
	  mvprintw (Y_hop_syn,1, "hop(syn)   : %06ld", pv->hop_syn);
	  mvprintw (Y_hop_ana,1, "hop(ana)   : %06ld", pv->hop_ana);
	  mvprintw (Y_hop_res,1, "hop(res)   : %06ld", pv->hop_res);
	  break;

	case KEY_DOWN:
	  pv_pitch -= 1.0;
	  pv_complex_change_rate_pitch_ (pv, sfinfo.samplerate, jack_sr,
					 pv_rate, pv_pitch);
	  curses_print_pitch (pv_pitch);
	  mvprintw (Y_hop_syn,1, "hop(syn)   : %06ld", pv->hop_syn);
	  mvprintw (Y_hop_ana,1, "hop(ana)   : %06ld", pv->hop_ana);
	  mvprintw (Y_hop_res,1, "hop(res)   : %06ld", pv->hop_res);
	  break;

	case KEY_LEFT:
	  pv_rate -= 0.1;
	  pv_complex_change_rate_pitch_ (pv, sfinfo.samplerate, jack_sr,
					 pv_rate, pv_pitch);
	  mvprintw (Y_rate,   1, "rate       : %-5.1f", pv_rate);
	  mvprintw (Y_hop_syn,1, "hop(syn)   : %06ld", pv->hop_syn);
	  mvprintw (Y_hop_ana,1, "hop(ana)   : %06ld", pv->hop_ana);
	  mvprintw (Y_hop_res,1, "hop(res)   : %06ld", pv->hop_res);
	  break;

	case KEY_RIGHT:
	  pv_rate += 0.1;
	  pv_complex_change_rate_pitch_ (pv, sfinfo.samplerate, jack_sr,
					 pv_rate, pv_pitch);
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
	  pv_complex_change_rate_pitch_ (pv, sfinfo.samplerate, jack_sr,
					 pv_rate, pv_pitch);
	  curses_print_pv (file, pv, flag_play, frame0, frame1,
			   pv_pitch, pv_rate);
	  mvprintw(Y_comment, 1, "reset everything");
	  break;

	case 'Q':
	case 'q':
	  //status = 0;
	  pv_jack->state = Exit;
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
  //while (status == 1);
  while (pv_jack->state != Exit);

  jack_client_close (pv_jack->client);
  pv_jack_free (pv_jack);

  pv_complex_free (pv);
  sf_close (sf) ;


  /* End ncurses mode */
  endwin();
}
