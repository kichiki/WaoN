/* header file for pv-loose-lock-complex.c --
 * PV - phase vocoder : pv-loose-lock-complex.c
 * Copyright (C) 2007 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: pv-loose-lock-complex.h,v 1.2 2007/02/17 05:37:37 kichiki Exp $
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
#ifndef	_PV_LOOSE_LOCK_COMPLEX_H_
#define	_PV_LOOSE_LOCK_COMPLEX_H_


/* puckette's loose phase lock scheme by complex arithmetics with fixed hops.
 *   t_i - s_i = u_i - u_{i-1} = hop_out
 *   where s_i and t_i are the times for two analysis FFT
 *   and u_i is the time for the synthesis FFT at step i
 * Reference: M.Puckette (1995)
 */
void pv_loose_lock_complex (const char *file, const char *outfile,
			    double rate, long len, long hop_out,
			    int flag_window);


#endif /* !_PV_LOOSE_LOCK_COMPLEX_H_ */
