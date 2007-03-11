/* header file for pv-conventional.c --
 * PV - phase vocoder : pv-conventional.c
 * Copyright (C) 2007 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: pv-conventional.h,v 1.6 2007/03/11 00:56:39 kichiki Exp $
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
#ifndef	_PV_CONVENTIONAL_H_
#define	_PV_CONVENTIONAL_H_


/** general utility routines for pv **/

/*
 * INPUT
 *  hop_res :
 *  hop_syn :
 *  l_out [hop_syn] :
 *  r_out [hop_syn] :
 *  ao, sfout, sfout_info : properties for output
 */
int
pv_play_resample (long hop_res, long hop_syn,
		  double *l_out, double *r_out,
		  ao_device *ao, SNDFILE *sfout, SF_INFO *sfout_info);


/* estimate the superposing weight for the window with hop
 */
double
get_scale_factor_for_window (int len, long hop_out, int flag_window);


/* standard phase vocoder
 * Ref: J.Laroche and M.Dolson (1999)
 */
void pv_conventional (const char *file, const char *outfile,
		      double rate, double pitch_shift,
		      long len, long hop_syn,
		      int flag_window);


#endif /* !_PV_CONVENTIONAL_H_ */
