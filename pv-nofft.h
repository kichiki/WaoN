/* header file for pv-nofft.c --
 * the core of phase vocoder without FFT, for fun (this is not phase vocoder)
 * Copyright (C) 2007 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: pv-nofft.h,v 1.1 2007/10/29 01:17:40 kichiki Exp $
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
#ifndef	_PV_NOFFT_H_
#define	_PV_NOFFT_H_


/* play one hop_syn by the phase vocoder without FFT
 * INPUT
 *  pv : struct pv_complex
 *  cur : current frame to play.
 *        you have to increment this by yourself.
 *  pv->flag_lock : 0 == no phase lock
 *                  1 == loose phase lock
 * OUTPUT (returned value)
 *  status : output frames (should be hop_res)
 */
long
pv_nofft_play_step (struct pv_complex *pv,
		    long cur);

/* phase vocoder by no-FFT -- wrapper for pv_nofft_play_step()
 * INPUT
 *  flag_lock : 0 == no phase lock is applied
 *              1 == loose phase lock is applied
 *  rate : time-streching rate
 *  pitch_shift : in the unit of half-note
 */
void pv_nofft (const char *file, const char *outfile,
	       double rate, double pitch_shift,
	       long len, long hop_syn,
	       int flag_window);


#endif /* !_PV_NOFFT_H_ */
