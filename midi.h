/* header file for midi.c : subroutines to write standard MIDI file
 * Copyright (C) 1998 Kengo ICHIKI (ichiki@geocities.com)
 * $Id: midi.h,v 1.1 2006/09/20 21:26:45 kichiki Exp $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* for estimate pitch shift  */
extern double adj_pitch;
extern double pitch_shift;
extern int n_pitch;

extern double mid2freq[128];


int get_note (double freq);

int smf_header_fmt (int fd,
		       unsigned short format,
		       unsigned short tracks,
		       unsigned short divisions);
int smf_prog_change (int fd, char channel, char prog);
int smf_note_on (int fd, long dtime, char note, char vel, char channel);
int smf_note_off (int fd, long dtime, char note, char vel, char channel);
int smf_track_head (int fd, unsigned long size);
int smf_track_end (int fd);
int write_var_len (int fd, long value);
int read_var_len (int fd, long *value);
int wblong (int fd, unsigned long ul);
int wbshort (int fd, unsigned short us);
