/* header file for pv-freq.c --
 * PV - phase vocoder : pv-freq.c
 * Copyright (C) 2007 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: pv-freq.h,v 1.1 2007/02/14 03:46:11 kichiki Exp $
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
#ifndef	_PV_FREQ_H_
#define	_PV_FREQ_H_


/* phase vocoder by frequency domain
 * only integer rate is working
 */
void pv_freq (const char *file, const char *outfile,
	      double rate, long len, long hop_in,
	      int flag_window);


#endif /* !_PV_FREQ_H_ */
