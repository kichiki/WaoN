/* header file for esd-wrapper.c --
 * some wrapper for esound
 * Copyright (C) 2007 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: esd-wrapper.h,v 1.3 2007/02/16 06:23:19 kichiki Exp $
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
#ifndef	_ESD_WRAPPER_H_
#define	_ESD_WRAPPER_H_


/* open esd device
 * the mode is 16-bits, stereo, stream, play.
 */
int
esd_init_16_stereo_strem_play (int samplerate);


long esd_write (int esd, double *left, double *right, int len);


#endif /* !_ESD_WRAPPER_H_ */
