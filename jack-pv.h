/* header file for jack-pv.c --
 * real-time phase vocoder with curses interface and JACK
 * Copyright (C) 2007 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: jack-pv.h,v 1.1 2007/10/29 02:49:09 kichiki Exp $
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
#ifndef	_JACK_PV_H_
#define	_JACK_PV_H_


#include "pv-complex.h" // struct pv_complex
#include <jack/jack.h> // jack_client_t, jack_port_t

/* a simple state machine for this client */
enum jack_state {
  Init,
  Run,
  Exit
};

struct pv_jack {
  struct pv_complex *pv;
  jack_client_t *client;
  jack_port_t   *out;
  enum jack_state state;
};


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
			   double *left, double *right);

/**
 * The process callback for this JACK application is called in a
 * special realtime thread once for each audio cycle.
 *
 * This client follows a simple rule: when the JACK transport is
 * running, copy the input port to the output.  When it stops, exit.
 */
int
my_jack_process (jack_nframes_t nframes, void *arg);

/**
 * JACK calls this shutdown_callback if the server ever shuts down or
 * decides to disconnect the client.
 */
void
jack_shutdown (void *arg);

/**
 * start jack process for output (playback)
 * INPUT
 * OUTPUT
 *  returned value : struct pv_jack *pv_jack.
 */
struct pv_jack *
pv_jack_init (struct pv_complex *pv);

void
pv_jack_free (struct pv_jack *pv_jack);

/**
 * phase vocoder by complex arithmetics with fixed hops.
 */
void pv_complex_curses_jack (const char *file,
			     long len, long hop_syn);


#endif /* !_JACK_PV_ */
