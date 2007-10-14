/* header file for pv-complex-curses.c --
 * real-time phase vocoder with curses interface
 * Copyright (C) 2007 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: pv-complex-curses.h,v 1.1 2007/10/14 06:18:34 kichiki Exp $
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
#ifndef	_PV_COMPLEX_CURSES_H_
#define	_PV_COMPLEX_CURSES_H_

#include "pv-complex.h" // struct pv_complex_data


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
		   long frame0, long frame1);

/* phase vocoder by complex arithmetics with fixed hops.
 */
void pv_complex_curses (const char *file,
			double pv_rate,
			double pv_pitch,
			long len, long hop_syn);

#endif /* !_PV_COMPLEX_CURSES_H_ */
