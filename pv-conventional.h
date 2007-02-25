/* header file for pv-conventional.c --
 * PV - phase vocoder : pv-conventional.c
 * Copyright (C) 2007 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: pv-conventional.h,v 1.4 2007/02/25 06:03:38 kichiki Exp $
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
 *  hop_in :
 *  hop_out :
 *  l_out [hop_out] :
 *  r_out [hop_out] :
 *  ao, sfout, sfout_info : properties for output
 *  flag_pitch :
 */
int
pv_play_resample (long hop_in, long hop_out,
		  double *l_out, double *r_out,
		  ao_device *ao, SNDFILE *sfout, SF_INFO *sfout_info,
		  int flag_pitch);

/* estimate the superposing weight for the window with hop
 */
double
get_scale_factor_for_window (int len, long hop_out, int flag_window);


/* standard phase vocoder
 * Ref: J.Laroche and M.Dolson (1999)
 */
void pv (const char *file, const char *outfile,
	 double rate, long len, long hop_out,
	 int flag_window,
	 int flag_pitch);


#endif /* !_PV_CONVENTIONAL_H_ */
