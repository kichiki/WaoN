/* header file for ao-wrapper.c --
 * some wrapper for ao
 * Copyright (C) 2007 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: ao-wrapper.h,v 1.2 2007/10/14 06:20:17 kichiki Exp $
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
#ifndef	_AO_WRAPPER_H_
#define	_AO_WRAPPER_H_


/* open ao device
 * the mode is 16-bits, stereo.
 * INPUT
 *  verbose : 0 == quiet
 *            1 == print info
 */
ao_device *
ao_init_16_stereo (int samplerate, int verbose);

void
print_ao_driver_info_list (void);


int
ao_write (ao_device *device, double *left, double *right, int len);


#endif /* !_AO_WRAPPER_H_ */
